/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_EVENTS_H_
#define APP_EVENTS_H_

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/zbus/zbus.h>

/* Raw key event from the input source. One published per press, one per
 * release. Producers are input drivers behind the input_source DT chosen.
 */
struct app_key_event {
	uint16_t code;
	bool pressed;
};

/* Marker that the user did something. The idle/pm module subscribes to
 * reset its inactivity timer.
 */
struct app_activity_event {
	uint8_t source;
};

/* Semantic intents emitted by the keyboard policy layer. Subscribers act:
 *   - pairing.c handles ACCEPT/REJECT
 *   - ble.c handles HOST_SELECT/CLEAR_BONDS
 */
enum app_sys_intent_kind {
	APP_SYS_INTENT_HOST_SELECT,
	APP_SYS_INTENT_PAIRING_ACCEPT,
	APP_SYS_INTENT_PAIRING_REJECT,
	APP_SYS_INTENT_CLEAR_BONDS,
};

struct app_sys_intent {
	enum app_sys_intent_kind kind;
	uint8_t arg; /* HOST_SELECT: slot index; otherwise unused */
};

ZBUS_CHAN_DECLARE(chan_key_event);
ZBUS_CHAN_DECLARE(chan_activity);
ZBUS_CHAN_DECLARE(chan_sys_intent);

#endif /* APP_EVENTS_H_ */
