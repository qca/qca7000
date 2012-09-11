%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : QCA7000 SPI Device Driver
Name            : qca-pl16cfg
Version         : 0.0.1
Release         : 1
License         : ISC
Vendor          : Qualcomm Atheros
Packager        : Nathaniel Houghton
Group           : Utilities
Source          : %{name}-%{version}.tar.gz
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup 

%Build
make

%Install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT/%{pfx}

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*
