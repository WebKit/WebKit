/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebGLBuffer.h"

#if ENABLE(WEBGL)

#include "WebGLContextGroup.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

Ref<WebGLBuffer> WebGLBuffer::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLBuffer(ctx));
}

WebGLBuffer::WebGLBuffer(WebGLRenderingContextBase& ctx)
    : WebGLSharedObject(ctx)
{
    setObject(ctx.graphicsContext3D()->createBuffer());
    clearCachedMaxIndices();
}

WebGLBuffer::~WebGLBuffer()
{
    deleteObject(0);
}

void WebGLBuffer::deleteObjectImpl(GraphicsContext3D* context3d, Platform3DObject object)
{
    context3d->deleteBuffer(object);
}

bool WebGLBuffer::associateBufferDataImpl(const void* data, GC3Dsizeiptr byteLength)
{
    if (byteLength < 0)
        return false;

    switch (m_target) {
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER:
        if (byteLength > std::numeric_limits<unsigned>::max())
            return false;
        m_byteLength = byteLength;
        clearCachedMaxIndices();
        if (byteLength) {
            m_elementArrayBuffer = ArrayBuffer::tryCreate(byteLength, 1);
            if (!m_elementArrayBuffer) {
                m_byteLength = 0;
                return false;
            }
            if (data) {
                // We must always clone the incoming data because client-side
                // modifications without calling bufferData or bufferSubData
                // must never be able to change the validation results.
                memcpy(m_elementArrayBuffer->data(), data, byteLength);
            }
        } else
            m_elementArrayBuffer = nullptr;
        return true;
    case GraphicsContext3D::ARRAY_BUFFER:
        m_byteLength = byteLength;
        return true;
    default:
#if ENABLE(WEBGL2)
        switch (m_target) {
        case GraphicsContext3D::COPY_READ_BUFFER:
        case GraphicsContext3D::COPY_WRITE_BUFFER:
        case GraphicsContext3D::PIXEL_PACK_BUFFER:
        case GraphicsContext3D::PIXEL_UNPACK_BUFFER:
        case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER:
        case GraphicsContext3D::UNIFORM_BUFFER:
            m_byteLength = byteLength;
            return true;
        }
#endif
        return false;
    }
}

bool WebGLBuffer::associateBufferData(GC3Dsizeiptr size)
{
    return associateBufferDataImpl(nullptr, size);
}

bool WebGLBuffer::associateBufferData(ArrayBuffer* array)
{
    if (!array)
        return false;
    return associateBufferDataImpl(array->data(), array->byteLength());
}

bool WebGLBuffer::associateBufferData(ArrayBufferView* array)
{
    if (!array)
        return false;
    return associateBufferDataImpl(array->baseAddress(), array->byteLength());
}

bool WebGLBuffer::associateBufferSubDataImpl(GC3Dintptr offset, const void* data, GC3Dsizeiptr byteLength)
{
    if (!data || offset < 0 || byteLength < 0)
        return false;

    if (byteLength) {
        Checked<GC3Dintptr, RecordOverflow> checkedBufferOffset(offset);
        Checked<GC3Dsizeiptr, RecordOverflow> checkedDataLength(byteLength);
        Checked<GC3Dintptr, RecordOverflow> checkedBufferMax = checkedBufferOffset + checkedDataLength;
        if (checkedBufferMax.hasOverflowed() || offset > m_byteLength || checkedBufferMax.unsafeGet() > m_byteLength)
            return false;
    }

    switch (m_target) {
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER:
        clearCachedMaxIndices();
        if (byteLength) {
            if (!m_elementArrayBuffer)
                return false;
            memcpy(static_cast<unsigned char*>(m_elementArrayBuffer->data()) + offset, data, byteLength);
        }
        return true;
    case GraphicsContext3D::ARRAY_BUFFER:
        return true;
    default:
#if ENABLE(WEBGL2)
        switch (m_target) {
        case GraphicsContext3D::COPY_READ_BUFFER:
        case GraphicsContext3D::COPY_WRITE_BUFFER:
        case GraphicsContext3D::PIXEL_PACK_BUFFER:
        case GraphicsContext3D::PIXEL_UNPACK_BUFFER:
        case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER:
        case GraphicsContext3D::UNIFORM_BUFFER:
            return true;
        }
#endif
        return false;
    }
}

bool WebGLBuffer::associateBufferSubData(GC3Dintptr offset, ArrayBuffer* array)
{
    if (!array)
        return false;
    return associateBufferSubDataImpl(offset, array->data(), array->byteLength());
}

