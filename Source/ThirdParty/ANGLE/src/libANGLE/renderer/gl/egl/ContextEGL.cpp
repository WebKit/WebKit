//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/gl/egl/ContextEGL.h"

#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

namespace rx
{

ContextEGL::ContextEGL(const gl::State &state,
                       gl::ErrorSet *errorSet,
                       const std::shared_ptr<RendererEGL> &renderer,
                       RobustnessVideoMemoryPurgeStatus robustnessVideoMemoryPurgeStatus)
    : ContextGL(state, errorSet, renderer, robustnessVideoMemoryPurgeStatus), mRendererEGL(renderer)
{}

ContextEGL::~ContextEGL() {}

void ContextEGL::acquireExternalContext(const gl::Context *context)
{
    ASSERT(context->isExternal());

    if (!mExtState)
    {
        mExtState = getStateManager()->createContextStateGL();
    }

    ANGLE_SWALLOW_ERR(getStateManager()->syncFromNativeContext(context, mExtState.get()));

    // Use current FBO as the default framebuffer when the external context is current.
    // First save the current ID of the default framebuffer to restore in
    // onUnMakeCurrent().
    gl::Framebuffer *framebuffer = mState.getDefaultFramebuffer();
    auto framebufferGL           = GetImplAs<FramebufferGL>(framebuffer);
    mPrevDefaultFramebufferID    = framebufferGL->getFramebufferID();
    framebufferGL->updateDefaultFramebufferID(
        mExtState->framebuffers[angle::FramebufferBindingDraw]);
}

void ContextEGL::releaseExternalContext(const gl::Context *context)
{
    ASSERT(context->isExternal());
    ASSERT(mExtState);

    ANGLE_SWALLOW_ERR(getStateManager()->restoreNativeContext(context, *mExtState.get()));

    // If the default framebuffer exists, update its ID (note that there can
    // be multiple consecutive onUnMakeCurrent() calls in destruction, and
    // the default FBO will have been unset by the first one).
    gl::Framebuffer *framebuffer = mState.getDefaultFramebuffer();
    if (framebuffer)
    {
        auto framebufferGL = GetImplAs<FramebufferGL>(framebuffer);
        framebufferGL->updateDefaultFramebufferID(mPrevDefaultFramebufferID);
    }
}

EGLContext ContextEGL::getContext() const
{
    return mRendererEGL->getContext();
}

}  // namespace rx
