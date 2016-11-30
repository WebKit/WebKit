/*
 * Copyright (C) 2009-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#pragma once

#include <wtf/text/StringView.h>

namespace WTF {

class StringBuilder {
    // Disallow copying since it's expensive and we don't want anyone to do it by accident.
    WTF_MAKE_NONCOPYABLE(StringBuilder);

public:
    StringBuilder() = default;

    WTF_EXPORT_PRIVATE void append(const UChar*, unsigned);
    WTF_EXPORT_PRIVATE void append(const LChar*, unsigned);

    ALWAYS_INLINE void append(const char* characters, unsigned length) { append(reinterpret_cast<const LChar*>(characters), length); }

    void append(const AtomicString& atomicString) { append(atomicString.string()); }

    void append(const String& string)
    {
        unsigned length = string.length();
        if (!length)
            return;

        // If we're appending to an empty string, and there is not a buffer
        // (reserveCapacity has not been called) then just retain the string.
        if (!m_length && !m_buffer) {
            m_string = string;
            m_length = length;
            m_is8Bit = string.is8Bit();
            return;
        }

        if (string.is8Bit())
            append(string.characters8(), length);
        else
            append(string.characters16(), length);
    }

    void append(const StringBuilder& other)
    {
        if (!other.m_length)
            return;

        // If we're appending to an empty string, and there is not a buffer
        // (reserveCapacity has not been called) then just retain the string.
        if (!m_length && !m_buffer && !other.m_string.isNull()) {
            m_string = other.m_string;
            m_length = other.m_length;
            m_is8Bit = other.m_is8Bit;
            return;
        }

        if (other.is8Bit())
            append(other.characters8(), other.m_length);
        else
            append(other.characters16(), other.m_length);
    }

    void append(StringView stringView)
    {
        if (stringView.is8Bit())
            append(stringView.characters8(), stringView.length());
        else
            append(stringView.characters16(), stringView.length());
    }

#if USE(CF)
    WTF_EXPORT_PRIVATE void append(CFStringRef);
#endif

#if USE(CF) && defined(__OBJC__)
    void append(NSString *string) { append((__bridge CFStringRef)string); }
#endif
    
    void append(const String& string, unsigned offset, unsigned length)
    {
        ASSERT(offset <= string.length());
        ASSERT(offset + length <= string.length());

        if (!length)
            return;

        // If we're appending to an empty string, and there is not a buffer
        // (reserveCapacity has not been called) then just retain the string.
        if (!offset && !m_length && !m_buffer && length == string.length()) {
            m_string = string;
            m_length = length;
            m_is8Bit = string.is8Bit();
            return;
        }

        if (string.is8Bit())
            append(string.characters8() + offset, length);
        else
            append(string.characters16() + offset, length);
    }

    void append(const char* characters)
    {
        if (characters)
            append(characters, strlen(characters));
    }

    void append(UChar character)
    {
        if (m_buffer && m_length < m_buffer->length() && m_string.isNull()) {
            if (!m_is8Bit) {
                m_bufferCharacters16[m_length++] = character;
                return;
            }
            if (!(character & ~0xFF)) {
                m_bufferCharacters8[m_length++] = static_cast<LChar>(character);
                return;
            }
        }
        append(&character, 1);
    }

    void append(LChar character)
    {
        if (m_buffer && m_length < m_buffer->length() && m_string.isNull()) {
            if (m_is8Bit)
                m_bufferCharacters8[m_length++] = character;
            else
                m_bufferCharacters16[m_length++] = character;
        } else
            append(&character, 1);
    }

    void append(char character) { append(static_cast<LChar>(character)); }

    void append(UChar32 c)
    {
        if (U_IS_BMP(c)) {
            append(static_cast<UChar>(c));
            return;
        }
        append(U16_LEAD(c));
        append(U16_TRAIL(c));
    }

    WTF_EXPORT_PRIVATE void appendQuotedJSONString(StringView);

    template<unsigned charactersCount> ALWAYS_INLINE void appendLiteral(const char (&characters)[charactersCount]) { append(characters, charactersCount - 1); }

    WTF_EXPORT_PRIVATE void appendNumber(int);
    WTF_EXPORT_PRIVATE void appendNumber(unsigned int);
    WTF_EXPORT_PRIVATE void appendNumber(long);
    WTF_EXPORT_PRIVATE void appendNumber(unsigned long);
    WTF_EXPORT_PRIVATE void appendNumber(long long);
    WTF_EXPORT_PRIVATE void appendNumber(unsigned long long);
    WTF_EXPORT_PRIVATE void appendNumber(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);
    WTF_EXPORT_PRIVATE void appendECMAScriptNumber(double);
    WTF_EXPORT_PRIVATE void appendFixedWidthNumber(double, unsigned decimalPlaces);

    String toString()
    {
        shrinkToFit();
        if (m_string.isNull())
            reifyString();
        return m_string;
    }

    const String& toStringPreserveCapacity() const
    {
        if (m_string.isNull())
            reifyString();
        return m_string;
    }

    AtomicString toAtomicString() const
    {
        if (!m_length)
            return emptyAtom;

        // If the buffer is sufficiently over-allocated, make a new AtomicString from a copy so its buffer is not so large.
        if (canShrink()) {
            if (is8Bit())
                return AtomicString(characters8(), length());
            return AtomicString(characters16(), length());            
        }

        if (!m_string.isNull())
            return AtomicString(m_string);

        ASSERT(m_buffer);
        return AtomicString(m_buffer.get(), 0, m_length);
    }

    unsigned length() const { return m_length; }
    bool isEmpty() const { return !m_length; }

    WTF_EXPORT_PRIVATE void reserveCapacity(unsigned newCapacity);

    unsigned capacity() const { return m_buffer ? m_buffer->length() : m_length; }

    WTF_EXPORT_PRIVATE void resize(unsigned newSize);
    WTF_EXPORT_PRIVATE bool canShrink() const;
    WTF_EXPORT_PRIVATE void shrinkToFit();

    UChar operator[](unsigned i) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(i < m_length);
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
    
    bool is8Bit() const { return m_is8Bit; }

    void clear()
    {
        m_length = 0;
        m_string = String();
        m_buffer = nullptr;
        m_bufferCharacters8 = 0;
        m_is8Bit = true;
    }

    void swap(StringBuilder& stringBuilder)
    {
        std::swap(m_length, stringBuilder.m_length);
        m_string.swap(stringBuilder.m_string);
        m_buffer.swap(stringBuilder.m_buffer);
        std::swap(m_is8Bit, stringBuilder.m_is8Bit);
        std::swap(m_bufferCharacters8, stringBuilder.m_bufferCharacters8);
        ASSERT(!m_buffer || m_buffer->length() >= m_length);
    }

private:
    void allocateBuffer(const LChar* currentCharacters, unsigned requiredLength);
    void allocateBuffer(const UChar* currentCharacters, unsigned requiredLength);
    void allocateBufferUpconvert(const LChar* currentCharacters, unsigned requiredLength);
    template<typename CharacterType> void reallocateBuffer(unsigned requiredLength);
    UChar* appendUninitializedUpconvert(unsigned length);
    template<typename CharacterType> CharacterType* appendUninitialized(unsigned length);
    template<typename CharacterType> CharacterType* appendUninitializedSlow(unsigned length);
    template<typename CharacterType> CharacterType* bufferCharacters();
    WTF_EXPORT_PRIVATE void reifyString() const;

    unsigned m_length { 0 };
    mutable String m_string;
    RefPtr<StringImpl> m_buffer;
    bool m_is8Bit { true };
    union {
        LChar* m_bufferCharacters8 { nullptr };
        UChar* m_bufferCharacters16;
    };
};

template<typename StringType> bool equal(const StringBuilder&, const StringType&);
bool equal(const StringBuilder&, const String&); // Only needed because is8Bit dereferences nullptr when the string is null.
template<typename CharacterType> bool equal(const StringBuilder&, const CharacterType*, unsigned length);

bool operator==(const StringBuilder&, const StringBuilder&);
bool operator!=(const StringBuilder&, const StringBuilder&);
bool operator==(const StringBuilder&, const String&);
bool operator!=(const StringBuilder&, const String&);
bool operator==(const String&, const StringBuilder&);
bool operator!=(const String&, const StringBuilder&);

template<typename CharacterType> inline bool equal(const StringBuilder& s, const CharacterType* buffer, unsigned length)
{
    if (s.length() != length)
        return false;

    if (s.is8Bit())
        return equal(s.characters8(), buffer, length);

    return equal(s.characters16(), buffer, length);
}

template<typename StringType> inline bool equal(const StringBuilder& a, const StringType& b)
{
    return equalCommon(a, b);
}

inline bool equal(const StringBuilder& a, const String& b)
{
    // FIXME: This preserves historic behavior where an empty StringBuilder compares as equal
    // to a null String. This does not seem like desirable behavior since a null String and
    // an empty String do not compare as equal, but we have regression tests expecting it,
    // and there is a slim chance we also have code depending on it.
    return b.isNull() ? a.isEmpty() : equalCommon(a, b);
}

inline bool operator==(const StringBuilder& a, const StringBuilder& b) { return equal(a, b); }
inline bool operator!=(const StringBuilder& a, const StringBuilder& b) { return !equal(a, b); }
inline bool operator==(const StringBuilder& a, const String& b) { return equal(a, b); }
inline bool operator!=(const StringBuilder& a, const String& b) { return !equal(a, b); }
inline bool operator==(const String& a, const StringBuilder& b) { return equal(b, a); }
inline bool operator!=(const String& a, const StringBuilder& b) { return !equal(b, a); }

} // namespace WTF

using WTF::StringBuilder;
