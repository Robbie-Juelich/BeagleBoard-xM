//**************************************************************************
// Copyright 2007 Micron Technology, Inc. All rights reserved.
//
// No permission to use, copy, modify, or distribute this software and/or
// its documentation for any purpose has been granted by Micron Technology, Inc.
// If any such permission has been granted ( by separate agreement ), it
// is required that the above copyright notice appear in all copies and
// that both that copyright notice and this permission notice appear in
// supporting documentation, and that the name of Micron Technology, Inc. or any
// of its trademarks may not be used in advertising or publicity pertaining
// to distribution of the software without specific, written prior permission.
//
//  This software and any associated documentation are provided AS IS and 
//  without warranty of any kind.   MICRON TECHNOLOGY, INC. EXPRESSLY DISCLAIMS 
//  ALL WARRANTIES EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO, NONINFRINGEMENT 
//  OF THIRD PARTY RIGHTS, AND ANY IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS 
//  FOR A PARTICULAR PURPOSE.  MICRON DOES NOT WARRANT THAT THE FUNCTIONS CONTAINED 
//  IN THIS SOFTWARE WILL MEET YOUR REQUIREMENTS, OR THAT THE OPERATION OF THIS SOFTWARE 
//  WILL BE UNINTERRUPTED OR ERROR-FREE.  FURTHERMORE, MICRON DOES NOT WARRANT OR 
//  MAKE ANY REPRESENTATIONS REGARDING THE USE OR THE RESULTS OF THE USE OF ANY 
//  ACCOMPANYING DOCUMENTATION IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY, 
//  OR OTHERWISE.  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h> /* for videodev2.h */
#include <linux/videodev2.h>  //FIX THIS!
#include <linux/types.h>
#include </home/abhishek/Desktop/beagle-data/beagleboard-validation-linux/trunk/kernel-2.6.32/arch/arm/plat-omap/include/plat/isp_user.h> 

//#include "videodev2.h"


#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef enum {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
    IO_METHOD_SETEXPOSURE,
    IO_METHOD_SETGAIN,
} io_method;

struct buffer {
    void * start;
    size_t length;
};
//The structure to set the gain or exposure


#define SAVECAPFRAME		// Save the last captured frame (comment out for no saving)

#define CAPCOUNT 10		// Number of frames to capture


    // Define the size of the image to capture
#define TARGETWIDTH      640 // 1280  // 1640  // 2592  // 2048  // 1296 // 1024
#define TARGETHEIGHT     480   // 1944  // 486   // 768   //1536

#define SIZEOFIMAGE TARGETWIDTH * TARGETHEIGHT * 2 // Should be valid for all captured types 
					   // (YUV, RGB, BAYER10). For Bayer8, remove the * 2


//        V4L2_PIX_FMT_YUYV; //V4L2_PIX_FMT_RGB565X; //V4L2_PIX_FMT_UYVY;  //V4L2_PIX_FMT_SGRBG10
//        V4L2_PIX_FMT_SBGGR8; 
#define PIXELFMT    V4L2_PIX_FMT_YUYV   //V4L2_PIX_FMT_RGB565X; //V4L2_PIX_FMT_SGRBG10
//#define PIXELFMT    V4L2_PIX_FMT_SGRBG10     //V4L2_PIX_FMT_RGB565X; //V4L2_PIX_FMT_SGRBG10
//#define PIXELFMT   V4L2_PIX_FMT_JPEG


#define SIZEOFBUFFER 2048*2048*3


int  bok = 0;			// Flag, something was captured
char save_buf[SIZEOFBUFFER];


#define VDEVICENAME "/dev/video0"		// Video 4 for Apache, 0 for SDP

#define CAPFILENAME "./capture_image.raw"

static char * dev_name = NULL;
static __u16 gain;
static __u32 exposure;
//static io_method io = IO_METHOD_READ;
static io_method io = IO_METHOD_MMAP;
int fd = -1;
struct buffer * buffers = NULL;
static unsigned int n_buffers = 0;
static unsigned int n_actualbuffers = 0;

//***********************************************************************************
static void errno_exit (const char * s)
{
    fprintf (stderr, "%s error %d, %s\n",
    s, errno, strerror (errno));
    exit (EXIT_FAILURE);
}

//***********************************************************************************
int xioctl (int fd,int request,void * arg)
{
int r;

    if((request==VIDIOC_DQBUF) || (request==VIDIOC_QBUF))
        r = ioctl (fd, request, arg);
    else{
	do 
	    r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
	return r;  //return r;   //don't be so strict.  Otherwise it can't 
                   // coexist with xawtv application
    }

    return 0;

}

