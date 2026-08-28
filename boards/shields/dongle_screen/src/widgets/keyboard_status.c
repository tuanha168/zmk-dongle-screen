/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "keyboard_status.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <zephyr/logging/log.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/physical_layouts.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define KEYBOARD_WIDTH 240
#define KEYBOARD_HEIGHT 70
#define KEYBOARD_PADDING 3
#define SCALE_PRECISION 1000
#define RIPPLE_FRAME_MS 40
#define RIPPLE_TRAVEL_MS 900
#define RIPPLE_BAND_WIDTH 26
#define RIPPLE_HUE_STEP 73

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct keyboard_status_state {
    uint32_t position;
    bool pressed;
    bool valid;
};

static void key_center(const struct zmk_key_physical_attrs *key, int32_t *x, int32_t *y)
{
    int32_t center_x = key->x + key->width / 2;
    int32_t center_y = key->y + key->height / 2;

#if IS_ENABLED(CONFIG_ZMK_PHYSICAL_LAYOUT_KEY_ROTATION)
    int32_t angle = key->r / 100;
    if (angle != 0) {
        int32_t sine = lv_trigo_sin(angle);
        int32_t cosine = lv_trigo_sin(angle + 90);
        int32_t relative_x = center_x - key->rx;
        int32_t relative_y = center_y - key->ry;

        center_x = key->rx + (relative_x * cosine - relative_y * sine) / LV_TRIGO_SIN_MAX;
        center_y = key->ry + (relative_x * sine + relative_y * cosine) / LV_TRIGO_SIN_MAX;
    }
#endif

    *x = center_x;
    *y = center_y;
}

static int draw_layout(struct zmk_widget_keyboard_status *widget)
{
    const struct zmk_physical_layout *const *layouts;
    size_t layout_count = zmk_physical_layouts_get_list(&layouts);
    int selected = zmk_physical_layouts_get_selected();

    if (selected < 0 || selected >= (int)layout_count || layouts[selected]->keys_len == 0) {
        return -ENODEV;
    }

    const struct zmk_physical_layout *layout = layouts[selected];
    widget->key_count = MIN(layout->keys_len, ARRAY_SIZE(widget->keys));

    int32_t min_x = INT32_MAX;
    int32_t min_y = INT32_MAX;
    int32_t max_x = INT32_MIN;
    int32_t max_y = INT32_MIN;

    for (size_t i = 0; i < widget->key_count; i++) {
        const struct zmk_key_physical_attrs *key = &layout->keys[i];
        int32_t center_x;
        int32_t center_y;
        key_center(key, &center_x, &center_y);

        min_x = MIN(min_x, center_x - key->width / 2);
        min_y = MIN(min_y, center_y - key->height / 2);
        max_x = MAX(max_x, center_x + key->width / 2);
        max_y = MAX(max_y, center_y + key->height / 2);
    }

    int32_t layout_width = max_x - min_x;
    int32_t layout_height = max_y - min_y;
    if (layout_width <= 0 || layout_height <= 0) {
        return -EINVAL;
    }

    int32_t available_width = KEYBOARD_WIDTH - 2 * KEYBOARD_PADDING;
    int32_t available_height = KEYBOARD_HEIGHT - 2 * KEYBOARD_PADDING;
    int32_t scale = MIN(available_width * SCALE_PRECISION / layout_width,
                        available_height * SCALE_PRECISION / layout_height);
    int32_t drawn_width = layout_width * scale / SCALE_PRECISION;
    int32_t drawn_height = layout_height * scale / SCALE_PRECISION;
    int32_t offset_x = (KEYBOARD_WIDTH - drawn_width) / 2;
    int32_t offset_y = (KEYBOARD_HEIGHT - drawn_height) / 2;

    for (size_t i = 0; i < widget->key_count; i++) {
        const struct zmk_key_physical_attrs *key = &layout->keys[i];
        int32_t center_x;
        int32_t center_y;
        key_center(key, &center_x, &center_y);

        int32_t width = MAX(3, key->width * scale / SCALE_PRECISION - 1);
        int32_t height = MAX(3, key->height * scale / SCALE_PRECISION - 1);
        int32_t x = offset_x + (center_x - min_x) * scale / SCALE_PRECISION - width / 2;
        int32_t y = offset_y + (center_y - min_y) * scale / SCALE_PRECISION - height / 2;

        lv_obj_t *key_obj = lv_obj_create(widget->obj);
        lv_obj_remove_style_all(key_obj);
        lv_obj_set_pos(key_obj, x, y);
        lv_obj_set_size(key_obj, width, height);
        lv_obj_set_style_bg_color(key_obj,
                                  lv_color_hex(CONFIG_DONGLE_SCREEN_KEYBOARD_KEY_COLOR), 0);
        lv_obj_set_style_bg_opa(key_obj, LV_OPA_60, 0);
        lv_obj_set_style_border_color(
            key_obj, lv_color_hex(CONFIG_DONGLE_SCREEN_KEYBOARD_BORDER_COLOR), 0);
        lv_obj_set_style_border_width(key_obj, 1, 0);
        lv_obj_set_style_radius(key_obj, 2, 0);
        widget->keys[i] = key_obj;
        widget->key_center_x[i] = x + width / 2;
        widget->key_center_y[i] = y + height / 2;
#if IS_ENABLED(CONFIG_DONGLE_SCREEN_KEYBOARD_RGB_RIPPLE)
        widget->key_lit[i] = false;
#endif
    }

    int map_length = zmk_physical_layouts_get_selected_to_stock_position_map(&widget->position_map);
    if (map_length < 0) {
        widget->position_map = NULL;
    }

    return 0;
}

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_KEYBOARD_RGB_RIPPLE)
static uint16_t approximate_distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    uint16_t dx = x1 > x2 ? x1 - x2 : x2 - x1;
    uint16_t dy = y1 > y2 ? y1 - y2 : y2 - y1;
    uint16_t longest = MAX(dx, dy);
    uint16_t shortest = MIN(dx, dy);

    return longest + shortest / 2;
}

