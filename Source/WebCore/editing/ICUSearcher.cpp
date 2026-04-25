/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ICUSearcher.h"

#include "FontCascade.h"
#include "TextBoundaries.h"
#include <limits>
#include <wtf/Compiler.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/text/TextBreakIteratorInternalICU.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

static bool searcherInUse;

static UStringSearch* createSearcher()
{
    // Provide a non-empty pattern and non-empty text so usearch_open will not fail,
    // but it doesn't matter exactly what it is, since we don't perform any searches
    // without setting both the pattern and the text.
    UErrorCode status = U_ZERO_ERROR;
    auto searchCollatorName = makeString(unsafeSpan(currentSearchLocaleID()), "@collation=search"_s);
    SUPPRESS_FORWARD_DECL_ARG UStringSearch* searcher = usearch_open(&newlineCharacter, 1, &newlineCharacter, 1, searchCollatorName.utf8().data(), 0, &status);
    ASSERT(U_SUCCESS(status) || status == U_USING_FALLBACK_WARNING || status == U_USING_DEFAULT_WARNING);
    return searcher;
}

static UStringSearch* globalSearcher()
{
    SUPPRESS_FORWARD_DECL_ARG static UStringSearch* searcher = createSearcher();
    return searcher;
}

ICUSearcher::ICUSearcher(const String& foldedTarget, FindOptions& options)
{
    lock();
    // Characters in the separator category never really occur at the beginning of a word,
    // so if the target begins with such a character, we just ignore the AtWordStart option.
    if (options.contains(FindOption::AtWordStarts) && foldedTarget.length()) {
        char32_t targetFirstCharacter;
        U16_GET(foldedTarget, 0, 0u, foldedTarget.length(), targetFirstCharacter);
        if (isSeparator(targetFirstCharacter))
            options.remove(FindOption::AtWordStarts);
    }

    UCollationStrength strength = options.contains(FindOption::CaseInsensitive) ? UCOL_SECONDARY : UCOL_TERTIARY;
    USearchAttributeValue comparator = options.contains(FindOption::CaseInsensitive)
        ? USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD
        : USEARCH_STANDARD_ELEMENT_COMPARISON;

    setCollationStrength(strength);
    setAttribute(USEARCH_ELEMENT_COMPARISON, comparator);
}

ICUSearcher::~ICUSearcher()
{
    reset();
    unlock();
}

// Grab the single global searcher.
// If we ever have a reason to do more than once search buffer at once, we'll have
// to move to multiple searchers.
void ICUSearcher::lock()
{
    RELEASE_ASSERT(!searcherInUse);
    searcherInUse = true;
}

void ICUSearcher::unlock()
{
    RELEASE_ASSERT(searcherInUse);
    searcherInUse = false;
}

void ICUSearcher::reset()
{
    // Leave the static object pointing to a valid string.
    UErrorCode status = U_ZERO_ERROR;
    SUPPRESS_FORWARD_DECL_ARG usearch_setPattern(globalSearcher(), &newlineCharacter, 1, &status);
    ASSERT(U_SUCCESS(status));
    SUPPRESS_FORWARD_DECL_ARG usearch_setText(globalSearcher(), &newlineCharacter, 1, &status);
    ASSERT(U_SUCCESS(status));
}

UStringSearch* ICUSearcher::searcher()
{
    return globalSearcher();
}

void ICUSearcher::setCollationStrength(UCollationStrength strength)
{
    SUPPRESS_FORWARD_DECL_ARG auto* s = searcher();
    SUPPRESS_FORWARD_DECL_ARG auto* collator = usearch_getCollator(s);
    if (ucol_getStrength(collator) != strength) {
        ucol_setStrength(collator, strength);
        SUPPRESS_FORWARD_DECL_ARG usearch_reset(s);
    }
}

