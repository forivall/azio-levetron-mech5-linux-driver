#!/bin/bash

# run me as root

# first unbind all
for dev in $(find /sys/bus/hid/drivers/hid-generic/ | grep '04D9:2819' | xargs -n1 basename); do
  echo "unbinding $dev"
  echo -n "$dev" > /sys/bus/hid/drivers/hid-generic/unbind
done

# now bind to correct module
for dev in $(find /sys/bus/hid/devices | grep '04D9:2819' | xargs -n1 basename); do
  echo "binding $dev"
  echo -n "$dev" > /sys/bus/hid/drivers/hid-azio-lv-mech5/bind
done
