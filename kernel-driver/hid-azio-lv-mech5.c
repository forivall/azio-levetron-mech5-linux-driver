/*
 *  Azio Levetron Mech5 Keyboard Input Driver
 *
 *  Driver generates additional key events for the keys A/B1-5 and A/B key and
 *  supports setting the backlight setting of the macro keys and volume knob
 *
 *  Copyright (c) 2015 Jordan Klassen <Wattos@gmail.com>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 */

#include <linux/hid.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/version.h>

#include "usbhid/usbhid.h"
#include "hid-ids.h"

#define USB_VENDOR_ID_HOLTEK_ALT_KEYBOARD_AZIO_LEVETRON_MECH5 0x2819

// Options for key-up behaviour
// Since the keyboard only reports a generic keyup event for the special keys,
// we have to decide how to report keyup events.
#define KEYUP_ON_NEXT_PRESS 0
// The next keypress will trigger that the previous key was released.
// This is how the windows driver operates.
#define KEYUP_ON_FIRST_RELEASE 1
// Multiple keys can be held down, but when the first key is released, all keys
// are reported as released
#define KEYUP_ON_LAST_RELEASE 2
// Not Implemented - Multiple keys can be held down, and they are only released
// when all keys are reported as released

#define KEYUP_DEFAULT_MODE KEYUP_ON_NEXT_PRESS

// 2 seconds timeout
#define WAIT_TIME_OUT 2000

#define LEVETRON_MECH5_KEY_MAP_SIZE 8

static const u8 levetron_mech5_key_map[LEVETRON_MECH5_KEY_MAP_SIZE] = {
  0,
  KEY_F17, /* A/B Toggle */
  KEY_F18, /* A1/B1 */
  KEY_F19, /* A2/B2 */
  KEY_F20, /* A3/B3 */
  KEY_F21, /* A4/B4 */
  KEY_F22, /* A5/B5 */
  0
};

/* Convenience macros */
#define azio_lv_mech5_get_data(hdev) \
  ((struct azio_lv_mech5_data *)(hid_get_drvdata(hdev)))

#define BIT_AT(var,pos) ((var) & (1<<(pos)))

struct azio_lv_mech5_data {
  struct hid_report *led_report; /* Controls the backlight of other buttons */

  u8 macro_button_state; /* Holds the last state of the ABswitch and AB1-AB6 buttons. Required to know which buttons were pressed and which to released */
  struct hid_device *hdev;
  struct usb_device *usbdev;
  struct input_dev *input_dev;
  struct attribute_group attr_group;

  u8 led; /* state of the backlight leds as sent to the keyboard  ==> 0 -> 4 */
  u8 keyup_mode;

  spinlock_t lock; /* lock for communication with user space and to ensure fifo*/
  struct completion ready; /* ready indicator */
};

// TODO: discover if state can be read, or just output the last written state
static ssize_t azio_lv_mech5_show_led(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t azio_lv_mech5_store_led(struct device *device, struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(led,  0660, azio_lv_mech5_show_led,  azio_lv_mech5_store_led);

static struct attribute *azio_lv_mech5_attrs[] = {
  &dev_attr_led.attr,
  NULL,
};

static inline void azio_lv_mech5_release_keys(struct azio_lv_mech5_data* mech5_data) {
  u8 i;
  for (i = 0; i < LEVETRON_MECH5_KEY_MAP_SIZE; i++) {
    if (levetron_mech5_key_map[i] != 0 && BIT_AT(mech5_data->macro_button_state, i)) {
      printk("azio-levetron-driver: releasing: %x %x 0\n", i, levetron_mech5_key_map[i]);
      input_report_key(mech5_data->input_dev, levetron_mech5_key_map[i], 0);
    }
  }
  mech5_data->macro_button_state = 0;
}

static int azio_lv_mech5_extra_key_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
  u8 report_id;
  u8 key_id;
  struct azio_lv_mech5_data* mech5_data = azio_lv_mech5_get_data(hdev);
  printk("azio-levetron-driver: key event size: %d\n", size);
  if (mech5_data != NULL && size >= 3) {
    printk("azio-levetron-driver: raw data: %x %x %x\n", data[0], data[1], data[2]);
  }
  if (mech5_data == NULL || size < 3 || (report_id = data[0]) != 0x02) {
    return 1; /* cannot handle the event */
  }

  if (data[2] == 0x13 && (key_id = data[1]) > 0 && key_id < LEVETRON_MECH5_KEY_MAP_SIZE) {
    if (mech5_data->keyup_mode == KEYUP_ON_NEXT_PRESS) { azio_lv_mech5_release_keys(mech5_data); }
    // Key down
    printk("azio-levetron-driver: reporting: %x %x 1\n", key_id, levetron_mech5_key_map[key_id]);
    input_report_key(mech5_data->input_dev, levetron_mech5_key_map[key_id], 1);
    mech5_data->macro_button_state |= (1<<(key_id));
  } else if (data[2] == 0x00 && data[1] == 0x00) {
    azio_lv_mech5_release_keys(mech5_data);
  }
  printk("azio-levetron-driver: syncing: %p\n", mech5_data->input_dev);
  input_sync(mech5_data->input_dev);
  return 1;
}

// static int azio_lv_mech5_extra_led_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
//     struct azio_lv_mech5_data* mech5_data = azio_lv_mech5_get_data(hdev);
//     mech5_data->led= data[1] << 4 | data[2];
//     complete_all(&mech5_data->ready);
//     return 1;
// }

static int azio_lv_mech5_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
  printk("azio-levetron-driver: event %x for %p\n",report->id, hdev);
  switch(report->id) {
    case 2: return azio_lv_mech5_extra_key_event(hdev, report, data, size);
    default: return 0;
  }
}