void ICUSearcher::setAttribute(USearchAttribute attribute, USearchAttributeValue value)
{
    UErrorCode status = U_ZERO_ERROR;
    SUPPRESS_FORWARD_DECL_ARG usearch_setAttribute(searcher(), attribute, value, &status);
    ASSERT(U_SUCCESS(status));
}

void ICUSearcher::setPattern(std::span<const char16_t> pattern)
{
    UErrorCode status = U_ZERO_ERROR;
    SUPPRESS_FORWARD_DECL_ARG usearch_setPattern(searcher(), pattern.data(), static_cast<int32_t>(pattern.size()), &status);
    ASSERT(U_SUCCESS(status));
}

void ICUSearcher::setText(std::span<const char16_t> text)
{
    UErrorCode status = U_ZERO_ERROR;
    SUPPRESS_FORWARD_DECL_ARG usearch_setText(searcher(), text.data(), static_cast<int32_t>(text.size()), &status);
    ASSERT(U_SUCCESS(status));
}

void ICUSearcher::setOffset(size_t offset)
{
    UErrorCode status = U_ZERO_ERROR;
    SUPPRESS_FORWARD_DECL_ARG usearch_setOffset(searcher(), static_cast<int32_t>(offset), &status);
    ASSERT(U_SUCCESS(status));
}

std::optional<size_t> ICUSearcher::next()
{
    UErrorCode status = U_ZERO_ERROR;
    SUPPRESS_FORWARD_DECL_ARG int32_t result = usearch_next(searcher(), &status);
    ASSERT(U_SUCCESS(status));
    if (result == USEARCH_DONE)
        return std::nullopt;
    return static_cast<size_t>(result);
}

#if !PLATFORM(PLAYSTATION)
std::optional<size_t> ICUSearcher::previous()
{
    UErrorCode status = U_ZERO_ERROR;
    SUPPRESS_FORWARD_DECL_ARG int32_t result = usearch_previous(searcher(), &status);
    ASSERT(U_SUCCESS(status));
    if (result == USEARCH_DONE)
        return std::nullopt;
    return static_cast<size_t>(result);
}
#endif

size_t ICUSearcher::matchedLength()
{
    SUPPRESS_FORWARD_DECL_ARG int32_t result = usearch_getMatchedLength(searcher());
    return static_cast<size_t>(result);
}

static bool isKanaLetter(char16_t character);
static bool isSmallKanaLetter(char16_t character);
enum class VoicedSoundMarkType { NoVoicedSoundMark, VoicedSoundMark, SemiVoicedSoundMark };
static VoicedSoundMarkType composedVoicedSoundMark(char16_t character);
static bool isCombiningVoicedSoundMark(char16_t character);
static bool isNonLatin1Separator(char32_t character);

bool isBadMatch(std::span<const char16_t> match, std::span<const char16_t> normalizedTarget, Vector<char16_t>& scratchBuffer)
{
    // Normalize into a match buffer. We reuse a single buffer rather than
    // creating a new one each time.
    normalizeCharacters(match.data(), match.size(), scratchBuffer);

    auto a = normalizedTarget;
    auto b = scratchBuffer.span();

    while (true) {
        // Skip runs of non-kana-letter characters. This is necessary so we can
        // correctly handle strings where the target and match have different-length
        // runs of characters that match, while still double checking the correctness
        // of matches of kana letters with other kana letters.
        skipUntil<isKanaLetter>(a);
        skipUntil<isKanaLetter>(b);

        // If we reached the end of either the target or the match, we should have
        // reached the end of both; both should have the same number of kana letters.
        if (a.empty() || b.empty()) {
            ASSERT(a.empty());
            ASSERT(b.empty());
            return false;
        }

        // Check for differences in the kana letter character itself.
        if (isSmallKanaLetter(a.front()) != isSmallKanaLetter(b.front()))
            return true;
        if (composedVoicedSoundMark(a.front()) != composedVoicedSoundMark(b.front()))
            return true;
        skip(a, 1);
        skip(b, 1);

        // Check for differences in combining voiced sound marks found after the letter.
        while (1) {
            if (!(!a.empty() && isCombiningVoicedSoundMark(a.front()))) {
                if (!b.empty() && isCombiningVoicedSoundMark(b.front()))
                    return true;
                break;
            }
            if (!(!b.empty() && isCombiningVoicedSoundMark(b.front())))
                return true;
            if (a.front() != b.front())
                return true;
            skip(a, 1);
            skip(b, 1);
        }
    }
}

