%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary		: System configuration package
Name		: sysconfig
Version		: 1.2
Release		: 5
License		: GPL
Vendor		: Freescale
Packager	: Stuart Hughes
Group		: System Environment/Base
BuildRoot	: %{_tmppath}/%{name}
Prefix		: %{pfx}

%Description
%{summary}

%Prep
#%setup

%Build

%Install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT/%{pfx}/root

if [ "$SYSCFG_START_SYSLOG" = "y" ]
then
    syslog=syslog
fi
if [ "$SYSCFG_START_UDEV" = "y" ]
then
    udev=udev
fi
if [ "$SYSCFG_START_MDEV" = "y" ]
then
    mdev=mdev
fi
if [ "$SYSCFG_START_DEVFSD" = "y" ]
then
    devfsd=devfsd
fi
if [ "$SYSCFG_START_NETWORK" = "y" ]
then
    network=network
fi
if [ "$SYSCFG_START_INETD" = "y" ]
then
    inetd=inetd
fi
if [ "$SYSCFG_START_PORTMAP" = "y" ]
then
    portmap=portmap
fi
if [ "$SYSCFG_START_DROPBEAR_SSH" = "y" ]
then
    dropbear=dropbear
fi
if [ "$SYSCFG_START_SSHD" = "y" ]
then
    sshd=sshd
fi
if [ "$SYSCFG_START_BOA" = "y" ]
then
    boa=boa
fi
if [ "$SYSCFG_SETTIME" = "y" ]
then
    settime=settime
fi
if [ "$SYSCFG_START_DHCPD" = "y" ]
then
    dhcpd=dhcp
fi
if [ "$SYSCFG_START_SAMBA" = "y" ]
then
    smb=smb
fi
if [ "$SYSCFG_START_WATCHDOG" = "y" ]
then
	watchdog=watchdog
fi
if [ "$SYSCFG_START_GTK2" = "y" ]
then
	gtk2=gtk2
fi
if [ "$SYSCFG_START_PANGO" = "y" ]
then
	pango=pango
fi
if [ "$SYSCFG_START_FSLGNOME" = "y" ]
then
        fslgnome=fslgnome
fi
if [ "$SYSCFG_START_QCA" = "y" ]
then
        qca_driver=qca_driver
	qca_bridge=qca_bridge
fi

cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/rc.conf
all_services="mount-proc-sys mdev udev hostname devfsd depmod modules filesystems syslog network inetd portmap dropbear sshd boa smb dhcpd settime fslgnome watchdog bluetooth gtk2 pango qca_driver qca_bridge"
all_services_r="qca_bridge qca_driver pango gtk2 bluetooth watchdog fslgnome settime dhcpd smb boa sshd dropbear portmap inetd network syslog filesystems modules depmod devfsd hostname udev mdev mount-proc-sys"

cfg_services="mount-proc-sys $mdev $udev hostname $devfsd depmod modules filesystems $syslog $network $inetd $portmap $dropbear $sshd $boa $smb $dhcpd $settime $fslgnome $watchdog $bluetooth $gtk2 $pango $qca_driver $qca_bridge"

cfg_services_r="$qca_bridge $qca_driver $pango $gtk2 $bluetooth $watchdog $fslgnome $settime $dhcpd $smb $boa $sshd $dropbear $portmap $inetd $network $syslog filesystems modules depmod $devfsd hostname $udev $mdev mount-proc-sys"

export HOSTNAME="${SYSCFG_HOSTNAME:-$PLATFORM}"
export NTP_SERVER="$SYSCFG_NTP_SERVER"
export MODLIST="$SYSCFG_MODLIST"
export RAMDIRS="$SYSCFG_RAM_DIRS"
export TMPFS="$SYSCFG_TMPFS"
export TMPFS_SIZE="${SYSCFG_TMPFS_SIZE:-512k}"
export READONLY_FS="$SYSCFG_READONLY_FS"
export INETD_ARGS="$SYSCFG_INETD_ARGS"
export BOA_ARGS="$SYSCFG_BOA_ARGS"
export SMBD_ARGS="${SYSCFG_SMBD_ARGS}"
export NMBD_ARGS="${SYSCFG_NMBD_ARGS}"
export DHCP_ARG="${SYSCFG_DHCP_ARG}"
export DEPLOYMENT_STYLE="${SYSCFG_DEPLOYMENT_STYLE:-NFS}"
export SYSCFG_DHCPC_CMD="${SYSCFG_DHCPC_CMD:-udhcpc -b -i }"
export DROPBEAR_ARGS="${SYSCFG_DROPBEAR_ARGS}"
EOF

cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/qca_bridge
#!/bin/sh

MODE="\${1}"

# configuration
. /etc/qca.conf

