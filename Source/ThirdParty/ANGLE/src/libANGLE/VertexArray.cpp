//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
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
// VertexArrayState implementation.
VertexArrayState::VertexArrayState(VertexArray *vertexArray,
                                   size_t maxAttribs,
                                   size_t maxAttribBindings)
    : mId(vertexArray->id())
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
    ASSERT(bindingIndex < mVertexBindings.size());
    return mVertexBindings[bindingIndex].getBoundAttributesMask();
}

// Set an attribute using a new binding.
void VertexArrayState::setAttribBinding(const Context *context,
                                        size_t attribIndex,
                                        GLuint newBindingIndex)
{
    ASSERT(attribIndex < mVertexAttributes.size() && newBindingIndex < mVertexBindings.size());

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
    mEnabledAttributesMask.set(attribIndex, attrib.enabled);
    updateCachedMutableOrNonPersistentArrayBuffers(attribIndex);
    mCachedInvalidMappedArrayBuffer = mCachedMappedArrayBuffers & mEnabledAttributesMask &
                                      mCachedMutableOrImpersistentArrayBuffers;
}

void VertexArrayState::updateCachedMutableOrNonPersistentArrayBuffers(size_t index)
{
    const VertexBinding &vertexBinding   = mVertexBindings[index];
    const BindingPointer<Buffer> &buffer = vertexBinding.getBuffer();
    bool isMutableOrImpersistentArrayBuffer =
        buffer.get() &&
        (!buffer->isImmutable() || (buffer->getAccessFlags() & GL_MAP_PERSISTENT_BIT_EXT) == 0);
    mCachedMutableOrImpersistentArrayBuffers.set(index, isMutableOrImpersistentArrayBuffer);
}

bool VertexArrayState::isDefault() const
{
    return mId.value == 0;
}

// VertexArray implementation.
VertexArray::VertexArray(rx::GLImplFactory *factory,
                         VertexArrayID id,
                         size_t maxAttribs,
                         size_t maxAttribBindings)
    : mId(id),
      mState(this, maxAttribs, maxAttribBindings),
      mVertexArray(factory->createVertexArray(mState)),
      mBufferAccessValidationEnabled(false)
{
}

void VertexArray::onDestroy(const Context *context)
{
    bool isBound = context->isCurrentVertexArray(this);

    Buffer *buffer = mState.mElementArrayBuffer.get();
    if (buffer)
    {
        if (isBound)
        {
            buffer->onNonTFBindingChanged(-1);
            buffer->removeVertexArrayBinding(context, kElementArrayBufferIndex);
        }
        mState.mElementArrayBuffer.set(context, nullptr);
        mState.mBufferBindingMask.reset(kElementArrayBufferIndex);
    }
    else
    {
        ASSERT(!mState.mBufferBindingMask[kElementArrayBufferIndex]);
    }

    for (size_t bindingIndex : mState.mBufferBindingMask)
    {
        VertexBinding &binding = mState.mVertexBindings[bindingIndex];
        buffer                 = binding.getBuffer().get();
        ASSERT(buffer != nullptr);
        if (isBound)
        {
            buffer->onNonTFBindingChanged(-1);
            buffer->removeVertexArrayBinding(context, bindingIndex);
        }
        binding.setBuffer(context, nullptr);
    }

    mState.mBufferBindingMask.reset();
    mVertexArray->destroy(context);
    SafeDelete(mVertexArray);
    delete this;
}

VertexArray::~VertexArray()
{
    ASSERT(!mVertexArray);
}

angle::Result VertexArray::setLabel(const Context *context, const std::string &label)
{
    mState.mLabel = label;

    if (mVertexArray)
    {
        return mVertexArray->onLabelUpdate(context);
    }
    return angle::Result::Continue;
}

const std::string &VertexArray::getLabel() const
{
    return mState.mLabel;
}

