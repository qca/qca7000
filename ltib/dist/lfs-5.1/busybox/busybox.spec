%define base %(echo %{_prefix} | sed -e s,/usr.*$,,)
%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : A small executable that replaces many UNIX utilities
Name            : busybox
Version         : 1.15.0
Release         : 1
License         : GPL
URL             : http://www.busybox.net/
Vendor          : Freescale
Packager        : Steve Papacharalambous/Stuart Hughes
Group           : System Environment/Shells
Source          : %{name}-%{version}.tar.gz
Patch1          : %{name}-%{version}-getty-nobaud-1.patch
Patch2		: qca-busybox-%{version}.patch
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup
%patch1 -p1
%patch2 -p2

%Build
PKG_BUSYBOX_PRECONFIG=${PKG_BUSYBOX_PRECONFIG:-busybox.config}
if [ -f "$PLATFORM_PATH/${PKG_BUSYBOX_PRECONFIG}" ]
then
    cp $PLATFORM_PATH/$PKG_BUSYBOX_PRECONFIG .config
else
    if [ -f "$CONFIG_DIR/defaults/$PKG_BUSYBOX_PRECONFIG" ]
    then
        cp "$CONFIG_DIR/defaults/$PKG_BUSYBOX_PRECONFIG"  .config
    fi
fi
if [ -n "$PKG_BUSYBOX_WANT_CF" -o -n "$SCB_WANT_CF" ]
then
    make menuconfig HOSTCC="$BUILDCC"
    cp .config $PLATFORM_PATH/$PKG_BUSYBOX_PRECONFIG
else
    yes "" | make config HOSTCC="$BUILDCC"
fi
if [ "$GNUTARCH" = m68knommu ]
then
    MAKE_EXTRA="SKIP_STRIP=y"
fi
make HOSTCC="$BUILDCC" $MAKE_EXTRA

%Install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/%{pfx}/%{base}/bin
make HOSTCC="$BUILDCC" CONFIG_PREFIX=$RPM_BUILD_ROOT/%{pfx}/%{base} install
for i in pidof ip
do
    
    if [ -f $RPM_BUILD_ROOT/%{pfx}/%{base}/bin/$i ]
    then
        rm $RPM_BUILD_ROOT/%{pfx}/%{base}/bin/$i
        ln -sf ../bin/busybox $RPM_BUILD_ROOT/%{pfx}/%{base}/sbin/$i
    fi
done
install -d $RPM_BUILD_ROOT/%{pfx}/%{base}/etc/rc.d/init.d
for i in run log
do
    install -d $RPM_BUILD_ROOT/%{pfx}/%{base}/var/$i
done
touch $RPM_BUILD_ROOT/%{pfx}/%{base}/var/run/utmp
touch $RPM_BUILD_ROOT/%{pfx}/%{base}/var/log/wtmp
cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/%{base}/etc/busybox.conf
[SUID]
su = ssx root.root
passwd = ssx root.root
EOF
chmod 644 $RPM_BUILD_ROOT/%{pfx}/%{base}/etc/busybox.conf

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%attr(4755,root,root) %{pfx}/%{base}/bin/busybox
%{pfx}/*
