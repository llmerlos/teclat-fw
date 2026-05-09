/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BATTERY_H_
#define BATTERY_H_

/* Start the periodic battery-level update.
 * Placeholder: cycles the BAS level for now. Replace with a real ADC
 * source once the battery measurement hardware is wired.
 */
void bat_init(void);

#endif /* BATTERY_H_ */
