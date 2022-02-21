//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_command_buffer_utils:
//    Helpers for secondary command buffer implementations.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_COMMAND_BUFFER_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_COMMAND_BUFFER_UTILS_H_

namespace rx
{
namespace vk
{

// A helper class to track commands recorded to a command buffer.
class CommandBufferCommandTracker
{
  public:
    void onDraw() { ++mRenderPassWriteCommandCount; }
    void onClearAttachments() { ++mRenderPassWriteCommandCount; }
    uint32_t getRenderPassWriteCommandCount() const { return mRenderPassWriteCommandCount; }

    void reset() { *this = CommandBufferCommandTracker{}; }

  private:
    // The number of commands recorded that can modify a render pass attachment, i.e.
    // vkCmdClearAttachment and vkCmdDraw*.  Used to know if a command might have written to an
    // attachment after it was invalidated.
    uint32_t mRenderPassWriteCommandCount = 0;
};

}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VK_COMMAND_BUFFER_UTILS_H_
