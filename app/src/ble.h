/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_BLE_H_
#define APP_BLE_H_

#include <stdbool.h>
#include <stdint.h>

/* Ensure CONFIG_APP_HOST_SLOTS Bluetooth identities exist. Call once,
 * after bt_enable() and settings_load(). Idempotent.
 */
int ble_init(void);

/* Start (or restart) connectable advertising on the active host slot. */
void ble_advertising_start(void);

/* True while advertising is active on any slot. main() uses this for the
 * blink LED.
 */
bool ble_is_advertising(void);

/* Slot index of the currently active host (0..APP_HOST_SLOTS-1). */
uint8_t ble_get_active_host(void);

#endif /* APP_BLE_H_ */
