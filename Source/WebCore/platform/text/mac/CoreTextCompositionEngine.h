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

#pragma once

#include <pal/spi/cf/CoreTextSPI.h>

using CharacterClass = uint8_t;

namespace WebCore {

enum class TextSpacingSupportedLanguage : uint8_t;

// Each glyph has some spacing on its left and right: |left-space|A|right-after|left-space|B|right-space|
// Spacing is processed for a pair of adjacent glyphs. Here 'A' is the current glyph and 'B' the next.
struct CJKAdjustingSpacing {
    CGFloat currentRightSpace = 0.0; // Space to the right of current glyph (l-s): |A|r-s|
    CGFloat nextLeftSpace = 0.0; // Space to the left of next glyph (r-s):               |r-s|B|

    // FIXME: Implement optical bound adjustment rdar://116480457
    bool shouldAdjustCurrentRightOpticalBound = false; // current glyph requires optical bound adjusting to the right. This adjustment is calculated based on the glyph's optical bounds.
    bool shouldAdjustNextLeftOpticalBound = false; // next glyph requires calculated optical bound adjusting to the left.This adjustment is calculated based on the glyph's optical bounds.
    bool shouldAdjustCurrentRightFixedOpticalBound = false; // current glyph requires fixed optical bound adjusting to the right. This adjustment is fixed and introduced by the engine.
    bool shouldAdjustNextLeftFixedOpticalBound = false; // next glyph requires fixed optical bound adjusting to the left. This adjustment is fixed and introduced by the engine.
};

struct CJKCompositionEngine {

enum class CJKCompositionCharacterClass : uint8_t {
    OpeningMarks,
    OpeningMarks_o,	// needs optical bounds adjustment
    OpeningMarks_c,	// Chinese only
    ClosingMarks,
    ClosingMarks_o,	// needs optical bounds adjustment
    ClosingMarks_c,	// Chinese only
    StopMarks,
    StopMarks_s,	// Soft stops
    StopMarks_j,	// Japanese only
    StopMarks_c,	// FULLWIDTH QUESTION MARK/EXCLAMATION MARK/COLON/SEMICOLON
    MiddleDots,		// Japaense only
    Foreign,		// Roman, etc.
    Whitespace,
    Inseparable,	// '…'
    Other,
    NumClasses
};

static CJKCompositionCharacterClass characterClass(UChar32 character, uint32_t generalCategoryMask);
// FIXME: implement optical bounds adjustment rdar://116480457
static CGFloat leftOpticalBoundsAdjustment();
static CGFloat rightOpticalBoundsAdjustment();
};

struct CJKCompositionRules {
enum class AditionalSpacingType : uint8_t {
    ______,		// do nothing
    ba_1_4,		// Add 1/4 em to the right optical edge of prev char, and add 1/4 em to the left optical edge current char.
    ba_1_5,		// Add 1/5 em to the right optical edge of prev char, and add 1/5 em to the left optical edge current char.
    pa_1_5,		// Add 1/5 em to the right optical edge of prev char.
    pa_1_4,		// Add 1/4 em to the right optical edge of prev char.
    pa_2_5,		// Add 2/5 em to the right optical edge prev char.
    ca_2_5,		// Add 2/5 em to the left optical edge current char.
    p__0__,		// Appears like nothing was added or removed.   NOTE: Half-width is assumed, so this actually means "adding 1/2 to the right side of previous char".
    ps_0__,		// Appears like nothing was added or removed.   NOTE: Centered Half-width is assumed, so this actually means "adding 1/4 to the right side of previous char".
    cs_0__,		// Appears like nothing was added or removed.   NOTE: Centered Half-width is assumed, so this actually means "adding 1/4 to the left side of current char".
    p__1_4,		// Remove 1/4 from the right side of previous char.  Note: This means "adding 1/4 to previous char".
    pa_1_8,		// Add 1/8 to the right side of previous char.  // NOTE: This is actual size, not assuming Half-width.
    ca_1_8,		// Add 1/8 to the left side of current char.	// NOTE: This is actual size, not assuming Half-width.
    ca_1_4,		// Add 1/4 to the left side of current char.	// NOTE: This is actual size, not assuming Half-width.
    ca_1_5,		// Add 1/5 to the left side of current char.	// NOTE: This is actual size, not assuming Half-width.
    NumberOfAdditionalSpacingTypes
};

enum class CharacterClass : uint8_t {
    // CoreText denomination.
    OpeningMarks,
    OpeningMarks3,
    ClosingMarks,
    ClosingMarks3,
    StopMarks,
    Latin,
    Other,
    Inseparable,
    // MiddleDots,
    NumberOfClasses // Reserved for counting the total number of classes.
};

    static CharacterClass characterClass(UChar32, TextSpacingSupportedLanguage, uint32_t);
    static CJKAdjustingSpacing characterSpacing(UChar32 currentCharacter, UChar32 nextCharacter, TextSpacingSupportedLanguage language, uint32_t currentCharGeneralCategoryMask);

    static bool isSoftStop(UChar32 stopChar);
    static bool isNoLeftMarginOpeningMark(UChar32 openingChar);
    static bool isNoRightMarginClosingMark(UChar32 closingChar);
    static bool isLatinOpening(UChar32 charCode);
    static bool isLatinClosing(UChar32 charCode);
    static bool isLeftQuotationMark(UChar32 charCode);
    static bool isRightQuotationMark(UChar32 charCode);
    static bool isDigit(UChar32 charCode);
    static bool isChineseMonthOrDay(UChar32 charCode);
    static bool isOpening(UChar32 character);
    static bool isClosing(UChar32 character);

    static bool shouldGlyphImageLeftFlush(TextSpacingSupportedLanguage, bool, UChar32, bool *hasNoMargin);
    static bool shouldGlyphImageRightFlush(TextSpacingSupportedLanguage, bool, UChar32, bool *hasNoMargin);
};

} // namespace WebCore
