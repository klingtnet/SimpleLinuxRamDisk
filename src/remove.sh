#!/bin/bash

# RamDisk - Remove module script
#
# Andreas Linz & Tobias Sekan
# andreas.linz@stud.htwk-leipzig.de
# Systemprogrammierung -- WS 2012/13
# HTWK

mountdir=/media/RamDisk
modname=ramdisk

echo "Unmounting..."
sudo umount $mountdir

echo "Removing module..."
sudo rmmod $modname

echo "Deleting mount directory..."
sudo rmdir $mountdir
