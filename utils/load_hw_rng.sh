#!/bin/sh

echo "Unloading bcm2835_rng kernel module"
sudo modprobe -r bcm2835_rng
echo "Loading tpm_rng kernel module"
sudo insmod /lib/modules/4.1.21-v7+/kernel/drivers/char/hw_random/tpm-rng.ko
sudo modprobe tpm_rng
echo "Done."

