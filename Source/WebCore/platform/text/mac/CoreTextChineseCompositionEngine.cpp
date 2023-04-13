/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CoreTextChineseCompositionEngine.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <wtf/unicode/CharacterNames.h>

static ChineseCompositionRules::ChineseCharacterClass characterToCharacterClass(UTF32Char character)
{
    if (character == zeroWidthSpace)
        return ChineseCompositionRules::ChineseCharacterClass::Other;

    /* Characters with the UCHAR_EAST_ASIAN_WIDTH enumerable property equal to U_EA_FULLWIDTH or U_EA_WIDE are of width 2. */
    int width = static_cast<int>(u_getIntPropertyValue(character, UCHAR_EAST_ASIAN_WIDTH));
    if (width == U_EA_FULLWIDTH || width == U_EA_WIDE)
        return ChineseCompositionRules::ChineseCharacterClass::FullWidth;

    /* any other characters have width 1 */
    return ChineseCompositionRules::ChineseCharacterClass::HalfWidth;
}

static inline bool isDigit(UTF32Char character)
{
    return character >= 0x0030 && character <= 0x0039;
}

static inline bool isNarrowFullWidth(UTF32Char character)
{
    return character == 0x6708 /* 月 */ || character == 0x65E5 /* 日 */;
}

ChineseCompositionRules::ChineseCharacterClass ChineseCompositionRules::characterClass(UTF32Char character, uint32_t generalCategoryMask, CTCompositionLanguage language)
{
    switch (character) {
    case 0x2985: // LEFT WHITE PARENTHESIS
    case 0x3008: // LEFT ANGLE BRACKET
    case 0x300A: // LEFT DOUBLE ANGLE BRACKET
    case 0x300C: // LEFT CORNER BRACKET
    case 0x300E: // LEFT WHITE CORNER BRACKET
    case 0x3010: // LEFT BLACK LENTICULAR BRACKET
    case 0x3014: // LEFT TORTOISE SHELL BRACKET
    case 0x3016: // LEFT WHITE LENTICULAR BRACKET
    case 0x3018: // LEFT WHITE TORTOISE SHELL BRACKET
    case 0x301A: // LEFT WHITE SQUARE BRACKET
    case 0x301D: // REVERSED DOUBLE PRIME QUOTATION MARK, used vertical composition
    case 0xFF08: // | FULLWIDTH LEFT PARENTHESIS
    case 0xFF3B: // | FULLWIDTH LEFT SQUARE BRACKET
    case 0xFF5B: // | FULLWIDTH LEFT CURLY BRACKET
    case 0xFF5F: // FULLWIDTH LEFT WHITE PARENTHESIS
        return Other; // kOpening

    case 0x0028: // LEFT PARENTHESIS
        return HalfWidthOpening;

    case 0x0029: // RIGHT PARENTHESIS
        return HalfWidthClosing;

    case 0x2986: // RIGHT WHITE PARENTHESIS
    case 0x3009: // RIGHT ANGLE BRACKET
    case 0x300B: // RIGHT DOUBLE ANGLE BRACKET
    case 0x300D: // RIGHT CORNER BRACKET
    case 0x300F: // RIGHT WHITE CORNER BRACKET
    case 0x3011: // RIGHT BLACK LENTICULAR BRACKET
    case 0x3015: // RIGHT TORTOISE SHELL BRACKET
    case 0x3017: // RIGHT WHITE LENTICULAR BRACKET
    case 0x3019: // RIGHT WHITE TORTOISE SHELL BRACKET
    case 0x301B: // RIGHT WHITE SQUARE BRACKET
    case 0x301E: // DOUBLE PRIME QUOTATION MARK
    case 0x301F: // LOW DOUBLE PRIME QUOTATION MARK, used vertical composition
    case 0xFF09: // | FULLWIDTH RIGHT PARENTHESIS
    case 0xFF3D: // | FULLWIDTH RIGHT SQUARE BRACKET
    case 0xFF5D: // | FULLWIDTH RIGHT CURLY BRACKET
    case 0xFF60: // FULLWIDTH RIGHT WHITE PARENTHESIS
        return Other; // kClosing

    case 0x3001: // IDEOGRAPHIC COMMA
    case 0x3002: // IDEOGRAPHIC FULL STOP
    case 0xFF0C: // FULLWIDTH COMMA
    case 0xFF0E: // FULLWIDTH FULL STOP
        return language == kCTCompositionLanguageTraditionalChinese ? Centered : Closing;

    case 0xFF01: // FULLWIDTH EXCLAMATION MARK
    case 0xFF1A: // FULLWIDTH COLON
    case 0xFF1B: // FULLWIDTH SEMICOLON
    case 0xFF1F: // FULLWIDTH QUESTION MARK
        return (language == kCTCompositionLanguageTraditionalChinese || language == kCTCompositionLanguageJapanese) ? Centered : Closing;

    case 0x22EF: // MIDLINE HORIZONTAL ELLIPSIS
    case 0xFF5E: // FULLWIDTH TILDE
        return Other;
    }

    // Half width punctuations.
    if ((character != 0x0025 && character != 0x002B && character >= 0x0021 && character <= 0x002E) || (character >= 0x003A && character <= 0x003F) || (character >= 0x005B && character <= 0x0060) || (character >= 0x2010 && character <= 0x2027))
        return Other;

    if (!generalCategoryMask)
        generalCategoryMask = U_GET_GC_MASK(character);

    if (generalCategoryMask & (U_GC_LM_MASK | U_GC_SK_MASK | U_GC_MN_MASK | U_GC_ME_MASK | U_GC_CF_MASK |U_GC_CC_MASK | U_GC_SO_MASK ))
        return Other;

    if (generalCategoryMask & U_GC_ZS_MASK)
        return Whitespace;

    UErrorCode error = U_ZERO_ERROR;
    UScriptCode unicodeScript = uscript_getScript(character, &error);
    if (unicodeScript == USCRIPT_HANGUL)
        return Other;

    if (character)
        characterToCharacterClass(character);

    return Other;
}

