This repository contains programs to test CL/GL sharing.

- gl_buffer_sharing
    - is simplified example taken from [Beignet project](https://github.com/intel/beignet/blob/master/examples/gl_buffer_sharing/gl_buffer_sharing.cpp)
    - it uses cl/gl sharing to display EGL texture modified by OCL kernel
- sharing
    - check whether EGL extension [MESA_image_dma_buf_export](https://www.khronos.org/registry/EGL/extensions/MESA/EGL_MESA_image_dma_buf_export.txt) can
      be used to read EGL texture using libdrm interface
