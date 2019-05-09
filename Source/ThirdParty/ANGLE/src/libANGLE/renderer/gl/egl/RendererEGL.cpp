//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/gl/egl/RendererEGL.h"

#include "libANGLE/renderer/gl/egl/DisplayEGL.h"

namespace rx
{

RendererEGL::RendererEGL(std::unique_ptr<FunctionsGL> functionsGL,
                         const egl::AttributeMap &attribMap,
                         DisplayEGL *display,
                         EGLContext context,
                         const native_egl::AttributeVector attribs)
    : RendererGL(std::move(functionsGL), attribMap),
      mDisplay(display),
      mContext(context),
      mAttribs(attribs)
{}

RendererEGL::~RendererEGL()
{
    mDisplay->destroyNativeContext(mContext);
    mContext = nullptr;
}

EGLContext RendererEGL::getContext() const
{
    return mContext;
}

WorkerContext *RendererEGL::createWorkerContext(std::string *infoLog)
{
    return mDisplay->createWorkerContext(infoLog, mContext, mAttribs);
}

}  // namespace rx
