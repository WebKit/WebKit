/*
 * Copyright (C) 2003, 2006, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#if USE(ICU_UNICODE)
#include <unicode/unorm.h>
#endif

using namespace WTF;
using namespace Unicode;

namespace WebCore {

inline static bool isUnicodeBMPVariationSelector(UChar character)
{
    return (0x180B <= character && character <= 0x180D) || (0xFE00 <= character && character <= 0xFE0F);
}

inline static bool isUnicodeSupplementaryVariationSelector(UChar lead, UChar trail)
{
    // A non-BMP variation selector character is in the range of U+E0100 to U+E01EF.
    // It can be a surrogate pair in which the high surrogate is 0xDB40 and
    // the low surrogate is in the range of U16_TRAIL(0xE0100 - 0x10000) to U16_TRAIL(0xE01EF - 0x10000).
    static const UChar trailStart = U16_TRAIL(0xE0100 - 0x10000);
    static const UChar trailEnd = U16_TRAIL(0xE01EF - 0x10000);
    return lead == 0xDB40 && trailStart <= trail && trail <= trailEnd;
}

SurrogatePairAwareTextIterator::SurrogatePairAwareTextIterator(const UChar* characters, int currentCharacter, int lastCharacter, int endCharacter)
    : m_characters(characters)
    , m_currentCharacter(currentCharacter)
    , m_lastCharacter(lastCharacter)
    , m_endCharacter(endCharacter)
{
}

bool SurrogatePairAwareTextIterator::consume(UChar32& character, unsigned& clusterLength)
{
    if (m_currentCharacter >= m_lastCharacter)
        return false;

    character = *m_characters;
    clusterLength = 1;

    if (character < 0x3041)
        return true;

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
    if (m_currentCharacter + 1 >= m_endCharacter)
        return false;

    UChar low = m_characters[1];
    if (!U16_IS_TRAIL(low))
        return false;

    character = U16_GET_SUPPLEMENTARY(character, low);
    clusterLength = 2;
    return true;
}

bool SurrogatePairAwareTextIterator::hasTrailingVariationSelector(UChar32& selector, unsigned& clusterLength)
{
    if (static_cast<int>(m_currentCharacter + clusterLength) < m_endCharacter && isUnicodeBMPVariationSelector(m_characters[clusterLength])) {
        selector = m_characters[clusterLength];
        clusterLength += 1;
        return true;
    }
    if (static_cast<int>(m_currentCharacter + clusterLength + 1) < m_endCharacter && isUnicodeSupplementaryVariationSelector(m_characters[clusterLength], m_characters[clusterLength + 1])) {
        selector = U16_GET_SUPPLEMENTARY(m_characters[clusterLength], m_characters[clusterLength + 1]);
        clusterLength += 2;
        return true;
    }
    return false;
}

void SurrogatePairAwareTextIterator::advance(unsigned advanceLength)
{
    m_characters += advanceLength;
    m_currentCharacter += advanceLength;
}

UChar32 SurrogatePairAwareTextIterator::normalizeVoicingMarks()
{
    // According to http://www.unicode.org/Public/UNIDATA/UCD.html#Canonical_Combining_Class_Values
    static const uint8_t hiraganaKatakanaVoicingMarksCombiningClass = 8;

    if (m_currentCharacter + 1 >= m_endCharacter)
        return 0;

    if (combiningClass(m_characters[1]) == hiraganaKatakanaVoicingMarksCombiningClass) {
#if USE(ICU_UNICODE)
        // Normalize into composed form using 3.2 rules.
        UChar normalizedCharacters[2] = { 0, 0 };
        UErrorCode uStatus = U_ZERO_ERROR;  
        int32_t resultLength = unorm_normalize(m_characters, 2, UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], 2, &uStatus);
        if (resultLength == 1 && !uStatus)
            return normalizedCharacters[0];
#elif USE(QT4_UNICODE)
        QString tmp(reinterpret_cast<const QChar*>(m_characters), 2);
        QString res = tmp.normalized(QString::NormalizationForm_C, QChar::Unicode_3_2);
        if (res.length() == 1)
            return res.at(0).unicode();
#endif
    }

    return 0;
}

}
