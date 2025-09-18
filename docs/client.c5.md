# Client ODROID-C5 (2025-01-06)

* Document : [ODROID-C5 - ADC Board 연결도(2025)](https://docs.google.com/spreadsheets/d/1DmyNXs4d5W-9Q2ZlV6k4eF86kqg6jsr3/edit?gid=346818897#gid=346818897)
* Config   : [DEV Config](/configs/c5_dev.cfg), [UI Config](/configs/c5_ui.cfg)
* Image PATH     : smb://odroidh3.local/sharedfolder/생산관리/jig/odroid_c5_c4/2025.06.19_NewImage/
* Release Image  : jig-c4-c5.client-c5.Jun_20_2025.emmc.img
* Linux OS Image : ubuntu-22.04-factory-odroidc5-odroidc5-20250619.img.xz
```
root@server:~# uname -a
Linux server 5.15.153-odroid-arm64 #1 SMP PREEMPT Wed, 18 Jun 2025 08:31:13 +0000 aarch64 aarch64 aarch64 GNU/Linux
```
#### ⚠️ **Note : `The DDR clock must be verified as 1896Mhz in the bootloader`**

## Test items
```
  HDMI     : FB, EDID, HPD
  STORAGE  : eMMC
  USB      : USB2.0 x 4, OTG
  ETHERNET : IPERF(Server), MAC Write
  HEADER   : H40
  ADC      : Header40 - 37Pin, 40Pin
  LED      : Power(Red), Alive(Blue), Ethernet(Green/Orange)
  IR       : IR Receive
```

## Disable Console (serial ttyS0), hdmi 800x480, gpio overlay disable
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

# Activate in Client Mode
overlays="ir"

gfx-heap-size=180

# Framebuffer resolution must be 1920x1080(Jig C4 Vu7 LCD) or 1920x7201920x720(Jig-C5 Vu12 LCD) on ServerMode. 
# outputmode="1080p60hz"

; overlays=""
...
```

## Sound setup (TDM-C-T9015-audio-hifi-alsaPORT-i2s)
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