CompositionRules::CharacterSpacingType ChineseCompositionRules::characterSpacing(CTCompositionLanguage language, bool isVertical, UTF32Char beforeCharacter, UTF32Char afterCharacter)
{
    using namespace CompositionRules;

    static const CompositionRules::CharacterSpacingType chineseSpacingTable[NumClasses][NumClasses] = {
                    /* Opening,    Closing,    Whitespace, FullWidth,   HalfWidth,   HalfWidthOpen, HalfWidthClose,   Centered    Other */
/* Opening */       {  _1_4_be_re, _______,    _______,    _______,     _______,     _______,       _______,          _______,    _______    }, // NOLINT
/* Closing */       {  _1_2_eq_re, _1_4_af_re, _______,    _1_4_af_re,  _1_4_af_re,  _1_4_af_re,    _1_4_af_re,       _1_2_eq_re, _1_4_af_re }, // NOLINT
/* Whitespace */    {  _1_4_be_re, _______,    _______,    _______,     _______,     _______,       _______,          _______,    _______    }, // NOLINT
/* FullWidth */     {  _1_4_be_re, _______,    _______,    _______,     _1_8_be,     _1_8_be,       _______,          _______,    _______    }, // NOLINT
/* HalfWidth */     {  _1_4_be_re, _______,    _______,    _1_8_be,     _______,     _______,       _______,          _______,    _______    }, // NOLINT
/* HalfWidthOpen */ {  _1_4_be_re, _______,    _______,    _______,     _______,     _______,       _______,          _______,    _______    }, // NOLINT
/* HalfWidthClose */{  _1_4_be_re, _______,    _______,    _1_8_be,     _______,     _______,       _______,          _______,    _______    }, // NOLINT
/* Centered */      {  _1_2_eq_re, _______,    _______,    _______,     _______,     _______,       _______,          _______,    _______    }, // NOLINT
/* Other */         {  _1_4_be_re, _______,    _______,    _______,     _______,     _______,       _______,          _______,    _______    }, // NOLINT
    };
    auto beforeGeneralCategoryMask = U_GET_GC_MASK(beforeCharacter);
    auto afterGeneralCategoryMask = U_GET_GC_MASK(afterCharacter);
    auto beforeClass = characterClass(beforeCharacter, beforeGeneralCategoryMask, language);
    auto afterClass = characterClass(beforeCharacter, afterGeneralCategoryMask, language);

    // Only change trailing closing marks for now.
    if (beforeClass == Closing && afterCharacter && !(afterGeneralCategoryMask & (U_GC_ZL_MASK | U_GC_ZP_MASK)) && afterCharacter != 0x0A)
        beforeClass = Other;

    if ((isDigit(beforeCharacter) && isNarrowFullWidth(afterCharacter)) || (isDigit(afterCharacter) && isNarrowFullWidth(beforeCharacter)))
        return _1_25_be;

    // http://www.w3.org/TR/2015/WD-clreq-20150723/#h-compression_of_adjacent_punctuation_marks:
    // In Simplified Chinese, when one or more closing brackets appear behind a slight-pause comma,
    // comma or period, a space of half a character width can be reduced. This rule does not apply
    // to Traditional Chinese.
    if (language == kCTCompositionLanguageTraditionalChinese && beforeClass == Closing && afterClass == Closing)
        return _______;

    // Don't increase space between fullwidth and halfwidth text in vertical mode.
    if (isVertical && (beforeClass == HalfWidth || afterClass == HalfWidth))
        return _______;

    return chineseSpacingTable[beforeClass][afterClass];
}
