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
class RendererVk;

class BufferVk : public BufferImpl, public ResourceVk
{
  public:
    BufferVk(const gl::BufferState &state);
    ~BufferVk() override;
    void destroy(const gl::Context *context) override;

    gl::Error setData(const gl::Context *context,
                      gl::BufferBinding target,
                      const void *data,
                      size_t size,
                      gl::BufferUsage usage) override;
    gl::Error setSubData(const gl::Context *context,
                         gl::BufferBinding target,
                         const void *data,
                         size_t size,
                         size_t offset) override;
    gl::Error copySubData(const gl::Context *context,
                          BufferImpl *source,
                          GLintptr sourceOffset,
                          GLintptr destOffset,
                          GLsizeiptr size) override;
    gl::Error map(const gl::Context *context, GLenum access, void **mapPtr) override;
    gl::Error mapRange(const gl::Context *context,
                       size_t offset,
                       size_t length,
                       GLbitfield access,
                       void **mapPtr) override;
    gl::Error unmap(const gl::Context *context, GLboolean *result) override;

    gl::Error getIndexRange(const gl::Context *context,
                            GLenum type,
                            size_t offset,
                            size_t count,
                            bool primitiveRestartEnabled,
                            gl::IndexRange *outRange) override;

    const vk::Buffer &getVkBuffer() const;

  private:
    vk::Error setDataImpl(ContextVk *contextVk, const uint8_t *data, size_t size, size_t offset);
    void release(RendererVk *renderer);

    vk::Buffer mBuffer;
    vk::DeviceMemory mBufferMemory;
    size_t mCurrentRequiredSize;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
