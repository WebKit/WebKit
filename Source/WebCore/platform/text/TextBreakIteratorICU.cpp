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
#include <unicode/ubrk.h>
#include <unicode/uloc.h>
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

TextBreakIterator* acquireLineBreakIterator(const LChar* string, int length, const AtomicString& locale, LineBreakIteratorMode mode, bool isCJK)
{
    TextBreakIterator* iterator = LineBreakIteratorPool::sharedPool().take(locale, mode, isCJK);
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

    UBreakIterator* ubrkIter = reinterpret_cast<UBreakIterator*>(iterator);
    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setUText(ubrkIter, uTextLatin1, &setTextStatus);
    if (U_FAILURE(setTextStatus)) {
        LOG_ERROR("ubrk_setUText failed with status %d", setTextStatus);
        return 0;
    }

    utext_close(uTextLatin1);

    return iterator;
}

TextBreakIterator* acquireLineBreakIterator(const UChar* string, int length, const AtomicString& locale, LineBreakIteratorMode mode, bool isCJK)
{
    TextBreakIterator* iterator = LineBreakIteratorPool::sharedPool().take(locale, mode, isCJK);
    if (!iterator)
        return 0;

    UBreakIterator* ubrkIter = reinterpret_cast<UBreakIterator*>(iterator);
    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setText(ubrkIter, string, length, &setTextStatus);
    if (U_FAILURE(setTextStatus)) {
        LOG_ERROR("ubrk_setText failed with status %d", setTextStatus);
        return 0;
    }

    return iterator;
}

void releaseLineBreakIterator(TextBreakIterator* iterator)
{
    ASSERT_ARG(iterator, iterator);

    LineBreakIteratorPool::sharedPool().put(iterator);
}

// Recognize BCP47 compliant primary language values of 'zh', 'ja', 'ko'
// (in any combination of case), optionally followed by subtags. Don't
// recognize 3-letter variants 'chi'/'zho', 'jpn', or 'kor' since BCP47
// requires use of shortest language tag.
template<typename T>
static bool isCJKLocale(const T* s, size_t length)
{
    if (!s || length < 2)
        return false;
    T c1 = s[0];
    T c2 = s[1];
    T c3 = length == 2 ? 0 : s[2];
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

bool isCJKLocale(const AtomicString& locale)
{
    if (locale.isEmpty())
        return false;
    size_t length = locale.length();
    if (locale.is8Bit())
        return isCJKLocale<LChar>(locale.characters8(), length);
    return isCJKLocale<UChar>(locale.characters16(), length);
}

static void mapLineIteratorModeToRules(LineBreakIteratorMode, bool isCJK, String& rules);

TextBreakIterator* openLineBreakIterator(const AtomicString& locale, LineBreakIteratorMode mode, bool isCJK)
{
    UBreakIterator* ubrkIter;
    UErrorCode openStatus = U_ZERO_ERROR;
    bool isLocaleEmpty = locale.isEmpty();
    if ((mode == LineBreakIteratorModeUAX14) && !isCJK)
        ubrkIter = ubrk_open(UBRK_LINE, isLocaleEmpty ? currentTextBreakLocaleID() : locale.string().utf8().data(), 0, 0, &openStatus);
    else {
        UParseError parseStatus;
        String rules;
        mapLineIteratorModeToRules(mode, isCJK, rules);
        ubrkIter = ubrk_openRules(rules.characters(), rules.length(), 0, 0, &parseStatus, &openStatus);
    }
    // Locale comes from a web page and it can be invalid, leading ICU
    // to fail, in which case we fall back to the default locale (with default rules).
    if (!isLocaleEmpty && U_FAILURE(openStatus)) {
        openStatus = U_ZERO_ERROR;
        ubrkIter = ubrk_open(UBRK_LINE, currentTextBreakLocaleID(), 0, 0, &openStatus);
    }

    if (U_FAILURE(openStatus)) {
        LOG_ERROR("ubrk_open failed with status %d", openStatus);
        ASSERT(!ubrkIter);
    }
    return reinterpret_cast<TextBreakIterator*>(ubrkIter);
}

void closeLineBreakIterator(TextBreakIterator*& iterator)
{
    UBreakIterator* ubrkIter = reinterpret_cast<UBreakIterator*>(iterator);
    ASSERT(ubrkIter);
    ubrk_close(ubrkIter);
    iterator = 0;
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
    "$CPcm ($ALcm | $HLcm | $NUcm);";

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
    "$CM* ($ALPlus | $HL | $NU) $CM* $CP;";

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

static void mapLineIteratorModeToRules(LineBreakIteratorMode mode, bool isCJK, String& rules)
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
    rules = rulesBuilder.toString();
}

}
