/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_HID_H_
#define APP_HID_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct bt_conn;

/* Initialize the HID service. Call once before bt_enable(). */
void ble_hid_init(void);

/* Send a press / release for the given key codes to all active clients. */
int ble_hid_buttons_press(const uint8_t *keys, size_t cnt);
int ble_hid_buttons_release(const uint8_t *keys, size_t cnt);

/* Connection lifecycle hooks. ble.c calls these from its conn callbacks. */
int ble_hid_on_connected(struct bt_conn *conn);
int ble_hid_on_disconnected(struct bt_conn *conn);

/* True if any HID client is currently connected. ble.c uses this to decide
 * whether to keep the CON_STATUS_LED on after one client disconnects. */
bool ble_hid_has_active_clients(void);

/* Number of currently-connected HID clients (0..CONFIG_BT_HIDS_MAX_CLIENT_COUNT). */
size_t ble_hid_active_client_count(void);

#endif /* APP_HID_H_ */
