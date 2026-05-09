/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef IDLE_H_
#define IDLE_H_

/* Arm the inactivity timer. Each chan_activity event resets it; on
 * expiry the device enters System OFF and wakes on a button press.
 */
void idle_init(void);

#endif /* IDLE_H_ */
