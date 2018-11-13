/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include <wtf/text/StringBuilder.h>

#include <wtf/dtoa.h>
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/IntegerToStringConversion.h>

namespace WTF {

static constexpr unsigned maxCapacity = String::MaxLength + 1;

static unsigned expandedCapacity(unsigned capacity, unsigned requiredLength)
{
    static const unsigned minimumCapacity = 16;
    return std::max(requiredLength, std::max(minimumCapacity, std::min(capacity * 2, maxCapacity)));
}

void StringBuilder::reifyString() const
{
    ASSERT(!hasOverflowed());
    // Check if the string already exists.
    if (!m_string.isNull()) {
        ASSERT(m_string.length() == m_length.unsafeGet<unsigned>());
        return;
    }

#if !ASSERT_DISABLED
    m_isReified = true;
#endif

    // Check for empty.
    if (!m_length) {
        m_string = StringImpl::empty();
        return;
    }

    // Must be valid in the buffer, take a substring (unless string fills the buffer).
    ASSERT(m_buffer && m_length.unsafeGet<unsigned>() <= m_buffer->length());
    if (m_length.unsafeGet<unsigned>() == m_buffer->length())
        m_string = m_buffer.get();
    else
        m_string = StringImpl::createSubstringSharingImpl(*m_buffer, 0, m_length.unsafeGet());
}

void StringBuilder::resize(unsigned newSize)
{
    if (hasOverflowed())
        return;

    // Check newSize < m_length, hence m_length > 0.
    unsigned oldLength = m_length.unsafeGet();
    ASSERT(newSize <= oldLength);
    if (newSize == oldLength)
        return;
    ASSERT(oldLength);

    m_length = newSize;
    ASSERT(!hasOverflowed());

    // If there is a buffer, we only need to duplicate it if it has more than one ref.
    if (m_buffer) {
        m_string = String(); // Clear the string to remove the reference to m_buffer if any before checking the reference count of m_buffer.
        if (!m_buffer->hasOneRef()) {
            if (m_buffer->is8Bit())
                allocateBuffer(m_buffer->characters8(), m_buffer->length());
            else
                allocateBuffer(m_buffer->characters16(), m_buffer->length());
        }
        ASSERT(hasOverflowed() || m_buffer->length() >= m_length.unsafeGet<unsigned>());
        return;
    }

    // Since m_length && !m_buffer, the string must be valid in m_string, and m_string.length() > 0.
    ASSERT(!m_string.isEmpty());
    ASSERT(oldLength == m_string.length());
    ASSERT(newSize < m_string.length());
    m_string = StringImpl::createSubstringSharingImpl(*m_string.impl(), 0, newSize);
}

// Allocate a new 8 bit buffer, copying in currentCharacters (these may come from either m_string
// or m_buffer, neither will be reassigned until the copy has completed).
void StringBuilder::allocateBuffer(const LChar* currentCharacters, unsigned requiredLength)
{
    ASSERT(!hasOverflowed());
    ASSERT(m_is8Bit);
    // Copy the existing data into a new buffer, set result to point to the end of the existing data.
    auto buffer = StringImpl::tryCreateUninitialized(requiredLength, m_bufferCharacters8);
    if (UNLIKELY(!buffer))
        return didOverflow();
    memcpy(m_bufferCharacters8, currentCharacters, static_cast<size_t>(m_length.unsafeGet()) * sizeof(LChar)); // This can't overflow.
    
    // Update the builder state.
    m_buffer = WTFMove(buffer);
    m_string = String();
    ASSERT(m_buffer->length() == requiredLength);
}

// Allocate a new 16 bit buffer, copying in currentCharacters (these may come from either m_string
// or m_buffer,  neither will be reassigned until the copy has completed).
void StringBuilder::allocateBuffer(const UChar* currentCharacters, unsigned requiredLength)
{
    ASSERT(!hasOverflowed());
    ASSERT(!m_is8Bit);
    // Copy the existing data into a new buffer, set result to point to the end of the existing data.
    auto buffer = StringImpl::tryCreateUninitialized(requiredLength, m_bufferCharacters16);
    if (UNLIKELY(!buffer))
        return didOverflow();
    memcpy(m_bufferCharacters16, currentCharacters, static_cast<size_t>(m_length.unsafeGet()) * sizeof(UChar)); // This can't overflow.
    
    // Update the builder state.
    m_buffer = WTFMove(buffer);
    m_string = String();
    ASSERT(m_buffer->length() == requiredLength);
}

// Allocate a new 16 bit buffer, copying in currentCharacters (which is 8 bit and may come
// from either m_string or m_buffer, neither will be reassigned until the copy has completed).
void StringBuilder::allocateBufferUpConvert(const LChar* currentCharacters, unsigned requiredLength)
{
    ASSERT(!hasOverflowed());
    ASSERT(m_is8Bit);
    unsigned length = m_length.unsafeGet();
    ASSERT(requiredLength <= maxCapacity && requiredLength >= length);
    // Copy the existing data into a new buffer, set result to point to the end of the existing data.
    auto buffer = StringImpl::tryCreateUninitialized(requiredLength, m_bufferCharacters16);
    if (UNLIKELY(!buffer))
        return didOverflow(); // Treat a failure to allcoate as an overflow.
    for (unsigned i = 0; i < length; ++i)
        m_bufferCharacters16[i] = currentCharacters[i];
    
    m_is8Bit = false;
    
    // Update the builder state.
    m_buffer = WTFMove(buffer);
    m_string = String();
    ASSERT(m_buffer->length() == requiredLength);
}

template <>
void StringBuilder::reallocateBuffer<LChar>(unsigned requiredLength)
{
    // If the buffer has only one ref (by this StringBuilder), reallocate it,
    // otherwise fall back to "allocate and copy" method.
    m_string = String();
    
    ASSERT(m_is8Bit);
    ASSERT(m_buffer->is8Bit());
    
    if (m_buffer->hasOneRef()) {
        auto expectedStringImpl = StringImpl::tryReallocate(m_buffer.releaseNonNull(), requiredLength, m_bufferCharacters8);
        if (UNLIKELY(!expectedStringImpl))
            return didOverflow();
        m_buffer = WTFMove(expectedStringImpl.value());
    } else
        allocateBuffer(m_buffer->characters8(), requiredLength);
    ASSERT(hasOverflowed() || m_buffer->length() == requiredLength);
}

template <>
void StringBuilder::reallocateBuffer<UChar>(unsigned requiredLength)
{
    // If the buffer has only one ref (by this StringBuilder), reallocate it,
    // otherwise fall back to "allocate and copy" method.
    m_string = String();
    
    if (m_buffer->is8Bit())
        allocateBufferUpConvert(m_buffer->characters8(), requiredLength);
    else if (m_buffer->hasOneRef()) {
        auto expectedStringImpl = StringImpl::tryReallocate(m_buffer.releaseNonNull(), requiredLength, m_bufferCharacters16);
        if (UNLIKELY(!expectedStringImpl))
            return didOverflow();
        m_buffer = WTFMove(expectedStringImpl.value());
    } else
        allocateBuffer(m_buffer->characters16(), requiredLength);
    ASSERT(hasOverflowed() || m_buffer->length() == requiredLength);
}

void StringBuilder::reserveCapacity(unsigned newCapacity)
{
    if (hasOverflowed())
        return;
    ASSERT(newCapacity <= String::MaxLength);
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
        unsigned length = m_length.unsafeGet();
        if (newCapacity > length) {
            if (!length) {
                LChar* nullPlaceholder = 0;
                allocateBuffer(nullPlaceholder, newCapacity);
            } else if (m_string.is8Bit())
                allocateBuffer(m_string.characters8(), newCapacity);
            else
                allocateBuffer(m_string.characters16(), newCapacity);
        }
    }
    ASSERT(hasOverflowed() || !newCapacity || m_buffer->length() >= newCapacity);
}

