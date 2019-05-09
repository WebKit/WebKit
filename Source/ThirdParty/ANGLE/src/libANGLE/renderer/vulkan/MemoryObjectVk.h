// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MemoryObjectVk.h: Defines the class interface for MemoryObjectVk,
// implementing MemoryObjectImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_MEMORYOBJECTVK_H_
#define LIBANGLE_RENDERER_VULKAN_MEMORYOBJECTVK_H_

#include "libANGLE/renderer/MemoryObjectImpl.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

namespace rx
{

class MemoryObjectVk : public MemoryObjectImpl
{
  public:
    MemoryObjectVk();
    ~MemoryObjectVk() override;

    void onDestroy(const gl::Context *context) override;

    angle::Result importFd(gl::Context *context,
                           GLuint64 size,
                           gl::HandleType handleType,
                           GLint fd) override;

    angle::Result createImage(const gl::Context *context,
                              gl::TextureType type,
                              size_t levels,
                              GLenum internalFormat,
                              const gl::Extents &size,
                              GLuint64 offset,
                              vk::ImageHelper *image);

  private:
    angle::Result importOpaqueFd(gl::Context *context, GLuint64 size, GLint fd);

    GLuint64 mSize;
    int mFd;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_MEMORYOBJECTVK_H_
