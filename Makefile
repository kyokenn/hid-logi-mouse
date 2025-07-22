obj-m+=hid-logi-mouse.o
KERNELDIR?=/lib/modules/$(shell uname -r)/build
DRIVERDIR?=$(shell pwd)

VERSION=0.2.0
SRC=\
	Makefile \
	README.md \
	dkms.conf \
	hid-logimouse-kmod.spec \
	hid-logimouse-kmod-common.spec \
	hid-logi-mouse.c \
	hid-logi-mouse.h
SRCDIR=hid-logimouse_$(VERSION)
ARCHIVE=$(SRCDIR).orig.tar.xz

all:
	echo "No need to build kernel modules now."
	echo "Kernel modules are built at packages installation time using DKMS or AKMOD."

# invoked by debian/rules
install:
	mkdir -p $(DESTDIR)/usr/src/hid-logimouse-$(VERSION)
	cp -fv hid-logi-mouse.c hid-logi-mouse.h Makefile $(DESTDIR)/usr/src/hid-logimouse-$(VERSION)/

# invoked by dkms
kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(DRIVERDIR) modules

kernel_modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(DRIVERDIR) modules_install

kernel_clean:
	$(MAKE) -C $(KERNELDIR) M=$(DRIVERDIR) clean

# build source archive, needed by rpm and deb
../$(ARCHIVE): $(SRC)
	mkdir -p $(SRCDIR)
	cp -fv $^ $(SRCDIR)/
	tar Jcvf $@ $(SRCDIR)
	rm -Rf $(SRCDIR)

# copies specs to rpmbuild's specs path
~/rpmbuild/SPECS/%.spec: %.spec
	mkdir -p ~/rpmbuild/SPECS
	cp -fv $^ $@

# copies archive from parent directory to rpmbuild's sources path
~/rpmbuild/SOURCES/%.tar.xz: ../%.tar.xz
	mkdir -p ~/rpmbuild/SOURCES
	cp -fv $^ $@

rpm: \
	~/rpmbuild/SOURCES/$(ARCHIVE) \
	~/rpmbuild/SPECS/hid-logimouse-kmod.spec \
	~/rpmbuild/SPECS/hid-logimouse-kmod-common.spec
	rpmbuild -bb hid-logimouse-kmod.spec
	rpmbuild -bb hid-logimouse-kmod-common.spec

builddep: \
	dnf install kmodtool
