/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/dis.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <dk_buttons_and_leds.h>

#include "input.h"
#include "hid.h"
#include "ble.h"
#include "pairing.h"

#define ADV_LED_BLINK_INTERVAL 1000

#define ADV_STATUS_LED DK_LED1

/* HIDs queue elements. */
#define HIDS_QUEUE_SIZE 10

static const uint8_t hello_world_str[] = {
	0x0b, /* Key h */
	0x08, /* Key e */
	0x0f, /* Key l */
	0x0f, /* Key l */
	0x12, /* Key o */
	0x28, /* Key Return */
};

static const uint8_t shift_key[] = {225};

static void button_text_changed(bool down)
{
	static const uint8_t *chr = hello_world_str;

	if (down) {
		hid_buttons_press(chr, 1);
	} else {
		hid_buttons_release(chr, 1);
		if (++chr == (hello_world_str + sizeof(hello_world_str))) {
			chr = hello_world_str;
		}
	}
}

static void button_shift_changed(bool down)
{
	if (down) {
		hid_buttons_press(shift_key, 1);
	} else {
		hid_buttons_release(shift_key, 1);
	}
}

static void on_input_event(uint16_t code, bool pressed)
{
	static bool pairing_button_latched;

	/* Pairing takes priority: consume press of KEY_0/KEY_1 when a
	 * passkey is pending; latch so we also swallow the matching release.
	 */
	if (pairing_is_confirm_pending()) {
		if (pressed && code == INPUT_KEY_0) {
			pairing_button_latched = true;
			pairing_respond(true);
			return;
		}
		if (pressed && code == INPUT_KEY_1) {
			pairing_button_latched = true;
			pairing_respond(false);
			return;
		}
	}
	if (pairing_button_latched && !pressed &&
	    (code == INPUT_KEY_0 || code == INPUT_KEY_1)) {
		pairing_button_latched = false;
		return;
	}

	switch (code) {
	case INPUT_KEY_0:
		button_text_changed(pressed);
		break;
	case INPUT_KEY_1:
		button_shift_changed(pressed);
		break;
	default:
		/* INPUT_KEY_2 and INPUT_KEY_3 are wired in the overlay but unused for now. */
		break;
	}
}

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
	app_input_register(on_input_event);

	if (pairing_init() != 0) {
		return 0;
	}

	hid_init();

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
