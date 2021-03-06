/* main.c - Application main entry point */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#define DEVICE_NAME		"Test peripheral"
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)
#define HEART_RATE_APPEARANCE	0x0341

static int read_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		     void *buf, uint16_t len, uint16_t offset)
{
	const char *name = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, name,
				 strlen(name));
}

static int read_appearance(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	uint16_t appearance = sys_cpu_to_le16(HEART_RATE_APPEARANCE);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &appearance,
				 sizeof(appearance));
}

static struct bt_gatt_ccc_cfg hrmc_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED] = {};
static uint8_t simulate_hrm = 0;

static void hrmc_ccc_cfg_changed(uint16_t value)
{
	simulate_hrm = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static int read_blsc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		     void *buf, uint16_t len, uint16_t offset)
{
	uint8_t value = 0x01;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value,
				 sizeof(value));
}

static struct bt_gatt_ccc_cfg  blvl_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED] = {};
static uint8_t simulate_blvl = 0;
static uint8_t battery = 100;
static uint8_t heartrate = 90;

static void blvl_ccc_cfg_changed(uint16_t value)
{
	simulate_blvl = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static int read_blvl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		     void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 sizeof(*value));
}

static void generate_current_time(uint8_t *buf)
{
	uint16_t year;

	/* 'Exact Time 256' contains 'Day Date Time' which contains
	 * 'Date Time' - characteristic contains fields for:
	 * year, month, day, hours, minutes and seconds.
	 */

	year = sys_cpu_to_le16(2015);
	memcpy(buf,  &year, 2); /* year */
	buf[2] = 5; /* months starting from 1 */
	buf[3] = 30; /* day */
	buf[4] = 12; /* hours */
	buf[5] = 45; /* minutes */
	buf[6] = 30; /* seconds */

	/* 'Day of Week' part of 'Day Date Time' */
	buf[7] = 1; /* day of week starting from 1 */

	/* 'Fractions 256 part of 'Exact Time 256' */
	buf[8] = 0;

	/* Adjust reason */
	buf[9] = 0; /* No update, change, etc */
}

static struct bt_gatt_ccc_cfg ct_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED] = {};

static void ct_ccc_cfg_changed(uint16_t value)
{
	/* TODO: Handle value */
}

static uint8_t ct[10];
static uint8_t ct_update = 0;

static int read_ct(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		   void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 sizeof(ct));
}

static int write_ct(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		    const void *buf, uint16_t len, uint16_t offset)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ct)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);
	ct_update = 1;

	return len;
}

static int read_model(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		   void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static int read_manuf(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		      void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

/* Custom Service Variables */
static struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
	0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static struct bt_uuid_128 vnd_enc_uuid = BT_UUID_INIT_128(
	0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static struct bt_uuid_128 vnd_auth_uuid = BT_UUID_INIT_128(
	0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static uint8_t vnd_value[] = { 'V', 'e', 'n', 'd', 'o', 'r' };

static int read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		   void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static int write_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		     const void *buf, uint16_t len, uint16_t offset)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(vnd_value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

#define MAX_DATA 74
static struct vnd_long_value {
	/* TODO: buffer needs to be per connection */
	uint8_t buf[MAX_DATA];
	uint8_t data[MAX_DATA];
} vnd_long_value = {
	.buf = { 'V', 'e', 'n', 'd', 'o', 'r' },
	.data = { 'V', 'e', 'n', 'd', 'o', 'r' },
};

static int read_long_vnd(struct bt_conn *conn,
			 const struct bt_gatt_attr *attr, void *buf,
			 uint16_t len, uint16_t offset)
{
	struct vnd_long_value *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value->data,
				 sizeof(value->data));
}

static int write_long_vnd(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, const void *buf,
			  uint16_t len, uint16_t offset)
{
	struct vnd_long_value *value = attr->user_data;

	if (offset + len > sizeof(value->buf)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	/* Copy to buffer */
	memcpy(value->buf + offset, buf, len);

	return len;
}

static int flush_long_vnd(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, uint8_t flags)
{
	struct vnd_long_value *value = attr->user_data;

	switch (flags) {
	case BT_GATT_FLUSH_DISCARD:
		/* Discard buffer reseting it back with data */
		memcpy(value->buf, value->data, sizeof(value->buf));
		return 0;
	case BT_GATT_FLUSH_SYNC:
		/* Sync buffer to data */
		memcpy(value->data, value->buf, sizeof(value->data));
		return 0;
	}

	return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static const struct bt_uuid_128 vnd_long_uuid = BT_UUID_INIT_128(
	0xf3, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static struct bt_gatt_cep vnd_long_cep = {
	.properties = BT_GATT_CEP_RELIABLE_WRITE,
};

static int signed_value;

static int read_signed(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		       void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 sizeof(signed_value));
}

static int write_signed(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			const void *buf, uint16_t len, uint16_t offset)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(signed_value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

static const struct bt_uuid_128 vnd_signed_uuid = BT_UUID_INIT_128(
	0xf3, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x13,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x13);

static struct bt_gatt_attr gap_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GAP),
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_DEVICE_NAME, BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_GAP_DEVICE_NAME, BT_GATT_PERM_READ,
			   read_name, NULL, DEVICE_NAME),
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_APPEARANCE, BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_GAP_APPEARANCE, BT_GATT_PERM_READ,
			   read_appearance, NULL, NULL),
};

/* Heart Rate Service Declaration */
static struct bt_gatt_attr hrs_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HRS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HRS_MEASUREMENT, BT_GATT_CHRC_NOTIFY),
	BT_GATT_DESCRIPTOR(BT_UUID_HRS_MEASUREMENT, BT_GATT_PERM_READ, NULL,
			   NULL, NULL),
	BT_GATT_CCC(hrmc_ccc_cfg, hrmc_ccc_cfg_changed),
	BT_GATT_CHARACTERISTIC(BT_UUID_HRS_BODY_SENSOR, BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_HRS_BODY_SENSOR, BT_GATT_PERM_READ,
			   read_blsc, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HRS_CONTROL_POINT, BT_GATT_CHRC_WRITE),
	/* TODO: Add write permission and callback */
	BT_GATT_DESCRIPTOR(BT_UUID_HRS_CONTROL_POINT, BT_GATT_PERM_READ, NULL,
			   NULL, NULL),
};

