%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : Qualcomm Atheros Powerline Toolkit
Name            : plc-utils
Version         : 2.0.6
Release         : 1
License         : ISC
Vendor          : Qualcomm Atheros
Packager        : Nathaniel Houghton
Group           : Applications/Communications
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
make install OWNER="$(id -u)" GROUP="$(id -g)" DESTDIR=$RPM_BUILD_ROOT/%{pfx}

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*