//***********************************************************************************
static void process_image (const void * p)
{
unsigned int i;
unsigned char *pC, *pDest;


    // Write something to the screen
    fputc ('.', stdout);
    fflush (stdout);

    // Copy the image to a local buffer
    pC = (unsigned char *)buffers[0].start;
    pDest = (unsigned char *)save_buf;

    for (i=0; i < SIZEOFIMAGE; i++) {
	*pDest++ = *pC++;
    }
    bok = 1;

    return;
}

//***********************************************************************************
int read_frame (int bprocess)
{
struct v4l2_buffer buf;
unsigned int i;
int lret=0;
struct timeval start_time,end_time;
struct timezone lzone;
static buf_idx=0;   //csu:new
//FILE *fd_lock=NULL;
unsigned int lock_flag;

    switch (io) {
        case IO_METHOD_READ:
            gettimeofday(&start_time,&lzone);
            lret= read (fd, buffers[0].start, buffers[0].length);
            gettimeofday(&end_time,&lzone);
            printf("Time diffrence in Microseconds is %d",start_time.tv_usec-end_time.tv_usec);
            if (-1 == lret) {
                switch (errno) {
                    case EAGAIN:
			printf("read_frame: EAGAIN error, return 0\n");
                        return 0;
                    case EIO:

                            /* Could ignore EIO, see spec. */
                            /* fall through */
                    default:
                        errno_exit ("read");
                }
            }

            if (bprocess) {
                process_image (buffers[0].start);
            }
            break;
        
        case IO_METHOD_MMAP:
            CLEAR (buf);
	    //memset (&(buf), 1, sizeof (buf)); //csu:new
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

	    buf.index=buf_idx++;  //csu:new

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
			printf("read_frame: EAGAIN error, return 0\n");
                        return 0;
                    case EIO:
                        /* Could ignore EIO, see spec. */
                        /* fall through */

                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

	    if(buf_idx >= n_actualbuffers) buf_idx=0;
            assert (buf.index < n_buffers);

            if (bprocess) {
                process_image(buffers[buf.index].start);
            }
	    else {
	        // Write something to the screen
		fputc ('.', stdout);
    		fflush (stdout);
	    }

//	    pmb_length=buffers[buf.index].length;


            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;

        case IO_METHOD_USERPTR:
            CLEAR (buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
			printf("read_frame: EAGAIN error, return 0\n");
                        return 0;
                    case EIO:
                        /* Could ignore EIO, see spec. */
                        /* fall through */
                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            for (i = 0; i < n_buffers; ++i)
                if (buf.m.userptr == (unsigned long) buffers[i].start
                    && buf.length == buffers[i].length)
                    break;

            assert (i < n_buffers);

            if (bprocess) {
                process_image ((void *) buf.m.userptr);
            }

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");
            break;


    }

    return 1;
}


//***********************************************************************************
//static void mainloop (void)
//
//  Multi loop call
//
void mainloop (void)
{
unsigned int count = CAPCOUNT;
struct v4l2_control v4l2c;
unsigned int val = 0x0000000;
unsigned int btimeout = 0;


	while (count-- > 0) {

            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO (&fds);
            FD_SET (fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            r = select (fd + 1, &fds, NULL, NULL, &tv);
            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                errno_exit ("select");
		btimeout = 1;
            }
            if (0 == r) {
                fprintf (stderr, "select timeout\n");
//;jr;$* ORG: COMMENTOUT
//                exit (EXIT_FAILURE);		// Probably shouldn't exit here...
		btimeout = 1;
            }

	    if (count == 0) {
		if (btimeout == 0) {		// Save the fram only on the last call

#ifdef SAVECAPFRAME
		    read_frame(1);
#else
		    read_frame(0);
#endif
		}
		else {

#ifdef SAVECAPFRAME
//;jr;$*1/15		    read_frame(0);
		    read_frame(1);
#else
		    read_frame(0);
#endif
		}
            }
            else {
                 read_frame(0);
//;jr;$*0501                  read_frame(1);
            }
	}

	return;
}



//***********************************************************************************
static void stop_capturing (void)
{
enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
                errno_exit ("VIDIOC_STREAMOFF");
            break;
        }
}



