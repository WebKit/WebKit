//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// entry_points_glx.h: Declares the exported GLX functions.

#ifndef LIBGL_GLX_H_
#define LIBGL_GLX_H_

#include <export.h>
#include "angle_gl.h"

#include "GLX/glxext.h"

extern "C" {

ANGLE_EXPORT const char *GL_APIENTRY glXQueryServerString(Display *dpy, int name);

ANGLE_EXPORT GLXFBConfig *GL_APIENTRY glXChooseFBConfig(Display *dpy,
                                                        int screen,
                                                        const int *attrib_list,
                                                        int *nelements);

ANGLE_EXPORT XVisualInfo GL_APIENTRY *glXChooseVisual(Display *dpy, int screen, int *attrib_list);

ANGLE_EXPORT void GL_APIENTRY glXCopyContext(Display *dpy,
                                             GLXContext src,
                                             GLXContext dst,
                                             unsigned long mask);

ANGLE_EXPORT GLXContext GL_APIENTRY glXCreateContext(Display *dpy,
                                                     XVisualInfo *vis,
                                                     GLXContext shareList,
                                                     bool direct);

ANGLE_EXPORT GLXPixmap GL_APIENTRY glXCreateGLXPixmap(Display *dpy,
                                                      XVisualInfo *vis,
                                                      Pixmap pixmap);

ANGLE_EXPORT GLXContext GL_APIENTRY glXCreateNewContext(Display *dpy,
                                                        GLXFBConfig config,
                                                        int render_type,
                                                        GLXContext share_list,
                                                        bool direct);

ANGLE_EXPORT GLXPbuffer GL_APIENTRY glXCreatePbuffer(Display *dpy,
                                                     GLXFBConfig config,
                                                     const int *attrib_list);

ANGLE_EXPORT GLXPixmap GL_APIENTRY glXCreatePixmap(Display *dpy,
                                                   GLXFBConfig config,
                                                   Pixmap pixmap,
                                                   const int *attrib_list);

ANGLE_EXPORT GLXWindow GL_APIENTRY glXCreateWindow(Display *dpy,
                                                   GLXFBConfig config,
                                                   Window win,
                                                   const int *attrib_list);

ANGLE_EXPORT void GL_APIENTRY glXDestroyContext(Display *dpy, GLXContext ctx);

ANGLE_EXPORT void GL_APIENTRY glXDestroyGLXPixmap(Display *dpy, GLXPixmap pix);

ANGLE_EXPORT void GL_APIENTRY glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuf);

ANGLE_EXPORT void GL_APIENTRY glXDestroyPixmap(Display *dpy, GLXPixmap pixmap);

ANGLE_EXPORT void GL_APIENTRY glXDestroyWindow(Display *dpy, GLXWindow win);

ANGLE_EXPORT const char *GL_APIENTRY glXGetClientString(Display *dpy, int name);

ANGLE_EXPORT int GL_APIENTRY glXGetConfig(Display *dpy, XVisualInfo *vis, int attrib, int *value);

ANGLE_EXPORT GLXContext GL_APIENTRY glXGetCurrentContext();

ANGLE_EXPORT Display *GL_APIENTRY glXGetCurrentDisplay();

ANGLE_EXPORT GLXDrawable GL_APIENTRY glXGetCurrentDrawable();

ANGLE_EXPORT GLXDrawable GL_APIENTRY glXGetCurrentReadDrawable();

ANGLE_EXPORT int GL_APIENTRY glXGetFBConfigAttrib(Display *dpy,
                                                  GLXFBConfig config,
                                                  int attribute,
                                                  int *value);

ANGLE_EXPORT GLXFBConfig *GL_APIENTRY glXGetFBConfigs(Display *dpy, int screen, int *nelements);

ANGLE_EXPORT __GLXextFuncPtr GL_APIENTRY glXGetProcAddress(const GLubyte *procName);

ANGLE_EXPORT void GL_APIENTRY glXGetSelectedEvent(Display *dpy,
                                                  GLXDrawable draw,
                                                  unsigned long *event_mask);

ANGLE_EXPORT XVisualInfo *GL_APIENTRY glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config);

ANGLE_EXPORT bool GL_APIENTRY glXIsDirect(Display *dpy, GLXContext ctx);

ANGLE_EXPORT bool GL_APIENTRY glXMakeContextCurrent(Display *dpy,
                                                    GLXDrawable draw,
                                                    GLXDrawable read,
                                                    GLXContext ctx);

ANGLE_EXPORT bool GL_APIENTRY glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);

ANGLE_EXPORT int GL_APIENTRY glXQueryContext(Display *dpy,
                                             GLXContext ctx,
                                             int attribute,
                                             int *value);

ANGLE_EXPORT int GL_APIENTRY glXQueryDrawable(Display *dpy,
                                              GLXDrawable draw,
                                              int attribute,
                                              unsigned int *value);

ANGLE_EXPORT bool GL_APIENTRY glXQueryExtension(Display *dpy, int *errorBase, int *eventBase);

ANGLE_EXPORT const char *GL_APIENTRY glXQueryExtensionsString(Display *dpy, int screen);

ANGLE_EXPORT bool GL_APIENTRY glXQueryVersion(Display *dpy, int *major, int *minor);

ANGLE_EXPORT void GL_APIENTRY glXSelectEvent(Display *dpy,
                                             GLXDrawable draw,
                                             unsigned long event_mask);

ANGLE_EXPORT void GL_APIENTRY glXSwapBuffers(Display *dpy, GLXDrawable drawable);

ANGLE_EXPORT void GL_APIENTRY glXUseXFont(Font font, int first, int count, int listBase);

ANGLE_EXPORT void GL_APIENTRY glXWaitGL();

ANGLE_EXPORT void GL_APIENTRY glXWaitX();

}  // extern "C"

#endif  // LIBGL_GLX_H_
