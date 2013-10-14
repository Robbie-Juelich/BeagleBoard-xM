
-----------------------------------------------------------------------
|                                                                     |
|Aptina AR0130 (1.2 megapixel bayer) image sensor linux driver release |
|                                                                     |
-----------------------------------------------------------------------
    Release notes for the Aptina AR0130 image sensor linux driver.
    This document contains information on how to download the OS, build
    the driver/OS and how to use the sensor. Please read this document
    before posting questions to the drivers@aptina.com e-mail address.


AR0130 Driver
--------------
    The AR0130 driver is a Video for Linux 2 (V4L2) driver that is compiled
    into the Beagleboard-xM Linux Angstrom distribution. It is distributed under
    the GNU General Public License agreement which can be obtained from Free Software 
    Foundation, Inc., 675 Mass Ave, Cambridge, MA02139, USA. 


TARGET HARDWARE/SOFTWARE
------------------------
    - Beagleboard-xM Rev C.
    - Linux Angstrom Distribution, kernel release 3.1.2.
    - Aptina Beagleboard-xM Adapter/Revision 0 and AR0130 1.2-megapixel raw sensor.


DRIVER SOURCE CODE FILES
------------------------
    Driver files and directory locations are listed below:
    ar0130.c, ar0130_data.h, Makefile, and Kconfig are located at:
        kernel-3.1.2/drivers/media/video

    ar0130.h and v4l2-chip-ident.h are located at:
        kernel-3.1.2/include/media

    board-omap3beagle.c and board-omap3beagle-camera.c are located at:
        kernel-3.1.2/arch/arm/mach-omap2


BEAGLEBOARD/ANGSTROM SETUP 
--------------------------
    For general beagleboard setup, please see the following link:
        http://code.google.com/p/beagleboard/wiki/HowToGetAngstromRunning
        
    Many of the Beagleboard/Angstrom files can be found at the following link:
        http://www.angstrom-distribution.org/demo/beagleboard/
    
    The Linux kernel source code tar file 
        beagleboard-validation-linux-beaglebardXM-camwork.tar.gz
    can be found at the following location:
        http://gitorious.org/beagleboard-validation/linux/trees/beaglebardXM-camwork

    Once downloaded, uncompress the file with the following command:
        $tar zxvf beagleboard-validation-linux-beaglebardXM-camwork.tar.gz
    Ignore the error messages if any.

    Download and install the compilation tool chain from:
        http://www.angstrom-distribution.org/toolchains/

    For Intel i686 Linux desktop PC platforms, the tar toolchain file is:
        angstrom-2011.03-i686-linux-armv7a-linux-gnueabi-toolchain.tar.bz2


PREPARING SD-CARD
-----------------
    Download mkcard.sh from:
        http://gitorious.org/beagleboard-validation/scripts/trees/60f43aeb22e5ce799eda06c82e7d36d3f04cf7d2

    Format & partition the sd-card at /dev/sdb 
    (the actual drive mounts listed here may be different on your host system)
        $sudo $Beagleboard/binaries/mkcard.sh /dev/sdb    

    This creates two partitions: /dev/sdb1 and /dev/sdb2, which are FAT32 and ext2 file formats respectively.


LINUX KERNEL CONFIGURATION/COMPILATION
--------------------------------------
    Copy AR0130 related files into the kernel directories:
        $cp your_ar0130_driver_directory/board-omap3beagle.c		./arch/arm/mach-omap2
        $cp your_ar0130_driver_directory/board-omap3beagle-camera.c	./arch/arm/mach-omap2
        $cp your_ar0130_driver_directory/ar0130.c			./drivers/media/video
        $cp your_ar0130_driver_directory/ar0130_data.h			./drivers/media/video
        $cp your_ar0130_driver_directory/Makefile			./drivers/media/video
        $cp your_ar0130_driver_directory/Kconfig			./drivers/media/video
        $cp your_ar0130_driver_directory/ar0130.h			./include/media
        $cp your_ar0130_driver_directory/v4l2-chip-ident.h		./include/media

    At the root directory of Linux kernel source files, enter the commands:
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- distclean
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- omap3_beagle_cam_defconfig
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- menuconfig

    In menuconfig, enable the AR0130 driver. The AR0130 driver configuration is at the
    following location:
        "Device Drivers"-->"Multimedia support"-->"Video capture adapters"-->"Encoders/decoders and other helper chips" 

    Select "Aptina AR0130 support" as either
        <*> Aptina AR0130 support
    so that the driver is part of kernel image, or

    Note: other image sensors (such as the MT9V011 or MT9P012) should be de-selected.

    Compile the kernel:
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- uImage

    Copy the kernel image to the SD card FAT partition:
        $cp arch/arm/boot/uImage  /media/boot

    Copy the u-boot and x-loader images to the SD card FAT partition:
        $cp MLO /media/boot/
        $cp u-boot.bin /media/boot/

        Prebuilt versions of MLO and u-boot.bin can be downloaded from the following location:
        http://www.angstrom-distribution.org/demo/beagleboard/

    Download the file-system file
    Angstrom-Beagleboard-demo-image-glibc-ipk-2011.1-beagleboard.rootfs.tar.bz2 
    from the following location:
        http://www.angstrom-distribution.org/demo/beagleboard/

    Uncompress the file-system and copy it to the SD card EXT3 partition
        $sudo tar -C /media/Angstrom -xjvf Angstrom-Beagleboard-demo-image-glibc-ipk-2011.1-beagleboard.rootfs.tar.bz2

    Then umount the card
        $sync; sudo umount /media/*

    Once the kernel and file-system are copied to the SD card, insert the SD card into the Beagleboard and power it on.


BOOT UP BEAGLEBOARD
-------------------
    Follow the standard procedures to boot up the Beagleboard.


AR0130 SUPPORTED OUTPUT FRAME SIZES
------------------------------------
    width=640,  height=360
    width=640,  height=480
    width=1280, height=720
    width=1280, height=960


AR0130 SUPPORTED OUTPUT FRAME FORMATS
--------------------------------------
	SRGB


LIMITATIONS
-----------
    Preview resolutions less than 640x360 are not supported.
    AE is enabled by default. AWB and AF are not supported.


KNOWN ISSUES
------------
    The debugging feature "SYSFS" is not enabled.
    The default sensor setting support the Aptina camera adapter card (vs. the Leopard
        Imaging adapter card)
