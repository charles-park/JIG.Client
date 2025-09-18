# ODROID-M1 (2025-09-16)

* Document : [ODROID-M1 - ADC Board 연결도(2024)](https://docs.google.com/spreadsheets/d/1mUUWAhZeI7kd9SqFgP_7Fea8CZK7xyCqQDq9VtFoWFI/edit?gid=0#gid=0)
* Config   : [DEV Config](/configs/m1_dev.cfg), [UI Config](/configs/m1_ui.cfg)
* Image PATH     : smb://odroidh3.local/sharedfolder/생산관리/jig/odroid-m1/2025.09.16_M1_JIG/
* Release Image  : m1_jig_client.Sep_15_2025.m1_emmc.img.xz
* Linux OS Image : bob_odroid_m1.server.base_image.250908.xz
```
root@server:~# uname -a
Linux server 5.10.198-odroid-arm64 #104 SMP Mon Sep 8 12:58:44 KST 2025 aarch64 aarch64 aarch64 GNU/Linux
```

#### ⚠️ **Note : `A Linux server image with efuse, Ethernet RX/TX delay, and console disabled must be used.`**

## Test items
```
  HDMI     : FB, EDID, HPD
  STORAGE  : eMMC, SATA, NVME
  USB      : USB3.0 x 2, USB2.0 x 2
  ETHERNET : IPERF(Server), IPERF(Client), MAC Write
  HEADER   : H40
  ADC      : Header40 - 37Pin, 40Pin
  LED      : Power(Red), Alive(Blue), Ethernet(Green/Orange), NVME(Green)
  AUDIO    : HP L/R, SPEAKER L/R
  IR       : IR Receive
  MISC     : SPI Button, HP Detect
```

## Petiboot skip
```
root@server:~# vi /boot/petitboot.cfg
[petitboot]
petitboot,timeout=0
```

## Disable Console (serial ttyS2), hdmi 800x480, gpio overlay disable
```
root@server:~# vi /boot/boot.ini
[generic]
#default_console=ttyFIQ0
overlay_resize=16384
overlay_profile=
overlays="fiq0_to_uart2"

[overlay_custom]
overlays="i2c0 i2c1"

[overlay_hktft32]
overlays="hktft32 ads7846"
```

## Sound setup (rk817-hifi-0)
```
// Codec info
root@server:~# aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: ODROIDM1FRONT [ODROID-M1-FRONT], device 0: dailink-multicodecs rk817-hifi-0 [dailink-multicodecs rk817-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 1: ODROIDM1HDMI [ODROID-M1-HDMI], device 0: fe400000.i2s-i2s-hifi i2s-hifi-0 [fe400000.i2s-i2s-hifi i2s-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0

// mixer config read & check (Playback Path)
root@server:~# amixer -c 0
...
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'OFF'
...

// Set config playback path (HP + SPK)
root@server:~# amixer -c 0 sset 'Playback Path' 'SPK_HP'

// mixer config confirm (Playback Path)
root@server:~# amixer -c 0
...
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'SPK_HP'
...

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

