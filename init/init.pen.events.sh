#!/system/bin/sh

# Filter Focus Pen events through daemon

EVENT_NAMES=$(awk 'BEGIN { RS=""; FS="\n" }
     /Name="Xiaomi Focus Pen/ && !/Keyboard/ && !/Mouse/ {
         for (i=1; i<=NF; i++)
             if ($i ~ /Handlers=/) {
                 match($i, /event[0-9]+/)
                 print substr($i, RSTART, RLENGTH)
             }
     }' /proc/bus/input/devices)

[ -z "$EVENT_NAMES" ] && exit

# Delete the broken event node
for event in $EVENT_NAMES; do
    rm -f "/dev/input/$event"
done
