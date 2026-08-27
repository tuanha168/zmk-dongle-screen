/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zmk/matrix.h>

struct zmk_widget_keyboard_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *keys[ZMK_KEYMAP_LEN];
    const uint32_t *position_map;
    size_t key_count;
};

int zmk_widget_keyboard_status_init(struct zmk_widget_keyboard_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_keyboard_status_obj(struct zmk_widget_keyboard_status *widget);
