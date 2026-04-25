/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_BLE_H_
#define APP_BLE_H_

#include <stdbool.h>

/* Start (or restart) connectable advertising. */
void ble_advertising_start(void);

/* True while advertising is active. main() uses this to drive the blink LED. */
bool ble_is_advertising(void);

#endif /* APP_BLE_H_ */
