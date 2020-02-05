//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackVk.cpp:
//    Implements the class methods for TransformFeedbackVk.
//

#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"

#include "libANGLE/Context.h"
#include "libANGLE/Query.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"

#include "common/debug.h"

namespace rx
{

TransformFeedbackVk::TransformFeedbackVk(const gl::TransformFeedbackState &state)
    : TransformFeedbackImpl(state), mRebindTransformFeedbackBuffer(false)
{
    mCounterBufferHandles.fill(0);
}

TransformFeedbackVk::~TransformFeedbackVk() {}

void TransformFeedbackVk::onDestroy(const gl::Context *context)
{
    RendererVk *rendererVk = vk::GetImpl(context)->getRenderer();

    for (vk::BufferHelper &bufferHelper : mCounterBuffer)
    {
        bufferHelper.release(rendererVk);
    }
}

angle::Result TransformFeedbackVk::begin(const gl::Context *context,
                                         gl::PrimitiveMode primitiveMode)
{
    ContextVk *contextVk = vk::GetImpl(context);

    contextVk->onTransformFeedbackStateChanged();

    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mRebindTransformFeedbackBuffer = true;
    }

    onTransformFeedbackStateChanged(context);

    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::end(const gl::Context *context)
{
    // If there's an active transform feedback query, accumulate the primitives drawn.
    const gl::State &glState = context->getState();
    gl::Query *transformFeedbackQuery =
        glState.getActiveQuery(gl::QueryType::TransformFeedbackPrimitivesWritten);

    if (transformFeedbackQuery)
    {
        vk::GetImpl(transformFeedbackQuery)->onTransformFeedbackEnd(context);
    }

    vk::GetImpl(context)->onTransformFeedbackStateChanged();

    onTransformFeedbackStateChanged(context);

    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::pause(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    contextVk->onTransformFeedbackStateChanged();

    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        // We need to create new commandGraphNode to perform transform feedback pause/resume
        // becasue vkCmdBegin/EndTransformFeedback can be placed once per commandGraphNode.
        onTransformFeedbackStateChanged(context);
    }

    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::resume(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    contextVk->onTransformFeedbackStateChanged();

    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        // We need to create new commandGraphNode to perform transform feedback pause/resume
        // becasue vkCmdBegin/EndTransformFeedback can be placed once per commandGraphNode.
        onTransformFeedbackStateChanged(context);
    }

    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::bindIndexedBuffer(
    const gl::Context *context,
    size_t index,
    const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        // Save xfb buffer state
        mTransformFeedbackBufferRange.offsets[index] = binding.getOffset();
        mTransformFeedbackBufferRange.sizes[index] =
            (binding.getSize()) ? binding.getSize() : VK_WHOLE_SIZE;
        mRebindTransformFeedbackBuffer = true;

        if (mCounterBufferHandles[index] == 0)
        {
            VkBufferCreateInfo createInfo = {};
            createInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            createInfo.size               = 16;
            createInfo.usage       = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
            createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            ANGLE_TRY(mCounterBuffer[index].init(contextVk, createInfo,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

            mCounterBufferHandles[index] = mCounterBuffer[index].getBuffer().getHandle();
        }

        // Set dirty bit for update xfb buffer
        contextVk->invalidateCurrentTransformFeedbackBuffers();
    }
    else if (contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        RendererVk *rendererVk = vk::GetImpl(context)->getRenderer();
        const VkDeviceSize offsetAlignment =
            rendererVk->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;

        // Make sure there's no possible under/overflow with binding size.
        static_assert(sizeof(VkDeviceSize) >= sizeof(binding.getSize()), "VkDeviceSize too small");

        mTransformFeedbackBufferRange.offsets[index] = binding.getOffset();
        mTransformFeedbackBufferRange.sizes[index]   = gl::GetBoundBufferAvailableSize(binding);

        // Set the offset as close as possible to the requested offset while remaining aligned.
        mTransformFeedbackBufferRange.alignedOffsets[index] =
            (mTransformFeedbackBufferRange.offsets[index] / offsetAlignment) * offsetAlignment;

        // Make sure the transform feedback buffers are bound to the program descriptor sets.
        contextVk->invalidateCurrentTransformFeedbackBuffers();
    }
    return angle::Result::Continue;
}

void TransformFeedbackVk::updateDescriptorSetLayout(
    ContextVk *contextVk,
    const gl::ProgramState &programState,
    vk::DescriptorSetLayoutDesc *descSetLayoutOut) const
{
    if (!contextVk->getFeatures().emulateTransformFeedback.enabled)
        return;

    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    for (uint32_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        descSetLayoutOut->update(kXfbBindingIndexStart + bufferIndex,
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
    }
}

void TransformFeedbackVk::addFramebufferDependency(ContextVk *contextVk,
                                                   const gl::ProgramState &programState,
                                                   vk::FramebufferHelper *framebuffer) const
{
    const std::vector<gl::OffsetBindingPointer<gl::Buffer>> &xfbBuffers =
        mState.getIndexedBuffers();
    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);
    ASSERT(programState.getTransformFeedbackBufferMode() != GL_INTERLEAVED_ATTRIBS ||
           xfbBufferCount == 1);

    VkAccessFlags writeAccessType = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
    if (!contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        writeAccessType = VK_ACCESS_SHADER_WRITE_BIT;
    }

    // Set framebuffer dependent to the transform feedback buffers.  This is especially done
    // separately from |updateDescriptorSet|, to avoid introducing unnecessary buffer barriers
    // every time the descriptor set is updated (which, as the set is shared with default uniforms,
    // could get updated frequently).
    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding = xfbBuffers[bufferIndex];
        gl::Buffer *buffer                                        = bufferBinding.get();
        ASSERT(buffer != nullptr);

        vk::BufferHelper &bufferHelper = vk::GetImpl(buffer)->getBuffer();
        bufferHelper.onWrite(contextVk, framebuffer, writeAccessType);
    }
}

void TransformFeedbackVk::initDescriptorSet(ContextVk *contextVk,
                                            size_t xfbBufferCount,
                                            vk::BufferHelper *emptyBuffer,
                                            VkDescriptorSet descSet) const
{
    if (!contextVk->getFeatures().emulateTransformFeedback.enabled)
        return;

    std::array<VkDescriptorBufferInfo, gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS>
        descriptorBufferInfo;

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[bufferIndex];
        bufferInfo.buffer                  = emptyBuffer->getBuffer().getHandle();
        bufferInfo.offset                  = 0;
        bufferInfo.range                   = VK_WHOLE_SIZE;
    }

    writeDescriptorSet(contextVk, xfbBufferCount, descriptorBufferInfo.data(), descSet);
}

void TransformFeedbackVk::updateDescriptorSet(ContextVk *contextVk,
                                              const gl::ProgramState &programState,
                                              VkDescriptorSet descSet) const
{
    if (!contextVk->getFeatures().emulateTransformFeedback.enabled)
        return;

    const std::vector<gl::OffsetBindingPointer<gl::Buffer>> &xfbBuffers =
        mState.getIndexedBuffers();
    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);
    ASSERT(programState.getTransformFeedbackBufferMode() != GL_INTERLEAVED_ATTRIBS ||
           xfbBufferCount == 1);

