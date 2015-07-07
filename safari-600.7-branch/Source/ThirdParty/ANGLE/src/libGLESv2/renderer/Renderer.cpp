#include "precompiled.h"
//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer.cpp: Implements EGL dependencies for creating and destroying Renderer instances.

#include <EGL/eglext.h>
#include "libGLESv2/main.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/renderer/Renderer.h"
#include "common/utilities.h"
#include "third_party/trace_event/trace_event.h"
#include "libGLESv2/Shader.h"

#if defined (ANGLE_ENABLE_D3D9)
#include "libGLESv2/renderer/d3d9/Renderer9.h"
#endif // ANGLE_ENABLE_D3D9

#if defined (ANGLE_ENABLE_D3D11)
#include "libGLESv2/renderer/d3d11/Renderer11.h"
#endif // ANGLE_ENABLE_D3D11

#if !defined(ANGLE_DEFAULT_D3D11)
// Enables use of the Direct3D 11 API for a default display, when available
#define ANGLE_DEFAULT_D3D11 0
#endif

namespace rx
{

Renderer::Renderer(egl::Display *display) : mDisplay(display)
{
    mCurrentClientVersion = 2;
}

Renderer::~Renderer()
{
    gl::Shader::releaseCompiler();
}

}

extern "C"
{

rx::Renderer *glCreateRenderer(egl::Display *display, HDC hDc, EGLNativeDisplayType displayId)
{
#if defined(ANGLE_ENABLE_D3D11)
    if (ANGLE_DEFAULT_D3D11 ||
        displayId == EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ||
        displayId == EGL_D3D11_ONLY_DISPLAY_ANGLE)
    {
        rx::Renderer11 *renderer = new rx::Renderer11(display, hDc);
        if (renderer->initialize() == EGL_SUCCESS)
        {
            return renderer;
        }
        else
        {
            // Failed to create a D3D11 renderer, try D3D9
            SafeDelete(renderer);
        }
    }
#endif

#if defined(ANGLE_ENABLE_D3D9)
    if (displayId != EGL_D3D11_ONLY_DISPLAY_ANGLE)
    {
        bool softwareDevice = (displayId == EGL_SOFTWARE_DISPLAY_ANGLE);
        rx::Renderer9 *renderer = new rx::Renderer9(display, hDc, softwareDevice);
        if (renderer->initialize() == EGL_SUCCESS)
        {
            return renderer;
        }
        else
        {
            SafeDelete(renderer);
        }
    }
#endif

    return NULL;
}

void glDestroyRenderer(rx::Renderer *renderer)
{
    delete renderer;
}

}
