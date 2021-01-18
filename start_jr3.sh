#!/bin/bash
sudo insmod ~/jr3/jr3pci-driver.ko
sleep 1
sudo rmmod jr3pci_driver
sleep 1
sudo insmod ~/jr3/jr3pci-driver.ko
echo Ready
