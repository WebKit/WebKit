//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PersistentCommandPool.h:
//    Defines the class interface for PersistentCommandBuffer
//

#ifndef LIBANGLE_RENDERER_VULKAN_PERSISTENTCOMMANDPOOL_H_
#define LIBANGLE_RENDERER_VULKAN_PERSISTENTCOMMANDPOOL_H_

#include <vector>

#include "libANGLE/renderer/vulkan/vk_utils.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

namespace rx
{

namespace vk
{

class PersistentCommandPool final
{
  public:
    PersistentCommandPool();
    ~PersistentCommandPool();

    void destroy(VkDevice device);
    angle::Result init(vk::Context *context, uint32_t queueFamilyIndex);

    angle::Result allocate(vk::Context *context, vk::PrimaryCommandBuffer *commandBufferOut);
    angle::Result collect(vk::Context *context, vk::PrimaryCommandBuffer &&buffer);

    bool valid() const { return mCommandPool.valid(); }

  private:
    angle::Result allocateCommandBuffer(vk::Context *context);

    std::vector<vk::PrimaryCommandBuffer> mFreeBuffers;

    vk::CommandPool mCommandPool;

    static const int kInitBufferNum = 2;
};

}  // namespace vk

}  // namespace rx

#endif
