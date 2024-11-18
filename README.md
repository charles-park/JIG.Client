# JIG.Client
2024 New version JIG-Client

### ODROID-C4 (2024-11-18)
* Linux OS Image https://dn.odroid.com/S905X3/ODROID-C4/Ubuntu/22.04/ubuntu-22.04-4.9-minimal-odroid-c4-hc4-20220705.img.xz

### Install package
```
root@odroid:~# uname -a
Linux odroid 4.9.312-6 #1 SMP PREEMPT Wed Jun 29 17:01:17 UTC 2022 aarch64 aarch64 aarch64 GNU/Linux

root@odroid:~# vi /etc/apt/sources.list
disable "jammy-updates" line
...
root@odroid:~# apt update && apt upgrade -y
...
root@odroid:~# apt install build-essential vim ssh git python3 python3-pip ethtool net-tools usbutils i2c-tools overlayroot nmap
...

root@odroid:~# reboot
...

root@odroid:~# uname -a
Linux odroid 4.9.337-17 #1 SMP PREEMPT Mon Sep 2 05:42:54 UTC 2024 aarch64 aarch64 aarch64 GNU/Linux

```

### Github setting
```
root@odroid:~# git config --global user.email "charles.park@hardkernel.com"
root@odroid:~# git config --global user.name "charles-park"
```

### Clone the reopsitory with submodule
```
root@server:~# git clone --recursive https://github.com/charles-park/JIG.Client

or

root@server:~# git clone https://github.com/charles-park/JIG.Client
root@server:~# cd JIG.Client
root@server:~/JIG.Client# git submodule update --init --recursive
```

### iperf3_odroid server mode install
```
root@server:~# git clone https://github.com/charles-park/iperf3_odroid

root@server:~# cd iperf3_odroid
root@server:~/iperf3_odroid# apt install iperf3
root@server:~/iperf3_odroid# make
root@server:~/iperf3_odroid# make install

root@server:~/iperf3_odroid# cd service
root@server:~/iperf3_odroid/service# ./install
```

### Auto login
```
root@server:~# systemctl edit getty@tty1.service
```
```
[Service]
ExecStart=
ExecStart=-/sbin/agetty --noissue --autologin root %I $TERM
Type=idle
```
* edit tool save
  save exit [Ctrl+ k, Ctrl + q]

### Disable Console (serial ttyS0), hdmi 1920xs1080, gpio overlay disable
```
root@server:~# vi /medoa/boot/boot.ini
...
# setenv condev "console=ttyS0,115200n8"   # on both
...

root@server:~# vi /medoa/boot/boot.ini
...
...
; display_autodetect=true
display_autodetect=false
...
; overlays="spi0 i2c0 i2c1 uart0"
overlays=""
...
...
```
### Disable screen off
```
root@server:~# vi ~/.bashrc
...
setterm -blank 0 -powerdown 0 -powersave off 2>/dev/null
echo 0 > /sys/class/graphics/fb0/blank
...
```

### server static ip settings
```
root@server:~# vi /etc/netplan/01-netcfg.yaml
```
```
network:
    version: 2
    renderer: networkd
    ethernets:
        eth0:
            dhcp4: no
            # static ip address
            addresses:
                - 192.168.20.162/24
            gateway4: 192.168.20.1
            nameservers:
              addresses: [8.8.8.8,168.126.63.1]

```
```
root@server:~# netplan apply
root@server:~# ifconfig
```

### server samba config
```
root@server:~# smbpasswd -a root
root@server:~# vi /etc/samba/smb.conf
```
```
[odroid]
   comment = odroid client root
   path = /root
   guest ok = no
   browseable = no
   writable = yes
   create mask = 0755
   directory mask = 0755
```
```
root@server:~# service smbd restart
```

### Sound setup
```
root@server:~# aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: rockchiphdmi0 [rockchip-hdmi0], device 0: fe400000.i2s-i2s-hifi i2s-hifi-0 [fe400000.i2s-i2s-hifi i2s-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 1: rockchiprk809 [rockchip-rk809], device 0: fe410000.i2s-rk817-hifi rk817-hifi-0 [fe410000.i2s-rk817-hifi rk817-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0

root@server:~# amixer -c 1
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'OFF'
Simple mixer control 'Capture MIC Path',0
  Capabilities: enum
  Items: 'MIC OFF' 'Main Mic' 'Hands Free Mic' 'BT Sco Mic'
  Item0: 'MIC OFF'

root@server:~# amixer -c 1 sset 'Playback Path' 'HP'
root@server:~# amixer -c 1
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'HP'
Simple mixer control 'Capture MIC Path',0
  Capabilities: enum
  Items: 'MIC OFF' 'Main Mic' 'Hands Free Mic' 'BT Sco Mic'
  Item0: 'MIC OFF'

// play audio file
root@server:~# aplay -Dhw:1,0 {audio file} -d {play time}
```

### Overlay root
* overlayroot enable
```
root@server:~# update-initramfs -c -k $(uname -r)
Using DTB: rockchip/rk3566-odroid-m1s.dtb
Installing rockchip into /boot/dtbs/5.10.0-odroid-arm64/rockchip/
Installing rockchip into /boot/dtbs/5.10.0-odroid-arm64/rockchip/
flash-kernel: installing version 5.10.0-odroid-arm64
Generating boot script u-boot image... done.
Taking backup of boot.scr.
Installing new boot.scr.

root@server:~# mkimage -A arm64 -O linux -T ramdisk -C none -a 0 -e 0 -n uInitrd -d /boot/initrd.img-$(uname -r) /boot/uInitrd 
Image Name:   uInitrd
Created:      Fri Oct 27 04:27:58 2023
Image Type:   AArch64 Linux RAMDisk Image (uncompressed)
Data Size:    7805996 Bytes = 7623.04 KiB = 7.44 MiB
Load Address: 00000000
Entry Point:  00000000

// Change overlayroot value "" to "tmpfs" for overlayroot enable
root@server:~# vi /etc/overlayroot.conf
...
overlayroot_cfgdisk="disabled"
overlayroot="tmpfs"
```
* overlayroot disable
```
// get write permission
root@server:~# overlayroot-chroot 
INFO: Chrooting into [/media/root-ro]
root@server:~# 

// Change overlayroot value "tmpfs" to "" for overlayroot disable
root@server:~# vi /etc/overlayroot.conf
...
overlayroot_cfgdisk="disabled"
overlayroot=""
```


