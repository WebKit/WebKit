/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#include <wtf/CheckedArithmetic.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WTF {

// StringBuilder currently uses a Checked<int32_t, ConditionalCrashOnOverflow> for m_length.
// Ideally, we would want to make StringBuilder a template with an OverflowHandler parameter, and
// m_length can be instantiated based on that OverflowHandler instead. However, currently, we're
// not able to get clang to export explicitly instantiated template methods (which would be needed
// if we templatize StringBuilder). As a workaround, we use the ConditionalCrashOnOverflow handler
// instead to do a runtime check on whether it should crash on overflows or not.
//
// When clang is able to export explicitly instantiated template methods, we can templatize
// StringBuilder and do away with ConditionalCrashOnOverflow.
// See https://bugs.webkit.org/show_bug.cgi?id=191050.

class StringBuilder {
    // Disallow copying since it's expensive and we don't want code to do it by accident.
    WTF_MAKE_NONCOPYABLE(StringBuilder);

public:
    enum class OverflowHandler {
        CrashOnOverflow,
        RecordOverflow
    };

    StringBuilder(OverflowHandler handler = OverflowHandler::CrashOnOverflow)
        : m_bufferCharacters8(nullptr)
    {
        m_length.setShouldCrashOnOverflow(handler == OverflowHandler::CrashOnOverflow);
    }
    StringBuilder(StringBuilder&&) = default;
    StringBuilder& operator=(StringBuilder&&) = default;

    ALWAYS_INLINE void didOverflow() { m_length.overflowed(); }
    ALWAYS_INLINE bool hasOverflowed() const { return m_length.hasOverflowed(); }
    ALWAYS_INLINE bool crashesOnOverflow() const { return m_length.shouldCrashOnOverflow(); }

    WTF_EXPORT_PRIVATE void append(const UChar*, unsigned);
    WTF_EXPORT_PRIVATE void append(const LChar*, unsigned);

    ALWAYS_INLINE void append(const char* characters, unsigned length) { append(reinterpret_cast<const LChar*>(characters), length); }

    void append(const AtomicString& atomicString)
    {
        append(atomicString.string());
    }