// Dictionary-based word break algorithms produce incorrect segmentation when the context-requiring
// characters (e.g., Thai, Lao, Khmer) are surrounded by very long runs of unrelated characters. This
// function limits the context surrounding context-sensitive words, which addresses this issue.
static std::pair<std::span<const char16_t>, size_t> extractSubspanIncludingContextNeededForDictionaryBasedWordBreak(std::span<const char16_t> buffer, size_t position)
{
    RELEASE_ASSERT(buffer.size() <= static_cast<size_t>(std::numeric_limits<int>::max()));
    RELEASE_ASSERT(position <= buffer.size());
    int size = static_cast<int>(buffer.size());

    {
        char32_t character;
        int offset = static_cast<int>(position);
        U16_GET(buffer, 0, offset, size, character);
        if (!requiresContextForWordBoundary(character))
            return { buffer, 0 };
    }

    size_t contextStart = position;
    {
        int index = static_cast<int>(position);
        while (index > 0) {
            char32_t character;
            U16_PREV(buffer, 0, index, character);
            if (!requiresContextForWordBoundary(character))
                break;
            contextStart = static_cast<size_t>(index);
        }
    }

    size_t contextEnd = position;
    {
        int index = static_cast<int>(position);
        while (index < size) {
            char32_t character;
            U16_NEXT(buffer, index, size, character);
            if (!requiresContextForWordBoundary(character))
                break;
            contextEnd = static_cast<size_t>(index);
        }
    }

    if (contextStart > 0) {
        int index = static_cast<int>(contextStart);
        U16_BACK_1(buffer, 0, index);
        contextStart = static_cast<size_t>(index);
    }
    if (contextEnd < buffer.size()) {
        int index = static_cast<int>(contextEnd);
        U16_FWD_1(buffer, index, size);
        contextEnd = static_cast<size_t>(index);
    }

    return { buffer.subspan(contextStart, contextEnd - contextStart), contextStart };
}

bool isWordStartMatch(std::span<const char16_t> buffer, size_t start, size_t length, FindOptions options)
{
    ASSERT(options.contains(FindOption::AtWordStarts));

    if (!start)
        return true;

    int size = buffer.size();
    int offset = start;
    char32_t firstCharacter;
    U16_GET(buffer, 0, offset, size, firstCharacter);

    if (options.contains(FindOption::TreatMedialCapitalAsWordStart)) {
        char32_t previousCharacter = StringView(buffer).codePointBefore(offset);

        if (isSeparator(firstCharacter)) {
            // The start of a separator run is a word start (".org" in "webkit.org").
            if (!isSeparator(previousCharacter))
                return true;
        } else if (isASCIIUpper(firstCharacter)) {
            // The start of an uppercase run is a word start ("Kit" in "WebKit").
            if (!isASCIIUpper(previousCharacter))
                return true;
            // The last character of an uppercase run followed by a non-separator, non-digit
            // is a word start ("Request" in "XMLHTTPRequest").
            offset = start;
            U16_FWD_1(buffer, offset, size);
            char32_t nextCharacter = 0;
            if (offset < size)
                U16_GET(buffer, 0, offset, size, nextCharacter);
            if (!isASCIIUpper(nextCharacter) && !isASCIIDigit(nextCharacter) && !isSeparator(nextCharacter))
                return true;
        } else if (isASCIIDigit(firstCharacter)) {
            // The start of a digit run is a word start ("2" in "WebKit2").
            if (!isASCIIDigit(previousCharacter))
                return true;
        } else if (isSeparator(previousCharacter) || isASCIIDigit(previousCharacter)) {
            // The start of a non-separator, non-uppercase, non-digit run is a word start,
            // except after an uppercase. ("org" in "webkit.org", but not "ore" in "WebCore").
            return true;
        }
    }

    // Chinese and Japanese lack word boundary marks, and there is no clear agreement on what constitutes
    // a word, so treat the position before any CJK character as a word start.
    if (FontCascade::isCJKIdeographOrSymbol(firstCharacter))
        return true;

    auto [contextBuffer, contextOffset] = extractSubspanIncludingContextNeededForDictionaryBasedWordBreak(buffer, start);
    size_t adjustedStart = start - contextOffset;

    size_t wordBreakSearchStart = adjustedStart + length;
    while (wordBreakSearchStart > adjustedStart)
        wordBreakSearchStart = findNextWordFromIndex(contextBuffer, wordBreakSearchStart, false /* backwards */);
    return wordBreakSearchStart == adjustedStart;
}

