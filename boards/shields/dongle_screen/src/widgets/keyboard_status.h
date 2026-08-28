/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zmk/matrix.h>

struct keyboard_status_ripple {
    uint32_t started_at;
    uint16_t hue;
    uint16_t origin_key;
    bool active;
};

struct zmk_widget_keyboard_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *keys[ZMK_KEYMAP_LEN];
    int16_t key_center_x[ZMK_KEYMAP_LEN];
    int16_t key_center_y[ZMK_KEYMAP_LEN];
    const uint32_t *position_map;
    size_t key_count;
#if IS_ENABLED(CONFIG_DONGLE_SCREEN_KEYBOARD_RGB_RIPPLE)
    lv_timer_t *ripple_timer;
    struct keyboard_status_ripple ripples[CONFIG_DONGLE_SCREEN_KEYBOARD_MAX_RIPPLES];
    bool key_lit[ZMK_KEYMAP_LEN];
    bool key_pressed[ZMK_KEYMAP_LEN];
    uint8_t press_count[ZMK_KEYMAP_LEN];
    uint16_t next_hue;
#endif
};

int zmk_widget_keyboard_status_init(struct zmk_widget_keyboard_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_keyboard_status_obj(struct zmk_widget_keyboard_status *widget);
