#include <zephyr/sys/util.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_hid.h>

#include "keyboard.h"

#define KEYBOARD_HID_SIZE 6
static struct keyboard_report_hid_t {
	uint8_t modifiers;
	uint8_t keycodes[KEYBOARD_HID_SIZE];
} kb_hid_report;

static struct keyboard_input_stack_t {
	uint32_t modifiers[8];
	uint32_t keycodes[KEYBOARD_HID_SIZE];
} kb_inputs_src;

static bool kb_fn_active = false;

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

void kb_process_input_press(uint32_t input_code)
{
	// Fn Layer
	if (input_code == INPUT_BTN_EXTRA) {
		kb_fn_active = true;
		return;
	}

	// Custom work
	uint32_t keycode = kb_fn_mapping(input_code, kb_fn_active);

	// HID regular
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

void kb_process_input_release(uint32_t input_code)
{
	if (input_code == INPUT_BTN_EXTRA) {
		kb_fn_active = false;
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
