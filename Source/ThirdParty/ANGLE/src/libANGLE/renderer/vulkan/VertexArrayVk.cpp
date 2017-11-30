//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayVk.cpp:
//    Implements the class methods for VertexArrayVk.
//

#include "libANGLE/renderer/vulkan/VertexArrayVk.h"

#include "common/debug.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/formatutilsvk.h"

namespace rx
{

VertexArrayVk::VertexArrayVk(const gl::VertexArrayState &state)
    : VertexArrayImpl(state),
      mCurrentVertexBufferHandlesCache(state.getMaxAttribs(), VK_NULL_HANDLE),
      mCurrentVkBuffersCache(state.getMaxAttribs(), nullptr),
      mCurrentVertexDescsValid(false)
{
    mCurrentVertexBindingDescs.reserve(state.getMaxAttribs());
    mCurrentVertexAttribDescs.reserve(state.getMaxAttribs());
}

VertexArrayVk::~VertexArrayVk()
{
}

void VertexArrayVk::destroy(const gl::Context *context)
{
}

void VertexArrayVk::syncState(const gl::Context *context,
                              const gl::VertexArray::DirtyBits &dirtyBits)
{
    ASSERT(dirtyBits.any());

    // Invalidate current pipeline.
    // TODO(jmadill): Use pipeline cache.
    ContextVk *contextVk = vk::GetImpl(context);
    contextVk->invalidateCurrentPipeline();

    // Invalidate the vertex descriptions.
    invalidateVertexDescriptions();

    // Rebuild current attribute buffers cache. This will fail horribly if the buffer changes.
    // TODO(jmadill): Handle buffer storage changes.
    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    for (auto dirtyBit : dirtyBits)
    {
        if (dirtyBit == gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER)
            continue;

        size_t attribIndex = gl::VertexArray::GetVertexIndexFromDirtyBit(dirtyBit);

        const auto &attrib  = attribs[attribIndex];
        const auto &binding = bindings[attrib.bindingIndex];

        if (attrib.enabled)
        {
            gl::Buffer *bufferGL = binding.getBuffer().get();

            if (bufferGL)
            {
                BufferVk *bufferVk                            = vk::GetImpl(bufferGL);
                mCurrentVkBuffersCache[attribIndex]           = bufferVk;
                mCurrentVertexBufferHandlesCache[attribIndex] = bufferVk->getVkBuffer().getHandle();
            }
            else
            {
                mCurrentVkBuffersCache[attribIndex]           = nullptr;
                mCurrentVertexBufferHandlesCache[attribIndex] = VK_NULL_HANDLE;
            }
        }
        else
        {
            UNIMPLEMENTED();
        }
    }
}

const std::vector<VkBuffer> &VertexArrayVk::getCurrentVertexBufferHandlesCache() const
{
    return mCurrentVertexBufferHandlesCache;
}

void VertexArrayVk::updateCurrentBufferSerials(const gl::AttributesMask &activeAttribsMask,
                                               Serial serial)
{
    for (auto attribIndex : activeAttribsMask)
    {
        mCurrentVkBuffersCache[attribIndex]->setQueueSerial(serial);
    }
}

void VertexArrayVk::invalidateVertexDescriptions()
{
    mCurrentVertexDescsValid = false;
    mCurrentVertexBindingDescs.clear();
    mCurrentVertexAttribDescs.clear();
}

void VertexArrayVk::updateVertexDescriptions(const gl::Context *context)
{
    if (mCurrentVertexDescsValid)
    {
        return;
    }

    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    const gl::Program *programGL = context->getGLState().getProgram();

    for (auto attribIndex : programGL->getActiveAttribLocationsMask())
    {
        const auto &attrib  = attribs[attribIndex];
        const auto &binding = bindings[attrib.bindingIndex];
        if (attrib.enabled)
        {
            VkVertexInputBindingDescription bindingDesc;
            bindingDesc.binding = static_cast<uint32_t>(mCurrentVertexBindingDescs.size());
            bindingDesc.stride  = static_cast<uint32_t>(gl::ComputeVertexAttributeTypeSize(attrib));
            bindingDesc.inputRate = (binding.getDivisor() > 0 ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                              : VK_VERTEX_INPUT_RATE_VERTEX);

            gl::VertexFormatType vertexFormatType = gl::GetVertexFormatType(attrib);

            VkVertexInputAttributeDescription attribDesc;
            attribDesc.binding  = bindingDesc.binding;
            attribDesc.format   = vk::GetNativeVertexFormat(vertexFormatType);
            attribDesc.location = static_cast<uint32_t>(attribIndex);
            attribDesc.offset =
                static_cast<uint32_t>(ComputeVertexAttributeOffset(attrib, binding));

            mCurrentVertexBindingDescs.push_back(bindingDesc);
            mCurrentVertexAttribDescs.push_back(attribDesc);
        }
        else
        {
            UNIMPLEMENTED();
        }
    }

    mCurrentVertexDescsValid = true;
}

const std::vector<VkVertexInputBindingDescription> &VertexArrayVk::getVertexBindingDescs() const
{
    return mCurrentVertexBindingDescs;
}

const std::vector<VkVertexInputAttributeDescription> &VertexArrayVk::getVertexAttribDescs() const
{
    return mCurrentVertexAttribDescs;
}

}  // namespace rx