    std::array<VkDescriptorBufferInfo, gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS>
        descriptorBufferInfo;

    // Update buffer descriptor binding info for output buffers
    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[bufferIndex];

        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding = xfbBuffers[bufferIndex];
        gl::Buffer *buffer                                        = bufferBinding.get();
        ASSERT(buffer != nullptr);

        vk::BufferHelper &bufferHelper = vk::GetImpl(buffer)->getBuffer();

        bufferInfo.buffer = bufferHelper.getBuffer().getHandle();
        bufferInfo.offset = mTransformFeedbackBufferRange.alignedOffsets[bufferIndex];
        bufferInfo.range  = mTransformFeedbackBufferRange.sizes[bufferIndex] +
                           (mTransformFeedbackBufferRange.offsets[bufferIndex] -
                            mTransformFeedbackBufferRange.alignedOffsets[bufferIndex]);
        bufferInfo.range = (bufferInfo.range == 0) ? bufferHelper.getSize() : bufferInfo.range;
    }

    writeDescriptorSet(contextVk, xfbBufferCount, descriptorBufferInfo.data(), descSet);
}

void TransformFeedbackVk::getBufferOffsets(ContextVk *contextVk,
                                           const gl::ProgramState &programState,
                                           GLint drawCallFirstVertex,
                                           int32_t *offsetsOut,
                                           size_t offsetsSize) const
{
    if (!contextVk->getFeatures().emulateTransformFeedback.enabled)
        return;

    GLsizeiptr verticesDrawn = mState.getVerticesDrawn();
    const std::vector<GLsizei> &bufferStrides =
        mState.getBoundProgram()->getTransformFeedbackStrides();
    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);

    // The caller should make sure the offsets array has enough space.  The maximum possible
    // number of outputs is gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS.
    ASSERT(offsetsSize >= xfbBufferCount);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        int64_t offsetFromDescriptor =
            static_cast<int64_t>(mTransformFeedbackBufferRange.offsets[bufferIndex] -
                                 mTransformFeedbackBufferRange.alignedOffsets[bufferIndex]);
        int64_t drawCallVertexOffset = static_cast<int64_t>(verticesDrawn) - drawCallFirstVertex;

        int64_t writeOffset =
            (offsetFromDescriptor + drawCallVertexOffset * bufferStrides[bufferIndex]) /
            static_cast<int64_t>(sizeof(uint32_t));

        offsetsOut[bufferIndex] = static_cast<int32_t>(writeOffset);

        // Assert on overflow.  For now, support transform feedback up to 2GB.
        ASSERT(offsetsOut[bufferIndex] == writeOffset);
    }
}

