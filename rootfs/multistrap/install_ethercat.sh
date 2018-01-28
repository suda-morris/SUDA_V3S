#!/bin/sh
#配置rules，创建设备号
sudo echo "KERNEL==\"EtherCAT[0-9]*\", MODE=\"0664\", GROUP=\"root\"" > rootfs/etc/udev/rules.d/99-EtherCAT.rules
#创建软链接
sudo chroot rootfs ln -s /opt/ethercat/etc/init.d/ethercat /etc/init.d/ethercat
sudo chroot rootfs ln -s /opt/ethercat/bin/ethercat /usr/bin/ethercat
#配置网卡参数
sudo mkdir -p rootfs/etc/sysconfig/
sudo cp rootfs/opt/ethercat/etc/sysconfig/ethercat rootfs/etc/sysconfig/ethercat
sudo sed -i 's/DEVICE_MODULES=\"\"/DEVICE_MODULES=\"generic\"/g' rootfs/etc/sysconfig/ethercat
sudo sed -i 's/MASTER0_DEVICE=\"\"/MASTER0_DEVICE=\"00:04:25:12:34:56\"/g' rootfs/etc/sysconfig/ethercat
