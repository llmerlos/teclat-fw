/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef INPUT_H_
#define INPUT_H_

/* Configure the active input source's GPIOs as wake sources for
 * sys_poweroff(). Returns 0 on success, -ENOTSUP if the configured
 * input backend doesn't have a wake-from-System-OFF implementation,
 * or another negative errno on failure.
 */
int inp_prepare_for_sleep(void);

#endif /* INPUT_H_ */
