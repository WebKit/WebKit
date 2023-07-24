/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include <unicode/utf16.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

class ComposedCharacterClusterTextIterator {
public:
    // The passed in UChar pointer starts at 'currentIndex'. The iterator operates on the range [currentIndex, lastIndex].
    // 'endIndex' denotes the maximum length of the UChar array, which might exceed 'lastIndex'.
    ComposedCharacterClusterTextIterator(const UChar* characters, unsigned currentIndex, unsigned lastIndex, unsigned endIndex)
        : m_iterator(StringView(characters, endIndex - currentIndex), nullptr, 0, TextBreakIterator::CaretMode { }, nullAtom())
        , m_characters(characters)
        , m_originalIndex(currentIndex)
        , m_currentIndex(currentIndex)
        , m_lastIndex(lastIndex)
    {
    }

    bool consume(UChar32& character, unsigned& clusterLength)
    {
        if (m_currentIndex >= m_lastIndex)
            return false;

        auto relativeIndex = m_currentIndex - m_originalIndex;
        if (auto result = m_iterator.following(relativeIndex)) {
            clusterLength = result.value() - relativeIndex;
            U16_NEXT(m_characters, relativeIndex, result.value(), character);
            return true;
        }
        
        return false;
    }

    void advance(unsigned advanceLength)
    {
        m_currentIndex += advanceLength;
    }

    unsigned currentIndex() const { return m_currentIndex; }

private:
    CachedTextBreakIterator m_iterator;
    const UChar* m_characters;
    unsigned m_originalIndex { 0 };
    unsigned m_currentIndex { 0 };
    unsigned m_lastIndex { 0 };
};

}
