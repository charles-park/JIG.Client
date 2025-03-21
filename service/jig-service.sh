#!/bin/bash

# device stable delay
# sleep 10 && sync

# ODROID-C5 audio setup
amixer sset 'TDMOUT_C Mute' off

#--------------------------
# ODROID-C4 Client enable
#--------------------------
/usr/bin/sync && /root/JIG.Client/JIG.Client > /dev/null 2>&1
