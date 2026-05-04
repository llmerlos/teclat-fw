/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <bluetooth/services/hids.h>
#include <dk_buttons_and_leds.h>

#include "hid.h"

#define BASE_USB_HID_SPEC_VERSION 0x0101

#define OUTPUT_REPORT_MAX_LEN            1
#define OUTPUT_REPORT_BIT_MASK_CAPS_LOCK 0x02
#define INPUT_REP_KEYS_REF_ID            0
#define OUTPUT_REP_KEYS_REF_ID           0

/* ********************* */
/* Buttons configuration */

/* Note: The configuration below is the same as BOOT mode configuration
 * This simplifies the code as the BOOT mode is the same as REPORT mode.
 * Changing this configuration would require separate implementation of
 * BOOT mode report generation.
 */
#define KEY_CTRL_CODE_MIN 224 /* Control key codes - required 8 of them */
#define KEY_CTRL_CODE_MAX 231 /* Control key codes - required 8 of them */
#define KEY_CODE_MIN      0   /* Normal key codes */
#define KEY_CODE_MAX      101 /* Normal key codes */
#define KEY_PRESS_MAX                                                                              \
	6 /* Maximum number of non-control keys                                                    \
	   * pressed simultaneously                                                                \
	   */

/* Number of bytes in key report
 *
 * 1B - control keys
 * 1B - reserved
 * rest - non-control keys
 */
#define INPUT_REPORT_KEYS_MAX_LEN (1 + 1 + KEY_PRESS_MAX)

/* Current report map construction requires exactly 8 buttons */
BUILD_ASSERT((KEY_CTRL_CODE_MAX - KEY_CTRL_CODE_MIN) + 1 == 8);

#define LED_CAPS_LOCK DK_LED3

/* OUT report internal indexes.
 *
 * This is a position in internal report table and is not related to
 * report ID.
 */
enum {
	OUTPUT_REP_KEYS_IDX = 0
};

/* INPUT report internal indexes.
 *
 * This is a position in internal report table and is not related to
 * report ID.
 */
enum {
	INPUT_REP_KEYS_IDX = 0
};

/* HIDS instance. */
BT_HIDS_DEF(hids_obj, OUTPUT_REPORT_MAX_LEN, INPUT_REPORT_KEYS_MAX_LEN);

static struct conn_mode {
	struct bt_conn *conn;
	bool in_boot_mode;
} hid_clients[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];

/* Current report status
 */
static struct keyboard_state {
	uint8_t ctrl_keys_state; /* Current keys state */
	uint8_t keys_state[KEY_PRESS_MAX];
} hid_keyboard_state;

static void caps_lock_handler(const struct bt_hids_rep *rep)
{
	uint8_t report_val = ((*rep->data) & OUTPUT_REPORT_BIT_MASK_CAPS_LOCK) ? 1 : 0;
	dk_set_led(LED_CAPS_LOCK, report_val);
}

static void hids_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (!write) {
		printk("Output report read\n");
		return;
	};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Output report has been received %s\n", addr);
	caps_lock_handler(rep);
}

static void hids_boot_kb_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (!write) {
		printk("Output report read\n");
		return;
	};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Boot Keyboard Output report has been received %s\n", addr);
	caps_lock_handler(rep);
}

