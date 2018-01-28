# SUDA-V3S User Guide

### 简介

1. 主控芯片
   * 全志V3S处理器，单核Cortex-A7，工作主频24M~1.2GHz，内置64MB DDR2
   * 支持从SD卡启动，支持从SPI Flash启动(8~32MB Nor Flash或者128MB Nand Flash)
   * 支持RGB LCD，分辨率可以为272x480，480x800，1024x600
   * 40nm工艺
2. 功耗
   * 1GHz，Linux空闲状态，90~100mA
   * 1GHz，Linux全速运行，180mA

### sunxi

> * 全志(Allwinner)公司的很多SoC产品代号都是sunxi，比如A10(sun4i)，A13(sun5i)，A20(sun7i)，[详细信息可以查看全志SoC家族产品](https://sunxi.org/Allwinner_SoC_Family)，我们将要使用的V3s属于sun8i.
> * sunxi也是一个专门负责推广和支持全志SoC平台开源硬件的社区，但是全志本身并没有加入这个社区.

### 系统引导流程

![引导流程](imgs/boot-sequence.png)

* 其中的BSP(boot select pin)引脚为PC0，短接PC0和GND后上电即可进入USB引导模式(可以通过sunxi-tool工具来烧写程序到SPI Flash中)

### 安装交叉编译器arm-linux-gnueabihf

[下载地址](https://releases.linaro.org/components/toolchain/binaries/latest/arm-linux-gnueabihf/)

```shell
wget https://releases.linaro.org/components/toolchain/binaries/6.4-2017.11/arm-linux-gnueabihf/gcc-linaro-6.4.1-2017.11-x86_64_arm-linux-gnueabihf.tar.xz
tar xvf gcc-linaro-6.4.1-2017.11-x86_64_arm-linux-gnueabihf.tar.xz
mv gcc-linaro-6.4.1-2017.11-x86_64_arm-linux-gnueabihf /opt/
vim /etc/bash.bashrc
# add: PATH="$PATH:/opt/gcc-linaro-6.4.1-2017.11-x86_64_arm-linux-gnueabihf/bin"
arm-linux-gnueabihf-gcc -v
sudo apt-get install device-tree-compiler
```

### 下载&编译u-boot

```shell
git clone https://github.com/Lichee-Pi/u-boot.git -b v3s-current
#or git clone https://github.com/Lichee-Pi/u-boot.git -b v3s-spi-experimental
cd u-boot
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- LicheePi_Zero_800x480LCD_defconfig
#or make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- LicheePi_Zero_480x272LCD_defconfig
#or make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- LicheePi_Zero_defconfig
make ARCH=arm menuconfig
time make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- 2>&1 | tee build.log
```

* 最终在当前目录中会增加两个文件：u-boot-sunxi-with-spl.bin和build.log

#### u-boot中的目录结果

```bash
├── api                存放uboot提供的API接口函数
├── arch               平台相关的部分我们只需要关心这个目录下的ARM文件夹
│   ├──arm
│   │   └──cpu
│   │   │   └──armv7
│   │   └──dts	
│   │   │   └──*.dts   存放设备的dts,也就是设备配置相关的引脚信息
├── board              对于不同的平台的开发板对应的代码
├── cmd                顾名思义，大部分的命令的实现都在这个文件夹下面。
├── common             公共的代码
├── configs            各个板子的对应的配置文件都在里面，我们的Lichee配置也在里面
├── disk               对磁盘的一些操作都在这个文件夹里面，例如分区等。
├── doc                参考文档，这里面有很多跟平台等相关的使用文档。
├── drivers            各式各样的驱动文件都在这里面
├── dts                一种树形结构（device tree）这个应该是uboot新的语法
├── examples           官方给出的一些样例程序
├── fs                 文件系统，uboot会用到的一些文件系统
├── include            头文件，所有的头文件都在这个文件夹下面
├── lib                一些常用的库文件在这个文件夹下面  
├── Licenses           这个其实跟编译无关了，就是一些license的声明
├── net                网络相关的，需要用的小型网络协议栈
├── post               上电自检程序
├── scripts            编译脚本和Makefile文件
├── spl                second program loader，即相当于二级uboot启动。
├── test               小型的单元测试程序。
└── tools              里面有很多uboot常用的工具
```

###编译boot.scr

#### 编写boot.cmd文件(启动参数文件)

```shell
setenv bootargs console=ttyS0,115200 panic=5 console=tty0 rootwait root=/dev/mmcblk0p2 earlyprintk rw
load mmc 0:1 0x41000000 zImage
load mmc 0:1 0x41800000 sun8i-v3s-licheepi-zero-dock.dtb
bootz 0x41000000 - 0x41800000
```

####编译成二进制文件

```ba
mkimage -C none -A arm -T script -d boot.cmd boot.scr
```

### 下载&编译Linux内核

```shell
git clone https://github.com/Lichee-Pi/linux.git
cd linux
make ARCH=arm licheepi_zero_defconfig
make ARCH=arm menuconfig   #add bluethooth, etc.
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j16
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j16 INSTALL_MOD_PATH=out modules
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j16 INSTALL_MOD_PATH=out modules_install
```

最终会在arch/arm/boot下生成zImage，在arch/arm/boot/dts/下生成设备树二进制文件，在out文件夹下生成驱动.ko文件



### 系统烧写

 #### SD卡分区

> SD卡中的系统镜像一般分为三个区，第一个区称为boot区或者引导区，该部分没有文件系统而是直接将二进制的bootloader(uboot)文件直接写入。第二个区可以被称为linux内核区，fat文件系统，存放linux内核、内核参数文件还有设备数dtb文件。第三个区是root分区，用来存放根文件系统和用户数据等，一般是ext4文件分区格式。
>
> SD卡区域划分：
>
> | 0~2MB  | +20M   | Rest   |
> | ------ | ------ | ------ |
> | u-boot | kernel | rootfs |

1. 准备一张空白的SD卡，打开GParted系统工具

![一张干净的SD卡](imgs/PartSD-0.png)

2. 新建一个20MB大小的分区，并选择文件系统的格式fat16

![新建存放kernel的分区](imgs/PartSD-1.png)

3. 讲SD卡剩余空间新建为一个新的分区，并选择文件系统的格式为ext4

![新建存放rootfs的分区](imgs/PartSD-2.png)

4. 最终SD的分区结果如下图所示，单击:heavy_check_mark:开始执行分区操作

![待分区的SD卡](imgs/PartSD-3.png)

![分区操作顺利完成](imgs/PartSD-4.png)

![分区后的SD卡](imgs/PartSD-5.png)

5. 打开终端，输入命令`sudo fdisk -l /dev/sdc`查看分区信息，其中sdc1代表sdc设备的第一个分区，sdc2代表第二个分区

![查看分区后的SD卡信息](imgs/PartSD-6.png)

#### 烧写u-boot

> u-boot需要烧写到SD卡的8K偏移处
>
> `sudo dd if=u-boot-sunxi-with-spl.bin of=/dev/sdc bs=1024 seek=8`

![烧写uboot至SD卡](imgs/Burn-uboot.png)

Linux主机安装minicom，使用命令：`sudo apt-get install minicom`

串口终端tty属于**dialout**组别，需要将当前用户添加到该组中，这样才有权限访问tty*设备，具体使用如下命令:`sudo usermod -a -G dialout your-user-name`

#### 烧写内核启动参数,zImage和设备树

1. 编写boot.cmd文件，并且编译成boot.scr二进制
2. 将boot.scr，zImage，dtb设备文件拷贝到SD卡的第一个分区

![uboot启动过程](imgs/uboot-starting.png)



### 根文件系统之buildroot

> buildroot可用于构建小型的linux根文件系统，大小最小可低至2M
>
> buildroot中可以方便地加入第三方软件包，省去了手工交叉编译的烦恼
>
> 美中不足的是不支持包管理系统

```bash
git clone https://git.busybox.net/buildroot
cd buildroot
make menuconfig
make
```

编译结束后，会在output/images下生成归档文件``rootfs.tar``，使用超级权限将其解压至SD卡的第二分区中

`sudo tar -vxf rootfs.tar -C /media/morris/rootfs/`

* 如果需要在buildroot的基础上做进一步的修改，直接进入``output/target/``目录下，在这里修改一些文件，然后重新在根目录下执行``make``，会将跟新的文件内容重新打包到新的rootfs.tar中

  * 注意output/target目录不是最终的根文件系统，是临时的，不能直接拿来烧录

* 举个例子：修改环境变量**PS1**(默认提示符)

  ```bash
  cd output/target
  vim etc/profile
  #增加export PS1='\[\e[32m\][\[\e[35m\]\u\[\e[m\]@\[\e[36m\]\h \[\e[31m\]\w\[\e[32m\]]\[\e[36m\]$\[\e[m\]'
  cd ../../
  make
  ```

* buildroot的一些重要配置

  1. 配置工具链，使用本地已经安装好的交叉编译工具链，在本机上外部工具链配置为：/home/morris/SUDA_V3S/toolchains/gcc-linaro-6.4.1-2017.11-x86_64_arm-linux-gnueabihf
  2. 工具链前缀是：arm-linux-gnueabihf
  3. 外部工具链gcc版本：这里使用的是6.4版本
  4. 外部工具链内核头文件：是在arm-linux-gnueabi/libc/usr/include/linux/version.h里读取内核版本信息。这里的版本是4.6
  5. C库还是选择传统的glibc。需要小体积可以选uclibc（需要自行编译安装）
  6. 在system 设置下主机名，root密码等

![buildroot工具链配置](imgs/br-toolchain.png)

![buildroot目标架构配置](imgs/br-target.png)

![buildroot系统配置](imgs/br-system.png)

#### 添加openssh服务

![buildroot系统配置](imgs/br-openssh.png)

* 默认情况下，openssh服务器是不允许客户以root身份登录的，需要修改output/target/etc/ssh/sshd_config文件，打开`PermitRootLogin yes`选项

#### 【解决】buildroot修改配置后编译得到的文件大小没变化

* 删除生成的二进制文件，再删除 .stamp_built文件
* 重新make



### 安装内核模块

```bash
mount ${cardroot} /mnt
mkdir -p /mnt/lib/modules
rm -rf /mnt/lib/modules/
cp -r <PATH_TO_KERNEL_TREE>/output/lib /mnt/
umount /mnt
```



### Debian系统之debootstrap

> Debootstrap可以用来在系统中安装Debian而不使用安装盘，也可以用来在chroot环境下运行不同风格的Debian。这样可以创建一个完整的（最小的）Debian系统，可以用于测试目的。制作流程分为四个步骤：
>
> 1. 从源里下载需要的.deb软件包
> 2. 解压它们到相应的目标目录
> 3. 用chroot命令将目标目录临时修改为根目录
> 4. 运行每个软件包的安装和配置脚本，完成安装
>
> 完成第1和2步可以使用debootstrap，cdebootstrap或者multistrap，第3和4步可以在QEMU环境或者实际开发板上执行

1. 安装必要的工具软件`sudo apt-get install qemu qemu-user-static binfmt-support dpkg-cross debootstrap debian-archive-keyring`其中debootstrap是根文件系统制作工具，qemu是模拟器，可以在宿主机上模拟开发版的环境
2. 使用debootstrap安装指定版本的文件系统，其中distro表示版本的名字，比如ubuntu的**xenial**，debian的**stretch**，armhf是目标架构，最后加上Debian镜像站点，这里选择清华大学的开源镜像站，后面的.deb包将从这里下载

```bash
sudo debootstrap --foreign --arch armhf $distro rootfs http://ftp2.cn.debian.org/debian/
```

3. 拷贝**qemu-arm-static** 到刚构建的根文件系统中。为了能chroot到目标文件系统，针对目标CPU的qemu模拟器需要从内部访问

```bash
sudo cp /usr/bin/qemu-arm-static rootfs/usr/bin/
sudo LC_ALL=C LANGUAGE=C LANG=C chroot rootfs	#改变程序执行时所参考的根目录位置
```

4. 执行debootstrap的第二个步骤来解压安装的软件包

```bash
/debootstrap/debootstrap --second-stage
dpkg --configure -a
```

5. 配置软件源

```bash
# for Debian
cat <<EOT > etc/apt/sources.list
deb http://http.debian.net/debian $distro main contrib non-free
deb-src http://http.debian.net/debian $distro main contrib non-free
deb http://http.debian.net/debian $distro-updates main contrib non-free
deb-src http://http.debian.net/debian $distro-updates main contrib non-free
deb http://security.debian.org/debian-security $distro/updates main contrib non-free
deb-src http://security.debian.org/debian-security $distro/updates main contrib non-free
EOT
```

```bash
# for Ubuntu
cat <<EOT > etc/apt/sources.list
deb http://ports.ubuntu.com/ $distro main universe
deb-src http://ports.ubuntu.com/ $distro main universe
deb http://ports.ubuntu.com/ $distro-security main universe
deb-src http://ports.ubuntu.com/ $distro-security main universe
deb http://ports.ubuntu.com/ $distro-updates main universe
deb-src http://ports.ubuntu.com/ $distro-updates main universe
EOT#
```

6. 设置apt

```bash
cat <<EOT > /etc/apt/apt.conf.d/71-no-recommends
APT::Install-Recommends "0";
APT::Install-Suggests "0";
EOT
```

7. 从服务器更新最新的数据库`apt-get update`
8. 更新系统`apt-get upgrade`
9. 按需安装软件包

```bash
apt-get install openssh-server
apt-get install dialog locales
dpkg-reconfigure locales
dpkg-reconfigure tzdata
```

10. 设置root密码`passwd`
11. 设置主机名`echo suda-v3s > /etc/hostname`
12. 清理无用的软件包，退出chroot会话

```bash
apt-get autoclean
apt-get autoremove
apt-get clean
exit
```

13. 清理辅助文件

```bash
sudo rm rootfs/usr/bin/qemu-arm-static 
```

14. 安装驱动模块

```bash
mount ${card}${p}2 /mnt
mkdir -p /mnt/lib/modules
rm -rf /mnt/lib/modules/
cp -r ${lib_dir}/lib /mnt/
umount /mnt
```



### Debian系统之multistrap

> Multistrap是一种与Debootstrap基本相同的工具，但具有不同的设计，拥有更多的灵活性。它通过简单地使用apt和dpkg，专注于为设备生成rootfs映像，而不是在现有的机器上chroots。Multistrap是一个自动创建完整的，可引导的根文件系统的工具。它可以合并来自不同存储库的软件包来创建rootfs。额外的软件包通过简单的列出来添加到rootfs中，所有的依赖关系都被处理了。

1. 安装依赖包

```bash
sudo apt-get install multistrap qemu qemu-user-static binfmt-support dpkg-cross
```

2. 编辑arm32.conf配置文件来指明需要安装的软件包和软件源，这里使用了中科大的镜像站

```makefile
[General]
cleanup=true
noauth=true
unpack=true
debootstrap=Debian Net Utils Python Init
aptsources=Debian

[Debian]
packages=apt kmod lsof apt-utils
source=https://mirrors.ustc.edu.cn/debian/
keyring=debian-archive-keyring
suite=stretch
components=main contrib non-free

[Net]
#Basic packages to enable the networking
packages=netbase net-tools ethtool udev iproute iputils-ping ifupdown isc-dhcp-client ssh ca-certificates apt-transport-https
source=https://mirrors.ustc.edu.cn/debian/

[Utils]
#General purpose utilities
packages=locales adduser vim less wget dialog usbutils rsync git sqlite3 libsqlite3-dev dphys-swapfile 
source=https://mirrors.ustc.edu.cn/debian/

[Python]
#Python Language
packages=python python-pip
source=https://mirrors.ustc.edu.cn/debian/

[Init]
#Init system
packages=init systemd
source=https://mirrors.ustc.edu.cn/debian/
```

3. 要创建根文件系统中/dev/中的设备，我们将使用multistrap提供的device-table.pl脚本。在此之前，需要创建dev_table文件，在其中指定设备列表

```bash
#<name>         <type>  <mode>  <uid>   <gid>   <major> <minor> <start> <inc> <count>
/dev    		d       755     0       0       -       -       -       -       -
/dev/mem        c       640     0       0       1       1       0       0      -
/dev/kmem       c       640     0       0       1       2       0       0      -
/dev/null       c       640     0       0       1       3       0       0      -
/dev/zero       c       640     0       0       1       5       0       0      -
/dev/random     c       640     0       0       1       8       0       0      -
/dev/urandom    c       640     0       0       1       9       0       0      -
/dev/tty        c       666     0       0       5       0       0       0      -
/dev/tty        c       666     0       0       4       0       0       1      6
/dev/console    c       640     0       0       5       1       0       0      -
/dev/ram        b       640     0       0       1       1       0       0      -
/dev/ram        b       640     0       0       1       0       0       1      4
/dev/loop       b       640     0       0       7       0       0       1      2
/dev/ptmx       c       666     0       0       5       2       0       0      -
```

4. 在根文件系统的dev中创建必要的设备

```bash
sudo /usr/share/multistrap/device-table.pl --no-fakeroot -d rootfs -f dev_table
```

5. 创建根文件系统

```bash
sudo multistrap -d rootfs -a armhf -f arm32.conf
```

* 执行完成后，rootfs即是所需的根文件系统

6. 使用qemu来配置软件包，将rootfs作为root挂载来操作

```bash
sudo cp /usr/bin/qemu-arm-static rootfs/usr/bin
sudo mount -o bind /dev/ rootfs/dev/
export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true
export LC_ALL=C LANGUAGE=C LANG=C
sudo chroot rootfs /var/lib/dpkg/info/dash.preinst install#安装dash，比bash速度快，功能没有bash多，dash主要是为了执行脚本而存在
sudo chroot rootfs dpkg --configure -a#配置、安装下载好的软件包
sudo umount rootfs/dev/	#最后记得卸载
```

7. 最后做一些个性化的设置，如主机名

```bash
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
echo /var/swap swap swap defaults 0 0 >> $filename

#添加软件源
#使用 HTTPS 可以有效避免国内运营商的缓存劫持
#使用https://mirrors.ustc.edu.cn/repogen/工具自动生成
filename=$TARGET_ROOTFS_DIR/etc/apt/sources.list
echo deb https://mirrors.ustc.edu.cn/debian/ stretch main contrib non-free > $filename
echo deb-src https://mirrors.ustc.edu.cn/debian/ stretch main contrib non-free >> $filename
echo deb https://mirrors.ustc.edu.cn/debian/ stretch-updates main contrib non-free >> $filename
echo deb-src https://mirrors.ustc.edu.cn/debian/ stretch-updates main contrib non-free >> $filename
echo deb https://mirrors.ustc.edu.cn/debian/ stretch-backports main contrib non-free >> $filename
echo deb-src https://mirrors.ustc.edu.cn/debian/ stretch-backports main contrib non-free >> $filename
echo deb https://mirrors.ustc.edu.cn/debian-security/ stretch/updates main contrib non-free >> $filename
echo deb-src https://mirrors.ustc.edu.cn/debian-security/ stretch/updates main contrib non-free >> $filename
```

8. 设置密码及安装额外需要的软件包（如桌面系统）

```bash
sudo chroot rootfs passwd
sudo LC_ALL=C LANGUAGE=C LANG=C chroot rootfs apt-get install xorg plasma-desktop konsole qupzilla dolphin plasma-nm sudo
```

9. 修改 rootfs/etc/ssh/sshd_config来使能root登录`PermitRootLogin yes`

```bash
sudo sed -i "s/#PermitRootLogin prohibit-password/PermitRootLogin yes/" rootfs/etc/ssh/sshd_config
```

10. 修改交换分区的大小，在`/etc/dphys-swapfile `文件中修改`CONF_SWAPSIZE=128`
11. 清理辅助文件，并将文件系统同步至SD卡的第二个分区

```bash
sudo rm rootfs/usr/bin/qemu-arm-static 
sudo rsync -axHAX --progress rootfs/ /media/morris/rootfs/
```



### Debian下如何更新动态链接库

1. 方法1（推荐）

   * ```bash
     #修改/etc/ld.so.conf，然后刷新
      echo "where is your library" >> /etc/ld.so.conf
      sudo ldconfig
     ```

2. 方法2

   * ```bash
     #修改LD_LIBRARY_PATH，然后刷新
      export LD_LIBRARY_PATH=/where/you/install/lib:$LD_LIBRARY_PATH
      sudo ldconfig
     ```




### 常用的shell脚本

#### 清除SD卡中的所有分区

```bash
#!/bin/bash
sudo fdisk $1 <<EOF
d
1
d
2
d
3
d
4
w
p
q
EOF
sync
```

#### 创建分区

```bash
#!/bin/bash
sudo fdisk $1 <<EOF
n
p
1

+8M

n
p
2


p
w
q
EOF
```

#### 格式化分区

```bash
#!/bin/bash
sudo mkfs.vfat "$1"1 &&\
sudo mkfs.ext4 "$1"2 
```

#### 烧写uboot

```bash
#!/bin/bash
sudo dd if=/dev/zero of=$1 bs=1024 seek=8 count=512 &&\
sudo dd if=u-boot-sunxi-with-spl.bin of=$1 bs=1024 seek=8 &&\
sync
```

#### 烧写zImage

```bash
#!/bin/bash
sudo mount "$1"1 mnt &&\
sudo cp zImage mnt/ &&\
sudo cp sun8i-v3s-licheepi-zero*.dtb mnt/ &&\
sudo cp boot.scr mnt/ &&\
sync &&\
sudo umount "$1"1 &&\
echo "###write partion 1 ok!"
sudo umount mnt >/dev/null 2>&1
echo ""
```

#### 烧写rootfs

```bash
#!/bin/bash
sudo mount "$1"2 mnt &&\
#sudo cp -R game/* mnt/usr/games/
#sudo chmod 777 -R mnt/usr/games
sudo rm -rf mnt/* &&\
sudo tar xzvf rootfs-$2\.tar.gz -C mnt/ &&\
sudo umount "$1"2 &&\
./write_overlay.sh $1 $2 &&\
./write_swap.sh $1 &&\
sync &&\
echo "###write partion 2 ok!"
sudo umount mnt >/dev/null 2>&1
```

#### 创建交换分区

```bash
#!/bin/sh
sudo mount "$1"2 mnt &&\
sudo dd if=/dev/zero of=mnt/swap bs=1M count=128 &&\
sudo mkswap mnt/swap &&\
echo "/swap swap swap defaults 0 0" >> mnt/etc/fstab &&\
sudo umount "$1"2 &&\
sync &&\
echo "###write swap ok!"
sudo umount mnt >/dev/null 2>&1
```



### TFT液晶

> 液晶模组：KD043FM3
>
> * 制造商：深圳市柯达科电子科技有限公司
>
>
> * 驱动芯片：ILI6408，工作电压：3.3V
> * 分辨率：480x272像素
> * RGB接口：8/16/18/24Bit RGB
> * 背光驱动：5*2LED，40mA，16.0V
> * 时序参数定义：
>   * Pixel Clock (KHz) [pclk_khz]：5~12MHz
>   * Horizontal resolution (pixels) [x]：480
>   * Vertical Resolution (pixels) [y]：272
>   * Color depth / format [depth]：24
>   * Horizontal Sync Length [hs]：1
>   * Vertical Sync Length [vs]：1
>   * Left Margin (Horizontal back porch)[le]：40
>   * Right Margin (Horizontal front porch)[ri]：5
>   * Top Margin (Vertical back porch) [up]：8
>   * Bottom Margin (Vertical front porch)[lo]：8

#### u-boot中CONFIG_VIDEO_LCD_MODE的设置

`CONFIG_VIDEO_LCD_MODE="x:480,y:272,depth:24,pclk_khz:100000,le:40,ri:5,up:8,lo:8,hs:1,vs:1,sync:3,vmode:0"`

* Linux内核会将这段字符串解析为drm_display_mode类型的结构体

```c
static const struct drm_display_mode unknown_display = {
    .clock = 100000,				// pclk_khz
    .hdisplay = 480,             	// x
    .hsync_start = 480 + 5,      	// x + ri
    .hsync_end = 480 + 5 + 1,     	// x + ri + hs
    .htotal = 480 + 5 + 1 + 40,     // x + ri + hs + le
    .vdisplay = 272,                // y (FEX: lcd_y)
    .vsync_start = 272 + 8,         // y + lo
    .vsync_end = 272 + 8 + 1,       // y + lo + vs
    .vtotal = 272 + 8 + 1 + 8,    	// y + lo + vs + up
    .vrefresh = 60,
};
```

* pclk_khz除了要满足液晶屏数据手册的要求，同时，一般还要满足以下公式：

$pclk\_khz \ge (x+le+ri)*(y+up+lo)*60$，其中60表示帧率

#### 颜色深度是18 VS. 24

使用以下的测试代码来确定是否是18位还是24位的颜色深度。如果能够看到流畅的渐变色图片，则说明是24位的颜色深度，否则就是18位。

```c
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int main()
{
    int fd, x, y;
    uint32_t *fb;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    if ((fd = open("/dev/fb0", O_RDWR)) == -1) {
        printf("Can't open /dev/fb0\n");
        return 1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo)) {
        printf("FBIOGET_FSCREENINFO failed\n");
        return 1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
        printf("FBIOGET_VSCREENINFO failed\n");
        return 1;
    }

    if (vinfo.bits_per_pixel != 32) {
        printf("Only 32bpp framebuffer is supported\n");
        return 1;
    }

    fb = mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == (void *)-1) {
        printf("mmap failed\n");
        return 1;
    }

    for (y = 0; y < vinfo.yres; y++)
        for (x = 0; x < vinfo.xres; x++)
            fb[y * vinfo.xres + x] = (255 * x / vinfo.xres) * 0x000100 +
                                     (255 * y / vinfo.yres) * 0x010001;

    return 0;
}
```

###  修改u-boot开机画面

> Uboot的开机logo默认情况（因为在include/configs/sunxi-common.h中只定义了CONFIG_VIDEO_LOGO）是企鹅logo，这个是存在于uboot代码中的一个头文件（include/video_logo.h或 bmp_logo.h），这是一个巨大的结构体，其中保存着图片每个像素点的色彩数据。

1. 安装netpbm工具包`sudo apt-get install netpbm`
2. 准备一张jpeg图片，通过命令行处理为8bit的BMP图片。注意图片的分辨率不要超过LCD的支持的最大分辨率

```bash
#!/bin/sh
jpegtopnm $1 | ppmquant 31 | ppmtobmp -bpp 8 > $2
```

3. 将生成的bmp文件放入tools/logos文件夹下
4. 修改tools文件夹下的Makefile，加入刚生成的bmp文件，编译的时候，bmp文件会被tools/bmp_logo.c编译出的工具bmp_logo制作成include/bmp_logo.h，并编译进uboot中

```makefile
# Generic logo
ifeq ($(LOGO_BMP),)
LOGO_BMP= $(srctree)/$(src)/logos/你的logo.bmp

# Use board logo and fallback to vendor
ifneq ($(wildcard $(srctree)/$(src)/logos/$(BOARD).bmp),)
LOGO_BMP= $(srctree)/$(src)/logos/$(BOARD).bmp
else
ifneq ($(wildcard $(srctree)/$(src)/logos/$(VENDOR).bmp),)
LOGO_BMP= $(srctree)/$(src)/logos/$(VENDOR).bmp
endif
endif

endif # !LOGO_BMP
```

5. 修改配置文件`vim include/configs/sunxi-common.h`

```c
#define CONFIG_VIDEO_LOGO
#define CONFIG_VIDEO_BMP_LOGO
#define CONFIG_HIDE_LOGO_VERSION
```

6. 重新编译uboot即可

### 修改Linux开机画面

> drivers/video/logo/logo_linux_clut224.ppm是默认的启动Logo图片，把自己的Logo图片（jpeg格式）转换成ppm格式，替换这个文件，同时删除logo_linux_clut224.c和logo_linux_clut224.o文件，重新编译

```bash
#!/bin/sh
#先把jpeg图片转换成pnm格式，但内核的Logo最高只支持224色，需要把颜色转换成224色，最后把pnm转成ppm，文件名必须是logo_linux_clut224.ppm
jpegtopnm $1 | pnmquant 224 | pnmtoplainpnm > logo_linux_clut224.ppm
```

将生成的logo_linux_clut224.ppm文件**覆盖到drivers/video/logo文件夹下**（必要时候做好备份）

在menuconfig中开启**CONFIG_LOGO=y**和**CONFIG_LOGO_LINUX_CLUT224=y**

![内核开机画面配置](imgs/kernel-bootup-logo.png)

#### [解决]如何将u-boot和内核的启动信息输出到液晶屏幕上

* 将uboot的stdout和stderr环境变量设置为vga即可`setenv stdout vga` `setenv stderr vga` `saveenv`
* 将传递给内核的参数bootargs中**增加**`console=tty0`

#### Framebuffer常用技巧

1. 设置屏保超时时间
   * 在内核参数中增加`consoleblank=0`可以取消屏保，设置为其余的数字表示屏保超时时间（单位秒）
2. 隐藏光标
   * 在内核参数中增加`vt.global_cursor_default=0`可以隐藏光标
   * 在内核起来后，也可以通过设置`/sys/class/graphics/fbcon/cursor_blink`的值为1或0来开始或者关闭光标
3. 旋转
   * 在内核参数中增加`fbcon=rotate:<n>`可以“旋转”现实的屏幕,其中的n可以被设置为：
     * 0，0度方向
     * 1，顺时针90度旋转
     * 2，顺时针180度旋转
     * 3，顺时针270度旋转
   * 在内核起来后，也可以通过设置`/sys/class/graphics/fbcon/rotate`的值为0,1,2,3来完成旋转操作




### 制作刷机包.img文件

> 介绍如何将u-boot，boot.scr，zImage，dtbs，modules，rootfs整合在一起，制作成.img刷机包，方便量产烧录

####直接从SD卡中导出

```bash
sudo dd if=/dev/sdc of=suda-v3s.img
```

#### 手动制作

1. 制作一张空白的.img文件`dd if=/dev/zero of=suda-v3s.img bs=1M count=1800`

2. 挂载为回环设备`sudo losetup --show -f suda-v3s.img`，成功后会告知挂载的节点编号，这里假设为loop1回环设备

3. 分区`sudo fdisk /dev/loop1`

   ```bash
   Welcome to fdisk (util-linux 2.27.1).
   Changes will remain in memory only, until you decide to write them.
   Be careful before using the write command.

   Device does not contain a recognized partition table.
   Created a new DOS disklabel with disk identifier 0xb8a0f051.

   Command (m for help): n                                 # Type n
   Partition type:
      p   primary (0 primary, 0 extended, 4 free)
      e   extended
   Select (default p):                                     # Press Enter Key      
   Using default response p
   Partition number (1-4, default 1):                      # Press Enter Key 
   Using default value 1
   First sector (2048-3891199, default 2048): 4096         # Type 4096(2MB)
   Last sector, +sectors or +size{K,M,G,T,P} (4096-3891199, default 3891199): +20M       

   Created a new partition 1 of type 'Linux' and of size 20 MiB.

   Command (m for help): n                                 # Type n
   Partition type:                                           
      p   primary (1 primary, 0 extended, 3 free)
      e   extended
   Select (default p):                                     # Press Enter Key
   Using default response p
   Partition number (1-4, default 2):                      # Press Enter Key
   Using default value 2
   First sector (2048-3891199, default 2048): 45056        # Type 45056(22MB)
   Last sector, +sectors or +size{K,M,G,T,P} (45056-3891199, default 3891199):  # Press Enter Key

   Created a new partition 2 of type 'Linux' and of size 1.9 GiB.

   Command (m for help): w                                 # Type w
   The partition table has been altered!
   ```


   Calling ioctl() to re-read partition table.
   WARNING: Re-reading the partition table failed with error 22: Invalid argument.
   The kernel still uses the old table. The new table will be used at
   the next reboot or after you run partprobe(8) or kpartx(8)
   Syncing disks.
   ```

4. 挂载带分区表的镜像文件，使分区同步到.img文件`sudo kpartx -av /dev/loop1`

5. 格式化.img文件的分区

   ```bash
   sudo mkfs.vfat /dev/mapper/loop1p1
   sudo mkfs.ext4 /dev/mapper/loop1p2
   ```

6. 烧写u-boot

   ```bash
   sudo dd if=/dev/zero of=/dev/loop1 bs=1k count=2000 seek=8
   sudo dd if=u-boot-sunxi-with-spl.bin of=/dev/loop1 bs=1024 seek=8
   ```

7. 拷贝文件到第一个分区

   ```bash
   sudo mount /dev/mapper/loop1p1 /mnt
   sudo cp zImage /mnt
   sudo cp boot.scr /mnt
   sudo cp sun8i-v3s-suda.dtb /mnt
   sudo umount /mnt
   ```

8. 拷贝根文件系统到第二个分区

   ```bash
   sudo mount /dev/mapper/loop1p2 /mnt
   sudo rsync -axHAX --progress rootfs/ /mnt
   sudo umount /mnt
   ```

9. 替换内核模块

   ```bash
   sudo mount /dev/mapper/loop1p2 /mnt
   sudo mkdir -p /mnt/lib/modules
   sudo rm -rf /mnt/lib/modules/
   sudo cp -r ${lib_dir}/lib /mnt/
   sudo umount /mnt
   ```

10. 卸载.img文件

   ```bash
   sudo kpartx -d /dev/loop1
   sudo losetup -d /dev/loop1
   ```



### 设备树简介

> Device Tree是一种描述硬件的数据结构。通过将dts(device tree source)文件编译成dtb二进制文件，供Linux操作系统内核识别目标板卡上的硬件信息。
>
> 在系统起来之后，可以在**/sys/firmware/devicetree**中查看实际使用的设备树。

```bash
/ {  
    node1 {  
        a-string-property = "A string";  
        a-string-list-property = "first string", "second string";  
        a-byte-data-property = [0x01 0x23 0x34 0x56];  
        child-node1 {  
            first-child-property;  
            second-child-property = <1>;  
            a-string-property = "Hello, world";  
        };  
        child-node2 {  
        };  
    };  
    node2 {  
        an-empty-property;  
        a-cell-property = <1 2 3 4>; /* each number (cell) is a uint32 */  
        child-node1 {  
        };  
    };  
};
```

- deviec tree由节点和属性组成，节点下面可以包含子节点，属性通过键值对来描述。

#### 设备树与驱动的配合

1. 驱动代码

```c
static struct of_device_id beep_table[] = {  
    {.compatible = "fs4412,beep"},  
};  
static struct platform_driver beep_driver=  
{  
    .probe = beep_probe,  
    .remove = beep_remove,  
    .driver={  
        .name = "bigbang",  
        .of_match_table = beep_table,  
    },  
};
```

1. 设备树描述

```c
fs4412-beep{  
         compatible = "fs4412,beep";  
         reg = <0x114000a0 0x4 0x139D0000 0x14>;  
};  
```

- **compatible**，关键属性，驱动中使用of_match_table，即of_device_id列表，其中就使用compatible字段来匹配设备。简单地说就是，内核启动后会把设备树加载到树状结构体中，当insmod的时候，就会在树中查找匹配的设备节点来加载
- **reg**，描述寄存器基址和长度，可以有多个




### EtherCAT Master IgH移植

1. [下载IgH源码](https://github.com/synapticon/Etherlab_EtherCAT_Master/releases/)

2. 解压后，进入源码顶层目录，进行编译前的配置，需要指定内核源码的路径和目标平台架构

   ```bash
   ./configure --with-linux-dir=/home/morris/SUDA_V3S/kernel/linux-zero-4.13.y/ --prefix=/home/morris/SUDA_V3S/3rdpart/ethercat-1.5.2-sncn-5/out --enable-8139too=no --host=arm-linux-gnueabihf CC=arm-linux-gnueabihf-gcc AR=arm-linux-gnueabihf-ar LD=arm-linux-gnueabihf-ld RANLIB=arm-linux-gnueabihf-ranlib AS=arm-linux-gnueabihf-as NM=arm-linux-gnueabihf-nm
   ```

3. 编译源码`make`

4. 安装用户空间的程序到out目录下`make install`

5. 编译模块，需要指定交叉编译工具`make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules`

   1. 报错*error: ‘struct vm_fault’ has no member named ‘virtual_address’*

      * ```c
        //在eccdev_vma_fault函数中修改
        +#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0))
        +            " offset = %lu, page = %p\n", (void*)vmf->address, offset, page);
        +#else
                     " offset = %lu, page = %p\n", vmf->virtual_address, offset, page);
        +#endif
        ```

   2. 报错*error: initialization from incompatible pointer type [-Werror=incompatible-pointer-types]  .fault = eccdev_vma_fault*

      * ```c
        //将函数原型==> 
        eccdev_vma_fault(struct vm_area_struct* vma, struct vm_fault* vmf);
        //替换为==>
        eccdev_vma_fault(struct vm_fault* vmf);
        ```

      * ```c
        //在eccdev_vma_fault函数中，重新获取vma变量（原来的函数原型中，vma是传入的参数）
        struct vm_area_struct* vma = vmf->vma;
        ```

   3. 报错`error: storage size of ‘param’ isn’t known  struct sched_param param = { .sched_priority = 0 };`

      * ```c
        //在该.c文件中增加头文件
        #include <uapi/linux/sched/types.h>
        ```

6. 安装内核模块到**内核目录**的out文件夹下`make INSTALL_MOD_PATH=/home/morris/SUDA_V3S/kernel/linux-zero-4.13.y/out/ modules_install`

7. 进入开发板进行配置

   1. 配置规则文件`99-EtherCAT.rules`

      * ```bash
        ethercatUserGroup:=$(shell whoami)
        echo "KERNEL==\"EtherCAT[0-9]*\",MODE=\"0664\",GROUP=\"$(ethercatUserGroup)\"">99-EtherCAT.rules
        ```

   2. 目录下各文件目录的内容复制到板子根文件系统根目录下相应目录下

   ​

### 修改内核printk等级

1. 查看当前控制台的打印级别`cat /proc/sys/kernel/printk`

   * 比如返回的是：7 4 1 7
   * 其中第一个数字7表示内核打印函数printk的打印级别，只有级别比他高的信息才能在控制台上打印出来，既 0－6级别的信息

2. 修改打印级别

   * **echo "新的打印级别  4    1    7" >/proc/sys/kernel/printk**

3. 不够打印级别的信息会被写到日志中，可通过dmesg 命令来查看

4. printk的打印级别

   ```c
   #define KERN_EMERG  		"<0>" /* system is unusable */
   #define KERN_ALERT         	"<1>" /* action must be taken immediately */
   #define KERN_CRIT           "<2>" /* critical conditions */
   #define KERN_ERR            "<3>" /* error conditions */
   #define KERN_WARNING   		"<4>" /* warning conditions */
   #define KERN_NOTICE       	"<5>" /* normal but significant condition */
   #define KERN_INFO           "<6>" /* informational */
   #define KERN_DEBUG       	"<7>" /* debug-level messages */
   ```

5. printk函数的使用

   * printk(打印级别  “要打印的信息”)
   * 打印级别就是上面定义的几个宏



### I2C OLED的移植与使用



### SDIO WiFi的移植与使用



#### 参考文档

[荔枝派Zero指南 · 看云](https://www.kancloud.cn/lichee/lpi0)

