if [ \$MODE = "start" ]; then
	# load the bootstrap variables
	# QCA_SPI_UART_SELECT="uart|spi"
	# QCA_RS232_RS485_SELECT="rs485|rs232"
	# QCA_XCVR_ENABLED="no|yes"
	# QCA_DISABLE_OUTPUTS="no|yes"
	# QCA_HOST_MODE="uart|spi|disabled"
	. /proc/qca_bootstrap

	if [ \${QCA_HOST_MODE} = "uart" ]; then
		brctl addbr br0 &&
		brctl addif br0 qca0 &&
		brctl addif br0 eth0 &&
		brctl setfd br0 0 &&
		ifconfig br0 inet \${IPADDR} &&
		ifconfig br0 up &&
		ifconfig eth0 up
		if [ \$? -eq 0 ]; then
			echo Started UART Bridge.
		else
			echo Failed to start UART Bridge!
			exit 1
		fi
	elif [ \${QCA_HOST_MODE} = "spi" ]; then
		brctl addbr br0 &&
		brctl addif br0 qca0 &&
		brctl addif br0 eth0 &&
		brctl setfd br0 0 &&
		ifconfig br0 inet \${IPADDR} &&
		ifconfig br0 up &&
		ifconfig eth0 up
		if [ \$? -eq 0 ]; then
			echo Started SPI Bridge.
			/root/pl16cfg &
		else
			echo Failed to start SPI Bridge!
			exit 1
		fi
	elif [ \${QCA_HOST_MODE} = "disabled" ]; then
		ifconfig eth0 inet \${IPADDR} &&
		ifconfig eth0 up
		if [ \$? -eq 0 ]; then
			echo Set eth0 IP address.
		else
			echo Failed to set eth0 IP address.
			exit 1
		fi
	else
		echo Unsupported QCA_HOST_MODE!
		exit 1
	fi

else
	# load the bootstrap variables
	# QCA_SPI_UART_SELECT="uart|spi"
	# QCA_RS232_RS485_SELECT="rs485|rs232"
	# QCA_XCVR_ENABLED="no|yes"
	# QCA_DISABLE_OUTPUTS="no|yes"
	# QCA_HOST_MODE="uart|spi|disabled"
	. /proc/qca_bootstrap

	if [ \${QCA_HOST_MODE} = "uart" ]; then
		ifconfig br0 down &&
		brctl delbr br0
		if [ \$? -eq 0 ]; then
			echo Shut down UART Bridge.
		else
			echo Failed to shut down UART Bridge.
			exit 1
		fi
	elif [ \${QCA_HOST_MODE} = "spi" ]; then
		ifconfig br0 down &&
		brctl delbr br0
		if [ \$? -eq 0 ]; then
			echo Shut down SPI Bridge.
		else
			echo Failed to shut down SPI Bridge.
			exit 1
		fi
	elif [ \${QCA_HOST_MODE} = "disabled" ]; then
		ifconfig eth0 inet 0.0.0.0 &&
		ifconfig eth0 down
	else
		echo Unsupported QCA_HOST_MODE!
		exit 1
	fi
fi
EOF

cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/root/configure
#!/bin/sh

TMP_FILE="/tmp/qca.conf"
ETC_FILE="/etc/qca.conf"

mount | grep '^rwfs on /etc' > /dev/null 2>&1
if [ \$? -eq 0 ]; then
	umount /etc
	if [ \$? -ne 0 ]; then
		echo Failed to remount /etc writeable.
		exit 1
	fi
fi

mount -o remount rootfs
if [ \$? -ne 0 ]; then
	echo Failed to remount /etc writeable.
	exit 1
fi

cp "\${ETC_FILE}" "\${TMP_FILE}"
if [ \$? -ne 0 ]; then
	echo Failed to copy configuration file to /tmp.
	exit 1
fi

vi "\${TMP_FILE}"
cmp "\${TMP_FILE}" "\${ETC_FILE}" > /dev/null 2>&1
if [ \$? -eq 0 ]; then
	echo No changes detected.
	echo Reboot to revert to read-only mode or re-run this script to try again.
else
	echo -n Changes detected. Updating "\${ETC_FILE}"...
	cp "\${TMP_FILE}" "\${ETC_FILE}"
	if [ \$? -ne 0 ]; then
		echo
		echo Error: could not update "\${ETC_FILE}".
		exit 1
	fi
	echo " updated."
	echo -n "Syncing flash... "
	sync
	echo "done."
	echo 'Please reboot using the "reboot" command.'
fi
EOF

cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/etc/qca.conf
#!/bin/sh

# IP address
IPADDR="192.168.1.2"

# MAC addresses
ETH_MAC_ADDR="00:B0:52:FF:FF:01"
SPI_MAC_ADDR="00:B0:52:FF:FF:02"
UART_MAC_ADDR="00:B0:52:FF:FF:03"

