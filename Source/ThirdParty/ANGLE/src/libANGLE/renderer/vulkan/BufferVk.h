//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferVk.h:
//    Defines the class interface for BufferVk, implementing BufferImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
#define LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_

#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/vulkan/renderervk_utils.h"

namespace rx
{

class BufferVk : public BufferImpl, public ResourceVk
{
  public:
    BufferVk(const gl::BufferState &state);
    ~BufferVk() override;
    void destroy(ContextImpl *contextImpl) override;

    gl::Error setData(ContextImpl *context,
                      GLenum target,
                      const void *data,
                      size_t size,
                      GLenum usage) override;
    gl::Error setSubData(ContextImpl *context,
                         GLenum target,
                         const void *data,
                         size_t size,
                         size_t offset) override;
    gl::Error copySubData(ContextImpl *contextImpl,
                          BufferImpl *source,
                          GLintptr sourceOffset,
                          GLintptr destOffset,
                          GLsizeiptr size) override;
    gl::Error map(ContextImpl *contextImpl, GLenum access, GLvoid **mapPtr) override;
    gl::Error mapRange(ContextImpl *contextImpl,
                       size_t offset,
                       size_t length,
                       GLbitfield access,
                       GLvoid **mapPtr) override;
    gl::Error unmap(ContextImpl *contextImpl, GLboolean *result) override;

    gl::Error getIndexRange(GLenum type,
                            size_t offset,
                            size_t count,
                            bool primitiveRestartEnabled,
                            gl::IndexRange *outRange) override;

    const vk::Buffer &getVkBuffer() const;

  private:
    vk::Error setDataImpl(VkDevice device, const uint8_t *data, size_t size, size_t offset);

    vk::Buffer mBuffer;
    size_t mRequiredSize;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
