#!/system/bin/sh

EVENT_NAME=$(awk 'BEGIN { RS=""; FS="\n" }
     /Name="Xiaomi Focus Pen"/ {
         for (i=1; i<=NF; i++)
             if ($i ~ /Handlers=/) {
                 match($i, /event[0-9]+/)
                 print substr($i, RSTART, RLENGTH)
             }
     }' /proc/bus/input/devices)

[ -z "$EVENT_NAME" ] && exit
rm /dev/input/$EVENT_NAME
