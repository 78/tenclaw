#!/bin/bash
# Build a minimal BusyBox initramfs for TenClaw Phase 1 testing.
# Run this in WSL2 or a Linux environment.
set -e

OUTDIR="$(cd "${1:-.}" && pwd)"
WORKDIR=$(mktemp -d)
trap "rm -rf $WORKDIR" EXIT

echo "[1/4] Downloading BusyBox static binary..."
cd "$WORKDIR"
apt-get download busybox-static 2>/dev/null || \
    apt download busybox-static 2>/dev/null
dpkg-deb -x busybox-static*.deb extract/
cp extract/bin/busybox "$WORKDIR/busybox"
chmod +x "$WORKDIR/busybox"

echo "[2/4] Creating initramfs layout..."
mkdir -p "$WORKDIR/initramfs"/{bin,dev,proc,sys,etc,tmp}
cp "$WORKDIR/busybox" "$WORKDIR/initramfs/bin/"

cat > "$WORKDIR/initramfs/init" << 'EOF'
#!/bin/busybox sh
/bin/busybox mkdir -p /proc /sys /dev /tmp
/bin/busybox mount -t proc none /proc
/bin/busybox mount -t sysfs none /sys
/bin/busybox mount -t devtmpfs none /dev 2>/dev/null

/bin/busybox --install -s /bin

echo ""
echo "====================================="
echo " TenClaw VM booted successfully!"
echo "====================================="
echo "Kernel: $(uname -r)"
echo "Memory: $(cat /proc/meminfo | head -1)"
echo ""

exec /bin/sh
EOF
chmod +x "$WORKDIR/initramfs/init"

echo "[3/4] Packing initramfs..."
cd "$WORKDIR/initramfs"
find . | cpio -o -H newc 2>/dev/null | gzip > "$WORKDIR/initramfs.cpio.gz"

echo "[4/4] Copying output..."
cp "$WORKDIR/initramfs.cpio.gz" "$OUTDIR/initramfs.cpio.gz"
echo "Done: $OUTDIR/initramfs.cpio.gz ($(du -h "$OUTDIR/initramfs.cpio.gz" | cut -f1))"
