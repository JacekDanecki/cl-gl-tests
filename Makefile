all:
	gcc -o sharing sharing.c -lEGL
	g++ -g -DCL_TARGET_OPENCL_VERSION=120 -Wno-deprecated-declarations -o gl_buffer_sharing gl_buffer_sharing.cpp -lEGL -lOpenCL -lGL -lX11

