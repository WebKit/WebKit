//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceEGL.h: EGL implementation of egl::Surface for windows

#include "libANGLE/renderer/gl/egl/WindowSurfaceEGL.h"

#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/egl/egl_utils.h"

namespace rx
{

WindowSurfaceEGL::WindowSurfaceEGL(const egl::SurfaceState &state,
                                   const FunctionsEGL *egl,
                                   EGLConfig config,
                                   EGLNativeWindowType window,
                                   RendererGL *renderer)
    : SurfaceEGL(state, egl, config, renderer), mWindow(window)
{
}

WindowSurfaceEGL::~WindowSurfaceEGL()
{
}

egl::Error WindowSurfaceEGL::initialize(const egl::Display *display)
{
    constexpr EGLint kForwardedWindowSurfaceAttributes[] = {
        EGL_RENDER_BUFFER, EGL_POST_SUB_BUFFER_SUPPORTED_NV,
    };

    native_egl::AttributeVector nativeAttribs =
        native_egl::TrimAttributeMap(mState.attributes, kForwardedWindowSurfaceAttributes);
    native_egl::FinalizeAttributeVector(&nativeAttribs);

    mSurface = mEGL->createWindowSurface(mConfig, mWindow, nativeAttribs.data());
    if (mSurface == EGL_NO_SURFACE)
    {
        return egl::Error(mEGL->getError(), "eglCreateWindowSurface failed");
    }

    return egl::NoError();
}

}  // namespace rx
