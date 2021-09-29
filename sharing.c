/*
    Copyright (C) 2021  Jacek Danecki <jacek.danecki@intel.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <stdio.h>
#include <libdrm/intel_bufmgr.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

//#define SHOW_WINDOW

int error = 1;

void check_error(int cond, int line)
{
    if (cond)
    {
        printf("error: %d in line: %d\n", error, line);
        exit(error);
    }
    error += 1;
}

#define check(cond) check_error(cond, __LINE__)

int main()
{
    PFNEGLEXPORTDMABUFIMAGEMESAPROC fun = (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");
    check(!fun);

    Display* xDisplay = XOpenDisplay(NULL);
    check(!xDisplay);

    XSetWindowAttributes swa;
    Window win, root;
    int w = 256;
    int h = 256;

    root = DefaultRootWindow(xDisplay);
    swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;

    win = XCreateWindow(xDisplay, root, 0, 0, w, h, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &swa);
    Window xWindow = win;

    EGLDisplay eglDisplay = eglGetDisplay((EGLNativeDisplayType)xDisplay);
    check(eglDisplay == EGL_NO_DISPLAY);

    EGLint ctxattr[] = {EGL_NONE};
    EGLint attr[] = {EGL_BUFFER_SIZE, 16, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
    EGLint numConfig;
    EGLConfig ecfg;

    eglBindAPI(EGL_OPENGL_API);
    int m, n;
    check(!eglInitialize(eglDisplay, &m, &n));
    check(!eglChooseConfig(eglDisplay, attr, &ecfg, 1, &numConfig));
    check(numConfig != 1);

    EGLSurface eglSurface = eglCreateWindowSurface(eglDisplay, ecfg, win, NULL);
    check(eglSurface == EGL_NO_SURFACE);

    EGLContext eglContext = eglCreateContext(eglDisplay, ecfg, EGL_NO_CONTEXT, ctxattr);
    check(eglContext == EGL_NO_CONTEXT);

    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    eglSwapBuffers(eglDisplay, eglSurface);

#ifdef SHOW_WINDOW
    XMapWindow(xDisplay, xWindow);
#endif
    int fd, stride, offset;
    int miplevel = 0;
    EGLenum e_target = EGL_GL_TEXTURE_2D;
    unsigned int data[w * h];
    for (int i = 0; i < 0xffff; i++) data[i] = 0x4488bbff;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, data);

    EGLAttrib attrib_list[] = {EGL_GL_TEXTURE_LEVEL, miplevel, EGL_NONE};
    EGLImage image = eglCreateImage(eglGetCurrentDisplay(), eglGetCurrentContext(), e_target, (EGLClientBuffer)tex, &attrib_list[0]);
    check(image == EGL_NO_IMAGE);

    glFinish();
    eglSwapBuffers(eglDisplay, eglSurface);

#ifdef SHOW_WINDOW
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
        if (event.type == KeyPress)
        {
            printf("key=%d\n", event.xkey.keycode);
            if (event.xkey.keycode == 9) break;
        }
    }
#endif
    check(fun(eglDisplay, image, &fd, &stride, &offset) != EGL_TRUE);

    int drm_fd = open("/dev/dri/renderD128", O_RDWR);
    check(drm_fd == -1);

    dri_bufmgr* bufmgr = drm_intel_bufmgr_gem_init(drm_fd, 0x4000);
    check(!bufmgr);

    drm_intel_bo* bo = drm_intel_bo_gem_create_from_prime(bufmgr, fd, 0);
    check(!bo);

    drm_intel_bo_map(bo, 1);
    check(*((unsigned int*)bo->virtual) != 0xffbb8844);
    drm_intel_bo_unmap(bo);

    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDisplay, eglContext);
    eglDestroySurface(eglDisplay, eglSurface);
    XDestroyWindow(xDisplay, xWindow);
    XCloseDisplay(xDisplay);

    printf("test passed\n");
    return 0;
}
