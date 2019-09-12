//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the state class for mananging GLES 3 Vertex Array Objects.
//

#include "libANGLE/VertexArray.h"

#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/VertexArrayImpl.h"

namespace gl
{
namespace
{
bool IsElementArrayBufferSubjectIndex(angle::SubjectIndex subjectIndex)
{
    return (subjectIndex == MAX_VERTEX_ATTRIBS);
}

ANGLE_INLINE ComponentType GetVertexAttributeComponentType(bool pureInteger, VertexAttribType type)
{
    if (pureInteger)
    {
        switch (type)
        {
            case VertexAttribType::Byte:
            case VertexAttribType::Short:
            case VertexAttribType::Int:
                return ComponentType::Int;

            case VertexAttribType::UnsignedByte:
            case VertexAttribType::UnsignedShort:
            case VertexAttribType::UnsignedInt:
                return ComponentType::UnsignedInt;

            default:
                UNREACHABLE();
                return ComponentType::NoType;
        }
    }
    else
    {
        return ComponentType::Float;
    }
}

constexpr angle::SubjectIndex kElementArrayBufferIndex = MAX_VERTEX_ATTRIBS;
}  // namespace

// VertexArrayState implementation.
VertexArrayState::VertexArrayState(VertexArray *vertexArray,
                                   size_t maxAttribs,
                                   size_t maxAttribBindings)
    : mElementArrayBuffer(vertexArray, kElementArrayBufferIndex)
{
    ASSERT(maxAttribs <= maxAttribBindings);

    for (size_t i = 0; i < maxAttribs; i++)
    {
        mVertexAttributes.emplace_back(static_cast<GLuint>(i));
        mVertexBindings.emplace_back(static_cast<GLuint>(i));
    }

    // Initially all attributes start as "client" with no buffer bound.
    mClientMemoryAttribsMask.set();
}

VertexArrayState::~VertexArrayState() {}

bool VertexArrayState::hasEnabledNullPointerClientArray() const
{
    return (mNullPointerClientMemoryAttribsMask & mEnabledAttributesMask).any();
}

AttributesMask VertexArrayState::getBindingToAttributesMask(GLuint bindingIndex) const
{
    ASSERT(bindingIndex < MAX_VERTEX_ATTRIB_BINDINGS);
    return mVertexBindings[bindingIndex].getBoundAttributesMask();
}

// Set an attribute using a new binding.
void VertexArrayState::setAttribBinding(const Context *context,
                                        size_t attribIndex,
                                        GLuint newBindingIndex)
{
    ASSERT(attribIndex < MAX_VERTEX_ATTRIBS && newBindingIndex < MAX_VERTEX_ATTRIB_BINDINGS);

    VertexAttribute &attrib = mVertexAttributes[attribIndex];

    // Update the binding-attribute map.
    const GLuint oldBindingIndex = attrib.bindingIndex;
    ASSERT(oldBindingIndex != newBindingIndex);

    VertexBinding &oldBinding = mVertexBindings[oldBindingIndex];
    VertexBinding &newBinding = mVertexBindings[newBindingIndex];

    ASSERT(oldBinding.getBoundAttributesMask().test(attribIndex) &&
           !newBinding.getBoundAttributesMask().test(attribIndex));

    oldBinding.resetBoundAttribute(attribIndex);
    newBinding.setBoundAttribute(attribIndex);

    // Set the attribute using the new binding.
    attrib.bindingIndex = newBindingIndex;

    if (context->isBufferAccessValidationEnabled())
    {
        attrib.updateCachedElementLimit(newBinding);
    }

    bool isMapped = newBinding.getBuffer().get() && newBinding.getBuffer()->isMapped();
    mCachedMappedArrayBuffers.set(attribIndex, isMapped);
    mCachedEnabledMappedArrayBuffers.set(attribIndex, isMapped && attrib.enabled);
}

// VertexArray implementation.
VertexArray::VertexArray(rx::GLImplFactory *factory,
                         GLuint id,
                         size_t maxAttribs,
                         size_t maxAttribBindings)
    : mId(id),
      mState(this, maxAttribs, maxAttribBindings),
      mVertexArray(factory->createVertexArray(mState))
{
    for (size_t attribIndex = 0; attribIndex < maxAttribBindings; ++attribIndex)
    {
        mArrayBufferObserverBindings.emplace_back(this, attribIndex);
    }
}

void VertexArray::onDestroy(const Context *context)
{
    bool isBound = context->isCurrentVertexArray(this);
    for (VertexBinding &binding : mState.mVertexBindings)
    {
        if (isBound)
        {
            if (binding.getBuffer().get())
                binding.getBuffer()->onNonTFBindingChanged(-1);
        }
        binding.setBuffer(context, nullptr);
    }
    if (isBound && mState.mElementArrayBuffer.get())
        mState.mElementArrayBuffer->onNonTFBindingChanged(-1);
    mState.mElementArrayBuffer.bind(context, nullptr);
    mVertexArray->destroy(context);
    SafeDelete(mVertexArray);
    delete this;
}

VertexArray::~VertexArray()
{
    ASSERT(!mVertexArray);
}

void VertexArray::setLabel(const Context *context, const std::string &label)
{
    mState.mLabel = label;
}

const std::string &VertexArray::getLabel() const
{
    return mState.mLabel;
}

bool VertexArray::detachBuffer(const Context *context, GLuint bufferName)
{
    bool isBound = context->isCurrentVertexArray(this);
    bool anyBufferDetached = false;
    for (size_t bindingIndex = 0; bindingIndex < gl::MAX_VERTEX_ATTRIB_BINDINGS; ++bindingIndex)
    {
        VertexBinding &binding = mState.mVertexBindings[bindingIndex];
        if (binding.getBuffer().id() == bufferName)
        {
            if (isBound)
            {
                if (binding.getBuffer().get())
                    binding.getBuffer()->onNonTFBindingChanged(-1);
            }
            binding.setBuffer(context, nullptr);
            mArrayBufferObserverBindings[bindingIndex].reset();

            if (context->getClientVersion() >= ES_3_1)
            {
                setDirtyBindingBit(bindingIndex, DIRTY_BINDING_BUFFER);
            }
            else
            {
                ASSERT(binding.getBoundAttributesMask() == AttributesMask(1u << bindingIndex));
                setDirtyAttribBit(bindingIndex, DIRTY_ATTRIB_POINTER);
            }

            anyBufferDetached = true;
            mState.mClientMemoryAttribsMask |= binding.getBoundAttributesMask();
        }
    }

    if (mState.mElementArrayBuffer.get() && mState.mElementArrayBuffer->id() == bufferName)
    {
        if (isBound && mState.mElementArrayBuffer.get())
            mState.mElementArrayBuffer->onNonTFBindingChanged(-1);
        mState.mElementArrayBuffer.bind(context, nullptr);
        mDirtyBits.set(DIRTY_BIT_ELEMENT_ARRAY_BUFFER);
        anyBufferDetached = true;
    }

    return anyBufferDetached;
}

const VertexAttribute &VertexArray::getVertexAttribute(size_t attribIndex) const
{
    ASSERT(attribIndex < getMaxAttribs());
    return mState.mVertexAttributes[attribIndex];
}

const VertexBinding &VertexArray::getVertexBinding(size_t bindingIndex) const
{
    ASSERT(bindingIndex < getMaxBindings());
    return mState.mVertexBindings[bindingIndex];
}

size_t VertexArray::GetVertexIndexFromDirtyBit(size_t dirtyBit)
{
    static_assert(gl::MAX_VERTEX_ATTRIBS == gl::MAX_VERTEX_ATTRIB_BINDINGS,
                  "The stride of vertex attributes should equal to that of vertex bindings.");
    ASSERT(dirtyBit > DIRTY_BIT_ELEMENT_ARRAY_BUFFER);
    return (dirtyBit - DIRTY_BIT_ATTRIB_0) % gl::MAX_VERTEX_ATTRIBS;
}

ANGLE_INLINE void VertexArray::setDirtyAttribBit(size_t attribIndex,
                                                 DirtyAttribBitType dirtyAttribBit)
{
    mDirtyBits.set(DIRTY_BIT_ATTRIB_0 + attribIndex);
    mDirtyAttribBits[attribIndex].set(dirtyAttribBit);
}

ANGLE_INLINE void VertexArray::setDirtyBindingBit(size_t bindingIndex,
                                                  DirtyBindingBitType dirtyBindingBit)
{
    mDirtyBits.set(DIRTY_BIT_BINDING_0 + bindingIndex);
    mDirtyBindingBits[bindingIndex].set(dirtyBindingBit);
}

ANGLE_INLINE void VertexArray::updateCachedBufferBindingSize(const Context *context,
                                                             VertexBinding *binding)
{
    if (!context->isBufferAccessValidationEnabled())
        return;

    for (size_t boundAttribute : binding->getBoundAttributesMask())
    {
        mState.mVertexAttributes[boundAttribute].updateCachedElementLimit(*binding);
    }
}

ANGLE_INLINE void VertexArray::updateCachedMappedArrayBuffers(
    bool isMapped,
    const AttributesMask &boundAttributesMask)
{
    if (isMapped)
    {
        mState.mCachedMappedArrayBuffers |= boundAttributesMask;
    }
    else
    {
        mState.mCachedMappedArrayBuffers &= ~boundAttributesMask;
    }

    mState.mCachedEnabledMappedArrayBuffers =
        mState.mCachedMappedArrayBuffers & mState.mEnabledAttributesMask;
}

ANGLE_INLINE void VertexArray::updateCachedMappedArrayBuffersBinding(const VertexBinding &binding)
{
    const Buffer *buffer = binding.getBuffer().get();
    return updateCachedMappedArrayBuffers(buffer && buffer->isMapped(),
                                          binding.getBoundAttributesMask());
}

ANGLE_INLINE void VertexArray::updateCachedTransformFeedbackBindingValidation(size_t bindingIndex,
                                                                              const Buffer *buffer)
{
    const bool hasConflict = buffer && buffer->isBoundForTransformFeedbackAndOtherUse();
    mCachedTransformFeedbackConflictedBindingsMask.set(bindingIndex, hasConflict);
}

void VertexArray::bindVertexBufferImpl(const Context *context,
                                       size_t bindingIndex,
                                       Buffer *boundBuffer,
                                       GLintptr offset,
                                       GLsizei stride)
{
    ASSERT(bindingIndex < getMaxBindings());
    ASSERT(context->isCurrentVertexArray(this));

    VertexBinding *binding = &mState.mVertexBindings[bindingIndex];

    Buffer *oldBuffer                = binding->getBuffer().get();
    angle::ObserverBinding *observer = &mArrayBufferObserverBindings[bindingIndex];
    observer->assignSubject(boundBuffer);

    // Several nullptr checks are combined here for optimization purposes.
    if (oldBuffer)
    {
        oldBuffer->onNonTFBindingChanged(-1);
        oldBuffer->removeObserver(observer);
        oldBuffer->release(context);
    }

    binding->assignBuffer(boundBuffer);
    binding->setOffset(offset);
    binding->setStride(stride);
    updateCachedBufferBindingSize(context, binding);

    // Update client memory attribute pointers. Affects all bound attributes.
    if (boundBuffer)
    {
        boundBuffer->addRef();
        boundBuffer->onNonTFBindingChanged(1);
        boundBuffer->addObserver(observer);
        mCachedTransformFeedbackConflictedBindingsMask.set(
            bindingIndex, boundBuffer->isBoundForTransformFeedbackAndOtherUse());
        mState.mClientMemoryAttribsMask &= ~binding->getBoundAttributesMask();
        updateCachedMappedArrayBuffers(boundBuffer->isMapped(), binding->getBoundAttributesMask());
    }
    else
    {
        mCachedTransformFeedbackConflictedBindingsMask.set(bindingIndex, false);
        mState.mClientMemoryAttribsMask |= binding->getBoundAttributesMask();
        updateCachedMappedArrayBuffers(false, binding->getBoundAttributesMask());
    }
}

void VertexArray::bindVertexBuffer(const Context *context,
                                   size_t bindingIndex,
                                   Buffer *boundBuffer,
                                   GLintptr offset,
                                   GLsizei stride)
{
    bindVertexBufferImpl(context, bindingIndex, boundBuffer, offset, stride);
    setDirtyBindingBit(bindingIndex, DIRTY_BINDING_BUFFER);
}

void VertexArray::setVertexAttribBinding(const Context *context,
                                         size_t attribIndex,
                                         GLuint bindingIndex)
{
    ASSERT(attribIndex < getMaxAttribs() && bindingIndex < getMaxBindings());

    if (mState.mVertexAttributes[attribIndex].bindingIndex != bindingIndex)
    {
        // In ES 3.0 contexts, the binding cannot change, hence the code below is unreachable.
        ASSERT(context->getClientVersion() >= ES_3_1);

        mState.setAttribBinding(context, attribIndex, bindingIndex);

        setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_BINDING);

        // Update client attribs mask.
        bool hasBuffer = mState.mVertexBindings[bindingIndex].getBuffer().get() != nullptr;
        mState.mClientMemoryAttribsMask.set(attribIndex, !hasBuffer);
    }
}

