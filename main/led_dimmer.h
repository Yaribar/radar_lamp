// Copyright (c) 2025 radar_lamp project
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>

/**
 * @brief Initialize LEDC PWM and start the LED dimmer task.
 *        Must be called once before any other led_dimmer function.
 */
void led_dimmer_init(void);

/**
 * @brief Feed a new distance reading to the dimmer.
 *
 * @param distance_m  Distance in meters (ignored when valid=false)
 * @param valid       True if a real object was detected, false otherwise
 */
void led_dimmer_update(float distance_m, bool valid);
