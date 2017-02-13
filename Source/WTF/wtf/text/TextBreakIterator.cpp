/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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
#include "TextBreakIteratorInternalICU.h"
#include "UTextProviderLatin1.h"
#include "UTextProviderUTF16.h"
#include <atomic>
#include <mutex>
#include <unicode/ubrk.h>
#include <wtf/text/StringBuilder.h>

// FIXME: This needs a better name
#define ADDITIONAL_EMOJI_SUPPORT (PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100))

namespace WTF {

// Iterator initialization

static UBreakIterator* initializeIterator(UBreakIteratorType type, const char* locale = currentTextBreakLocaleID())
{
    UErrorCode openStatus = U_ZERO_ERROR;
    UBreakIterator* iterator = ubrk_open(type, locale, 0, 0, &openStatus);
    ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    return iterator;
}

#if !PLATFORM(IOS)

static UBreakIterator* initializeIteratorWithRules(const char* breakRules)
{
    UParseError parseStatus;
    UErrorCode openStatus = U_ZERO_ERROR;
    unsigned length = strlen(breakRules);
    auto upconvertedCharacters = StringView(reinterpret_cast<const LChar*>(breakRules), length).upconvertedCharacters();
    UBreakIterator* iterator = ubrk_openRules(upconvertedCharacters, length, 0, 0, &parseStatus, &openStatus);
    ASSERT_WITH_MESSAGE(U_SUCCESS(openStatus), "ICU could not open a break iterator: %s (%d)", u_errorName(openStatus), openStatus);
    return iterator;
}

#endif


// Iterator text setting

static UBreakIterator* setTextForIterator(UBreakIterator& iterator, StringView string)
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
        ubrk_setUText(&iterator, text, &setTextStatus);
        if (U_FAILURE(setTextStatus)) {
            LOG_ERROR("ubrk_setUText failed with status %d", setTextStatus);
            return nullptr;
        }

        utext_close(text);
    } else {
        UErrorCode setTextStatus = U_ZERO_ERROR;
        ubrk_setText(&iterator, string.characters16(), string.length(), &setTextStatus);
        if (U_FAILURE(setTextStatus))
            return nullptr;
    }

    return &iterator;
}

static UBreakIterator* setContextAwareTextForIterator(UBreakIterator& iterator, StringView string, const UChar* priorContext, unsigned priorContextLength)
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
        ubrk_setUText(&iterator, text, &setTextStatus);
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
        ubrk_setUText(&iterator, text, &setTextStatus);
        if (U_FAILURE(setTextStatus)) {
            LOG_ERROR("ubrk_setUText failed with status %d", setTextStatus);
            return nullptr;
        }

        utext_close(text);
    }

    return &iterator;
}


// Static iterators

UBreakIterator* wordBreakIterator(StringView string)
{
    static UBreakIterator* staticWordBreakIterator = initializeIterator(UBRK_WORD);
    if (!staticWordBreakIterator)
        return nullptr;

    return setTextForIterator(*staticWordBreakIterator, string);
}

UBreakIterator* sentenceBreakIterator(StringView string)
{
    static UBreakIterator* staticSentenceBreakIterator = initializeIterator(UBRK_SENTENCE);
    if (!staticSentenceBreakIterator)
        return nullptr;

    return setTextForIterator(*staticSentenceBreakIterator, string);
}

