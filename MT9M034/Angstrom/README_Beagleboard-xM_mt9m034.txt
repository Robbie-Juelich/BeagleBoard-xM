
-------------------------------------------------------------------------
|									|
|	Aptina MT9M034 (1.2MP) CMOS image sensor linux driver release	|
|									|
-------------------------------------------------------------------------
    Release notes for the Aptina MT9M034 image sensor linux driver.
    This document contains information on how to download the OS, build
    the driver/OS and how to use the sensor. Please read this document
    before posting questions to the drivers@aptina.com e-mail address.


MT9M034 Driver
--------------
    The MT9M034 driver is a Video for Linux 2 (V4L2) driver that is compiled
    into the Beagleboard-xM Linux Angstrom distribution. It is distributed under
    the GNU General Public License agreement which can be obtained from Free Software 
    Foundation, Inc., 675 Mass Ave, Cambridge, MA02139, USA. 


TARGET HARDWARE/SOFTWARE
------------------------
    - Beagleboard-xM Rev C.
    - Linux Angstrom Distribution, kernel release 3.1.2.
    - Aptina Beagleboard-XM Adapter/Revision 0 and MT9M034 1.2MP CMOS sensor.


DRIVER SOURCE CODE FILES
------------------------
    Driver files and directory locations are listed below:
    mt9m034.c, Makefile, and Kconfig are located at:
        kernel-3.1.2/drivers/media/video

    mt9m034.h is located at:
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
    Copy MT9M034 related files into the kernel directories:
        $cp your_mt9m034_driver_directory/board-omap3beagle.c		./arch/arm/mach-omap2
        $cp your_mt9m034_driver_directory/board-omap3beagle-camera.c	./arch/arm/mach-omap2
        $cp your_mt9m034_driver_directory/mt9m034.c			./drivers/media/video
        $cp your_mt9m034_driver_directory/Makefile			./drivers/media/video
        $cp your_mt9m034_driver_directory/Kconfig			./drivers/media/video
        $cp your_mt9m034_driver_directory/mt9m034.h			./include/media

    Edit ./arch/arm/mach-omap2/Makefile to include board-omap3beagle-camera.c
        obj-$(CONFIG_MACH_OMAP3_BEAGLE)         += board-omap3beagle.o \
        	                                   board-omap3beagle-camera.o \
                	                           hsmmc.o

    Copy omap3_beagle_cam_defconfig file from https://github.com/aptina/BeagleBoard-xM :
        $cp omap3_beagle_cam_defconfig ./arch/arm/configs/

    At the root directory of Linux kernel source files, enter the commands:
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- distclean
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- omap3_beagle_cam_defconfig
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- menuconfig

    In menuconfig, enable the MT9M034 driver. The MT9M034 driver configuration is at the
    following location:
        "Device Drivers"-->"Multimedia support"-->"Video capture adapters"-->"Encoders/decoders and other helper chips" 

    Select "Aptina MT9M034 support"
	<*> Aptina MT9M034 support

    Note: other image sensors (such as the MT9V011 or MT9P015) should be de-selected.

    Enable/Disable MT9M034 driver compilation options that are located in the mt9m034.c file:
        MT9M034_DEBUG - Define this to enable debug print support
            Undefine this for a driver release. 
        MT9M034_I2C_DEBUG - Define this to enable i2c register writes debug print support
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

    Copy the driver to the EXT3 partition if the mt9m034 driver is configured as a loadable module
        $cp drivers/media/video/mt9m034.ko  /media/Anstrom/home/root

    Then umount the card
        $sync; sudo umount /media/*

    Once the kernel and file-system are copied to the SD card, insert the SD card into the Beagleboard and power it on.


BOOT UP BEAGLEBOARD
-------------------
    Follow the standard procedures to boot up the Beagleboard.  


MT9M034 SUPPORTED OUTPUT FRAME FORMATS
------------------------------
  RGB


KNOWN ISSUE
-----------
