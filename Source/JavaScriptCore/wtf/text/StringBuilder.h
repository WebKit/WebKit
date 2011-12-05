/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
    StringBuilder()
        : m_length(0)
        , m_is8Bit(true)
        , m_bufferCharacters8(0)
    {
    }

    void append(const UChar*, unsigned);
    void append(const LChar*, unsigned);

    ALWAYS_INLINE void append(const char* characters, unsigned length) { append(reinterpret_cast<const LChar*>(characters), length); }

    void append(const String& string)
    {
        if (!string.length())
            return;

        // If we're appending to an empty string, and there is not buffer
        // (in case reserveCapacity has been called) then just retain the
        // string.
        if (!m_length && !m_buffer) {
            m_string = string;
            m_length = string.length();
            m_is8Bit = m_string.is8Bit();
            return;
        }

        if (string.is8Bit())
            append(string.characters8(), string.length());
        else
            append(string.characters16(), string.length());
    }

    void append(const char* characters)
    {
        if (characters)
            append(characters, strlen(characters));
    }

    void append(UChar c)
    {
        if (m_buffer && !m_is8Bit && m_length < m_buffer->length() && m_string.isNull())
            m_bufferCharacters16[m_length++] = c;
        else
            append(&c, 1);
    }

    void append(char c)
    {
        if (m_buffer && m_length < m_buffer->length() && m_string.isNull()) {
            if (m_is8Bit)
                m_bufferCharacters8[m_length++] = (LChar)c;
            else
                m_bufferCharacters16[m_length++] = (LChar)c;
        }
        else
            append(&c, 1);
    }

    String toString()
    {
        if (m_string.isNull()) {
            shrinkToFit();
            reifyString();
        }
        return m_string;
    }

    String toStringPreserveCapacity()
    {
        if (m_string.isNull())
            reifyString();
        return m_string;
    }

    unsigned length() const
    {
        return m_length;
    }

    bool isEmpty() const { return !length(); }

    void reserveCapacity(unsigned newCapacity);

    void resize(unsigned newSize);

    void shrinkToFit();

    UChar operator[](unsigned i) const
    {
        ASSERT(i < m_length);
        if (m_is8Bit)
            return characters8()[i];
        return characters16()[i];
    }

    const LChar* characters8() const
    {
        ASSERT(m_is8Bit);
        if (!m_length)
            return 0;
        if (!m_string.isNull())
            return m_string.characters8();
        ASSERT(m_buffer);
        return m_buffer->characters8();
    }

    const UChar* characters16() const
    {
        ASSERT(!m_is8Bit);
        if (!m_length)
            return 0;
        if (!m_string.isNull())
            return m_string.characters16();
        ASSERT(m_buffer);
        return m_buffer->characters16();
    }
    
    const UChar* characters() const
    {
        if (!m_length)
            return 0;
        if (!m_string.isNull())
            return m_string.characters();
        ASSERT(m_buffer);
        return m_buffer->characters();
    }
    
    void clear()
    {
        m_length = 0;
        m_string = String();
        m_buffer = 0;
    }

private:
    void allocateBuffer(const LChar* currentCharacters, unsigned requiredLength);
    void allocateBuffer(const UChar* currentCharacters, unsigned requiredLength);
    void allocateBufferUpConvert(const LChar* currentCharacters, unsigned requiredLength);
    template <typename CharType>
    void reallocateBuffer(unsigned requiredLength);
    template <typename CharType>
    ALWAYS_INLINE CharType* appendUninitialized(unsigned length);
    template <typename CharType>
    CharType* appendUninitializedSlow(unsigned length);
    template <typename CharType>
    ALWAYS_INLINE CharType * getBufferCharacters();
    void reifyString();

    unsigned m_length;
    String m_string;
    RefPtr<StringImpl> m_buffer;
    bool m_is8Bit;
    union {
        LChar* m_bufferCharacters8;
        UChar* m_bufferCharacters16;
    };
};

template <>
ALWAYS_INLINE LChar* StringBuilder::getBufferCharacters<LChar>()
{
    ASSERT(m_is8Bit);
    return m_bufferCharacters8;
}

template <>
ALWAYS_INLINE UChar* StringBuilder::getBufferCharacters<UChar>()
{
    ASSERT(!m_is8Bit);
    return m_bufferCharacters16;
}    

} // namespace WTF

using WTF::StringBuilder;

#endif // StringBuilder_h
