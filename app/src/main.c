/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/zbus/zbus.h>

#include <dk_buttons_and_leds.h>

#include "events.h"
#include "hid.h"
#include "ble.h"
#include "pairing.h"
#include "keyboard.h"

#define ADV_LED_BLINK_INTERVAL 1000

#define ADV_STATUS_LED DK_LED1

static void on_key_event(const struct zbus_channel *chan)
{
	const struct app_key_event *evt = zbus_chan_const_msg(chan);
	static bool pairing_button_latched;

	/* Pairing takes priority: consume press of KEY_0/KEY_1 when a
	 * passkey is pending; latch so we also swallow the matching release.
	 */
	if (pairing_is_confirm_pending()) {
		if (evt->pressed && evt->code == INPUT_KEY_0) {
			pairing_button_latched = true;
			pairing_respond(true);
			return;
		}
		if (evt->pressed && evt->code == INPUT_KEY_1) {
			pairing_button_latched = true;
			pairing_respond(false);
			return;
		}
	}
	if (pairing_button_latched && !evt->pressed &&
	    (evt->code == INPUT_KEY_0 || evt->code == INPUT_KEY_1)) {
		pairing_button_latched = false;
		return;
	}

	if (evt->pressed) {
		kb_process_input_press(evt->code);
	} else {
		kb_process_input_release(evt->code);
	}
}

ZBUS_LISTENER_DEFINE(input_listener, on_key_event);
ZBUS_CHAN_ADD_OBS(chan_key_event, input_listener, 4);

static void configure_leds(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		printk("Cannot init LEDs (err: %d)\n", err);
	}
}

static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}

int main(void)
{
	int err;
	int blink_status = 0;

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

	ble_advertising_start();

	for (;;) {
		if (ble_is_advertising()) {
			dk_set_led(ADV_STATUS_LED, (++blink_status) % 2);
		} else {
			dk_set_led_off(ADV_STATUS_LED);
		}
		k_sleep(K_MSEC(ADV_LED_BLINK_INTERVAL));
		/* Battery level simulation */
		bas_notify();
	}
}
