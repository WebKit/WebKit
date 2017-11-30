//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLImageD3D.cpp: Implements the rx::EGLImageD3D class, the D3D implementation of EGL images

#include "libANGLE/renderer/d3d/EGLImageD3D.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/TextureStorage.h"

#include <EGL/eglext.h>

namespace rx
{

EGLImageD3D::EGLImageD3D(const egl::ImageState &state,
                         EGLenum target,
                         const egl::AttributeMap &attribs,
                         RendererD3D *renderer)
    : ImageImpl(state), mRenderer(renderer), mRenderTarget(nullptr)
{
    ASSERT(renderer != nullptr);
}

EGLImageD3D::~EGLImageD3D()
{
    SafeDelete(mRenderTarget);
}

egl::Error EGLImageD3D::initialize()
{
    return egl::NoError();
}

gl::Error EGLImageD3D::orphan(const gl::Context *context, egl::ImageSibling *sibling)
{
    if (sibling == mState.source.get())
    {
        ANGLE_TRY(copyToLocalRendertarget(context));
    }

    return gl::NoError();
}

gl::Error EGLImageD3D::getRenderTarget(const gl::Context *context, RenderTargetD3D **outRT) const
{
    if (mState.source.get())
    {
        ASSERT(!mRenderTarget);
        FramebufferAttachmentRenderTarget *rt = nullptr;
        ANGLE_TRY(
            mState.source->getAttachmentRenderTarget(context, GL_NONE, mState.imageIndex, &rt));
        *outRT = static_cast<RenderTargetD3D *>(rt);
        return gl::NoError();
    }
    else
    {
        ASSERT(mRenderTarget);
        *outRT = mRenderTarget;
        return gl::NoError();
    }
}

gl::Error EGLImageD3D::copyToLocalRendertarget(const gl::Context *context)
{
    ASSERT(mState.source.get() != nullptr);
    ASSERT(mRenderTarget == nullptr);

    RenderTargetD3D *curRenderTarget = nullptr;
    ANGLE_TRY(getRenderTarget(context, &curRenderTarget));

    // This only currently applies do D3D11, where it invalidates FBOs with this Image attached.
    curRenderTarget->signalDirty(context);

    return mRenderer->createRenderTargetCopy(curRenderTarget, &mRenderTarget);
}
}  // namespace rx
