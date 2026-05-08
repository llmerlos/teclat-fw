/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_IDLE_H_
#define APP_IDLE_H_

/* Arm the inactivity timer. Each chan_activity event resets it; on
 * expiry the device enters System OFF and wakes on a button press.
 */
void idle_init(void);

#endif /* APP_IDLE_H_ */