void VertexArray::setVertexBindingDivisor(size_t bindingIndex, GLuint divisor)
{
    ASSERT(bindingIndex < getMaxBindings());

    VertexBinding &binding = mState.mVertexBindings[bindingIndex];

    binding.setDivisor(divisor);
    setDirtyBindingBit(bindingIndex, DIRTY_BINDING_DIVISOR);

    // Trigger updates in all bound attributes.
    for (size_t attribIndex : binding.getBoundAttributesMask())
    {
        mState.mVertexAttributes[attribIndex].updateCachedElementLimit(binding);
    }
}

ANGLE_INLINE void VertexArray::setVertexAttribFormatImpl(VertexAttribute *attrib,
                                                         GLint size,
                                                         VertexAttribType type,
                                                         bool normalized,
                                                         GLuint relativeOffset)
{
    attrib->size           = size;
    attrib->type           = type;
    attrib->normalized     = normalized;
    attrib->relativeOffset = relativeOffset;
}

void VertexArray::setVertexAttribFormat(size_t attribIndex,
                                        GLint size,
                                        VertexAttribType type,
                                        bool normalized,
                                        bool pureInteger,
                                        GLuint relativeOffset)
{
    VertexAttribute &attrib = mState.mVertexAttributes[attribIndex];
    attrib.pureInteger      = pureInteger;

    ComponentType componentType = GetVertexAttributeComponentType(pureInteger, type);
    SetComponentTypeMask(componentType, attribIndex, &mState.mVertexAttributesTypeMask);

    setVertexAttribFormatImpl(&attrib, size, type, normalized, relativeOffset);
    setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_FORMAT);

    attrib.updateCachedElementLimit(mState.mVertexBindings[attrib.bindingIndex]);
}

