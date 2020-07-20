#!/bin/bash

# Copy preset
echo "Copy preset"
sudo cp ./linux.preset /etc/mkinitcpio.d/linux5.4-dynamic.preset

echo "Change folder: linux-detection"
cd ../

# Create Headers
echo "Make headers"
make headers_install

# Install modules
echo "Installing modules"
sudo make modules_install

echo "Copy kernel image to boot"
sudo cp -v arch/x86_64/boot/bzImage /boot/vmlinuz-5.4-dynamic

# Generate initramfs
echo "Generate initramfs"
sudo mkinitcpio -p linux5.4-dynamic

# Copy System.Map
echo Copying System.map""
# sudo cp System.map /boot/System.map-5.4-hack
# sudo ln -sf /boot/System.map-5.4-hack /boot/System.map

echo "Updating grub"
sudo update-grub