// only used so that we have a local handle to the input device
static int azio_lv_mech5_input_mapping(struct hid_device *hdev, struct hid_input *hi, struct hid_field *field, struct hid_usage *usage, unsigned long **bit, int *max) {
  struct azio_lv_mech5_data* data = azio_lv_mech5_get_data(hdev);
  if (data != NULL && data->input_dev == NULL) {
    data->input_dev= hi->input;
  }
  return 0;
}

enum req_type {
    REQTYPE_READ,
    REQTYPE_WRITE
};

// static void hidhw_request(struct hid_device *hdev, struct hid_report *report, enum req_type reqtype) {
// #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
//     hid_hw_request(hdev, report, reqtype == REQTYPE_READ ? HID_REQ_GET_REPORT : HID_REQ_SET_REPORT);
// #else
//     usbhid_submit_report(hdev, report, reqtype == REQTYPE_READ ? USB_DIR_IN : USB_DIR_OUT);
// #endif
// }

static int azio_lv_mech5_has_led_control(struct azio_lv_mech5_data *mech5_data) {
  // struct list_head *feature_report_list = &hdev->report_enum[HID_FEATURE_REPORT].report_list;
  // if (list_empty(feature_report_list)) {
  //   return 0; /* Currently, the keyboard registers as two different devices */
  // }
  return mech5_data->hdev->type == HID_TYPE_OTHER;
}

static int azio_lv_mech5_initialize(struct hid_device *hdev) {
  int ret = 0;
  struct azio_lv_mech5_data *data;
  // struct hid_report *report;

  data = azio_lv_mech5_get_data(hdev);

  if (!azio_lv_mech5_has_led_control(data)) { return 0; }
  printk("azio-levetron-driver: creating %p\n", hdev);
  // list_for_each_entry(report, feature_report_list, list) {
  //   printk("azio-levetron-driver: report id: %d\n", report->id);
  // }

  ret = sysfs_create_group(&hdev->dev.kobj, &data->attr_group);
  return ret;
}

static struct azio_lv_mech5_data* azio_lv_mech5_create(struct hid_device *hdev) {
  struct azio_lv_mech5_data* mech5_data;
  mech5_data = kzalloc(sizeof(struct azio_lv_mech5_data), GFP_KERNEL);
  if (mech5_data == NULL) {
    return NULL;
  }
  mech5_data->keyup_mode = 0;

  mech5_data->attr_group.name = "azio-levetron-mech5";
  mech5_data->attr_group.attrs = azio_lv_mech5_attrs;
  mech5_data->hdev = hdev;

  spin_lock_init(&mech5_data->lock);
  init_completion(&mech5_data->ready);
  return mech5_data;
}

static int azio_lv_mech5_probe(struct hid_device *hdev, const struct hid_device_id *id) {
  int ret;
  struct azio_lv_mech5_data *data;

  data = azio_lv_mech5_create(hdev);
  if (data == NULL) {
    dev_err(&hdev->dev, "can't allocate space for Azio Levetron Mech5 device attributes\n");
    ret = -ENOMEM;
    goto err_free;
  }

  data->usbdev = interface_to_usbdev(to_usb_interface(hdev->dev.parent));

  hid_set_drvdata(hdev, data);

  /*
   * Without this, the device would send a first report with a key down event for
   * certain buttons, but never the key up event
   */
  // hdev->quirks |= HID_QUIRK_NOGET;
  // printk("azio-levetron-driver: quirks: %x\n", hdev->quirks);

  ret = hid_parse(hdev);
  if (ret) {
    hid_err(hdev, "parse failed\n");
    goto err_free;
  }

  ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
  if (ret) {
    hid_err(hdev, "hw start failed\n");
    goto err_free;
  }

  ret = azio_lv_mech5_initialize(hdev);
  if (ret) {
    ret = -ret;
    hid_hw_stop(hdev);
    goto err_free;
  }

  return 0;

err_free:
  if (data != NULL) {
      kfree(data);
  }
  return ret;
}

