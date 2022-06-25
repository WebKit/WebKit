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

#pragma once

#include <wtf/text/StringBuilder.h>

namespace WTF {

// Allocate a new buffer, copying in currentCharacters (these may come from either m_string or m_buffer.
template<typename AllocationCharacterType, typename CurrentCharacterType> void StringBuilder::allocateBuffer(const CurrentCharacterType* currentCharacters, unsigned requiredCapacity)
{
    AllocationCharacterType* bufferCharacters;
    auto buffer = StringImpl::tryCreateUninitialized(requiredCapacity, bufferCharacters);
    if (UNLIKELY(!buffer)) {
        didOverflow();
        return;
    }

    ASSERT(!hasOverflowed());
    StringImpl::copyCharacters(bufferCharacters, currentCharacters, m_length);

    m_buffer = WTFMove(buffer);
    m_string = { };
}

template<typename CharacterType> void StringBuilder::reallocateBuffer(unsigned requiredCapacity)
{
    // If the buffer has only one ref (by this StringBuilder), reallocate it.
    if (m_buffer) {
        m_string = { }; // Clear the string to remove the reference to m_buffer if any before checking the reference count of m_buffer.
        if (m_buffer->hasOneRef()) {
            CharacterType* bufferCharacters;
            auto buffer = StringImpl::tryReallocate(m_buffer.releaseNonNull(), requiredCapacity, bufferCharacters);
            if (UNLIKELY(!buffer)) {
                didOverflow();
                return;
            }
            m_buffer = WTFMove(*buffer);
            return;
        }
    }

    allocateBuffer<CharacterType>(characters<CharacterType>(), requiredCapacity);
}

// Make 'additionalLength' additional capacity be available in m_buffer, update m_string & m_length to use,
// that capacity and return a pointer to the newly allocated storage so the caller can write characters there.
// Returns nullptr if allocation fails, length overflows, or if total capacity is 0 so no buffer is needed.
// The caller has the responsibility for checking that CharacterType is the type of the existing buffer.
template<typename CharacterType> CharacterType* StringBuilder::extendBufferForAppending(unsigned requiredLength)
{
    if (m_buffer && requiredLength <= m_buffer->length()) {
        m_string = { };
        return const_cast<CharacterType*>(m_buffer->characters<CharacterType>()) + std::exchange(m_length, requiredLength);
    }
    return extendBufferForAppendingSlowCase<CharacterType>(requiredLength);
}

// Shared by the other extendBuffer functions.
template<typename CharacterType> CharacterType* StringBuilder::extendBufferForAppendingSlowCase(unsigned requiredLength)
{
    ASSERT(!hasOverflowed());
    if (!requiredLength)
        return nullptr;
    reallocateBuffer(expandedCapacity(capacity(), requiredLength));
    if (UNLIKELY(hasOverflowed()))
        return nullptr;
    return const_cast<CharacterType*>(m_buffer->characters<CharacterType>()) + std::exchange(m_length, requiredLength);
}

} // namespace WTF