// Make 'length' additional capacity be available in m_buffer, update m_string & m_length,
// return a pointer to the newly allocated storage.
// Returns nullptr if the size of the new builder would have overflowed
template <typename CharType>
ALWAYS_INLINE CharType* StringBuilder::appendUninitialized(unsigned length)
{
    ASSERT(length);

    // Calculate the new size of the builder after appending.
    CheckedInt32 requiredLength = m_length + length;
    if (requiredLength.hasOverflowed()) {
        didOverflow();
        return nullptr;
    }

    if (m_buffer && (requiredLength.unsafeGet<unsigned>() <= m_buffer->length())) {
        // If the buffer is valid it must be at least as long as the current builder contents!
        ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
        unsigned currentLength = m_length.unsafeGet();
        m_string = String();
        m_length = requiredLength;
        return getBufferCharacters<CharType>() + currentLength;
    }
    
    return appendUninitializedSlow<CharType>(requiredLength.unsafeGet());
}

// Make 'length' additional capacity be available in m_buffer, update m_string & m_length,
// return a pointer to the newly allocated storage.
template <typename CharType>
CharType* StringBuilder::appendUninitializedSlow(unsigned requiredLength)
{
    ASSERT(!hasOverflowed());
    ASSERT(requiredLength);

    if (m_buffer) {
        // If the buffer is valid it must be at least as long as the current builder contents!
        ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
        
        reallocateBuffer<CharType>(expandedCapacity(capacity(), requiredLength));
    } else {
        ASSERT(m_string.length() == m_length.unsafeGet<unsigned>());
        allocateBuffer(m_length ? m_string.characters<CharType>() : 0, expandedCapacity(capacity(), requiredLength));
    }
    if (UNLIKELY(hasOverflowed()))
        return nullptr;

    CharType* result = getBufferCharacters<CharType>() + m_length.unsafeGet();
    m_length = requiredLength;
    ASSERT(!hasOverflowed());
    ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
    return result;
}

