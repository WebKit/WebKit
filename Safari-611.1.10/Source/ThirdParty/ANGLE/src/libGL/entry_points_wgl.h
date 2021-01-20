//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// entry_points_wgl.h: Declares the exported WGL functions.

#ifndef LIBGL_WGL_H_
#define LIBGL_WGL_H_

// Define _GDI32_ so that wingdi.h doesn't declare functions as imports
#ifndef _GDI32_
#    define _GDI32_
#endif

#include "angle_gl.h"

#include "WGL/wgl.h"

extern "C" {

// WGL 1.0
int GL_APIENTRY wglChoosePixelFormat(HDC hDc, const PIXELFORMATDESCRIPTOR *pPfd);

int GL_APIENTRY wglDescribePixelFormat(HDC hdc, int ipfd, UINT cjpfd, PIXELFORMATDESCRIPTOR *ppfd);

UINT GL_APIENTRY wglGetEnhMetaFilePixelFormat(HENHMETAFILE hemf,
                                              UINT cbBuffer,
                                              PIXELFORMATDESCRIPTOR *ppfd);

int GL_APIENTRY wglGetPixelFormat(HDC hdc);

BOOL GL_APIENTRY wglSetPixelFormat(HDC hdc, int ipfd, const PIXELFORMATDESCRIPTOR *ppfd);

BOOL GL_APIENTRY wglSwapBuffers(HDC hdc);

}  // extern "C"

#endif  // LIBGL_WGL_H_
