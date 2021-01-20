//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/gl/wgl/RendererWGL.h"

#include "libANGLE/renderer/gl/wgl/DisplayWGL.h"

namespace rx
{

RendererWGL::RendererWGL(std::unique_ptr<FunctionsGL> functionsGL,
                         const egl::AttributeMap &attribMap,
                         DisplayWGL *display,
                         HGLRC context,
                         HGLRC sharedContext,
                         const std::vector<int> workerContextAttribs)
    : RendererGL(std::move(functionsGL), attribMap, display),
      mDisplay(display),
      mContext(context),
      mSharedContext(sharedContext),
      mWorkerContextAttribs(workerContextAttribs)
{}

RendererWGL::~RendererWGL()
{
    if (mSharedContext != nullptr)
    {
        mDisplay->destroyNativeContext(mSharedContext);
    }
    mDisplay->destroyNativeContext(mContext);
    mContext = nullptr;
}

HGLRC RendererWGL::getContext() const
{
    return mContext;
}

WorkerContext *RendererWGL::createWorkerContext(std::string *infoLog)
{
    return mDisplay->createWorkerContext(infoLog, mSharedContext, mWorkerContextAttribs);
}

}  // namespace rx
