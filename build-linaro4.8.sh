#!/bin/bash
TOOLCHAIN="/home/shivam/development/toolchains/arm-eabi-4.8/bin/arm-eabi"
MODULES_DIR="/home/shivam/development/modules"
ZIMAGE="/home/shivam/development/android_kernel_xiaomi_cancro/arch/arm/boot/zImage"
KERNEL_DIR="/home/shivam/development/android_kernel_xiaomi_cancro"
MKBOOTIMG="/home/shivam/boot-tools-falcon/tools/mkbootimg"
MKBOOTFS="/home/shivam/boot-tools-falcon/tools/mkbootfs"
DTBTOOL="/home/shivam/boot-tools-falcon/tools/dtbTool"
BUILD_START=$(date +"%s")
if [ -a $ZIMAGE ];
then
rm $ZIMAGE
rm $MODULES_DIR/*
fi
make ARCH=arm CROSS_COMPILE=$TOOLCHAIN- cancro_user_defconfig
make ARCH=arm CROSS_COMPILE=$TOOLCHAIN- -j8
if [ -a $ZIMAGE ];
then
echo "Copying modules"
rm $MODULES_DIR/*
find . -name '*.ko' -exec cp {} $MODULES_DIR/ \;
cd $MODULES_DIR
echo "Stripping modules for size"
$TOOLCHAIN-strip --strip-unneeded *.ko
cd $KERNEL_DIR
$DTBTOOL -o dt.img -s 2048 -p scripts/dtc/ arch/arm/boot/
$MKBOOTFS ramdisk/ > $KERNEL_DIR/ramdisk.cpio
cat $KERNEL_DIR/ramdisk.cpio | gzip > $KERNEL_DIR/root.fs
$MKBOOTIMG --kernel $ZIMAGE --ramdisk $KERNEL_DIR/root.fs --base 0x00000000 --cmdline "console=ttyHSL0,115200,n8 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3 androidboot.bootdevice=msm_sdcc.1 androidboot.selinux=permissive" --pagesize 2048 --dt dt.img -o $KERNEL_DIR/boot.img
BUILD_END=$(date +"%s")
DIFF=$(($BUILD_END - $BUILD_START))
echo "Build completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."
else
echo "Compilation failed! Fix the errors!"
fi
