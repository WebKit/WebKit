//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/gl/glx/RendererGLX.h"

#include "libANGLE/renderer/gl/glx/DisplayGLX.h"

namespace rx
{

RendererGLX::RendererGLX(std::unique_ptr<FunctionsGL> functionsGL,
                         const egl::AttributeMap &attribMap,
                         const FunctionsGLX &glx,
                         DisplayGLX *display,
                         glx::Context context,
                         glx::Pbuffer pbuffer)
    : RendererGL(std::move(functionsGL), attribMap, display),
      mGLX(glx),
      mContext(context),
      mPbuffer(pbuffer)
{}

RendererGLX::~RendererGLX()
{
    if (mPbuffer)
    {
        mGLX.destroyPbuffer(mPbuffer);
    }

    if (mContext)
    {
        mGLX.destroyContext(mContext);
    }
}

glx::Context RendererGLX::getContext() const
{
    return mContext;
}

glx::Pbuffer RendererGLX::getPbuffer() const
{
    return mPbuffer;
}

}  // namespace rx