static bool render_ripples(struct zmk_widget_keyboard_status *widget)
{
    uint32_t now = lv_tick_get();
    bool any_active = false;

    for (size_t key_index = 0; key_index < widget->key_count; key_index++) {
        uint8_t strongest = 0;
        uint16_t hue = 0;

        for (size_t ripple_index = 0; ripple_index < ARRAY_SIZE(widget->ripples);
             ripple_index++) {
            struct keyboard_status_ripple *ripple = &widget->ripples[ripple_index];
            if (!ripple->active) {
                continue;
            }

            uint32_t elapsed = now - ripple->started_at;
            uint32_t radius = elapsed * KEYBOARD_WIDTH / RIPPLE_TRAVEL_MS;
            if (radius > KEYBOARD_WIDTH + RIPPLE_BAND_WIDTH) {
                ripple->active = false;
                continue;
            }

            any_active = true;
            uint16_t origin = ripple->origin_key;
            uint16_t distance = approximate_distance(
                widget->key_center_x[origin], widget->key_center_y[origin],
                widget->key_center_x[key_index], widget->key_center_y[key_index]);
            uint16_t delta = distance > radius ? distance - radius : radius - distance;
            if (delta >= RIPPLE_BAND_WIDTH) {
                continue;
            }

            uint8_t intensity = 100 - delta * 100 / RIPPLE_BAND_WIDTH;
            if (intensity > strongest) {
                strongest = intensity;
                hue = (ripple->hue + distance * 2) % 360;
            }
        }

        if (strongest == 0) {
            if (widget->key_lit[key_index]) {
                lv_obj_set_style_bg_color(
                    widget->keys[key_index],
                    lv_color_hex(CONFIG_DONGLE_SCREEN_KEYBOARD_KEY_COLOR), 0);
                lv_obj_set_style_bg_opa(widget->keys[key_index], LV_OPA_60, 0);
                widget->key_lit[key_index] = false;
            }
            continue;
        }

        lv_obj_set_style_bg_color(widget->keys[key_index], lv_color_hsv_to_rgb(hue, 100, 100),
                                  0);
        lv_obj_set_style_bg_opa(widget->keys[key_index],
                                LV_OPA_60 + strongest * (LV_OPA_COVER - LV_OPA_60) / 100, 0);
        widget->key_lit[key_index] = true;
    }

    return any_active;
}