bool VertexArray::detachBuffer(const Context *context, BufferID bufferID)
{
    bool isBound           = context->isCurrentVertexArray(this);
    bool anyBufferDetached = false;

    if (mState.mElementArrayBuffer.get())
    {
        if (mState.mElementArrayBuffer->id() == bufferID)
        {
            if (isBound && mState.mElementArrayBuffer.get())
            {
                mState.mElementArrayBuffer->onNonTFBindingChanged(-1);
            }
            mState.mElementArrayBuffer->removeVertexArrayBinding(context, kElementArrayBufferIndex);
            mState.mElementArrayBuffer.set(context, nullptr);
            mState.mBufferBindingMask.reset(kElementArrayBufferIndex);
            mDirtyBits.set(DIRTY_BIT_ELEMENT_ARRAY_BUFFER);
            anyBufferDetached = true;
        }
    }
    else
    {
        ASSERT(!mState.mBufferBindingMask[kElementArrayBufferIndex]);
    }

    // Now process all other binding bits other than kElementArrayBufferIndex;
    VertexArrayBufferBindingMask bufferBindingMask = mState.mBufferBindingMask;
    bufferBindingMask.reset(kElementArrayBufferIndex);
    for (size_t bindingIndex : bufferBindingMask)
    {
        VertexBinding &binding                      = mState.mVertexBindings[bindingIndex];
        const BindingPointer<Buffer> &bufferBinding = binding.getBuffer();
        if (bufferBinding.id() == bufferID)
        {
            if (isBound)
            {
                if (bufferBinding.get())
                {
                    bufferBinding->onNonTFBindingChanged(-1);
                }
            }
            bufferBinding->removeVertexArrayBinding(context, bindingIndex);
            binding.setBuffer(context, nullptr);
            mState.mBufferBindingMask.reset(bindingIndex);

            if (context->getClientVersion() >= ES_3_1 && !mState.isDefault())
            {
                setDirtyBindingBit(bindingIndex, DIRTY_BINDING_BUFFER);
            }
            else
            {
                static_assert(MAX_VERTEX_ATTRIB_BINDINGS < 8 * sizeof(uint32_t),
                              "Not enough bits in bindingIndex");
                // The redundant uint32_t cast here is required to avoid a warning on MSVC.
                ASSERT(binding.getBoundAttributesMask() ==
                       AttributesMask(static_cast<uint32_t>(1 << bindingIndex)));
                setDirtyAttribBit(bindingIndex, DIRTY_ATTRIB_POINTER);
            }

            anyBufferDetached = true;
            mState.mClientMemoryAttribsMask |= binding.getBoundAttributesMask();
        }
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

ANGLE_INLINE void VertexArray::setDirtyAttribBit(size_t attribIndex,
                                                 DirtyAttribBitType dirtyAttribBit)
{
    mDirtyBits.set(DIRTY_BIT_ATTRIB_0 + attribIndex);
    mDirtyAttribBits[attribIndex].set(dirtyAttribBit);
}

ANGLE_INLINE void VertexArray::clearDirtyAttribBit(size_t attribIndex,
                                                   DirtyAttribBitType dirtyAttribBit)
{
    mDirtyAttribBits[attribIndex].set(dirtyAttribBit, false);
    if (mDirtyAttribBits[attribIndex].any())
    {
        return;
    }
    mDirtyBits.set(DIRTY_BIT_ATTRIB_0 + attribIndex, false);
}

ANGLE_INLINE void VertexArray::setDirtyBindingBit(size_t bindingIndex,
                                                  DirtyBindingBitType dirtyBindingBit)
{
    mDirtyBits.set(DIRTY_BIT_BINDING_0 + bindingIndex);
    mDirtyBindingBits[bindingIndex].set(dirtyBindingBit);
}

ANGLE_INLINE void VertexArray::updateCachedBufferBindingSize(VertexBinding *binding)
{
    ASSERT(mBufferAccessValidationEnabled);
    for (size_t boundAttribute : binding->getBoundAttributesMask())
    {
        mState.mVertexAttributes[boundAttribute].updateCachedElementLimit(*binding);
    }
}

ANGLE_INLINE void VertexArray::updateCachedArrayBuffersMasks(
    bool isMapped,
    bool isImmutable,
    bool isPersistent,
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

    if (!isImmutable || !isPersistent)
    {
        mState.mCachedMutableOrImpersistentArrayBuffers |= boundAttributesMask;
    }
    else
    {
        mState.mCachedMutableOrImpersistentArrayBuffers &= ~boundAttributesMask;
    }

    mState.mCachedInvalidMappedArrayBuffer = mState.mCachedMappedArrayBuffers &
                                             mState.mEnabledAttributesMask &
                                             mState.mCachedMutableOrImpersistentArrayBuffers;
}

ANGLE_INLINE void VertexArray::updateCachedMappedArrayBuffersBinding(const VertexBinding &binding)
{
    const Buffer *buffer = binding.getBuffer().get();
    bool isMapped        = buffer && buffer->isMapped();
    bool isImmutable     = buffer && buffer->isImmutable();
    bool isPersistent    = buffer && (buffer->getAccessFlags() & GL_MAP_PERSISTENT_BIT_EXT) != 0;
    return updateCachedArrayBuffersMasks(isMapped, isImmutable, isPersistent,
                                         binding.getBoundAttributesMask());
}

ANGLE_INLINE void VertexArray::updateCachedTransformFeedbackBindingValidation(size_t bindingIndex,
                                                                              const Buffer *buffer)
{
    const bool hasConflict = buffer && buffer->hasWebGLXFBBindingConflict(true);
    mCachedTransformFeedbackConflictedBindingsMask.set(bindingIndex, hasConflict);
}

void VertexArray::bindElementBuffer(const Context *context, Buffer *boundBuffer)
{
    Buffer *oldBuffer = mState.mElementArrayBuffer.get();

    if (oldBuffer)
    {
        oldBuffer->removeVertexArrayBinding(context, kElementArrayBufferIndex);
        if (context->isWebGL())
        {
            oldBuffer->onNonTFBindingChanged(-1);
        }
        oldBuffer->release(context);
        mState.mBufferBindingMask.reset(kElementArrayBufferIndex);
    }

    mState.mElementArrayBuffer.assign(boundBuffer);

    if (boundBuffer)
    {
        boundBuffer->addVertexArrayBinding(context, kElementArrayBufferIndex);
        if (context->isWebGL())
        {
            boundBuffer->onNonTFBindingChanged(1);
        }
        boundBuffer->addRef();
        mState.mBufferBindingMask.set(kElementArrayBufferIndex);
    }

    mDirtyBits.set(VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER);
    mIndexRangeInlineCache = {};
}

ANGLE_INLINE VertexArray::DirtyBindingBits VertexArray::bindVertexBufferImpl(const Context *context,
                                                                             size_t bindingIndex,
                                                                             Buffer *boundBuffer,
                                                                             GLintptr offset,
                                                                             GLsizei stride)
{
    ASSERT(bindingIndex < getMaxBindings());
    ASSERT(context->isCurrentVertexArray(this));

    VertexBinding *binding = &mState.mVertexBindings[bindingIndex];

    Buffer *oldBuffer = binding->getBuffer().get();

    DirtyBindingBits dirtyBindingBits;
    dirtyBindingBits.set(DIRTY_BINDING_BUFFER, oldBuffer != boundBuffer);
    dirtyBindingBits.set(DIRTY_BINDING_STRIDE, static_cast<GLuint>(stride) != binding->getStride());
    dirtyBindingBits.set(DIRTY_BINDING_OFFSET, offset != binding->getOffset());

    if (dirtyBindingBits.none())
    {
        return dirtyBindingBits;
    }

    if (boundBuffer != oldBuffer)
    {
        // Several nullptr checks are combined here for optimization purposes.
        if (oldBuffer)
        {
            oldBuffer->onNonTFBindingChanged(-1);
            oldBuffer->removeVertexArrayBinding(context, bindingIndex);
            oldBuffer->release(context);
            mState.mBufferBindingMask.reset(bindingIndex);
        }

        binding->assignBuffer(boundBuffer);

        // Update client memory attribute pointers. Affects all bound attributes.
        if (boundBuffer)
        {
            boundBuffer->addRef();
            boundBuffer->onNonTFBindingChanged(1);
            boundBuffer->addVertexArrayBinding(context, bindingIndex);
            if (context->isWebGL())
            {
                mCachedTransformFeedbackConflictedBindingsMask.set(
                    bindingIndex, boundBuffer->hasWebGLXFBBindingConflict(true));
            }
            mState.mBufferBindingMask.set(bindingIndex);
            mState.mClientMemoryAttribsMask &= ~binding->getBoundAttributesMask();

            bool isMapped     = boundBuffer->isMapped() == GL_TRUE;
            bool isImmutable  = boundBuffer->isImmutable() == GL_TRUE;
            bool isPersistent = (boundBuffer->getAccessFlags() & GL_MAP_PERSISTENT_BIT_EXT) != 0;
            updateCachedArrayBuffersMasks(isMapped, isImmutable, isPersistent,
                                          binding->getBoundAttributesMask());
        }
        else
        {
            if (context->isWebGL())
            {
                mCachedTransformFeedbackConflictedBindingsMask.set(bindingIndex, false);
            }
            mState.mClientMemoryAttribsMask |= binding->getBoundAttributesMask();
            updateCachedArrayBuffersMasks(false, false, false, binding->getBoundAttributesMask());
        }
    }

    binding->setOffset(offset);
    binding->setStride(stride);

    if (mBufferAccessValidationEnabled)
    {
        updateCachedBufferBindingSize(binding);
    }

    return dirtyBindingBits;
}

void VertexArray::bindVertexBuffer(const Context *context,
                                   size_t bindingIndex,
                                   Buffer *boundBuffer,
                                   GLintptr offset,
                                   GLsizei stride)
{
    const VertexArray::DirtyBindingBits dirtyBindingBits =
        bindVertexBufferImpl(context, bindingIndex, boundBuffer, offset, stride);
    if (dirtyBindingBits.any())
    {
        mDirtyBits.set(DIRTY_BIT_BINDING_0 + bindingIndex);
        mDirtyBindingBits[bindingIndex] |= dirtyBindingBits;
    }
}

void VertexArray::setVertexAttribBinding(const Context *context,
                                         size_t attribIndex,
                                         GLuint bindingIndex)
{
    ASSERT(attribIndex < getMaxAttribs() && bindingIndex < getMaxBindings());

    if (mState.mVertexAttributes[attribIndex].bindingIndex == bindingIndex)
    {
        return;
    }

    // In ES 3.0 contexts, the binding cannot change, hence the code below is unreachable.
    ASSERT(context->getClientVersion() >= ES_3_1 && !mState.isDefault());

    mState.setAttribBinding(context, attribIndex, bindingIndex);

    setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_BINDING);

    // Update client attribs mask.
    bool hasBuffer = mState.mVertexBindings[bindingIndex].getBuffer().get() != nullptr;
    mState.mClientMemoryAttribsMask.set(attribIndex, !hasBuffer);
}

void VertexArray::setVertexBindingDivisor(const Context *context,
                                          size_t bindingIndex,
                                          GLuint divisor)
{
    ASSERT(bindingIndex < getMaxBindings());

    VertexBinding &binding = mState.mVertexBindings[bindingIndex];

    if (binding.getDivisor() == divisor)
    {
        return;
    }

    binding.setDivisor(divisor);
    setDirtyBindingBit(bindingIndex, DIRTY_BINDING_DIVISOR);
}

ANGLE_INLINE bool VertexArray::setVertexAttribFormatImpl(VertexAttribute *attrib,
                                                         GLint size,
                                                         VertexAttribType type,
                                                         bool normalized,
                                                         bool pureInteger,
                                                         GLuint relativeOffset)
{
    angle::FormatID formatID = GetVertexFormatID(type, normalized, size, pureInteger);

    if (formatID != attrib->format->id || attrib->relativeOffset != relativeOffset)
    {
        attrib->relativeOffset = relativeOffset;
        attrib->format         = &angle::Format::Get(formatID);
        return true;
    }

    return false;
}

void VertexArray::setVertexAttribFormat(size_t attribIndex,
                                        GLint size,
                                        VertexAttribType type,
                                        bool normalized,
                                        bool pureInteger,
                                        GLuint relativeOffset)
{
    VertexAttribute &attrib = mState.mVertexAttributes[attribIndex];

    ComponentType componentType = GetVertexAttributeComponentType(pureInteger, type);
    SetComponentTypeMask(componentType, attribIndex, &mState.mVertexAttributesTypeMask);

    if (setVertexAttribFormatImpl(&attrib, size, type, normalized, pureInteger, relativeOffset))
    {
        setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_FORMAT);
    }

    attrib.updateCachedElementLimit(mState.mVertexBindings[attrib.bindingIndex]);
}

