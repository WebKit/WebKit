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
#include "CoreTextCompositionEngine.h"

#include "TextSpacing.h"

namespace WebCore {

CJKCompositionEngine::CJKCompositionCharacterClass CJKCompositionEngine::characterClass(UChar32 character, uint32_t generalCategoryMask)
{
// <http://www.w3.org/TR/jlreq/#character_classes>
	switch (character) {
			// Opening Marks
		case 0x300C: // 「	LEFT CORNER BRACKET
		case 0x300E: // 『	LEFT WHITE CORNER BRACKET
		case 0xFF08: // （	FULLWIDTH LEFT PARENTHESIS
		case 0x3014: // 〔	LEFT TORTOISE SHELL BRACKET
		case 0x3010: // 【	LEFT BLACK LENTICULAR BRACKET
		case 0x3016: // 〖	LEFT WHITE LENTICULAR BRACKET
			return CJKCompositionCharacterClass::OpeningMarks;
		case 0x3008: // 〈	LEFT ANGLE BRACKET
		case 0x300A: // 《	LEFT DOUBLE ANGLE BRACKET
		case 0xFF5B: // ｛	FULLWIDTH LEFT CURLY BRACKET
			return CJKCompositionCharacterClass::OpeningMarks_o;
		case 0x2018: // ‘	LEFT SINGLE QUOTATION MARK; used horizontal composition
		case 0x201C: // “	LEFT DOUBLE QUOTATION MARK; used horizontal composition
			return CJKCompositionCharacterClass::OpeningMarks_c;
			// Closing Marks
		case 0x300D: // 」	RIGHT CORNER BRACKET
		case 0x300F: // 』	RIGHT WHITE CORNER BRACKET
		case 0xFF09: // ）	| FULLWIDTH RIGHT PARENTHESIS
		case 0x3015: // 〕	RIGHT TORTOISE SHELL BRACKET
		case 0x3011: // 】	RIGHT BLACK LENTICULAR BRACKET
		case 0x3017: // 〗	RIGHT WHITE LENTICULAR BRACKET
			return CJKCompositionCharacterClass::ClosingMarks;
		case 0x3009: // 〉	RIGHT ANGLE BRACKET
		case 0x300B: // 》	RIGHT DOUBLE ANGLE BRACKET
		case 0xFF5D: // ｝	FULLWIDTH RIGHT CURLY BRACKET
			return CJKCompositionCharacterClass::ClosingMarks_o;
		case 0x2019: // ’	RIGHT SINGLE QUOTATION MARK; used horizontal composition
		case 0x201D: // ”	RIGHT DOUBLE QUOTATION MARK; used horizontal composition
			return CJKCompositionCharacterClass::ClosingMarks_c;
			// Stop Marks
		case 0x3002: // 。	IDEOGRAPHIC FULL STOP
			return CJKCompositionCharacterClass::StopMarks;
		case 0x3001: // 、	IDEOGRAPHIC COMMA
		case 0xFF0C: // ，	FULLWIDTH COMMA
			return CJKCompositionCharacterClass::StopMarks_s;
		case 0xFF1A: // ：	| FULLWIDTH COLON
		case 0xFF1B: // ；	| FULLWIDTH SEMICOLON
		case 0xFF1F: // ？	| FULLWIDTH QUESTION MARK
		case 0xFF01: // ！	| FULLWIDTH EXCLAMATION MARK
			return CJKCompositionCharacterClass::StopMarks_c; // rdar://108179052
		case 0xFF0E: // ．	FULLWIDTH FULL STOP
			return CJKCompositionCharacterClass::StopMarks_j;
        // Middle dots (cl-05)
        //	case 0x30FB: // ・	KATAKANA MIDDLE DOT		// Waiting for the final spec - rdar://111078249
        //		return kMiddleDots;
        // Inseparable characters (cl-08)
		case 0x2014: // EM DASH
		case 0x2026: // HORIZONTAL ELLIPSIS
		case 0x2025: // TWO DOT LEADER
			return CJKCompositionCharacterClass::Inseparable;
	}
	// Half width punctuations.
	if ((character != 0x0025 && character != 0x002B && character >= 0x0021 && character <= 0x002E) || (character >= 0x003A && character <= 0x003F) || (character >= 0x005B && character <= 0x0060) || (character >= 0x2010 && character <= 0x2027)) {
		return CJKCompositionCharacterClass::Inseparable;
	}

	if (generalCategoryMask == 0) {
		generalCategoryMask = U_GET_GC_MASK(character);
	}
	if ((generalCategoryMask & (U_GC_LM_MASK|U_GC_SK_MASK|U_GC_MN_MASK|U_GC_ME_MASK|U_GC_CF_MASK|U_GC_CC_MASK|U_GC_SO_MASK)) != 0) {
		return CJKCompositionCharacterClass::Inseparable;
	}

	if ((generalCategoryMask & U_GC_ZS_MASK) != 0)
		return CJKCompositionCharacterClass::Whitespace;

	UErrorCode error = U_ZERO_ERROR;
	UScriptCode scriptCode = uscript_getScript(character, &error);
	if (scriptCode == USCRIPT_HANGUL) {
		return CJKCompositionCharacterClass::Inseparable;
	}

	/* Characters with the UCHAR_EAST_ASIAN_WIDTH enumerable property equal to U_EA_FULLWIDTH or U_EA_WIDE are of width 2. */
	int charWidthKind = (int)u_getIntPropertyValue(character, UCHAR_EAST_ASIAN_WIDTH);
	if (charWidthKind != U_EA_FULLWIDTH && charWidthKind != U_EA_WIDE) {
		return CJKCompositionCharacterClass::Foreign;
	}
	return CJKCompositionCharacterClass::Other;
}

CJKCompositionRules::CharacterClass CJKCompositionRules::characterClass(UChar32 character, TextSpacingSupportedLanguage language, uint32_t generalCategoryMask)
{
	if ((generalCategoryMask & (U_GC_LM_MASK|U_GC_SK_MASK|U_GC_MN_MASK|U_GC_ME_MASK|U_GC_CF_MASK|U_GC_CC_MASK|U_GC_SO_MASK)) != 0) {
		return CharacterClass::Inseparable;
	}

	CJKCompositionEngine::CJKCompositionCharacterClass baseCharClass = CJKCompositionEngine::characterClass(character, generalCategoryMask);
	switch (baseCharClass) {
		case CJKCompositionEngine::CJKCompositionCharacterClass::OpeningMarks:
            return CharacterClass::OpeningMarks;
		case CJKCompositionEngine::CJKCompositionCharacterClass::OpeningMarks_o:
            return CharacterClass::OpeningMarks;
		case CJKCompositionEngine::CJKCompositionCharacterClass::OpeningMarks_c:
            return (language == TextSpacingSupportedLanguage::TraditionalChinese) ? CharacterClass::Inseparable : CharacterClass::OpeningMarks3;
		case CJKCompositionEngine::CJKCompositionCharacterClass::ClosingMarks:
            return CharacterClass::ClosingMarks;
		case CJKCompositionEngine::CJKCompositionCharacterClass::ClosingMarks_o:
            return CharacterClass::ClosingMarks;
		case CJKCompositionEngine::CJKCompositionCharacterClass::ClosingMarks_c:
            return (language == TextSpacingSupportedLanguage::TraditionalChinese) ?  CharacterClass::Inseparable : CharacterClass::ClosingMarks3;
		case CJKCompositionEngine::CJKCompositionCharacterClass::StopMarks:
            return CharacterClass::StopMarks;
		case CJKCompositionEngine::CJKCompositionCharacterClass::StopMarks_s:
            return CharacterClass::StopMarks;
		case CJKCompositionEngine::CJKCompositionCharacterClass::StopMarks_j:
            return (language == TextSpacingSupportedLanguage::Japanese) ? CharacterClass::StopMarks : CharacterClass::Inseparable;
		case CJKCompositionEngine::CJKCompositionCharacterClass::StopMarks_c:
            return (language == TextSpacingSupportedLanguage::Japanese) ? CharacterClass::Inseparable : CharacterClass::StopMarks;
		case CJKCompositionEngine::CJKCompositionCharacterClass::Foreign:
            return CharacterClass::Latin;
		case CJKCompositionEngine::CJKCompositionCharacterClass::Inseparable:
            return CharacterClass::Inseparable;
		case CJKCompositionEngine::CJKCompositionCharacterClass::Whitespace:
            return CharacterClass::Inseparable;
//		case CJKCompositionEngine::CJKCompositionCharacterClass::MiddleDots:			 return CharacterClass::MiddleDots;
		default:
            return CharacterClass::Other;
	}
}
CJKAdjustingSpacing CJKCompositionRules::characterSpacing(UChar32 character, UChar32 nextCharacter, TextSpacingSupportedLanguage language, uint32_t generalCategoryMask)
{
    CJKAdjustingSpacing spacing;
    static const auto numberOfClasses = static_cast<unsigned>(CharacterClass::NumberOfClasses);
    using enum AditionalSpacingType;
    static const AditionalSpacingType spacingTypeTable[3][numberOfClasses][numberOfClasses] = { {
        /* Japanese:                                                                                                */
		/*                    Opening,   Opening3, Closing,  Closing3,  Stop,      kLatin,    Other,   kInseparable */
		/* Opening */       {  ca_1_4,   ca_1_4,   ______,   ______,    ______,    ______,    ______,  ______  },
		/* Opening3 */      {  ca_1_4,   ca_1_4,   ______,   ______,    ______,    ______,    ______,  ______  },
		/* Closing */       {  ba_1_4,   ba_1_4,   pa_1_4,   pa_1_4,    pa_1_8,    pa_2_5,    pa_2_5,  ______  },
		/* Closing3 */      {  ba_1_4,   ba_1_4,   pa_1_4,   pa_1_4,    pa_1_8,    pa_1_4,    p__1_4,  ______  },
		/* Stop */          {  p__0__,   p__0__,   pa_1_4,   pa_1_4,    p__0__,    p__0__,    p__0__,  p__0__  },
		/* kLatin */        {  ca_2_5,   ca_1_4,   ______,   ______,    ______,    ______,    pa_1_8,  ______  },
		/* Other */         {  ca_2_5,   ca_1_4,   ______,   ______,    ______,    ca_1_8,    ______,  ______  },
		/* kInseparable */  {  ______,   ______,   ______,   ______,    ______,    ______,    ______,  ______  },
    },
    {
		/* Simplified Chinese:                                                                                      */
		/*                    Opening,   Opening3, Closing,  Closing3,  Stop,      kLatin,    Other,   kInseparable */
		/* Opening */       {  ca_1_4,   ca_1_4,   ______,   ______,    ______,    ______,    ______,  ______  },
		/* Opening3 */      {  ca_1_4,   ca_1_4,   ______,   ______,    ______,    ______,    ______,  ______  },
		/* Closing */       {  ba_1_4,   ba_1_4,   pa_1_4,   pa_1_4,    pa_1_8,    pa_2_5,    pa_2_5,  ______  },
		/* Closing3 */      {  ba_1_4,   ba_1_4,   pa_1_4,   pa_1_4,    pa_1_8,    pa_1_4,    p__1_4,  ______  },
		/* Stop */          {  p__0__,   p__0__,   pa_1_4,   pa_1_4,    pa_1_4,    p__0__,    p__0__,  p__0__  },
		/* kLatin */        {  ca_2_5,   ca_1_4,   ______,   ______,    ______,    ______,    pa_1_8,  ______  },
		/* Other */         {  ca_2_5,   ca_1_4,   ______,   ______,    ______,    ca_1_8,    ______,  ______  },
		/* kInseparable */  {  ______,   ______,   ______,   ______,    ______,    ______,    ______,  ______  },
    },
    {
        /* Traditional Chinese:                                                                                      */
		/*                    Opening,   Opening3, Closing,  Closing3,  Stop,      kLatin,     Other,   kInseparable */
		/* Opening */       {  ca_1_5,   ______,   ______,   ______,    ______,    ______,    ______,  ______  },
		/* Opening3 */      {  ______,   ______,   ______,   ______,    ______,    ______,    ______,  ______  },
		/* Closing */       {  ba_1_5,   ______,   pa_1_5,   ______,    cs_0__,    pa_2_5,    pa_2_5,  ______  },
		/* Closing3 */      {  ______,   ______,   ______,   ______,    ______,    ______,    ______,  ______  },
		/* Stop */          {  ps_0__,   ______,   pa_1_4,   ______,    cs_0__,    ps_0__,    ps_0__,  p__0__  },
		/* kLatin */        {  ca_2_5,   ______,   ______,   ______,    ca_1_8,    ______,    pa_1_8,  ______  },
		/* Other */         {  ca_2_5,   ______,   ______,   ______,    cs_0__,    ca_1_8,    ______,  ______  },
		/* kInseparable */  {  ______,   ______,   ______,   ______,    ______,    ______,    ______,  ______  },
    } };

    static const struct {CGFloat current; CGFloat next; } spacingValuesTable[] = {
        /* ______ */	{ 0.0,      0.0  },
		/* ba_1_4 */	{ 0.25,     0.25 },
		/* ba_1_5 */	{ 0.2,      0.2  },
		/* pa_1_5 */	{ 0.2,      0.0  },
		/* pa_1_4 */	{ 0.25,     0.0  },
		/* pa_2_5 */	{ 0.4,      0.0  },
		/* ca_2_5 */	{ 0.0,      0.4  },
		/* p__0__ */	{ 0.5,      0.0  },		// Half-width current char appears as Full-width.
		/* ps_0__ */	{ 0.25,     0.0  },		// Centered Half-width current char appears as Full-width.
		/* cs_0__ */	{ 0.0,      0.25 },		// Centered Half-width next char appears as Full-width.
		/* p__1_4 */	{ 0.25,     0.0  },		// 1 - 1/4 - 1/2 = 1/4
		/* pa_1_8 */	{ 0.125,    0.0  },		// This is not assuming Half-width.
		/* ca_1_8 */	{ 0.0,      0.125},     // This is not assuming Half-width.
		/* ca_1_4 */	{ 0.0,      0.25 },     // This is not assuming Half-width.
		/* ca_1_5 */	{ 0.0,      0.2  },     // This is not assuming Half-width.
    };

    ASSERT((sizeof(spacingValuesTable)/sizeof(spacingValuesTable[0])) == static_cast<unsigned>(AditionalSpacingType::NumberOfAdditionalSpacingTypes));

    auto currentCharacterClass = characterClass(character, language, generalCategoryMask);
    auto nextCharacterClass = characterClass(nextCharacter, language, generalCategoryMask);

    bool currentCharacterOpening = currentCharacterClass == CharacterClass::OpeningMarks || currentCharacterClass == CharacterClass::OpeningMarks3;
    bool currentCharacterClosing = currentCharacterClass == CharacterClass::ClosingMarks || currentCharacterClass == CharacterClass::ClosingMarks3;

    bool currentCharacterStop = currentCharacterClass == CharacterClass::StopMarks;
    bool currentCharacterSoftStop = currentCharacterStop && isSoftStop(character);

    bool nextCharacterOpening = nextCharacterClass == CharacterClass::OpeningMarks || nextCharacterClass == CharacterClass::OpeningMarks3;
    bool nextCharacterClosing = nextCharacterClass == CharacterClass::ClosingMarks || nextCharacterClass == CharacterClass::ClosingMarks3;
    bool nextCharacterStop = nextCharacterClass == CharacterClass::StopMarks;

    spacing.shouldAdjustCurrentRightOpticalBound = ((currentCharacterOpening && nextCharacterOpening) || (currentCharacterClosing && (nextCharacterClass != CharacterClass::Inseparable)) || (currentCharacterStop && nextCharacterClosing)) && !((language == TextSpacingSupportedLanguage::TraditionalChinese) && currentCharacterClosing && nextCharacterStop);
    spacing.shouldAdjustNextLeftOpticalBound = ((nextCharacterOpening && (currentCharacterClass != CharacterClass::Inseparable)) || (currentCharacterClosing && nextCharacterClosing) || (currentCharacterStop && (nextCharacterClosing || nextCharacterStop))) && !((language == TextSpacingSupportedLanguage::TraditionalChinese) && currentCharacterStop && nextCharacterOpening);

    spacing.shouldAdjustCurrentRightFixedOpticalBound = (currentCharacterClosing && isNoRightMarginClosingMark(character)) && (nextCharacterClass == CharacterClass::ClosingMarks || nextCharacterClass == CharacterClass::ClosingMarks3);
    spacing.shouldAdjustNextLeftFixedOpticalBound = (nextCharacterOpening && isNoLeftMarginOpeningMark(nextCharacter)) && (currentCharacterClass == CharacterClass::OpeningMarks || currentCharacterClass == CharacterClass::OpeningMarks3);

    int languageIndex =  static_cast<int>(language) - static_cast<int>(TextSpacingSupportedLanguage::Japanese);

    // Special case for date (Month and Day);
    if ((isDigit(character) && isChineseMonthOrDay(nextCharacter)) || (isChineseMonthOrDay(character) && isDigit(nextCharacter))) {
    spacing.currentRightSpace = 0.04;
    spacing.nextLeftSpace = 0.0;
    return spacing;
    }

    // Treat Latin parentheses as "Half-Width", but only in certain direction.
    if ((currentCharacterClass == CharacterClass::Inseparable) && (nextCharacterClass == CharacterClass::Other) && isLatinClosing(character) ) {
        currentCharacterClass = CharacterClass::Latin;
    } else if ((currentCharacterClass == CharacterClass::Other) && (nextCharacterClass == CharacterClass::Inseparable) && isLatinOpening(nextCharacter)) {
    	nextCharacterClass = CharacterClass::Latin;
    }

    // Bind Latin + SINGLE QUOTATION MARK - rdar://113037655&111166254
    if ((nextCharacter == 0x2018 /* ‘ LEFT SINGLE QUOTATION MARK */) && (currentCharacterClass == CharacterClass::Latin)){
        nextCharacterClass = CharacterClass::Latin;
        spacing.shouldAdjustNextLeftOpticalBound = false;
    } else if ((character == 0x2019 /* ’ RIGHT SINGLE QUOTATION MARK */) && (nextCharacterClass == CharacterClass::Latin)){
        currentCharacterClass = CharacterClass::Latin;
        spacing.shouldAdjustCurrentRightOpticalBound = false;
    }

    if (language == TextSpacingSupportedLanguage::TraditionalChinese) { // rdar://111605650
    	if (nextCharacterOpening && isLeftQuotationMark(character)) {
            spacing.shouldAdjustNextLeftOpticalBound = false;
    	} else if (currentCharacterClosing && isRightQuotationMark(nextCharacter)) {
            spacing.shouldAdjustCurrentRightOpticalBound = false;
    	}
    }

    AditionalSpacingType spacingType = spacingTypeTable[languageIndex][static_cast<unsigned>(currentCharacterClass)][static_cast<unsigned>(nextCharacterClass)];

    auto spacingValue = spacingValuesTable[static_cast<unsigned>(spacingType)];
    spacing.currentRightSpace = spacingValue.current;
    spacing.nextLeftSpace = spacingValue.next;

if ((language != TextSpacingSupportedLanguage::TraditionalChinese) && currentCharacterSoftStop)
    if (nextCharacterOpening || nextCharacterClass == CharacterClass::Latin) { // Soft-Stop followed by Opening or Latin - 2/5 em to SoftStop
	    spacing.currentRightSpace = 0.4;
	    spacing.nextLeftSpace = 0.0;
	}

    return spacing;
}

bool CJKCompositionRules::isSoftStop(UChar32 stopChar)
{
    return CJKCompositionEngine::characterClass(stopChar, 0) == CJKCompositionEngine::CJKCompositionCharacterClass::StopMarks_s;
}

bool CJKCompositionRules::isNoLeftMarginOpeningMark(UChar32 openingChar)
{
	return CJKCompositionEngine::characterClass(openingChar, 0) == CJKCompositionEngine::CJKCompositionCharacterClass::OpeningMarks_o;
}

bool CJKCompositionRules::isNoRightMarginClosingMark(UChar32 closingChar)
{
    return CJKCompositionEngine::characterClass(closingChar, 0) == CJKCompositionEngine::CJKCompositionCharacterClass::ClosingMarks_o;
}

bool CJKCompositionRules::isLeftQuotationMark(UChar32 charCode)
{
    return (charCode == 0x2018 /* ‘ */ || charCode == 0x201C /* “ */);
}

bool CJKCompositionRules::isRightQuotationMark(UChar32 charCode)
{
    return (charCode == 0x2019 /* ’ */ || charCode == 0x201D /* ” */);
}

bool CJKCompositionRules::isDigit(UChar32 charCode)
{
    return charCode >= 0x0030 && charCode <= 0x0039;
}

bool CJKCompositionRules::isChineseMonthOrDay(UChar32 charCode)
{
    return charCode == 0x6708 /* 月 */ || charCode == 0x65E5 /* 日 */;
}

bool CJKCompositionRules::isLatinOpening(UChar32 charCode)
{
    return (charCode == '(');
}

bool CJKCompositionRules::isLatinClosing(UChar32 charCode)
{
    return (charCode == ')');
}
} // namespace WebCore
