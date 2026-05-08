/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/sys/util.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_hid.h>
#include <zephyr/zbus/zbus.h>

#include "events.h"
#include "pairing.h"

static struct app_hid_report kb_hid_report;
static struct app_hid_report kb_last_published;

static struct keyboard_input_stack_t {
	uint32_t modifiers[8];
	uint32_t keycodes[APP_HID_KEYCODES];
} kb_inputs_src;

static bool kb_fn_active;

/* Set after a pairing-pending KEY_0/KEY_1 press is consumed as an intent;
 * makes the matching release a no-op so it doesn't reach the HID path.
 */
static bool kb_pairing_release_pending;

static void publish_intent(enum app_sys_intent_kind kind, uint8_t arg)
{
	struct app_sys_intent intent = { .kind = kind, .arg = arg };
	(void)zbus_chan_pub(&chan_sys_intent, &intent, K_NO_WAIT);
}

static uint32_t kb_fn_mapping(uint32_t input_code, bool is_fn_active)
{
	if (!is_fn_active) {
		return input_code;
	}

	switch (input_code) {
	case INPUT_KEY_Q:
		return INPUT_BTN_0;
	case INPUT_KEY_W:
		return INPUT_BTN_1;
	case INPUT_KEY_E:
		return INPUT_BTN_2;
	case INPUT_KEY_A:
		return INPUT_KEY_B;
	case INPUT_KEY_B:
		return INPUT_BTN_0;
	default:
		return input_code;
	}
}

/* Fn-layer system intents. Placeholders -- swap once a real keyboard
 * layout exists. The DK overlay only wires KEY_0..KEY_3 with no Fn key,
 * so these are dormant on this hardware for now.
 */
static bool kb_try_fn_intent(uint32_t input_code)
{
	if (!kb_fn_active) {
		return false;
	}

	switch (input_code) {
	case INPUT_KEY_0:
		publish_intent(APP_SYS_INTENT_CLEAR_BONDS, 0);
		return true;
	case INPUT_KEY_1:
		publish_intent(APP_SYS_INTENT_HOST_SELECT, 0);
		return true;
	case INPUT_KEY_2:
		publish_intent(APP_SYS_INTENT_HOST_SELECT, 1);
		return true;
	case INPUT_KEY_3:
		publish_intent(APP_SYS_INTENT_HOST_SELECT, 2);
		return true;
	default:
		return false;
	}
}

static bool kb_try_pairing_intent(uint32_t input_code)
{
	if (!pairing_is_confirm_pending()) {
		return false;
	}

	switch (input_code) {
	case INPUT_KEY_0:
		publish_intent(APP_SYS_INTENT_PAIRING_ACCEPT, 0);
		kb_pairing_release_pending = true;
		return true;
	case INPUT_KEY_1:
		publish_intent(APP_SYS_INTENT_PAIRING_REJECT, 0);
		kb_pairing_release_pending = true;
		return true;
	default:
		return false;
	}
}

static void kb_on_press(uint32_t input_code)
{
	if (input_code == INPUT_BTN_EXTRA) {
		kb_fn_active = true;
		return;
	}

	if (kb_try_fn_intent(input_code)) {
		return;
	}

	if (kb_try_pairing_intent(input_code)) {
		return;
	}

	uint32_t keycode = kb_fn_mapping(input_code, kb_fn_active);
	if (keycode == 0) {
		return;
	}

	uint8_t modifier = input_to_hid_modifier(keycode);
	if (modifier != 0) {
		kb_hid_report.modifiers |= modifier;
		kb_inputs_src.modifiers[find_lsb_set(modifier) - 1] = input_code;
		return;
	}

	int16_t ret = input_to_hid_code(keycode);
	if (ret < 0) {
		return;
	}

	uint16_t hid_code = (uint16_t)ret;
	for (size_t i = 0; i < ARRAY_SIZE(kb_hid_report.keycodes); i++) {
		if (kb_hid_report.keycodes[i] == 0) {
			kb_hid_report.keycodes[i] = hid_code;
			kb_inputs_src.keycodes[i] = input_code;
			break;
		}
	}
}

static void kb_on_release(uint32_t input_code)
{
	if (input_code == INPUT_BTN_EXTRA) {
		kb_fn_active = false;
		return;
	}

	if (kb_pairing_release_pending &&
	    (input_code == INPUT_KEY_0 || input_code == INPUT_KEY_1)) {
		kb_pairing_release_pending = false;
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(kb_inputs_src.modifiers); i++) {
		if (kb_inputs_src.modifiers[i] == input_code) {
			kb_hid_report.modifiers &= ~(1u << i);
			kb_inputs_src.modifiers[i] = 0;
			return;
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(kb_inputs_src.keycodes); i++) {
		if (kb_inputs_src.keycodes[i] == input_code) {
			for (size_t j = i; j < ARRAY_SIZE(kb_inputs_src.keycodes) - 1; j++) {
				kb_hid_report.keycodes[j] = kb_hid_report.keycodes[j + 1];
				kb_inputs_src.keycodes[j] = kb_inputs_src.keycodes[j + 1];
			}
			kb_hid_report.keycodes[ARRAY_SIZE(kb_inputs_src.keycodes) - 1] = 0;
			kb_inputs_src.keycodes[ARRAY_SIZE(kb_inputs_src.keycodes) - 1] = 0;
			return;
		}
	}
}

static void kb_publish_report_if_changed(void)
{
	if (memcmp(&kb_hid_report, &kb_last_published, sizeof(kb_hid_report)) == 0) {
		return;
	}
	kb_last_published = kb_hid_report;
	(void)zbus_chan_pub(&chan_hid_report, &kb_hid_report, K_NO_WAIT);
}

static void kb_on_key_event(const struct zbus_channel *chan)
{
	const struct app_key_event *evt = zbus_chan_const_msg(chan);

	if (evt->pressed) {
		kb_on_press(evt->code);
	} else {
		kb_on_release(evt->code);
	}

	kb_publish_report_if_changed();
}

ZBUS_LISTENER_DEFINE(kb_listener, kb_on_key_event);
ZBUS_CHAN_ADD_OBS(chan_key_event, kb_listener, 4);
