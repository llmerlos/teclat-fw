/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#include <dk_buttons_and_leds.h>

#include "ble.h"
#include "events.h"
#include "hid.h"

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define CON_STATUS_LED DK_LED2

static volatile bool is_adv;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

void ble_advertising_start(void)
{
	int err;
	const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
		BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);

	err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		if (err == -EALREADY) {
			printk("Advertising continued\n");
		} else {
			printk("Advertising failed to start (err %d)\n", err);
		}

		return;
	}

	is_adv = true;
	printk("Advertising successfully started\n");
}

static void ble_connected_cb(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s 0x%02x %s\n", addr, err, bt_hci_err_to_str(err));
		return;
	}

	printk("Connected %s\n", addr);
	dk_set_led_on(CON_STATUS_LED);

	if (ble_hid_on_connected(conn) != 0) {
		return;
	}

	/* If another slot is still free, keep advertising; otherwise stop. */
	if (ble_hid_active_client_count() < CONFIG_BT_HIDS_MAX_CLIENT_COUNT) {
		ble_advertising_start();
		return;
	}
	is_adv = false;
}

static void ble_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	(void)ble_hid_on_disconnected(conn);

	if (!ble_hid_has_active_clients()) {
		dk_set_led_off(CON_STATUS_LED);
	}

	ble_advertising_start();
}

static void ble_security_changed_cb(struct bt_conn *conn, bt_security_t level,
				    enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d %s\n", addr, level, err,
		       bt_security_err_to_str(err));
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = ble_connected_cb,
	.disconnected = ble_disconnected_cb,
	.security_changed = ble_security_changed_cb,
};

bool ble_is_advertising(void)
{
	return is_adv;
}

static void ble_on_intent(const struct zbus_channel *chan)
{
	const struct app_sys_intent *intent = zbus_chan_const_msg(chan);

	switch (intent->kind) {
	case APP_SYS_INTENT_HOST_SELECT:
		printk("Intent HOST_SELECT slot=%u (not yet implemented)\n", intent->arg);
		break;
	case APP_SYS_INTENT_CLEAR_BONDS:
		printk("Intent CLEAR_BONDS (not yet implemented)\n");
		break;
	default:
		break;
	}
}

ZBUS_LISTENER_DEFINE(ble_listener, ble_on_intent);
ZBUS_CHAN_ADD_OBS(chan_sys_intent, ble_listener, 4);
