//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class contains prototypes for representing GLES 3 Vertex Array Objects:
//
//   The buffer objects that are to be used by the vertex stage of the GL are collected
//   together to form a vertex array object. All state related to the definition of data used
//   by the vertex processor is encapsulated in a vertex array object.
//

#ifndef LIBANGLE_VERTEXARRAY_H_
#define LIBANGLE_VERTEXARRAY_H_

#include "common/Optional.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Debug.h"
#include "libANGLE/IndexRangeCache.h"
#include "libANGLE/Observer.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/VertexAttribute.h"

#include <vector>

namespace rx
{
class GLImplFactory;
class VertexArrayImpl;
}  // namespace rx

namespace gl
{
class Buffer;

class VertexArrayState final : angle::NonCopyable
{
  public:
    VertexArrayState(VertexArrayID vertexArrayID, size_t maxAttribs, size_t maxBindings);
    ~VertexArrayState();

    const std::string &getLabel() const { return mLabel; }

    size_t getMaxAttribs() const { return mVertexAttributes.size(); }
    size_t getMaxBindings() const { return mVertexBindings.size(); }
    const AttributesMask &getEnabledAttributesMask() const { return mEnabledAttributesMask; }
    const std::vector<VertexAttribute> &getVertexAttributes() const { return mVertexAttributes; }
    const VertexAttribute &getVertexAttribute(size_t attribIndex) const
    {
        return mVertexAttributes[attribIndex];
    }
    const std::vector<VertexBinding> &getVertexBindings() const { return mVertexBindings; }
    const VertexBinding &getVertexBinding(size_t bindingIndex) const
    {
        return mVertexBindings[bindingIndex];
    }
    const VertexBinding &getBindingFromAttribIndex(size_t attribIndex) const
    {
        return mVertexBindings[mVertexAttributes[attribIndex].bindingIndex];
    }
    size_t getBindingIndexFromAttribIndex(size_t attribIndex) const
    {
        return mVertexAttributes[attribIndex].bindingIndex;
    }

    void setAttribBinding(size_t attribIndex, GLuint newBindingIndex);

    // Extra validation performed on the Vertex Array.
    bool hasEnabledNullPointerClientArray() const;

    // Get all the attributes in an AttributesMask that are using the given binding.
    AttributesMask getBindingToAttributesMask(GLuint bindingIndex) const;

    ComponentTypeMask getVertexAttributesTypeMask() const { return mVertexAttributesTypeMask; }

    AttributesMask getClientMemoryAttribsMask() const { return mClientMemoryAttribsMask; }

    AttributesMask getNullPointerClientMemoryAttribsMask() const
    {
        return mNullPointerClientMemoryAttribsMask;
    }

    VertexArrayID id() const { return mId; }

    bool isDefault() const;

  private:
    friend class VertexArrayPrivate;
    friend class VertexArray;
    VertexArrayID mId;
    std::string mLabel;
    std::vector<VertexAttribute> mVertexAttributes;
    // mMaxVertexAttribBindings is the max size of vertex attributes. element buffer is stored in
    // mVertexBindings[kElementArrayBufferIndex].
    std::vector<VertexBinding> mVertexBindings;

    AttributesMask mEnabledAttributesMask;
    ComponentTypeMask mVertexAttributesTypeMask;
    AttributesMask mLastSyncedEnabledAttributesMask;

    // This is a performance optimization for buffer binding. Allows element array buffer updates.
    friend class State;

    // From the GLES 3.1 spec:
    // When a generic attribute array is sourced from client memory, the vertex attribute binding
    // state is ignored. Thus we don't have to worry about binding state when using client memory
    // attribs.
    AttributesMask mClientMemoryAttribsMask;
    AttributesMask mNullPointerClientMemoryAttribsMask;
};

class VertexArrayPrivate : public angle::NonCopyable
{
  public:
    // Dirty bits for VertexArrays use a hierarchical design. At the top level, each attribute
    // has a single dirty bit. Then an array of MAX_ATTRIBS dirty bits each has a dirty bit for
    // enabled/pointer/format/binding. Bindings are handled similarly. Note that because the
    // total number of dirty bits is 33, it will not be as fast on a 32-bit machine, which
    // can't support the advanced 64-bit scanning intrinsics. We could consider packing the
    // binding and attribute bits together if this becomes a problem.
    //
    // Special note on "DIRTY_ATTRIB_POINTER_BUFFER": this is a special case when the app
    // calls glVertexAttribPointer but only changes a VBO and/or offset binding. This allows
    // the Vulkan back-end to skip performing a pipeline change for performance.
    enum DirtyBitType
    {
        // Dirty bits for bindings.
        DIRTY_BIT_BINDING_0,
        DIRTY_BIT_BINDING_MAX          = DIRTY_BIT_BINDING_0 + MAX_VERTEX_ATTRIB_BINDINGS,
        DIRTY_BIT_ELEMENT_ARRAY_BUFFER = DIRTY_BIT_BINDING_MAX,

