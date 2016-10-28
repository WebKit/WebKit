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

gl::Error RenderbufferVk::setStorage(GLenum internalformat, size_t width, size_t height)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error RenderbufferVk::setStorageMultisample(size_t samples,
                                                GLenum internalformat,
                                                size_t width,
                                                size_t height)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error RenderbufferVk::setStorageEGLImageTarget(egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error RenderbufferVk::getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                                    FramebufferAttachmentRenderTarget **rtOut)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