UBreakIterator* cursorMovementIterator(StringView string)
{
#if !PLATFORM(IOS)
    // This rule set is based on character-break iterator rules of ICU 57
    // <http://source.icu-project.org/repos/icu/icu/tags/release-57-1/source/data/brkitr/>.
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
        "$VoiceMarks = [\\uFF9E\\uFF9F];" // Japanese half-width katakana voiced marks
        "$Extend  = [\\p{Grapheme_Cluster_Break = Extend} $VoiceMarks - [\\u0E30 \\u0E32 \\u0E45 \\u0EB0 \\u0EB2]];"
        "$SpacingMark = [[\\p{General_Category = Spacing Mark}] - $Extend];"
        "$L       = [\\p{Grapheme_Cluster_Break = L}];"
        "$V       = [\\p{Grapheme_Cluster_Break = V}];"
        "$T       = [\\p{Grapheme_Cluster_Break = T}];"
        "$LV      = [\\p{Grapheme_Cluster_Break = LV}];"
        "$LVT     = [\\p{Grapheme_Cluster_Break = LVT}];"
        "$Hin0    = [\\u0905-\\u0939];" // Devanagari Letter A,...,Ha
        "$HinV    = \\u094D;" // Devanagari Sign Virama
        "$Hin1    = [\\u0915-\\u0939];" // Devanagari Letter Ka,...,Ha
        "$Ben0    = [\\u0985-\\u09B9];" // Bengali Letter A,...,Ha
        "$BenV    = \\u09CD;" // Bengali Sign Virama
        "$Ben1    = [\\u0995-\\u09B9];" // Bengali Letter Ka,...,Ha
        "$Pan0    = [\\u0A05-\\u0A39];" // Gurmukhi Letter A,...,Ha
        "$PanV    = \\u0A4D;" // Gurmukhi Sign Virama
        "$Pan1    = [\\u0A15-\\u0A39];" // Gurmukhi Letter Ka,...,Ha
        "$Guj0    = [\\u0A85-\\u0AB9];" // Gujarati Letter A,...,Ha
        "$GujV    = \\u0ACD;" // Gujarati Sign Virama
        "$Guj1    = [\\u0A95-\\u0AB9];" // Gujarati Letter Ka,...,Ha
        "$Ori0    = [\\u0B05-\\u0B39];" // Oriya Letter A,...,Ha
        "$OriV    = \\u0B4D;" // Oriya Sign Virama
        "$Ori1    = [\\u0B15-\\u0B39];" // Oriya Letter Ka,...,Ha
        "$Tel0    = [\\u0C05-\\u0C39];" // Telugu Letter A,...,Ha
        "$TelV    = \\u0C4D;" // Telugu Sign Virama
        "$Tel1    = [\\u0C14-\\u0C39];" // Telugu Letter Ka,...,Ha
        "$Kan0    = [\\u0C85-\\u0CB9];" // Kannada Letter A,...,Ha
        "$KanV    = \\u0CCD;" // Kannada Sign Virama
        "$Kan1    = [\\u0C95-\\u0CB9];" // Kannada Letter A,...,Ha
        "$Mal0    = [\\u0D05-\\u0D39];" // Malayalam Letter A,...,Ha
        "$MalV    = \\u0D4D;" // Malayalam Sign Virama
        "$Mal1    = [\\u0D15-\\u0D39];" // Malayalam Letter A,...,Ha
        "$RI      = [\\U0001F1E6-\\U0001F1FF];" // Emoji regional indicators
        "$ZWJ     = \\u200D;" // Zero width joiner
        "$EmojiVar = [\\uFE0F];" // Emoji-style variation selector
#if ADDITIONAL_EMOJI_SUPPORT
        "$EmojiForSeqs = [\\u2640 \\u2642 \\u26F9 \\u2764 \\U0001F308 \\U0001F3C3-\\U0001F3C4 \\U0001F3CA-\\U0001F3CC \\U0001F3F3 \\U0001F441 \\U0001F466-\\U0001F469 \\U0001F46E-\\U0001F46F \\U0001F471 \\U0001F473 \\U0001F477 \\U0001F481-\\U0001F482 \\U0001F486-\\U0001F487 \\U0001F48B \\U0001F575 \\U0001F5E8 \\U0001F645-\\U0001F647 \\U0001F64B \\U0001F64D-\\U0001F64E \\U0001F6A3 \\U0001F6B4-\\U0001F6B6 \\u2695-\\u2696 \\u2708 \\U0001F33E \\U0001F373 \\U0001F393 \\U0001F3A4 \\U0001F3A8 \\U0001F3EB \\U0001F3ED \\U0001F4BB-\\U0001F4BC \\U0001F527 \\U0001F52C \\U0001F680 \\U0001F692 \\U0001F926 \\U0001F937-\\U0001F939 \\U0001F93C-\\U0001F93E];" // Emoji that participate in ZWJ sequences
        "$EmojiForMods = [\\u261D \\u26F9 \\u270A-\\u270D \\U0001F385 \\U0001F3C3-\\U0001F3C4 \\U0001F3CA \\U0001F3CB \\U0001F442-\\U0001F443 \\U0001F446-\\U0001F450 \\U0001F466-\\U0001F478 \\U0001F47C \\U0001F481-\\U0001F483 \\U0001F485-\\U0001F487 \\U0001F4AA \\U0001F575 \\U0001F590 \\U0001F595 \\U0001F596 \\U0001F645-\\U0001F647 \\U0001F64B-\\U0001F64F \\U0001F6A3 \\U0001F6B4-\\U0001F6B6 \\U0001F6C0 \\U0001F918 \\U0001F3C2 \\U0001F3C7 \\U0001F3CC \\U0001F574 \\U0001F57A \\U0001F6CC \\U0001F919-\\U0001F91E \\U0001F926 \\U0001F930 \\U0001F933-\\U0001F939 \\U0001F93C-\\U0001F93E] ;" // Emoji that take Fitzpatrick modifiers
#else
        "$EmojiForSeqs = [\\u2764 \\U0001F466-\\U0001F469 \\U0001F48B];" // Emoji that participate in ZWJ sequences
        "$EmojiForMods = [\\u261D \\u270A-\\u270C \\U0001F385 \\U0001F3C3-\\U0001F3C4 \\U0001F3C7 \\U0001F3CA \\U0001F442-\\U0001F443 \\U0001F446-\\U0001F450 \\U0001F466-\\U0001F469 \\U0001F46E-\\U0001F478 \\U0001F47C \\U0001F481-\\U0001F483 \\U0001F485-\\U0001F487 \\U0001F4AA \\U0001F596 \\U0001F645-\\U0001F647 \\U0001F64B-\\U0001F64F \\U0001F6A3 \\U0001F6B4-\\U0001F6B6 \\U0001F6C0] ;" // Emoji that take Fitzpatrick modifiers
#endif
        "$EmojiMods = [\\U0001F3FB-\\U0001F3FF];" // Fitzpatrick modifiers
        "!!chain;"
#if ADDITIONAL_EMOJI_SUPPORT
        "!!RINoChain;"
#endif
        "!!forward;"
        "$CR $LF;"
        "$L ($L | $V | $LV | $LVT);"
        "($LV | $V) ($V | $T);"
        "($LVT | $T) $T;"
#if ADDITIONAL_EMOJI_SUPPORT
        "$RI $RI $Extend* / $RI;"
        "$RI $RI $Extend*;"
        "[^$Control $CR $LF] $Extend;"
        "[^$Control $CR $LF] $SpacingMark;"
#else
        "[^$Control $CR $LF] $Extend;"
        "[^$Control $CR $LF] $SpacingMark;"
        "$RI $RI / $RI;"
        "$RI $RI;"
#endif
        "$Hin0 $HinV $Hin1;" // Devanagari Virama (forward)
        "$Ben0 $BenV $Ben1;" // Bengali Virama (forward)
        "$Pan0 $PanV $Pan1;" // Gurmukhi Virama (forward)
        "$Guj0 $GujV $Guj1;" // Gujarati Virama (forward)
        "$Ori0 $OriV $Ori1;" // Oriya Virama (forward)
        "$Tel0 $TelV $Tel1;" // Telugu Virama (forward)
        "$Kan0 $KanV $Kan1;" // Kannada Virama (forward)
        "$Mal0 $MalV $Mal1;" // Malayalam Virama (forward)
        "$ZWJ $EmojiForSeqs;" // Don't break in emoji ZWJ sequences
        "$EmojiForMods $EmojiVar? $EmojiMods;" // Don't break between relevant emoji (possibly with variation selector) and Fitzpatrick modifier
        "!!reverse;"
        "$LF $CR;"
        "($L | $V | $LV | $LVT) $L;"
        "($V | $T) ($LV | $V);"
        "$T ($LVT | $T);"
#if ADDITIONAL_EMOJI_SUPPORT
        "$Extend* $RI $RI / $Extend* $RI $RI;"
        "$Extend* $RI $RI;"
        "$Extend      [^$Control $CR $LF];"
        "$SpacingMark [^$Control $CR $LF];"
#else
        "$Extend      [^$Control $CR $LF];"
        "$SpacingMark [^$Control $CR $LF];"
        "$RI $RI / $RI $RI;"
        "$RI $RI;"
#endif
        "$Hin1 $HinV $Hin0;" // Devanagari Virama (backward)
        "$Ben1 $BenV $Ben0;" // Bengali Virama (backward)
        "$Pan1 $PanV $Pan0;" // Gurmukhi Virama (backward)
        "$Guj1 $GujV $Guj0;" // Gujarati Virama (backward)
        "$Ori1 $OriV $Ori0;" // Gujarati Virama (backward)
        "$Tel1 $TelV $Tel0;" // Telugu Virama (backward)
        "$Kan1 $KanV $Kan0;" // Kannada Virama (backward)
        "$Mal1 $MalV $Mal0;" // Malayalam Virama (backward)
        "$EmojiForSeqs $ZWJ;" // Don't break in emoji ZWJ sequences
        "$EmojiMods $EmojiVar? $EmojiForMods;" // Don't break between relevant emoji (possibly with variation selector) and Fitzpatrick modifier
#if ADDITIONAL_EMOJI_SUPPORT
        "!!safe_reverse;"
        "$RI $RI+;"
        "[$EmojiVar $EmojiMods]+ $EmojiForMods;"
        "!!safe_forward;"
        "$RI $RI+;"
        "$EmojiForMods [$EmojiVar $EmojiMods]+;";
#else
        "[$EmojiVar $EmojiMods]+ $EmojiForMods;"
        "$EmojiForMods [$EmojiVar $EmojiMods]+;"
        "!!safe_reverse;"
        "!!safe_forward;";
#endif
    static UBreakIterator* staticCursorMovementIterator = initializeIteratorWithRules(kRules);
#else // PLATFORM(IOS)
    // Use the special Thai character break iterator for all locales
    static UBreakIterator* staticCursorMovementIterator = initializeIterator(UBRK_CHARACTER, "th");
#endif // !PLATFORM(IOS)

    if (!staticCursorMovementIterator)
        return nullptr;

    return setTextForIterator(*staticCursorMovementIterator, string);
}

