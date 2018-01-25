#!/bin/sh
#Directory contains the target rootfs
TARGET_ROOTFS_DIR="rootfs"

#Board hostname
filename=$TARGET_ROOTFS_DIR/etc/hostname
echo suda-v3s > $filename

#Default name servers
filename=$TARGET_ROOTFS_DIR/etc/resolv.conf
echo nameserver 8.8.8.8 > $filename
echo nameserver 8.8.4.4 >> $filename

#Default network interfaces
filename=$TARGET_ROOTFS_DIR/etc/network/interfaces
echo auto eth0 >> $filename
echo allow-hotplug eth0 >> $filename
echo iface eth0 inet dhcp >> $filename
#eth0 MAC address
echo hwaddress ether 00:04:25:12:34:56 >> $filename

#Set the the debug port
filename=$TARGET_ROOTFS_DIR/etc/inittab
echo T0:2345:respawn:/sbin/getty -L ttyS0 115200 vt100 >> $filename

#microSD partitions mounting
filename=$TARGET_ROOTFS_DIR/etc/fstab
echo /dev/mmcblk0p1 /boot vfat noatime 0 1 > $filename
echo /dev/mmcblk0p2 / ext4 noatime 0 1 >> $filename
echo proc /proc proc defaults 0 0 >> $filename

filename=$TARGET_ROOTFS_DIR/etc/apt/sources.list
echo deb https://mirrors.ustc.edu.cn/debian/ stretch main contrib non-free > $filename
echo deb-src https://mirrors.ustc.edu.cn/debian/ stretch main contrib non-free >> $filename
echo deb https://mirrors.ustc.edu.cn/debian/ stretch-updates main contrib non-free >> $filename
echo deb-src https://mirrors.ustc.edu.cn/debian/ stretch-updates main contrib non-free >> $filename
echo deb https://mirrors.ustc.edu.cn/debian/ stretch-backports main contrib non-free >> $filename
echo deb-src https://mirrors.ustc.edu.cn/debian/ stretch-backports main contrib non-free >> $filename
echo deb https://mirrors.ustc.edu.cn/debian-security/ stretch/updates main contrib non-free >> $filename
echo deb-src https://mirrors.ustc.edu.cn/debian-security/ stretch/updates main contrib non-free >> $filename
