/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <unicode/utypes.h>
#include <wtf/Assertions.h>
#include <wtf/text/LChar.h>

namespace WTF {

template<typename CharacterType>
class CodePointIterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ALWAYS_INLINE CodePointIterator() = default;
    ALWAYS_INLINE CodePointIterator(std::span<const CharacterType> data)
        : m_data(data)
    {
    }
    
    ALWAYS_INLINE CodePointIterator(const CodePointIterator& begin, const CodePointIterator& end)
        : CodePointIterator({ begin.m_data.data(), end.m_data.data() })
    {
        ASSERT(end.m_data.data() >= begin.m_data.data());
    }
    
    ALWAYS_INLINE char32_t operator*() const;
    ALWAYS_INLINE CodePointIterator& operator++();

    ALWAYS_INLINE friend bool operator==(const CodePointIterator& a, const CodePointIterator& b)
    {
        return a.m_data.data() == b.m_data.data() && a.m_data.size() == b.m_data.size();
    }

    ALWAYS_INLINE bool atEnd() const
    {
        return m_data.empty();
    }
    
    ALWAYS_INLINE size_t codeUnitsSince(const CharacterType* reference) const
    {
        ASSERT(m_data.data() >= reference);
        return m_data.data() - reference;
    }

    ALWAYS_INLINE size_t codeUnitsSince(const CodePointIterator& other) const
    {
        return codeUnitsSince(other.m_data.data());
    }
    
private:
    std::span<const CharacterType> m_data;
};

template<>
ALWAYS_INLINE char32_t CodePointIterator<LChar>::operator*() const
{
    ASSERT(!atEnd());
    return m_data.front();
}

template<>
ALWAYS_INLINE auto CodePointIterator<LChar>::operator++() -> CodePointIterator&
{
    m_data = m_data.subspan(1);
    return *this;
}

template<>
ALWAYS_INLINE char32_t CodePointIterator<UChar>::operator*() const
{
    ASSERT(!atEnd());
    char32_t c;
    U16_GET(m_data, 0, 0, m_data.size(), c);
    return c;
}

template<>
ALWAYS_INLINE auto CodePointIterator<UChar>::operator++() -> CodePointIterator&
{
    unsigned i = 0;
    size_t length = m_data.size();
    U16_FWD_1(m_data, i, length);
    m_data = m_data.subspan(i);
    return *this;
}

template<typename CharacterType> CodePointIterator(std::span<const CharacterType>) -> CodePointIterator<CharacterType>;

} // namespace WTF
