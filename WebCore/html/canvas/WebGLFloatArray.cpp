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

#include "WebGLFloatArray.h"

namespace WebCore {
    
    PassRefPtr<WebGLFloatArray> WebGLFloatArray::create(unsigned length)
    {
        RefPtr<WebGLArrayBuffer> buffer = WebGLArrayBuffer::create(length * sizeof(float));
        return create(buffer, 0, length);
    }

    PassRefPtr<WebGLFloatArray> WebGLFloatArray::create(float* array, unsigned length)
    {
        RefPtr<WebGLFloatArray> a = WebGLFloatArray::create(length);
        for (unsigned i = 0; i < length; ++i)
            a->set(i, array[i]);
        return a;
    }

    PassRefPtr<WebGLFloatArray> WebGLFloatArray::create(PassRefPtr<WebGLArrayBuffer> buffer, int offset, unsigned length)
    {
        // Make sure the offset results in valid alignment.
        if ((offset % sizeof(float)) != 0)
            return NULL;

        if (buffer) {
            // Check to make sure we are talking about a valid region of
            // the given WebGLArrayBuffer's storage.
            if ((offset + (length * sizeof(float))) > buffer->byteLength())
                return NULL;
        }
        return adoptRef(new WebGLFloatArray(buffer, offset, length));
    }
    
    WebGLFloatArray::WebGLFloatArray(PassRefPtr<WebGLArrayBuffer> buffer, int offset, unsigned length)
        : WebGLArray(buffer, offset)
        , m_size(length)
    {
    }
    
    unsigned WebGLFloatArray::length() const {
        return m_size;
    }
        
    unsigned WebGLFloatArray::sizeInBytes() const {
        return length() * sizeof(float);
    }
}

#endif // ENABLE(3D_CANVAS)