static void hids_pm_evt_handler(enum bt_hids_pm_evt evt, struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	size_t i;

	for (i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (hid_clients[i].conn == conn) {
			break;
		}
	}

	if (i >= CONFIG_BT_HIDS_MAX_CLIENT_COUNT) {
		printk("Cannot find connection handle when processing PM");
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	switch (evt) {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		printk("Boot mode entered %s\n", addr);
		hid_clients[i].in_boot_mode = true;
		break;

	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		printk("Report mode entered %s\n", addr);
		hid_clients[i].in_boot_mode = false;
		break;

	default:
		break;
	}
}

void ble_hid_init(void)
{
	int err;
	struct bt_hids_init_param hids_init_obj = {0};
	struct bt_hids_inp_rep *hids_inp_rep;
	struct bt_hids_outp_feat_rep *hids_outp_rep;

	static const uint8_t report_map[] = {
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x06, /* Usage (Keyboard) */
		0xA1, 0x01, /* Collection (Application) */

	/* Keys */
#if INPUT_REP_KEYS_REF_ID
		0x85, INPUT_REP_KEYS_REF_ID,
#endif
		0x05, 0x07, /* Usage Page (Key Codes) */
		0x19, 0xe0, /* Usage Minimum (224) */
		0x29, 0xe7, /* Usage Maximum (231) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x01, /* Logical Maximum (1) */
		0x75, 0x01, /* Report Size (1) */
		0x95, 0x08, /* Report Count (8) */
		0x81, 0x02, /* Input (Data, Variable, Absolute) */

		0x95, 0x01, /* Report Count (1) */
		0x75, 0x08, /* Report Size (8) */
		0x81, 0x01, /* Input (Constant) reserved byte(1) */

		0x95, 0x06, /* Report Count (6) */
		0x75, 0x08, /* Report Size (8) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x65, /* Logical Maximum (101) */
		0x05, 0x07, /* Usage Page (Key codes) */
		0x19, 0x00, /* Usage Minimum (0) */
		0x29, 0x65, /* Usage Maximum (101) */
		0x81, 0x00, /* Input (Data, Array) Key array(6 bytes) */

	/* LED */
#if OUTPUT_REP_KEYS_REF_ID
		0x85, OUTPUT_REP_KEYS_REF_ID,
#endif
		0x95, 0x05, /* Report Count (5) */
		0x75, 0x01, /* Report Size (1) */
		0x05, 0x08, /* Usage Page (Page# for LEDs) */
		0x19, 0x01, /* Usage Minimum (1) */
		0x29, 0x05, /* Usage Maximum (5) */
		0x91, 0x02, /* Output (Data, Variable, Absolute), */
			    /* Led report */
		0x95, 0x01, /* Report Count (1) */
		0x75, 0x03, /* Report Size (3) */
		0x91, 0x01, /* Output (Data, Variable, Absolute), */
			    /* Led report padding */

		0xC0 /* End Collection (Application) */
	};

	hids_init_obj.rep_map.data = report_map;
	hids_init_obj.rep_map.size = sizeof(report_map);

	hids_init_obj.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_obj.info.b_country_code = 0x00;
	hids_init_obj.info.flags = (BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE);

	hids_inp_rep = &hids_init_obj.inp_rep_group_init.reports[INPUT_REP_KEYS_IDX];
	hids_inp_rep->size = INPUT_REPORT_KEYS_MAX_LEN;
	hids_inp_rep->id = INPUT_REP_KEYS_REF_ID;
	hids_init_obj.inp_rep_group_init.cnt++;

	hids_outp_rep = &hids_init_obj.outp_rep_group_init.reports[OUTPUT_REP_KEYS_IDX];
	hids_outp_rep->size = OUTPUT_REPORT_MAX_LEN;
	hids_outp_rep->id = OUTPUT_REP_KEYS_REF_ID;
	hids_outp_rep->handler = hids_outp_rep_handler;
	hids_init_obj.outp_rep_group_init.cnt++;

	hids_init_obj.is_kb = true;
	hids_init_obj.boot_kb_outp_rep_handler = hids_boot_kb_outp_rep_handler;
	hids_init_obj.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &hids_init_obj);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}

/** @brief Function process keyboard state and sends it
 *
 *  @param pstate     The state to be sent
 *  @param boot_mode  Information if boot mode protocol is selected.
 *  @param conn       Connection handler
 *
 *  @return 0 on success or negative error code.
 */
static int key_report_con_send(const struct keyboard_state *state, bool boot_mode,
			       struct bt_conn *conn)
{
	int err = 0;
	uint8_t data[INPUT_REPORT_KEYS_MAX_LEN];
	uint8_t *key_data;
	const uint8_t *key_state;
	size_t n;

	data[0] = state->ctrl_keys_state;
	data[1] = 0;
	key_data = &data[2];
	key_state = state->keys_state;

	for (n = 0; n < KEY_PRESS_MAX; ++n) {
		*key_data++ = *key_state++;
	}
	if (boot_mode) {
		err = bt_hids_boot_kb_inp_rep_send(&hids_obj, conn, data, sizeof(data), NULL);
	} else {
		err = bt_hids_inp_rep_send(&hids_obj, conn, INPUT_REP_KEYS_IDX, data, sizeof(data),
					   NULL);
	}
	return err;
}

