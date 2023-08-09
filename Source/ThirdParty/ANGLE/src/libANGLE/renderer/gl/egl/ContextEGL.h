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

    void acquireExternalContext(const gl::Context *context) override;
    void releaseExternalContext(const gl::Context *context) override;

    EGLContext getContext() const;

  private:
    std::shared_ptr<RendererEGL> mRendererEGL;
    std::unique_ptr<ExternalContextState> mExtState;

    // Used to restore the default FBO's ID on unmaking an external context
    // current, as when making an external context current ANGLE sets the
    // default FBO's ID to that bound in the external context.
    GLuint mPrevDefaultFramebufferID = 0;

    // onMakeCurrent() can be called when this context is already current, which
    // introduces complexities for the case where this context is wrapping an
    // external context. This variable is used to ensure that in this case we
    // save state from the native context only when first transitioning this
    // context to be current. It is true for the duration of time between an
    // onMakeCurrent() call and an onUnMakeCurrent() call (note: there is no
    // "nesting" of onUnMakeCurrent() calls, i.e., no matter how many
    // onMakeCurrent() calls have occurred consecutively, an onUnMakeCurrent()
    // call transitions this context away from being current).
    bool mIsCurrent = false;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_RENDEREREGL_H_
