/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 */

#include "config.h"
#include "TextBreakIterator.h"

#include "LineBreakIteratorPoolICU.h"
#include "UTextProviderLatin1.h"
#include "UTextProviderUTF16.h"
#include <mutex>
#include <wtf/Atomics.h>
#include <wtf/text/StringView.h>

namespace WebCore {

// Iterator initialization

static TextBreakIterator* initializeIterator(UBreakIteratorType type, const char* locale = currentTextBreakLocaleID())
{
    UErrorCode openStatus = U_ZERO_ERROR;
    TextBreakIterator* iterator = reinterpret_cast<TextBreakIterator*>(ubrk_open(type, locale, 0, 0, &openStatus));
    ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    return iterator;
}

#if !PLATFORM(IOS)

static TextBreakIterator* initializeIteratorWithRules(const char* breakRules)
{
    UParseError parseStatus;
    UErrorCode openStatus = U_ZERO_ERROR;
    unsigned length = strlen(breakRules);
    auto upconvertedCharacters = StringView(reinterpret_cast<const LChar*>(breakRules), length).upconvertedCharacters();
    TextBreakIterator* iterator = reinterpret_cast<TextBreakIterator*>(ubrk_openRules(upconvertedCharacters, length, 0, 0, &parseStatus, &openStatus));
    ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    return iterator;
}

#endif


// Iterator text setting

static TextBreakIterator* setTextForIterator(TextBreakIterator& iterator, StringView string)
{
    if (string.is8Bit()) {
        UTextWithBuffer textLocal;
        textLocal.text = UTEXT_INITIALIZER;
        textLocal.text.extraSize = sizeof(textLocal.buffer);
        textLocal.text.pExtra = textLocal.buffer;

        UErrorCode openStatus = U_ZERO_ERROR;
        UText* text = openLatin1UTextProvider(&textLocal, string.characters8(), string.length(), &openStatus);
        if (U_FAILURE(openStatus)) {
            LOG_ERROR("uTextOpenLatin1 failed with status %d", openStatus);
            return nullptr;
        }

        UErrorCode setTextStatus = U_ZERO_ERROR;
        ubrk_setUText(reinterpret_cast<UBreakIterator*>(&iterator), text, &setTextStatus);
        if (U_FAILURE(setTextStatus)) {
            LOG_ERROR("ubrk_setUText failed with status %d", setTextStatus);
            return nullptr;
        }

        utext_close(text);
    } else {
        UErrorCode setTextStatus = U_ZERO_ERROR;
        ubrk_setText(reinterpret_cast<UBreakIterator*>(&iterator), string.characters16(), string.length(), &setTextStatus);
        if (U_FAILURE(setTextStatus))
            return nullptr;
    }

    return &iterator;
}

static TextBreakIterator* setContextAwareTextForIterator(TextBreakIterator& iterator, StringView string, const UChar* priorContext, unsigned priorContextLength)
{
    if (string.is8Bit()) {
        UTextWithBuffer textLocal;
        textLocal.text = UTEXT_INITIALIZER;
        textLocal.text.extraSize = sizeof(textLocal.buffer);
        textLocal.text.pExtra = textLocal.buffer;

        UErrorCode openStatus = U_ZERO_ERROR;
        UText* text = openLatin1ContextAwareUTextProvider(&textLocal, string.characters8(), string.length(), priorContext, priorContextLength, &openStatus);
        if (U_FAILURE(openStatus)) {
            LOG_ERROR("openLatin1ContextAwareUTextProvider failed with status %d", openStatus);
            return nullptr;
        }

        UErrorCode setTextStatus = U_ZERO_ERROR;
        ubrk_setUText(reinterpret_cast<UBreakIterator*>(&iterator), text, &setTextStatus);
        if (U_FAILURE(setTextStatus)) {
            LOG_ERROR("ubrk_setUText failed with status %d", setTextStatus);
            return nullptr;
        }

        utext_close(text);
    } else {
        UText textLocal = UTEXT_INITIALIZER;

        UErrorCode openStatus = U_ZERO_ERROR;
        UText* text = openUTF16ContextAwareUTextProvider(&textLocal, string.characters16(), string.length(), priorContext, priorContextLength, &openStatus);
        if (U_FAILURE(openStatus)) {
            LOG_ERROR("openUTF16ContextAwareUTextProvider failed with status %d", openStatus);
            return 0;
        }

        UErrorCode setTextStatus = U_ZERO_ERROR;
        ubrk_setUText(reinterpret_cast<UBreakIterator*>(&iterator), text, &setTextStatus);
        if (U_FAILURE(setTextStatus)) {
            LOG_ERROR("ubrk_setUText failed with status %d", setTextStatus);
            return nullptr;
        }

        utext_close(text);
    }

    return &iterator;
}


// Static iterators

TextBreakIterator* wordBreakIterator(StringView string)
{
    static TextBreakIterator* staticWordBreakIterator = initializeIterator(UBRK_WORD);
    if (!staticWordBreakIterator)
        return nullptr;

    return setTextForIterator(*staticWordBreakIterator, string);
}

TextBreakIterator* sentenceBreakIterator(StringView string)
{
    static TextBreakIterator* staticSentenceBreakIterator = initializeIterator(UBRK_SENTENCE);
    if (!staticSentenceBreakIterator)
        return nullptr;

    return setTextForIterator(*staticSentenceBreakIterator, string);
}

TextBreakIterator* cursorMovementIterator(StringView string)
{
#if !PLATFORM(IOS)
    // This rule set is based on character-break iterator rules of ICU 4.0
    // <http://source.icu-project.org/repos/icu/icu/tags/release-4-0/source/data/brkitr/char.txt>.
    // The major differences from the original ones are listed below:
    // * Replaced '[\p{Grapheme_Cluster_Break = SpacingMark}]' with '[\p{General_Category = Spacing Mark} - $Extend]' for ICU 3.8 or earlier;
    // * Removed rules that prevent a cursor from moving after prepend characters (Bug 24342);
    // * Added rules that prevent a cursor from moving after virama signs of Indic languages except Tamil (Bug 15790), and;
    // * Added rules that prevent a cursor from moving before Japanese half-width katakara voiced marks.
    // * Added rules for regional indicator symbols.
    static const char* kRules =
        "$CR      = [\\p{Grapheme_Cluster_Break = CR}];"
        "$LF      = [\\p{Grapheme_Cluster_Break = LF}];"
        "$Control = [\\p{Grapheme_Cluster_Break = Control}];"
        "$VoiceMarks = [\\uFF9E\\uFF9F];"  // Japanese half-width katakana voiced marks
        "$Extend  = [\\p{Grapheme_Cluster_Break = Extend} $VoiceMarks - [\\u0E30 \\u0E32 \\u0E45 \\u0EB0 \\u0EB2]];"
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
        "$RI      = [\\U0001F1E6-\\U0001F1FF];" // Emoji regional indicators
        "$ZWJ     = \\u200D;"               // Zero width joiner
        "$EmojiVar = [\\uFE0F];"            // Emoji-style variation selector
        "$EmojiForSeqs = [\\u2764 \\U0001F466-\\U0001F469 \\U0001F48B];" // Emoji that participate in ZWJ sequences
        "$EmojiForMods = [\\u261D \\u270A-\\u270C \\U0001F385 \\U0001F3C3-\\U0001F3C4 \\U0001F3C7 \\U0001F3CA \\U0001F442-\\U0001F443 \\U0001F446-\\U0001F450 \\U0001F466-\\U0001F469 \\U0001F46E-\\U0001F478 \\U0001F47C \\U0001F481-\\U0001F483 \\U0001F485-\\U0001F487 \\U0001F4AA \\U0001F596 \\U0001F645-\\U0001F647 \\U0001F64B-\\U0001F64F \\U0001F6A3 \\U0001F6B4-\\U0001F6B6 \\U0001F6C0] ;" // Emoji that take Fitzpatrick modifiers
        "$EmojiMods = [\\U0001F3FB-\\U0001F3FF];" // Fitzpatrick modifiers
        "!!chain;"
        "!!forward;"
        "$CR $LF;"
        "$L ($L | $V | $LV | $LVT);"
        "($LV | $V) ($V | $T);"
        "($LVT | $T) $T;"
        "[^$Control $CR $LF] $Extend;"
        "[^$Control $CR $LF] $SpacingMark;"
        "$RI $RI / $RI;"
        "$RI $RI;"
        "$Hin0 $HinV $Hin1;"               // Devanagari Virama (forward)
        "$Ben0 $BenV $Ben1;"               // Bengali Virama (forward)
        "$Pan0 $PanV $Pan1;"               // Gurmukhi Virama (forward)
        "$Guj0 $GujV $Guj1;"               // Gujarati Virama (forward)
        "$Ori0 $OriV $Ori1;"               // Oriya Virama (forward)
        "$Tel0 $TelV $Tel1;"               // Telugu Virama (forward)
        "$Kan0 $KanV $Kan1;"               // Kannada Virama (forward)
        "$Mal0 $MalV $Mal1;"               // Malayalam Virama (forward)
        "$ZWJ $EmojiForSeqs;"              // Don't break in emoji ZWJ sequences
        "$EmojiForMods $EmojiVar? $EmojiMods;" // Don't break between relevant emoji (possibly with variation selector) and Fitzpatrick modifier
        "!!reverse;"
        "$LF $CR;"
        "($L | $V | $LV | $LVT) $L;"
        "($V | $T) ($LV | $V);"
        "$T ($LVT | $T);"
        "$Extend      [^$Control $CR $LF];"
        "$SpacingMark [^$Control $CR $LF];"
        "$RI $RI / $RI $RI;"
        "$RI $RI;"
        "$Hin1 $HinV $Hin0;"               // Devanagari Virama (backward)
        "$Ben1 $BenV $Ben0;"               // Bengali Virama (backward)
        "$Pan1 $PanV $Pan0;"               // Gurmukhi Virama (backward)
        "$Guj1 $GujV $Guj0;"               // Gujarati Virama (backward)
        "$Ori1 $OriV $Ori0;"               // Gujarati Virama (backward)
        "$Tel1 $TelV $Tel0;"               // Telugu Virama (backward)
        "$Kan1 $KanV $Kan0;"               // Kannada Virama (backward)
        "$Mal1 $MalV $Mal0;"               // Malayalam Virama (backward)
        "$EmojiForSeqs $ZWJ;"              // Don't break in emoji ZWJ sequences
        "$EmojiMods $EmojiVar? $EmojiForMods;" // Don't break between relevant emoji (possibly with variation selector) and Fitzpatrick modifier
        "[$EmojiVar $EmojiMods]+ $EmojiForMods;"
        "$EmojiForMods [$EmojiVar $EmojiMods]+;"
        "!!safe_reverse;"
        "!!safe_forward;";
    static TextBreakIterator* staticCursorMovementIterator = initializeIteratorWithRules(kRules);
#else // PLATFORM(IOS)
    // Use the special Thai character break iterator for all locales
    static TextBreakIterator* staticCursorMovementIterator = initializeIterator(UBRK_CHARACTER, "th");
#endif // !PLATFORM(IOS)

    if (!staticCursorMovementIterator)
        return nullptr;

    return setTextForIterator(*staticCursorMovementIterator, string);
}

TextBreakIterator* acquireLineBreakIterator(StringView string, const AtomicString& locale, const UChar* priorContext, unsigned priorContextLength, LineBreakIteratorMode mode, bool isCJK)
{
    TextBreakIterator* iterator = LineBreakIteratorPool::sharedPool().take(locale, mode, isCJK);
    if (!iterator)
        return nullptr;

    return setContextAwareTextForIterator(*iterator, string, priorContext, priorContextLength);
}

void releaseLineBreakIterator(TextBreakIterator* iterator)
{
    ASSERT_ARG(iterator, iterator);

    LineBreakIteratorPool::sharedPool().put(iterator);
}

static const char* uax14Prologue =
    "!!chain;"
    "!!LBCMNoChain;"
    "!!lookAheadHardBreak;";

static const char* uax14AssignmentsBefore =
    // explicitly enumerate $CJ since ICU versions prior to 49 don't support :LineBreak=Conditional_Japanese_Starter:
    "$CJ = ["
#if (U_ICU_VERSION_MAJOR_NUM >= 4) && (U_ICU_VERSION_MINOR_NUM >= 9)
    ":LineBreak=Conditional_Japanese_Starter:"
#else
    "\\u3041\\u3043\\u3045\\u3047\\u3049\\u3063\\u3083\\u3085\\u3087\\u308E\\u3095\\u3096\\u30A1\\u30A3\\u30A5\\u30A7"
    "\\u30A9\\u30C3\\u30E3\\u30E5\\u30E7\\u30EE\\u30F5\\u30F6\\u30FC"
    "\\u31F0\\u31F1\\u31F2\\u31F3\\u31F4\\u31F5\\u31F6\\u31F7\\u31F8\\u31F9\\u31FA\\u31FB\\u31FC\\u31FD\\u31FE\\u31FF"
    "\\uFF67\\uFF68\\uFF69\\uFF6A\\uFF6B\\uFF6C\\uFF6D\\uFF6E\\uFF6F\\uFF70"
#endif
    "];";

static const char* uax14AssignmentsCustomLooseCJK =
    "$BA_SUB = [\\u2010\\u2013];"
    "$EX_SUB = [\\u0021\\u003F\\uFF01\\uFF1F];"
    "$ID_SUB = '';"
    "$IN_SUB = [\\u2025\\u2026];"
    "$IS_SUB = [\\u003A\\u003B];"
    "$NS_SUB = [\\u203C\\u2047\\u2048\\u2049\\u3005\\u301C\\u303B\\u309D\\u309E\\u30A0\\u30FB\\u30FD\\u30FE\\uFF1A\\uFF1B\\uFF65];"
    "$PO_SUB = [\\u0025\\u00A2\\u00B0\\u2030\\u2032\\u2033\\u2103\\uFF05\\uFFE0];"
    "$PR_SUB = [\\u0024\\u00A3\\u00A5\\u20AC\\u2116\\uFF04\\uFFE1\\uFFE5];"
    "$ID_ADD = [$CJ $BA_SUB $EX_SUB $IN_SUB $IS_SUB $NS_SUB $PO_SUB $PR_SUB];"
    "$NS_ADD = '';";

static const char* uax14AssignmentsCustomLooseNonCJK =
    "$BA_SUB = '';"
    "$EX_SUB = '';"
    "$ID_SUB = '';"
    "$IN_SUB = [\\u2025\\u2026];"
    "$IS_SUB = '';"
    "$NS_SUB = [\\u3005\\u303B\\u309D\\u309E\\u30FD\\u30FE];"
    "$PO_SUB = '';"
    "$PR_SUB = '';"
    "$ID_ADD = [$CJ $IN_SUB $NS_SUB];"
    "$NS_ADD = '';";

static const char* uax14AssignmentsCustomNormalCJK =
    "$BA_SUB = [\\u2010\\u2013];"
    "$EX_SUB = '';"
    "$IN_SUB = '';"
    "$ID_SUB = '';"
    "$IS_SUB = '';"
    "$NS_SUB = [\\u301C\\u30A0];"
    "$PO_SUB = '';"
    "$PR_SUB = '';"
    "$ID_ADD = [$CJ $BA_SUB $NS_SUB];"
    "$NS_ADD = '';";

static const char* uax14AssignmentsCustomNormalNonCJK =
    "$BA_SUB = '';"
    "$EX_SUB = '';"
    "$ID_SUB = '';"
    "$IN_SUB = '';"
    "$IS_SUB = '';"
    "$NS_SUB = '';"
    "$PO_SUB = '';"
    "$PR_SUB = '';"
    "$ID_ADD = [$CJ];"
    "$NS_ADD = '';";

static const char* uax14AssignmentsCustomStrictCJK =
    "$BA_SUB = '';"
    "$EX_SUB = '';"
    "$ID_SUB = '';"
    "$IN_SUB = '';"
    "$IS_SUB = '';"
    "$NS_SUB = '';"
    "$PO_SUB = '';"
    "$PR_SUB = '';"
    "$ID_ADD = '';"
    "$NS_ADD = [$CJ];";

#define uax14AssignmentsCustomStrictNonCJK      uax14AssignmentsCustomStrictCJK
#define uax14AssignmentsCustomDefaultCJK        uax14AssignmentsCustomNormalCJK
#define uax14AssignmentsCustomDefaultNonCJK     uax14AssignmentsCustomStrictNonCJK

static const char* uax14AssignmentsAfter =
    "$AI = [:LineBreak = Ambiguous:];"
    "$AL = [:LineBreak = Alphabetic:];"
    "$BA = [[:LineBreak = Break_After:] - $BA_SUB];"
    "$BB = [:LineBreak = Break_Before:];"
    "$BK = [:LineBreak = Mandatory_Break:];"
    "$B2 = [:LineBreak = Break_Both:];"
    "$CB = [:LineBreak = Contingent_Break:];"
    "$CL = [:LineBreak = Close_Punctuation:];"
    "$CM = [:LineBreak = Combining_Mark:];"
    "$CP = [:LineBreak = Close_Parenthesis:];"
    "$CR = [:LineBreak = Carriage_Return:];"
    "$EX = [[:LineBreak = Exclamation:] - $EX_SUB];"
    "$GL = [:LineBreak = Glue:];"
#if (U_ICU_VERSION_MAJOR_NUM >= 4) && (U_ICU_VERSION_MINOR_NUM >= 9)
    "$HL = [:LineBreak = Hebrew_Letter:];"
#else
    "$HL = [[:Hebrew:] & [:Letter:]];"
#endif
    "$HY = [:LineBreak = Hyphen:];"
    "$H2 = [:LineBreak = H2:];"
    "$H3 = [:LineBreak = H3:];"
    "$ID = [[[[:LineBreak = Ideographic:] - $CJ] $ID_ADD] - $ID_SUB];"
    "$IN = [[:LineBreak = Inseparable:] - $IN_SUB];"
    "$IS = [[:LineBreak = Infix_Numeric:] - $IS_SUB];"
    "$JL = [:LineBreak = JL:];"
    "$JV = [:LineBreak = JV:];"
    "$JT = [:LineBreak = JT:];"
    "$LF = [:LineBreak = Line_Feed:];"
    "$NL = [:LineBreak = Next_Line:];"
    "$NS = [[[[:LineBreak = Nonstarter:] - $CJ] $NS_ADD] - $NS_SUB];"
    "$NU = [:LineBreak = Numeric:];"
    "$OP = [:LineBreak = Open_Punctuation:];"
    "$PO = [[:LineBreak = Postfix_Numeric:] - $PO_SUB];"
    "$PR = [[:LineBreak = Prefix_Numeric:] - $PR_SUB];"
    "$QU = [:LineBreak = Quotation:];"
    "$SA = [:LineBreak = Complex_Context:];"
    "$SG = [:LineBreak = Surrogate:];"
    "$SP = [:LineBreak = Space:];"
    "$SY = [:LineBreak = Break_Symbols:];"
    "$WJ = [:LineBreak = Word_Joiner:];"
    "$XX = [:LineBreak = Unknown:];"
    "$ZW = [:LineBreak = ZWSpace:];"
    "$ZWJ = \\u200D;"
    "$EmojiVar = \\uFE0F;"
    "$EmojiForSeqs = [\\u2764 \\U0001F466-\\U0001F469 \\U0001F48B];"
    "$EmojiForMods = [\\u261D \\u270A-\\u270C \\U0001F385 \\U0001F3C3-\\U0001F3C4 \\U0001F3C7 \\U0001F3CA \\U0001F442-\\U0001F443 \\U0001F446-\\U0001F450 \\U0001F466-\\U0001F469 \\U0001F46E-\\U0001F478 \\U0001F47C \\U0001F481-\\U0001F483 \\U0001F485-\\U0001F487 \\U0001F4AA \\U0001F596 \\U0001F645-\\U0001F647 \\U0001F64B-\\U0001F64F \\U0001F6A3 \\U0001F6B4-\\U0001F6B6 \\U0001F6C0] ;" // Emoji that take Fitzpatrick modifiers
    "$EmojiMods = [\\U0001F3FB-\\U0001F3FF];"
    "$dictionary = [:LineBreak = Complex_Context:];"
    "$ALPlus = [$AL $AI $SA $SG $XX];"
    "$ALcm = $ALPlus $CM*;"
    "$BAcm = $BA $CM*;"
    "$BBcm = $BB $CM*;"
    "$B2cm = $B2 $CM*;"
    "$CLcm = $CL $CM*;"
    "$CPcm = $CP $CM*;"
    "$EXcm = $EX $CM*;"
    "$GLcm = $GL $CM*;"
    "$HLcm = $HL $CM*;"
    "$HYcm = $HY $CM*;"
    "$H2cm = $H2 $CM*;"
    "$H3cm = $H3 $CM*;"
    "$IDcm = $ID $CM*;"
    "$INcm = $IN $CM*;"
    "$IScm = $IS $CM*;"
    "$JLcm = $JL $CM*;"
    "$JVcm = $JV $CM*;"
    "$JTcm = $JT $CM*;"
    "$NScm = $NS $CM*;"
    "$NUcm = $NU $CM*;"
    "$OPcm = $OP $CM*;"
    "$POcm = $PO $CM*;"
    "$PRcm = $PR $CM*;"
    "$QUcm = $QU $CM*;"
    "$SYcm = $SY $CM*;"
    "$WJcm = $WJ $CM*;";

static const char* uax14Forward =
    "!!forward;"
    "$CAN_CM = [^$SP $BK $CR $LF $NL $ZW $CM];"
    "$CANT_CM = [$SP $BK $CR $LF $NL $ZW $CM];"
    "$AL_FOLLOW_NOCM = [$BK $CR $LF $NL $ZW $SP];"
    "$AL_FOLLOW_CM = [$CL $CP $EX $HL $IS $SY $WJ $GL $OP $QU $BA $HY $NS $IN $NU $ALPlus];"
    "$AL_FOLLOW = [$AL_FOLLOW_NOCM $AL_FOLLOW_CM];"
    "$LB4Breaks = [$BK $CR $LF $NL];"
    "$LB4NonBreaks = [^$BK $CR $LF $NL];"
    "$LB8Breaks = [$LB4Breaks $ZW];"
    "$LB8NonBreaks = [[$LB4NonBreaks] - [$ZW]];"
    "$LB18NonBreaks = [$LB8NonBreaks - [$SP]];"
    "$LB18Breaks = [$LB8Breaks $SP];"
    "$LB20NonBreaks = [$LB18NonBreaks - $CB];"
    "$ALPlus $CM+;"
    "$BA $CM+;"
    "$BB $CM+;"
    "$B2 $CM+;"
    "$CL $CM+;"
    "$CP $CM+;"
    "$EX $CM+;"
    "$GL $CM+;"
    "$HL $CM+;"
    "$HY $CM+;"
    "$H2 $CM+;"
    "$H3 $CM+;"
    "$ID $CM+;"
    "$IN $CM+;"
    "$IS $CM+;"
    "$JL $CM+;"
    "$JV $CM+;"
    "$JT $CM+;"
    "$NS $CM+;"
    "$NU $CM+;"
    "$OP $CM+;"
    "$PO $CM+;"
    "$PR $CM+;"
    "$QU $CM+;"
    "$SY $CM+;"
    "$WJ $CM+;"
    "$CR $LF {100};"
    "$LB4NonBreaks? $LB4Breaks {100};"
    "$CAN_CM $CM* $LB4Breaks {100};"
    "$CM+ $LB4Breaks {100};"
    "$LB4NonBreaks [$SP $ZW];"
    "$CAN_CM $CM* [$SP $ZW];"
    "$CM+ [$SP $ZW];"
    "$EmojiForSeqs $EmojiVar? $EmojiMods? $ZWJ $EmojiForSeqs;"
    "$CAN_CM $CM+;"
    "$CM+;"
    "$CAN_CM $CM* $WJcm;"
    "$LB8NonBreaks $WJcm;"
    "$CM+ $WJcm;"
    "$WJcm $CANT_CM;"
    "$WJcm $CAN_CM $CM*;"
    "$GLcm $CAN_CM $CM*;"
    "$GLcm $CANT_CM;"
    "[[$LB8NonBreaks] - [$SP $BA $HY]] $CM* $GLcm;"
    "$CM+ GLcm;"
    "$LB8NonBreaks $CL;"
    "$CAN_CM $CM* $CL;"
    "$CM+ $CL;"
    "$LB8NonBreaks $CP;"
    "$CAN_CM $CM* $CP;"
    "$CM+ $CP;"
    "$LB8NonBreaks $EX;"
    "$CAN_CM $CM* $EX;"
    "$CM+ $EX;"
    "$LB8NonBreaks $IS;"
    "$CAN_CM $CM* $IS;"
    "$CM+ $IS;"
    "$LB8NonBreaks $SY;"
    "$CAN_CM $CM* $SY;"
    "$CM+ $SY;"
    "$OPcm $SP* $CAN_CM $CM*;"
    "$OPcm $SP* $CANT_CM;"
    "$OPcm $SP+ $CM+ $AL_FOLLOW?;"
    "$QUcm $SP* $OPcm;"
    "($CLcm | $CPcm) $SP* $NScm;"
    "$B2cm $SP* $B2cm;"
    "$LB18NonBreaks $CM* $QUcm;"
    "$CM+ $QUcm;"
    "$QUcm .?;"
    "$QUcm $LB18NonBreaks $CM*;"
    "$LB20NonBreaks $CM* ($BAcm | $HYcm | $NScm); "
    "$BBcm [^$CB];"
    "$BBcm $LB20NonBreaks $CM*;"
    "$HLcm ($HYcm | $BAcm) [^$CB]?;"
    "($ALcm | $HLcm) $INcm;"
    "$CM+ $INcm;"
    "$IDcm $INcm;"
    "$INcm $INcm;"
    "$NUcm $INcm;"
    "$IDcm $POcm;"
    "$ALcm $NUcm;"
    "$HLcm $NUcm;"
    "$CM+ $NUcm;"
    "$NUcm $ALcm;"
    "$NUcm $HLcm;"
    "$PRcm $IDcm;"
    "$PRcm ($ALcm | $HLcm);"
    "$POcm ($ALcm | $HLcm);"
    "($PRcm | $POcm)? ($OPcm | $HYcm)? $NUcm ($NUcm | $SYcm | $IScm)* ($CLcm | $CPcm)? ($PRcm | $POcm)?;"
    "$JLcm ($JLcm | $JVcm | $H2cm | $H3cm);"
    "($JVcm | $H2cm) ($JVcm | $JTcm);"
    "($JTcm | $H3cm) $JTcm;"
    "($JLcm | $JVcm | $JTcm | $H2cm | $H3cm) $INcm;"
    "($JLcm | $JVcm | $JTcm | $H2cm | $H3cm) $POcm;"
    "$PRcm ($JLcm | $JVcm | $JTcm | $H2cm | $H3cm);"
    "($ALcm | $HLcm) ($ALcm | $HLcm);"
    "$CM+ ($ALcm | $HLcm);"
    "$IScm ($ALcm | $HLcm);"
    "($ALcm | $HLcm | $NUcm) $OPcm;"
    "$CM+ $OPcm;"
    "$CPcm ($ALcm | $HLcm | $NUcm);"
    "$EmojiForMods $EmojiVar? $EmojiMods;";

static const char* uax14Reverse =
    "!!reverse;"
    "$CM+ $ALPlus;"
    "$CM+ $BA;"
    "$CM+ $BB;"
    "$CM+ $B2;"
    "$CM+ $CL;"
    "$CM+ $CP;"
    "$CM+ $EX;"
    "$CM+ $GL;"
    "$CM+ $HL;"
    "$CM+ $HY;"
    "$CM+ $H2;"
    "$CM+ $H3;"
    "$CM+ $ID;"
    "$CM+ $IN;"
    "$CM+ $IS;"
    "$CM+ $JL;"
    "$CM+ $JV;"
    "$CM+ $JT;"
    "$CM+ $NS;"
    "$CM+ $NU;"
    "$CM+ $OP;"
    "$CM+ $PO;"
    "$CM+ $PR;"
    "$CM+ $QU;"
    "$CM+ $SY;"
    "$CM+ $WJ;"
    "$CM+;"
    "$AL_FOLLOW $CM+ / ([$BK $CR $LF $NL $ZW {eof}] | $SP+ $CM+ $SP | $SP+ $CM* ([^$OP $CM $SP] | [$AL {eof}]));"
    "[$PR] / $CM+ [$BK $CR $LF $NL $ZW $SP {eof}];"
    "$LB4Breaks [$LB4NonBreaks-$CM];"
    "$LB4Breaks $CM+ $CAN_CM;"
    "$LF $CR;"
    "[$SP $ZW] [$LB4NonBreaks-$CM];"
    "[$SP $ZW] $CM+ $CAN_CM;"
    "$EmojiForSeqs $ZWJ $EmojiMods? $EmojiVar? $EmojiForSeqs;"
    "$CM+ $CAN_CM;"
    "$CM* $WJ $CM* $CAN_CM;"
    "$CM* $WJ [$LB8NonBreaks-$CM];"
    "$CANT_CM $CM* $WJ;"
    "$CM* $CAN_CM $CM* $WJ;"
    "$CM* $GL $CM* [$LB8NonBreaks-[$CM $SP $BA $HY]];"
    "$CANT_CM $CM* $GL;"
    "$CM* $CAN_CM $CM* $GL;"
    "$CL $CM+ $CAN_CM;"
    "$CP $CM+ $CAN_CM;"
    "$EX $CM+ $CAN_CM;"
    "$IS $CM+ $CAN_CM;"
    "$SY $CM+ $CAN_CM;"
    "$CL [$LB8NonBreaks-$CM];"
    "$CP [$LB8NonBreaks-$CM];"
    "$EX [$LB8NonBreaks-$CM];"
    "$IS [$LB8NonBreaks-$CM];"
    "$SY [$LB8NonBreaks-$CM];"
    "[$CL $CP $EX $IS $SY] $CM+ $SP+ $CM* $OP; "
    "$CM* $CAN_CM $SP* $CM* $OP;"
    "$CANT_CM $SP* $CM* $OP;"
    "$AL_FOLLOW? $CM+ $SP $SP* $CM* $OP;"
    "$AL_FOLLOW_NOCM $CM+ $SP+ $CM* $OP;"
    "$CM* $AL_FOLLOW_CM $CM+ $SP+ $CM* $OP;"
    "$SY $CM $SP+ $OP;"
    "$CM* $OP $SP* $CM* $QU;"
    "$CM* $NS $SP* $CM* ($CL | $CP);"
    "$CM* $B2 $SP* $CM* $B2;"
    "$CM* $QU $CM* $CAN_CM;"
    "$CM* $QU $LB18NonBreaks;"
    "$CM* $CAN_CM $CM* $QU;"
    "$CANT_CM $CM* $QU;"
    "$CM* ($BA | $HY | $NS) $CM* [$LB20NonBreaks-$CM];"
    "$CM* [$LB20NonBreaks-$CM] $CM* $BB;"
    "[^$CB] $CM* $BB;"
    "[^$CB] $CM* ($HY | $BA) $CM* $HL;"
    "$CM* $IN $CM* ($ALPlus | $HL);"
    "$CM* $IN $CM* $ID;"
    "$CM* $IN $CM* $IN;"
    "$CM* $IN $CM* $NU;"
    "$CM* $PO $CM* $ID;"
    "$CM* $NU $CM* ($ALPlus | $HL);"
    "$CM* ($ALPlus | $HL) $CM* $NU;"
    "$CM* $ID $CM* $PR;"
    "$CM* ($ALPlus | $HL) $CM* $PR;"
    "$CM* ($ALPlus | $HL) $CM* $PO;"
    "($CM* ($PR | $PO))? ($CM* ($CL | $CP))? ($CM* ($NU | $IS | $SY))* $CM* $NU ($CM* ($OP | $HY))? ($CM* ($PR | $PO))?;"
    "$CM* ($H3 | $H2 | $JV | $JL) $CM* $JL;"
    "$CM* ($JT | $JV) $CM* ($H2 | $JV);"
    "$CM* $JT $CM* ($H3 | $JT);"
    "$CM* $IN $CM* ($H3 | $H2 | $JT | $JV | $JL);"
    "$CM* $PO $CM* ($H3 | $H2 | $JT | $JV | $JL);"
    "$CM* ($H3 | $H2 | $JT | $JV | $JL) $CM* $PR;"
    "$CM* ($ALPlus | $HL) $CM* ($ALPlus | $HL);"
    "$CM* ($ALPlus | $HL) $CM* $IS;"
    "$CM* $OP $CM* ($ALPlus | $HL | $NU);"
    "$CM* ($ALPlus | $HL | $NU) $CM* $CP;"
    "$EmojiMods $EmojiVar? $EmojiForMods;";

static const char* uax14SafeForward =
    "!!safe_forward;"
    "[$CM $OP $QU $CL $CP $B2 $PR $HY $BA $SP $dictionary]+ [^$CM $OP $QU $CL $CP $B2 $PR $HY $BA $dictionary];"
    "$dictionary $dictionary;";

static const char* uax14SafeReverse =
    "!!safe_reverse;"
    "$CM+ [^$CM $BK $CR $LF $NL $ZW $SP];"
    "$CM+ $SP / .;"
    "$SP+ $CM* $OP;"
    "$SP+ $CM* $QU;"
    "$SP+ $CM* ($CL | $CP);"
    "$SP+ $CM* $B2;"
    "$CM* ($HY | $BA) $CM* $HL;"
    "($CM* ($IS | $SY))+ $CM* $NU;"
    "($CL | $CP) $CM* ($NU | $IS | $SY);"
    "$dictionary $dictionary;";

static String mapLineIteratorModeToRules(LineBreakIteratorMode mode, bool isCJK)
{
    StringBuilder rulesBuilder;
    rulesBuilder.append(uax14Prologue);
    rulesBuilder.append(uax14AssignmentsBefore);
    switch (mode) {
    case LineBreakIteratorModeUAX14:
        rulesBuilder.append(isCJK ? uax14AssignmentsCustomDefaultCJK : uax14AssignmentsCustomDefaultNonCJK);
        break;
    case LineBreakIteratorModeUAX14Loose:
        rulesBuilder.append(isCJK ? uax14AssignmentsCustomLooseCJK : uax14AssignmentsCustomLooseNonCJK);
        break;
    case LineBreakIteratorModeUAX14Normal:
        rulesBuilder.append(isCJK ? uax14AssignmentsCustomNormalCJK : uax14AssignmentsCustomNormalNonCJK);
        break;
    case LineBreakIteratorModeUAX14Strict:
        rulesBuilder.append(isCJK ? uax14AssignmentsCustomStrictCJK : uax14AssignmentsCustomStrictNonCJK);
        break;
    }
    rulesBuilder.append(uax14AssignmentsAfter);
    rulesBuilder.append(uax14Forward);
    rulesBuilder.append(uax14Reverse);
    rulesBuilder.append(uax14SafeForward);
    rulesBuilder.append(uax14SafeReverse);
    return rulesBuilder.toString();
}

// Recognize BCP47 compliant primary language values of 'zh', 'ja', 'ko'
// (in any combination of case), optionally followed by subtags. Don't
// recognize 3-letter variants 'chi'/'zho', 'jpn', or 'kor' since BCP47
// requires use of shortest language tag.
bool isCJKLocale(const AtomicString& locale)
{
    size_t length = locale.length();
    if (length < 2)
        return false;
    auto c1 = locale[0];
    auto c2 = locale[1];
    auto c3 = length == 2 ? 0 : locale[2];
    if (!c3 || c3 == '-' || c3 == '_' || c3 == '@') {
        if (c1 == 'z' || c1 == 'Z')
            return c2 == 'h' || c2 == 'H';
        if (c1 == 'j' || c1 == 'J')
            return c2 == 'a' || c2 == 'A';
        if (c1 == 'k' || c1 == 'K')
            return c2 == 'o' || c2 == 'O';
    }
    return false;
}

TextBreakIterator* openLineBreakIterator(const AtomicString& locale, LineBreakIteratorMode mode, bool isCJK)
{
    UBreakIterator* ubrkIter;
    UErrorCode openStatus = U_ZERO_ERROR;
    bool localeIsEmpty = locale.isEmpty();
    if (mode == LineBreakIteratorModeUAX14)
        ubrkIter = ubrk_open(UBRK_LINE, localeIsEmpty ? currentTextBreakLocaleID() : locale.string().utf8().data(), 0, 0, &openStatus);
    else {
        UParseError parseStatus;
        auto rules = mapLineIteratorModeToRules(mode, isCJK);
        ubrkIter = ubrk_openRules(StringView(rules).upconvertedCharacters(), rules.length(), 0, 0, &parseStatus, &openStatus);
    }
    // locale comes from a web page and it can be invalid, leading ICU
    // to fail, in which case we fall back to the default locale.
    if (!localeIsEmpty && U_FAILURE(openStatus)) {
        openStatus = U_ZERO_ERROR;
        ubrkIter = ubrk_open(UBRK_LINE, currentTextBreakLocaleID(), 0, 0, &openStatus);
    }

    if (U_FAILURE(openStatus)) {
        LOG_ERROR("ubrk_open failed with status %d", openStatus);
        return nullptr;
    }

    return reinterpret_cast<TextBreakIterator*>(ubrkIter);
}

void closeLineBreakIterator(TextBreakIterator*& iterator)
{
    UBreakIterator* ubrkIter = reinterpret_cast<UBreakIterator*>(iterator);
    ASSERT(ubrkIter);
    ubrk_close(ubrkIter);
    iterator = nullptr;
}

static TextBreakIterator* nonSharedCharacterBreakIterator;

static inline bool compareAndSwapNonSharedCharacterBreakIterator(TextBreakIterator* expected, TextBreakIterator* newValue)
{
#if ENABLE(COMPARE_AND_SWAP)
    return WTF::weakCompareAndSwap(reinterpret_cast<void**>(&nonSharedCharacterBreakIterator), expected, newValue);
#else
    DEPRECATED_DEFINE_STATIC_LOCAL(std::mutex, nonSharedCharacterBreakIteratorMutex, ());
    std::lock_guard<std::mutex> locker(nonSharedCharacterBreakIteratorMutex);
    if (nonSharedCharacterBreakIterator != expected)
        return false;
    nonSharedCharacterBreakIterator = newValue;
    return true;
#endif
}

NonSharedCharacterBreakIterator::NonSharedCharacterBreakIterator(StringView string)
{
    m_iterator = nonSharedCharacterBreakIterator;

    bool createdIterator = m_iterator && compareAndSwapNonSharedCharacterBreakIterator(m_iterator, 0);
    if (!createdIterator)
        m_iterator = initializeIterator(UBRK_CHARACTER);
    if (!m_iterator)
        return;

    m_iterator = setTextForIterator(*m_iterator, string);
}

NonSharedCharacterBreakIterator::~NonSharedCharacterBreakIterator()
{
    if (!compareAndSwapNonSharedCharacterBreakIterator(0, m_iterator))
        ubrk_close(reinterpret_cast<UBreakIterator*>(m_iterator));
}


// Iterator implemenation.

int textBreakFirst(TextBreakIterator* iterator)
{
    return ubrk_first(reinterpret_cast<UBreakIterator*>(iterator));
}

int textBreakLast(TextBreakIterator* iterator)
{
    return ubrk_last(reinterpret_cast<UBreakIterator*>(iterator));
}

int textBreakNext(TextBreakIterator* iterator)
{
    return ubrk_next(reinterpret_cast<UBreakIterator*>(iterator));
}

int textBreakPrevious(TextBreakIterator* iterator)
{
    return ubrk_previous(reinterpret_cast<UBreakIterator*>(iterator));
}

int textBreakPreceding(TextBreakIterator* iterator, int pos)
{
    return ubrk_preceding(reinterpret_cast<UBreakIterator*>(iterator), pos);
}

int textBreakFollowing(TextBreakIterator* iterator, int pos)
{
    return ubrk_following(reinterpret_cast<UBreakIterator*>(iterator), pos);
}

int textBreakCurrent(TextBreakIterator* iterator)
{
    return ubrk_current(reinterpret_cast<UBreakIterator*>(iterator));
}

bool isTextBreak(TextBreakIterator* iterator, int position)
{
    return ubrk_isBoundary(reinterpret_cast<UBreakIterator*>(iterator), position);
}

bool isWordTextBreak(TextBreakIterator* iterator)
{
    int ruleStatus = ubrk_getRuleStatus(reinterpret_cast<UBreakIterator*>(iterator));
    return ruleStatus != UBRK_WORD_NONE;
}

unsigned numGraphemeClusters(const String& s)
{
    unsigned stringLength = s.length();
    
    if (!stringLength)
        return 0;

    // The only Latin-1 Extended Grapheme Cluster is CR LF
    if (s.is8Bit() && !s.contains('\r'))
        return stringLength;

    NonSharedCharacterBreakIterator it(s);
    if (!it)
        return stringLength;

    unsigned num = 0;
    while (textBreakNext(it) != TextBreakDone)
        ++num;
    return num;
}

unsigned numCharactersInGraphemeClusters(const String& s, unsigned numGraphemeClusters)
{
    unsigned stringLength = s.length();

    if (!stringLength)
        return 0;

    // The only Latin-1 Extended Grapheme Cluster is CR LF
    if (s.is8Bit() && !s.contains('\r'))
        return std::min(stringLength, numGraphemeClusters);

    NonSharedCharacterBreakIterator it(s);
    if (!it)
        return std::min(stringLength, numGraphemeClusters);

    for (unsigned i = 0; i < numGraphemeClusters; ++i) {
        if (textBreakNext(it) == TextBreakDone)
            return stringLength;
    }
    return textBreakCurrent(it);
}

} // namespace WebCore
