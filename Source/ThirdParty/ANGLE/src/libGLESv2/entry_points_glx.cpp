//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// entry_points_glx.cpp: Implements the exported GLX functions.

#include <X11/Xlib.h>
#include <X11/Xutil.h>

using GLXPixmap   = XID;
using GLXDrawable = XID;
using GLXPbuffer  = XID;
using GLXContext  = XID;
#include "entry_points_glx.h"

#include "common/debug.h"

extern "C" {

const char *GL_APIENTRY glXQueryServerString(Display *dpy, int name)
{
    UNIMPLEMENTED();
    return nullptr;
}

GLXFBConfig *GL_APIENTRY glXChooseFBConfig(Display *dpy,
                                           int screen,
                                           const int *attrib_list,
                                           int *nelements)
{
    UNIMPLEMENTED();
    return nullptr;
}

XVisualInfo *GL_APIENTRY glXChooseVisual(Display *dpy, int screen, int *attrib_list)
{
    UNIMPLEMENTED();
    return nullptr;
}

void GL_APIENTRY glXCopyContext(Display *dpy, GLXContext src, GLXContext dst, unsigned long mask)
{
    UNIMPLEMENTED();
}

GLXContext GL_APIENTRY glXCreateContext(Display *dpy,
                                        XVisualInfo *vis,
                                        GLXContext shareList,
                                        bool direct)
{
    UNIMPLEMENTED();
    return GLXContext{};
}

GLXPixmap GL_APIENTRY glXCreateGLXPixmap(Display *dpy, XVisualInfo *vis, Pixmap pixmap)
{
    UNIMPLEMENTED();
    return GLXPixmap{};
}

GLXContext GL_APIENTRY glXCreateNewContext(Display *dpy,
                                           GLXFBConfig config,
                                           int render_type,
                                           GLXContext share_list,
                                           bool direct)
{
    UNIMPLEMENTED();
    return GLXContext{};
}

GLXPbuffer GL_APIENTRY glXCreatePbuffer(Display *dpy, GLXFBConfig config, const int *attrib_list)
{
    UNIMPLEMENTED();
    return GLXPbuffer{};
}

GLXPixmap GL_APIENTRY glXCreatePixmap(Display *dpy,
                                      GLXFBConfig config,
                                      Pixmap pixmap,
                                      const int *attrib_list)
{
    UNIMPLEMENTED();
    return GLXPixmap{};
}

GLXWindow GL_APIENTRY glXCreateWindow(Display *dpy,
                                      GLXFBConfig config,
                                      Window win,
                                      const int *attrib_list)
{
    UNIMPLEMENTED();
    return GLXWindow{};
}

void GL_APIENTRY glXDestroyContext(Display *dpy, GLXContext ctx)
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXDestroyGLXPixmap(Display *dpy, GLXPixmap pix)
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuf)
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXDestroyPixmap(Display *dpy, GLXPixmap pixmap)
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXDestroyWindow(Display *dpy, GLXWindow win)
{
    UNIMPLEMENTED();
}

const char *GL_APIENTRY glXGetClientString(Display *dpy, int name)
{
    UNIMPLEMENTED();
    return nullptr;
}

int GL_APIENTRY glXGetConfig(Display *dpy, XVisualInfo *vis, int attrib, int *value)
{
    UNIMPLEMENTED();
    return 1;
}

GLXContext GL_APIENTRY glXGetCurrentContext()
{
    UNIMPLEMENTED();
    return GLXContext{};
}

Display *GL_APIENTRY glXGetCurrentDisplay()
{
    UNIMPLEMENTED();
    return nullptr;
}

GLXDrawable GL_APIENTRY glXGetCurrentDrawable()
{
    UNIMPLEMENTED();
    return GLXDrawable{};
}

GLXDrawable GL_APIENTRY glXGetCurrentReadDrawable()
{
    UNIMPLEMENTED();
    return GLXDrawable{};
}

int GL_APIENTRY glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value)
{
    UNIMPLEMENTED();
    return 1;
}

GLXFBConfig *GL_APIENTRY glXGetFBConfigs(Display *dpy, int screen, int *nelements)
{
    UNIMPLEMENTED();
    return nullptr;
}

__GLXextFuncPtr GL_APIENTRY glXGetProcAddress(const GLubyte *procName)
{
    UNIMPLEMENTED();
    return nullptr;
}

void GL_APIENTRY glXGetSelectedEvent(Display *dpy, GLXDrawable draw, unsigned long *event_mask)
{
    UNIMPLEMENTED();
}

XVisualInfo *GL_APIENTRY glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config)
{
    UNIMPLEMENTED();
    return nullptr;
}

bool GL_APIENTRY glXIsDirect(Display *dpy, GLXContext ctx)
{
    UNIMPLEMENTED();
    return false;
}

bool GL_APIENTRY glXMakeContextCurrent(Display *dpy,
                                       GLXDrawable draw,
                                       GLXDrawable read,
                                       GLXContext ctx)
{
    UNIMPLEMENTED();
    return false;
}

bool GL_APIENTRY glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
    UNIMPLEMENTED();
    return false;
}

int GL_APIENTRY glXQueryContext(Display *dpy, GLXContext ctx, int attribute, int *value)
{
    UNIMPLEMENTED();
    return 1;
}

int GL_APIENTRY glXQueryDrawable(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value)
{
    UNIMPLEMENTED();
    return 1;
}

bool GL_APIENTRY glXQueryExtension(Display *dpy, int *errorBase, int *eventBase)
{
    UNIMPLEMENTED();
    return 1;
}

const char *GL_APIENTRY glXQueryExtensionsString(Display *dpy, int screen)
{
    UNIMPLEMENTED();
    return nullptr;
}

bool GL_APIENTRY glXQueryVersion(Display *dpy, int *major, int *minor)
{
    UNIMPLEMENTED();
    return false;
}

void GL_APIENTRY glXSelectEvent(Display *dpy, GLXDrawable draw, unsigned long event_mask)
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXUseXFont(Font font, int first, int count, int listBase)
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXWaitGL()
{
    UNIMPLEMENTED();
}

void GL_APIENTRY glXWaitX()
{
    UNIMPLEMENTED();
}

}  // extern "C"
