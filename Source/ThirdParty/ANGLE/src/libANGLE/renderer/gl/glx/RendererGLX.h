//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererGLX.h: Renderer class for GL on Linux.  Owns a GLX context object.

#ifndef LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_
#define LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_

#include "libANGLE/renderer/gl/RendererGL.h"

#include "libANGLE/renderer/gl/glx/FunctionsGLX.h"

#undef Success

namespace rx
{
class DisplayGLX;

class RendererGLX : public RendererGL
{
  public:
    RendererGLX(std::unique_ptr<FunctionsGL> functionsGL,
                const egl::AttributeMap &attribMap,
                const FunctionsGLX &glx,
                DisplayGLX *display,
                glx::Context context,
                glx::Pbuffer pbuffer);
    ~RendererGLX() override;

    glx::Context getContext() const;
    glx::Pbuffer getPbuffer() const;

  private:
    const FunctionsGLX &mGLX;
    glx::Context mContext;
    glx::Pbuffer mPbuffer;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_
