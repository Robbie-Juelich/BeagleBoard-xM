
-----------------------------------------------------------------------
|                                                                     |
|Aptina MT9D131 (2-meg pixel SOC) image sensor linux driver release   |
|                                                                     |
-----------------------------------------------------------------------
    Release notes for the Aptina MT9D131 image sensor linux driver.
    This document contains information on how to download the OS, build
    the driver/OS and how to use the sensor. Please read this document
    before posting questions to the drivers@aptina.com e-mail address.


MT9D131 Driver
--------------
    The MT9D131 driver is a Video for Linux 2 (V4L2) driver that is compiled
    into the Beagleboard-xM Linux Angstrom distribution. It is distributed under
    the GNU General Public License agreement which can be obtained from Free Software 
    Foundation, Inc., 675 Mass Ave, Cambridge, MA02139, USA. 


TARGET HARDWARE/SOFTWARE
------------------------
    - Beagleboard-xM Rev B.
    - Linux Angstrom Distribution, kernel release 2.6.32.
    - Aptina Beagleboard-XM Adapter/Revision 0 and MT9D131 2-megapixel soc sensor.


DRIVER SOURCE CODE FILES
------------------------
    Driver files and directory locations are listed below:
    mt9d131.c, Makefile, and Kconfig are located at:
        kernel-2.6.32/drivers/media/video

    mt9d131.h and v4l2-chip-ident.h are located at:
        kernel-2.6.32/include/media

    board-omap3beagle.c and board-omap3beagle-camera.c are located at:
        kernel-2.6.32/arch/arm/mach-omap2


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
    Copy MT9D131 related files into the kernel directories:
        $cp your_mt9d131_driver_directory/board-omap3beagle.c  ./arch/arm/mach-omap2
        $cp your_mt9d131_driver_directory/board-omap3beagle-camera.c  ./arch/arm/mach-omap2
        $cp your_mt9d131_driver_directory/mt9d131.c            ./drivers/media/video
        $cp your_mt9d131_driver_directory/Makefile             ./drivers/media/video
        $cp your_mt9d131_driver_directory/Kconfig              ./drivers/media/video
        $cp your_mt9d131_driver_directory/mt9d131.h            ./include/media
        $cp your_mt9d131_driver_directory/v4l2-chip-ident.h    ./include/media

    At the root directory of Linux kernel source files, enter the commands:
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- distclean
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- omap3_beagle_cam_defconfig
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- menuconfig

    In menuconfig, enable the MT9D131 driver. The Mt9D131 driver configuration is at the
    following location:
        "Device Drivers"-->"Multimedia support"-->"Video capture adapters"-->"Encoders/decoders and other helper chips" 

    Select "mt9d131 support" as either
        <*> mt9d131 support
    so that the driver is part of kernel image, or

        <M> mt9d131 support
    which configures the mt9d131 driver as a loadable module.

    Note: other image sensors (such as the MT9V011 or MT9P012) should be de-selected.

    Enable/Disable MT9D131 driver compilation options that are located in the mt9d131.c file:
        MT9D131_DEBUG - Define this to enable SYSFS sensor register access and debug printk support
            Undefine this for a driver release. 
 
    Compile the kernel:
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- uImage

    and compile the mt9d131 driver separately if it is configured as a loadable module
        $make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- modules

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

    Copy the driver to the EXT3 partition if the mt9pd131 driver is configured as a loadable module
        $cp drivers/media/video/mt9d131.ko  /media/Anstrom/home/root

    Then umount the card
        $sync; sudo umount /media/*

    Once the kernel and file-system are copied to the SD card, insert the SD card into the Beagleboard and power it on.


BOOT UP BEAGLEBOARD
-------------------
    Follow the standard procedures to boot up the Beagleboard.  If the sensor driver is configured as a loadable module, 
    it needs to be inserted manually:
        #cd /home/root
        #insmod mt9d131.ko


ANGSTROM VIDEO APPLICATION USING THE MT9D131 SENSOR
---------------------------------------------------
    The Angstrom "mplayer" video application can be used for previewing video at various
    resolutions such as VGA(640x480) or 720p(1280x720):
        $mplayer tv:// -tv driver=v4l2:width=640:height=480:device=/dev/video0:fps=12 -vo x11
        $mplayer tv:// -tv driver=v4l2:width=1280:height=720:device=/dev/video0:fps=4 -vo x11

    Note: run the following commands from the Angstrom terminal prompt if the mplayer application
    is not installed:
        #opkg install mplayer midori
        #opkg update; opkg install ntpdate


MT9D131 SUPPORTED OUTPUT FRAME SIZES
------------------------------------
    width=80,   height=60
    width=176,  height=144
    width=320,  height=240
    width=352,  height=288
    width=400,  height=300
    width=640,  height=480
    width=800,  height=600
    width=1280, height=720
    width=1280, height=960
    width=1280, height=1024
    width=1600, height=1200


MT9D131 SUPPORTED OUTPUT FRAME FORMATS
------------------------------
    YUYV(Y-Cb-Y-Cr)
    UYVY(Cb-Y-Cr-Y)


USE MPLAYER TO TAKE SNAPSHOTS
-----------------------------
    To save a JPEG format image to the current directory:
    $mplayer tv:// -tv driver=v4l2:width=640:height=480:device=/dev/video0:fps=12 -vo jpeg
    $mplayer tv:// -tv driver=v4l2:width=1600:height=1200:device=/dev/video0:fps=4 -vo jpeg


DIRECT ACCESS TO IMAGE SENSOR REGISTERS VIA SYSFS
-------------------------------------------------
    For the following to work, the MT9D131 driver must be compiled with the debugging
    option enabled (i.e., the MT9D131_DEBUG flag is enabled).

    The MT9D131 driver exposes device registers to the user via the SYSFS interface. This
    means device registers can be written/read as though they are files.

    To access sensor registers, navigate to the following system directory:
        $cd /sys/devices/platform/i2c_omap.2/i2c-2/2-0048/

Basic Register Read/Write
    for reading Register 0x07:
        $echo 0x07 > basic_reg_addr
        $cat basic_reg_val

    for writing 0x4 to Register 0x08:
        $echo 0x08 > basic_reg_addr
        $echo 0x04 > basic_reg_val

KNOWN ISSUE
-----------
    V4L2 support is limited for this release:
    1. The AE/AWB are on by default and cannot be disabled.
    2. The maximum frame rate is 12 frame/seconds which changes dynamically depending on the lighting 
           conditions and the frame size.  The frame rate cannot be set to a specific value by end-user. 
