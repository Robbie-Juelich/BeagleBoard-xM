/**************************************************************************************
;* Copyright 2009 Aptina Imaging Corporation. All rights reserved.
;*
;*
;* No permission to use, copy, modify, or distribute this software and/or
;* its documentation for any purpose has been granted by Aptina Imaging Corporation.
;* If any such permission has been granted ( by separate agreement ), it
;* is required that the above copyright notice appear in all copies and
;* that both that copyright notice and this permission notice appear in
;* supporting documentation, and that the name of Aptina Imaging Corporation or any
;* of its trademarks may not be used in advertising or publicity pertaining
;* to distribution of the software without specific, written prior permission.
;*
;*
;*  This software and any associated documentation are provided AS IS and 
;*  without warranty of any kind.   APTINA IMAGING CORPORATION EXPRESSLY DISCLAIMS 
;*  ALL WARRANTIES EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO, NONINFRINGEMENT 
;*  OF THIRD PARTY RIGHTS, AND ANY IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS 
;*  FOR A PARTICULAR PURPOSE.  APTINA IMAGING DOES NOT WARRANT THAT THE FUNCTIONS CONTAINED 
;*  IN THIS SOFTWARE WILL MEET YOUR REQUIREMENTS, OR THAT THE OPERATION OF THIS SOFTWARE 
;*  WILL BE UNINTERRUPTED OR ERROR-FREE.  FURTHERMORE, APTINA IMAGING DOES NOT WARRANT OR 
;*  MAKE ANY REPRESENTATIONS REGARDING THE USE OR THE RESULTS OF THE USE OF ANY 
;*  ACCOMPANYING DOCUMENTATION IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY, 
;*  OR OTHERWISE.  
;**************************************************************************************/

#ifndef __APTINAVPFE_H
#define __APTINAVPFE_H


////////////////////////// VPFE defs

// video port requires output to be 1 line fewer, we use 2 just to be safe
#define VPORT_OFFSET    2 // loss of lines due to video port at the output 
#define VPORTMASK_VPORT 0x01
#define VPORTMASK_SDRAM 0x02

#define FASTGRAB_DEFS
#define FASTGRAB_NOWAIT_START 0
#define FASTGRAB_NOWAIT_STATUS 1
#define FASTGRAB_NOWAIT_END 2
#define FASTGRAB_WAIT_FOR_COMPLETION 3

#define DEVICE_RAW		0
#define DEVICE_YUV		1

#define MAX_FASTGRAB_FRAME 1000
struct fastgrab_struct_kernel {
    unsigned long fastgrab_frames[MAX_FASTGRAB_FRAME];
    int num_frame;
    int framesize;
    int timediff_usec;
    int width;
    int height;
    int wait_for_completion;
    int current_fastgrab_frame;
};

struct black_compensation {
	char r_comp;		/* Constant value to subtract from Red component */
	char gr_comp;		/* Constant value to subtract from Gr component */
	char b_comp;		/* Constant value to subtract from Blue component */
	char gb_comp;		/* Constant value to subtract from Gb component */
};

struct device_format_struct {
    int device_format;
    unsigned long display_buffer_addr[3];
    int working_display;
    int target;
    int bitsPerSample;
    int thru_videoport;
    int MIPI2mem_direct;
};


////////////////////////// sensor i2c defs

// Define the size of data/address values
#define DATA_8BIT		1
#define DATA_16BIT		2
#define DATA_32BIT		4
#define ADDR_8BIT		1
#define ADDR_16BIT		2
#define ADDR_32BIT		4

// Register Interface Open/Close status flags
#define APTI2CSTAT_OK        0x1000		// I2C request (open/close) successful
#define APTI2CSTAT_FAIL      0x1001		// I2C request failed
#define APTI2CSTAT_ALREADY   0x1002		// I2C addr already in use

// Structure used to open and close the register transation
//  Used with VPFE_CMD_OPEN_I2C_DEV, VPFE_CMD_CLOSE_I2C_DEV calls
//  Single open call opens for both I2C and ISP register access
struct aptcam_i2c_params {
    unsigned int i2caddr;     // IN: I2C address to be opened
    unsigned int i2caddrsize; // IN: Size of the sensor address range (8, 16 or 32 bits)
    unsigned int i2cstatus;   // OUT: APTI2CSTAT_X return value
};

