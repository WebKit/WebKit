/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <limits>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringView.h>

namespace WTF {

template<typename OffsetType>
class BoyerMooreHorspoolTable {
    WTF_MAKE_FAST_ALLOCATED(BoyerMooreHorspoolTable);
public:
    static constexpr unsigned size = 256;
    static constexpr unsigned maxPatternLength = std::numeric_limits<OffsetType>::max();

    explicit BoyerMooreHorspoolTable(StringView pattern)
    {
        if (pattern.is8Bit())
            initializeTable(Span { pattern.characters8(), pattern.characters8() + pattern.length() });
        else
            initializeTable(Span { pattern.characters16(), pattern.characters16() + pattern.length() });
    }

    explicit constexpr BoyerMooreHorspoolTable(ASCIILiteral pattern)
    {
        initializeTable(Span { pattern.characters(), pattern.characters() + pattern.length() });
    }

    ALWAYS_INLINE size_t find(StringView string, StringView matchString) const
    {
        unsigned length = string.length();
        unsigned matchLength = matchString.length();
        if (matchLength > length)
            return notFound;

        if (UNLIKELY(!matchLength))
            return 0;

        if (string.is8Bit()) {
            if (matchString.is8Bit())
                return findInner(string.characters8(), matchString.characters8(), length, matchLength);
            return findInner(string.characters8(), matchString.characters16(), length, matchLength);
        }

        if (matchString.is8Bit())
            return findInner(string.characters16(), matchString.characters8(), length, matchLength);
        return findInner(string.characters16(), matchString.characters16(), length, matchLength);
    }

private:
    template<typename CharacterType>
    constexpr void initializeTable(Span<CharacterType> pattern)
    {
        size_t length = pattern.size();
        ASSERT_UNDER_CONSTEXPR_CONTEXT(length <= maxPatternLength);
        if (length) {
            for (auto& element : m_table)
                element = length;
            for (unsigned i = 0; i < (pattern.size() - 1); ++i) {
                unsigned index = pattern.data()[i] & 0xff;
                m_table[index] = length - 1 - i;
            }
        }
    }

    template <typename SearchCharacterType, typename MatchCharacterType>
    ALWAYS_INLINE size_t findInner(const SearchCharacterType* characters, const MatchCharacterType* matchCharacters, unsigned length, unsigned matchLength) const
    {
        auto* cursor = characters;
        auto* last = characters + length - matchLength;
        while (cursor <= last) {
            if (equal(cursor, matchCharacters, matchLength))
                return cursor - characters;
            cursor += m_table[static_cast<uint8_t>(cursor[matchLength - 1])];
        }
        return notFound;
    }

    OffsetType m_table[size];
};

}

using WTF::BoyerMooreHorspoolTable;
