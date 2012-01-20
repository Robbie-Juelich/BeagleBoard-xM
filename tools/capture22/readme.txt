

	3/17/11
	Simple V4L2 test application, derrived from V4L2 document/article.

	To compile: 1) Add compiler location to makefile
		2) 'make clean' to clean
		3) 'make' to compile

	To run:  type './capture22'  (no command line parameters)

	Currently captures 10 images, saves the last one to file.
	Image size: 640x480, V4L2_PIX_FORMAT_YUYV
	
	Top page of source code contains all the interesting parameters (format,
	resolution, video device number, etc).

	Probably do not want to use the videodev2.h file included in the release...