//***********************************************************************************
static void start_capturing (void)
{
unsigned int i;
enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;
                CLEAR (buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;
                CLEAR (buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
                buf.m.userptr = (unsigned long) buffers[i].start;
                buf.length = buffers[i].length;
                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");

            break;
    }
}

//***********************************************************************************
static void uninit_device (void)
{
unsigned int i;

    switch (io) {
        case IO_METHOD_READ:
            free (buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap (buffers[i].start, buffers[i].length))
                    errno_exit ("munmap");

            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
                free (buffers[i].start);
            break;

    }

    free (buffers);
}

//***********************************************************************************
static void init_read (unsigned int buffer_size)
{
    buffers = calloc (1, sizeof (*buffers));
    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
    buffers[0].length = buffer_size;
    buffers[0].start = malloc (buffer_size);
    if (!buffers[0].start) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
}

//***********************************************************************************
static void init_mmap (void)
{
struct v4l2_requestbuffers req;

    CLEAR (req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support memory mapping\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf (stderr, "Insufficient buffer memory on %s, buffer count: %d\n", dev_name, req.count);
//;jr;$*    exit (EXIT_FAILURE);
    }

    n_actualbuffers = req.count;

    buffers = calloc (req.count, sizeof (*buffers));
    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;
        CLEAR (buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap (NULL /* start anywhere */,
                                            buf.length,
                                            PROT_READ | PROT_WRITE /* required */,
                                            MAP_SHARED /* recommended */,
                                            fd, buf.m.offset);
       if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit ("mmap");

    }

    return;
}


//***********************************************************************************
static void init_userp (unsigned int buffer_size)
{
struct v4l2_requestbuffers req;

    CLEAR (req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support user pointer i/o\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    buffers = calloc (4, sizeof (*buffers));
    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        buffers[n_buffers].start = malloc (buffer_size);
        if (!buffers[n_buffers].start) {
            fprintf (stderr, "Out of memory\n");
            exit (EXIT_FAILURE);
        }
    }

    return;
}

//***********************************************************************************
static void init_exposure(void)
{
  struct isph3a_aewb_data expo;
  expo.update = SET_EXPOSURE;
  expo.shutter = exposure;
  xioctl(fd, VIDIOC_PRIVATE_ISP_AEWB_REQ,  &expo);
  
}
//***********************************************************************************
static void init_gain(void)
{
  struct isph3a_aewb_data Gain;
  Gain.update = SET_ANALOG_GAIN;
  Gain.gain = gain;
  xioctl(fd, VIDIOC_PRIVATE_ISP_AEWB_REQ,  &Gain);
  
}
//***********************************************************************************
static void init_device (void)
{
struct v4l2_capability cap;
struct v4l2_cropcap cropcap;
struct v4l2_crop crop;
struct v4l2_format fmt;
unsigned int min;


    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s is no V4L2 device\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_QUERYCAP");
        }
    }
    else {
        printf ("Caps returns: 0x%x\n", cap.capabilities);
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf (stderr, "%s is no video capture device\n", dev_name);
        exit (EXIT_FAILURE);
    }

    switch (io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                fprintf (stderr, "%s does not support read i/o\n", dev_name);
                //exit (EXIT_FAILURE);
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                fprintf (stderr, "%s does not support streaming i/o\n", dev_name);
                exit (EXIT_FAILURE);
            }
            break;
        case IO_METHOD_SETEXPOSURE:
            puts("I am in expo");
            init_exposure();
            break;
        case IO_METHOD_SETGAIN :
            puts("I am in gain");
            init_gain();
            break;
    }

    /* Select video input, video standard and tune here. */
    CLEAR (cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                /* Cropping not supported. */
		fprintf(stderr, " Cropping not supported\n");
                break;

                default:
                /* Errors ignored. */
                break;
            }
        }
        } else {
            /* Errors ignored. */
        }
        CLEAR (fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = TARGETWIDTH;
        fmt.fmt.pix.height = TARGETHEIGHT;
        fmt.fmt.pix.pixelformat = PIXELFMT;     // defined at the top of the file

	printf("capture: size: W - %d  H - %d, format: 0x%x\n", 
                               fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);

        fmt.fmt.pix.field = V4L2_FIELD_NONE;

#if 1
        if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt)) {
            printf("xioctl(VIDIOC_S_FMT) failed--->It's doesn't matter. Continue...");
        }
        else {
            printf("VIDIOC_S_FMT returned success\n");
	    printf("    returned: pix.width: %d   pix.height: %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

        }


//;jr;$* exit for now
//		printf("EXIT APPLICATION for now......\n");
//	    exit(0);


#endif

        /* Note VIDIOC_S_FMT may change width and height. */
        /* Buggy driver paranoia. */
        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
            fmt.fmt.pix.bytesperline = min;

        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
            fmt.fmt.pix.sizeimage = min;
        
        switch (io) {
            case IO_METHOD_READ:
                init_read (fmt.fmt.pix.sizeimage);
                break;

            case IO_METHOD_MMAP:
                init_mmap ();
                break;

            case IO_METHOD_USERPTR:
                init_userp (fmt.fmt.pix.sizeimage);
                break;
           case IO_METHOD_SETEXPOSURE:
               break;
        }
}


