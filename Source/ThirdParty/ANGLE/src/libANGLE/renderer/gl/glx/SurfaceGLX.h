//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGLX.h: common interface for GLX surfaces

#ifndef LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_
#define LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

namespace rx
{

class SurfaceGLX : public SurfaceGL
{
  public:
    SurfaceGLX(const egl::SurfaceState &state, RendererGL *renderer) : SurfaceGL(state, renderer) {}

    virtual egl::Error checkForResize() = 0;
};
}

#endif  // LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_
