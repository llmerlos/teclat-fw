/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include "input.h"

LOG_MODULE_REGISTER(app_input, CONFIG_LOG_DEFAULT_LEVEL);

static app_input_cb_t user_cb;

void app_input_register(app_input_cb_t cb)
{
	user_cb = cb;
}

static void on_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY) {
		return;
	}
	if (!user_cb) {
		return;
	}
	user_cb(evt->code, evt->value != 0);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(buttons)), on_input_cb, NULL);
