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

RenderbufferNULL::RenderbufferNULL(const gl::RenderbufferState &state) : RenderbufferImpl(state) {}

RenderbufferNULL::~RenderbufferNULL() {}

angle::Result RenderbufferNULL::setStorage(const gl::Context *context,
                                           GLenum internalformat,
                                           size_t width,
                                           size_t height)
{
    return angle::Result::Continue;
}

angle::Result RenderbufferNULL::setStorageMultisample(const gl::Context *context,
                                                      size_t samples,
                                                      GLenum internalformat,
                                                      size_t width,
                                                      size_t height)
{
    return angle::Result::Continue;
}

angle::Result RenderbufferNULL::setStorageEGLImageTarget(const gl::Context *context,
                                                         egl::Image *image)
{
    return angle::Result::Continue;
}

angle::Result RenderbufferNULL::initializeContents(const gl::Context *context,
                                                   const gl::ImageIndex &imageIndex)
{
    return angle::Result::Continue;
}

}  // namespace rx
