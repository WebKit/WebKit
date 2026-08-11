//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/gl/glx/ContextGLX.h"
#include "libANGLE/renderer/gl/glx/RendererGLX.h"

namespace rx
{

ContextGLX::ContextGLX(const gl::State &state,
                       gl::ErrorSet *errorSet,
                       const std::shared_ptr<RendererGLX> &renderer,
                       RobustnessVideoMemoryPurgeStatus robustnessVideoMemoryPurgeStatus)
    : ContextGL(state, errorSet, renderer, robustnessVideoMemoryPurgeStatus), mRendererGLX(renderer)
{}

ContextGLX::~ContextGLX() {}

glx::Context ContextGLX::getContext() const
{
    return mRendererGLX->getContext();
}

glx::Pbuffer ContextGLX::getPbuffer() const
{
    return mRendererGLX->getPbuffer();
}

}  // namespace rx