        // We keep separate dirty bits for bound buffers whose data changed since last update.
        DIRTY_BIT_BUFFER_DATA_0,
        DIRTY_BIT_BUFFER_DATA_MAX           = DIRTY_BIT_BUFFER_DATA_0 + MAX_VERTEX_ATTRIB_BINDINGS,
        DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA = DIRTY_BIT_BUFFER_DATA_MAX,

        // Dirty bits for attributes.
        DIRTY_BIT_ATTRIB_0,
        DIRTY_BIT_ATTRIB_MAX = DIRTY_BIT_ATTRIB_0 + MAX_VERTEX_ATTRIBS,

        DIRTY_BIT_UNKNOWN = DIRTY_BIT_ATTRIB_MAX,
        DIRTY_BIT_MAX     = DIRTY_BIT_UNKNOWN,
    };

    // We want to keep the number of dirty bits within 64 to keep iteration times fast.
    static_assert(DIRTY_BIT_MAX <= 64, "Too many vertex array dirty bits.");
    // The dirty bit processing has the logic to avoid redundant processing by removing other dirty
    // bits when it processes dirtyBits. This assertion ensures these dirty bit order matches what
    // VertexArrayVk::syncState expects.
    static_assert(DIRTY_BIT_BINDING_0 < DIRTY_BIT_BUFFER_DATA_0,
                  "BINDING dirty bits should come before DATA.");
    static_assert(DIRTY_BIT_BUFFER_DATA_0 < DIRTY_BIT_ATTRIB_0,
                  "DATA dirty bits should come before ATTRIB.");

    enum DirtyAttribBitType
    {
        DIRTY_ATTRIB_ENABLED,
        DIRTY_ATTRIB_POINTER,
        DIRTY_ATTRIB_FORMAT,
        DIRTY_ATTRIB_BINDING,
        DIRTY_ATTRIB_POINTER_BUFFER,
        DIRTY_ATTRIB_MAX,
    };

    enum DirtyBindingBitType
    {
        DIRTY_BINDING_BUFFER,
        DIRTY_BINDING_DIVISOR,
        DIRTY_BINDING_STRIDE,
        DIRTY_BINDING_OFFSET,
        DIRTY_BINDING_MAX,
    };

    using DirtyBits                = angle::BitSet<DIRTY_BIT_MAX>;
    using DirtyAttribBits          = angle::BitSet<DIRTY_ATTRIB_MAX>;
    using DirtyBindingBits         = angle::BitSet<DIRTY_BINDING_MAX>;
    using DirtyAttribBitsArray     = std::array<DirtyAttribBits, MAX_VERTEX_ATTRIBS>;
    using DirtyBindingBitsArray    = std::array<DirtyBindingBits, MAX_VERTEX_ATTRIB_BINDINGS>;

    VertexArrayPrivate(rx::GLImplFactory *factory,
                       VertexArrayID id,
                       size_t maxAttribs,
                       size_t maxAttribBindings);

    VertexArrayID id() const { return mId; }

    void enableAttribute(size_t attribIndex, bool enabledState);

    const VertexBinding &getVertexBinding(size_t bindingIndex) const;
    const VertexAttribute &getVertexAttribute(size_t attribIndex) const;
    const VertexBinding &getBindingFromAttribIndex(size_t attribIndex) const
    {
        return mState.getBindingFromAttribIndex(attribIndex);
    }

    void setVertexBindingDivisor(size_t bindingIndex, GLuint divisor);

    size_t getMaxAttribs() const { return mState.getMaxAttribs(); }
    size_t getMaxBindings() const { return mState.getMaxBindings(); }

    const std::vector<VertexAttribute> &getVertexAttributes() const
    {
        return mState.getVertexAttributes();
    }
    const std::vector<VertexBinding> &getVertexBindings() const
    {
        return mState.getVertexBindings();
    }

    const AttributesMask &getEnabledAttributesMask() const
    {
        return mState.getEnabledAttributesMask();
    }

    AttributesMask getClientAttribsMask() const { return mState.mClientMemoryAttribsMask; }

