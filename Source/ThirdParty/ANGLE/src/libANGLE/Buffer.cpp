//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.cpp: Implements the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#include "libANGLE/Buffer.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/GLImplFactory.h"

namespace gl
{
namespace
{
constexpr size_t kInvalidContentsObserverIndex            = std::numeric_limits<size_t>::max();
}  // anonymous namespace

// VertexArrayBufferBindingMaskAndContext implementation
VertexArrayBufferBindingMaskAndContext::VertexArrayBufferBindingMaskAndContext() {}
VertexArrayBufferBindingMaskAndContext::~VertexArrayBufferBindingMaskAndContext()
{
    mBufferBindingMask.clear();
}

void VertexArrayBufferBindingMaskAndContext::add(const gl::Context *context, size_t bindingIndex)
{
    for (auto &contextAndMask : mBufferBindingMask)
    {
        if (contextAndMask.first == context)
        {
            contextAndMask.second.set(bindingIndex);
            return;
        }
    }
    mBufferBindingMask.emplace_back(context, VertexArrayBufferBindingMask({bindingIndex}));
}

void VertexArrayBufferBindingMaskAndContext::remove(const gl::Context *context, size_t bindingIndex)
{
    for (auto iter = mBufferBindingMask.begin(); iter != mBufferBindingMask.end(); ++iter)
    {
        if (iter->first == context)
        {
            iter->second.reset(bindingIndex);
            if (iter->second.none())
            {
                mBufferBindingMask.erase(iter);
            }
            return;
        }
    }
    UNREACHABLE();
}

VertexArrayBufferBindingMask VertexArrayBufferBindingMaskAndContext::getBufferBindingMask(
    const gl::Context *context) const
{
    for (auto &contextAndMask : mBufferBindingMask)
    {
        if (contextAndMask.first == context)
        {
            return contextAndMask.second;
        }
    }
    return VertexArrayBufferBindingMask::Zero();
}

// BufferState implementation
BufferState::BufferState()
    : mLabel(),
      mUsage(BufferUsage::StaticDraw),
      mSize(0),
      mAccessFlags(0),
      mAccess(GL_WRITE_ONLY_OES),
      mMapped(GL_FALSE),
      mMapPointer(nullptr),
      mMapOffset(0),
      mMapLength(0),
      mBindingCount(0),
      mTransformFeedbackIndexedBindingCount(0),
      mTransformFeedbackGenericBindingCount(0),
      mImmutable(GL_FALSE),
      mStorageExtUsageFlags(0),
      mExternal(GL_FALSE),
      mWebGLType(WebGLBufferType::Undefined)
{}

BufferState::~BufferState() {}

Buffer::Buffer(rx::GLImplFactory *factory, BufferID id)
    : RefCountObject(factory->generateSerial(), id), mImpl(factory->createBuffer(mState))
{
}

Buffer::~Buffer()
{
    SafeDelete(mImpl);
}

void Buffer::onDestroy(const Context *context)
{
    mContentsObservers.clear();

    // In tests, mImpl might be null.
    if (mImpl)
        mImpl->destroy(context);
}

void Buffer::onBind(const Context *context, BufferBinding target)
{
    ASSERT(context->isWebGL());

    if (mState.mWebGLType == WebGLBufferType::Undefined)
    {
        if (target == BufferBinding::ElementArray)
        {
            mState.mWebGLType = WebGLBufferType::ElementArray;
        }
        else
        {
            mState.mWebGLType = WebGLBufferType::OtherData;
        }
    }
}

angle::Result Buffer::setLabel(const Context *context, const std::string &label)
{
    mState.mLabel = label;
    if (mImpl)
    {
        return mImpl->onLabelUpdate(context);
    }
    return angle::Result::Continue;
}

const std::string &Buffer::getLabel() const
{
    return mState.mLabel;
}

angle::Result Buffer::bufferStorageExternal(Context *context,
                                            BufferBinding target,
                                            GLsizeiptr size,
                                            GLeglClientBufferEXT clientBuffer,
                                            GLbitfield flags)
{
    return bufferExternalDataImpl(context, target, clientBuffer, size, flags);
}

angle::Result Buffer::bufferStorage(Context *context,
                                    BufferBinding target,
                                    GLsizeiptr size,
                                    const void *data,
                                    GLbitfield flags)
{
    return bufferDataImpl(context, target, data, size, BufferUsage::DynamicDraw, flags,
                          BufferStorage::Immutable);
}

angle::Result Buffer::bufferData(Context *context,
                                 BufferBinding target,
                                 const void *data,
                                 GLsizeiptr size,
                                 BufferUsage usage)
{
    GLbitfield flags = (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT_EXT);
    return bufferDataImpl(context, target, data, size, usage, flags, BufferStorage::Mutable);
}

angle::Result Buffer::setDataWithUsageFlags(const gl::Context *context,
                                            gl::BufferBinding target,
                                            GLeglClientBufferEXT clientBuffer,
                                            const void *data,
                                            size_t size,
                                            gl::BufferUsage usage,
                                            GLbitfield flags,
                                            gl::BufferStorage bufferStorage)
{
    rx::BufferFeedback feedback;
    angle::Result result = mImpl->setDataWithUsageFlags(context, target, clientBuffer, data, size,
                                                        usage, flags, bufferStorage, &feedback);

    applyImplFeedback(context, feedback);

    if (result == angle::Result::Stop)
    {
        // If setData fails, the buffer contents are undefined. Set a zero size to indicate that.
        mIndexRangeCache.clear();
        mState.mSize = 0;

        // Notify when storage changes.
        onStateChange(context, angle::SubjectMessage::SubjectChanged);
    }
    return result;
}

angle::Result Buffer::bufferDataImpl(Context *context,
                                     BufferBinding target,
                                     const void *data,
                                     GLsizeiptr size,
                                     BufferUsage usage,
                                     GLbitfield flags,
                                     BufferStorage bufferStorage)
{
    const void *dataForImpl = data;

    if (mState.isMapped())
    {
        // Per the OpenGL ES 3.0 spec, buffers are implicity unmapped when a call to
        // BufferData happens on a mapped buffer:
        //
        //     If any portion of the buffer object is mapped in the current context or any context
        //     current to another thread, it is as though UnmapBuffer (see section 2.10.3) is
        //     executed in each such context prior to deleting the existing data store.
        //
        GLboolean dontCare = GL_FALSE;
        ANGLE_TRY(unmap(context, &dontCare));
    }

    // If we are using robust resource init, make sure the buffer starts cleared.
    // Note: the Context is checked for nullptr because of some testing code.
    // TODO(jmadill): Investigate lazier clearing.
    if (context && context->isRobustResourceInitEnabled() && !data && size > 0)
    {
        angle::MemoryBuffer *scratchBuffer = nullptr;
        ANGLE_CHECK_GL_ALLOC(
            context, context->getZeroFilledBuffer(static_cast<size_t>(size), &scratchBuffer));
        dataForImpl = scratchBuffer->data();
    }

    ANGLE_TRY(setDataWithUsageFlags(context, target, nullptr, dataForImpl, size, usage, flags,
                                    bufferStorage));

    bool wholeBuffer = size == mState.mSize;

    mIndexRangeCache.clear();
    mState.mUsage                = usage;
    mState.mSize                 = size;
    mState.mImmutable            = (bufferStorage == BufferStorage::Immutable);
    mState.mStorageExtUsageFlags = flags;

    // Notify when storage changes.
    if (wholeBuffer)
    {
        onContentsChange(context);
    }
    else
    {
        onStateChange(context, angle::SubjectMessage::SubjectChanged);
    }

    return angle::Result::Continue;
}

angle::Result Buffer::bufferExternalDataImpl(Context *context,
                                             BufferBinding target,
                                             GLeglClientBufferEXT clientBuffer,
                                             GLsizeiptr size,
                                             GLbitfield flags)
{
    if (mState.isMapped())
    {
        // Per the OpenGL ES 3.0 spec, buffers are implicitly unmapped when a call to
        // BufferData happens on a mapped buffer:
        //
        //     If any portion of the buffer object is mapped in the current context or any context
        //     current to another thread, it is as though UnmapBuffer (see section 2.10.3) is
        //     executed in each such context prior to deleting the existing data store.
        //
        GLboolean dontCare = GL_FALSE;
        ANGLE_TRY(unmap(context, &dontCare));
    }

    ANGLE_TRY(setDataWithUsageFlags(context, target, clientBuffer, nullptr, size,
                                    BufferUsage::DynamicDraw, flags, BufferStorage::Immutable));

    mIndexRangeCache.clear();
    mState.mUsage                = BufferUsage::DynamicDraw;
    mState.mSize                 = size;
    mState.mImmutable            = GL_TRUE;
    mState.mStorageExtUsageFlags = flags;
    mState.mExternal             = GL_TRUE;

    // Notify when storage changes.
    onStateChange(context, angle::SubjectMessage::SubjectChanged);

    return angle::Result::Continue;
}

angle::Result Buffer::bufferSubData(const Context *context,
                                    BufferBinding target,
                                    const void *data,
                                    GLsizeiptr size,
                                    GLintptr offset)
{
    rx::BufferFeedback feedback;
    ANGLE_TRY_WITH_FINALLY(mImpl->setSubData(context, target, data, size, offset, &feedback),
                           applyImplFeedback(context, feedback));

    mIndexRangeCache.invalidateRange(static_cast<unsigned int>(offset),
                                     static_cast<unsigned int>(size));

    // Notify when data changes.
    onContentsChange(context);

    return angle::Result::Continue;
}

angle::Result Buffer::copyBufferSubData(const Context *context,
                                        Buffer *source,
                                        GLintptr sourceOffset,
                                        GLintptr destOffset,
                                        GLsizeiptr size)
{
    rx::BufferFeedback feedback;
    ANGLE_TRY_WITH_FINALLY(mImpl->copySubData(context, source->getImplementation(), sourceOffset,
                                              destOffset, size, &feedback),
                           applyImplFeedback(context, feedback));

    mIndexRangeCache.invalidateRange(static_cast<unsigned int>(destOffset),
                                     static_cast<unsigned int>(size));

    // Notify when data changes.
    onContentsChange(context);

    return angle::Result::Continue;
}

angle::Result Buffer::map(const Context *context, GLenum access)
{
    ASSERT(!mState.mMapped);

    rx::BufferFeedback feedback;
    mState.mMapPointer = nullptr;
    ANGLE_TRY_WITH_FINALLY(mImpl->map(context, access, &mState.mMapPointer, &feedback),
                           applyImplFeedback(context, feedback));

    ASSERT(access == GL_WRITE_ONLY_OES);

    mState.mMapped      = GL_TRUE;
    mState.mMapOffset   = 0;
    mState.mMapLength   = mState.mSize;
    mState.mAccess      = access;
    mState.mAccessFlags = GL_MAP_WRITE_BIT;
    mIndexRangeCache.clear();

    // Notify when state changes.
    onStateChange(context, angle::SubjectMessage::SubjectMapped);

    return angle::Result::Continue;
}

angle::Result Buffer::mapRange(const Context *context,
                               GLintptr offset,
                               GLsizeiptr length,
                               GLbitfield access)
{
    ASSERT(!mState.mMapped);
    ASSERT(offset + length <= mState.mSize);

    rx::BufferFeedback feedback;
    mState.mMapPointer = nullptr;
    ANGLE_TRY_WITH_FINALLY(
        mImpl->mapRange(context, offset, length, access, &mState.mMapPointer, &feedback),
        applyImplFeedback(context, feedback));

    mState.mMapped      = GL_TRUE;
    mState.mMapOffset   = static_cast<GLint64>(offset);
    mState.mMapLength   = static_cast<GLint64>(length);
    mState.mAccess      = GL_WRITE_ONLY_OES;
    mState.mAccessFlags = access;

    // The OES_mapbuffer extension states that GL_WRITE_ONLY_OES is the only valid
    // value for GL_BUFFER_ACCESS_OES because it was written against ES2.  Since there is
    // no update for ES3 and the GL_READ_ONLY and GL_READ_WRITE enums don't exist for ES,
    // we cannot properly set GL_BUFFER_ACCESS_OES when glMapBufferRange is called.

    if ((access & GL_MAP_WRITE_BIT) > 0)
    {
        mIndexRangeCache.invalidateRange(static_cast<unsigned int>(offset),
                                         static_cast<unsigned int>(length));
    }

    // Notify when state changes.
    onStateChange(context, angle::SubjectMessage::SubjectMapped);

    return angle::Result::Continue;
}

angle::Result Buffer::unmap(const Context *context, GLboolean *result)
{
    ASSERT(mState.mMapped);

    rx::BufferFeedback feedback;
    *result = GL_FALSE;
    ANGLE_TRY_WITH_FINALLY(mImpl->unmap(context, result, &feedback),
                           applyImplFeedback(context, feedback));

    mState.mMapped      = GL_FALSE;
    mState.mMapPointer  = nullptr;
    mState.mMapOffset   = 0;
    mState.mMapLength   = 0;
    mState.mAccess      = GL_WRITE_ONLY_OES;
    mState.mAccessFlags = 0;

    // Notify when data changes.
    onStateChange(context, angle::SubjectMessage::SubjectUnmapped);

    return angle::Result::Continue;
}

void Buffer::onDataChanged(const Context *context)
{
    mIndexRangeCache.clear();

    // Notify when data changes.
    onContentsChange(context);

    mImpl->onDataChanged();
}

angle::Result Buffer::getIndexRange(const gl::Context *context,
                                    DrawElementsType type,
                                    size_t offset,
                                    size_t count,
                                    bool primitiveRestartEnabled,
                                    IndexRange *outRange) const
{
    if (mIndexRangeCache.findRange(type, offset, count, primitiveRestartEnabled, outRange))
    {
        return angle::Result::Continue;
    }

    ANGLE_TRY(
        mImpl->getIndexRange(context, type, offset, count, primitiveRestartEnabled, outRange));

    mIndexRangeCache.addRange(type, offset, count, primitiveRestartEnabled, *outRange);

    return angle::Result::Continue;
}

GLint64 Buffer::getMemorySize() const
{
    GLint64 implSize = mImpl->getMemorySize();
    return implSize > 0 ? implSize : mState.mSize;
}

bool Buffer::isDoubleBoundForTransformFeedback() const
{
    return mState.mTransformFeedbackIndexedBindingCount > 1;
}

void Buffer::onTFBindingChanged(const Context *context, bool bound, bool indexed)
{
    ASSERT(bound || mState.mBindingCount > 0);
    mState.mBindingCount += bound ? 1 : -1;
    if (indexed)
    {
        ASSERT(bound || mState.mTransformFeedbackIndexedBindingCount > 0);
        mState.mTransformFeedbackIndexedBindingCount += bound ? 1 : -1;

        onStateChange(context, angle::SubjectMessage::BindingChanged);
    }
    else
    {
        mState.mTransformFeedbackGenericBindingCount += bound ? 1 : -1;
    }
}

angle::Result Buffer::getSubData(const gl::Context *context,
                                 GLintptr offset,
                                 GLsizeiptr size,
                                 void *outData)
{
    return mImpl->getSubData(context, offset, size, outData);
}

size_t Buffer::getContentsObserverIndex(void *observer, uint32_t bufferIndex) const
{
    ContentsObserver contentsObserver{bufferIndex, observer};
    for (size_t observerIndex = 0; observerIndex < mContentsObservers.size(); ++observerIndex)
    {
        if (mContentsObservers[observerIndex] == contentsObserver)
        {
            return observerIndex;
        }
    }

    return kInvalidContentsObserverIndex;
}

void Buffer::addContentsObserver(VertexArray *vertexArray, uint32_t bufferIndex)
{
    ASSERT(bufferIndex != ContentsObserver::kBufferTextureIndex);
    if (getContentsObserverIndex(vertexArray, bufferIndex) == kInvalidContentsObserverIndex)
    {
        mContentsObservers.push_back({bufferIndex, vertexArray});
    }
}

void Buffer::removeContentsObserverImpl(void *observer, uint32_t bufferIndex)
{
    size_t foundObserver = getContentsObserverIndex(observer, bufferIndex);
    if (foundObserver != kInvalidContentsObserverIndex)
    {
        size_t lastObserverIndex = mContentsObservers.size() - 1;
        if (foundObserver != lastObserverIndex)
        {
            mContentsObservers[foundObserver] = mContentsObservers[lastObserverIndex];
        }
        mContentsObservers.pop_back();
    }
}

void Buffer::removeContentsObserver(VertexArray *vertexArray, uint32_t bufferIndex)
{
    removeContentsObserverImpl(vertexArray, bufferIndex);
}

void Buffer::addContentsObserver(Texture *texture)
{
    if (!hasContentsObserver(texture))
    {
        mContentsObservers.push_back({ContentsObserver::kBufferTextureIndex, texture});
    }
}

void Buffer::removeContentsObserver(Texture *texture)
{
    removeContentsObserverImpl(texture, ContentsObserver::kBufferTextureIndex);
}

bool Buffer::hasContentsObserver(Texture *texture) const
{
    return getContentsObserverIndex(texture, ContentsObserver::kBufferTextureIndex) !=
           kInvalidContentsObserverIndex;
}

void Buffer::onStateChange(const Context *context, angle::SubjectMessage message)
{
    // Pass message to other buffer observers such as XFB and Texture
    angle::Subject::onStateChange(message);

    // Apply the change directly on current context's current vertex array. All other vertex arrays
    // requires a buffer rebind in order to pick up the change.
    context->onBufferChanged(this, message,
                             mVertexArrayBufferBindingMaskAndContext.getBufferBindingMask(context));
}

void Buffer::onContentsChange(const Context *context)
{
    for (const ContentsObserver &contentsObserver : mContentsObservers)
    {
        ASSERT(contentsObserver.bufferIndex == ContentsObserver::kBufferTextureIndex);
        static_cast<Texture *>(contentsObserver.observer)->onBufferContentsChange();
    }

    context->onBufferChanged(this, angle::SubjectMessage::ContentsChanged,
                             mVertexArrayBufferBindingMaskAndContext.getBufferBindingMask(context));
}

void Buffer::applyImplFeedback(const gl::Context *context, const rx::BufferFeedback &feedback)
{
    // Pass it along to observer of Buffer
    if (feedback.internalMemoryAllocationChanged)
    {
        onStateChange(context, angle::SubjectMessage::InternalMemoryAllocationChanged);
    }

    if (feedback.bufferStateChanged)
    {
        onStateChange(context, angle::SubjectMessage::SubjectChanged);
    }
}
}  // namespace gl
