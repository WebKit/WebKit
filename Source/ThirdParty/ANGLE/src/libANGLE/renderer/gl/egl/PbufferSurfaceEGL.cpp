//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PbufferSurfaceEGL.h: EGL implementation of egl::Surface for pbuffers

#include "libANGLE/renderer/gl/egl/PbufferSurfaceEGL.h"

namespace rx
{

PbufferSurfaceEGL::PbufferSurfaceEGL(const egl::SurfaceState &state,
                                     const FunctionsEGL *egl,
                                     EGLConfig config,
                                     const std::vector<EGLint> &attribList,
                                     EGLContext context,
                                     RendererGL *renderer)
    : SurfaceEGL(state, egl, config, attribList, context, renderer)
{
}

PbufferSurfaceEGL::~PbufferSurfaceEGL()
{
}

egl::Error PbufferSurfaceEGL::initialize()
{
    mSurface = mEGL->createPbufferSurface(mConfig, mAttribList.data());
    if (mSurface == EGL_NO_SURFACE)
    {
        return egl::Error(mEGL->getError(), "eglCreatePbufferSurface failed");
    }

    return egl::Error(EGL_SUCCESS);
}

}  // namespace rx
