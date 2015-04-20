
KVERSION = $(shell uname -r)
KGITTAG = $(shell uname -r|sed -nE 's/([0-9]+\.[0-9]+).*/v\1/p')
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)

obj-m = hid-azio-lv-mech5.o

default: build

hid-ids.h:
	curl -OL https://github.com/torvalds/linux/raw/$(KGITTAG)/drivers/hid/hid-ids.h

usbhid/usbhid.h:
	curl -L https://github.com/torvalds/linux/raw/$(KGITTAG)/drivers/hid/usbhid/usbhid.h > usbhid/usbhid.h

headers: hid-ids.h usbhid/usbhid.h
clean-headers:
	rm hid-ids.h usbhid/usbhid.h

build: headers
	make -C $(KDIR) M=$(PWD) modules
clean: clean-headers
	make -C $(KDIR) M=$(PWD) clean
install:
	make -C $(KDIR) M=$(PWD) modules_install
