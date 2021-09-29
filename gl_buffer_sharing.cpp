/*
 * Copyright © 2012 Intel Corporation
 * Copyright (C) 2021  Jacek Danecki <jacek.danecki@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include "CL/cl.h"
#include "CL/cl_ext.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <CL/cl_gl.h>
#include <unistd.h>

const size_t w = 256;
const size_t h = 256;
GLuint tex;
EGLDisplay eglDisplay;
EGLContext eglContext;
EGLSurface eglSurface;
Display* xDisplay;
Window xWindow;
cl_mem buf = {};
size_t globals[3];
size_t locals[3];
cl_kernel kernel = NULL;
cl_command_queue queue = NULL;
cl_platform_id platform = NULL;
cl_device_id device = NULL;
cl_context ctx = NULL;
cl_program program = NULL;

const char* src = "\
__kernel void \
fill_gl_image(write_only image2d_t img) \
{\
  int2 coord;\
  float4 color_v4;\
  int lgid_x = get_group_id(0);\
  int lgid_y = get_group_id(1);\
  int num_groups_x = get_num_groups(0);\
  int num_groups_y = get_num_groups(1);\
  coord.x = get_global_id(0);\
  coord.y = get_global_id(1);\
  color_v4 = (float4)( lgid_x/(float)num_groups_x, lgid_y/(float)num_groups_y, 1.0, 1.0);\
  write_imagef(img, coord, color_v4);\
}\
";

void draw()
{
    XEvent event;
    int shown = 0;
    float vertices[8] = {-1, 1, 1, 1, 1, -1, -1, -1};
    float tex_coords[8] = {0, 0, 1, 0, 1, 1, 0, 1};

    for (;;)
    {
        XNextEvent(xDisplay, &event);

        if (event.type == Expose && !shown)
        {
            unsigned int tex_data[w * h];
            unsigned int ptr[w * h];
            memset(tex_data, 0, w * h * 4);
            memset(ptr, 0, w * h * 4);

            glClearColor(0.0, 1.0, 1.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);

            clSetKernelArg(kernel, 0, sizeof(cl_mem), &buf);
            globals[0] = w;
            globals[1] = h;
            locals[0] = 16;
            locals[1] = 16;
            glFinish();
            clEnqueueAcquireGLObjects(queue, 1, &buf, 0, 0, 0);
            clEnqueueNDRangeKernel(queue, kernel, 2, NULL, globals, locals, 0, NULL, NULL);

            size_t origin[3] = {0, 0, 0};
            size_t region[3] = {10, 2, 1};
            clEnqueueReadImage(queue, buf, CL_TRUE, origin, region, 1024, 0, ptr, 0, NULL, NULL);
            clEnqueueReleaseGLObjects(queue, 1, &buf, 0, 0, 0);
            clFinish(queue);

            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, tex_data);
            printf("data[0]: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", tex_data[0], tex_data[1], tex_data[2], tex_data[3], tex_data[4],
                   tex_data[5], tex_data[6], tex_data[7], tex_data[8], tex_data[9], tex_data[10], tex_data[11], tex_data[12], tex_data[13], tex_data[14],
                   tex_data[15], tex_data[16], tex_data[17], tex_data[18], tex_data[19]);

            /*printf("float[0]: %f %f %f %f\n",
             *((float*)(&ptr[0])),
             *((float*)(&ptr[1])),
             *((float*)(&ptr[2])),
             *((float*)(&ptr[3])));
             */
            printf("data[1]: %x %x %x %x %x %x %x %x %x %x\n", tex_data[w], tex_data[w + 1], tex_data[w + 2], tex_data[w + 3], tex_data[w + 4], tex_data[w + 5],
                   tex_data[w + 6], tex_data[w + 7], tex_data[w + 8], tex_data[w + 9]);

            printf("ptr[0]: %x %x %x %x %x %x %x %x %x %x\n", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9]);
            printf("ptr[1]: %x %x %x %x %x %x %x %x %x %x\n", ptr[w], ptr[w + 1], ptr[w + 2], ptr[w + 3], ptr[w + 4], ptr[w + 5], ptr[w + 6], ptr[w + 7],
                   ptr[w + 8], ptr[w + 9]);

            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glEnable(GL_TEXTURE_2D);
            glDisable(GL_BLEND);
            glVertexPointer(2, GL_FLOAT, sizeof(float) * 2, vertices);
            glEnableClientState(GL_VERTEX_ARRAY);
            glClientActiveTexture(GL_TEXTURE0);
            glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 2, tex_coords);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glFinish();
            eglSwapBuffers(eglDisplay, eglSurface);
            shown++;
        }
        if (event.type == KeyPress) break;
    }
}

