/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_INPUT_H_
#define APP_INPUT_H_

#include <stdint.h>
#include <stdbool.h>

typedef void (*app_input_cb_t)(uint16_t code, bool pressed);

/* Register the handler that receives button events.
 * Must be called once at startup before any button may be pressed.
 * Passing NULL disables event delivery.
 */
void app_input_register(app_input_cb_t cb);

#endif /* APP_INPUT_H_ */