void VertexArray::setVertexAttribDivisor(const Context *context, size_t attribIndex, GLuint divisor)
{
    ASSERT(attribIndex < getMaxAttribs());

    setVertexAttribBinding(context, attribIndex, static_cast<GLuint>(attribIndex));
    setVertexBindingDivisor(attribIndex, divisor);
}

void VertexArray::enableAttribute(size_t attribIndex, bool enabledState)
{
    ASSERT(attribIndex < getMaxAttribs());

    VertexAttribute &attrib = mState.mVertexAttributes[attribIndex];

    if (mState.mEnabledAttributesMask.test(attribIndex) == enabledState)
    {
        return;
    }

    attrib.enabled = enabledState;

    setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_ENABLED);

    // Update state cache
    mState.mEnabledAttributesMask.set(attribIndex, enabledState);
    mState.mCachedEnabledMappedArrayBuffers =
        mState.mCachedMappedArrayBuffers & mState.mEnabledAttributesMask;
}

ANGLE_INLINE void VertexArray::setVertexAttribPointerImpl(const Context *context,
                                                          ComponentType componentType,
                                                          bool pureInteger,
                                                          size_t attribIndex,
                                                          Buffer *boundBuffer,
                                                          GLint size,
                                                          VertexAttribType type,
                                                          bool normalized,
                                                          GLsizei stride,
                                                          const void *pointer)
{
    ASSERT(attribIndex < getMaxAttribs());

    GLintptr offset = boundBuffer ? reinterpret_cast<GLintptr>(pointer) : 0;

    VertexAttribute &attrib = mState.mVertexAttributes[attribIndex];
    attrib.pureInteger      = pureInteger;

    SetComponentTypeMask(componentType, attribIndex, &mState.mVertexAttributesTypeMask);

    setVertexAttribFormatImpl(&attrib, size, type, normalized, 0);
    setVertexAttribBinding(context, attribIndex, static_cast<GLuint>(attribIndex));

    GLsizei effectiveStride =
        stride != 0 ? stride : static_cast<GLsizei>(ComputeVertexAttributeTypeSize(attrib));
    attrib.pointer                 = pointer;
    attrib.vertexAttribArrayStride = stride;

    bindVertexBufferImpl(context, attribIndex, boundBuffer, offset, effectiveStride);

    setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_POINTER);

    mState.mNullPointerClientMemoryAttribsMask.set(attribIndex,
                                                   boundBuffer == nullptr && pointer == nullptr);
}

