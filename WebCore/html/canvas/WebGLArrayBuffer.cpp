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

#include "WebGLArrayBuffer.h"

#include <wtf/RefPtr.h>

namespace WebCore {

PassRefPtr<WebGLArrayBuffer> WebGLArrayBuffer::create(unsigned numElements, unsigned elementByteSize)
{
    void* data = tryAllocate(numElements, elementByteSize);
    if (!data)
        return 0;
    return adoptRef(new WebGLArrayBuffer(data, numElements * elementByteSize));
}

PassRefPtr<WebGLArrayBuffer> WebGLArrayBuffer::create(WebGLArrayBuffer* other)
{
    void* data = tryAllocate(other->byteLength(), 1);
    if (!data)
        return 0;
    RefPtr<WebGLArrayBuffer> buffer = adoptRef(new WebGLArrayBuffer(data, other->byteLength()));
    memcpy(buffer->data(), other->data(), other->byteLength());
    return buffer.release();
}

WebGLArrayBuffer::WebGLArrayBuffer(void* data, unsigned sizeInBytes)
    : m_sizeInBytes(sizeInBytes)
    , m_data(data) {
}

void* WebGLArrayBuffer::data() {
    return m_data;
}

const void* WebGLArrayBuffer::data() const {
    return m_data;
}

unsigned WebGLArrayBuffer::byteLength() const {
    return m_sizeInBytes;
}

WebGLArrayBuffer::~WebGLArrayBuffer() {
    WTF::fastFree(m_data);
}

void* WebGLArrayBuffer::tryAllocate(unsigned numElements, unsigned elementByteSize) {
    void* result;
    // Do not allow 32-bit overflow of the total size
    if (numElements) {
        unsigned totalSize = numElements * elementByteSize;
        if (totalSize / numElements != elementByteSize)
            return 0;
    }
    if (WTF::tryFastCalloc(numElements, elementByteSize).getValue(result))
        return result;
    return 0;
}

}

#endif // ENABLE(3D_CANVAS)