void VertexArray::setVertexAttribDivisor(const Context *context, size_t attribIndex, GLuint divisor)
{
    ASSERT(attribIndex < getMaxAttribs());

    setVertexAttribBinding(context, attribIndex, static_cast<GLuint>(attribIndex));
    setVertexBindingDivisor(context, attribIndex, divisor);
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

    // Update state cache
    mState.mEnabledAttributesMask.set(attribIndex, enabledState);
    bool enableChanged = (mState.mEnabledAttributesMask.test(attribIndex) !=
                          mState.mLastSyncedEnabledAttributesMask.test(attribIndex));

    if (enableChanged)
    {
        setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_ENABLED);
    }
    else
    {
        clearDirtyAttribBit(attribIndex, DIRTY_ATTRIB_ENABLED);
    }

    mState.updateCachedMutableOrNonPersistentArrayBuffers(attribIndex);
    mState.mCachedInvalidMappedArrayBuffer = mState.mCachedMappedArrayBuffers &
                                             mState.mEnabledAttributesMask &
                                             mState.mCachedMutableOrImpersistentArrayBuffers;
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
                                                          const void *pointer,
                                                          bool *isVertexAttribDirtyOut)
{
    ASSERT(isVertexAttribDirtyOut);
    ASSERT(attribIndex < getMaxAttribs());

    VertexAttribute &attrib = mState.mVertexAttributes[attribIndex];

    SetComponentTypeMask(componentType, attribIndex, &mState.mVertexAttributesTypeMask);

    bool attribDirty = setVertexAttribFormatImpl(&attrib, size, type, normalized, pureInteger, 0);

    if (attrib.bindingIndex != attribIndex)
    {
        setVertexAttribBinding(context, attribIndex, static_cast<GLuint>(attribIndex));
    }

    GLsizei effectiveStride =
        stride == 0 ? static_cast<GLsizei>(ComputeVertexAttributeTypeSize(attrib)) : stride;

    if (attrib.vertexAttribArrayStride != static_cast<GLuint>(stride))
    {
        attribDirty = true;
    }
    attrib.vertexAttribArrayStride = stride;

    // If we switch from an array buffer to a client pointer(or vice-versa), we set the whole
    // attribute dirty. This notifies the Vulkan back-end to update all its caches.
    const VertexBinding &binding = mState.mVertexBindings[attribIndex];
    if ((boundBuffer == nullptr) != (binding.getBuffer().get() == nullptr))
    {
        attribDirty = true;
    }

    // If using client arrays and the pointer changes, set the attribute as dirty
    if (boundBuffer == nullptr && attrib.pointer != pointer)
    {
        attribDirty = true;
    }

    // Change of attrib.pointer is not part of attribDirty. Pointer is actually the buffer offset
    // which is handled within bindVertexBufferImpl and reflected in bufferDirty.
    attrib.pointer  = pointer;
    GLintptr offset = boundBuffer ? reinterpret_cast<GLintptr>(pointer) : 0;
    const VertexArray::DirtyBindingBits dirtyBindingBits =
        bindVertexBufferImpl(context, attribIndex, boundBuffer, offset, effectiveStride);

    if (attribDirty)
    {
        setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_POINTER);
        *isVertexAttribDirtyOut = true;
    }
    else if (dirtyBindingBits.any())
    {
        setDirtyAttribBit(attribIndex, DIRTY_ATTRIB_POINTER_BUFFER);
        *isVertexAttribDirtyOut = true;
    }

    mState.mNullPointerClientMemoryAttribsMask.set(attribIndex,
                                                   boundBuffer == nullptr && pointer == nullptr);
}