/* Battery Service Declaration */
static struct bt_gatt_attr bas_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
	BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY),
	BT_GATT_DESCRIPTOR(BT_UUID_BAS_BATTERY_LEVEL, BT_GATT_PERM_READ,
			   read_blvl, NULL, &battery),
	BT_GATT_CCC(blvl_ccc_cfg, blvl_ccc_cfg_changed),
};

/* Current Time Service Declaration */
static struct bt_gatt_attr cts_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_CTS),
	BT_GATT_CHARACTERISTIC(BT_UUID_CTS_CURRENT_TIME, BT_GATT_CHRC_READ |
			       BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE),
	BT_GATT_DESCRIPTOR(BT_UUID_CTS_CURRENT_TIME,
			   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			   read_ct, write_ct, ct),
	BT_GATT_CCC(ct_ccc_cfg, ct_ccc_cfg_changed),
};

/* Device Information Service Declaration */
static struct bt_gatt_attr dis_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MODEL_NUMBER, BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_DIS_MODEL_NUMBER, BT_GATT_PERM_READ,
			   read_model, NULL, CONFIG_SOC),
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MANUFACTURER_NAME,
			       BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_DIS_MANUFACTURER_NAME, BT_GATT_PERM_READ,
			   read_manuf, NULL, "Manufacturer"),
};

/* Vendor Primary Service Declaration */
static struct bt_gatt_attr vnd_attrs[] = {
	/* Vendor Primary Service Declaration */
	BT_GATT_PRIMARY_SERVICE(&vnd_uuid),
	BT_GATT_CHARACTERISTIC(&vnd_enc_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE),
	BT_GATT_DESCRIPTOR(&vnd_enc_uuid.uuid,
			   BT_GATT_PERM_READ | BT_GATT_PERM_READ_ENCRYPT |
			   BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_ENCRYPT,
			   read_vnd, write_vnd, vnd_value),
	BT_GATT_CHARACTERISTIC(&vnd_auth_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE),
	BT_GATT_DESCRIPTOR(&vnd_auth_uuid.uuid,
			   BT_GATT_PERM_READ | BT_GATT_PERM_READ_AUTHEN |
			   BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_AUTHEN,
			   read_vnd, write_vnd, vnd_value),
	BT_GATT_CHARACTERISTIC(&vnd_long_uuid.uuid, BT_GATT_CHRC_READ |
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_EXT_PROP),
	BT_GATT_LONG_DESCRIPTOR(&vnd_long_uuid.uuid,
			   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			   read_long_vnd, write_long_vnd, flush_long_vnd,
			   &vnd_long_value),
	BT_GATT_CEP(&vnd_long_cep),
	BT_GATT_CHARACTERISTIC(&vnd_signed_uuid.uuid, BT_GATT_CHRC_READ |
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_AUTH),
	BT_GATT_DESCRIPTOR(&vnd_signed_uuid.uuid,
			   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			   read_signed, write_signed, &signed_value),
};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      0x0d, 0x18, 0x0f, 0x18, 0x05, 0x18),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
		      0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_gatt_register(gap_attrs, ARRAY_SIZE(gap_attrs));
	bt_gatt_register(hrs_attrs, ARRAY_SIZE(hrs_attrs));
	bt_gatt_register(bas_attrs, ARRAY_SIZE(bas_attrs));
	bt_gatt_register(cts_attrs, ARRAY_SIZE(cts_attrs));
	bt_gatt_register(dis_attrs, ARRAY_SIZE(dis_attrs));
	bt_gatt_register(vnd_attrs, ARRAY_SIZE(vnd_attrs));

	err = bt_le_adv_start(BT_LE_ADV(BT_LE_ADV_IND), ad, ARRAY_SIZE(ad),
					sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
};

#ifdef CONFIG_MICROKERNEL
void mainloop(void)
#else
void main(void)
#endif
{
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	/* Simulate current time for Current Time Service */
	generate_current_time(ct);

	bt_conn_cb_register(&conn_callbacks);
	bt_conn_auth_cb_register(&auth_cb_display);

	/* Implement notification. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
	while (1) {
		static uint8_t hrm[2];

		task_sleep(sys_clock_ticks_per_sec);

		/* Current Time Service updates only when time is changed */
		if (ct_update) {
			ct_update = 0;
			bt_gatt_notify(NULL, &cts_attrs[3], &ct, sizeof(ct));
		}

		/* Heartrate measurements simulation */
		if (simulate_hrm) {
			heartrate++;
			if (heartrate == 160) {
				heartrate = 90;
			}

			hrm[0] = 0x06; /* uint8, sensor contact */
			hrm[1] = heartrate;

			bt_gatt_notify(NULL, &hrs_attrs[2], &hrm, sizeof(hrm));
		}

		/* Battery level simulation */
		if (simulate_blvl) {
			battery -= 1;

			if (!battery) {
				/* Software eco battery charger */
				battery = 100;
			}

			bt_gatt_notify(NULL, &bas_attrs[2], &battery,
				       sizeof(battery));
		}
	}
}