static void azio_lv_mech5_remove(struct hid_device *hdev) {
  struct azio_lv_mech5_data* data = azio_lv_mech5_get_data(hdev);

  if (data != NULL && azio_lv_mech5_has_led_control(data)) {
    sysfs_remove_group(&hdev->dev.kobj, &data->attr_group);
  }
  hid_hw_stop(hdev);
  if (data != NULL) {
    kfree(data);
  }
}

static ssize_t azio_lv_mech5_show_led(struct device *device, struct device_attribute *attr, char *buf) {
  struct azio_lv_mech5_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));
  if (data != NULL) {
    // TODO: figure out if the state can be read in any way from the device
    // spin_lock(&data->lock);
    // init_completion(&data->ready);
    // hidhw_request(data->hdev, data->led_report, REQTYPE_READ);
    // wait_for_completion_timeout(&data->ready, WAIT_TIME_OUT);
    // spin_unlock(&data->lock);
    return sprintf(buf, "%d\n", data->led);
  }
  return 0;
}

static ssize_t azio_lv_mech5_store_led(struct device *device, struct device_attribute *attr, const char *buf, size_t count) {
  int retval;
  unsigned long key_mask;
  u8 led_mask;

  struct azio_lv_mech5_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));

  unsigned int pipe;
  u8 control_msg_data[2] = {0x05, 0x00};

  retval = kstrtoul(buf, 10, &key_mask);
  if (retval) { return retval; }

  led_mask = (key_mask) & 0xF;
  led_mask = led_mask > 4 ? 4 : led_mask;

  control_msg_data[1] = led_mask;

  pipe = usb_sndctrlpipe(data->usbdev, 0);

  // TODO: should this run in an atomic context? raw urb would have to be used then.
  // spin_lock(&data->lock);

  retval = usb_control_msg(data->usbdev, pipe,
    USB_REQ_SET_CONFIGURATION, // 0x09
    USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, // 0x21
    0x0205, // magic?
    0x01,
    control_msg_data, 2,
    WAIT_TIME_OUT);

  data->led = led_mask;
  // spin_unlock(&data->lock);
  return count;
}

// TODO: figure out how to change the button mapping for the additional included
// SiGma Micro devices (so that the D1-D6 buttons map to something better than F1-F6)
static const struct hid_device_id azio_lv_mech5_devices[] = {
  // 04d9:2819
  { HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK_ALT, USB_VENDOR_ID_HOLTEK_ALT_KEYBOARD_AZIO_LEVETRON_MECH5) },
  // 1c4f:0016
  // { HID_USB_DEVICE(USB_VENDOR_ID_SIGMA_MICRO, USB_DEVICE_ID_AZIO_KEYBOARD_LEVETRON_MECH5_NUMPAD_OR_ATTACHMENT) },
  { }
};

MODULE_DEVICE_TABLE(hid, azio_lv_mech5_devices);
static struct hid_driver azio_lv_mech5_driver = {
  .name = "hid-azio-lv-mech5",
  .id_table = azio_lv_mech5_devices,
  .raw_event = azio_lv_mech5_raw_event,
  .input_mapping = azio_lv_mech5_input_mapping,
  .probe = azio_lv_mech5_probe,
  .remove = azio_lv_mech5_remove,
};

// static int __init azio_lv_mech5_init(void) {
//   printk("azio-levetron-driver: register\n");
//   return hid_register_driver(&azio_lv_mech5_driver);
// }
//
// static void __exit azio_lv_mech5_exit(void) {
//   printk("azio-levetron-driver: unregister\n");
//   hid_unregister_driver(&azio_lv_mech5_driver);
// }
//
// module_init(azio_lv_mech5_init);
// module_exit(azio_lv_mech5_exit);
module_hid_driver(azio_lv_mech5_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jordan Klassen <jordan@klassen.me.uk>");
MODULE_DESCRIPTION("Azio Levetron Mech5 driver");
