/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007, 2011, 2012 Apple Inc. All rights reserved.
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

#include "LineBreakIteratorPoolICU.h"
#include <wtf/Atomics.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace std;

namespace WebCore {

static TextBreakIterator* setUpIterator(bool& createdIterator, TextBreakIterator*& iterator,
    UBreakIteratorType type, const UChar* string, int length)
{
    if (!string)
        return 0;

    if (!createdIterator) {
        UErrorCode openStatus = U_ZERO_ERROR;
        iterator = reinterpret_cast<TextBreakIterator*>(ubrk_open(type, currentTextBreakLocaleID(), 0, 0, &openStatus));
        createdIterator = true;
        ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    }
    if (!iterator)
        return 0;

    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setText(reinterpret_cast<UBreakIterator*>(iterator), string, length, &setTextStatus);
    if (U_FAILURE(setTextStatus))
        return 0;

    return iterator;
}

static const int s_UTextCharacterBufferSize = 16;

typedef struct {
    UText uTextStruct;
    UChar uCharBuffer[s_UTextCharacterBufferSize + 1];
} UTextWithBuffer;

static UText emptyUText = UTEXT_INITIALIZER;

static UText* uTextLatin1Clone(UText*, const UText*, UBool, UErrorCode*);
static int64_t uTextLatin1NativeLength(UText*);
static UBool uTextLatin1Access(UText*, int64_t, UBool);
static int32_t uTextLatin1Extract(UText*, int64_t, int64_t, UChar*, int32_t, UErrorCode*);
static int64_t uTextLatin1MapOffsetToNative(const UText*);
static int32_t uTextLatin1MapNativeIndexToUTF16(const UText*, int64_t);
static void uTextLatin1Close(UText*);

static struct UTextFuncs uTextLatin1Funcs = {
    sizeof(UTextFuncs),
    0, 0, 0,
    uTextLatin1Clone,
    uTextLatin1NativeLength,
    uTextLatin1Access,
    uTextLatin1Extract,
    0,
    0,
    uTextLatin1MapOffsetToNative,
    uTextLatin1MapNativeIndexToUTF16,
    uTextLatin1Close,
    0, 0, 0
};

static UText* uTextLatin1Clone(UText* destination, const UText* source, UBool deep, UErrorCode* status)
{
    ASSERT_UNUSED(deep, !deep);

    if (U_FAILURE(*status))
        return 0;

    UText* result = utext_setup(destination, sizeof(UChar) * (s_UTextCharacterBufferSize + 1), status);
    if (U_FAILURE(*status))
        return destination;
    
    result->providerProperties = source->providerProperties;
    
    /* Point at the same position, but with an empty buffer */
    result->chunkNativeStart = source->chunkNativeStart;
    result->chunkNativeLimit = source->chunkNativeStart;
    result->nativeIndexingLimit = static_cast<int32_t>(source->chunkNativeStart);
    result->chunkOffset = 0;
    result->context = source->context;
    result->a = source->a;
    result->pFuncs = &uTextLatin1Funcs;
    result->chunkContents = (UChar*)result->pExtra;
    memset(const_cast<UChar*>(result->chunkContents), 0, sizeof(UChar) * (s_UTextCharacterBufferSize + 1));

    return result;
}

static int64_t uTextLatin1NativeLength(UText* uText)
{
    return uText->a;
}

static UBool uTextLatin1Access(UText* uText, int64_t index, UBool forward)
{
    int64_t length = uText->a;

    if (forward) {
        if (index < uText->chunkNativeLimit && index >= uText->chunkNativeStart) {
            /* Already inside the buffer. Set the new offset. */
            uText->chunkOffset = (int32_t)(index - uText->chunkNativeStart);
            return TRUE;
        }
        if (index >= length && uText->chunkNativeLimit == length) {
            /* Off the end of the buffer, but we can't get it. */
            uText->chunkOffset = uText->chunkLength;
            return FALSE;
        }
    } else {
        if (index <= uText->chunkNativeLimit && index > uText->chunkNativeStart) {
            /* Already inside the buffer. Set the new offset. */
            uText->chunkOffset = (int32_t)(index - uText->chunkNativeStart);
            return TRUE;
        }
        if (!index && !uText->chunkNativeStart) {
            /* Already at the beginning; can't go any farther */
            uText->chunkOffset = 0;
            return FALSE;
        }
    }
    
    if (forward) {
        uText->chunkNativeStart = index;
        uText->chunkNativeLimit = uText->chunkNativeStart + s_UTextCharacterBufferSize;
        if (uText->chunkNativeLimit > length)
            uText->chunkNativeLimit = length;

        uText->chunkOffset = 0;
    } else {
        uText->chunkNativeLimit = index;
        if (uText->chunkNativeLimit > length)
            uText->chunkNativeLimit = length;

        uText->chunkNativeStart = uText->chunkNativeLimit -  s_UTextCharacterBufferSize;
        if (uText->chunkNativeStart < 0)
            uText->chunkNativeStart = 0;

        uText->chunkOffset = uText->chunkLength;
    }
    uText->chunkLength = (int32_t) (uText->chunkNativeLimit - uText->chunkNativeStart);

    StringImpl::copyChars(const_cast<UChar*>(uText->chunkContents), static_cast<const LChar*>(uText->context) + uText->chunkNativeStart, static_cast<unsigned>(uText->chunkLength));

    uText->nativeIndexingLimit = uText->chunkLength;

    return TRUE;
}

static int32_t uTextLatin1Extract(UText* uText, int64_t start, int64_t limit, UChar* dest, int32_t destCapacity, UErrorCode* status)
{
    int64_t length = uText->a;
    if (U_FAILURE(*status))
        return 0;

    if (destCapacity < 0 || (!dest && destCapacity > 0)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (start < 0 || start > limit || (limit - start) > INT32_MAX) {
        *status = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    if (start > length)
        start = length;
    if (limit > length)
        limit = length;

    length = limit - start;
    
    if (!length)
        return 0;

    if (destCapacity > 0 && !dest) {
        int32_t trimmedLength = static_cast<int32_t>(length);
        if (trimmedLength > destCapacity)
            trimmedLength = destCapacity;

        StringImpl::copyChars(dest, static_cast<const LChar*>(uText->context) + start, static_cast<unsigned>(trimmedLength));
    }

    if (length < destCapacity) {
        dest[length] = 0;
        if (*status == U_STRING_NOT_TERMINATED_WARNING)
            *status = U_ZERO_ERROR;
    } else if (length == destCapacity)
        *status = U_STRING_NOT_TERMINATED_WARNING;
    else
        *status = U_BUFFER_OVERFLOW_ERROR;

    return static_cast<int32_t>(length);
}

static int64_t uTextLatin1MapOffsetToNative(const UText* uText)
{
    return uText->chunkNativeStart + uText->chunkOffset;
}

static int32_t uTextLatin1MapNativeIndexToUTF16(const UText* uText, int64_t nativeIndex)
{
    ASSERT_UNUSED(uText, uText->chunkNativeStart >= nativeIndex);
    ASSERT_UNUSED(uText, nativeIndex < uText->chunkNativeLimit);
    return static_cast<int32_t>(nativeIndex);
}

static void uTextLatin1Close(UText* uText)
{
    uText->context = 0;
}

static UText* UTextOpenLatin1(UTextWithBuffer* uTextLatin1, const LChar* string, unsigned length, UErrorCode* errorCode)
{
    UText* result = utext_setup(reinterpret_cast<UText*>(uTextLatin1), sizeof(UChar) * (s_UTextCharacterBufferSize + 1), errorCode);
    
    if (!U_SUCCESS(*errorCode))
        return 0;

    result->context = string;
    result->a = (int64_t)length;
    result->pFuncs = &uTextLatin1Funcs;
    result->chunkContents = (UChar*)result->pExtra;
    memset(const_cast<UChar*>(result->chunkContents), 0, sizeof(UChar) * (s_UTextCharacterBufferSize + 1));
    
    return result;
}

TextBreakIterator* wordBreakIterator(const UChar* string, int length)
{
    static bool createdWordBreakIterator = false;
    static TextBreakIterator* staticWordBreakIterator;
    return setUpIterator(createdWordBreakIterator,
        staticWordBreakIterator, UBRK_WORD, string, length);
}

TextBreakIterator* acquireLineBreakIterator(const LChar* string, int length, const AtomicString& locale)
{
    UBreakIterator* iterator = LineBreakIteratorPool::sharedPool().take(locale);
    if (!iterator)
        return 0;

    UTextWithBuffer uTextLatin1Local;
    uTextLatin1Local.uTextStruct = emptyUText;
    uTextLatin1Local.uTextStruct.extraSize = sizeof(uTextLatin1Local.uCharBuffer);
    uTextLatin1Local.uTextStruct.pExtra = uTextLatin1Local.uCharBuffer;

    UErrorCode uTextOpenStatus = U_ZERO_ERROR;
    UText* uTextLatin1 = UTextOpenLatin1(&uTextLatin1Local, string, length, &uTextOpenStatus);
    if (U_FAILURE(uTextOpenStatus)) {
        LOG_ERROR("UTextOpenLatin1 failed with status %d", uTextOpenStatus);
        return 0;
    }

    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setUText(iterator, uTextLatin1, &setTextStatus);
    if (U_FAILURE(setTextStatus)) {
        LOG_ERROR("ubrk_setUText failed with status %d", setTextStatus);
        return 0;
    }

    utext_close(uTextLatin1);

    return reinterpret_cast<TextBreakIterator*>(iterator);
}

TextBreakIterator* acquireLineBreakIterator(const UChar* string, int length, const AtomicString& locale)
{
    UBreakIterator* iterator = LineBreakIteratorPool::sharedPool().take(locale);
    if (!iterator)
        return 0;

    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setText(iterator, string, length, &setTextStatus);
    if (U_FAILURE(setTextStatus)) {
        LOG_ERROR("ubrk_setText failed with status %d", setTextStatus);
        return 0;
    }

    return reinterpret_cast<TextBreakIterator*>(iterator);
}

void releaseLineBreakIterator(TextBreakIterator* iterator)
{
    ASSERT_ARG(iterator, iterator);

    LineBreakIteratorPool::sharedPool().put(reinterpret_cast<UBreakIterator*>(iterator));
}

static TextBreakIterator* nonSharedCharacterBreakIterator;

static inline bool compareAndSwapNonSharedCharacterBreakIterator(TextBreakIterator* expected, TextBreakIterator* newValue)
{
#if ENABLE(COMPARE_AND_SWAP)
    return weakCompareAndSwap(reinterpret_cast<void**>(&nonSharedCharacterBreakIterator), expected, newValue);
#else
    DEFINE_STATIC_LOCAL(Mutex, nonSharedCharacterBreakIteratorMutex, ());
    MutexLocker locker(nonSharedCharacterBreakIteratorMutex);
    if (nonSharedCharacterBreakIterator != expected)
        return false;
    nonSharedCharacterBreakIterator = newValue;
    return true;
#endif
}

NonSharedCharacterBreakIterator::NonSharedCharacterBreakIterator(const UChar* buffer, int length)
{
    m_iterator = nonSharedCharacterBreakIterator;
    bool createdIterator = m_iterator && compareAndSwapNonSharedCharacterBreakIterator(m_iterator, 0);
    m_iterator = setUpIterator(createdIterator, m_iterator, UBRK_CHARACTER, buffer, length);
}

NonSharedCharacterBreakIterator::~NonSharedCharacterBreakIterator()
{
    if (!compareAndSwapNonSharedCharacterBreakIterator(0, m_iterator))
        ubrk_close(reinterpret_cast<UBreakIterator*>(m_iterator));
}

TextBreakIterator* sentenceBreakIterator(const UChar* string, int length)
{
    static bool createdSentenceBreakIterator = false;
    static TextBreakIterator* staticSentenceBreakIterator;
    return setUpIterator(createdSentenceBreakIterator,
        staticSentenceBreakIterator, UBRK_SENTENCE, string, length);
}

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

static TextBreakIterator* setUpIteratorWithRules(bool& createdIterator, TextBreakIterator*& iterator,
    const char* breakRules, const UChar* string, int length)
{
    if (!string)
        return 0;

    if (!createdIterator) {
        UParseError parseStatus;
        UErrorCode openStatus = U_ZERO_ERROR;
        String rules(breakRules);
        iterator = reinterpret_cast<TextBreakIterator*>(ubrk_openRules(rules.characters(), rules.length(), 0, 0, &parseStatus, &openStatus));
        createdIterator = true;
        ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    }
    if (!iterator)
        return 0;

    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setText(reinterpret_cast<UBreakIterator*>(iterator), string, length, &setTextStatus);
    if (U_FAILURE(setTextStatus))
        return 0;

    return iterator;
}

TextBreakIterator* cursorMovementIterator(const UChar* string, int length)
{
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
        "!!safe_reverse;"
        "!!safe_forward;";
    static bool createdCursorMovementIterator = false;
    static TextBreakIterator* staticCursorMovementIterator;
    return setUpIteratorWithRules(createdCursorMovementIterator, staticCursorMovementIterator, kRules, string, length);
}

}