    void append(const String& string)
    {
        if (hasOverflowed())
            return;

        if (!string.length())
            return;

        // If we're appending to an empty string, and there is not a buffer (reserveCapacity has not been called)
        // then just retain the string.
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

    void append(const StringBuilder& other)
    {
        if (hasOverflowed())
            return;
        if (other.hasOverflowed())
            return didOverflow();

        if (!other.m_length)
            return;

        // If we're appending to an empty string, and there is not a buffer (reserveCapacity has not been called)
        // then just retain the string.
        if (!m_length && !m_buffer && !other.m_string.isNull()) {
            m_string = other.m_string;
            m_length = other.m_length;
            return;
        }

        if (other.is8Bit())
            append(other.characters8(), other.m_length.unsafeGet());
        else
            append(other.characters16(), other.m_length.unsafeGet());
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
        if (!string.length())
            return;

        if ((offset + length) > string.length())
            return;

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

    void append(UChar c)
    {
        if (hasOverflowed())
            return;
        unsigned length = m_length.unsafeGet<unsigned>();
        if (m_buffer && length < m_buffer->length() && m_string.isNull()) {
            if (!m_is8Bit) {
                m_bufferCharacters16[length] = c;
                m_length++;
                return;
            }

            if (!(c & ~0xff)) {
                m_bufferCharacters8[length] = static_cast<LChar>(c);
                m_length++;
                return;
            }
        }
        append(&c, 1);
    }

    void append(LChar c)
    {
        if (hasOverflowed())
            return;
        unsigned length = m_length.unsafeGet<unsigned>();
        if (m_buffer && length < m_buffer->length() && m_string.isNull()) {
            if (m_is8Bit)
                m_bufferCharacters8[length] = c;
            else
                m_bufferCharacters16[length] = c;
            m_length++;
        } else
            append(&c, 1);
    }

    void append(char c)
    {
        append(static_cast<LChar>(c));
    }

    void append(UChar32 c)
    {
        if (U_IS_BMP(c)) {
            append(static_cast<UChar>(c));
            return;
        }
        append(U16_LEAD(c));
        append(U16_TRAIL(c));
    }

    WTF_EXPORT_PRIVATE void appendQuotedJSONString(const String&);

    template<unsigned characterCount>
    ALWAYS_INLINE void appendLiteral(const char (&characters)[characterCount]) { append(characters, characterCount - 1); }

    WTF_EXPORT_PRIVATE void appendNumber(int);
    WTF_EXPORT_PRIVATE void appendNumber(unsigned);
    WTF_EXPORT_PRIVATE void appendNumber(long);
    WTF_EXPORT_PRIVATE void appendNumber(unsigned long);
    WTF_EXPORT_PRIVATE void appendNumber(long long);
    WTF_EXPORT_PRIVATE void appendNumber(unsigned long long);
    // FIXME: Change appendNumber to be appendShortestFormNumber instead of appendFixedPrecisionNumber.
    void appendNumber(float);
    void appendNumber(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);

    WTF_EXPORT_PRIVATE void appendShortestFormNumber(float);
    WTF_EXPORT_PRIVATE void appendShortestFormNumber(double);
    WTF_EXPORT_PRIVATE void appendFixedPrecisionNumber(float, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);
    WTF_EXPORT_PRIVATE void appendFixedPrecisionNumber(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);
    WTF_EXPORT_PRIVATE void appendFixedWidthNumber(float, unsigned decimalPlaces);
    WTF_EXPORT_PRIVATE void appendFixedWidthNumber(double, unsigned decimalPlaces);

    // FIXME: Delete in favor of the name appendShortestFormNumber or just appendNumber.
    void appendECMAScriptNumber(float);
    void appendECMAScriptNumber(double);

    String toString()
    {
        if (!m_string.isNull()) {
            ASSERT(!m_buffer || m_isReified);
            ASSERT(!hasOverflowed());
            return m_string;
        }

        RELEASE_ASSERT(!hasOverflowed());
        shrinkToFit();
        reifyString();
        return m_string;
    }

    const String& toStringPreserveCapacity() const
    {
        RELEASE_ASSERT(!hasOverflowed());
        if (m_string.isNull())
            reifyString();
        return m_string;
    }

    AtomicString toAtomicString() const
    {
        RELEASE_ASSERT(!hasOverflowed());
        if (!m_length)
            return emptyAtom();

        // If the buffer is sufficiently over-allocated, make a new AtomicString from a copy so its buffer is not so large.
        if (canShrink()) {
            if (is8Bit())
                return AtomicString(characters8(), length());
            return AtomicString(characters16(), length());            
        }

        if (!m_string.isNull())
            return AtomicString(m_string);

        ASSERT(m_buffer);
        return AtomicString(m_buffer.get(), 0, m_length.unsafeGet());
    }

    unsigned length() const
    {
        RELEASE_ASSERT(!hasOverflowed());
        return m_length.unsafeGet();
    }

    bool isEmpty() const { return !m_length; }

    WTF_EXPORT_PRIVATE void reserveCapacity(unsigned newCapacity);

    unsigned capacity() const
    {
        RELEASE_ASSERT(!hasOverflowed());
        return m_buffer ? m_buffer->length() : m_length.unsafeGet();
    }

    WTF_EXPORT_PRIVATE void resize(unsigned newSize);

    WTF_EXPORT_PRIVATE bool canShrink() const;

    WTF_EXPORT_PRIVATE void shrinkToFit();

    UChar operator[](unsigned i) const
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!hasOverflowed() && i < m_length.unsafeGet<unsigned>());
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
        ASSERT(!m_buffer || hasOverflowed() || m_buffer->length() >= m_length.unsafeGet<unsigned>());
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
    WTF_EXPORT_PRIVATE void reifyString() const;

    mutable String m_string;
    RefPtr<StringImpl> m_buffer;
    union {
        LChar* m_bufferCharacters8;
        UChar* m_bufferCharacters16;
    };
    static_assert(String::MaxLength == std::numeric_limits<int32_t>::max(), "");
    Checked<int32_t, ConditionalCrashOnOverflow> m_length;
    bool m_is8Bit { true };
#if !ASSERT_DISABLED
    mutable bool m_isReified { false };
#endif
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

inline void StringBuilder::appendNumber(float number)
{
    appendFixedPrecisionNumber(number);
}

inline void StringBuilder::appendNumber(double number, unsigned precision, TrailingZerosTruncatingPolicy policy)
{
    appendFixedPrecisionNumber(number, precision, policy);
}

inline void StringBuilder::appendECMAScriptNumber(float number)
{
    // FIXME: This preserves existing behavior but is not what we want.
    // In the future, this should either be a compilation error or call appendShortestFormNumber without converting to double.
    appendShortestFormNumber(static_cast<double>(number));
}

inline void StringBuilder::appendECMAScriptNumber(double number)
{
    appendShortestFormNumber(number);
}

template <typename CharType>
bool equal(const StringBuilder& s, const CharType* buffer, unsigned length)
{
    if (s.length() != length)
        return false;

    if (s.is8Bit())
        return equal(s.characters8(), buffer, length);

    return equal(s.characters16(), buffer, length);
}

template <typename StringType>
bool equal(const StringBuilder& a, const StringType& b)
{
    if (a.length() != b.length())
        return false;

    if (!a.length())
        return true;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return equal(a.characters8(), b.characters8(), a.length());
        return equal(a.characters8(), b.characters16(), a.length());
    }

    if (b.is8Bit())
        return equal(a.characters16(), b.characters8(), a.length());
    return equal(a.characters16(), b.characters16(), a.length());
}

inline bool operator==(const StringBuilder& a, const StringBuilder& b) { return equal(a, b); }
inline bool operator!=(const StringBuilder& a, const StringBuilder& b) { return !equal(a, b); }
inline bool operator==(const StringBuilder& a, const String& b) { return equal(a, b); }
inline bool operator!=(const StringBuilder& a, const String& b) { return !equal(a, b); }
inline bool operator==(const String& a, const StringBuilder& b) { return equal(b, a); }
inline bool operator!=(const String& a, const StringBuilder& b) { return !equal(b, a); }

template<> struct IntegerToStringConversionTrait<StringBuilder> {
    using ReturnType = void;
    using AdditionalArgumentType = StringBuilder;
    static void flush(LChar* characters, unsigned length, StringBuilder* stringBuilder) { stringBuilder->append(characters, length); }
};

} // namespace WTF

using WTF::StringBuilder;
