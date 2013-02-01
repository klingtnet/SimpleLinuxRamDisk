#!/bin/bash

# RamDisk - Insert and test script
#
# Andreas Linz & Tobias Sekan
# andreas.linz@stud.htwk-leipzig.de
# Systemprogrammierung -- WS 2012/13
# HTWK

mountdir=/media/RamDisk
devfile=/dev/RamDisk_a
modname=ramdisk
filesys=ext3
disksize=64

clear
echo "Press any key to create Ramdisk..."
read -n 1   # read key

if [ $# -eq 0 ]  # $# gives number of arguments
    then
        echo "No argument supplied, using standard disksize."
elif [ $1 -lt 1 ]
    then
        echo "Bad disk size ("$1")"
        echo "Disksize must be in range: [1, Max. Memsize]"
        exit
else
    echo "Trying to create disk with "$1" MiB Capacity."
    disksize=$1
fi

echo "Creating mount directory..."
sudo mkdir $mountdir

echo "Insert module ramdisk.ko..."
sudo insmod ramdisk.ko user_disk_size=$disksize

echo "Creating filesystem \""$filesys"\" on ramdisk"
sudo mkfs -t $filesys $devfile

echo "Mounting ramdisk in "$mountdir
sudo mount -t $filesys $devfile $mountdir

echo "Changing permissions of "$mountdir
sudo chmod a+wrx $mountdir

echo "Executing benchmark..."
sudo hdparm -Tt $devfile

echo "Goodbye and have a nice day!"
