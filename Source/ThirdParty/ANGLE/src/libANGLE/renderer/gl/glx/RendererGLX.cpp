//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererGLX.cpp: Implements the class methods for RendererGLX.

#include "libANGLE/renderer/gl/glx/RendererGLX.h"

#include "libANGLE/renderer/gl/glx/DisplayGLX.h"

namespace rx
{

RendererGLX::RendererGLX(std::unique_ptr<FunctionsGL> functions,
                         const egl::AttributeMap &attribMap,
                         DisplayGLX *display)
    : RendererGL(std::move(functions), attribMap), mDisplay(display)
{}

RendererGLX::~RendererGLX() {}

WorkerContext *RendererGLX::createWorkerContext(std::string *infoLog)
{
    return mDisplay->createWorkerContext(infoLog);
}

}  // namespace rx
