/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include "events.h"
#include "pairing.h"

LOG_MODULE_REGISTER(pair, CONFIG_LOG_DEFAULT_LEVEL);

static struct k_work pair_work;
struct pair_data_mitm {
	struct bt_conn *conn;
	unsigned int passkey;
};

K_MSGQ_DEFINE(pair_mitm_queue, sizeof(struct pair_data_mitm), CONFIG_BT_HIDS_MAX_CLIENT_COUNT, 4);

static void pair_process(struct k_work *work)
{
	ARG_UNUSED(work);
	int err;
	struct pair_data_mitm pair_data;

	char addr[BT_ADDR_LE_STR_LEN];

	err = k_msgq_peek(&pair_mitm_queue, &pair_data);
	if (err) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(pair_data.conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", addr, pair_data.passkey);
	LOG_INF("Press Button 0 to confirm, Button 1 to reject.");
}

static void pair_auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void pair_auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	int err;

	struct pair_data_mitm pair_data;

	pair_data.conn = bt_conn_ref(conn);
	pair_data.passkey = passkey;

	err = k_msgq_put(&pair_mitm_queue, &pair_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("Pairing queue is full. Purge previous data.");
	}

	/* In the case of multiple pairing requests, trigger
	 * pairing confirmation which needed user interaction only
	 * once to avoid display information about all devices at
	 * the same time. Passkey confirmation for next devices will
	 * be proccess from queue after handling the earlier ones.
	 */
	if (k_msgq_num_used_get(&pair_mitm_queue) == 1) {
		k_work_submit(&pair_work);
	}
}

static void pair_auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Pairing cancelled: %s", addr);
}

static void pair_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}

static void pair_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	struct pair_data_mitm pair_data;

	if (k_msgq_peek(&pair_mitm_queue, &pair_data) != 0) {
		return;
	}

	if (pair_data.conn == conn) {
		bt_conn_unref(pair_data.conn);
		k_msgq_get(&pair_mitm_queue, &pair_data, K_NO_WAIT);
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_ERR("Pairing failed conn: %s, reason %d %s", addr, reason,
		bt_security_err_to_str(reason));
}

static struct bt_conn_auth_cb pair_conn_auth_cbs = {
	.passkey_display = pair_auth_passkey_display,
	.passkey_confirm = pair_auth_passkey_confirm,
	.cancel = pair_auth_cancel,
};

static struct bt_conn_auth_info_cb pair_conn_auth_info_cbs = {.pairing_complete = pair_complete,
							      .pairing_failed = pair_failed};

static void pair_respond(bool accept)
{
	struct pair_data_mitm pair_data;
	struct bt_conn *conn;

	if (k_msgq_get(&pair_mitm_queue, &pair_data, K_NO_WAIT) != 0) {
		return;
	}

	conn = pair_data.conn;

	if (accept) {
		bt_conn_auth_passkey_confirm(conn);
		LOG_INF("Numeric Match, conn %p", (void *)conn);
	} else {
		bt_conn_auth_cancel(conn);
		LOG_INF("Numeric Reject, conn %p", (void *)conn);
	}

	bt_conn_unref(pair_data.conn);

	if (k_msgq_num_used_get(&pair_mitm_queue)) {
		k_work_submit(&pair_work);
	}
}

int pair_init(void)
{
	int err;

	k_work_init(&pair_work, pair_process);

	err = bt_conn_auth_cb_register(&pair_conn_auth_cbs);
	if (err) {
		LOG_ERR("Failed to register authorization callbacks (err %d)", err);
		return err;
	}

	err = bt_conn_auth_info_cb_register(&pair_conn_auth_info_cbs);
	if (err) {
		LOG_ERR("Failed to register authorization info callbacks (err %d)", err);
		return err;
	}

	return 0;
}

bool pair_is_confirm_pending(void)
{
	return k_msgq_num_used_get(&pair_mitm_queue) > 0;
}

static void pair_on_intent(const struct zbus_channel *chan)
{
	const struct evt_sys_intent *intent = zbus_chan_const_msg(chan);

	switch (intent->kind) {
	case EVT_SYS_INTENT_PAIRING_ACCEPT:
		pair_respond(true);
		break;
	case EVT_SYS_INTENT_PAIRING_REJECT:
		pair_respond(false);
		break;
	default:
		break;
	}
}

ZBUS_LISTENER_DEFINE(pair_listener, pair_on_intent);
ZBUS_CHAN_ADD_OBS(chan_sys_intent, pair_listener, 4);
