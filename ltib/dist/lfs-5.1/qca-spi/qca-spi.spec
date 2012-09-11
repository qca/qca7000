%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : QCA7000 SPI Device Driver
Name            : qca-spi
Version         : 0.1.8
Release         : 1
License         : ISC
Vendor          : Qualcomm Atheros
Packager        : Nathaniel Houghton
Group           : Device Drivers
Source          : %{name}-%{version}.tar.gz
BuildRoot       : %{_tmppath}/%{name}
Patch0		: qca-spi-dppm.patch
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup 
%patch0 -p2

%Build
if [ -z "$PKG_KERNEL_KBUILD_PRECONFIG" ]
then
      KERNELDIR="$PWD/../linux"
      KBUILD_OUTPUT="$PWD/../linux"
else
      KERNELDIR="$PKG_KERNEL_PATH_PRECONFIG"
      KBUILD_OUTPUT="$(eval echo ${PKG_KERNEL_KBUILD_PRECONFIG})"
fi
make

%Install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT/%{pfx}

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*