static void ripple_timer_cb(lv_timer_t *timer)
{
    struct zmk_widget_keyboard_status *widget = lv_timer_get_user_data(timer);
    if (!render_ripples(widget)) {
        lv_timer_pause(timer);
    }
}

static void start_ripple(struct zmk_widget_keyboard_status *widget, size_t origin_key)
{
    struct keyboard_status_ripple *slot = &widget->ripples[0];
    uint32_t now = lv_tick_get();

    for (size_t i = 0; i < ARRAY_SIZE(widget->ripples); i++) {
        if (!widget->ripples[i].active) {
            slot = &widget->ripples[i];
            break;
        }

        if (now - widget->ripples[i].started_at > now - slot->started_at) {
            slot = &widget->ripples[i];
        }
    }

    *slot = (struct keyboard_status_ripple){
        .started_at = now,
        .hue = widget->next_hue,
        .origin_key = origin_key,
        .active = true,
    };
    widget->next_hue = (widget->next_hue + RIPPLE_HUE_STEP) % 360;

    render_ripples(widget);
    lv_timer_resume(widget->ripple_timer);
}
#endif

static void keyboard_status_update_cb(struct keyboard_status_state state)
{
    if (!state.valid) {
        return;
    }

    struct zmk_widget_keyboard_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        for (size_t i = 0; i < widget->key_count; i++) {
            uint32_t position = widget->position_map ? widget->position_map[i] : i;
            if (position != state.position) {
                continue;
            }

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_KEYBOARD_RGB_RIPPLE)
            if (state.pressed) {
                start_ripple(widget, i);
            }
#else
            lv_obj_set_style_bg_color(
                widget->keys[i],
                lv_color_hex(state.pressed ? CONFIG_DONGLE_SCREEN_KEYBOARD_PRESSED_COLOR
                                           : CONFIG_DONGLE_SCREEN_KEYBOARD_KEY_COLOR),
                0);
            lv_obj_set_style_bg_opa(widget->keys[i], state.pressed ? LV_OPA_COVER : LV_OPA_60, 0);
#endif
            break;
        }
    }
}

static struct keyboard_status_state keyboard_status_get_state(const zmk_event_t *eh)
{
    if (eh == NULL) {
        return (struct keyboard_status_state){.valid = false};
    }

    const struct zmk_position_state_changed *event = as_zmk_position_state_changed(eh);
    return (struct keyboard_status_state){
        .position = event->position,
        .pressed = event->state,
        .valid = true,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_keyboard_status, struct keyboard_status_state,
                            keyboard_status_update_cb, keyboard_status_get_state)
ZMK_SUBSCRIPTION(widget_keyboard_status, zmk_position_state_changed);

int zmk_widget_keyboard_status_init(struct zmk_widget_keyboard_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, KEYBOARD_WIDTH, KEYBOARD_HEIGHT);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 0, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);
    lv_obj_remove_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);

    int err = draw_layout(widget);
    if (err < 0) {
        return err;
    }

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_KEYBOARD_RGB_RIPPLE)
    widget->ripple_timer = lv_timer_create(ripple_timer_cb, RIPPLE_FRAME_MS, widget);
    if (widget->ripple_timer == NULL) {
        return -ENOMEM;
    }
    lv_timer_pause(widget->ripple_timer);
#endif

    sys_slist_append(&widgets, &widget->node);
    widget_keyboard_status_init();
    return 0;
}

lv_obj_t *zmk_widget_keyboard_status_obj(struct zmk_widget_keyboard_status *widget)
{
    return widget->obj;
}