# options given to the SPI driver
SPI_OPTIONS="qcaspi_clkspeed=8000000 qcaspi_legacy_mode=0 qcaspi_burst_len=5000"

# slattach command line
SLATTACH="slattach -s 115200 -p qca /dev/ttySP0"

EOF

cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/qca_driver
#!/bin/sh

MODE="\${1}"
MODULE_DIR="/root"

# configuration
. /etc/qca.conf

if [ \$MODE = "start" ]; then
	insmod "\${MODULE_DIR}/qcabootstrap.ko"
	if [ \$? -ne 0 ]; then
		echo Failed to start bootstrap module.
		exit 1
	fi

	# load the bootstrap variables
	# QCA_SPI_UART_SELECT="uart|spi"
	# QCA_RS232_RS485_SELECT="rs485|rs232"
	# QCA_XCVR_ENABLED="no|yes"
	# QCA_DISABLE_OUTPUTS="no|yes"
	# QCA_HOST_MODE="uart|spi|disabled"
	. /proc/qca_bootstrap

	# cat them out for the user
	echo Bootstrap options:
	cat /proc/qca_bootstrap

	# set ethernet MAC address
	ifconfig eth0 hw ether "\${ETH_MAC_ADDR}"

	if [ \${QCA_HOST_MODE} = "uart" ]; then
		insmod "\${MODULE_DIR}/qcauart.ko"
		if [ \$? -eq 0 ]; then
			echo Started UART driver.
		else
			echo Failed to start UART driver!
			exit 1
		fi
		ifconfig qca0 hw ether "\${UART_MAC_ADDR}"
		\${SLATTACH} &
#		# make sure slattach is running after 1 second
#		sleep 1
#		ps auxw | grep slattach | grep -v grep > /dev/null 2>&1
#		if [ \$? -ne 0 ]; then
#			echo slattach failed to start.
#			exit 1
#		fi
		ifconfig qca0 up
		if [ \$? -ne 0 ]; then
			echo Failed to bring up qca0 interface!
			exit 1
		fi
	elif [ \${QCA_HOST_MODE} = "spi" ]; then
		insmod "\${MODULE_DIR}/qcaspi.ko" \${SPI_OPTIONS}
		if [ \$? -eq 0 ]; then
			echo Started SPI driver.
		else
			echo Failed to start SPI driver!
			exit 1
		fi
		ifconfig qca0 hw ether "\${SPI_MAC_ADDR}"
		ifconfig qca0 up
		if [ \$? -ne 0 ]; then
			echo Failed to bring up qca0 interface!
			exit 1
		fi
	elif [ \${QCA_HOST_MODE} = "disabled" ]; then
		echo Bootstrap options disabled host.
	else
		echo Unsupported QCA_HOST_MODE!
		exit 1
	fi

else
	# load the bootstrap variables
	# QCA_SPI_UART_SELECT="uart|spi"
	# QCA_RS232_RS485_SELECT="rs485|rs232"
	# QCA_XCVR_ENABLED="no|yes"
	# QCA_DISABLE_OUTPUTS="no|yes"
	# QCA_HOST_MODE="uart|spi|disabled"
	. /proc/qca_bootstrap

	# cat them out for the user
	echo Bootstrap options:
	cat /proc/qca_bootstrap

	# don't need the bootstrap module anymore
	rmmod "\${MODULE_DIR}/qcabootstrap.ko"
	if [ \$? -ne 0 ]; then
		echo Failed to stop bootstrap module.
		exit 1
	fi

	if [ \${QCA_HOST_MODE} = "uart" ]; then
		kill \$(ps | grep -v grep | grep slattach | cut -d ' ' -f 2) &&
		rmmod "\${MODULE_DIR}/qcauart.ko"
	elif [ \${QCA_HOST_MODE} = "spi" ]; then
		rmmod "\${MODULE_DIR}/qcaspi.ko"
	elif [ \${QCA_HOST_MODE} = "disabled" ]; then
		echo Bootstrap options disabled host.
	else
		echo Unsupported QCA_HOST_MODE!
		exit 1
	fi
fi
EOF
chmod a+x $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/qca_bridge
chmod a+x $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/qca_driver
chmod a+x $RPM_BUILD_ROOT/%{pfx}/root/configure

# network interfaces
for i in 0 1 2 3 4 5
do
    if [  "$(eval echo \$$(echo SYSCFG_IFACE$i))" = "y" ]
    then
	if [ "$(eval echo \$$(echo SYSCFG_DHCPC$i))" = "y" ]
	then
	    cat <<EOF >> $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/rc.conf