bool isWordEndMatch(std::span<const char16_t> buffer, size_t start, size_t length, FindOptions options)
{
    ASSERT(length);
    ASSERT(options.contains(FindOption::AtWordEnds));
    UNUSED_PARAM(options);

    auto [contextBuffer, contextOffset] = extractSubspanIncludingContextNeededForDictionaryBasedWordBreak(buffer, start);
    size_t adjustedStart = start - contextOffset;

    // Start searching at the end of matched search, so that multiple word matches succeed.
    int endWord;
    findEndWordBoundary(contextBuffer, adjustedStart + length - 1, &endWord);
    return static_cast<size_t>(endWord) == adjustedStart + length;
}

void normalizeCharacters(const char16_t* characters, unsigned length, Vector<char16_t>& buffer)
{
    UErrorCode status = U_ZERO_ERROR;
    auto* normalizer = unorm2_getNFCInstance(&status);
    ASSERT(U_SUCCESS(status));

    buffer.reserveCapacity(length);

    status = callBufferProducingFunction(unorm2_normalize, normalizer, characters, length, buffer);
    ASSERT(U_SUCCESS(status));
}

char16_t NODELETE foldQuoteMarkAndReplaceNoBreakSpace(char16_t c)
{
    switch (c) {
    case hebrewPunctuationGershayim:
    case leftDoubleQuotationMark:
    case leftLowDoubleQuotationMark:
    case rightDoubleQuotationMark:
    case leftPointingDoubleAngleQuotationMark:
    case rightPointingDoubleAngleQuotationMark:
    case doubleHighReversed9QuotationMark:
    case doubleLowReversed9QuotationMark:
    case reversedDoublePrimeQuotationMark:
    case doublePrimeQuotationMark:
    case lowDoublePrimeQuotationMark:
    case fullwidthQuotationMark:
        return '"';
    case hebrewPunctuationGeresh:
    case leftSingleQuotationMark:
    case leftLowSingleQuotationMark:
    case rightSingleQuotationMark:
    case singleLow9QuotationMark:
    case singleLeftPointingAngleQuotationMark:
    case singleRightPointingAngleQuotationMark:
    case leftCornerBracket:
    case rightCornerBracket:
    case leftWhiteCornerBracket:
    case rightWhiteCornerBracket:
    case presentationFormForVerticalLeftCornerBracket:
    case presentationFormForVerticalRightCornerBracket:
    case presentationFormForVerticalLeftWhiteCornerBracket:
    case presentationFormForVerticalRightWhiteCornerBracket:
    case fullwidthApostrophe:
    case halfwidthLeftCornerBracket:
    case halfwidthRightCornerBracket:
        return '\'';
    case noBreakSpace:
        return ' ';
    default:
        return c;
    }
}

