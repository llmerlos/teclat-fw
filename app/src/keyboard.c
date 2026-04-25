/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "keyboard.h"
#include "hid.h"

static const uint8_t hello_world_str[] = {
	0x0b, /* Key h */
	0x08, /* Key e */
	0x0f, /* Key l */
	0x0f, /* Key l */
	0x12, /* Key o */
	0x28, /* Key Return */
};

static const uint8_t shift_key[] = {225};

void keyboard_text_button(bool pressed)
{
	static const uint8_t *chr = hello_world_str;

	if (pressed) {
		hid_buttons_press(chr, 1);
	} else {
		hid_buttons_release(chr, 1);
		if (++chr == (hello_world_str + sizeof(hello_world_str))) {
			chr = hello_world_str;
		}
	}
}

void keyboard_shift_button(bool pressed)
{
	if (pressed) {
		hid_buttons_press(shift_key, 1);
	} else {
		hid_buttons_release(shift_key, 1);
	}
}