    bool hasEnabledNullPointerClientArray() const
    {
        return mState.hasEnabledNullPointerClientArray();
    }

    bool hasInvalidMappedArrayBuffer() const { return mCachedInvalidMappedArrayBuffer.any(); }

    const VertexArrayState &getState() const { return mState; }

    bool isBufferAccessValidationEnabled() const { return mBufferAccessValidationEnabled; }

    bool hasAnyDirtyBit() const { return mDirtyBits.any(); }

    ComponentTypeMask getAttributesTypeMask() const { return mState.mVertexAttributesTypeMask; }
    AttributesMask getAttributesMask() const { return mState.mEnabledAttributesMask; }

    bool hasTransformFeedbackBindingConflict(const Context *context) const;

    void setBufferAccessValidationEnabled(bool enabled)
    {
        mBufferAccessValidationEnabled = enabled;
        if (mBufferAccessValidationEnabled)
        {
            mCachedBufferSize.resize(mState.getMaxBindings(), 0);
        }
    }

    size_t getBindingIndexFromAttribIndex(size_t attribIndex) const
    {
        return mState.getBindingIndexFromAttribIndex(attribIndex);
    }

    void setVertexAttribBinding(size_t attribIndex, GLuint bindingIndex);
    void setVertexAttribDivisor(size_t index, GLuint divisor);
    void setVertexAttribFormat(size_t attribIndex,
                               GLint size,
                               VertexAttribType type,
                               bool normalized,
                               bool pureInteger,
                               GLuint relativeOffset);

  protected:
    ~VertexArrayPrivate();

    // This is a performance optimization for buffer binding. Allows element array buffer updates.
    friend class State;

    void setDirtyAttribBit(size_t attribIndex, DirtyAttribBitType dirtyAttribBit);
    void setDirtyBindingBit(size_t bindingIndex, DirtyBindingBitType dirtyBindingBit);
    void clearDirtyAttribBit(size_t attribIndex, DirtyAttribBitType dirtyAttribBit);

    // These are used to optimize draw call validation.
    void updateCachedElementLimit(const VertexBinding &binding, GLint64 bufferSize);
    void updateCachedArrayBuffersMasks(bool isMapped,
                                       bool isImmutable,
                                       bool isPersistent,
                                       const AttributesMask &boundAttributesMask);

    // These two functions return true if the state was dirty.
    bool setVertexAttribFormatImpl(VertexAttribute *attrib,
                                   GLint size,
                                   VertexAttribType type,
                                   bool normalized,
                                   bool pureInteger,
                                   GLuint relativeOffset);

    VertexArrayID mId;

    VertexArrayState mState;
    DirtyBits mDirtyBits;
    DirtyAttribBitsArray mDirtyAttribBits;
    DirtyBindingBitsArray mDirtyBindingBits;
    Optional<DirtyBits> mDirtyBitsGuard;

    mutable IndexRangeInlineCache mIndexRangeInlineCache;
    bool mBufferAccessValidationEnabled;

    // Cached buffer size indexed by bindingIndex, only used when mBufferAccessValidationEnabled is
    // true.
    std::vector<GLint64> mCachedBufferSize;
    // Cached XFB property indexed by bindingIndex, only used for webGL
    VertexArrayBufferBindingMask mCachedBufferPropertyTransformFeedbackConflict;

    // Cached buffer properties indexed by bindingIndex
    VertexArrayBufferBindingMask mBufferBindingMask;
    VertexArrayBufferBindingMask mCachedBufferPropertyMapped;
    VertexArrayBufferBindingMask mCachedBufferPropertyMutableOrImpersistent;

    // Used for validation cache. Indexed by attribute.
    AttributesMask mCachedMappedArrayBuffers;
    AttributesMask mCachedMutableOrImpersistentArrayBuffers;
    AttributesMask mCachedInvalidMappedArrayBuffer;
};

using VertexArrayBuffers = std::array<gl::BindingPointer<gl::Buffer>, kElementArrayBufferIndex + 1>;

class VertexArray final : public VertexArrayPrivate, public LabeledObject, public angle::Subject
{
  public:
    VertexArray(rx::GLImplFactory *factory,
                VertexArrayID id,
                size_t maxAttribs,
                size_t maxAttribBindings);

    void onDestroy(const Context *context);

    angle::Result setLabel(const Context *context, const std::string &label) override;
    const std::string &getLabel() const override;

    // Returns true if the function finds and detaches a bound buffer.
    bool detachBuffer(const Context *context, BufferID bufferID);

