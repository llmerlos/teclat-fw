/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/zbus/zbus.h>

#include "events.h"
#include "idle.h"
#include "input.h"

#define IDLE_TIMEOUT K_MINUTES(CONFIG_APP_IDLE_TIMEOUT_MIN)

static void idle_enter_sleep(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(idle_work, idle_enter_sleep);

static void on_activity(const struct zbus_channel *chan)
{
	ARG_UNUSED(chan);
	(void)k_work_reschedule(&idle_work, IDLE_TIMEOUT);
}

ZBUS_LISTENER_DEFINE(idle_listener, on_activity);
ZBUS_CHAN_ADD_OBS(chan_activity, idle_listener, 4);

static void idle_enter_sleep(struct k_work *work)
{
	ARG_UNUSED(work);
	int err;

	err = input_prepare_for_sleep();
	if (err) {
		printk("Wake-source config failed (err %d), retrying later\n", err);
		(void)k_work_reschedule(&idle_work, IDLE_TIMEOUT);
		return;
	}

	printk("Idle %d min, entering system off\n", CONFIG_APP_IDLE_TIMEOUT_MIN);
	sys_poweroff();
}

void idle_init(void)
{
	(void)k_work_reschedule(&idle_work, IDLE_TIMEOUT);
}
