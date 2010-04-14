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

#ifndef WebGLArray_h
#define WebGLArray_h

#include <algorithm>
#include "ExceptionCode.h"
#include <limits.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include "WebGLArrayBuffer.h"

namespace WebCore {

class WebGLArray : public RefCounted<WebGLArray> {
  public:
    virtual bool isByteArray() const { return false; }
    virtual bool isUnsignedByteArray() const { return false; }
    virtual bool isShortArray() const { return false; }
    virtual bool isUnsignedShortArray() const { return false; }
    virtual bool isIntArray() const { return false; }
    virtual bool isUnsignedIntArray() const { return false; }
    virtual bool isFloatArray() const { return false; }

    PassRefPtr<WebGLArrayBuffer> buffer() {
        return m_buffer;
    }

    void* baseAddress() {
        return m_baseAddress;
    }

    unsigned byteOffset() const {
        return m_byteOffset;
    }

    virtual unsigned length() const = 0;
    virtual unsigned byteLength() const = 0;
    virtual PassRefPtr<WebGLArray> slice(int start, int end) = 0;

    virtual ~WebGLArray();

  protected:
    WebGLArray(PassRefPtr<WebGLArrayBuffer> buffer, unsigned byteOffset);

    void setImpl(WebGLArray* array, unsigned byteOffset, ExceptionCode& ec);

    void calculateOffsetAndLength(int start, int end, unsigned arraySize,
                                  unsigned* offset, unsigned* length);

    // Helper to verify that a given sub-range of an ArrayBuffer is
    // within range.
    template <typename T>
    static bool verifySubRange(PassRefPtr<WebGLArrayBuffer> buffer,
                               unsigned byteOffset,
                               unsigned numElements)
    {
        if (!buffer)
            return false;
        if (sizeof(T) > 1 && byteOffset % sizeof(T))
            return false;
        if (byteOffset > buffer->byteLength())
            return false;
        unsigned remainingElements = (buffer->byteLength() - byteOffset) / sizeof(T);
        if (numElements > remainingElements)
            return false;
        return true;
    }

    // Input offset is in number of elements from this array's view;
    // output offset is in number of bytes from the underlying buffer's view.
    template <typename T>
    static void clampOffsetAndNumElements(PassRefPtr<WebGLArrayBuffer> buffer,
                                          unsigned arrayByteOffset,
                                          unsigned *offset,
                                          unsigned *numElements)
    {
        unsigned maxOffset = (UINT_MAX - arrayByteOffset) / sizeof(T);
        if (*offset > maxOffset) {
            *offset = buffer->byteLength();
            *numElements = 0;
            return;
        }
        *offset = arrayByteOffset + *offset * sizeof(T);
        *offset = std::min(buffer->byteLength(), *offset);
        unsigned remainingElements = (buffer->byteLength() - *offset) / sizeof(T);
        *numElements = std::min(remainingElements, *numElements);
    }

    // This is the address of the WebGLArrayBuffer's storage, plus the byte offset.
    void* m_baseAddress;

    unsigned m_byteOffset;

  private:
    RefPtr<WebGLArrayBuffer> m_buffer;
};

} // namespace WebCore

#endif // WebGLArray_h
