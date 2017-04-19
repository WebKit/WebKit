//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArray11:
//   Implementation of rx::VertexArray11.
//

#include "libANGLE/renderer/d3d/d3d11/VertexArray11.h"

#include "common/bitset_utils.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"

using namespace angle;

namespace rx
{

VertexArray11::VertexArray11(const gl::VertexArrayState &data)
    : VertexArrayImpl(data),
      mAttributeStorageTypes(data.getMaxAttribs(), VertexStorageType::CURRENT_VALUE),
      mTranslatedAttribs(data.getMaxAttribs()),
      mCurrentBuffers(data.getMaxAttribs())
{
    for (size_t attribIndex = 0; attribIndex < mCurrentBuffers.size(); ++attribIndex)
    {
        mOnBufferDataDirty.emplace_back(this, static_cast<uint32_t>(attribIndex));
    }
}

VertexArray11::~VertexArray11()
{
    for (auto &buffer : mCurrentBuffers)
    {
        if (buffer.get())
        {
            buffer.set(nullptr);
        }
    }
}

void VertexArray11::syncState(ContextImpl *contextImpl, const gl::VertexArray::DirtyBits &dirtyBits)
{
    for (auto dirtyBit : angle::IterateBitSet(dirtyBits))
    {
        if (dirtyBit == gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER)
            continue;

        size_t index = gl::VertexArray::GetAttribIndex(dirtyBit);
        // TODO(jiawei.shao@intel.com): Vertex Attrib Bindings
        ASSERT(index == mData.getBindingIndexFromAttribIndex(index));
        mAttribsToUpdate.set(index);
    }
}

void VertexArray11::flushAttribUpdates(const gl::State &state)
{
    const gl::Program *program  = state.getProgram();
    const auto &activeLocations = program->getActiveAttribLocationsMask();

    if (mAttribsToUpdate.any())
    {
        // Skip attrib locations the program doesn't use.
        const auto &activeToUpdate = (mAttribsToUpdate & activeLocations);

        for (auto toUpdateIndex : angle::IterateBitSet(activeToUpdate))
        {
            mAttribsToUpdate.reset(toUpdateIndex);
            updateVertexAttribStorage(toUpdateIndex);
        }
    }
}

void VertexArray11::updateVertexAttribStorage(size_t attribIndex)
{
    const auto &attrib = mData.getVertexAttribute(attribIndex);
    const auto &binding = mData.getBindingFromAttribIndex(attribIndex);

    // Note: having an unchanged storage type doesn't mean the attribute is clean.
    auto oldStorageType = mAttributeStorageTypes[attribIndex];
    auto newStorageType = ClassifyAttributeStorage(attrib, binding);

    mAttributeStorageTypes[attribIndex] = newStorageType;

    if (newStorageType == VertexStorageType::DYNAMIC)
    {
        if (oldStorageType != VertexStorageType::DYNAMIC)
        {
            // Sync dynamic attribs in a different set.
            mAttribsToTranslate.reset(attribIndex);
            mDynamicAttribsMask.set(attribIndex);
        }
    }
    else
    {
        mAttribsToTranslate.set(attribIndex);

        if (oldStorageType == VertexStorageType::DYNAMIC)
        {
            ASSERT(mDynamicAttribsMask[attribIndex]);
            mDynamicAttribsMask.reset(attribIndex);
        }
    }

    gl::Buffer *oldBufferGL = mCurrentBuffers[attribIndex].get();
    gl::Buffer *newBufferGL = binding.buffer.get();
    Buffer11 *oldBuffer11   = oldBufferGL ? GetImplAs<Buffer11>(oldBufferGL) : nullptr;
    Buffer11 *newBuffer11   = newBufferGL ? GetImplAs<Buffer11>(newBufferGL) : nullptr;

    if (oldBuffer11 != newBuffer11 || oldStorageType != newStorageType)
    {
        // Note that for static callbacks, promotion to a static buffer from a dynamic buffer means
        // we need to tag dynamic buffers with static callbacks.
        BroadcastChannel<> *newChannel = nullptr;
        if (newBuffer11 != nullptr)
        {
            switch (newStorageType)
            {
                case VertexStorageType::DIRECT:
                    newChannel = newBuffer11->getDirectBroadcastChannel();
                    break;
                case VertexStorageType::STATIC:
                case VertexStorageType::DYNAMIC:
                    newChannel = newBuffer11->getStaticBroadcastChannel();
                    break;
                default:
                    break;
            }
        }
        mOnBufferDataDirty[attribIndex].bind(newChannel);
        mCurrentBuffers[attribIndex] = binding.buffer;
    }
}

bool VertexArray11::hasDynamicAttrib(const gl::State &state)
{
    flushAttribUpdates(state);
    return mDynamicAttribsMask.any();
}

gl::Error VertexArray11::updateDirtyAndDynamicAttribs(VertexDataManager *vertexDataManager,
                                                      const gl::State &state,
                                                      GLint start,
                                                      GLsizei count,
                                                      GLsizei instances)
{
    flushAttribUpdates(state);

    const gl::Program *program  = state.getProgram();
    const auto &activeLocations = program->getActiveAttribLocationsMask();
    const auto &attribs         = mData.getVertexAttributes();
    const auto &bindings        = mData.getVertexBindings();

    if (mAttribsToTranslate.any())
    {
        // Skip attrib locations the program doesn't use, saving for the next frame.
        const auto &dirtyActiveAttribs = (mAttribsToTranslate & activeLocations);

        for (auto dirtyAttribIndex : angle::IterateBitSet(dirtyActiveAttribs))
        {
            mAttribsToTranslate.reset(dirtyAttribIndex);

            auto *translatedAttrib = &mTranslatedAttribs[dirtyAttribIndex];
            const auto &currentValue =
                state.getVertexAttribCurrentValue(static_cast<unsigned int>(dirtyAttribIndex));

            // Record basic attrib info
            translatedAttrib->attribute = &attribs[dirtyAttribIndex];
            translatedAttrib->binding   = &bindings[translatedAttrib->attribute->bindingIndex];
            translatedAttrib->currentValueType = currentValue.Type;
            translatedAttrib->divisor          = translatedAttrib->binding->divisor;

            switch (mAttributeStorageTypes[dirtyAttribIndex])
            {
                case VertexStorageType::DIRECT:
                    VertexDataManager::StoreDirectAttrib(translatedAttrib);
                    break;
                case VertexStorageType::STATIC:
                {
                    ANGLE_TRY(VertexDataManager::StoreStaticAttrib(translatedAttrib));
                    break;
                }
                case VertexStorageType::CURRENT_VALUE:
                    // Current value attribs are managed by the StateManager11.
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
        }
    }

    if (mDynamicAttribsMask.any())
    {
        auto activeDynamicAttribs = (mDynamicAttribsMask & activeLocations);

        for (auto dynamicAttribIndex : angle::IterateBitSet(activeDynamicAttribs))
        {
            auto *dynamicAttrib = &mTranslatedAttribs[dynamicAttribIndex];
            const auto &currentValue =
                state.getVertexAttribCurrentValue(static_cast<unsigned int>(dynamicAttribIndex));

            // Record basic attrib info
            dynamicAttrib->attribute        = &attribs[dynamicAttribIndex];
            dynamicAttrib->binding          = &bindings[dynamicAttrib->attribute->bindingIndex];
            dynamicAttrib->currentValueType = currentValue.Type;
            dynamicAttrib->divisor          = dynamicAttrib->binding->divisor;
        }

        return vertexDataManager->storeDynamicAttribs(&mTranslatedAttribs, activeDynamicAttribs,
                                                      start, count, instances);
    }

    return gl::NoError();
}

const std::vector<TranslatedAttribute> &VertexArray11::getTranslatedAttribs() const
{
    return mTranslatedAttribs;
}

void VertexArray11::signal(uint32_t channelID)
{
    ASSERT(mAttributeStorageTypes[channelID] != VertexStorageType::CURRENT_VALUE);

    // This can change a buffer's storage, we'll need to re-check.
    mAttribsToUpdate.set(channelID);
}

void VertexArray11::clearDirtyAndPromoteDynamicAttribs(const gl::State &state, GLsizei count)
{
    const gl::Program *program  = state.getProgram();
    const auto &activeLocations = program->getActiveAttribLocationsMask();
    mAttribsToUpdate &= ~activeLocations;

    // Promote to static after we clear the dirty attributes, otherwise we can lose dirtyness.
    auto activeDynamicAttribs = (mDynamicAttribsMask & activeLocations);
    VertexDataManager::PromoteDynamicAttribs(mTranslatedAttribs, activeDynamicAttribs, count);
}
}  // namespace rx
