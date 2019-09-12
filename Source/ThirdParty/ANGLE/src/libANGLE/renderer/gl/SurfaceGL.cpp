//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGL.cpp: OpenGL implementation of egl::Surface

#include "libANGLE/renderer/gl/SurfaceGL.h"

#include "libANGLE/Context.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/BlitGL.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"

namespace rx
{

SurfaceGL::SurfaceGL(const egl::SurfaceState &state) : SurfaceImpl(state) {}

SurfaceGL::~SurfaceGL() {}

FramebufferImpl *SurfaceGL::createDefaultFramebuffer(const gl::Context *context,
                                                     const gl::FramebufferState &data)
{
    return new FramebufferGL(data, 0, true);
}

egl::Error SurfaceGL::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    UNREACHABLE();
    return egl::EglBadSurface();
}

egl::Error SurfaceGL::unMakeCurrent()
{
    return egl::NoError();
}

angle::Result SurfaceGL::initializeContents(const gl::Context *context,
                                            const gl::ImageIndex &imageIndex)
{
    FramebufferGL *framebufferGL = GetImplAs<FramebufferGL>(context->getFramebuffer(0));
    ASSERT(framebufferGL->isDefault());

    BlitGL *blitter = GetBlitGL(context);
    ANGLE_TRY(blitter->clearFramebuffer(framebufferGL));

    return angle::Result::Continue;
}

}  // namespace rx
