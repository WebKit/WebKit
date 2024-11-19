/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#include <wtf/text/StringBuilderInternals.h>

namespace WTF {

static constexpr unsigned maxCapacity = String::MaxLength;

unsigned StringBuilder::expandedCapacity(unsigned capacity, unsigned requiredCapacity)
{
    static constexpr unsigned minimumCapacity = 16;
    return std::max(requiredCapacity, std::max(minimumCapacity, std::min(capacity * 2, maxCapacity)));
}

void StringBuilder::didOverflow()
{
    if (m_shouldCrashOnOverflow)
        CRASH();
    m_length = std::numeric_limits<unsigned>::max();
}

void StringBuilder::reifyString() const
{
    RELEASE_ASSERT(!hasOverflowed());

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
    ASSERT(m_length <= m_buffer->length());
    if (m_length == m_buffer->length())
        m_string = m_buffer.get();
    else
        m_string = StringImpl::createSubstringSharingImpl(*m_buffer, 0, m_length);
}

void StringBuilder::shrink(unsigned newLength)
{
    if (hasOverflowed())
        return;

    ASSERT(newLength <= m_length);
    if (newLength >= m_length) {
        if (newLength > m_length)
            didOverflow();
        return;
    }

    m_length = newLength;

    if (m_buffer) {
        m_string = { }; // Clear the string to remove the reference to m_buffer if any before checking the reference count of m_buffer.
        if (m_buffer->hasOneRef()) {
            // If the old buffer had only the one ref, we can just reduce the length and keep using the buffer.
            return;
        }
        // Allocate a fresh buffer, with a copy of the characters we are keeping.
        if (m_buffer->is8Bit())
            allocateBuffer<LChar>(m_buffer->span8(), newLength);
        else
            allocateBuffer<UChar>(m_buffer->span16(), newLength);
        return;
    }

    // Since the old length was not 0 and m_buffer is null, m_string is guaranteed to be non-null.
    m_string = StringImpl::createSubstringSharingImpl(*m_string.impl(), 0, newLength);
}

void StringBuilder::reallocateBuffer(unsigned requiredCapacity)
{
    if (is8Bit())
        reallocateBuffer<LChar>(requiredCapacity);
    else
        reallocateBuffer<UChar>(requiredCapacity);
}

void StringBuilder::reserveCapacity(unsigned newCapacity)
{
    if (hasOverflowed())
        return;

    if (m_buffer) {
        if (newCapacity > m_buffer->length())
            reallocateBuffer(newCapacity);
    } else {
        if (newCapacity > m_length) {
            if (!m_length)
                allocateBuffer<LChar>(std::span<const LChar> { }, newCapacity);
            else if (m_string.is8Bit())
                allocateBuffer<LChar>(m_string.span8(), newCapacity);
            else
                allocateBuffer<UChar>(m_string.span16(), newCapacity);
        }
    }
    ASSERT(hasOverflowed() || !newCapacity || m_buffer->length() >= newCapacity);
}

// Alterative extendBufferForAppending that can be called from the header without inlining.
std::span<LChar> StringBuilder::extendBufferForAppendingLChar(unsigned requiredLength)
{
    return extendBufferForAppending<LChar>(requiredLength);
}

std::span<UChar> StringBuilder::extendBufferForAppendingWithUpconvert(unsigned requiredLength)
{
    if (is8Bit()) {
        allocateBuffer<UChar>(span8(), expandedCapacity(capacity(), requiredLength));
        if (UNLIKELY(hasOverflowed()))
            return { };
        return spanConstCast<UChar>(m_buffer->span16().subspan(std::exchange(m_length, requiredLength)));
    }
    return extendBufferForAppending<UChar>(requiredLength);
}

void StringBuilder::append(std::span<const UChar> characters)
{
    if (characters.empty() || hasOverflowed())
        return;
    if (characters.size() == 1 && isLatin1(characters[0]) && is8Bit()) {
        append(static_cast<LChar>(characters[0]));
        return;
    }
    RELEASE_ASSERT(characters.size() < std::numeric_limits<uint32_t>::max());
    if (auto destination = extendBufferForAppendingWithUpconvert(saturatedSum<uint32_t>(m_length, static_cast<uint32_t>(characters.size()))); destination.data())
        StringImpl::copyCharacters(destination.data(), characters);
}

void StringBuilder::append(std::span<const LChar> characters)
{
    if (characters.empty() || hasOverflowed())
        return;
    RELEASE_ASSERT(characters.size() < std::numeric_limits<uint32_t>::max());
    if (is8Bit()) {
        if (auto destination = extendBufferForAppending<LChar>(saturatedSum<uint32_t>(m_length, static_cast<uint32_t>(characters.size()))); destination.data())
            StringImpl::copyCharacters(destination.data(), characters);
    } else {
        if (auto destination = extendBufferForAppending<UChar>(saturatedSum<uint32_t>(m_length, static_cast<uint32_t>(characters.size()))); destination.data())
            StringImpl::copyCharacters(destination.data(), characters);
    }
}

bool StringBuilder::shouldShrinkToFit() const
{
    // Shrink the buffer if it's 80% full or less.
    static_assert(static_cast<size_t>(String::MaxLength) + (String::MaxLength >> 2) <= static_cast<size_t>(std::numeric_limits<unsigned>::max()));
    return !hasOverflowed() && m_buffer && m_buffer->length() > m_length + (m_length >> 2);
}

void StringBuilder::shrinkToFit()
{
    if (shouldShrinkToFit()) {
        reallocateBuffer(m_length);
        m_string = std::exchange(m_buffer, nullptr);
    }
}

bool StringBuilder::containsOnlyASCII() const
{
    return StringView { *this }.containsOnlyASCII();
}

} // namespace WTF
