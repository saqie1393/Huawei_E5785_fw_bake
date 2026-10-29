#!/system/bin/busybox sh
# Create bin directory and symlink
mkdir /bin
ln -s /system/bin/sh /bin/sh
# Start background process
/system/sbin/NwInquire &
# System configurations
busybox echo 0 > /proc/sys/net/netfilter/nf_conntrack_checksum
# Huawei process start
/etc/huawei_process_start
# Remount partitions as writable
mount -o remount,rw /mnvm2:0
mount -o remount,rw /system
mount -o remount,rw /app
# NV tool operations
balong-nvtool -m 8268:01:00:00:00:02:00:00:00:0A:00:00:00
# Restore original autorun script
mv /system/etc/autorun.sh.orig /system/etc/autorun.sh

# Reset command
echo -e "at^reset\r" >/dev/appvcom1
