#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <settings/settings.h>

#include <drivers/gpio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#define BT_UUID_CUSTOM_SERVICE_VAL BT_UUID_128_ENCODE(0x2314f4ce, 0x1847, 0x49ec, 0x879e, 0x8fbfd895c877)

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(BT_UUID_CUSTOM_SERVICE_VAL);
static struct bt_uuid_128 led_uuid     = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x0ac1eae9, 0xb54f, 0x4b01, 0xbd75, 0xe820c22e1d5d));
static struct bt_uuid_128 buttons_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x40403bde, 0x3593, 0x473a, 0xa286, 0xc6c5813a692c));

static uint8_t led_value[1] = { 0 };
static const struct gpio_dt_spec ledGpios[4] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};

static ssize_t read_led(struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
	return bt_gatt_attr_read(conn, attr, buf, len, offset, (uint8_t*)attr->user_data, 1);
}

static ssize_t write_led(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len, uint16_t offset, uint8_t flags) {
	if (len != 1) {
		printk("Error: length was not 1 when writing to LED characteristic.\n");
		return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
	}
	if (offset > 0) {
		printk("Error: non-zero offset given when writing to LED characteristic.\n");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	uint8_t value = ((uint8_t*)buf)[0];

	for (int i=0; i<4; i++) {
		const int err = gpio_pin_set_dt(&ledGpios[i], (value >> i) & 1);
		if (err != 0) {
			printk("Error while toggling GPIO pin %d: %d\n", i, err);
		}
	}

	return 1;
}


#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)

static const struct gpio_dt_spec button_gpios[4] = {
	GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0}),
	GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0}),
	GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0}),
	GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0}),
};
static struct gpio_callback buttonGpioCallbacks[4];
// static struct gpio_callback button2_cb_data;
// static struct gpio_callback button3_cb_data;
// static struct gpio_callback button4_cb_data;
static uint8_t buttons_value[1] = { 0 };

static ssize_t read_buttons(struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
	return bt_gatt_attr_read(conn, attr, buf, len, offset, (uint8_t*)attr->user_data, 1);
}

static void ct_ccc_cfg_changed(const struct bt_gatt_attr* attr, uint16_t value) {
	/* TODO: Handle value */
}

BT_GATT_SERVICE_DEFINE(gatt_service,
	BT_GATT_PRIMARY_SERVICE(&service_uuid),
	BT_GATT_CHARACTERISTIC(&led_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_led, write_led, led_value),
	BT_GATT_CHARACTERISTIC(&buttons_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ,
		read_buttons, NULL, buttons_value),
	BT_GATT_CCC(ct_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

void button_pressed(const struct device* dev, struct gpio_callback* cb, uint32_t pins) {
	int buttonIndex = 0;
	switch (pins) {
		case 8192:  buttonIndex = 1; break;
		case 16384: buttonIndex = 2; break;
		case 32768: buttonIndex = 3; break;
		case 65536: buttonIndex = 4; break;
	}
	printk("Button %d pressed, pin: %d \n", buttonIndex, button_gpios[0].pin);
	buttons_value[0] = buttonIndex;
	int err = bt_gatt_notify(NULL, &gatt_service.attrs[4], &buttons_value, 1);
	if (err != 0) {
		printk("Error while notifying GATT of button 1 press (err 0x%02x)\n", err);
	} else {
		printk("Button 1 pressed notification sent over bluetooth\n");
	}
}

static const struct bt_data bluetooth_advertising_config[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_CUSTOM_SERVICE_VAL),
};

// Handle Bluetooth connect/disconnect events
static void connected(struct bt_conn* conn, uint8_t err) {
	if (err) {
		printk("Bluetooth connection failed (err 0x%02x)\n", err);
	} else {
		printk("Bluetooth connected\n");
	}
}
static void disconnected(struct bt_conn* conn, uint8_t reason) {
	printk("Bluetooth disconnected (reason 0x%02x)\n", reason);
}
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

void main(void) {
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
	printk("Bluetooth enabled\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, bluetooth_advertising_config, ARRAY_SIZE(bluetooth_advertising_config), NULL, 0);
	if (err) {
		printk("Bluetooth advertising failed to start (err %d)\n", err);
		return;
	}
	printk("Bluetooth advertising successfully started\n");

	// Set up LEDs
	for (int i=0; i<4; i++) {
		err = gpio_pin_configure_dt(&ledGpios[i], GPIO_OUTPUT_INACTIVE);
		if (err != 0) {
			printk("Error while configuring GPIO pin: %d\n", err);
			return;
		}
	}

	// Set up buttons
	for (int i=0; i<4; i++) {
		if (!device_is_ready(button_gpios[i].port)) {
			printk("Error: button device %s is not ready\n", button_gpios[i].port->name);
			return;
		}
		err = gpio_pin_configure_dt(&button_gpios[i], GPIO_INPUT);
		if (err != 0) {
			printk("Error %d: failed to configure %s pin %d\n", err, button_gpios[i].port->name, button_gpios[i].pin);
			return;
		}
		err = gpio_pin_interrupt_configure_dt(&button_gpios[i], GPIO_INT_EDGE_TO_ACTIVE);
		if (err != 0) {
			printk("Error %d: failed to configure interrupt on %s pin %d\n", err, button_gpios[i].port->name, button_gpios[i].pin);
			return;
		}
		gpio_init_callback(&buttonGpioCallbacks[i], button_pressed, BIT(button_gpios[i].pin));
		gpio_add_callback(button_gpios[i].port, &buttonGpioCallbacks[i]);
	}

	while (1) {
		k_sleep(K_SECONDS(1));
	}
}
