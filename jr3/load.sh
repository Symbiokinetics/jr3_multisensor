#!/bin/bash

module="jr3"
device="jr3"
group="staff"
mode="664"

echo Removing drivers
driver=`lsmod | grep -i jr3_pci | wc -l`
if [[ $driver -gt 0 ]]; then
	echo Removing jr3_pci
	rmmod jr3_pci
fi

driver=`lsmod | grep -i jr3pci_driver | wc -l`
if [[ $driver -gt 0 ]]; then
	echo Removing jr3pci_driver
	rmmod jr3pci_driver
fi

sync
sleep 1

echo Loading driver
insmod ./jr3pci-driver.ko || exit 1
sync
sleep 1

major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices) 
echo Major number $major

devices=`dmesg | tail -n 20 | grep "registered, current number of devices:" | tail -n 1 | cut -d ':' -f 2`

if [[ $devices -gt 0 ]]; then
	echo $devices available
	rm -f /dev/$device*
	for (( i=0; i<$devices; i++))
	do
		d="$device$i"
		echo Creating device $d
		mknod /dev/$d c $major $i
		chgrp $group /dev/$d
		chmod $mode /dev/$d
	done
else
	echo No device detected
fi