void VertexArray::setVertexAttribPointer(const Context *context,
                                         size_t attribIndex,
                                         Buffer *boundBuffer,
                                         GLint size,
                                         VertexAttribType type,
                                         bool normalized,
                                         GLsizei stride,
                                         const void *pointer,
                                         bool *isVertexAttribDirtyOut)
{
    setVertexAttribPointerImpl(context, ComponentType::Float, false, attribIndex, boundBuffer, size,
                               type, normalized, stride, pointer, isVertexAttribDirtyOut);
}

void VertexArray::setVertexAttribIPointer(const Context *context,
                                          size_t attribIndex,
                                          Buffer *boundBuffer,
                                          GLint size,
                                          VertexAttribType type,
                                          GLsizei stride,
                                          const void *pointer,
                                          bool *isVertexAttribDirtyOut)
{
    ComponentType componentType = GetVertexAttributeComponentType(true, type);
    setVertexAttribPointerImpl(context, componentType, true, attribIndex, boundBuffer, size, type,
                               false, stride, pointer, isVertexAttribDirtyOut);
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
        mState.mLastSyncedEnabledAttributesMask = mState.mEnabledAttributesMask;
    }
    return angle::Result::Continue;
}

// This becomes current vertex array on the context
void VertexArray::onBind(const Context *context)
{
    VertexArrayBufferBindingMask bufferBindingMask = mState.mBufferBindingMask;

    if (bufferBindingMask[kElementArrayBufferIndex])
    {
        Buffer *bufferGL = mState.mElementArrayBuffer.get();
        ASSERT(bufferGL != nullptr);
        bufferGL->addVertexArrayBinding(context, kElementArrayBufferIndex);
        bufferBindingMask.reset(kElementArrayBufferIndex);
    }
    else
    {
        ASSERT(mState.mElementArrayBuffer.get() == nullptr);
    }

    // This vertex array becoming current. Some of the bindings we may have removed from buffer's
    // observer list. We need to add it back to the buffer's observer list and update dirty bits
    // that we may have missed while we were not observing.
    for (size_t bindingIndex : bufferBindingMask)
    {
        const VertexBinding &binding = mState.mVertexBindings[bindingIndex];
        Buffer *bufferGL             = binding.getBuffer().get();
        ASSERT(bufferGL != nullptr);

        bufferGL->addVertexArrayBinding(context, bindingIndex);
        updateCachedMappedArrayBuffersBinding(binding);

        if (mBufferAccessValidationEnabled)
        {
            for (size_t boundAttribute :
                 mState.mVertexBindings[bindingIndex].getBoundAttributesMask())
            {
                mState.mVertexAttributes[boundAttribute].updateCachedElementLimit(
                    mState.mVertexBindings[bindingIndex]);
            }
        }

        if (context->isWebGL())
        {
            updateCachedTransformFeedbackBindingValidation(bindingIndex, bufferGL);
        }
    }

    mDirtyBits.set(DIRTY_BIT_LOST_OBSERVATION);
    // Always reset mIndexRangeInlineCache since we lost buffer observation while unbind
    mIndexRangeInlineCache = {};

    onStateChange(angle::SubjectMessage::ContentsChanged);
}

