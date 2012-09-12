# Add include path & extra define
EXTRA_CFLAGS += -Werror -Wall

ifneq ($(KERNELRELEASE),) # kbuild part of makefile

obj-m += qcaspi.o
qcaspi-objs := qca_spi.o qca_framing.o qca_7k.o qca_debug.o

else

KERNELDIR = ../linux
ARCH      = $(LINTARCH)
DESTDIR=../../../rootfs

all:
	$(MAKE) ARCH=$(ARCH) -C $(KERNELDIR) M=$(PWD)

install:
	install -d $(DESTDIR)/root
	install qcaspi.ko $(DESTDIR)/root/

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	@rm -f Module.symvers
endif