void VertexArray::setVertexAttribPointer(const Context *context,
                                         size_t attribIndex,
                                         gl::Buffer *boundBuffer,
                                         GLint size,
                                         VertexAttribType type,
                                         bool normalized,
                                         GLsizei stride,
                                         const void *pointer)
{
    setVertexAttribPointerImpl(context, ComponentType::Float, false, attribIndex, boundBuffer, size,
                               type, normalized, stride, pointer);
}

void VertexArray::setVertexAttribIPointer(const Context *context,
                                          size_t attribIndex,
                                          gl::Buffer *boundBuffer,
                                          GLint size,
                                          VertexAttribType type,
                                          GLsizei stride,
                                          const void *pointer)
{
    ComponentType componentType = GetVertexAttributeComponentType(true, type);
    setVertexAttribPointerImpl(context, componentType, true, attribIndex, boundBuffer, size, type,
                               false, stride, pointer);
}

angle::Result VertexArray::syncState(const Context *context)
{
    if (mDirtyBits.any())
    {
        mDirtyBitsGuard = mDirtyBits;
        ANGLE_TRY(
            mVertexArray->syncState(context, mDirtyBits, &mDirtyAttribBits, &mDirtyBindingBits));
        mDirtyBits.reset();
        mDirtyBitsGuard.reset();

        // The dirty bits should be reset in the back-end. To simplify ASSERTs only check attrib 0.
        ASSERT(mDirtyAttribBits[0].none());
        ASSERT(mDirtyBindingBits[0].none());
    }
    return angle::Result::Continue;
}