// This becomes non-current vertex array on the context
void VertexArray::onUnbind(const Context *context)
{
    VertexArrayBufferBindingMask bufferBindingMask = mState.mBufferBindingMask;
    if (bufferBindingMask[kElementArrayBufferIndex])
    {
        Buffer *bufferGL = mState.mElementArrayBuffer.get();
        ASSERT(bufferGL != nullptr);
        bufferGL->removeVertexArrayBinding(context, kElementArrayBufferIndex);
        bufferBindingMask.reset(kElementArrayBufferIndex);
    }
    else
    {
        ASSERT(mState.mElementArrayBuffer.get() == nullptr);
    }

    // This vertex array becoming non-current. For performance reason, we remove it from the
    // buffers' observer list so that the cost of buffer sending signal to observers will not be too
    // expensive.
    for (size_t bindingIndex : bufferBindingMask)
    {
        const VertexBinding &binding = mState.mVertexBindings[bindingIndex];
        Buffer *bufferGL             = binding.getBuffer().get();
        ASSERT(bufferGL != nullptr);
        bufferGL->removeVertexArrayBinding(context, bindingIndex);
    }
}

void VertexArray::onBindingChanged(const Context *context, int incr)
{
    // When vertex array gets unbound, we remove it from bound buffers' observer list so that when
    // buffer changes, it wont has to loop over all these non-current vertex arrays and set dirty
    // bit on them. To compensate for that, when we bind a vertex array, we have to check against
    // each bound buffers and see if they have changed and needs to update vertex array's dirty bits
    // accordingly
    ASSERT(incr == 1 || incr == -1);
    if (incr < 0)
    {
        onUnbind(context);
    }
    else
    {
        onBind(context);
    }

    if (context->isWebGL())
    {
        VertexArrayBufferBindingMask bufferBindingMask = mState.mBufferBindingMask;
        if (bufferBindingMask[kElementArrayBufferIndex])
        {
            ASSERT(mState.mElementArrayBuffer.get() != nullptr);
            mState.mElementArrayBuffer->onNonTFBindingChanged(incr);
            bufferBindingMask.reset(kElementArrayBufferIndex);
        }
        else
        {
            ASSERT(mState.mElementArrayBuffer.get() == nullptr);
        }

        for (size_t bindingIndex : bufferBindingMask)
        {
            mState.mVertexBindings[bindingIndex].onContainerBindingChanged(context, incr);
        }
    }
}

