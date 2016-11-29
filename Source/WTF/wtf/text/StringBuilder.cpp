/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StringBuilder.h"

#include "IntegerToStringConversion.h"
#include "MathExtras.h"
#include <wtf/dtoa.h>

namespace WTF {

static unsigned expandedCapacity(unsigned capacity, unsigned requiredLength)
{
    static const unsigned minimumCapacity = 16;
    return std::max(requiredLength, std::max(minimumCapacity, capacity * 2));
}

template<> ALWAYS_INLINE LChar* StringBuilder::bufferCharacters<LChar>()
{
    ASSERT(m_is8Bit);
    return m_bufferCharacters8;
}

template<> ALWAYS_INLINE UChar* StringBuilder::bufferCharacters<UChar>()
{
    ASSERT(!m_is8Bit);
    return m_bufferCharacters16;
}

void StringBuilder::reifyString() const
{
    // Check if the string already exists.
    if (!m_string.isNull()) {
        ASSERT(m_string.length() == m_length);
        return;
    }

    // Check for empty.
    if (!m_length) {
        m_string = StringImpl::empty();
        return;
    }

    // Must be valid in the buffer, take a substring (unless string fills the buffer).
    ASSERT(m_buffer && m_length <= m_buffer->length());
    if (m_length == m_buffer->length())
        m_string = m_buffer.get();
    else
        m_string = StringImpl::createSubstringSharingImpl(*m_buffer, 0, m_length);
}

void StringBuilder::resize(unsigned newSize)
{
    // Check newSize < m_length, hence m_length > 0.
    ASSERT(newSize <= m_length);
    if (newSize == m_length)
        return;
    ASSERT(m_length);

    // If there is a buffer, we only need to duplicate it if it has more than one ref.
    if (m_buffer) {
        m_string = String(); // Clear the string to remove the reference to m_buffer if any before checking the reference count of m_buffer.
        if (!m_buffer->hasOneRef()) {
            if (m_buffer->is8Bit())
                allocateBuffer(m_buffer->characters8(), m_buffer->length());
            else
                allocateBuffer(m_buffer->characters16(), m_buffer->length());
        }
        m_length = newSize;
        ASSERT(m_buffer->length() >= m_length);
        return;
    }

    // Since m_length && !m_buffer, the string must be valid in m_string, and m_string.length() > 0.
    ASSERT(!m_string.isEmpty());
    ASSERT(m_length == m_string.length());
    ASSERT(newSize < m_string.length());
    m_length = newSize;
    m_string = StringImpl::createSubstringSharingImpl(*m_string.impl(), 0, newSize);
}

// Allocate a new 8 bit buffer, copying in currentCharacters (these may come from either m_string
// or m_buffer, neither will be reassigned until the copy has completed).
void StringBuilder::allocateBuffer(const LChar* currentCharacters, unsigned requiredLength)
{
    ASSERT(m_is8Bit);

    // Copy the existing data into a new buffer, set result to point to the end of the existing data.
    auto buffer = StringImpl::createUninitialized(requiredLength, m_bufferCharacters8);
    memcpy(m_bufferCharacters8, currentCharacters, static_cast<size_t>(m_length) * sizeof(LChar)); // This can't overflow.
    
    // Update the builder state.
    m_buffer = WTFMove(buffer);
    m_string = String();
    ASSERT(m_buffer->length() == requiredLength);
}

// Allocate a new 16 bit buffer, copying in currentCharacters (these may come from either m_string
// or m_buffer,  neither will be reassigned until the copy has completed).
void StringBuilder::allocateBuffer(const UChar* currentCharacters, unsigned requiredLength)
{
    ASSERT(!m_is8Bit);

    // Copy the existing data into a new buffer, set result to point to the end of the existing data.
    auto buffer = StringImpl::createUninitialized(requiredLength, m_bufferCharacters16);
    memcpy(m_bufferCharacters16, currentCharacters, static_cast<size_t>(m_length) * sizeof(UChar)); // This can't overflow.
    
    // Update the builder state.
    m_buffer = WTFMove(buffer);
    m_string = String();
    ASSERT(m_buffer->length() == requiredLength);
}

// Allocate a new 16 bit buffer, copying in currentCharacters (which is 8 bit and may come
// from either m_string or m_buffer, neither will be reassigned until the copy has completed).
void StringBuilder::allocateBufferUpconvert(const LChar* currentCharacters, unsigned requiredLength)
{
    ASSERT(m_is8Bit);
    ASSERT(requiredLength >= m_length);

    // Copy the existing data into a new buffer, set result to point to the end of the existing data.
    auto buffer = StringImpl::createUninitialized(requiredLength, m_bufferCharacters16);
    for (unsigned i = 0; i < m_length; ++i)
        m_bufferCharacters16[i] = currentCharacters[i];
    
    m_is8Bit = false;
    
    // Update the builder state.
    m_buffer = WTFMove(buffer);
    m_string = String();
    ASSERT(m_buffer->length() == requiredLength);
}

template<> void StringBuilder::reallocateBuffer<LChar>(unsigned requiredLength)
{
    // If the buffer has only one ref (by this StringBuilder), reallocate it,
    // otherwise fall back to "allocate and copy" method.
    m_string = String();
    
    ASSERT(m_is8Bit);
    ASSERT(m_buffer->is8Bit());
    
    if (m_buffer->hasOneRef())
        m_buffer = StringImpl::reallocate(m_buffer.releaseNonNull(), requiredLength, m_bufferCharacters8);
    else
        allocateBuffer(m_buffer->characters8(), requiredLength);
    ASSERT(m_buffer->length() == requiredLength);
}

template<> void StringBuilder::reallocateBuffer<UChar>(unsigned requiredLength)
{
    // If the buffer has only one ref (by this StringBuilder), reallocate it,
    // otherwise fall back to "allocate and copy" method.
    m_string = String();
    
    if (m_buffer->is8Bit())
        allocateBufferUpconvert(m_buffer->characters8(), requiredLength);
    else if (m_buffer->hasOneRef())
        m_buffer = StringImpl::reallocate(m_buffer.releaseNonNull(), requiredLength, m_bufferCharacters16);
    else
        allocateBuffer(m_buffer->characters16(), requiredLength);
    ASSERT(m_buffer->length() == requiredLength);
}

void StringBuilder::reserveCapacity(unsigned newCapacity)
{
    if (m_buffer) {
        // If there is already a buffer, then grow if necessary.
        if (newCapacity > m_buffer->length()) {
            if (m_buffer->is8Bit())
                reallocateBuffer<LChar>(newCapacity);
            else
                reallocateBuffer<UChar>(newCapacity);
        }
    } else {
        // Grow the string, if necessary.
        if (newCapacity > m_length) {
            if (!m_length) {
                LChar* nullPlaceholder = nullptr;
                allocateBuffer(nullPlaceholder, newCapacity);
            } else if (m_string.is8Bit())
                allocateBuffer(m_string.characters8(), newCapacity);
            else
                allocateBuffer(m_string.characters16(), newCapacity);
        }
    }
    ASSERT(!newCapacity || m_buffer->length() >= newCapacity);
}

// Make 'length' additional capacity be available in m_buffer, update m_string & m_length,
// return a pointer to the newly allocated storage.
template<typename CharacterType> ALWAYS_INLINE CharacterType* StringBuilder::appendUninitialized(unsigned length)
{
    ASSERT(length);

    // Calculate the new size of the builder after appending.
    unsigned requiredLength = length + m_length;
    if (requiredLength < length)
        CRASH();

    if ((m_buffer) && (requiredLength <= m_buffer->length())) {
        // If the buffer is valid it must be at least as long as the current builder contents!
        ASSERT(m_buffer->length() >= m_length);
        unsigned currentLength = m_length;
        m_string = String();
        m_length = requiredLength;
        return bufferCharacters<CharacterType>() + currentLength;
    }

    return appendUninitializedSlow<CharacterType>(requiredLength);
}

// Make 'length' additional capacity be available in m_buffer, update m_string & m_length,
// return a pointer to the newly allocated storage.
template<typename CharacterType> CharacterType* StringBuilder::appendUninitializedSlow(unsigned requiredLength)
{
    ASSERT(requiredLength);

    if (m_buffer) {
        // If the buffer is valid it must be at least as long as the current builder contents!
        ASSERT(m_buffer->length() >= m_length);
        reallocateBuffer<CharacterType>(expandedCapacity(capacity(), requiredLength));
    } else {
        ASSERT(m_string.length() == m_length);
        allocateBuffer(m_length ? m_string.characters<CharacterType>() : nullptr, expandedCapacity(capacity(), requiredLength));
    }
    
    auto* result = bufferCharacters<CharacterType>() + m_length;
    m_length = requiredLength;
    ASSERT(m_buffer->length() >= m_length);
    return result;
}

inline UChar* StringBuilder::appendUninitializedUpconvert(unsigned length)
{
    unsigned requiredLength = length + m_length;
    if (requiredLength < length)
        CRASH();

    if (m_buffer) {
        // If the buffer is valid it must be at least as long as the current builder contents!
        ASSERT(m_buffer->length() >= m_length);
        allocateBufferUpconvert(m_buffer->characters8(), expandedCapacity(capacity(), requiredLength));
    } else {
        ASSERT(m_string.length() == m_length);
        allocateBufferUpconvert(m_string.isNull() ? nullptr : m_string.characters8(), expandedCapacity(capacity(), requiredLength));
    }

    auto* result = m_bufferCharacters16 + m_length;
    m_length = requiredLength;
    return result;
}

void StringBuilder::append(const UChar* characters, unsigned length)
{
    if (!length)
        return;

    ASSERT(characters);

    if (m_is8Bit) {
        if (length == 1 && !(*characters & ~0xFF)) {
            // Append as 8 bit character
            LChar lChar = static_cast<LChar>(*characters);
            append(&lChar, 1);
            return;
        }
        memcpy(appendUninitializedUpconvert(length), characters, static_cast<size_t>(length) * sizeof(UChar));
    } else
        memcpy(appendUninitialized<UChar>(length), characters, static_cast<size_t>(length) * sizeof(UChar));

    ASSERT(m_buffer->length() >= m_length);
}

void StringBuilder::append(const LChar* characters, unsigned length)
{
    if (!length)
        return;
    ASSERT(characters);

    if (m_is8Bit) {
        auto* destination = appendUninitialized<LChar>(length);
        // FIXME: How did we determine a threshold of 8 here was the right one?
        // Also, this kind of optimization could be useful anywhere else we have a
        // performance-sensitive code path that calls memcpy.
        if (length > 8)
            memcpy(destination, characters, length);
        else {
            const LChar* end = characters + length;
            while (characters < end)
                *destination++ = *characters++;
        }
    } else {
        auto* destination = appendUninitialized<UChar>(length);
        const LChar* end = characters + length;
        while (characters < end)
            *destination++ = *characters++;
    }
}

#if USE(CF)

void StringBuilder::append(CFStringRef string)
{
    // Fast path: avoid constructing a temporary String when possible.
    if (auto* characters = CFStringGetCStringPtr(string, kCFStringEncodingISOLatin1)) {
        append(reinterpret_cast<const LChar*>(characters), CFStringGetLength(string));
        return;
    }
    append(String(string));
}

#endif

void StringBuilder::appendNumber(int number)
{
    numberToStringSigned<StringBuilder>(number, this);
}

void StringBuilder::appendNumber(unsigned int number)
{
    numberToStringUnsigned<StringBuilder>(number, this);
}

void StringBuilder::appendNumber(long number)
{
    numberToStringSigned<StringBuilder>(number, this);
}

void StringBuilder::appendNumber(unsigned long number)
{
    numberToStringUnsigned<StringBuilder>(number, this);
}

void StringBuilder::appendNumber(long long number)
{
    numberToStringSigned<StringBuilder>(number, this);
}

void StringBuilder::appendNumber(unsigned long long number)
{
    numberToStringUnsigned<StringBuilder>(number, this);
}

void StringBuilder::appendNumber(double number, unsigned precision, TrailingZerosTruncatingPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    append(numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TruncateTrailingZeros));
}

