ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
KDIR ?= /opt/raspberrypi/linux

obj-m += beahpong.o

all: lkm dts

lkm:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$$PWD src=$$PWD/.. modules

dts:
	$(KDIR)/scripts/dtc/dtc -@ -I dts -O dtb -o $$PWD/beahpong.dtbo $$PWD/../beahpong.dts

clean: clean-lkm clean-dts

clean-lkm:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$$PWD src=$$PWD/.. clean

clean-dts:
	rm -f $$PWD/beahpong.dtbo