bool isSeparator(char32_t character)
{
    static constexpr std::array<bool, 256> latin1SeparatorTable {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // space ! " # $ % & ' ( ) * + , - . /
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, //                         : ; < = > ?
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   @
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //                         [ \ ] ^ _
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   `
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, //                           { | } ~
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0
    };

    if (isLatin1(character))
        return latin1SeparatorTable[character];

    return isNonLatin1Separator(character);
}

bool NODELETE containsKanaLetters(const String& pattern)
{
    if (pattern.is8Bit())
        return false;
    for (auto character : pattern.span16()) {
        if (isKanaLetter(character))
            return true;
    }
    return false;
}

// FIXME: We'd like to tailor the searcher to fold quote marks for us instead
// of doing it in a separate replacement pass here, but ICU doesn't offer a way
// to add tailoring on top of the locale-specific tailoring as of this writing.
String foldQuoteMarks(const String& stringToFold)
{
    String result = makeStringByReplacingAll(stringToFold, hebrewPunctuationGeresh, '\'');
    result = makeStringByReplacingAll(result, hebrewPunctuationGershayim, '"');
    result = makeStringByReplacingAll(result, leftDoubleQuotationMark, '"');
    result = makeStringByReplacingAll(result, leftLowDoubleQuotationMark, '"');
    result = makeStringByReplacingAll(result, leftSingleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, leftLowSingleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, rightDoubleQuotationMark, '"');
    result = makeStringByReplacingAll(result, singleLow9QuotationMark, '\'');
    result = makeStringByReplacingAll(result, singleLeftPointingAngleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, singleRightPointingAngleQuotationMark, '\'');
    result = makeStringByReplacingAll(result, leftCornerBracket, '\'');
    result = makeStringByReplacingAll(result, rightCornerBracket, '\'');
    result = makeStringByReplacingAll(result, leftWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, rightWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalLeftCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalRightCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalLeftWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, presentationFormForVerticalRightWhiteCornerBracket, '\'');
    result = makeStringByReplacingAll(result, fullwidthApostrophe, '\'');
    result = makeStringByReplacingAll(result, halfwidthLeftCornerBracket, '\'');
    result = makeStringByReplacingAll(result, halfwidthRightCornerBracket, '\'');
    result = makeStringByReplacingAll(result, leftPointingDoubleAngleQuotationMark, '"');
    result = makeStringByReplacingAll(result, rightPointingDoubleAngleQuotationMark, '"');
    result = makeStringByReplacingAll(result, doubleHighReversed9QuotationMark, '"');
    result = makeStringByReplacingAll(result, doubleLowReversed9QuotationMark, '"');
    result = makeStringByReplacingAll(result, reversedDoublePrimeQuotationMark, '"');
    result = makeStringByReplacingAll(result, doublePrimeQuotationMark, '"');
    result = makeStringByReplacingAll(result, lowDoublePrimeQuotationMark, '"');
    result = makeStringByReplacingAll(result, fullwidthQuotationMark, '"');
    return makeStringByReplacingAll(result, rightSingleQuotationMark, '\'');
}

// ICU's search ignores the distinction between small kana letters and ones
// that are not small, and also characters that differ only in the voicing
// marks when considering only primary collation strength differences.
// This is not helpful for end users, since these differences make words
// distinct, so for our purposes we need these to be considered.
// The Unicode folks do not think the collation algorithm should be
// changed. To work around this, we would like to tailor the ICU searcher,
// but we can't get that to work yet. So instead, we check for cases where
// these differences occur, and skip those matches.

// We refer to the above technique as the "kana workaround". The next few
// functions are helper functinos for the kana workaround.
static bool NODELETE isKanaLetter(char16_t character)
{
    // Hiragana letters.
    if (character >= 0x3041 && character <= 0x3096)
        return true;

    // Katakana letters.
    if (character >= 0x30A1 && character <= 0x30FA)
        return true;
    if (character >= 0x31F0 && character <= 0x31FF)
        return true;

    // Halfwidth katakana letters.
    if (character >= 0xFF66 && character <= 0xFF9D && character != 0xFF70)
        return true;

    return false;
}

static bool NODELETE isSmallKanaLetter(char16_t character)
{
    ASSERT(isKanaLetter(character));

    switch (character) {
    case 0x3041: // HIRAGANA LETTER SMALL A
    case 0x3043: // HIRAGANA LETTER SMALL I
    case 0x3045: // HIRAGANA LETTER SMALL U
    case 0x3047: // HIRAGANA LETTER SMALL E
    case 0x3049: // HIRAGANA LETTER SMALL O
    case 0x3063: // HIRAGANA LETTER SMALL TU
    case 0x3083: // HIRAGANA LETTER SMALL YA
    case 0x3085: // HIRAGANA LETTER SMALL YU
    case 0x3087: // HIRAGANA LETTER SMALL YO
    case 0x308E: // HIRAGANA LETTER SMALL WA
    case 0x3095: // HIRAGANA LETTER SMALL KA
    case 0x3096: // HIRAGANA LETTER SMALL KE
    case 0x30A1: // KATAKANA LETTER SMALL A
    case 0x30A3: // KATAKANA LETTER SMALL I
    case 0x30A5: // KATAKANA LETTER SMALL U
    case 0x30A7: // KATAKANA LETTER SMALL E
    case 0x30A9: // KATAKANA LETTER SMALL O
    case 0x30C3: // KATAKANA LETTER SMALL TU
    case 0x30E3: // KATAKANA LETTER SMALL YA
    case 0x30E5: // KATAKANA LETTER SMALL YU
    case 0x30E7: // KATAKANA LETTER SMALL YO
    case 0x30EE: // KATAKANA LETTER SMALL WA
    case 0x30F5: // KATAKANA LETTER SMALL KA
    case 0x30F6: // KATAKANA LETTER SMALL KE
    case 0x31F0: // KATAKANA LETTER SMALL KU
    case 0x31F1: // KATAKANA LETTER SMALL SI
    case 0x31F2: // KATAKANA LETTER SMALL SU
    case 0x31F3: // KATAKANA LETTER SMALL TO
    case 0x31F4: // KATAKANA LETTER SMALL NU
    case 0x31F5: // KATAKANA LETTER SMALL HA
    case 0x31F6: // KATAKANA LETTER SMALL HI
    case 0x31F7: // KATAKANA LETTER SMALL HU
    case 0x31F8: // KATAKANA LETTER SMALL HE
    case 0x31F9: // KATAKANA LETTER SMALL HO
    case 0x31FA: // KATAKANA LETTER SMALL MU
    case 0x31FB: // KATAKANA LETTER SMALL RA
    case 0x31FC: // KATAKANA LETTER SMALL RI
    case 0x31FD: // KATAKANA LETTER SMALL RU
    case 0x31FE: // KATAKANA LETTER SMALL RE
    case 0x31FF: // KATAKANA LETTER SMALL RO
    case 0xFF67: // HALFWIDTH KATAKANA LETTER SMALL A
    case 0xFF68: // HALFWIDTH KATAKANA LETTER SMALL I
    case 0xFF69: // HALFWIDTH KATAKANA LETTER SMALL U
    case 0xFF6A: // HALFWIDTH KATAKANA LETTER SMALL E
    case 0xFF6B: // HALFWIDTH KATAKANA LETTER SMALL O
    case 0xFF6C: // HALFWIDTH KATAKANA LETTER SMALL YA
    case 0xFF6D: // HALFWIDTH KATAKANA LETTER SMALL YU
    case 0xFF6E: // HALFWIDTH KATAKANA LETTER SMALL YO
    case 0xFF6F: // HALFWIDTH KATAKANA LETTER SMALL TU
        return true;
    }
    return false;
}

static VoicedSoundMarkType NODELETE composedVoicedSoundMark(char16_t character)
{
    ASSERT(isKanaLetter(character));

    switch (character) {
    case 0x304C: // HIRAGANA LETTER GA
    case 0x304E: // HIRAGANA LETTER GI
    case 0x3050: // HIRAGANA LETTER GU
    case 0x3052: // HIRAGANA LETTER GE
    case 0x3054: // HIRAGANA LETTER GO
    case 0x3056: // HIRAGANA LETTER ZA
    case 0x3058: // HIRAGANA LETTER ZI
    case 0x305A: // HIRAGANA LETTER ZU
    case 0x305C: // HIRAGANA LETTER ZE
    case 0x305E: // HIRAGANA LETTER ZO
    case 0x3060: // HIRAGANA LETTER DA
    case 0x3062: // HIRAGANA LETTER DI
    case 0x3065: // HIRAGANA LETTER DU
    case 0x3067: // HIRAGANA LETTER DE
    case 0x3069: // HIRAGANA LETTER DO
    case 0x3070: // HIRAGANA LETTER BA
    case 0x3073: // HIRAGANA LETTER BI
    case 0x3076: // HIRAGANA LETTER BU
    case 0x3079: // HIRAGANA LETTER BE
    case 0x307C: // HIRAGANA LETTER BO
    case 0x3094: // HIRAGANA LETTER VU
    case 0x30AC: // KATAKANA LETTER GA
    case 0x30AE: // KATAKANA LETTER GI
    case 0x30B0: // KATAKANA LETTER GU
    case 0x30B2: // KATAKANA LETTER GE
    case 0x30B4: // KATAKANA LETTER GO
    case 0x30B6: // KATAKANA LETTER ZA
    case 0x30B8: // KATAKANA LETTER ZI
    case 0x30BA: // KATAKANA LETTER ZU
    case 0x30BC: // KATAKANA LETTER ZE
    case 0x30BE: // KATAKANA LETTER ZO
    case 0x30C0: // KATAKANA LETTER DA
    case 0x30C2: // KATAKANA LETTER DI
    case 0x30C5: // KATAKANA LETTER DU
    case 0x30C7: // KATAKANA LETTER DE
    case 0x30C9: // KATAKANA LETTER DO
    case 0x30D0: // KATAKANA LETTER BA
    case 0x30D3: // KATAKANA LETTER BI
    case 0x30D6: // KATAKANA LETTER BU
    case 0x30D9: // KATAKANA LETTER BE
    case 0x30DC: // KATAKANA LETTER BO
    case 0x30F4: // KATAKANA LETTER VU
    case 0x30F7: // KATAKANA LETTER VA
    case 0x30F8: // KATAKANA LETTER VI
    case 0x30F9: // KATAKANA LETTER VE
    case 0x30FA: // KATAKANA LETTER VO
        return VoicedSoundMarkType::VoicedSoundMark;
    case 0x3071: // HIRAGANA LETTER PA
    case 0x3074: // HIRAGANA LETTER PI
    case 0x3077: // HIRAGANA LETTER PU
    case 0x307A: // HIRAGANA LETTER PE
    case 0x307D: // HIRAGANA LETTER PO
    case 0x30D1: // KATAKANA LETTER PA
    case 0x30D4: // KATAKANA LETTER PI
    case 0x30D7: // KATAKANA LETTER PU
    case 0x30DA: // KATAKANA LETTER PE
    case 0x30DD: // KATAKANA LETTER PO
        return VoicedSoundMarkType::SemiVoicedSoundMark;
    }
    return VoicedSoundMarkType::NoVoicedSoundMark;
}

static bool NODELETE isCombiningVoicedSoundMark(char16_t character)
{
    switch (character) {
    case 0x3099: // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
    case 0x309A: // COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
        return true;
    }
    return false;
}

static bool isNonLatin1Separator(char32_t character)
{
    ASSERT_ARG(character, !isLatin1(character));
    return U_GET_GC_MASK(character) & (U_GC_S_MASK | U_GC_P_MASK | U_GC_Z_MASK | U_GC_CF_MASK);
}

} // namespace WebCore