#define MAXBUFLEN_I2C 20
struct my_i2c_data {
 	unsigned short isRead;		
 	unsigned short len;		// msg length
 	unsigned char buf[MAXBUFLEN_I2C]; // pointer to msg data
};

struct my_i2c_param {
    int i2cfreq;
    int psc;
};


    // Memory allocation structures
#define CAMBUFCNTMAX  5			// Max buffers per allocation

    // Per memory block information (virtual and physical addresses)
struct om34cam_mem1 {
    unsigned int logaddr;            // OUT: kernel mode logical address
    unsigned int physaddr;           // OUT: physical address
    unsigned int mmap_addr;          // OUT: user mode logical address
};


    // Structure to allocate memory. From one to CAMBUFCNTMAX buffers per allocation allowed
struct om34cam_memalloc {
    unsigned int width;              // IN:  Width (in bytes) of the buffer to allocate
    unsigned int height;             // IN:  Height (in lines) of the buffer to allocate
    unsigned int size;               // IN:  - OR - Size of memory to allocate
    unsigned int mflags;             // IN:  Flags (defined below)
    unsigned int mcount;             // IN:  Number of buffers to allocate (0 or 1 for single buffer)
    unsigned int mhandle;            // OUT: Handle of the memory buffer
    struct om34cam_mem1 m1[CAMBUFCNTMAX];  // OUT: Array of buffer addresses 
};


struct om34cam_memfree {
    unsigned int mhandle;            // IN: Handle of memory to free
};

	// Structure to return the MMU ISP Address that's used by differnt HW blocks
struct om34cam_memispaddr {
    struct v4l2_buffer *v4l2buf;	// IN: V4L@ buffer structure
    unsigned int       ispaddr;		// OUT: ISP MMU address
};


	// Defines and structure for setting/getting 
#define ISPBUSGET       0x00000000
#define ISPBUSSET       0x00000001


#define ISPBUS_MIPI	0x00001001	// MIPI only output (SOC or Bayer to memory) (default)
#define ISPBUS_MIPI_VP	0x00001002	// MIPI + Video Port (CCDC)
#define ISPBUS_PARA_VP	0x00001003	// Parallel + Video Port (CCDC)


struct isp_bus_selection {
    int   bset;				// IN: Flag, set or return bus value
    int   busvalue;			// IN/OUT: bus value to set, current bus value
};


////////////////////////// ioctl cmds

#define VPFE_CMD_FASTGRAB \
    _IOWR('V',BASE_VIDIOC_PRIVATE+ 1,struct fastgrab_struct_kernel)
#define VPFE_CMD_CONFIG_BLACKLEVEL \
	_IOW('V',BASE_VIDIOC_PRIVATE + 2,struct black_compensation)
#define VPFE_CMD_S_DEVICE_FORMAT \
	_IOW('V',BASE_VIDIOC_PRIVATE + 3,struct device_format_struct)

#define VPFE_CMD_OPEN_I2C_DEV \
    _IOWR ('V', BASE_VIDIOC_PRIVATE + 4, struct aptcam_i2c_params)
#define VPFE_CMD_CLOSE_I2C_DEV \
    _IOWR ('V', BASE_VIDIOC_PRIVATE + 5, struct aptcam_i2c_params)
#define VPFE_CMD_REG_DIRECT \
    _IOWR  ('V', BASE_VIDIOC_PRIVATE + 6, struct my_i2c_data)
#define VPFE_CMD_REG_PARAM \
    _IOWR  ('V', BASE_VIDIOC_PRIVATE + 7, struct my_i2c_param)

#define VPFE_CMD_MEM_ALLOC_PARAM  \
    _IOWR  ('V', BASE_VIDIOC_PRIVATE + 1000, struct om34cam_memalloc)
#define VPFE_CMD_MEM_FREE_PARAM  \
    _IOWR  ('V', BASE_VIDIOC_PRIVATE + 1001, struct om34cam_memfree)
#define VPFE_CMD_MEM_FREEALL_PARAM  \
    _IOWR  ('V', BASE_VIDIOC_PRIVATE + 1002, struct om34cam_memfree)
#define VPFE_CMD_MEM_RET_ISPADDR  \
    _IOWR  ('V', BASE_VIDIOC_PRIVATE + 1003, struct om34cam_memispaddr)

#define VIDIOC_PRIVATE_ISP_BUS_SELECT \
    _IOWR  ('V', BASE_VIDIOC_PRIVATE + 1050, struct isp_bus_selection)

#endif

