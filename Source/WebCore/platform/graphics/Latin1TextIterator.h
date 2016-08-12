/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef Latin1TextIterator_h
#define Latin1TextIterator_h

#include <wtf/text/WTFString.h>

namespace WebCore {

class Latin1TextIterator {
public:
    // The passed in LChar pointer starts at 'currentIndex'. The iterator operates on the range [currentIndex, lastIndex].
    // 'endCharacter' denotes the maximum length of the UChar array, which might exceed 'lastIndex'.
    Latin1TextIterator(const LChar* characters, unsigned currentIndex, unsigned lastIndex, unsigned /*endCharacter*/)
        : m_characters(characters)
        , m_currentIndex(currentIndex)
        , m_lastIndex(lastIndex)
    {
    }

    bool consume(UChar32& character, unsigned& clusterLength)
    {
        if (m_currentIndex >= m_lastIndex)
            return false;

        character = *m_characters;
        clusterLength = 1;
        return true;
    }

    void advance(unsigned advanceLength)
    {
        m_characters += advanceLength;
        m_currentIndex += advanceLength;
    }

    unsigned currentIndex() const { return m_currentIndex; }
    const LChar* characters() const { return m_characters; }

private:
    const LChar* m_characters;
    unsigned m_currentIndex;
    unsigned m_lastIndex;
};

}

#endif
