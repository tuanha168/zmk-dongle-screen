/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "keyboard_state.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

K_MUTEX_DEFINE(keyboard_state_mutex);
static struct keyboard_state_snapshot keyboard_state;

static struct keyboard_state_snapshot keyboard_state_get_snapshot(void)
{
    k_mutex_lock(&keyboard_state_mutex, K_FOREVER);
    struct keyboard_state_snapshot snapshot = keyboard_state;
    k_mutex_unlock(&keyboard_state_mutex);
    return snapshot;
}

static void keyboard_state_work_cb(struct k_work *work)
{
    struct keyboard_state_snapshot snapshot = keyboard_state_get_snapshot();
    zmk_widget_keyboard_status_update(&snapshot);
}

K_WORK_DEFINE(keyboard_state_work, keyboard_state_work_cb);

static int keyboard_state_listener_cb(const zmk_event_t *eh)
{
    if (!zmk_display_is_initialized()) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_position_state_changed *event = as_zmk_position_state_changed(eh);
    if (event->position < ARRAY_SIZE(keyboard_state.pressed)) {
        k_mutex_lock(&keyboard_state_mutex, K_FOREVER);
        keyboard_state.pressed[event->position] = event->state;
        if (event->state) {
            keyboard_state.press_count[event->position]++;
        }
        k_mutex_unlock(&keyboard_state_mutex);
        k_work_submit_to_queue(zmk_display_work_q(), &keyboard_state_work);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

void zmk_widget_keyboard_state_listener_init(void) { keyboard_state_work_cb(NULL); }

ZMK_LISTENER(widget_keyboard_state, keyboard_state_listener_cb);
ZMK_SUBSCRIPTION(widget_keyboard_state, zmk_position_state_changed);
