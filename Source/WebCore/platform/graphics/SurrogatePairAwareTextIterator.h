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
    // The passed in UChar pointer starts at 'currentIndex'. The iterator operates on the range [currentIndex, lastIndex].
    // 'endIndex' denotes the maximum length of the UChar array, which might exceed 'lastIndex'.
    SurrogatePairAwareTextIterator(const UChar* characters, unsigned currentIndex, unsigned lastIndex, unsigned endIndex)
        : m_characters(characters)
        , m_currentIndex(currentIndex)
        , m_lastIndex(lastIndex)
        , m_endIndex(endIndex)
    {
    }

    bool consume(UChar32& character, unsigned& clusterLength)
    {
        if (m_currentIndex >= m_lastIndex)
            return false;

        clusterLength = 0;
        U16_NEXT(m_characters, clusterLength, m_endIndex - m_currentIndex, character);
        return true;
    }

    void advance(unsigned advanceLength)
    {
        m_characters += advanceLength;
        m_currentIndex += advanceLength;
    }

    unsigned currentIndex() const { return m_currentIndex; }

private:
    const UChar* m_characters { nullptr };
    unsigned m_currentIndex { 0 };
    unsigned m_lastIndex { 0 };
    unsigned m_endIndex { 0 };
};

}
