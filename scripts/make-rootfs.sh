#!/bin/bash
# Build a minimal Debian rootfs as qcow2 for TenClaw Phase 2.
# Requires: debootstrap, qemu-utils. Run as root in WSL2 or Linux.
set -e

ROOTFS_SIZE="2G"
OUTPUT="${1:-rootfs.qcow2}"
SUITE="bookworm"
MIRROR="http://deb.debian.org/debian"

echo "[1/5] Creating raw image..."
truncate -s $ROOTFS_SIZE rootfs.raw
mkfs.ext4 -F rootfs.raw

echo "[2/5] Bootstrapping Debian ${SUITE}..."
MOUNT_DIR=$(mktemp -d)
sudo mount -o loop rootfs.raw "$MOUNT_DIR"
sudo debootstrap --variant=minbase "$SUITE" "$MOUNT_DIR" "$MIRROR"

echo "[3/5] Configuring system..."
sudo chroot "$MOUNT_DIR" /bin/bash << 'EOF'
echo "root:tenclaw" | chpasswd
echo "tenclaw-vm" > /etc/hostname
echo "/dev/vda / ext4 defaults 0 1" > /etc/fstab

apt-get install -y --no-install-recommends \
    systemd-sysv udev iproute2 iputils-ping curl
apt-get clean
rm -rf /var/lib/apt/lists/*

mkdir -p /etc/systemd/system/serial-getty@ttyS0.service.d
cat > /etc/systemd/system/serial-getty@ttyS0.service.d/autologin.conf << 'INNER'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin root --noclear %I 115200 linux
INNER

cat > /etc/network/interfaces << 'NET'
auto lo
iface lo inet loopback
auto eth0
iface eth0 inet dhcp
NET
EOF

echo "[4/5] Unmounting..."
sudo umount "$MOUNT_DIR"
rmdir "$MOUNT_DIR"

echo "[5/5] Converting to qcow2..."
qemu-img convert -f raw -O qcow2 -o cluster_size=65536 rootfs.raw "$OUTPUT"
rm rootfs.raw

echo "Done: $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
