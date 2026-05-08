/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include "battery.h"

#define BATTERY_NOTIFY_INTERVAL_MS 1000

static void battery_notify(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(battery_work, battery_notify);

static void battery_notify(struct k_work *work)
{
	ARG_UNUSED(work);

	uint8_t level = bt_bas_get_battery_level();

	level--;
	if (!level) {
		level = 100U;
	}
	bt_bas_set_battery_level(level);

	(void)k_work_reschedule(&battery_work, K_MSEC(BATTERY_NOTIFY_INTERVAL_MS));
}

void battery_init(void)
{
	(void)k_work_reschedule(&battery_work, K_MSEC(BATTERY_NOTIFY_INTERVAL_MS));
}
