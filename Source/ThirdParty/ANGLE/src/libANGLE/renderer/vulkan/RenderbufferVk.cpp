//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferVk.cpp:
//    Implements the class methods for RenderbufferVk.
//

#include "libANGLE/renderer/vulkan/RenderbufferVk.h"

#include "common/debug.h"

namespace rx
{

RenderbufferVk::RenderbufferVk() : RenderbufferImpl()
{
}

RenderbufferVk::~RenderbufferVk()
{
}

gl::Error RenderbufferVk::setStorage(const gl::Context *context,
                                     GLenum internalformat,
                                     size_t width,
                                     size_t height)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error RenderbufferVk::setStorageMultisample(const gl::Context *context,
                                                size_t samples,
                                                GLenum internalformat,
                                                size_t width,
                                                size_t height)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error RenderbufferVk::setStorageEGLImageTarget(const gl::Context *context, egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error RenderbufferVk::getAttachmentRenderTarget(const gl::Context *context,
                                                    GLenum binding,
                                                    const gl::ImageIndex &imageIndex,
                                                    FramebufferAttachmentRenderTarget **rtOut)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error RenderbufferVk::initializeContents(const gl::Context *context,
                                             const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return gl::NoError();
}

}  // namespace rx
