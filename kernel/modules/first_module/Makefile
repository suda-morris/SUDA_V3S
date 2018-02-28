ifeq ($(KERNELRELEASE),)

ifeq ($(ARCH),arm)
KERNELDIR ?= /home/morris/SUDA_V3S/kernel/linux-zero-4.13.y
ROOTFSDIR ?= /home/morris/SUDA_V3S/rootfs/multistrap/rootfs
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
endif

PWD ?= $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(ROOTFSDIR) modules_install
clean:
	rm -rf *.o *.ko .*.cmd *.mod.* modules.order Module.symvers .tmp_versions 

else

obj-m := vser.o

endif