void VertexArray::onBindingChanged(const Context *context, int incr)
{
    if (mState.mElementArrayBuffer.get())
        mState.mElementArrayBuffer->onNonTFBindingChanged(incr);
    for (auto &binding : mState.mVertexBindings)
    {
        binding.onContainerBindingChanged(context, incr);
    }
}

VertexArray::DirtyBitType VertexArray::getDirtyBitFromIndex(bool contentsChanged,
                                                            angle::SubjectIndex index) const
{
    if (IsElementArrayBufferSubjectIndex(index))
    {
        mIndexRangeCache.invalidate();
        return contentsChanged ? DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA
                               : DIRTY_BIT_ELEMENT_ARRAY_BUFFER;
    }
    else
    {
        // Note: this currently just gets the top-level dirty bit.
        ASSERT(index < mArrayBufferObserverBindings.size());
        return static_cast<DirtyBitType>(
            (contentsChanged ? DIRTY_BIT_BUFFER_DATA_0 : DIRTY_BIT_BINDING_0) + index);
    }
}

void VertexArray::onSubjectStateChange(const gl::Context *context,
                                       angle::SubjectIndex index,
                                       angle::SubjectMessage message)
{
    switch (message)
    {
        case angle::SubjectMessage::CONTENTS_CHANGED:
            setDependentDirtyBit(context, true, index);
            break;

        case angle::SubjectMessage::STORAGE_CHANGED:
            if (!IsElementArrayBufferSubjectIndex(index))
            {
                updateCachedBufferBindingSize(context, &mState.mVertexBindings[index]);
            }
            setDependentDirtyBit(context, false, index);
            break;

        case angle::SubjectMessage::BINDING_CHANGED:
            if (!IsElementArrayBufferSubjectIndex(index))
            {
                const Buffer *buffer = mState.mVertexBindings[index].getBuffer().get();
                updateCachedTransformFeedbackBindingValidation(index, buffer);
            }
            break;

        case angle::SubjectMessage::RESOURCE_MAPPED:
            if (!IsElementArrayBufferSubjectIndex(index))
            {
                updateCachedMappedArrayBuffersBinding(mState.mVertexBindings[index]);
            }
            onStateChange(context, angle::SubjectMessage::RESOURCE_MAPPED);
            break;

        case angle::SubjectMessage::RESOURCE_UNMAPPED:
            setDependentDirtyBit(context, true, index);

            if (!IsElementArrayBufferSubjectIndex(index))
            {
                updateCachedMappedArrayBuffersBinding(mState.mVertexBindings[index]);
            }
            onStateChange(context, angle::SubjectMessage::RESOURCE_UNMAPPED);
            break;

        default:
            UNREACHABLE();
            break;
    }
}

