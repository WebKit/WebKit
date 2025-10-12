/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <unicode/utf16.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SurrogatePairAwareTextIterator {
public:
    // The passed in char16_t pointer starts at 'currentIndex'. The iterator operates on the range [currentIndex, lastIndex].
    // 'endIndex' denotes the maximum length of the char16_t array, which might exceed 'lastIndex'.
    SurrogatePairAwareTextIterator(std::span<const char16_t> characters, unsigned currentIndex, unsigned lastIndex)
        : m_characters(characters)
        , m_currentIndex(currentIndex)
        , m_originalIndex(currentIndex)
        , m_lastIndex(lastIndex)
        , m_endIndex(characters.size() + currentIndex)
    {
    }

    bool consume(char32_t& character, unsigned& clusterLength)
    {
        if (m_currentIndex >= m_lastIndex)
            return false;

        auto relativeIndex = m_currentIndex - m_originalIndex;
        clusterLength = 0;
        auto spanAtRelativeIndex = m_characters.subspan(relativeIndex);
        U16_NEXT(spanAtRelativeIndex, clusterLength, m_endIndex - m_currentIndex, character);
        return true;
    }

    void advance(unsigned advanceLength)
    {
        m_currentIndex += advanceLength;
    }

    void reset(unsigned index)
    {
        if (index >= m_lastIndex)
            return;
        m_currentIndex = index;
    }

    std::span<const char16_t> remainingCharacters() const
    {
        auto relativeIndex = m_currentIndex - m_originalIndex;
        return m_characters.subspan(relativeIndex);
    }

    unsigned currentIndex() const { return m_currentIndex; }
    std::span<const char16_t> characters() const { return m_characters; }

private:
    std::span<const char16_t> m_characters;
    unsigned m_currentIndex { 0 };
    const unsigned m_originalIndex { 0 };
    const unsigned m_lastIndex { 0 };
    const unsigned m_endIndex { 0 };
};

} // namespace WebCore
