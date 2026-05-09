/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dk_buttons_and_leds.h>

#include "battery.h"
#include "ble.h"
#include "hid.h"
#include "idle.h"
#include "pairing.h"

LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

static void configure_leds(void)
{
	int err = dk_leds_init();

	if (err) {
		LOG_ERR("Cannot init LEDs (err %d)", err);
	}
}

int main(void)
{
	int err;

	LOG_INF("Starting Bluetooth Peripheral HIDS keyboard sample");

	configure_leds();

	if (pair_init() != 0) {
		return 0;
	}

	hid_init();

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	if (ble_init() != 0) {
		return 0;
	}

	ble_advertising_start();
	idle_init();
	bat_init();

	return 0;
}
