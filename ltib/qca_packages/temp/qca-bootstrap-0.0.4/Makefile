# Add include path & extra define
EXTRA_CFLAGS += -Werror -Wall

ifneq ($(KERNELRELEASE),) # kbuild part of makefile

obj-m += qcabootstrap.o
qcabootstrap-objs := qca_bootstrap.o

else

KERNELDIR = ../linux
ARCH      = $(LINTARCH)
DESTDIR=../../../rootfs

all:
	$(MAKE) ARCH=$(ARCH) -C $(KERNELDIR) M=$(PWD)

install:
	install -d $(DESTDIR)/root
	install qcabootstrap.ko $(DESTDIR)/root/

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	@rm -f Module.symvers
endif
