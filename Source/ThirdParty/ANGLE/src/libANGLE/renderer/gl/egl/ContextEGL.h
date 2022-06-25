//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextEGL.h: Context class for GL on Android/ChromeOS.  Wraps a RendererEGL.

#ifndef LIBANGLE_RENDERER_GL_EGL_CONTEXTEGL_H_
#define LIBANGLE_RENDERER_GL_EGL_CONTEXTEGL_H_

#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/egl/RendererEGL.h"

namespace rx
{

struct ExternalContextState;

class ContextEGL : public ContextGL
{
  public:
    ContextEGL(const gl::State &state,
               gl::ErrorSet *errorSet,
               const std::shared_ptr<RendererEGL> &renderer,
               RobustnessVideoMemoryPurgeStatus robustnessVideoMemoryPurgeStatus);
    ~ContextEGL() override;

    angle::Result onMakeCurrent(const gl::Context *context) override;
    angle::Result onUnMakeCurrent(const gl::Context *context) override;

    EGLContext getContext() const;

  private:
    std::shared_ptr<RendererEGL> mRendererEGL;
    std::unique_ptr<ExternalContextState> mExtState;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_RENDEREREGL_H_
