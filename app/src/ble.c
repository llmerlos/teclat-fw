/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <string.h>

#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/settings/settings.h>

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

#define ADV_NONE (-1)

static atomic_t active_host_id = ATOMIC_INIT(0);
static int advertising_id = ADV_NONE;

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

uint8_t ble_get_active_host(void)
{
	return (uint8_t)atomic_get(&active_host_id);
}

struct slot_check {
	uint8_t slot;
	bool found;
};

static void slot_check_cb(struct bt_conn *conn, void *data)
{
	struct slot_check *sc = data;
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) == 0 && info.id == sc->slot) {
		sc->found = true;
	}
}

static bool slot_has_conn(uint8_t slot)
{
	struct slot_check sc = { .slot = slot, .found = false };

	bt_conn_foreach(BT_CONN_TYPE_LE, slot_check_cb, &sc);
	return sc.found;
}

static void adv_stop(void)
{
	int err;

	if (advertising_id == ADV_NONE) {
		return;
	}
	err = bt_le_adv_stop();
	if (err && err != -EALREADY) {
		printk("Advertising stop failed (err %d)\n", err);
	}
	advertising_id = ADV_NONE;
}

static int adv_start_for(uint8_t id)
{
	struct bt_le_adv_param adv_param = *BT_LE_ADV_PARAM(
		BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);
	int err;

	if (advertising_id != ADV_NONE && advertising_id != id) {
		adv_stop();
	}

	adv_param.id = id;
	err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err == -EALREADY) {
		advertising_id = id;
		return 0;
	}
	if (err) {
		printk("Advertising failed to start (id=%u err=%d)\n", id, err);
		return err;
	}

	advertising_id = id;
	printk("Advertising started for slot %u\n", id);
	return 0;
}

void ble_advertising_start(void)
{
	(void)adv_start_for(ble_get_active_host());
}

bool ble_is_advertising(void)
{
	return advertising_id != ADV_NONE;
}

static int active_host_settings_set(const char *key, size_t len, settings_read_cb read_cb,
				    void *cb_arg)
{
	if (settings_name_steq(key, "active", NULL)) {
		uint8_t v;
		ssize_t n;

		if (len != sizeof(v)) {
			return -EINVAL;
		}
		n = read_cb(cb_arg, &v, sizeof(v));
		if (n != sizeof(v)) {
			return n < 0 ? n : -EIO;
		}
		if (v < CONFIG_APP_HOST_SLOTS) {
			atomic_set(&active_host_id, v);
		}
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(app_host, "app/host", NULL, active_host_settings_set, NULL, NULL);

static void persist_active_host(uint8_t slot)
{
	int err = settings_save_one("app/host/active", &slot, sizeof(slot));

	if (err) {
		printk("Active-host save failed (err %d)\n", err);
	}
}

int ble_init(void)
{
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = ARRAY_SIZE(addrs);

	bt_id_get(addrs, &count);

	for (size_t i = count; i < CONFIG_APP_HOST_SLOTS; i++) {
		int id = bt_id_create(NULL, NULL);

		if (id < 0) {
			printk("bt_id_create failed (err %d)\n", id);
			return id;
		}
		printk("Created identity %d\n", id);
	}

	printk("Active host slot: %u\n", ble_get_active_host());
	return 0;
}

static void select_host(uint8_t slot)
{
	if (slot >= CONFIG_APP_HOST_SLOTS) {
		printk("HOST_SELECT: invalid slot %u\n", slot);
		return;
	}
	if (slot == ble_get_active_host() && (slot_has_conn(slot) || advertising_id == slot)) {
		return;
	}

	atomic_set(&active_host_id, slot);
	persist_active_host(slot);

	if (slot_has_conn(slot)) {
		adv_stop();
	} else {
		(void)adv_start_for(slot);
	}
}

struct disconnect_ctx {
	uint8_t slot;
};

static void disconnect_slot_cb(struct bt_conn *conn, void *data)
{
	const struct disconnect_ctx *ctx = data;
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) != 0 || info.id != ctx->slot) {
		return;
	}
	(void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void clear_active_bonds(void)
{
	uint8_t slot = ble_get_active_host();
	struct disconnect_ctx ctx = { .slot = slot };
	int err;

	bt_conn_foreach(BT_CONN_TYPE_LE, disconnect_slot_cb, &ctx);

	err = bt_unpair(slot, BT_ADDR_LE_ANY);
	if (err) {
		printk("bt_unpair slot=%u err=%d\n", slot, err);
	} else {
		printk("Cleared bonds for slot %u\n", slot);
	}

	(void)adv_start_for(slot);
}

static void ble_connected_cb(struct bt_conn *conn, uint8_t err)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s 0x%02x %s\n", addr, err, bt_hci_err_to_str(err));
		return;
	}

	if (bt_conn_get_info(conn, &info) != 0) {
		printk("Connected (no info) %s\n", addr);
		return;
	}

	printk("Connected slot=%u addr=%s\n", info.id, addr);
	dk_set_led_on(CON_STATUS_LED);

	if (ble_hid_on_connected(conn) != 0) {
		return;
	}

	if (advertising_id == info.id) {
		/* The host stack stopped advertising on connect. */
		advertising_id = ADV_NONE;
	}
}

static void ble_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	(void)bt_conn_get_info(conn, &info);

	printk("Disconnected slot=%u addr=%s reason 0x%02x %s\n", info.id, addr, reason,
	       bt_hci_err_to_str(reason));

	(void)ble_hid_on_disconnected(conn);

	if (!ble_hid_has_active_clients()) {
		dk_set_led_off(CON_STATUS_LED);
	}

	if (info.id == ble_get_active_host()) {
		(void)adv_start_for(info.id);
	}
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

static void ble_on_intent(const struct zbus_channel *chan)
{
	const struct app_sys_intent *intent = zbus_chan_const_msg(chan);

	switch (intent->kind) {
	case APP_SYS_INTENT_HOST_SELECT:
		select_host(intent->arg);
		break;
	case APP_SYS_INTENT_CLEAR_BONDS:
		clear_active_bonds();
		break;
	default:
		break;
	}
}

ZBUS_LISTENER_DEFINE(ble_listener, ble_on_intent);
ZBUS_CHAN_ADD_OBS(chan_sys_intent, ble_listener, 4);
