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
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"

#include "common/debug.h"

namespace rx
{
TransformFeedbackVk::TransformFeedbackVk(const gl::TransformFeedbackState &state)
    : TransformFeedbackImpl(state),
      mRebindTransformFeedbackBuffer(false),
      mBufferHelpers{},
      mBufferHandles{},
      mBufferOffsets{},
      mBufferSizes{},
      mCounterBufferHandles{},
      mCounterBufferOffsets{}
{
    for (angle::SubjectIndex bufferIndex = 0;
         bufferIndex < gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; ++bufferIndex)
    {
        mBufferObserverBindings.emplace_back(this, bufferIndex);
    }
}

TransformFeedbackVk::~TransformFeedbackVk() {}

void TransformFeedbackVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk   = vk::GetImpl(context);
    RendererVk *rendererVk = contextVk->getRenderer();

    releaseCounterBuffers(rendererVk);
}

void TransformFeedbackVk::releaseCounterBuffers(RendererVk *renderer)
{
    for (vk::BufferHelper &bufferHelper : mCounterBufferHelpers)
    {
        bufferHelper.release(renderer);
    }
    for (VkBuffer &buffer : mCounterBufferHandles)
    {
        buffer = VK_NULL_HANDLE;
    }
    for (VkDeviceSize &offset : mCounterBufferOffsets)
    {
        offset = 0;
    }
}

void TransformFeedbackVk::initializeXFBBuffersDesc(ContextVk *contextVk, size_t xfbBufferCount)
{
    mXFBBuffersDesc.reset();
    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        const gl::OffsetBindingPointer<gl::Buffer> &binding = mState.getIndexedBuffer(bufferIndex);
        ASSERT(binding.get());

        BufferVk *bufferVk = vk::GetImpl(binding.get());

        if (bufferVk->isBufferValid())
        {
            mBufferHelpers[bufferIndex] = &bufferVk->getBuffer();
            mBufferOffsets[bufferIndex] =
                binding.getOffset() + mBufferHelpers[bufferIndex]->getOffset();
            mBufferSizes[bufferIndex] = gl::GetBoundBufferAvailableSize(binding);
            mBufferObserverBindings[bufferIndex].bind(bufferVk);
        }
        else
        {
            // This can happen in error conditions.
            vk::BufferHelper &nullBuffer = contextVk->getEmptyBuffer();
            mBufferHelpers[bufferIndex]  = &nullBuffer;
            mBufferOffsets[bufferIndex]  = 0;
            mBufferSizes[bufferIndex]    = nullBuffer.getSize();
            mBufferObserverBindings[bufferIndex].reset();
        }

        mXFBBuffersDesc.updateTransformFeedbackBuffer(
            bufferIndex, mBufferHelpers[bufferIndex]->getBufferSerial(),
            mBufferOffsets[bufferIndex]);
    }
}

angle::Result TransformFeedbackVk::begin(const gl::Context *context,
                                         gl::PrimitiveMode primitiveMode)
{
    ContextVk *contextVk = vk::GetImpl(context);

    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);
    size_t xfbBufferCount = executable->getTransformFeedbackBufferCount();

    initializeXFBBuffersDesc(contextVk, xfbBufferCount);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        mBufferHandles[bufferIndex] = mBufferHelpers[bufferIndex]->getBuffer().getHandle();
        if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
        {
            if (mCounterBufferHandles[bufferIndex] == VK_NULL_HANDLE)
            {
                vk::BufferHelper &bufferHelper = mCounterBufferHelpers[bufferIndex];
                ANGLE_TRY(bufferHelper.initSuballocation(
                    contextVk, contextVk->getRenderer()->getDeviceLocalMemoryTypeIndex(), 16,
                    GetDefaultBufferAlignment(contextVk->getRenderer())));
                mCounterBufferHandles[bufferIndex] = bufferHelper.getBuffer().getHandle();
                mCounterBufferOffsets[bufferIndex] = bufferHelper.getOffset();
            }
        }
    }

    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mRebindTransformFeedbackBuffer = true;
    }

    return contextVk->onBeginTransformFeedback(xfbBufferCount, mBufferHelpers,
                                               mCounterBufferHelpers);
}

