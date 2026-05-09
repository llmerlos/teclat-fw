/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef PAIRING_H_
#define PAIRING_H_

#include <stdbool.h>

/* Initialize the pairing module. Registers bt_conn_auth_cb and
 * bt_conn_auth_info_cb. Call once before bt_enable().
 * Returns 0 on success, negative errno on registration failure.
 */
int pair_init(void);

/* True while at least one numeric-compare passkey is waiting for user
 * confirmation. */
bool pair_is_confirm_pending(void);

#endif /* PAIRING_H_ */