UBreakIterator* acquireLineBreakIterator(StringView string, const AtomicString& locale, const UChar* priorContext, unsigned priorContextLength, LineBreakIteratorMode mode)
{
    UBreakIterator* iterator = LineBreakIteratorPool::sharedPool().take(locale, mode);
    if (!iterator)
        return nullptr;

    return setContextAwareTextForIterator(*iterator, string, priorContext, priorContextLength);
}

void releaseLineBreakIterator(UBreakIterator* iterator)
{
    ASSERT_ARG(iterator, iterator);

    LineBreakIteratorPool::sharedPool().put(iterator);
}

UBreakIterator* openLineBreakIterator(const AtomicString& locale)
{
    bool localeIsEmpty = locale.isEmpty();
    UErrorCode openStatus = U_ZERO_ERROR;
    UBreakIterator* ubrkIter = ubrk_open(UBRK_LINE, localeIsEmpty ? currentTextBreakLocaleID() : locale.string().utf8().data(), 0, 0, &openStatus);
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

    return ubrkIter;
}

void closeLineBreakIterator(UBreakIterator*& iterator)
{
    UBreakIterator* ubrkIter = iterator;
    ASSERT(ubrkIter);
    ubrk_close(ubrkIter);
    iterator = nullptr;
}

static std::atomic<UBreakIterator*> nonSharedCharacterBreakIterator = ATOMIC_VAR_INIT(nullptr);

