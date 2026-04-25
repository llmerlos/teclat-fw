/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_KEYBOARD_H_
#define APP_KEYBOARD_H_

#include <stdbool.h>

/* Toggle the "type next character of hello-world" key. On release the
 * rolling index advances so the next press emits the next character.
 */
void keyboard_text_button(bool pressed);

/* Toggle the shift modifier. */
void keyboard_shift_button(bool pressed);

#endif /* APP_KEYBOARD_H_ */
