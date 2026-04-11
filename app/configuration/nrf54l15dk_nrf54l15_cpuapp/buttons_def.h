/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once
#include "zephyr/sys/util.h"
#include <caf/gpio_pins.h>
#include <caf/key_id.h>

#define KM_SPLITS 1
#define KM_LAYERS 2

#define KM_LAYER_BASE 0
#define KM_LAYER_FUNC 1

static const struct gpio_pin col[] = {};

static const struct gpio_pin row[] = {
	{.port = DT_PROP(DT_GPIO_CTLR(DT_NODELABEL(button0), gpios), port),
	 .pin = DT_GPIO_PIN(DT_NODELABEL(button0), gpios)},
	{.port = DT_PROP(DT_GPIO_CTLR(DT_NODELABEL(button1), gpios), port),
	 .pin = DT_GPIO_PIN(DT_NODELABEL(button1), gpios)},
	{.port = DT_PROP(DT_GPIO_CTLR(DT_NODELABEL(button2), gpios), port),
	 .pin = DT_GPIO_PIN(DT_NODELABEL(button2), gpios)},
	{.port = DT_PROP(DT_GPIO_CTLR(DT_NODELABEL(button3), gpios), port),
	 .pin = DT_GPIO_PIN(DT_NODELABEL(button3), gpios)}};

#define KM_COLS     MAX(ARRAY_SIZE(col), 1)
#define KM_ROWS     ARRAY_SIZE(row)
#define KM_NBUTTONS (KM_ROWS * KM_COLS)

static const uint16_t keymap[KM_SPLITS][KM_LAYERS][KM_NBUTTONS] = {
	[0] = {
		[KM_LAYER_BASE] = {0x01, 0x02, 0x03, 0x04},
		[KM_LAYER_FUNC] = {0x01, 0x02, 0x03, 0x04},
	}};
