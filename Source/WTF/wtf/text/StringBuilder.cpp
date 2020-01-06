/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

namespace WTF {

static constexpr unsigned maxCapacity = String::MaxLength;

static unsigned expandedCapacity(unsigned capacity, unsigned requiredLength)
{
    static constexpr unsigned minimumCapacity = 16;
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

#if ASSERT_ENABLED
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
    std::memcpy(m_bufferCharacters8, currentCharacters, m_length.unsafeGet());
    
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
    std::memcpy(m_bufferCharacters16, currentCharacters, static_cast<size_t>(m_length.unsafeGet()) * sizeof(UChar)); // This can't overflow.
    
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

template<>
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

template<>
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
                LChar* nullPlaceholder = nullptr;
                allocateBuffer(nullPlaceholder, newCapacity);
            } else if (m_string.is8Bit())
                allocateBuffer(m_string.characters8(), newCapacity);
            else
                allocateBuffer(m_string.characters16(), newCapacity);
        }
    }
    ASSERT(hasOverflowed() || !newCapacity || m_buffer->length() >= newCapacity);
}

// Make 'additionalLength' additional capacity be available in m_buffer, update m_string & m_length,
// return a pointer to the newly allocated storage.
// Returns nullptr if the size of the new builder would have overflowed
template<typename CharacterType> ALWAYS_INLINE CharacterType* StringBuilder::extendBufferForAppending(unsigned additionalLength)
{
    ASSERT(additionalLength);

    // Calculate the new size of the builder after appending.
    CheckedInt32 requiredLength = m_length + additionalLength;
    if (requiredLength.hasOverflowed()) {
        didOverflow();
        return nullptr;
    }

    return extendBufferForAppendingWithoutOverflowCheck<CharacterType>(requiredLength);
}

template<typename CharacterType> ALWAYS_INLINE CharacterType* StringBuilder::extendBufferForAppendingWithoutOverflowCheck(CheckedInt32 requiredLength)
{
    ASSERT(!requiredLength.hasOverflowed());

    if (m_buffer && (requiredLength.unsafeGet<unsigned>() <= m_buffer->length())) {
        // If the buffer is valid it must be at least as long as the current builder contents!
        ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
        unsigned currentLength = m_length.unsafeGet();
        m_string = String();
        m_length = requiredLength;
        return getBufferCharacters<CharacterType>() + currentLength;
    }

    return extendBufferForAppendingSlowCase<CharacterType>(requiredLength.unsafeGet());
}

LChar* StringBuilder::extendBufferForAppending8(CheckedInt32 requiredLength)
{
    if (UNLIKELY(requiredLength.hasOverflowed())) {
        didOverflow();
        return nullptr;
    }
    return extendBufferForAppendingWithoutOverflowCheck<LChar>(requiredLength);
}

UChar* StringBuilder::extendBufferForAppending16(CheckedInt32 requiredLength)
{
    if (UNLIKELY(requiredLength.hasOverflowed())) {
        didOverflow();
        return nullptr;
    }
    if (m_is8Bit) {
        const LChar* characters;
        if (m_buffer) {
            ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
            characters = m_buffer->characters8();
        } else {
            ASSERT(m_string.length() == m_length.unsafeGet<unsigned>());
            characters = m_string.isNull() ? nullptr : m_string.characters8();
        }
        allocateBufferUpConvert(characters, expandedCapacity(capacity(), requiredLength.unsafeGet()));
        if (UNLIKELY(hasOverflowed()))
            return nullptr;
        unsigned oldLength = m_length.unsafeGet();
        m_length = requiredLength.unsafeGet();
        return m_bufferCharacters16 + oldLength;
    }
    return extendBufferForAppendingWithoutOverflowCheck<UChar>(requiredLength);
}

// Make 'requiredLength' capacity be available in m_buffer, update m_string & m_length,
// return a pointer to the newly allocated storage.
template<typename CharacterType> CharacterType* StringBuilder::extendBufferForAppendingSlowCase(unsigned requiredLength)
{
    ASSERT(!hasOverflowed());
    ASSERT(requiredLength);

    if (m_buffer) {
        // If the buffer is valid it must be at least as long as the current builder contents!
        ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
        
        reallocateBuffer<CharacterType>(expandedCapacity(capacity(), requiredLength));
    } else {
        ASSERT(m_string.length() == m_length.unsafeGet<unsigned>());
        allocateBuffer(m_length ? m_string.characters<CharacterType>() : nullptr, expandedCapacity(capacity(), requiredLength));
    }
    if (UNLIKELY(hasOverflowed()))
        return nullptr;

    CharacterType* result = getBufferCharacters<CharacterType>() + m_length.unsafeGet();
    m_length = requiredLength;
    ASSERT(!hasOverflowed());
    ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
    return result;
}

void StringBuilder::appendCharacters(const UChar* characters, unsigned length)
{
    if (!length || hasOverflowed())
        return;

    ASSERT(characters);

    if (m_is8Bit && length == 1 && isLatin1(characters[0])) {
        append(static_cast<LChar>(characters[0]));
        return;
    }

    // FIXME: Should we optimize memory by keeping the string 8-bit when all the characters are Latin-1?

    UChar* destination = extendBufferForAppending16(m_length + length);
    if (UNLIKELY(!destination))
        return;
    std::memcpy(destination, characters, static_cast<size_t>(length) * sizeof(UChar));
    ASSERT(!hasOverflowed());
    ASSERT(m_buffer->length() >= m_length.unsafeGet<unsigned>());
}

void StringBuilder::appendCharacters(const LChar* characters, unsigned length)
{
    if (!length || hasOverflowed())
        return;

    ASSERT(characters);

    if (m_is8Bit) {
        LChar* destination = extendBufferForAppending<LChar>(length);
        if (!destination) {
            ASSERT(hasOverflowed());
            return;
        }
        if (length > 8)
            std::memcpy(destination, characters, length);
        else {
            // FIXME: How strong is our evidence that this is faster than memcpy? What platforms is this true for?
            const LChar* end = characters + length;
            while (characters < end)
                *destination++ = *characters++;
        }
    } else {
        UChar* destination = extendBufferForAppending<UChar>(length);
        if (!destination) {
            ASSERT(hasOverflowed());
            return;
        }
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
        appendCharacters(reinterpret_cast<const LChar*>(characters), CFStringGetLength(string));
        return;
    }
    append(String(string));
}

#endif

void StringBuilder::appendNumber(int number)
{
    numberToStringSigned<StringBuilder>(number, this);
}

void StringBuilder::appendNumber(unsigned number)
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

void StringBuilder::appendNumber(float number)
{
    NumberToStringBuffer buffer;
    append(numberToString(number, buffer));
}

void StringBuilder::appendNumber(double number)
{
    NumberToStringBuffer buffer;
    append(numberToString(number, buffer));
}

bool StringBuilder::canShrink() const
{
    if (hasOverflowed())
        return false;
    // Only shrink the buffer if it's less than 80% full.
    // FIXME: We should tune this heuristic based some actual test case measurements.
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
