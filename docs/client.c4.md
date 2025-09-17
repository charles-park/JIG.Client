# ODROID-C4 (2025-01-06)

* Document : [ODROID-C4 - ADC Board 연결도(2024)](https://docs.google.com/spreadsheets/d/1igBObU7CnP6FRaRt-x46l5R77-8uAKEskkhthnFwtpY/edit?gid=0#gid=0)
* Image PATH     : smb://odroidh3.local/sharedfolder/생산관리/jig/ODROID-C4.new/
* Linux OS Image : ubuntu-24.01-server-c4-20241206-all_update.img
* Release Image  : jig-c4.client-c4.Mar_14_2025.img

## Test items
```
  HDMI     : FB, EDID, HPD
  STORAGE  : eMMC
  USB      : USB3.0 x 4, OTG
  ETHERNET : IPERF(Server), MAC Write
  HEADER   : H40.H07
  ADC      : Header40 - 37Pin, 40Pin
  LED      : Power(Red), Alive(Blue), Ethernet(Green/Orange)
  IR       : IR Receive
  FW       : USB Hub F/W Write
```
## IR Settung
 * [LIRC Setup](https://wiki.odroid.com/odroid-c4/application_note/lirc/lirc_ubuntu18.04)

## Disable Console (serial ttyS0), hdmi 1920x1080, gpio overlay disable
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