# net interface $i
export $(echo SYSCFG_IFACE$i)=y
export $(echo INTERFACE$i)="$(eval echo \$$(echo SYSCFG_NET_INTERFACE$i))"
export $(echo IPADDR$i)="dhcp"
EOF
	else
	    cat <<EOF >> $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/rc.conf
# net interface $i
export $(echo SYSCFG_IFACE$i)=y
export $(echo INTERFACE$i)="$(eval echo \$$(echo SYSCFG_NET_INTERFACE$i))"
export $(echo IPADDR$i)="$(eval echo \$$(echo SYSCFG_IPADDR$i))"
export $(echo NETMASK$i)="$(eval echo \$$(echo SYSCFG_NET_MASK$i))"
export $(echo BROADCAST$i)="$(eval echo \$$(echo SYSCFG_NET_BROADCAST$i))"
export $(echo GATEWAY$i)="$(eval echo \$$(echo SYSCFG_NET_GATEWAY$i))"
export $(echo NAMESERVER$i)="$(eval echo \$$(echo SYSCFG_NAMESERVER$i))"
EOF
	fi
    fi
done

if [ "$PKG_BUSYBOX" = "y" -a "$PKG_SYSVINIT" != "y" ]
then
    # BusyBox init
    if [ "$SYSCFG_WANT_LOGIN_TTY" = "y" ]
    then
	sys_login=`echo "$SYSCFG_LOGING_TTY" | sed 's/\\\\\\\\n/\n/'`
    else
	sys_login="::respawn:-/bin/sh"
    fi
    cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/etc/inittab
# see busybox-1.00rc2/examples/inittab for more examples
::sysinit:/etc/rc.d/rcS
$sys_login
::ctrlaltdel:/sbin/reboot
::shutdown:/etc/rc.d/rcS stop
::restart:/sbin/init
EOF
else
    # SysVInit
    if [ "$SYSCFG_WANT_LOGIN_TTY" = "y" ]
    then
	run_level=3
    else
	run_level=1
    fi
    cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/etc/inittab
id:$run_level:initdefault:

si::sysinit:/etc/rc.d/rcS start

# Runlevel 0 is halt
# Runlevel 1 is single-user
# Runlevels 2-5 are multi-user
# Runlevel 6 is reboot

l0:0:wait:/etc/rc.d/rcS stop
l1:1:respawn:/bin/sh -i
l6:6:wait:/sbin/reboot

co:2345:respawn:${SYSCFG_LOGING_TTY:-$INITTAB_LINE}

ca::ctrlaltdel:/sbin/reboot
EOF
fi

cat <<'EOF' > $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/rc.local
#!/bin/sh
#
# This script will be executed *after* all the other init scripts.
# You can put your own initialization stuff in here
if [ -x "/usr/bin/rpm" -a -e "/tmp/ltib" ]
then
    echo "rebuilding rpm database"
    rm -rf /tmp/ltib
    rpm --rebuilddb
fi

# fix up permissions
#if [ -d /home/user ]
#then
#    chown -R user.user /home/user
#fi

# Add nodes when running under the hypervisor and static devices
if [ -r /sys/class/misc/fsl-hv/dev -a ! -r /dev/fsl-hv ]
then
   echo "creating hypervisor nodes"
   DEVID=`cat /sys/class/misc/fsl-hv/dev`
   if [ -n "$DEVID" ]
   then
       MAJOR="${DEVID%%:*}"
       MINOR="${DEVID##*:}"

       if [ \( "$MAJOR" -gt 0 \) -a \( "$MINOR" -gt 0 \) ]
       then
	   rm -f /dev/fsl-hv
	   mknod /dev/fsl-hv c $MAJOR $MINOR
       fi
   fi
   for i in 0 1 2 3 4 5 6 7
   do
       mknod /dev/hvc$i c 229 $i
   done
fi

# add the fm device nodes
if [ -n "$(cat /proc/devices | grep fm | sed 's/\([0-9]*\).*/\1/')" -a ! -r /dev/fm0 ]
then
    echo "creating fman device nodes"
    cd /usr/share/doc/fmd-uspace-01.01/test/
    sh fm_dev_create
    cd -
fi

for i in 0 1 2; do
    if [ -e /sys/class/graphics/fb$i ]; then
        chmod 0666 /sys/class/graphics/fb$i/pan
    fi
done

EOF
chmod +x $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/rc.local

cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/etc/issue
$( gcc --version | grep 'gcc')
root filesystem built on $(date -R)
Freescale Semiconductor, Inc.

EOF

# The kernel attempts to run /init (not /sbin/init!) from initramfs images
if [ "$SYSCFG_DEPLOYMENT_STYLE" = "INITRAMFS" ]
then
    ln -s /sbin/init $RPM_BUILD_ROOT/%{pfx}/init
fi

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*
