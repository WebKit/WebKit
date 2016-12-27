//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferNULL.cpp:
//    Implements the class methods for RenderbufferNULL.
//

#include "libANGLE/renderer/null/RenderbufferNULL.h"

#include "common/debug.h"

namespace rx
{

RenderbufferNULL::RenderbufferNULL() : RenderbufferImpl()
{
}

RenderbufferNULL::~RenderbufferNULL()
{
}

gl::Error RenderbufferNULL::setStorage(GLenum internalformat, size_t width, size_t height)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error RenderbufferNULL::setStorageMultisample(size_t samples,
                                                  GLenum internalformat,
                                                  size_t width,
                                                  size_t height)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error RenderbufferNULL::setStorageEGLImageTarget(egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
