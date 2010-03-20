/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebGLByteArray_h
#define WebGLByteArray_h

#include "WebGLArray.h"
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class WebGLArrayBuffer;

class WebGLByteArray : public WebGLArray {
  public:
    virtual bool isByteArray() const { return true; }

    static PassRefPtr<WebGLByteArray> create(unsigned length);
    static PassRefPtr<WebGLByteArray> create(signed char* array, unsigned length);
    static PassRefPtr<WebGLByteArray> create(PassRefPtr<WebGLArrayBuffer> buffer, unsigned byteOffset, unsigned length);

    char* data() { return static_cast<char*>(baseAddress()); }

    virtual unsigned length() const;
    virtual unsigned byteLength() const;
    virtual PassRefPtr<WebGLArray> slice(int start, int end);

    void set(unsigned index, double value)
    {
        if (index >= m_size)
            return;
        if (isnan(value)) // Clamp NaN to 0
            value = 0;
        if (value < std::numeric_limits<signed char>::min())
            value = std::numeric_limits<signed char>::min();
        else if (value > std::numeric_limits<signed char>::max())
            value = std::numeric_limits<signed char>::max();
        signed char* storage = static_cast<signed char*>(m_baseAddress);
        storage[index] = static_cast<signed char>(value);
    }

    bool get(unsigned index, signed char& result) const
    {
        if (index >= m_size)
            return false;
        signed char* storage = static_cast<signed char*>(m_baseAddress);
        result = storage[index];
        return true;
    }

    signed char get(unsigned index) const
    {
        return item(index);
    }

    signed char item(unsigned index) const
    {
        ASSERT(index < m_size);
        signed char* storage = static_cast<signed char*>(m_baseAddress);
        return storage[index];
    }

    void set(WebGLByteArray* array, unsigned offset, ExceptionCode& ec);

  private:
    WebGLByteArray(PassRefPtr<WebGLArrayBuffer> buffer,
                   unsigned offset,
                   unsigned length);
    unsigned m_size;
};

} // namespace WebCore

#endif // WebGLByteArray_h
