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

static struct evt_hid_report tec_hid_report;
static struct evt_hid_report tec_last_published;

static struct tec_input_stack {
	uint32_t modifiers[8];
	uint32_t keycodes[EVT_HID_KEYCODES];
} tec_inputs_src;

static bool tec_fn_active;

/* Set after a pairing-pending KEY_0/KEY_1 press is consumed as an intent;
 * makes the matching release a no-op so it doesn't reach the HID path.
 */
static bool tec_pair_release_pending;

static void tec_publish_intent(enum evt_sys_intent_kind kind, uint8_t arg)
{
	struct evt_sys_intent intent = { .kind = kind, .arg = arg };
	(void)zbus_chan_pub(&chan_sys_intent, &intent, K_NO_WAIT);
}

static uint32_t tec_fn_mapping(uint32_t input_code, bool is_fn_active)
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
static bool tec_try_fn_intent(uint32_t input_code)
{
	if (!tec_fn_active) {
		return false;
	}

	switch (input_code) {
	case INPUT_KEY_0:
		tec_publish_intent(EVT_SYS_INTENT_CLEAR_BONDS, 0);
		return true;
	case INPUT_KEY_1:
		tec_publish_intent(EVT_SYS_INTENT_HOST_SELECT, 0);
		return true;
	case INPUT_KEY_2:
		tec_publish_intent(EVT_SYS_INTENT_HOST_SELECT, 1);
		return true;
	case INPUT_KEY_3:
		tec_publish_intent(EVT_SYS_INTENT_HOST_SELECT, 2);
		return true;
	default:
		return false;
	}
}

static bool tec_try_pair_intent(uint32_t input_code)
{
	if (!pair_is_confirm_pending()) {
		return false;
	}

	switch (input_code) {
	case INPUT_KEY_0:
		tec_publish_intent(EVT_SYS_INTENT_PAIRING_ACCEPT, 0);
		tec_pair_release_pending = true;
		return true;
	case INPUT_KEY_1:
		tec_publish_intent(EVT_SYS_INTENT_PAIRING_REJECT, 0);
		tec_pair_release_pending = true;
		return true;
	default:
		return false;
	}
}

static void tec_on_press(uint32_t input_code)
{
	if (input_code == INPUT_BTN_EXTRA) {
		tec_fn_active = true;
		return;
	}

	if (tec_try_fn_intent(input_code)) {
		return;
	}

	if (tec_try_pair_intent(input_code)) {
		return;
	}

	uint32_t keycode = tec_fn_mapping(input_code, tec_fn_active);
	if (keycode == 0) {
		return;
	}

	uint8_t modifier = input_to_hid_modifier(keycode);
	if (modifier != 0) {
		tec_hid_report.modifiers |= modifier;
		tec_inputs_src.modifiers[find_lsb_set(modifier) - 1] = input_code;
		return;
	}

	int16_t ret = input_to_hid_code(keycode);
	if (ret < 0) {
		return;
	}

	uint16_t hid_code = (uint16_t)ret;
	for (size_t i = 0; i < ARRAY_SIZE(tec_hid_report.keycodes); i++) {
		if (tec_hid_report.keycodes[i] == 0) {
			tec_hid_report.keycodes[i] = hid_code;
			tec_inputs_src.keycodes[i] = input_code;
			break;
		}
	}
}

static void tec_on_release(uint32_t input_code)
{
	if (input_code == INPUT_BTN_EXTRA) {
		tec_fn_active = false;
		return;
	}

	if (tec_pair_release_pending &&
	    (input_code == INPUT_KEY_0 || input_code == INPUT_KEY_1)) {
		tec_pair_release_pending = false;
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(tec_inputs_src.modifiers); i++) {
		if (tec_inputs_src.modifiers[i] == input_code) {
			tec_hid_report.modifiers &= ~(1u << i);
			tec_inputs_src.modifiers[i] = 0;
			return;
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(tec_inputs_src.keycodes); i++) {
		if (tec_inputs_src.keycodes[i] == input_code) {
			for (size_t j = i; j < ARRAY_SIZE(tec_inputs_src.keycodes) - 1; j++) {
				tec_hid_report.keycodes[j] = tec_hid_report.keycodes[j + 1];
				tec_inputs_src.keycodes[j] = tec_inputs_src.keycodes[j + 1];
			}
			tec_hid_report.keycodes[ARRAY_SIZE(tec_inputs_src.keycodes) - 1] = 0;
			tec_inputs_src.keycodes[ARRAY_SIZE(tec_inputs_src.keycodes) - 1] = 0;
			return;
		}
	}
}

static void tec_publish_report_if_changed(void)
{
	if (memcmp(&tec_hid_report, &tec_last_published, sizeof(tec_hid_report)) == 0) {
		return;
	}
	tec_last_published = tec_hid_report;
	(void)zbus_chan_pub(&chan_hid_report, &tec_hid_report, K_NO_WAIT);
}

static void tec_on_key_event(const struct zbus_channel *chan)
{
	const struct evt_key *evt = zbus_chan_const_msg(chan);

	if (evt->pressed) {
		tec_on_press(evt->code);
	} else {
		tec_on_release(evt->code);
	}

	tec_publish_report_if_changed();
}

ZBUS_LISTENER_DEFINE(tec_listener, tec_on_key_event);
ZBUS_CHAN_ADD_OBS(chan_key, tec_listener, 4);
