//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SyncVk:
//    Defines the class interface for SyncVk, implementing SyncImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_
#define LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_

#include "libANGLE/renderer/SyncImpl.h"

namespace rx
{

class SyncVk : public SyncImpl
{
  public:
    SyncVk();
    ~SyncVk() override;

    gl::Error set(GLenum condition, GLbitfield flags) override;
    gl::Error clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult) override;
    gl::Error serverWait(GLbitfield flags, GLuint64 timeout) override;
    gl::Error getStatus(GLint *outResult) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_