bool init_egl_window()
{
    XSetWindowAttributes swa;
    Window win, root;
    EGLint attr[] = {EGL_BUFFER_SIZE, 16, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
    EGLint ctxattr[] = {EGL_NONE};

    EGLConfig ecfg;
    EGLint numConfig;

    eglContext = EGL_NO_CONTEXT;
    xDisplay = XOpenDisplay(NULL);
    if (xDisplay == NULL)
    {
        fprintf(stderr, "Failed to open DISPLAY.\n");
        return false;
    }
    root = DefaultRootWindow(xDisplay);
    swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;

    win = XCreateWindow(xDisplay, root, 0, 0, w, h, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &swa);
    xWindow = win;

    eglDisplay = eglGetDisplay((EGLNativeDisplayType)xDisplay);
    if (eglDisplay == EGL_NO_DISPLAY)
    {
        fprintf(stderr, "Got no EGL display.\n");
        return false;
    }
    eglBindAPI(EGL_OPENGL_API);
    int m, n;
    if (!eglInitialize(eglDisplay, &m, &n))
    {
        fprintf(stderr, "Unable to initialize EGL\n");
        return false;
    }
    if (!eglChooseConfig(eglDisplay, attr, &ecfg, 1, &numConfig))
    {
        fprintf(stderr, "Failed to choose config (eglError: %d)\n", eglGetError());
        return false;
    }
    if (numConfig != 1)
    {
        fprintf(stderr, "Didn't get exactly one config, but %d", numConfig);
        return false;
    }
    eglSurface = eglCreateWindowSurface(eglDisplay, ecfg, win, NULL);
    if (eglSurface == EGL_NO_SURFACE)
    {
        fprintf(stderr, "Unable to create EGL surface (eglError: %d)\n", eglGetError());
        return false;
    }
    eglContext = eglCreateContext(eglDisplay, ecfg, EGL_NO_CONTEXT, ctxattr);
    if (eglContext == EGL_NO_CONTEXT)
    {
        fprintf(stderr, "Unable to create EGL context (eglError: %d)\n", eglGetError());
        exit(1);
    }
    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    eglSwapBuffers(eglDisplay, eglSurface);
    return true;
}

int cl_ocl_init(void)
{
    cl_int status = CL_SUCCESS;
    cl_uint platform_n;
    cl_context_properties* props = NULL;
    const size_t sz = strlen(src);

    clGetPlatformIDs(0, NULL, &platform_n);
    if (!platform_n)
    {
        fprintf(stderr, "can't find any platform\n");
    }

    clGetPlatformIDs(1, &platform, &platform_n);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

    int i = 0;
    props = new cl_context_properties[7];
    props[i++] = CL_CONTEXT_PLATFORM;
    props[i++] = (cl_context_properties)platform;
    if (init_egl_window())
    {
        props[i++] = CL_EGL_DISPLAY_KHR;
        props[i++] = (cl_context_properties)eglGetCurrentDisplay();
        props[i++] = CL_GL_CONTEXT_KHR;
        props[i++] = (cl_context_properties)eglGetCurrentContext();
    }
    props[i++] = 0;

    ctx = clCreateContext(props, 1, &device, NULL, NULL, &status);
    if (status != CL_SUCCESS)
    {
        fprintf(stderr, "error calling clCreateContext %d\n", status);
        goto error;
    }

    queue = clCreateCommandQueue(ctx, device, 0, &status);
    if (status != CL_SUCCESS)
    {
        fprintf(stderr, "error calling clCreateCommandQueue\n");
        goto error;
    }

    program = clCreateProgramWithSource(ctx, 1, &src, &sz, &status);

    if (status != CL_SUCCESS)
    {
        fprintf(stderr, "error calling clCreateProgramWithSource\n");
        exit(1);
    }

    clBuildProgram(program, 1, &device, NULL, NULL, NULL);

    kernel = clCreateKernel(program, "fill_gl_image", &status);
    if (status != CL_SUCCESS)
    {
        fprintf(stderr, "error calling clCreateKernel\n");
        exit(1);
    }

error:
    if (props) delete[] props;
    return status;
}

int main(int argc, char* argv[])
{
    unsigned int init_data[w * h];
    for (int i = 0; i < 128; i++) init_data[i] = 0x11223344;
    for (int i = 0; i < 128; i++) init_data[256 + i] = 0x55667788;

    unsigned int tex_data[w * h];
    memset(tex_data, 0, w * h * 4);

    cl_int status = CL_SUCCESS;
    if (cl_ocl_init() != CL_SUCCESS) exit(1);

    XMapWindow(xDisplay, xWindow);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, init_data);
    printf("init_data[0]: %x %x %x %x %x\n", init_data[0], init_data[1], init_data[2], init_data[3], init_data[4]);
    printf("init_data[1]: %x %x %x %x %x\n", init_data[w], init_data[w + 1], init_data[w + 2], init_data[w + 3], init_data[w + 4]);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, tex_data);
    printf("tex_data[0]: %x %x %x %x %x\n", tex_data[0], tex_data[1], tex_data[2], tex_data[3], tex_data[4]);
    printf("tex_data[1]: %x %x %x %x %x\n", tex_data[w], tex_data[w + 1], tex_data[w + 2], tex_data[w + 3], tex_data[w + 4]);

    unsigned int target = GL_TEXTURE_2D;
    int miplevel = 0;
    GLint param_value;
    glGetTexLevelParameteriv(target, miplevel, GL_TEXTURE_WIDTH, &param_value);
    printf("width = %d\n", param_value);
    glGetTexLevelParameteriv(target, miplevel, GL_TEXTURE_HEIGHT, &param_value);
    printf("height= %d\n", param_value);
    glGetTexLevelParameteriv(target, miplevel, GL_TEXTURE_DEPTH, &param_value);
    printf("depth = %d\n", param_value);
    glGetTexLevelParameteriv(target, miplevel, GL_TEXTURE_INTERNAL_FORMAT, &param_value);
    printf("gl_format = %x\n", param_value);
    //#define GL_RGBA					0x1908
    // cl_format->image_channel_order = CL_RGBA;
    // cl_format->image_channel_data_type = CL_UNORM_INT8;
    // case GL_TEXTURE_2D: *type = CL_MEM_OBJECT_IMAGE2D; break;

    // uint32_t tiling_mode, swizzle_mode;
    // drm_intel_bo_get_tiling(intel_bo, &tiling_mode, &swizzle_mode);

    buf = clCreateFromGLTexture(ctx, 0, GL_TEXTURE_2D, 0, tex, &status);

    // image type                     size of buffer
    // CL_MEM_OBJECT_IMAGE2D >= image_row_pitch * image_height

    size_t value;
    unsigned int value1;

    clGetImageInfo(buf, CL_IMAGE_WIDTH, sizeof(size_t), &value, NULL);
    printf("CL_IMAGE_WIDTH=%lu\n", value);

    clGetImageInfo(buf, CL_IMAGE_HEIGHT, sizeof(size_t), &value, NULL);
    printf("CL_IMAGE_HEIGHT=%lu\n", value);

    clGetImageInfo(buf, CL_IMAGE_DEPTH, sizeof(size_t), &value, NULL);
    printf("CL_IMAGE_DEPTH=%lu\n", value);

    clGetImageInfo(buf, CL_IMAGE_ARRAY_SIZE, sizeof(size_t), &value, NULL);
    printf("CL_IMAGE_ARRAY_SIZE=%lu\n", value);

    clGetImageInfo(buf, CL_IMAGE_NUM_MIP_LEVELS, sizeof(cl_uint), &value1, NULL);
    printf("CL_IMAGE_NUM_MIP_LEVELS=%u\n", value1);

    clGetImageInfo(buf, CL_IMAGE_NUM_SAMPLES, sizeof(cl_uint), &value1, NULL);
    printf("CL_IMAGE_NUM_SAMPLES=%u\n", value1);

    clGetImageInfo(buf, CL_IMAGE_ELEMENT_SIZE, sizeof(size_t), &value, NULL);
    printf("CL_IMAGE_ELEMENT_SIZE=%lu\n", value);

    clGetImageInfo(buf, CL_IMAGE_ROW_PITCH, sizeof(size_t), &value, NULL);
    printf("CL_IMAGE_ROW_PITCH=%lu\n", value);

    clGetImageInfo(buf, CL_IMAGE_SLICE_PITCH, sizeof(size_t), &value, NULL);
    printf("CL_IMAGE_SLICE_PITCH=%lu\n", value);

    cl_image_format format;
    clGetImageInfo(buf, CL_IMAGE_FORMAT, sizeof(cl_image_format), &format, NULL);
    printf("channel_order==%x channel_data_type=%x\n", format.image_channel_order, format.image_channel_data_type);
    //#define CL_RGBA                                     0x10B5
    //#define CL_UNORM_INT8                               0x10D2