void StringBuilder::appendECMAScriptNumber(double number)
{
    NumberToStringBuffer buffer;
    append(numberToString(number, buffer));
}

void StringBuilder::appendFixedWidthNumber(double number, unsigned decimalPlaces)
{
    NumberToStringBuffer buffer;
    append(numberToFixedWidthString(number, decimalPlaces, buffer));
}

bool StringBuilder::canShrink() const
{
    // Only shrink the buffer if it's less than 80% full. Need to tune this heuristic!
    return m_buffer && m_buffer->length() > (m_length + (m_length >> 2));
}

void StringBuilder::shrinkToFit()
{
    if (canShrink()) {
        if (m_is8Bit)
            reallocateBuffer<LChar>(m_length);
        else
            reallocateBuffer<UChar>(m_length);
        m_string = WTFMove(m_buffer);
    }
}

template<typename LengthType, typename CharacterType> static LengthType quotedJSONStringLength(const CharacterType* input, unsigned length)
{
    LengthType quotedLength = 2;
    for (unsigned i = 0; i < length; ++i) {
        auto character = input[i];
        if (LIKELY(character > 0x1F)) {
            switch (character) {
            case '"':
            case '\\':
                quotedLength += 2;
                break;
            default:
                ++quotedLength;
                break;
            }
        } else {
            switch (character) {
            case '\t':
            case '\r':
            case '\n':
            case '\f':
            case '\b':
                quotedLength += 2;
                break;
            default:
                quotedLength += 6;
            }
        }
    }
    return quotedLength;
}

