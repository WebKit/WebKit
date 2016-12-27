//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferAttachmentObjectImpl.h:
//   Common ancenstor for all implementations of FBO attachable-objects.
//   This means Surfaces, Textures and Renderbuffers.
//

#ifndef LIBANGLE_RENDERER_FRAMEBUFFER_ATTACHMENT_OBJECT_IMPL_H_
#define LIBANGLE_RENDERER_FRAMEBUFFER_ATTACHMENT_OBJECT_IMPL_H_

#include "libANGLE/FramebufferAttachment.h"

namespace rx
{

class FramebufferAttachmentObjectImpl : angle::NonCopyable
{
  public:
    FramebufferAttachmentObjectImpl() {}
    virtual ~FramebufferAttachmentObjectImpl() {}

    virtual gl::Error getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                                FramebufferAttachmentRenderTarget **rtOut)
    {
        UNIMPLEMENTED();
        return gl::Error(GL_OUT_OF_MEMORY, "getAttachmentRenderTarget not supported.");
    }
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_FRAMEBUFFER_ATTACHMENT_OBJECT_IMPL_H_
