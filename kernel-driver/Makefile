
KVERSION = $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)

obj-m = hid-azio-lv-mech5.o

default: build

build:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
install:
	make -C $(KDIR) M=$(PWD) modules_install
