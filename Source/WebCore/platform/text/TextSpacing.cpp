/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TextSpacing.h"

#include "Font.h"

namespace WebCore {

using namespace TextSpacing;

bool TextSpacingTrim::shouldTrimSpacing(const CharactersData& charactersData) const
{
    switch (m_trim) {
    case TrimType::SpaceAll:
        return false;
    case TrimType::Auto:
        return false;
    case TrimType::TrimAll:
        return charactersData.currentCharacterClass == CharacterClass::FullWidthOpeningPunctuation || charactersData.currentCharacterClass == CharacterClass::FullWidthClosingPunctuation || charactersData.currentCharacterClass == CharacterClass::FullWidthMiddleDotPunctuation;
    default:
        return false;
    }
}

bool TextAutospace::shouldApplySpacing(CharacterClass firstCharacterClass, CharacterClass secondCharacterClass) const
{
    constexpr uint8_t ideographAlphaMask = static_cast<uint8_t>(CharacterClass::Ideograph) | static_cast<uint8_t>(CharacterClass::NonIdeographLetter);
    constexpr uint8_t ideographNumericMask = static_cast<uint8_t>(CharacterClass::Ideograph) | static_cast<uint8_t>(CharacterClass::NonIdeographNumeral);

    uint8_t characterClassesMask = (static_cast<uint8_t>(firstCharacterClass) | static_cast<uint8_t>(secondCharacterClass));
    if (hasIdeographAlpha() && characterClassesMask == ideographAlphaMask)
        return true;
    if (hasIdeographNumeric() && characterClassesMask == ideographNumericMask)
        return true;
    return false;
}

bool TextAutospace::shouldApplySpacing(char32_t firstCharacter, char32_t secondCharacter) const
{
    return shouldApplySpacing(TextSpacing::characterClass(firstCharacter), TextSpacing::characterClass(secondCharacter));
}

float TextAutospace::textAutospaceSize(const Font& font)
{
    // https://www.w3.org/TR/css-text-4/#text-autospace-property
    // The amount of space introduced by these keywords is 1/8 of the CJK advance measure, i.e 0.125ic.
    return 0.125 * font.fontMetrics().ideogramWidth().value_or(0);
}

namespace TextSpacing {

bool isIdeograph(char32_t character)
{
    // Lowest possible ideographic codepoint
    if (character < 0x2E80)
        return false;

    // All characters in the range of U+3041 to U+30FF, except those that belong to Unicode Punctuation [P*]
    if ((character >= 0x3041 && character <= 0x30FF) && !isPunctuation(character))
        return true;
    // CJK Strokes (U+31C0 to U+31EF).
    if (character >= 0x31C0 && character <= 0x31EF)
        return true;
    // Katakana Phonetic Extensions (U+31F0 to U+31FF)
    if (character >= 0x31F0 && character <= 0x31FF)
        return true;
    if (isOfScriptType(character, UScriptCode::USCRIPT_HAN))
        return true;

    return false;
}

static bool isNonIdeographicNumeral(char32_t character, uint32_t generalCategoryMask)
{
    // FIXME: Should also check that it is not: upright in vertical text flow using the text-orientation property or the text-combine-upright property.
    return (generalCategoryMask & U_GC_ND_MASK) && !isEastAsianFullWidth(character);
}

// Classes are defined at https://www.w3.org/TR/css-text-4/#text-spacing-classes
CharacterClass characterClass(char32_t character)
{
    auto generalCategoryMask = U_GET_GC_MASK(character);
    if (isIdeograph(character))
        return CharacterClass::Ideograph;

    // We already know it is not an Ideograph from here
    // FIXME: Should also check that it is not: upright in vertical text flow using the text-orientation property or the text-combine-upright property.
    if (generalCategoryMask & (U_GC_M_MASK | U_GC_L_MASK)) {
        if (!isEastAsianFullWidth(character))
            return CharacterClass::NonIdeographLetter;
        // General Category M/L won't apply to anything else
        return CharacterClass::Undefined;
    }

    if (isNonIdeographicNumeral(character, generalCategoryMask))
        return CharacterClass::NonIdeographNumeral;

    if (generalCategoryMask & U_GC_P_MASK) {
        if (isCJKSymbolOrPunctuation(character) || isEastAsianFullWidth(character)) {
            if (isOpeningPunctuation(generalCategoryMask))
                return CharacterClass::FullWidthOpeningPunctuation;
            if (isClosingPunctuation(generalCategoryMask))
                return CharacterClass::FullWidthClosingPunctuation;
        }
        if (character == leftSingleQuotationMark || character == leftDoubleQuotationMark)
            return CharacterClass::FullWidthOpeningPunctuation;
        if (character == rightSingleQuotationMark || character == rightDoubleQuotationMark)
            return CharacterClass::FullWidthClosingPunctuation;
    }

    if (isFullwidthMiddleDotPunctuation(character))
        return CharacterClass::FullWidthMiddleDotPunctuation;
    // FIXME: implement remaining classes for text-autospace: punctuation
    return CharacterClass::Undefined;
}

} // namespace TextSpacing
} // namespace WebCore
