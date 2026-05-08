/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dk_buttons_and_leds.h>

#include "battery.h"
#include "ble.h"
#include "hid.h"
#include "idle.h"
#include "pairing.h"

static void configure_leds(void)
{
	int err = dk_leds_init();

	if (err) {
		printk("Cannot init LEDs (err: %d)\n", err);
	}
}

int main(void)
{
	int err;

	printk("Starting Bluetooth Peripheral HIDS keyboard sample\n");

	configure_leds();

	if (pairing_init() != 0) {
		return 0;
	}

	ble_hid_init();

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	if (ble_init() != 0) {
		return 0;
	}

	ble_advertising_start();
	idle_init();
	battery_init();

	return 0;
}
