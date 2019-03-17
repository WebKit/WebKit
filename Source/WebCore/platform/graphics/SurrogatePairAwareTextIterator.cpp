/*
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#include "config.h"
#include "SurrogatePairAwareTextIterator.h"

#include <unicode/unorm2.h>

namespace WebCore {

SurrogatePairAwareTextIterator::SurrogatePairAwareTextIterator(const UChar* characters, unsigned currentIndex, unsigned lastIndex, unsigned endIndex)
    : m_characters(characters)
    , m_currentIndex(currentIndex)
    , m_lastIndex(lastIndex)
    , m_endIndex(endIndex)
{
}

bool SurrogatePairAwareTextIterator::consumeSlowCase(UChar32& character, unsigned& clusterLength)
{
    if (character <= 0x30FE) {
        // Deal with Hiragana and Katakana voiced and semi-voiced syllables.
        // Normalize into composed form, and then look for glyph with base + combined mark.
        // Check above for character range to minimize performance impact.
        if (UChar32 normalized = normalizeVoicingMarks()) {
            character = normalized;
            clusterLength = 2;
        }
        return true;
    }

    if (!U16_IS_SURROGATE(character))
        return true;

    // If we have a surrogate pair, make sure it starts with the high part.
    if (!U16_IS_SURROGATE_LEAD(character))
        return false;

    // Do we have a surrogate pair? If so, determine the full Unicode (32 bit) code point before glyph lookup.
    // Make sure we have another character and it's a low surrogate.
    if (m_currentIndex + 1 >= m_endIndex)
        return false;

    UChar low = m_characters[1];
    if (!U16_IS_TRAIL(low))
        return false;

    character = U16_GET_SUPPLEMENTARY(character, low);
    clusterLength = 2;
    return true;
}

UChar32 SurrogatePairAwareTextIterator::normalizeVoicingMarks()
{
    // According to http://www.unicode.org/Public/UNIDATA/UCD.html#Canonical_Combining_Class_Values
    static constexpr uint8_t hiraganaKatakanaVoicingMarksCombiningClass = 8;

    if (m_currentIndex + 1 >= m_endIndex)
        return 0;

    if (u_getCombiningClass(m_characters[1]) == hiraganaKatakanaVoicingMarksCombiningClass) {
        UErrorCode status = U_ZERO_ERROR;
        const UNormalizer2* normalizer = unorm2_getNFCInstance(&status);
        ASSERT(U_SUCCESS(status));
        auto composedCharacter = unorm2_composePair(normalizer, m_characters[0], m_characters[1]);
        if (composedCharacter > 0)
            return composedCharacter;
    }

    return 0;
}

}
