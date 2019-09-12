//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackVk.h:
//    Defines the class interface for TransformFeedbackVk, implementing TransformFeedbackImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_TRANSFORMFEEDBACKVK_H_
#define LIBANGLE_RENDERER_VULKAN_TRANSFORMFEEDBACKVK_H_

#include "libANGLE/renderer/TransformFeedbackImpl.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace gl
{
class ProgramState;
}  // namespace gl

namespace rx
{
namespace vk
{
class DescriptorSetLayoutDesc;
}

class TransformFeedbackVk : public TransformFeedbackImpl
{
  public:
    TransformFeedbackVk(const gl::TransformFeedbackState &state);
    ~TransformFeedbackVk() override;

    angle::Result begin(const gl::Context *context, gl::PrimitiveMode primitiveMode) override;
    angle::Result end(const gl::Context *context) override;
    angle::Result pause(const gl::Context *context) override;
    angle::Result resume(const gl::Context *context) override;

    angle::Result bindIndexedBuffer(const gl::Context *context,
                                    size_t index,
                                    const gl::OffsetBindingPointer<gl::Buffer> &binding) override;

    void updateDescriptorSetLayout(const gl::ProgramState &programState,
                                   vk::DescriptorSetLayoutDesc *descSetLayoutOut) const;
    void addFramebufferDependency(ContextVk *contextVk,
                                  const gl::ProgramState &programState,
                                  vk::FramebufferHelper *framebuffer) const;
    void initDescriptorSet(ContextVk *contextVk,
                           size_t xfbBufferCount,
                           vk::BufferHelper *emptyBuffer,
                           VkDescriptorSet descSet) const;
    void updateDescriptorSet(ContextVk *contextVk,
                             const gl::ProgramState &programState,
                             VkDescriptorSet descSet) const;
    void getBufferOffsets(ContextVk *contextVk,
                          const gl::ProgramState &programState,
                          GLint drawCallFirstVertex,
                          int32_t *offsetsOut,
                          size_t offsetsSize) const;

  private:
    void onBeginOrEnd(const gl::Context *context);
    void writeDescriptorSet(ContextVk *contextVk,
                            size_t xfbBufferCount,
                            VkDescriptorBufferInfo *pBufferInfo,
                            VkDescriptorSet descSet) const;

    // Cached buffer properties for faster descriptor set update and offset calculation.
    struct BoundBufferRange
    {
        // Offset as provided by OffsetBindingPointer.
        VkDeviceSize offset = 0;
        // Size as provided by OffsetBindingPointer.
        VkDeviceSize size = 0;
        // Aligned offset usable for VkDescriptorBufferInfo.  This value could be smaller than
        // offset.
        VkDeviceSize alignedOffset = 0;
    };
    gl::TransformFeedbackBuffersArray<BoundBufferRange> mBoundBufferRanges;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_TRANSFORMFEEDBACKVK_H_