void TransformFeedbackVk::onTransformFeedbackStateChanged(const gl::Context *context)
{
    // Currently, we don't handle resources switching from read-only to writable and back correctly.
    // In the case of transform feedback, the attached buffers can switch between being written by
    // transform feedback and being read as a vertex array buffer.  For now, we'll end the render
    // pass every time transform feedback is started or ended to work around this issue temporarily.
    //
    // TODO(syoussefi): detect changes to buffer usage (e.g. as transform feedback output, vertex
    // or index data etc) in the front end and notify the backend.  A new node should be created
    // only on such changes.  http://anglebug.com/3205
    ContextVk *contextVk               = vk::GetImpl(context);
    FramebufferVk *framebufferVk       = vk::GetImpl(context->getState().getDrawFramebuffer());
    vk::FramebufferHelper *framebuffer = framebufferVk->getFramebuffer();

    framebuffer->updateCurrentAccessNodes();
    if (framebuffer->hasStartedRenderPass())
    {
        framebuffer->finishCurrentCommands(contextVk);
    }
}

void TransformFeedbackVk::writeDescriptorSet(ContextVk *contextVk,
                                             size_t xfbBufferCount,
                                             VkDescriptorBufferInfo *pBufferInfo,
                                             VkDescriptorSet descSet) const
{
    VkDevice device = contextVk->getDevice();

    VkWriteDescriptorSet writeDescriptorInfo = {};
    writeDescriptorInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorInfo.dstSet               = descSet;
    writeDescriptorInfo.dstBinding           = kXfbBindingIndexStart;
    writeDescriptorInfo.dstArrayElement      = 0;
    writeDescriptorInfo.descriptorCount      = static_cast<uint32_t>(xfbBufferCount);
    writeDescriptorInfo.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorInfo.pImageInfo           = nullptr;
    writeDescriptorInfo.pBufferInfo          = pBufferInfo;
    writeDescriptorInfo.pTexelBufferView     = nullptr;

    vkUpdateDescriptorSets(device, 1, &writeDescriptorInfo, 0, nullptr);
}

}  // namespace rx