//***********************************************************************************
static void close_device (void)
{
    if (-1 == close (fd))
        errno_exit ("close");

    fd = -1;
}

//***********************************************************************************
static void open_device (void)
{
struct stat st;

    if (-1 == stat (dev_name, &st)) {
        fprintf (stderr, "Cannot identify %s: %d, %s\n",
        dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }

    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
        exit (EXIT_FAILURE);
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
    if (-1 == fd) {
        fprintf (stderr, "Cannot open %s: %d, %s\n", dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
}


//***********************************************************************************
void saveimage(void)
{
FILE *hFile;

    if((hFile = fopen (CAPFILENAME, "wb")) == NULL) {
        printf("saveimage: unable to open the file\n");
        goto si_out;
    }

    if (fwrite(save_buf, sizeof(char), SIZEOFIMAGE, hFile) != SIZEOFIMAGE) {
        printf("saveimage: unable to open the file\n");
        goto si_out;
    }

    fclose (hFile);

si_out:
    return;
}


//***********************************************************************************
static void usage (FILE * fp, int argc, char ** argv)
{

    fprintf (fp, "Usage: %s [options]\n\n Options:\n"
        "-d | --device name Video device name [/dev/video]\n"
        "-h | --help Print this message\n"
        "-m | --mmap Use memory mapped buffers\n"
        "-r | --read Use read() calls\n"
        "-e | --exposure Set the exposure\n"
        "-g | --gain Set the Analog gain\n"
        "-u | --userp Use application allocated buffers\n", argv[0]);
}


static const char short_options [] = "d:hmrue:g:";

static const struct option

long_options [] = {
    { "device", required_argument, NULL, 'd' },
    { "help", no_argument, NULL, 'h' },
    { "mmap", no_argument, NULL, 'm' },
    { "read", no_argument, NULL, 'r' },
    { "userp", no_argument, NULL, 'u' },
    { "exposure", required_argument, NULL, 'e' },
    { "gain", required_argument, NULL, 'g'},
    { 0, 0, 0, 0 }
};


#if 1

//***********************************************************************************
int main (int argc, char ** argv)
{
int i;
    
    dev_name = VDEVICENAME;
    printf ("Opening device %s\n", dev_name);
    bok = 0;

    for (;;) {
        int index;
        int c;

        c = getopt_long (argc, argv, short_options, long_options, &index);
        printf("%d",c);
        if (-1 == c)
            break;

        switch (c) {
            case 0: /* getopt_long() flag */
                break;
    
            case 'd':
                dev_name = optarg;
                break;

            case 'h':
                usage (stdout, argc, argv);
                exit (EXIT_SUCCESS);

            case 'm':
                io = IO_METHOD_MMAP;
                break;

            case 'r':
                io = IO_METHOD_READ;
                break;

            case 'u':
                io = IO_METHOD_USERPTR;
                break;
            case 'e':
                io = IO_METHOD_SETEXPOSURE;
                exposure = atoi(optarg);
                break;
            case 'g':
                io = IO_METHOD_SETGAIN;
                gain = atoi(optarg);
                break;
            default:
                usage (stderr, argc, argv);
                exit (EXIT_FAILURE);
        }
    }

    open_device ();
    init_device ();
    start_capturing ();
    mainloop ();
    stop_capturing ();
    uninit_device ();
    close_device ();

    if (bok) {
        saveimage();
    }

    printf("\n");
    exit (EXIT_SUCCESS);
    return 0;
}

#else

//***********************************************************************************
int InitImage()
{
char buf[20];
static int fd1 = -1;
int i,j;


    io = IO_METHOD_READ;
    io = IO_METHOD_MMAP;
    dev_name = VDEVICENAME;

    open_device ();
    init_device ();

    start_capturing ();

    printf("InitImage success\n");
    return 0;
}


int ExitImage()
{
    stop_capturing();
    uninit_device();
    close_device();

    printf ("ExitImage success\n");
    return (0);
}

#endif
