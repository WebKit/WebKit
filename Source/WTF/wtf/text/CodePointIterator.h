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

namespace WTF {

template<typename CharacterType>
class CodePointIterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ALWAYS_INLINE CodePointIterator() { }
    ALWAYS_INLINE CodePointIterator(const CharacterType* begin, const CharacterType* end)
        : m_begin(begin)
        , m_end(end)
    {
    }
    
    ALWAYS_INLINE CodePointIterator(const CodePointIterator& begin, const CodePointIterator& end)
        : CodePointIterator(begin.m_begin, end.m_begin)
    {
        ASSERT(end.m_begin >= begin.m_begin);
    }
    
    ALWAYS_INLINE UChar32 operator*() const;
    ALWAYS_INLINE CodePointIterator& operator++();

    ALWAYS_INLINE bool operator==(const CodePointIterator& other) const
    {
        return m_begin == other.m_begin
            && m_end == other.m_end;
    }
    ALWAYS_INLINE bool operator!=(const CodePointIterator& other) const { return !(*this == other); }

    ALWAYS_INLINE bool atEnd() const
    {
        ASSERT(m_begin <= m_end);
        return m_begin >= m_end;
    }
    
    ALWAYS_INLINE size_t codeUnitsSince(const CharacterType* reference) const
    {
        ASSERT(m_begin >= reference);
        return m_begin - reference;
    }

    ALWAYS_INLINE size_t codeUnitsSince(const CodePointIterator& other) const
    {
        return codeUnitsSince(other.m_begin);
    }
    
private:
    const CharacterType* m_begin { nullptr };
    const CharacterType* m_end { nullptr };
};

template<>
ALWAYS_INLINE UChar32 CodePointIterator<LChar>::operator*() const
{
    ASSERT(!atEnd());
    return *m_begin;
}

template<>
ALWAYS_INLINE auto CodePointIterator<LChar>::operator++() -> CodePointIterator&
{
    m_begin++;
    return *this;
}

template<>
ALWAYS_INLINE UChar32 CodePointIterator<UChar>::operator*() const
{
    ASSERT(!atEnd());
    UChar32 c;
    U16_GET(m_begin, 0, 0, m_end - m_begin, c);
    return c;
}

template<>
ALWAYS_INLINE auto CodePointIterator<UChar>::operator++() -> CodePointIterator&
{
    unsigned i = 0;
    size_t length = m_end - m_begin;
    U16_FWD_1(m_begin, i, length);
    m_begin += i;
    return *this;
}

} // namespace WTF
