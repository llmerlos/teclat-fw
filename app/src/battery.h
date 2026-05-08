/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_BATTERY_H_
#define APP_BATTERY_H_

/* Start the periodic battery-level update.
 * Placeholder: cycles the BAS level for now. Replace with a real ADC
 * source once the battery measurement hardware is wired.
 */
void battery_init(void);

#endif /* APP_BATTERY_H_ */