void VertexArray::setDependentDirtyBit(const gl::Context *context,
                                       bool contentsChanged,
                                       angle::SubjectIndex index)
{
    DirtyBitType dirtyBit = getDirtyBitFromIndex(contentsChanged, index);
    ASSERT(!mDirtyBitsGuard.valid() || mDirtyBitsGuard.value().test(dirtyBit));
    mDirtyBits.set(dirtyBit);
    onStateChange(context, angle::SubjectMessage::CONTENTS_CHANGED);
}

bool VertexArray::hasTransformFeedbackBindingConflict(const gl::Context *context) const
{
    // Fast check first.
    if (!mCachedTransformFeedbackConflictedBindingsMask.any())
    {
        return false;
    }

    const AttributesMask &activeAttribues = context->getStateCache().getActiveBufferedAttribsMask();

    // Slow check. We must ensure that the conflicting attributes are enabled/active.
    for (size_t attribIndex : activeAttribues)
    {
        const VertexAttribute &attrib = mState.mVertexAttributes[attribIndex];
        if (mCachedTransformFeedbackConflictedBindingsMask[attrib.bindingIndex])
        {
            return true;
        }
    }

    return false;
}

angle::Result VertexArray::getIndexRangeImpl(const Context *context,
                                             DrawElementsType type,
                                             GLsizei indexCount,
                                             const void *indices,
                                             IndexRange *indexRangeOut) const
{
    Buffer *elementArrayBuffer = mState.mElementArrayBuffer.get();
    if (!elementArrayBuffer)
    {
        *indexRangeOut = ComputeIndexRange(type, indices, indexCount,
                                           context->getState().isPrimitiveRestartEnabled());
        return angle::Result::Continue;
    }

    size_t offset = reinterpret_cast<uintptr_t>(indices);
    ANGLE_TRY(elementArrayBuffer->getIndexRange(context, type, offset, indexCount,
                                                context->getState().isPrimitiveRestartEnabled(),
                                                indexRangeOut));

    mIndexRangeCache.put(type, indexCount, offset, *indexRangeOut);
    return angle::Result::Continue;
}

VertexArray::IndexRangeCache::IndexRangeCache() = default;

void VertexArray::IndexRangeCache::put(DrawElementsType type,
                                       GLsizei indexCount,
                                       size_t offset,
                                       const IndexRange &indexRange)
{
    ASSERT(type != DrawElementsType::InvalidEnum);

    mTypeKey       = type;
    mIndexCountKey = indexCount;
    mOffsetKey     = offset;
    mPayload       = indexRange;
}
}  // namespace gl
