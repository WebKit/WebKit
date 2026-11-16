//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextGLX.h: Context class for GLX on Linux.

#ifndef LIBANGLE_RENDERER_GL_GLX_CONTEXTGLX_H_
#define LIBANGLE_RENDERER_GL_GLX_CONTEXTGLX_H_

#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/glx/FunctionsGLX.h"

namespace rx
{

class RendererGLX;
struct ExternalContextState;

class ContextGLX : public ContextGL
{
  public:
    ContextGLX(const gl::State &state,
               gl::ErrorSet *errorSet,
               const std::shared_ptr<RendererGLX> &renderer,
               RobustnessVideoMemoryPurgeStatus robustnessVideoMemoryPurgeStatus);
    ~ContextGLX() override;

    glx::Context getContext() const;
    glx::Pbuffer getPbuffer() const;

  private:
    std::shared_ptr<RendererGLX> mRendererGLX;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_
