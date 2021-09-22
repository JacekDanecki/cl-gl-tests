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
cl_mem buf[16] = {};
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
\
  coord.x = get_global_id(0);\
  coord.y = get_global_id(1);\
  color_v4 = (float4)( lgid_x/(float)num_groups_x, lgid_y/(float)num_groups_y, 1.0, 1.0);\
  write_imagef(img, coord, color_v4);\
}\
";

void draw()
{
    XEvent event;

    float vertices[8] = {-1, 1, 1, 1, 1, -1, -1, -1};
    float tex_coords[8] = {0, 0, 1, 0, 1, 1, 0, 1};

    for (;;)
    {
        XNextEvent(xDisplay, &event);

        if (event.type == Expose)
        {
            glClearColor(0.0, 1.0, 1.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);

            clSetKernelArg(kernel, 0, sizeof(cl_mem), &buf[0]);
            globals[0] = w;
            globals[1] = h;
            locals[0] = 16;
            locals[1] = 16;
            glFinish();
            clEnqueueAcquireGLObjects(queue, 1, &buf[0], 0, 0, 0);
            clEnqueueNDRangeKernel(queue, kernel, 2, NULL, globals, locals, 0, NULL, NULL);
            clEnqueueReleaseGLObjects(queue, 1, &buf[0], 0, 0, 0);
            clFinish(queue);

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
    cl_int status = CL_SUCCESS;
    if (cl_ocl_init() != CL_SUCCESS) exit(1);

    XMapWindow(xDisplay, xWindow);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);

    buf[0] = clCreateFromGLTexture(ctx, 0, GL_TEXTURE_2D, 0, tex, &status);

    draw();

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
