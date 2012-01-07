//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// main.h: Management of thread-local data.

#ifndef LIBGLESV2_MAIN_H_
#define LIBGLESV2_MAIN_H_

#define GL_APICALL
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "common/debug.h"
#include "libEGL/Display.h"

#include "libGLESv2/Context.h"

namespace gl
{
struct Current
{
    Context *context;
    egl::Display *display;
};

void makeCurrent(Context *context, egl::Display *display, egl::Surface *surface);

Context *getContext();
Context *getNonLostContext();
egl::Display *getDisplay();

IDirect3DDevice9 *getDevice();

bool checkDeviceLost(HRESULT errorCode);
}

void error(GLenum errorCode);

template<class T>
const T &error(GLenum errorCode, const T &returnValue)
{
    error(errorCode);

    return returnValue;
}

#endif   // LIBGLESV2_MAIN_H_