angle::Result TransformFeedbackVk::end(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // If there's an active transform feedback query, accumulate the primitives drawn.
    const gl::State &glState = context->getState();
    gl::Query *transformFeedbackQuery =
        glState.getActiveQuery(gl::QueryType::TransformFeedbackPrimitivesWritten);

    if (transformFeedbackQuery && contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        vk::GetImpl(transformFeedbackQuery)->onTransformFeedbackEnd(mState.getPrimitivesDrawn());
    }

    for (angle::ObserverBinding &bufferBinding : mBufferObserverBindings)
    {
        bufferBinding.reset();
    }

    contextVk->onEndTransformFeedback();

    releaseCounterBuffers(contextVk->getRenderer());

    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::pause(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        // Bind the empty buffer until we resume.
        const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
        ASSERT(executable);
        size_t xfbBufferCount = executable->getTransformFeedbackBufferCount();

        const vk::BufferHelper &emptyBuffer = contextVk->getEmptyBuffer();

        for (size_t xfbIndex = 0; xfbIndex < xfbBufferCount; ++xfbIndex)
        {
            mXFBBuffersDesc.updateTransformFeedbackBuffer(xfbIndex, emptyBuffer.getBufferSerial(),
                                                          0);
        }
    }

    return contextVk->onPauseTransformFeedback();
}

angle::Result TransformFeedbackVk::resume(const gl::Context *context)
{
    ContextVk *contextVk                    = vk::GetImpl(context);
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);
    size_t xfbBufferCount = executable->getTransformFeedbackBufferCount();

    if (contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        initializeXFBBuffersDesc(contextVk, xfbBufferCount);
    }

    return contextVk->onBeginTransformFeedback(xfbBufferCount, mBufferHelpers,
                                               mCounterBufferHelpers);
}

angle::Result TransformFeedbackVk::bindIndexedBuffer(
    const gl::Context *context,
    size_t index,
    const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // Make sure the transform feedback buffers are bound to the program descriptor sets.
    contextVk->invalidateCurrentTransformFeedbackBuffers();

    return angle::Result::Continue;
}

void TransformFeedbackVk::updateDescriptorSetLayout(
    ContextVk *contextVk,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    size_t xfbBufferCount,
    vk::DescriptorSetLayoutDesc *descSetLayoutOut) const
{
    if (!contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        return;
    }

    for (uint32_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        const std::string bufferName = GetXfbBufferName(bufferIndex);
        const ShaderInterfaceVariableInfo &info =
            variableInfoMap.get(gl::ShaderType::Vertex, bufferName);

        ASSERT(info.binding != std::numeric_limits<uint32_t>::max());

        descSetLayoutOut->update(info.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    }
}

void TransformFeedbackVk::initDescriptorSet(vk::Context *context,
                                            UpdateDescriptorSetsBuilder *updateBuilder,
                                            vk::BufferHelper *emptyBuffer,
                                            const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                            size_t xfbBufferCount,
                                            VkDescriptorSet descSet) const
{
    if (!context->getRenderer()->getFeatures().emulateTransformFeedback.enabled)
    {
        return;
    }

    VkDescriptorBufferInfo *descriptorBufferInfo =
        updateBuilder->allocDescriptorBufferInfos(xfbBufferCount);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[bufferIndex];
        bufferInfo.buffer                  = emptyBuffer->getBuffer().getHandle();
        bufferInfo.offset                  = emptyBuffer->getOffset();
        bufferInfo.range                   = emptyBuffer->getSize();
    }

    writeDescriptorSet(context, updateBuilder, variableInfoMap, xfbBufferCount,
                       descriptorBufferInfo, descSet);
}

void TransformFeedbackVk::updateDescriptorSet(vk::Context *context,
                                              UpdateDescriptorSetsBuilder *updateBuilder,
                                              const gl::ProgramExecutable &executable,
                                              const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                              VkDescriptorSet descSet) const
{
    RendererVk *renderer = context->getRenderer();

    if (!renderer->getFeatures().emulateTransformFeedback.enabled)
    {
        return;
    }

    size_t xfbBufferCount = executable.getTransformFeedbackBufferCount();
    const VkDeviceSize offsetAlignment =
        renderer->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;

    ASSERT(xfbBufferCount > 0);
    ASSERT(executable.getTransformFeedbackBufferMode() != GL_INTERLEAVED_ATTRIBS ||
           xfbBufferCount == 1);

    VkDescriptorBufferInfo *descriptorBufferInfo =
        updateBuilder->allocDescriptorBufferInfos(xfbBufferCount);

    // Update buffer descriptor binding info for output buffers
    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[bufferIndex];

        bufferInfo.buffer = mBufferHandles[bufferIndex];
        // Set the offset as close as possible to the requested offset while remaining aligned.
        bufferInfo.offset = (mBufferOffsets[bufferIndex] / offsetAlignment) * offsetAlignment;
        bufferInfo.range =
            mBufferSizes[bufferIndex] + (mBufferOffsets[bufferIndex] - bufferInfo.offset);
        ASSERT(bufferInfo.range != 0);
    }

    writeDescriptorSet(context, updateBuilder, variableInfoMap, xfbBufferCount,
                       descriptorBufferInfo, descSet);
}