static inline UBreakIterator* getNonSharedCharacterBreakIterator()
{
    if (auto *res = nonSharedCharacterBreakIterator.exchange(nullptr, std::memory_order_acquire))
        return res;
    return initializeIterator(UBRK_CHARACTER);
}

static inline void cacheNonSharedCharacterBreakIterator(UBreakIterator* cacheMe)
{
    if (auto *old = nonSharedCharacterBreakIterator.exchange(cacheMe, std::memory_order_release))
        ubrk_close(old);
}

NonSharedCharacterBreakIterator::NonSharedCharacterBreakIterator(StringView string)
{
    if ((m_iterator = getNonSharedCharacterBreakIterator()))
        m_iterator = setTextForIterator(*m_iterator, string);
}

NonSharedCharacterBreakIterator::~NonSharedCharacterBreakIterator()
{
    if (m_iterator)
        cacheNonSharedCharacterBreakIterator(m_iterator);
}

NonSharedCharacterBreakIterator::NonSharedCharacterBreakIterator(NonSharedCharacterBreakIterator&& other)
    : m_iterator(nullptr)
{
    std::swap(m_iterator, other.m_iterator);
}

// Iterator implemenation.

bool isWordTextBreak(UBreakIterator* iterator)
{
    int ruleStatus = ubrk_getRuleStatus(iterator);
    return ruleStatus != UBRK_WORD_NONE;
}

unsigned numGraphemeClusters(StringView string)
{
    unsigned stringLength = string.length();
    
    if (!stringLength)
        return 0;

    // The only Latin-1 Extended Grapheme Cluster is CRLF.
    if (string.is8Bit()) {
        auto* characters = string.characters8();
        unsigned numCRLF = 0;
        for (unsigned i = 1; i < stringLength; ++i)
            numCRLF += characters[i - 1] == '\r' && characters[i] == '\n';
        return stringLength - numCRLF;
    }

    NonSharedCharacterBreakIterator iterator { string };
    if (!iterator) {
        ASSERT_NOT_REACHED();
        return stringLength;
    }

    unsigned numGraphemeClusters = 0;
    while (ubrk_next(iterator) != UBRK_DONE)
        ++numGraphemeClusters;
    return numGraphemeClusters;
}

unsigned numCharactersInGraphemeClusters(StringView string, unsigned numGraphemeClusters)
{
    unsigned stringLength = string.length();

    if (stringLength <= numGraphemeClusters)
        return stringLength;

    // The only Latin-1 Extended Grapheme Cluster is CRLF.
    if (string.is8Bit()) {
        auto* characters = string.characters8();
        unsigned i, j;
        for (i = 0, j = 0; i < numGraphemeClusters && j + 1 < stringLength; ++i, ++j)
            j += characters[j] == '\r' && characters[j + 1] == '\n';
        return j + (i < numGraphemeClusters);
    }

    NonSharedCharacterBreakIterator iterator { string };
    if (!iterator) {
        ASSERT_NOT_REACHED();
        return stringLength;
    }

    for (unsigned i = 0; i < numGraphemeClusters; ++i) {
        if (ubrk_next(iterator) == UBRK_DONE)
            return stringLength;
    }
    return ubrk_current(iterator);
}

} // namespace WTF
