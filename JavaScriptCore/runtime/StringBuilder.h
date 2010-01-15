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

#ifndef StringBuilder_h
#define StringBuilder_h

#include <wtf/Vector.h>

namespace JSC {

class StringBuilder {
public:
    void append(const UChar u)
    {
        buffer.append(u);
    }

    void append(const char* str)
    {
        buffer.append(str, strlen(str));
    }

    void append(const char* str, size_t len)
    {
        buffer.reserveCapacity(buffer.size() + len);
        for (size_t i = 0; i < len; i++)
            buffer.append(static_cast<unsigned char>(str[i]));
    }

    void append(const UChar* str, size_t len)
    {
        buffer.append(str, len);
    }

    void append(const UString& str)
    {
        buffer.append(str.data(), str.size());
    }

    bool isEmpty() { return buffer.isEmpty(); }
    void reserveCapacity(size_t newCapacity) { buffer.reserveCapacity(newCapacity); }
    void resize(size_t size) { buffer.resize(size); }
    size_t size() const { return buffer.size(); }

    UChar operator[](size_t i) const { return buffer.at(i); }

    UString release()
    {
        buffer.shrinkToFit();
        return UString::adopt(buffer);
    }

private:
    Vector<UChar, 64> buffer;
};

}

#endif