void TransformFeedbackVk::getBufferOffsets(ContextVk *contextVk,
                                           GLint drawCallFirstVertex,
                                           int32_t *offsetsOut,
                                           size_t offsetsSize) const
{
    if (!contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        return;
    }

    GLsizeiptr verticesDrawn                = mState.getVerticesDrawn();
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);
    const std::vector<GLsizei> &bufferStrides = executable->getTransformFeedbackStrides();
    size_t xfbBufferCount                     = executable->getTransformFeedbackBufferCount();
    const VkDeviceSize offsetAlignment        = contextVk->getRenderer()
                                             ->getPhysicalDeviceProperties()
                                             .limits.minStorageBufferOffsetAlignment;

    ASSERT(xfbBufferCount > 0);

    // The caller should make sure the offsets array has enough space.  The maximum possible
    // number of outputs is gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS.
    ASSERT(offsetsSize >= xfbBufferCount);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        int64_t offsetFromDescriptor =
            static_cast<int64_t>(mBufferOffsets[bufferIndex] % offsetAlignment);
        int64_t drawCallVertexOffset = static_cast<int64_t>(verticesDrawn) - drawCallFirstVertex;

        int64_t writeOffset =
            (offsetFromDescriptor + drawCallVertexOffset * bufferStrides[bufferIndex]) /
            static_cast<int64_t>(sizeof(uint32_t));

        offsetsOut[bufferIndex] = static_cast<int32_t>(writeOffset);

        // Assert on overflow.  For now, support transform feedback up to 2GB.
        ASSERT(offsetsOut[bufferIndex] == writeOffset);
    }
}

void TransformFeedbackVk::onSubjectStateChange(angle::SubjectIndex index,
                                               angle::SubjectMessage message)
{
    if (message == angle::SubjectMessage::BufferVkStorageChanged)
    {
        ASSERT(index < mBufferObserverBindings.size());
        const gl::OffsetBindingPointer<gl::Buffer> &binding = mState.getIndexedBuffer(index);
        ASSERT(binding.get());

        BufferVk *bufferVk = vk::GetImpl(binding.get());

        ASSERT(bufferVk->isBufferValid());
        mBufferHelpers[index] = &bufferVk->getBuffer();
        mBufferOffsets[index] = binding.getOffset() + mBufferHelpers[index]->getOffset();
        mBufferSizes[index]   = gl::GetBoundBufferAvailableSize(binding);
        mBufferObserverBindings[index].bind(bufferVk);

        mXFBBuffersDesc.updateTransformFeedbackBuffer(
            index, mBufferHelpers[index]->getBufferSerial(), mBufferOffsets[index]);

        mBufferHandles[index] = mBufferHelpers[index]->getBuffer().getHandle();
    }
}

void TransformFeedbackVk::writeDescriptorSet(vk::Context *context,
                                             UpdateDescriptorSetsBuilder *updateBuilder,
                                             const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                             size_t xfbBufferCount,
                                             VkDescriptorBufferInfo *bufferInfo,
                                             VkDescriptorSet descSet) const
{
    ASSERT(context->getRenderer()->getFeatures().emulateTransformFeedback.enabled);

    const std::string bufferName = GetXfbBufferName(0);
    const ShaderInterfaceVariableInfo &info =
        variableInfoMap.get(gl::ShaderType::Vertex, bufferName);

    VkWriteDescriptorSet &writeDescriptorInfo = updateBuilder->allocWriteDescriptorSet();
    writeDescriptorInfo.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorInfo.dstSet                = descSet;
    writeDescriptorInfo.dstBinding            = info.binding;
    writeDescriptorInfo.dstArrayElement       = 0;
    writeDescriptorInfo.descriptorCount       = static_cast<uint32_t>(xfbBufferCount);
    writeDescriptorInfo.descriptorType        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorInfo.pImageInfo            = nullptr;
    writeDescriptorInfo.pBufferInfo           = bufferInfo;
    writeDescriptorInfo.pTexelBufferView      = nullptr;
}

}  // namespace rx