void VertexArray::setDependentDirtyBits(bool contentsChanged,
                                        VertexArrayBufferBindingMask bufferBindingMask)
{
    DirtyBits dirtyBits(contentsChanged ? (bufferBindingMask.bits() << DIRTY_BIT_BUFFER_DATA_0)
                                        : (bufferBindingMask.bits() << DIRTY_BIT_BINDING_0));
    ASSERT(!mDirtyBitsGuard.valid() || (mDirtyBitsGuard.value() & dirtyBits) == dirtyBits);
    mDirtyBits |= dirtyBits;

    if (bufferBindingMask.test(kElementArrayBufferIndex))
    {
        mIndexRangeInlineCache = {};
    }

    onStateChange(angle::SubjectMessage::ContentsChanged);
}

bool VertexArray::hasTransformFeedbackBindingConflict(const Context *context) const
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

void VertexArray::onBufferChanged(const Context *context,
                                  angle::SubjectMessage message,
                                  VertexArrayBufferBindingMask vertexArrayBufferBindingMask)
{
    VertexArrayBufferBindingMask bufferBindingMask =
        vertexArrayBufferBindingMask & mState.mBufferBindingMask;
    ASSERT(bufferBindingMask.any());

    switch (message)
    {
        case angle::SubjectMessage::SubjectChanged:
            if (mBufferAccessValidationEnabled)
            {
                VertexArrayBufferBindingMask VertexBufferBindingMask = bufferBindingMask;
                VertexBufferBindingMask.reset(kElementArrayBufferIndex);
                for (size_t bindingIndex : VertexBufferBindingMask)
                {
                    updateCachedBufferBindingSize(&mState.mVertexBindings[bindingIndex]);
                }
            }
            // This has to be called after updateCachedBufferBindingSize due to
            // mCachedElementLimit dependency
            setDependentDirtyBits(false, bufferBindingMask);
            break;

        case angle::SubjectMessage::BindingChanged:
            bufferBindingMask.reset(kElementArrayBufferIndex);
            for (size_t bindingIndex : bufferBindingMask)
            {
                const Buffer *buffer = mState.mVertexBindings[bindingIndex].getBuffer().get();
                updateCachedTransformFeedbackBindingValidation(bindingIndex, buffer);
            }
            break;

        case angle::SubjectMessage::SubjectMapped:
            bufferBindingMask.reset(kElementArrayBufferIndex);
            for (size_t bindingIndex : bufferBindingMask)
            {
                updateCachedMappedArrayBuffersBinding(mState.mVertexBindings[bindingIndex]);
            }
            onStateChange(angle::SubjectMessage::SubjectMapped);
            break;

        case angle::SubjectMessage::SubjectUnmapped:
        {
            VertexArrayBufferBindingMask VertexBufferBindingMask = bufferBindingMask;
            VertexBufferBindingMask.reset(kElementArrayBufferIndex);
            for (size_t bindingIndex : VertexBufferBindingMask)
            {
                updateCachedMappedArrayBuffersBinding(mState.mVertexBindings[bindingIndex]);
            }
            setDependentDirtyBits(true, bufferBindingMask);
            onStateChange(angle::SubjectMessage::SubjectUnmapped);
        }
        break;

        case angle::SubjectMessage::InternalMemoryAllocationChanged:
            setDependentDirtyBits(false, bufferBindingMask);
            break;

        case angle::SubjectMessage::ContentsChanged:
        {
            VertexArrayBufferBindingMask bufferContentObserverBindingMask =
                vertexArrayBufferBindingMask & mVertexArray->getContentObserversBindingMask();
            if (bufferContentObserverBindingMask.any())
            {
                setDependentDirtyBits(true, bufferBindingMask);
            }
        }
        break;

        default:
            UNREACHABLE();
            break;
    }
}
}  // namespace gl