/** @brief Function process and send keyboard state to all active connections
 *
 * Function process global keyboard state and send it to all connected
 * clients.
 *
 * @return 0 on success or negative error code.
 */
static int key_report_send(void)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (hid_clients[i].conn) {
			int err;

			err = key_report_con_send(&hid_keyboard_state, hid_clients[i].in_boot_mode,
						  hid_clients[i].conn);
			if (err) {
				printk("Key report send error: %d\n", err);
				return err;
			}
		}
	}
	return 0;
}

/** @brief Change key code to ctrl code mask
 *
 *  Function changes the key code to the mask in the control code
 *  field inside the raport.
 *  Returns 0 if key code is not a control key.
 *
 *  @param key Key code
 *
 *  @return Mask of the control key or 0.
 */
static uint8_t button_ctrl_code(uint8_t key)
{
	if (KEY_CTRL_CODE_MIN <= key && key <= KEY_CTRL_CODE_MAX) {
		return (uint8_t)(1U << (key - KEY_CTRL_CODE_MIN));
	}
	return 0;
}

static int hid_kbd_state_key_set(uint8_t key)
{
	uint8_t ctrl_mask = button_ctrl_code(key);

	if (ctrl_mask) {
		hid_keyboard_state.ctrl_keys_state |= ctrl_mask;
		return 0;
	}
	for (size_t i = 0; i < KEY_PRESS_MAX; ++i) {
		if (hid_keyboard_state.keys_state[i] == 0) {
			hid_keyboard_state.keys_state[i] = key;
			return 0;
		}
	}
	/* All slots busy */
	return -EBUSY;
}

static int hid_kbd_state_key_clear(uint8_t key)
{
	uint8_t ctrl_mask = button_ctrl_code(key);

	if (ctrl_mask) {
		hid_keyboard_state.ctrl_keys_state &= ~ctrl_mask;
		return 0;
	}
	for (size_t i = 0; i < KEY_PRESS_MAX; ++i) {
		if (hid_keyboard_state.keys_state[i] == key) {
			hid_keyboard_state.keys_state[i] = 0;
			return 0;
		}
	}
	/* Key not found */
	return -EINVAL;
}

/** @brief Press a button and send report
 *
 *  @note Functions to manipulate hid state are not reentrant
 *  @param keys
 *  @param cnt
 *
 *  @return 0 on success or negative error code.
 */
int ble_hid_buttons_press(const uint8_t *keys, size_t cnt)
{
	while (cnt--) {
		int err;

		err = hid_kbd_state_key_set(*keys++);
		if (err) {
			printk("Cannot set selected key.\n");
			return err;
		}
	}

	return key_report_send();
}

/** @brief Release the button and send report
 *
 *  @note Functions to manipulate hid state are not reentrant
 *  @param keys
 *  @param cnt
 *
 *  @return 0 on success or negative error code.
 */
int ble_hid_buttons_release(const uint8_t *keys, size_t cnt)
{
	while (cnt--) {
		int err;

		err = hid_kbd_state_key_clear(*keys++);
		if (err) {
			printk("Cannot clear selected key.\n");
			return err;
		}
	}

	return key_report_send();
}

int ble_hid_on_connected(struct bt_conn *conn)
{
	int err;

	err = bt_hids_connected(&hids_obj, conn);
	if (err) {
		printk("Failed to notify HID service about connection\n");
		return err;
	}

	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!hid_clients[i].conn) {
			hid_clients[i].conn = conn;
			hid_clients[i].in_boot_mode = false;
			break;
		}
	}
	return 0;
}

int ble_hid_on_disconnected(struct bt_conn *conn)
{
	int err;

	err = bt_hids_disconnected(&hids_obj, conn);
	if (err) {
		printk("Failed to notify HID service about disconnection\n");
	}

	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (hid_clients[i].conn == conn) {
			hid_clients[i].conn = NULL;
			break;
		}
	}
	return err;
}

bool ble_hid_has_active_clients(void)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (hid_clients[i].conn) {
			return true;
		}
	}
	return false;
}

size_t ble_hid_active_client_count(void)
{
	size_t n = 0;
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (hid_clients[i].conn) {
			n++;
		}
	}
	return n;
}
