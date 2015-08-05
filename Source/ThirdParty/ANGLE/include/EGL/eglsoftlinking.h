/* 
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SoftLinking.h"

SOFT_LINK_LIBRARY(libEGL)

SOFT_LINK(libEGL, eglGetError, EGLint, EGLAPIENTRY, (void), ());
SOFT_LINK_OPTIONAL(libEGL, eglGetDisplay, EGLDisplay, EGLAPIENTRY, (EGLNativeDisplayType display_id));
SOFT_LINK(libEGL, eglInitialize, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLint *major, EGLint *minor), (dpy, major, minor));
SOFT_LINK(libEGL, eglTerminate, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy), (dpy));
SOFT_LINK(libEGL, eglQueryString, const char *, EGLAPIENTRY, (EGLDisplay dpy, EGLint name), (dpy, name));
SOFT_LINK(libEGL, eglGetConfigs, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config), (dpy, configs, config_size, num_config));
SOFT_LINK(libEGL, eglChooseConfig, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config), (dpy, attrib_list, configs, config_size, num_config));
SOFT_LINK(libEGL, eglGetConfigAttrib, EGLint, EGLAPIENTRY, (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value), (dpy, config, attribute, value));
SOFT_LINK(libEGL, eglCreateWindowSurface, EGLSurface, EGLAPIENTRY, (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list), (dpy, config, win, attrib_list));
SOFT_LINK(libEGL, eglCreatePbufferSurface, EGLSurface, EGLAPIENTRY, (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list), (dpy, config, attrib_list));
SOFT_LINK(libEGL, eglCreatePixmapSurface, EGLSurface, EGLAPIENTRY, (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list), (dpy, config, pixmap, attrib_list));
SOFT_LINK(libEGL, eglDestroySurface, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface surface), (dpy, surface));
SOFT_LINK(libEGL, eglQuerySurface, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value), (dpy, surface, attribute, value));
SOFT_LINK(libEGL, eglBindAPI, EGLBoolean, EGLAPIENTRY, (EGLenum api), (api));
SOFT_LINK(libEGL, eglQueryAPI, EGLenum, EGLAPIENTRY, (void), ());
SOFT_LINK(libEGL, eglWaitClient, EGLBoolean, EGLAPIENTRY, (void), ());
SOFT_LINK(libEGL, eglReleaseThread, EGLBoolean, EGLAPIENTRY, (void), ());
SOFT_LINK(libEGL, eglCreatePbufferFromClientBuffer, EGLSurface, EGLAPIENTRY, (EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list), (dpy, buftype, buffer, config, attrib_list));
SOFT_LINK(libEGL, eglSurfaceAttrib, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value), (dpy, surface, attribute, value));
SOFT_LINK(libEGL, eglBindTexImage, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface surface, EGLint buffer), (dpy, surface, buffer));
SOFT_LINK(libEGL, eglReleaseTexImage, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface surface, EGLint buffer), (dpy, surface, buffer));
SOFT_LINK(libEGL, eglSwapInterval, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLint interval), (dpy, interval));
SOFT_LINK(libEGL, eglCreateContext, EGLContext, EGLAPIENTRY, (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list), (dpy, config, share_context, attrib_list));
SOFT_LINK(libEGL, eglDestroyContext, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLContext ctx), (dpy, ctx));
SOFT_LINK(libEGL, eglMakeCurrent, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx), (dpy, draw, read, ctx));
SOFT_LINK(libEGL, eglGetCurrentContext, EGLContext, EGLAPIENTRY, (void), ());
SOFT_LINK(libEGL, eglGetCurrentSurface, EGLSurface, EGLAPIENTRY, (EGLint readdraw), (readdraw));
SOFT_LINK(libEGL, eglGetCurrentDisplay, EGLDisplay, EGLAPIENTRY, (void), ());
SOFT_LINK(libEGL, eglQueryContext, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value), (dpy, ctx, attribute, value));
SOFT_LINK(libEGL, eglWaitGL, EGLBoolean, EGLAPIENTRY, (void), ());
SOFT_LINK(libEGL, eglWaitNative, EGLBoolean, EGLAPIENTRY, (EGLint engine), (engine));
SOFT_LINK(libEGL, eglSwapBuffers, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface surface), (dpy, surface));
SOFT_LINK(libEGL, eglCopyBuffers, EGLBoolean, EGLAPIENTRY, (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target), (dpy, surface, target));
typedef void (*__eglMustCastToProperFunctionPointerType)(void);
SOFT_LINK(libEGL, eglGetProcAddress, __eglMustCastToProperFunctionPointerType, EGLAPIENTRY, (const char *procname), (procname));