bool WebGLBuffer::associateBufferSubData(GC3Dintptr offset, ArrayBufferView* array)
{
    if (!array)
        return false;
    return associateBufferSubDataImpl(offset, array->baseAddress(), array->byteLength());
}

bool WebGLBuffer::associateCopyBufferSubData(const WebGLBuffer& readBuffer, GC3Dintptr readOffset, GC3Dintptr writeOffset, GC3Dsizeiptr size)
{
    if (readOffset < 0 || writeOffset < 0 || size < 0)
        return false;

    if (size) {
        Checked<GC3Dintptr, RecordOverflow> checkedReadBufferOffset(readOffset);
        Checked<GC3Dsizeiptr, RecordOverflow> checkedDataLength(size);
        Checked<GC3Dintptr, RecordOverflow> checkedReadBufferMax = checkedReadBufferOffset + checkedDataLength;
        if (checkedReadBufferMax.hasOverflowed() || readOffset > readBuffer.byteLength() || checkedReadBufferMax.unsafeGet() > readBuffer.byteLength())
            return false;

        Checked<GC3Dintptr, RecordOverflow> checkedWriteBufferOffset(writeOffset);
        Checked<GC3Dintptr, RecordOverflow> checkedWriteBufferMax = checkedWriteBufferOffset + checkedDataLength;
        if (checkedWriteBufferMax.hasOverflowed() || writeOffset > m_byteLength || checkedWriteBufferMax.unsafeGet() > m_byteLength)
            return false;
    }

    switch (m_target) {
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER:
        clearCachedMaxIndices();
        if (size) {
            if (!m_elementArrayBuffer)
                return false;
            memcpy(static_cast<unsigned char*>(m_elementArrayBuffer->data()) + writeOffset, static_cast<const unsigned char*>(readBuffer.elementArrayBuffer()->data()) + readOffset, size);
        }
        return true;
    case GraphicsContext3D::ARRAY_BUFFER:
        return true;
    default:
#if ENABLE(WEBGL2)
        switch (m_target) {
        case GraphicsContext3D::COPY_READ_BUFFER:
        case GraphicsContext3D::COPY_WRITE_BUFFER:
        case GraphicsContext3D::PIXEL_PACK_BUFFER:
        case GraphicsContext3D::PIXEL_UNPACK_BUFFER:
        case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER:
        case GraphicsContext3D::UNIFORM_BUFFER:
            return true;
        }
#endif
        return false;
    }
}

void WebGLBuffer::disassociateBufferData()
{
    m_byteLength = 0;
    clearCachedMaxIndices();
}

GC3Dsizeiptr WebGLBuffer::byteLength() const
{
    return m_byteLength;
}

Optional<unsigned> WebGLBuffer::getCachedMaxIndex(GC3Denum type)
{
    for (auto& cache : m_maxIndexCache) {
        if (cache.type == type)
            return cache.maxIndex;
    }
    return WTF::nullopt;
}

void WebGLBuffer::setCachedMaxIndex(GC3Denum type, unsigned value)
{
    for (auto& cache : m_maxIndexCache) {
        if (cache.type == type) {
            cache.maxIndex = value;
            return;
        }
    }
    m_maxIndexCache[m_nextAvailableCacheEntry].type = type;
    m_maxIndexCache[m_nextAvailableCacheEntry].maxIndex = value;
    m_nextAvailableCacheEntry = (m_nextAvailableCacheEntry + 1) % WTF_ARRAY_LENGTH(m_maxIndexCache);
}

void WebGLBuffer::setTarget(GC3Denum target, bool forWebGL2)
{
    // In WebGL, a buffer is bound to one target in its lifetime
    if (m_target)
        return;
    if (target == GraphicsContext3D::ARRAY_BUFFER || target == GraphicsContext3D::ELEMENT_ARRAY_BUFFER)
        m_target = target;
    else if (forWebGL2) {
#if ENABLE(WEBGL2)
        switch (target) {
        case GraphicsContext3D::COPY_READ_BUFFER:
        case GraphicsContext3D::COPY_WRITE_BUFFER:
        case GraphicsContext3D::PIXEL_PACK_BUFFER:
        case GraphicsContext3D::PIXEL_UNPACK_BUFFER:
        case GraphicsContext3D::TRANSFORM_FEEDBACK_BUFFER:
        case GraphicsContext3D::UNIFORM_BUFFER:
            m_target = target;
        }
#endif
    }
}

void WebGLBuffer::clearCachedMaxIndices()
{
    memset(m_maxIndexCache, 0, sizeof(m_maxIndexCache));
}

}

#endif // ENABLE(WEBGL)
