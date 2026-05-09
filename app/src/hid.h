/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef HID_H_
#define HID_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct bt_conn;

/* Initialize the HID service. Call once before bt_enable(). */
void hid_init(void);

/* Connection lifecycle hooks. ble.c calls these from its conn callbacks. */
int hid_on_connected(struct bt_conn *conn);
int hid_on_disconnected(struct bt_conn *conn);

/* True if any HID client is currently connected. ble.c uses this to decide
 * whether to keep the CON_STATUS_LED on after one client disconnects. */
bool hid_has_active_clients(void);

/* Number of currently-connected HID clients (0..CONFIG_BT_HIDS_MAX_CLIENT_COUNT). */
size_t hid_active_client_count(void);

#endif /* HID_H_ */
