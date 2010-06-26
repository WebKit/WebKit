/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "WebGLBuffer.h"
#include "WebGLRenderingContext.h"

namespace WebCore {
    
PassRefPtr<WebGLBuffer> WebGLBuffer::create(WebGLRenderingContext* ctx)
{
    return adoptRef(new WebGLBuffer(ctx));
}

WebGLBuffer::WebGLBuffer(WebGLRenderingContext* ctx)
    : CanvasObject(ctx)
    , m_target(0)
    , m_byteLength(0)
    , m_nextAvailableCacheEntry(0)
{
    setObject(context()->graphicsContext3D()->createBuffer());
    clearCachedMaxIndices();
}

void WebGLBuffer::_deleteObject(Platform3DObject object)
{
    context()->graphicsContext3D()->deleteBuffer(object);
}

bool WebGLBuffer::associateBufferData(int size)
{
    switch (m_target) {
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER:
    case GraphicsContext3D::ARRAY_BUFFER:
        m_byteLength = size;
        return true;
    default:
        return false;
    }
}

bool WebGLBuffer::associateBufferData(ArrayBufferView* array)
{
    if (!m_target)
        return false;
    if (!array)
        return false;

    if (m_target == GraphicsContext3D::ELEMENT_ARRAY_BUFFER) {
        clearCachedMaxIndices();
        m_byteLength = array->byteLength();
        // We must always clone the incoming data because client-side
        // modifications without calling bufferData or bufferSubData
        // must never be able to change the validation results.
        m_elementArrayBuffer = ArrayBuffer::create(array->buffer().get());
        return true;
    }

    if (m_target == GraphicsContext3D::ARRAY_BUFFER) {
        m_byteLength = array->byteLength();
        return true;
    }
    
    return false;
}

bool WebGLBuffer::associateBufferSubData(long offset, ArrayBufferView* array)
{
    if (!m_target)
        return false;
    if (!array)
        return false;

    if (m_target == GraphicsContext3D::ELEMENT_ARRAY_BUFFER) {
        clearCachedMaxIndices();

        // We need to protect against integer overflow with these tests
        if (offset < 0)
            return false;
            
        unsigned long uoffset = static_cast<unsigned long>(offset);
        if (uoffset > m_byteLength || array->byteLength() > m_byteLength - uoffset)
            return false;
            
        memcpy(static_cast<unsigned char*>(m_elementArrayBuffer->data()) + offset, array->baseAddress(), array->byteLength());
        return true;
    }

    if (m_target == GraphicsContext3D::ARRAY_BUFFER)
        return array->byteLength() + offset <= m_byteLength;

    return false;
}

unsigned WebGLBuffer::byteLength() const
{
    return m_byteLength;
}

long WebGLBuffer::getCachedMaxIndex(unsigned long type)
{
    size_t numEntries = sizeof(m_maxIndexCache) / sizeof(MaxIndexCacheEntry);
    for (size_t i = 0; i < numEntries; i++)
        if (m_maxIndexCache[i].type == type)
            return m_maxIndexCache[i].maxIndex;
    return -1;
}

void WebGLBuffer::setCachedMaxIndex(unsigned long type, long value)
{
    int numEntries = sizeof(m_maxIndexCache) / sizeof(MaxIndexCacheEntry);
    for (int i = 0; i < numEntries; i++)
        if (m_maxIndexCache[i].type == type) {
            m_maxIndexCache[i].maxIndex = value;
            return;
        }
    m_maxIndexCache[m_nextAvailableCacheEntry].type = type;
    m_maxIndexCache[m_nextAvailableCacheEntry].maxIndex = value;
    m_nextAvailableCacheEntry = (m_nextAvailableCacheEntry + 1) % numEntries;
}

void WebGLBuffer::setTarget(unsigned long target)
{
    // In WebGL, a buffer is bound to one target in its lifetime
    if (m_target)
        return;
    if (target == GraphicsContext3D::ARRAY_BUFFER || target == GraphicsContext3D::ELEMENT_ARRAY_BUFFER)
        m_target = target;
}

void WebGLBuffer::clearCachedMaxIndices()
{
    memset(m_maxIndexCache, 0, sizeof(m_maxIndexCache));
}

}

#endif // ENABLE(3D_CANVAS)
