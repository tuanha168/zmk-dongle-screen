/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zmk/matrix.h>

struct keyboard_state_snapshot {
    bool pressed[ZMK_KEYMAP_LEN];
    uint8_t press_count[ZMK_KEYMAP_LEN];
};

void zmk_widget_keyboard_state_listener_init(void);
void zmk_widget_keyboard_status_update(const struct keyboard_state_snapshot *state);
