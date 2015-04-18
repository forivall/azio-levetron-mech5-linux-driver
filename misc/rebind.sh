#!/bin/bash

# run me as root

# first unbind all
for dev in $(find /sys/bus/hid/drivers/hid-generic/ | grep '04d9:2819'); do
  echo -n "$dev" > /sys/bus/hid/drivers/generic-usb/unbind
done

# now bind to correct module
for dev in $(find /sys/bus/hid/devices | grep '04d9:2819'); do
  echo -n "$dev" > /sys/bus/hid/drivers/hid-azio-lv-mech5/bind
done
