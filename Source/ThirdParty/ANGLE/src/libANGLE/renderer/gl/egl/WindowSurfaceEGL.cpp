//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceEGL.h: EGL implementation of egl::Surface for windows

#include "libANGLE/renderer/gl/egl/WindowSurfaceEGL.h"

namespace rx
{

WindowSurfaceEGL::WindowSurfaceEGL(const egl::SurfaceState &state,
                                   const FunctionsEGL *egl,
                                   EGLConfig config,
                                   EGLNativeWindowType window,
                                   const std::vector<EGLint> &attribList,
                                   EGLContext context,
                                   RendererGL *renderer)
    : SurfaceEGL(state, egl, config, attribList, context, renderer), mWindow(window)
{
}

WindowSurfaceEGL::~WindowSurfaceEGL()
{
}

egl::Error WindowSurfaceEGL::initialize()
{
    mSurface = mEGL->createWindowSurface(mConfig, mWindow, mAttribList.data());
    if (mSurface == EGL_NO_SURFACE)
    {
        return egl::Error(mEGL->getError(), "eglCreateWindowSurface failed");
    }

    return egl::Error(EGL_SUCCESS);
}

}  // namespace rx
