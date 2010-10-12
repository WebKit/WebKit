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
#include <wtf/text/WTFString.h>

namespace WTF {

class StringBuilder {
public:
    void append(const UChar u)
    {
        m_buffer.append(u);
    }

    void append(const char* str)
    {
        append(str, strlen(str));
    }

    void append(const char* str, size_t len)
    {
        reserveCapacity(m_buffer.size() + len);
        for (size_t i = 0; i < len; i++)
            m_buffer.append(static_cast<unsigned char>(str[i]));
    }

    void append(const UChar* str, size_t len)
    {
        m_buffer.append(str, len);
    }

    void append(const String& str)
    {
        m_buffer.append(str.characters(), str.length());
    }

    bool isEmpty() const { return m_buffer.isEmpty(); }
    void reserveCapacity(size_t newCapacity)
    {
        if (newCapacity < m_buffer.capacity())
            return;
        m_buffer.reserveCapacity(std::max(newCapacity, m_buffer.capacity() + m_buffer.capacity() / 4 + 1));
    }
    void resize(size_t size) { m_buffer.resize(size); }
    size_t size() const { return m_buffer.size(); }
    // FIXME: remove size(), above (strings have a length, not a size).
    // also, should return an unsigned, not a size_t.
    size_t length() const { return size(); }

    UChar operator[](size_t i) const { return m_buffer.at(i); }

    String toString()
    {
        m_buffer.shrinkToFit();
        ASSERT(m_buffer.data() || !m_buffer.size());
        return String::adopt(m_buffer);
    }

protected:
    Vector<UChar, 64> m_buffer;
};

} // namespace WTF

using WTF::StringBuilder;

#endif // StringBuilder_h
