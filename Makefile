all:
	g++ -g -DCL_TARGET_OPENCL_VERSION=120 -Wno-deprecated-declarations -o gl_buffer_sharing gl_buffer_sharing.cpp -lEGL -lOpenCL -lGL -lX11
	gcc -g -Wno-int-to-pointer-cast -o sharing sharing.c -lEGL -lX11 -ldrm_intel -lGL