#if 0
//clGetGLObjectInfo and clGetGLTextureInfo crashes on Beignet, and works with Neo
//
    cl_gl_object_type objectType = 0u;
    cl_GLuint objectId = 0u;
    clGetGLObjectInfo(buf, &objectType, &objectId);

    printf("objectType=%x tex=%x id=%x\n", objectType, tex, objectId);
//#define CL_GL_OBJECT_TEXTURE2D                  0x2001

    GLenum textureTarget = 0u;
    size_t retSize = 0;
    GLint mipLevel = 0u;

    clGetGLTextureInfo(buf, CL_GL_TEXTURE_TARGET, sizeof(GLenum), &textureTarget, &retSize);
    printf("textureTarget=%x\n", textureTarget);
//#define GL_TEXTURE_2D				0x0DE1
    printf("sizeof(GLenum)=%lu retSize=%lu\n", sizeof(GLenum), retSize);

    clGetGLTextureInfo(buf, CL_GL_MIPMAP_LEVEL, sizeof(GLenum), &mipLevel, &retSize);
    printf("mipLevel=%d sizeof(GLenum)=%lu retSize=%lu\n", mipLevel, sizeof(GLenum), retSize);
#endif
    draw();

    clReleaseMemObject(buf);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);

    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDisplay, eglContext);
    eglDestroySurface(eglDisplay, eglSurface);
    XDestroyWindow(xDisplay, xWindow);
    XCloseDisplay(xDisplay);
}