    void setVertexAttribPointer(const Context *context,
                                size_t attribIndex,
                                Buffer *boundBuffer,
                                GLint size,
                                VertexAttribType type,
                                bool normalized,
                                GLsizei stride,
                                const void *pointer,
                                bool *isVertexAttribDirtyOut);

    void setVertexAttribIPointer(const Context *context,
                                 size_t attribIndex,
                                 Buffer *boundBuffer,
                                 GLint size,
                                 VertexAttribType type,
                                 GLsizei stride,
                                 const void *pointer,
                                 bool *isVertexAttribDirtyOut);

    void bindElementBuffer(const Context *context, Buffer *boundBuffer);

    void bindVertexBuffer(const Context *context,
                          size_t bindingIndex,
                          Buffer *boundBuffer,
                          GLintptr offset,
                          GLsizei stride);

    Buffer *getElementArrayBuffer() const
    {
        return mVertexArrayBuffers[kElementArrayBufferIndex].get();
    }
    Buffer *getVertexArrayBuffer(size_t bindingIndex) const
    {
        return mVertexArrayBuffers[bindingIndex].get();
    }

    rx::VertexArrayImpl *getImplementation() const { return mVertexArray; }

    const BufferID getVertexArrayBufferID(size_t bindingIndex) const
    {
        return mVertexArrayBuffers[bindingIndex].id();
    }

    angle::Result syncState(const Context *context);

    void onBindingChanged(const Context *context, int incr);
    void onRebind(const Context *context) { onBind(context); }

    angle::Result getIndexRange(const Context *context,
                                DrawElementsType type,
                                GLsizei indexCount,
                                const void *indices,
                                bool primitiveRestartEnabled,
                                IndexRange *indexRangeOut) const;

    void onBufferChanged(const Context *context,
                         const Buffer *buffer,
                         angle::SubjectMessage message,
                         VertexArrayBufferBindingMask bufferBindingMask);

    // A buffer attached to this vertex array is being bound. It might have been modified by other
    // context.
    void onSharedBufferBind(const Context *context,
                            const Buffer *buffer,
                            VertexArrayBufferBindingMask bufferBindingMask);

    const VertexArrayBuffers &getBufferBindingPointers() const { return mVertexArrayBuffers; }

  private:
    ~VertexArray() override;

    void setVertexAttribPointerImpl(const Context *context,
                                    ComponentType componentType,
                                    bool pureInteger,
                                    size_t attribIndex,
                                    Buffer *boundBuffer,
                                    GLint size,
                                    VertexAttribType type,
                                    bool normalized,
                                    GLsizei stride,
                                    const void *pointer,
                                    bool *isVertexAttribDirtyOut);

    DirtyBindingBits bindVertexBufferImpl(const Context *context,
                                          size_t bindingIndex,
                                          Buffer *boundBuffer,
                                          GLintptr offset,
                                          GLsizei stride);

    void onBind(const Context *context);
    void onUnbind(const Context *context);

    void setDependentDirtyBits(bool contentsChanged,
                               VertexArrayBufferBindingMask bufferBindingMask);
    void updateCachedMappedArrayBuffersBinding(size_t bindingIndex);
    bool bufferMaskBitsPointToTheSameBuffer(VertexArrayBufferBindingMask bufferBindingMask) const;

    VertexArrayBuffers mVertexArrayBuffers;
    rx::VertexArrayImpl *mVertexArray;
};

inline angle::Result VertexArray::getIndexRange(const Context *context,
                                                DrawElementsType type,
                                                GLsizei indexCount,
                                                const void *indices,
                                                bool primitiveRestartEnabled,
                                                IndexRange *indexRangeOut) const
{
    Buffer *elementArrayBuffer = getElementArrayBuffer();
    if (!elementArrayBuffer)
    {
        *indexRangeOut = ComputeIndexRange(type, indices, indexCount, primitiveRestartEnabled);
        return angle::Result::Continue;
    }
    size_t offset = reinterpret_cast<uintptr_t>(indices);
    size_t count  = static_cast<size_t>(indexCount);
    if (mIndexRangeInlineCache.get(type, offset, count, primitiveRestartEnabled, indexRangeOut))
    {
        return angle::Result::Continue;
    }
    ANGLE_TRY(elementArrayBuffer->getIndexRange(context, type, offset, count,
                                                primitiveRestartEnabled, indexRangeOut));
    mIndexRangeInlineCache =
        IndexRangeInlineCache{type, offset, count, primitiveRestartEnabled, *indexRangeOut};
    return angle::Result::Continue;
}

}  // namespace gl

#endif  // LIBANGLE_VERTEXARRAY_H_
