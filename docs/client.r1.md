# Client : ODROID-M1 (2025-09-16)

* Document : [ODROID-R1 JIG연결도(2026)](https://docs.google.com/spreadsheets/d/1uO0tWpi965akbPAEyKgAVrds5GoxAPgPy4x0gtpPmIw/edit?gid=0#gid=0)
* Config   : [DEV Config](/configs/r1_dev.cfg), [UI Config](/configs/r1_ui.cfg)
* Image PATH     : smb://odroidh3.local/sharedfolder/생산관리/jig/odroid-r1/
* Release Image  : jig_r1_client_20261027.img
* Linux OS Image : bob_odroid_m1.client.base_image.250908.xz
```
root@server:~# uname -a
Linux server 5.15.153-odroid-arm64 #1 SMP PREEMPT Thu, 01 Oct 2026 11:53:24 +0000 aarch64 aarch64 aarch64 GNU/Linux
```

#### ⚠️ **Note : `A Linux server image with efuse, Ethernet RX/TX delay, and console disabled must be used.`**

## Test items
```
  HDMI     : FB, EDID, HPD
  STORAGE  : eMMC, SATA, NVME
  USB      : USB3.0 x 2, USB2.0 x 2
  ETHERNET : IPERF(Server), IPERF(Client), MAC Write
  HEADER   : H40, H14
  ADC      : Header40 - 37Pin, 40Pin
  LED      : Power(Red), Alive(Blue), Ethernet(Green/Orange), NVME(Green)
  MISC     : FAN Speed
```
## Documents
  * ODROID-R1 H/W Reference (https://docs.google.com/document/d/1TNhSs4qZkUbFp89wtBPSbXLV8aOOJYGdCszPE2My2Nc/edit?tab=t.0#heading=h.5yr21pqx03w4)
  * UUID (New MAC Server) (https://docs.google.com/document/d/1XN0q9RoX0bSwLqH3smzBSMWo7QhCxXD4S3sSKmDnW5I/edit?tab=t.0)
