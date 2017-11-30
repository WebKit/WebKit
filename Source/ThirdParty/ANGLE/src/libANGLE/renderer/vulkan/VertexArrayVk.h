//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayVk.h:
//    Defines the class interface for VertexArrayVk, implementing VertexArrayImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
#define LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_

#include "libANGLE/renderer/VertexArrayImpl.h"
#include "libANGLE/renderer/vulkan/renderervk_utils.h"

namespace rx
{
class BufferVk;

class VertexArrayVk : public VertexArrayImpl
{
  public:
    VertexArrayVk(const gl::VertexArrayState &state);
    ~VertexArrayVk() override;

    void destroy(const gl::Context *context) override;

    void syncState(const gl::Context *context,
                   const gl::VertexArray::DirtyBits &dirtyBits) override;

    const std::vector<VkBuffer> &getCurrentVertexBufferHandlesCache() const;

    void updateCurrentBufferSerials(const gl::AttributesMask &activeAttribsMask, Serial serial);

    void invalidateVertexDescriptions();
    void updateVertexDescriptions(const gl::Context *context);

    const std::vector<VkVertexInputBindingDescription> &getVertexBindingDescs() const;
    const std::vector<VkVertexInputAttributeDescription> &getVertexAttribDescs() const;

  private:
    std::vector<VkBuffer> mCurrentVertexBufferHandlesCache;
    std::vector<BufferVk *> mCurrentVkBuffersCache;

    // Keep a cache of binding and attribute descriptions for easy pipeline updates.
    // TODO(jmadill): Update this when we support pipeline caching.
    bool mCurrentVertexDescsValid;
    std::vector<VkVertexInputBindingDescription> mCurrentVertexBindingDescs;
    std::vector<VkVertexInputAttributeDescription> mCurrentVertexAttribDescs;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
