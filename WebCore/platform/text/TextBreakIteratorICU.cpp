/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "TextBreakIterator.h"

#include "PlatformString.h"
#include "TextBreakIteratorInternalICU.h"

#include <unicode/ubrk.h>
#include <wtf/Assertions.h>

namespace WebCore {

static TextBreakIterator* setUpIterator(bool& createdIterator, TextBreakIterator*& iterator,
    UBreakIteratorType type, const UChar* string, int length)
{
    if (!string)
        return 0;

    if (!createdIterator) {
        UErrorCode openStatus = U_ZERO_ERROR;
        iterator = static_cast<TextBreakIterator*>(ubrk_open(type, currentTextBreakLocaleID(), 0, 0, &openStatus));
        createdIterator = true;
        ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    }
    if (!iterator)
        return 0;

    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setText(iterator, string, length, &setTextStatus);
    if (U_FAILURE(setTextStatus))
        return 0;

    return iterator;
}

TextBreakIterator* characterBreakIterator(const UChar* string, int length)
{
    static bool createdCharacterBreakIterator = false;
    static TextBreakIterator* staticCharacterBreakIterator;
    return setUpIterator(createdCharacterBreakIterator,
        staticCharacterBreakIterator, UBRK_CHARACTER, string, length);
}

TextBreakIterator* wordBreakIterator(const UChar* string, int length)
{
    static bool createdWordBreakIterator = false;
    static TextBreakIterator* staticWordBreakIterator;
    return setUpIterator(createdWordBreakIterator,
        staticWordBreakIterator, UBRK_WORD, string, length);
}

TextBreakIterator* lineBreakIterator(const UChar* string, int length)
{
    static bool createdLineBreakIterator = false;
    static TextBreakIterator* staticLineBreakIterator;
    return setUpIterator(createdLineBreakIterator,
        staticLineBreakIterator, UBRK_LINE, string, length);
}

TextBreakIterator* sentenceBreakIterator(const UChar* string, int length)
{
    static bool createdSentenceBreakIterator = false;
    static TextBreakIterator* staticSentenceBreakIterator;
    return setUpIterator(createdSentenceBreakIterator,
        staticSentenceBreakIterator, UBRK_SENTENCE, string, length);
}

int textBreakFirst(TextBreakIterator* bi)
{
    return ubrk_first(bi);
}

int textBreakNext(TextBreakIterator* bi)
{
    return ubrk_next(bi);
}

int textBreakPreceding(TextBreakIterator* bi, int pos)
{
    return ubrk_preceding(bi, pos);
}

int textBreakFollowing(TextBreakIterator* bi, int pos)
{
    return ubrk_following(bi, pos);
}

int textBreakCurrent(TextBreakIterator* bi)
{
    return ubrk_current(bi);
}

bool isTextBreak(TextBreakIterator* bi, int pos)
{
    return ubrk_isBoundary(bi, pos);
}

#ifndef BUILDING_ON_TIGER
static TextBreakIterator* setUpIteratorWithRules(bool& createdIterator, TextBreakIterator*& iterator,
    const char* breakRules, const UChar* string, int length)
{
    if (!string)
        return 0;

    if (!createdIterator) {
        UParseError parseStatus;
        UErrorCode openStatus = U_ZERO_ERROR;
        String rules(breakRules);
        iterator = static_cast<TextBreakIterator*>(ubrk_openRules(rules.characters(), rules.length(), 0, 0, &parseStatus, &openStatus));
        createdIterator = true;
        ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    }
    if (!iterator)
        return 0;

    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setText(iterator, string, length, &setTextStatus);
    if (U_FAILURE(setTextStatus))
        return 0;

    return iterator;
}
#endif // BUILDING_ON_TIGER

TextBreakIterator* cursorMovementIterator(const UChar* string, int length)
{
#ifdef BUILDING_ON_TIGER
    // ICU 3.2 cannot compile the below rules.
    return characterBreakIterator(string, length);
#else
    // This rule set is based on character-break iterator rules of ICU 4.0
    // <http://source.icu-project.org/repos/icu/icu/tags/release-4-0/source/data/brkitr/char.txt>.
    // The major differences from the original ones are listed below:
    // * Replaced '[\p{Grapheme_Cluster_Break = SpacingMark}]' with '[\p{General_Category = Spacing Mark} - $Extend]' for ICU 3.8 or earlier;
    // * Removed rules that prevent a cursor from moving after prepend characters (Bug 24342);
    // * Added rules that prevent a cursor from moving after virama signs of Indic languages except Tamil (Bug 15790), and;
    // * Added rules that prevent a cursor from moving before Japanese half-width katakara voiced marks.
    static const char* kRules =
        "$CR      = [\\p{Grapheme_Cluster_Break = CR}];"
        "$LF      = [\\p{Grapheme_Cluster_Break = LF}];"
        "$Control = [\\p{Grapheme_Cluster_Break = Control}];"
        "$VoiceMarks = [\\uFF9E\\uFF9F];"  // Japanese half-width katakana voiced marks
        "$Extend  = [\\p{Grapheme_Cluster_Break = Extend} $VoiceMarks];"
        "$SpacingMark = [[\\p{General_Category = Spacing Mark}] - $Extend];"
        "$L       = [\\p{Grapheme_Cluster_Break = L}];"
        "$V       = [\\p{Grapheme_Cluster_Break = V}];"
        "$T       = [\\p{Grapheme_Cluster_Break = T}];"
        "$LV      = [\\p{Grapheme_Cluster_Break = LV}];"
        "$LVT     = [\\p{Grapheme_Cluster_Break = LVT}];"
        "$Hin0    = [\\u0905-\\u0939];"    // Devanagari Letter A,...,Ha
        "$HinV    = \\u094D;"              // Devanagari Sign Virama
        "$Hin1    = [\\u0915-\\u0939];"    // Devanagari Letter Ka,...,Ha
        "$Ben0    = [\\u0985-\\u09B9];"    // Bengali Letter A,...,Ha
        "$BenV    = \\u09CD;"              // Bengali Sign Virama
        "$Ben1    = [\\u0995-\\u09B9];"    // Bengali Letter Ka,...,Ha
        "$Pan0    = [\\u0A05-\\u0A39];"    // Gurmukhi Letter A,...,Ha
        "$PanV    = \\u0A4D;"              // Gurmukhi Sign Virama
        "$Pan1    = [\\u0A15-\\u0A39];"    // Gurmukhi Letter Ka,...,Ha
        "$Guj0    = [\\u0A85-\\u0AB9];"    // Gujarati Letter A,...,Ha
        "$GujV    = \\u0ACD;"              // Gujarati Sign Virama
        "$Guj1    = [\\u0A95-\\u0AB9];"    // Gujarati Letter Ka,...,Ha
        "$Ori0    = [\\u0B05-\\u0B39];"    // Oriya Letter A,...,Ha
        "$OriV    = \\u0B4D;"              // Oriya Sign Virama
        "$Ori1    = [\\u0B15-\\u0B39];"    // Oriya Letter Ka,...,Ha
        "$Tel0    = [\\u0C05-\\u0C39];"    // Telugu Letter A,...,Ha
        "$TelV    = \\u0C4D;"              // Telugu Sign Virama
        "$Tel1    = [\\u0C14-\\u0C39];"    // Telugu Letter Ka,...,Ha
        "$Kan0    = [\\u0C85-\\u0CB9];"    // Kannada Letter A,...,Ha
        "$KanV    = \\u0CCD;"              // Kannada Sign Virama
        "$Kan1    = [\\u0C95-\\u0CB9];"    // Kannada Letter A,...,Ha
        "$Mal0    = [\\u0D05-\\u0D39];"    // Malayalam Letter A,...,Ha
        "$MalV    = \\u0D4D;"              // Malayalam Sign Virama
        "$Mal1    = [\\u0D15-\\u0D39];"    // Malayalam Letter A,...,Ha
        "!!chain;"
        "!!forward;"
        "$CR $LF;"
        "$L ($L | $V | $LV | $LVT);"
        "($LV | $V) ($V | $T);"
        "($LVT | $T) $T;"
        "[^$Control $CR $LF] $Extend;"
        "[^$Control $CR $LF] $SpacingMark;"
        "$Hin0 $HinV $Hin1;"               // Devanagari Virama (forward)
        "$Ben0 $BenV $Ben1;"               // Bengali Virama (forward)
        "$Pan0 $PanV $Pan1;"               // Gurmukhi Virama (forward)
        "$Guj0 $GujV $Guj1;"               // Gujarati Virama (forward)
        "$Ori0 $OriV $Ori1;"               // Oriya Virama (forward)
        "$Tel0 $TelV $Tel1;"               // Telugu Virama (forward)
        "$Kan0 $KanV $Kan1;"               // Kannada Virama (forward)
        "$Mal0 $MalV $Mal1;"               // Malayalam Virama (forward)
        "!!reverse;"
        "$LF $CR;"
        "($L | $V | $LV | $LVT) $L;"
        "($V | $T) ($LV | $V);"
        "$T ($LVT | $T);"
        "$Extend      [^$Control $CR $LF];"
        "$SpacingMark [^$Control $CR $LF];"
        "$Hin1 $HinV $Hin0;"               // Devanagari Virama (backward)
        "$Ben1 $BenV $Ben0;"               // Bengali Virama (backward)
        "$Pan1 $PanV $Pan0;"               // Gurmukhi Virama (backward)
        "$Guj1 $GujV $Guj0;"               // Gujarati Virama (backward)
        "$Ori1 $OriV $Ori0;"               // Gujarati Virama (backward)
        "$Tel1 $TelV $Tel0;"               // Telugu Virama (backward)
        "$Kan1 $KanV $Kan0;"               // Kannada Virama (backward)
        "$Mal1 $MalV $Mal0;"               // Malayalam Virama (backward)
        "!!safe_reverse;"
        "!!safe_forward;";
    static bool createdCursorMovementIterator = false;
    static TextBreakIterator* staticCursorMovementIterator;
    return setUpIteratorWithRules(createdCursorMovementIterator, staticCursorMovementIterator, kRules, string, length);
#endif // BUILDING_ON_TIGER
}

}