template<typename CharacterType> static inline unsigned quotedJSONStringLength(const CharacterType* input, unsigned length)
{
    constexpr auto maxSafeLength = (std::numeric_limits<unsigned>::max() - 2) / 6;
    if (length <= maxSafeLength)
        return quotedJSONStringLength<unsigned>(input, length);
    return quotedJSONStringLength<Checked<unsigned>>(input, length).unsafeGet();
}

template<typename OutputCharacterType, typename InputCharacterType> static inline void appendQuotedJSONStringInternal(OutputCharacterType* output, const InputCharacterType* input, unsigned length)
{
    *output++ = '"';
    for (unsigned i = 0; i < length; ++i) {
        auto character = input[i];
        if (LIKELY(character > 0x1F)) {
            if (UNLIKELY(character == '"' || character == '\\'))
                *output++ = '\\';
            *output++ = character;
            continue;
        }
        switch (character) {
        case '\t':
            *output++ = '\\';
            *output++ = 't';
            break;
        case '\r':
            *output++ = '\\';
            *output++ = 'r';
            break;
        case '\n':
            *output++ = '\\';
            *output++ = 'n';
            break;
        case '\f':
            *output++ = '\\';
            *output++ = 'f';
            break;
        case '\b':
            *output++ = '\\';
            *output++ = 'b';
            break;
        default:
            ASSERT(!(character & ~0xFF));
            *output++ = '\\';
            *output++ = 'u';
            *output++ = '0';
            *output++ = '0';
            *output++ = upperNibbleToLowercaseASCIIHexDigit(character);
            *output++ = lowerNibbleToLowercaseASCIIHexDigit(character);
            break;
        }
    }
    *output = '"';
}

void StringBuilder::appendQuotedJSONString(StringView string)
{
    unsigned length = string.length();
    if (string.is8Bit()) {
        auto* characters = string.characters8();
        if (m_is8Bit)
            appendQuotedJSONStringInternal(appendUninitialized<LChar>(quotedJSONStringLength(characters, length)), characters, length);
        else
            appendQuotedJSONStringInternal(appendUninitialized<UChar>(quotedJSONStringLength(characters, length)), characters, length);
    } else {
        auto* characters = string.characters16();
        if (m_is8Bit)
            appendQuotedJSONStringInternal(appendUninitializedUpconvert(quotedJSONStringLength(characters, length)), characters, length);
        else
            appendQuotedJSONStringInternal(appendUninitialized<UChar>(quotedJSONStringLength(characters, length)), characters, length);
    }
}

} // namespace WTF