void StringBuilder::append(const UChar* characters, unsigned length)
{
    if (!length || hasOverflowed())
        return;

    ASSERT(characters);

    if (m_is8Bit) {
        if (length == 1 && !(*characters & ~0xff)) {
            // Append as 8 bit character
            LChar lChar = static_cast<LChar>(*characters);
            return append(&lChar, 1);
        }

        // Calculate the new size of the builder after appending.
        CheckedInt32 requiredLength = m_length + length;
        if (requiredLength.hasOverflowed())
            return didOverflow();
        
        if (m_buffer) {
            // If the buffer is valid it must be at least as long as the current builder contents!
            ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
            allocateBufferUpConvert(m_buffer->characters8(), expandedCapacity(capacity(), requiredLength.unsafeGet()));
        } else {
            ASSERT(m_string.length() == m_length.unsafeGet<unsigned>());
            allocateBufferUpConvert(m_string.isNull() ? 0 : m_string.characters8(), expandedCapacity(capacity(), requiredLength.unsafeGet()));
        }
        if (UNLIKELY(hasOverflowed()))
            return;

        memcpy(m_bufferCharacters16 + m_length.unsafeGet<unsigned>(), characters, static_cast<size_t>(length) * sizeof(UChar));
        m_length = requiredLength;
    } else {
        UChar* dest = appendUninitialized<UChar>(length);
        if (!dest)
            return;
        memcpy(dest, characters, static_cast<size_t>(length) * sizeof(UChar));
    }
    ASSERT(!hasOverflowed());
    ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
}

void StringBuilder::append(const LChar* characters, unsigned length)
{
    if (!length || hasOverflowed())
        return;

    ASSERT(characters);

    if (m_is8Bit) {
        LChar* dest = appendUninitialized<LChar>(length);
        if (!dest) {
            ASSERT(hasOverflowed());
            return;
        }
        if (length > 8)
            memcpy(dest, characters, static_cast<size_t>(length) * sizeof(LChar));
        else {
            const LChar* end = characters + length;
            while (characters < end)
                *(dest++) = *(characters++);
        }
    } else {
        UChar* dest = appendUninitialized<UChar>(length);
        if (!dest) {
            ASSERT(hasOverflowed());
            return;
        }
        const LChar* end = characters + length;
        while (characters < end)
            *(dest++) = *(characters++);
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
    if (hasOverflowed())
        return false;
    // Only shrink the buffer if it's less than 80% full. Need to tune this heuristic!
    unsigned length = m_length.unsafeGet();
    return m_buffer && m_buffer->length() > (length + (length >> 2));
}

void StringBuilder::shrinkToFit()
{
    if (canShrink()) {
        if (m_is8Bit)
            reallocateBuffer<LChar>(m_length.unsafeGet());
        else
            reallocateBuffer<UChar>(m_length.unsafeGet());
        ASSERT(!hasOverflowed());
        m_string = WTFMove(m_buffer);
    }
}

} // namespace WTF
