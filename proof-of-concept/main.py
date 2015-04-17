#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import usb.core


class Mech5Keyboard:
    def __init__(self):
        self.keyboard = usb.core.find(
            idVendor=0x04d9, idProduct=0x2819) or None
        # endpoint 1: normal keys, endpoint 2: a/b keys
        self.numpad = None
        self.keypad = None

        for device in usb.core.find(
                find_all=True, idVendor=0x1c4f, idProduct=0x0016):
            # TODO: probe the device somehow (backlight?) to determine which
            # device is the num and which is the 6 key pad
            # for now, assume that the keyboard is the device with the lowest
            # address
            # numpad endpoint 1 = keys, endpoint 2 = calculator button
            if self.numpad:
                if self.numpad.address > device.address:
                    self.keypad, self.numpad = self.numpad, device
                else:
                    self.keypad = device
            else:
                self.numpad = device

        self._find_interfaces()

    def _find_interfaces(self):
        device = self.keyboard
        self.keyboard_special_key_interface = None
        self.keyboard_special_key_endpoint = None
        if device is None:
            return
        for interface in device.get_active_configuration():
            if interface.bInterfaceNumber == 1:
                self.keyboard_special_key_interface = interface
            for endpoint in interface:
                if endpoint.bEndpointAddress == 0x82:
                    self.keyboard_special_key_endpoint = endpoint

    def set_backlight(self, special_keys, volume_knob):
        val = (special_keys and 0x01) | (volume_knob and 0x02)
        # return self.keyboard.write(0x00, array.array('B', [0x21, 0x09, 0x05, 0x02, 0x01, 0x00, 0x02, 0x00, 0x05, 0x03]))

        self.keyboard.ctrl_transfer(
            bmRequestType=0x21,
            bRequest=0x09,
            wValue=0x0205,
            wIndex=1,
            data_or_wLength=[0x05, val])

    def get_keyboard_descriptor(self):
        return self.keyboard.ctrl_transfer(
            bmRequestType=0x81,
            bRequest=0x06,
            wValue=0x2200,
            wIndex=0x01,
            data_or_wLength=0x77
        )
    # array('B', [5, 12, 9, 1, 161, 1, 133, 2, 25, 0, 42, 255, 31, 21, 0, 38, 255, 31,
    #  117, 16, 149, 1, 129, 0, 192, 5, 1, 9, 128, 161, 1, 133, 3, 117, 1, 149, 3, 21,
    #  0, 37, 1, 9, 129, 9, 130, 9, 131, 129, 98, 149, 5, 129, 3, 192, 5, 8, 9, 137, 1
    # 61, 1, 133, 5, 25, 129, 41, 136, 21, 0, 37, 1, 149, 8, 117, 1, 145, 2, 192])

    def detach_kernel_driver(self):
        if self.keyboard.is_kernel_driver_active(self.keyboard_special_key_interface.bInterfaceNumber):
            return self.keyboard.detach_kernel_driver(self.keyboard_special_key_interface.bInterfaceNumber)

    def attach_kernel_driver(self):
        return self.keyboard.attach_kernel_driver(self.keyboard_special_key_interface.bInterfaceNumber)

    def __str__(self):
        return 'Keyboard: {}\nNumpad: {}\nKeypad:{}'.format(
            self.keyboard, self.numpad, self.keypad)

    def __enter__(self):
        self.detach_kernel_driver()
        return self

    def __exit__(self, *_):
        self.attach_kernel_driver()
        self.dispose_resources()

    def dispose_resources(self):
        usb.util.dispose_resources(self.keyboard)
        usb.util.dispose_resources(self.numpad)
        usb.util.dispose_resources(self.keypad)

    def __del__(self):
        self.dispose_resources()


KEYMAP = {
    # array.array('B', [1, 19]):
    0x01: 'A/B',    # 0x020113
    # array.array('B', [2, 19]):
    0x02: 'A1/B1',  # 0x020213
    # array.array('B', [3, 19]):
    0x03: 'A2/B2',  # 0x020313
    # array.array('B', [4, 19]):
    0x04: 'A3/B3',  # 0x020413
    # array.array('B', [5, 19]):
    0x05: 'A4/B4',  # 0x020513
    # array.array('B', [6, 19]):
    0x06: 'A5/B5',  # 0x020613
    # array.array('B', [0, 0]):
    # '(UP)',    # 0x020000
}


def watch_special_keys(dev=None):
    with (dev or Mech5Keyboard()) as device:
        endpoint = device.keyboard_special_key_endpoint

        while True:
            try:
                data = endpoint.read(endpoint.wMaxPacketSize, timeout=10000)
                packet_id = data[0]
                data = data[1:]
                if data[1] == 0x13 and data[0] in KEYMAP:
                    print('Pressed', KEYMAP[data[0]])
                elif data[0] == 0x00 and data[1] == 0x00:
                    print('Released')
                else:
                    print('packet id:', packet_id)
                    print('data:     ', data)
            except usb.core.USBError as ex:
                if ex.errno == 110:
                    return
                raise

if __name__ == '__main__':
    pass
