/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_KEYBOARD_H_
#define APP_KEYBOARD_H_

#include <stdint.h>

void kb_process_input_press(uint32_t input_code);
void kb_process_input_release(uint32_t input_code);

#endif /* APP_KEYBOARD_H_ */
