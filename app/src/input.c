/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "events.h"

LOG_MODULE_REGISTER(app_input, CONFIG_LOG_DEFAULT_LEVEL);

/* Input source selected via the `app,input-source` DT chosen. Falls back to
 * the `buttons` node label so existing overlays keep working. To swap the
 * physical input (matrix kscan, ADC threshold driver, encoder, ...), point
 * the chosen at a different node — no app code changes required.
 */
#if DT_HAS_CHOSEN(app_input_source)
#define INPUT_SRC_NODE DT_CHOSEN(app_input_source)
#else
#define INPUT_SRC_NODE DT_NODELABEL(buttons)
#endif

static void on_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	struct app_key_event ke = {
		.code = evt->code,
		.pressed = evt->value != 0,
	};
	(void)zbus_chan_pub(&chan_key_event, &ke, K_NO_WAIT);

	struct app_activity_event ae = { .source = 0 };
	(void)zbus_chan_pub(&chan_activity, &ae, K_NO_WAIT);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(INPUT_SRC_NODE), on_input_cb, NULL);
