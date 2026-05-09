/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <string.h>

#include <zephyr/sys/atomic.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
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

LOG_MODULE_REGISTER(ble, CONFIG_LOG_DEFAULT_LEVEL);

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define ADV_STATUS_LED      DK_LED1
#define CON_STATUS_LED      DK_LED2
#define ADV_LED_INTERVAL_MS 1000

#define ADV_NONE (-1)

static atomic_t ble_active_host_id = ATOMIC_INIT(0);
static int ble_advertising_id = ADV_NONE;

static int ble_adv_led_state;

static void ble_adv_led_blink(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	ble_adv_led_state ^= 1;
	dk_set_led(ADV_STATUS_LED, ble_adv_led_state);
}

static K_TIMER_DEFINE(ble_adv_led_timer, ble_adv_led_blink, NULL);

static void ble_adv_led_start(void)
{
	ble_adv_led_state = 0;
	k_timer_start(&ble_adv_led_timer, K_MSEC(ADV_LED_INTERVAL_MS),
		      K_MSEC(ADV_LED_INTERVAL_MS));
}

static void ble_adv_led_stop(void)
{
	k_timer_stop(&ble_adv_led_timer);
	dk_set_led_off(ADV_STATUS_LED);
}

static const struct bt_data ble_ad[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data ble_sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

uint8_t ble_get_active_host(void)
{
	return (uint8_t)atomic_get(&ble_active_host_id);
}

struct ble_slot_check {
	uint8_t slot;
	bool found;
};

static void ble_slot_check_cb(struct bt_conn *conn, void *data)
{
	struct ble_slot_check *sc = data;
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) == 0 && info.id == sc->slot) {
		sc->found = true;
	}
}

static bool ble_slot_has_conn(uint8_t slot)
{
	struct ble_slot_check sc = { .slot = slot, .found = false };

	bt_conn_foreach(BT_CONN_TYPE_LE, ble_slot_check_cb, &sc);
	return sc.found;
}

static void ble_adv_stop(void)
{
	int err;

	if (ble_advertising_id == ADV_NONE) {
		return;
	}
	err = bt_le_adv_stop();
	if (err && err != -EALREADY) {
		LOG_ERR("Advertising stop failed (err %d)", err);
	}
	ble_advertising_id = ADV_NONE;
	ble_adv_led_stop();
}

static int ble_adv_start_for(uint8_t id)
{
	struct bt_le_adv_param adv_param = *BT_LE_ADV_PARAM(
		BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);
	int err;

	if (ble_advertising_id != ADV_NONE && ble_advertising_id != id) {
		ble_adv_stop();
	}

	adv_param.id = id;
	err = bt_le_adv_start(&adv_param, ble_ad, ARRAY_SIZE(ble_ad), ble_sd, ARRAY_SIZE(ble_sd));
	if (err && err != -EALREADY) {
		LOG_ERR("Advertising failed to start (id=%u err=%d)", id, err);
		return err;
	}

	ble_advertising_id = id;
	ble_adv_led_start();
	if (err != -EALREADY) {
		LOG_INF("Advertising started for slot %u", id);
	}
	return 0;
}

void ble_advertising_start(void)
{
	(void)ble_adv_start_for(ble_get_active_host());
}

static int ble_active_host_settings_set(const char *key, size_t len, settings_read_cb read_cb,
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
			atomic_set(&ble_active_host_id, v);
		}
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(host, "host", NULL, ble_active_host_settings_set, NULL, NULL);

static void ble_persist_active_host(uint8_t slot)
{
	int err = settings_save_one("host/active", &slot, sizeof(slot));

	if (err) {
		LOG_ERR("Active-host save failed (err %d)", err);
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
			LOG_ERR("bt_id_create failed (err %d)", id);
			return id;
		}
		LOG_INF("Created identity %d", id);
	}

	LOG_INF("Active host slot: %u", ble_get_active_host());
	return 0;
}

static void ble_select_host(uint8_t slot)
{
	if (slot >= CONFIG_APP_HOST_SLOTS) {
		LOG_WRN("HOST_SELECT: invalid slot %u", slot);
		return;
	}
	if (slot == ble_get_active_host() &&
	    (ble_slot_has_conn(slot) || ble_advertising_id == slot)) {
		return;
	}

	atomic_set(&ble_active_host_id, slot);
	ble_persist_active_host(slot);

	if (ble_slot_has_conn(slot)) {
		ble_adv_stop();
	} else {
		(void)ble_adv_start_for(slot);
	}
}

struct ble_disconnect_ctx {
	uint8_t slot;
};

static void ble_disconnect_slot_cb(struct bt_conn *conn, void *data)
{
	const struct ble_disconnect_ctx *ctx = data;
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) != 0 || info.id != ctx->slot) {
		return;
	}
	(void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void ble_clear_active_bonds(void)
{
	uint8_t slot = ble_get_active_host();
	struct ble_disconnect_ctx ctx = { .slot = slot };
	int err;

	bt_conn_foreach(BT_CONN_TYPE_LE, ble_disconnect_slot_cb, &ctx);

	err = bt_unpair(slot, BT_ADDR_LE_ANY);
	if (err) {
		LOG_ERR("bt_unpair slot=%u err=%d", slot, err);
	} else {
		LOG_INF("Cleared bonds for slot %u", slot);
	}

	(void)ble_adv_start_for(slot);
}

static void ble_connected_cb(struct bt_conn *conn, uint8_t err)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s 0x%02x %s", addr, err, bt_hci_err_to_str(err));
		return;
	}

	if (bt_conn_get_info(conn, &info) != 0) {
		LOG_WRN("Connected (no info) %s", addr);
		return;
	}

	LOG_INF("Connected slot=%u addr=%s", info.id, addr);
	dk_set_led_on(CON_STATUS_LED);

	if (hid_on_connected(conn) != 0) {
		return;
	}

	if (ble_advertising_id == info.id) {
		/* The host stack stopped advertising on connect. */
		ble_advertising_id = ADV_NONE;
		ble_adv_led_stop();
	}
}

static void ble_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	(void)bt_conn_get_info(conn, &info);

	LOG_INF("Disconnected slot=%u addr=%s reason 0x%02x %s", info.id, addr, reason,
		bt_hci_err_to_str(reason));

	(void)hid_on_disconnected(conn);

	if (!hid_has_active_clients()) {
		dk_set_led_off(CON_STATUS_LED);
	}

	if (info.id == ble_get_active_host()) {
		(void)ble_adv_start_for(info.id);
	}
}

static void ble_security_changed_cb(struct bt_conn *conn, bt_security_t level,
				    enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_ERR("Security failed: %s level %u err %d %s", addr, level, err,
			bt_security_err_to_str(err));
	}
}

BT_CONN_CB_DEFINE(ble_conn_callbacks) = {
	.connected = ble_connected_cb,
	.disconnected = ble_disconnected_cb,
	.security_changed = ble_security_changed_cb,
};

static void ble_on_intent(const struct zbus_channel *chan)
{
	const struct evt_sys_intent *intent = zbus_chan_const_msg(chan);

	switch (intent->kind) {
	case EVT_SYS_INTENT_HOST_SELECT:
		ble_select_host(intent->arg);
		break;
	case EVT_SYS_INTENT_CLEAR_BONDS:
		ble_clear_active_bonds();
		break;
	default:
		break;
	}
}

ZBUS_LISTENER_DEFINE(ble_listener, ble_on_intent);
ZBUS_CHAN_ADD_OBS(chan_sys_intent, ble_listener, 4);
