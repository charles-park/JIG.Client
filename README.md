# JIG.Client
2024 New version JIG-Client (branch odroid-c5)
* Document : https://docs.google.com/spreadsheets/d/1igBObU7CnP6FRaRt-x46l5R77-8uAKEskkhthnFwtpY/edit?gid=719914769#gid=719914769

### ODROID-C4 (2024-11-18)
* Linux OS Image https://dn.odroid.com/S905X3/ODROID-C4/Ubuntu/22.04/ubuntu-22.04-4.9-minimal-odroid-c4-hc4-20220705.img.xz
* LIRC Install for IR : https://wiki.odroid.com/odroid-c4/application_note/lirc/lirc_ubuntu18.04#tab__odroid-c2n2c4hc4

### Install package
```
// upgrade ubuntu system 22.04 -> 24.01
root@odroid:~# do-release-upgrade

root@odroid:~# uname -a
Linux odroid 4.9.312-6 #1 SMP PREEMPT Wed Jun 29 17:01:17 UTC 2022 aarch64 aarch64 aarch64 GNU/Linux

root@odroid:~# vi /etc/apt/sources.list
* disable "jammy-updates" line
...

// system update
root@odroid:~# apt update && apt upgrade -y
// ubuntu package install
root@odroid:~# apt install build-essential vim ssh git python3 python3-pip ethtool net-tools usbutils i2c-tools overlayroot nmap evtest htop cups cups-bsd iperf3 alsa samba lirc

// python3 package (ubuntu 18.04)
root@odroid:~# pip install aiohttp asyncio

// ubuntu 24.01 version python3 package install
root@odroid:~# apt install python3-aiohttp python3-async-timeout

// system reboot
root@odroid:~# reboot

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
root@odroid:~# git clone --recursive https://github.com/charles-park/JIG.Client

or

root@odroid:~# git clone https://github.com/charles-park/JIG.Client
root@odroid:~# cd JIG.Client
root@odroid:~/JIG.Client# git submodule update --init --recursive
```

### Auto login
```
root@odroid:~# systemctl edit getty@tty1.service
```
```
[Service]
ExecStart=
ExecStart=-/sbin/agetty --noissue --autologin root %I $TERM
Type=idle
```
* edit tool save
  save exit [Ctrl+ k, Ctrl + q]

### Disable Console (serial ttyS0), hdmi 1920x1080, gpio overlay disable
```
root@odroid:~# vi /medoa/boot/boot.ini
...
# setenv condev "console=ttyS0,115200n8"   # on both
...

root@odroid:~# vi /medoa/boot/config.ini
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
root@odroid:~# vi ~/.bashrc
...
setterm -blank 0 -powerdown 0 -powersave off 2>/dev/null
echo 0 > /sys/class/graphics/fb0/blank
...
```

### server static ip settings
```
root@odroid:~# vi /etc/netplan/01-netcfg.yaml
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
root@odroid:~# netplan apply
root@odroid:~# ifconfig
```

### server samba config
```
root@odroid:~# smbpasswd -a root
root@odroid:~# vi /etc/samba/smb.conf
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
root@odroid:~# service smbd restart
```

### Overlay root
* overlayroot enable
```
root@odroid:~# update-initramfs -c -k $(uname -r)
update-initramfs: Generating /boot/initrd.img-4.9.337-17

root@odroid:~# mkimage -A arm64 -O linux -T ramdisk -C none -a 0 -e 0 -n uInitrd -d /boot/initrd.img-$(uname -r) /media/boot/uInitrd 
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
root@odroid:~# overlayroot-chroot 
INFO: Chrooting into [/media/root-ro]
root@odroid:~# 

// Change overlayroot value "tmpfs" to "" for overlayroot disable
root@odroid:~# vi /etc/overlayroot.conf
...
overlayroot_cfgdisk="disabled"
overlayroot=""
```


