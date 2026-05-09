/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef EVENTS_H_
#define EVENTS_H_

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/zbus/zbus.h>

/* Raw key event from the input source. One published per press, one per
 * release. Producers are input drivers behind the input_source DT chosen.
 */
struct evt_key {
	uint16_t code;
	bool pressed;
};

/* Marker that the user did something. The idle/pm module subscribes to
 * reset its inactivity timer.
 */
struct evt_activity {
	uint8_t source;
};

/* Semantic intents emitted by the keyboard policy layer. Subscribers act:
 *   - pairing.c handles ACCEPT/REJECT
 *   - ble.c handles HOST_SELECT/CLEAR_BONDS
 */
enum evt_sys_intent_kind {
	EVT_SYS_INTENT_HOST_SELECT,
	EVT_SYS_INTENT_PAIRING_ACCEPT,
	EVT_SYS_INTENT_PAIRING_REJECT,
	EVT_SYS_INTENT_CLEAR_BONDS,
};

struct evt_sys_intent {
	enum evt_sys_intent_kind kind;
	uint8_t arg; /* HOST_SELECT: slot index; otherwise unused */
};

/* HID keyboard report snapshot. Published by keyboard.c whenever the
 * report state changes. hid.c subscribes and pushes to active clients.
 */
#define EVT_HID_KEYCODES 6
struct evt_hid_report {
	uint8_t modifiers;
	uint8_t keycodes[EVT_HID_KEYCODES];
};

ZBUS_CHAN_DECLARE(chan_key);
ZBUS_CHAN_DECLARE(chan_activity);
ZBUS_CHAN_DECLARE(chan_sys_intent);
ZBUS_CHAN_DECLARE(chan_hid_report);

#endif /* EVENTS_H_ */
