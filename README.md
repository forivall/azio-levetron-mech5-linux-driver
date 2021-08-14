Azio Levetron Mech5 Keyboard Driver
===================================

Linux Driver for the azio levetron mech5 keyboard

## Features

* Macro buttons (a/b+a/b1-5)
* Toggling the lights
  * demo: `for i in {0..3}; do echo $i | sudo tee /sys/bus/hid/devices/*:04D9:2819.*/azio-levetron-mech5/led; sleep 1; done`
* [DKMS packaged in the AUR](https://aur.archlinux.org/packages/hid-azio-lv-mech5-dkms-git/)

## TODO
* Win on/off key & state (seems like it's not possible)
* D1-6 Keys (default mapped to F1-6, they can be remapped with a patched hid descriptor)
* only attach sysfs to one of the hid devices

## Protocol

* Key events

* Control Values

## API
The driver exposes the backlight leds via sysfs. Write 0-3 to

    /sys/bus/hid/devices/0003:04D9:2819.XXXX/azio-levetron-mech5/led

where XXXX varies (e.g. 0008). 0-3 corresponds in binary to which led you want to be on or off. `0b00 = 0` means both lights off, `0b11 = 3` means both lights on. 1 = just macro key led on, 2 = just volume knob light on.

## Kernel Driver Information

### Manual (testing)

    cd kernel-driver

Compile the driver

    make

Load it from where it is

    sudo insmod hid-azio-lv-mech5.ko

Replace the default generic driver

    sudo ../misc/rebind.sh

Watch the debug messages

    dmesg -w

Make changes and rebuild
    sudo rmmod hid-azio-lv-mech5
    make
    ...

Note that rebind will only need to be run once.

### Installation

Install dkms, yada yada yada. A package will be available in the archlinux AUR.

## Proof of Concept Information

Requires Python 3.4+ and PyUSB

### Usage

    cd proof-of-concept
    sudo python3 -i main.py
    # to read the key states
    >>> watch_special_keys()
    # to set the backlight
    >>> kbd = Mech5Keyboard()
    >>> kbd.detach_kernel_driver()
    >>> kbd.set_backlight(True, False)
    >>> kbd.attach_kernel_driver()

## Acknowledgements

[K900/g710](https://github.com/K900/g710)  
[Wattos/logitech-g710-linux-driver](https://github.com/Wattos/logitech-g710-linux-driver)
