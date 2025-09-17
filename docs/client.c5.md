# JIG.Client

2024 New version JIG-Client (JIG Client Base version)
2025 New version JIG-Client (branch client.v20)

* Document : https://docs.google.com/spreadsheets/d/1igBObU7CnP6FRaRt-x46l5R77-8uAKEskkhthnFwtpY/edit?gid=719914769#gid=719914769


### ODROID-C5 (2025-01-06)
* Linux OS Image : factory-odroidc5-0307.img (odroidh server)
* jig-c5.base.0307.img (auto login, all ubuntu package installed, lirc installed, python3 module installed, git default setting)
* jig-c5.client-c4.img, jig-c5.client-c5.img (2025.03.13 first release)
```
root@server:~# uname -a
Linux server 5.15.153-odroid-arm64 #101 SMP PREEMPT Fri Mar 7 11:28:21 KST 2025 aarch64 aarch64 aarch64 GNU/Linux
```

### ODROID-C5 (2025-06-19) Update : Bootloader DDR Clock Error Fix (1968Mhz -> 1896Mhz)
* Linux OS Image : ubuntu-22.04-factory-odroidc5-odroidc5-20250619.img.xz
```
root@server:~# uname -a
Linux server 5.15.153-odroid-arm64 #1 SMP PREEMPT Wed, 18 Jun 2025 08:31:13 +0000 aarch64 aarch64 aarch64 GNU/Linux
```
  
### Install package
```
// ubuntu package install
root@server:~# apt install build-essential vim ssh git python3 python3-pip ethtool net-tools usbutils i2c-tools overlayroot nmap evtest htop cups cups-bsd iperf3 alsa samba lirc evtest

// ubuntu 24.01 version python3 package install
root@server:~# apt install python3-aiohttp python3-async-timeout

// system reboot
root@server:~# reboot


```

### Github setting
```
root@server:~# git config --global user.email "charles.park@hardkernel.com"
root@server:~# git config --global user.name "charles-park"
```

### Clone the reopsitory with submodule (update 2025.06.19 : branch merge client.v20 -> main)
```
root@server:~# git clone -b client.v20 --recursive https://github.com/charles-park/JIG.Client (old)
root@server:~# git clone --recursive https://github.com/charles-park/JIG.Client (update 2025.06.19)

or

root@server:~# git clone -b client.v20 https://github.com/charles-park/JIG.Client (old)
root@server:~# git clone https://github.com/charles-park/JIG.Client (old)
root@server:~# cd JIG.Client
root@server:~/JIG.Client# git submodule update --init --recursive
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

### Disable Console (serial ttyS0), hdmi 1920x1080, gpio overlay disable
```
root@server:~# vi /boot/boot.ini
...
# setenv condev "console=ttyS0,115200n8"   # on both (old)
...

root@server:~# vi /boot/config.ini
# default_console=ttyS0,921600
default_console=ttyS0,19200
overlay_resize=16384
overlay_profile=""

# overlays="spi0 i2c0 i2c1"

# Activate in Server Mode
# overlays="i2c0 i2c1"

# Activate in Server Mode
overlays="ir"

gfx-heap-size=180

# Framebuffer resolution must be 1920x1080(Jig C4 Vu7 LCD) or 1920x7201920x720(Jig-C5 Vu12 LCD) on ServerMode. 
# outputmode="1080p60hz"

; overlays=""
...
```

### Sound setup (TDM-C-T9015-audio-hifi-alsaPORT-i2s)
```
// Codec info
root@server:~# aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: AMLAUGESOUND [AML-AUGESOUND], device 0: TDM-B-dummy-alsaPORT-i2s2hdmi soc:dummy-0 []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: AMLAUGESOUND [AML-AUGESOUND], device 1: SPDIF-B-dummy-alsaPORT-spdifb soc:dummy-1 []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: AMLAUGESOUND [AML-AUGESOUND], device 2: TDM-C-T9015-audio-hifi-alsaPORT-i2s fe01a000.t9015-2 []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: AMLAUGESOUND [AML-AUGESOUND], device 3: SPDIF-dummy-alsaPORT-spdif soc:dummy-3 []
  Subdevices: 1/1
  Subdevice #0: subdevice #0

// config mixer (mute off)
root@server:~# amixer sset 'TDMOUT_C Mute' off
```

* Sound test (Sign-wave 1Khz)
```
// use speaker-test
root@server:~# speaker-test -D hw:0,2 -c 2 -t sine -f 1000           # pin header target, all
root@server:~# speaker-test -D hw:0,2 -c 2 -t sine -f 1000 -p 1 -s 1 # pin header target, left
root@server:~# speaker-test -D hw:0,2 -c 2 -t sine -f 1000 -p 1 -s 2 # pin header target, right

// or use aplay with (1Khz audio file)
root@server:~# aplay -Dhw:0,2 {audio file} -d {play time}
```

### Disable screen off
```
root@server:~# vi ~/.bashrc
...
setterm -blank 0 -powerdown 0 -powersave off 2>/dev/null
echo 0 > /sys/class/graphics/fb0/blank
...
```

### server static ip settings (For Debugging)
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

### Overlay root
* overlayroot enable
```
root@server:~# update-initramfs -c -k $(uname -r)
update-initramfs: Generating /boot/initrd.img-4.9.337-17

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
