
-----------------------------------------------------------------------
|                                                                     |
|Aptina MT9V034 (Wide-VGA) image sensor linux driver release   |
|                                                                     |
-----------------------------------------------------------------------
    Release notes for the Aptina MT9V034 image sensor linux driver.
    This document contains information on how to download the OS, build
    the driver/OS and how to use the sensor. Please read this document
    before posting questions to the drivers@aptina.com e-mail address.


MT9V034 Driver
--------------
    The MT9V034 driver is a Video for Linux 2 (V4L2) driver that is compiled
    into the Beagleboard-xM Linux Angstrom distribution. It is distributed under
    the GNU General Public License agreement which can be obtained from Free Software 
    Foundation, Inc., 675 Mass Ave, Cambridge, MA02139, USA. 


TARGET HARDWARE/SOFTWARE
------------------------
    - Beagleboard-xM Rev C.
    - Linux Angstrom Distribution, kernel release 3.1.2.
    - Aptina Beagleboard-XM Adapter/Revision 0 and MT9V034 Wide-VGA sensor.


DRIVER SOURCE CODE FILES
------------------------
    Driver files and directory locations are listed below:
    mt9v034.c, Makefile, and Kconfig are located at:
        kernel-3.1.2/drivers/media/video

    mt9v034.h and v4l2-chip-ident.h are located at:
        kernel-3.1.2/include/media

    board-omap3beagle.c and board-omap3beagle-camera.c are located at:
        kernel-3.1.2/arch/arm/mach-omap2


BEAGLEBOARD/ANGSTROM SETUP 
--------------------------
    For general beagleboard setup, please see the following link:
        http://code.google.com/p/beagleboard/wiki/HowToGetAngstromRunning
        
    Many of the Beagleboard/Angstrom files can be found at the following link:
        http://www.angstrom-distribution.org/demo/beagleboard/
    
    The Linux kernel source code tar file can be found at the following location:
	http://www.kernel.org/pub/linux/kernel/v3.x/linux-3.1.2.tar.bz2

    Once downloaded, uncompress the file with the following command:
        $tar jxvf linux-3.1.2.tar.bz2

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
    Copy MT9V034 related files into the kernel directories:
        $cp your_mt9v034_driver_directory/board-omap3beagle.c  ./arch/arm/mach-omap2
        $cp your_mt9v034_driver_directory/board-omap3beagle-camera.c  ./arch/arm/mach-omap2
        $cp your_mt9v034_driver_directory/mt9v034.c            ./drivers/media/video
        $cp your_mt9v034_driver_directory/Makefile             ./drivers/media/video
        $cp your_mt9v034_driver_directory/Kconfig              ./drivers/media/video
        $cp your_mt9v034_driver_directory/mt9v034.h            ./include/media
        $cp your_mt9v034_driver_directory/v4l2-chip-ident.h    ./include/media

    At the root directory of Linux kernel source files, enter the commands:
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- distclean
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- omap3_beagle_cam_defconfig
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- menuconfig

    In menuconfig, enable the MT9V034 driver. The MT9V034 driver configuration is at the
    following location:
        "Device Drivers"-->"Multimedia support"-->"Video capture adapters"-->"Encoders/decoders and other helper chips" 

    Select "Aptina A-351(MT9V034) Wide-VGA CMOS sensor support"
	<*> Aptina A-351(MT9V034) Wide-VGA CMOS sensor support

    Note: other image sensors (such as the MT9V011 or MT9P012) should be de-selected.

    Enable/Disable MT9V034 driver compilation options that are located in the mt9v034.c file:
        MT9V034_DEBUG - Define this to enable debug printk support
            Undefine this for a driver release. 
 
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

    Copy the driver to the EXT3 partition if the mt9v034 driver is configured as a loadable module
        $cp drivers/media/video/mt9v034.ko  /media/Anstrom/home/root

    Then umount the card
        $sync; sudo umount /media/*

    Once the kernel and file-system are copied to the SD card, insert the SD card into the Beagleboard and power it on.


BOOT UP BEAGLEBOARD
-------------------
    Follow the standard procedures to boot up the Beagleboard.  


MT9V034 SUPPORTED OUTPUT FRAME SIZES
------------------------------------
    width=80,   height=60
    width=160,  height=120
    width=176,  height=144
    width=320,  height=240
    width=352,  height=288 
    width=400,  height=300 
    width=640,  height=480
    width=752,  height=480


MT9V034 SUPPORTED OUTPUT FRAME FORMATS
------------------------------
  SRGB


KNOWN ISSUE
-----------
