/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "events.h"
#include "input.h"

LOG_MODULE_REGISTER(app_input, CONFIG_LOG_DEFAULT_LEVEL);

/* Input source selected via the `app,input-source` DT chosen. Falls back to
 * the `buttons` node label so existing overlays keep working. To swap the
 * physical input (matrix kscan, ADC threshold driver, encoder, ...), point
 * the chosen at a different node — no app code changes required apart from
 * extending input_prepare_for_sleep() with the matching wake-source setup.
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

#if DT_NODE_HAS_COMPAT(INPUT_SRC_NODE, gpio_keys)

#define WAKE_SPEC_INIT(node_id) GPIO_DT_SPEC_GET(node_id, gpios),

static const struct gpio_dt_spec wake_specs[] = {
	DT_FOREACH_CHILD(INPUT_SRC_NODE, WAKE_SPEC_INIT)
};

int input_prepare_for_sleep(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(wake_specs); i++) {
		int err = gpio_pin_interrupt_configure_dt(&wake_specs[i],
							  GPIO_INT_LEVEL_ACTIVE);
		if (err) {
			LOG_ERR("wake config pin %u failed (err %d)",
				wake_specs[i].pin, err);
			return err;
		}
	}
	return 0;
}

#else  /* unsupported input backend */

int input_prepare_for_sleep(void)
{
	LOG_WRN("No wake-source implementation for the configured input backend");
	return -ENOTSUP;
}

#endif
