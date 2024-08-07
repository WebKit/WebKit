/*
 * Copyright (C) 2011 Nokia Inc.  All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2017 Apple Inc. All rights reserved.
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
#include "RenderQuote.h"

#include "QuotesData.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderTextFragment.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
using namespace WTF::Unicode;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderQuote);

RenderQuote::RenderQuote(Document& document, RenderStyle&& style, QuoteType quote)
    : RenderInline(Type::Quote, document, WTFMove(style))
    , m_type(quote)
    , m_text(emptyString())
{
    ASSERT(isRenderQuote());
}

RenderQuote::~RenderQuote()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderQuote::insertedIntoTree()
{
    RenderInline::insertedIntoTree();
    view().setHasQuotesNeedingUpdate(true);
}

void RenderQuote::willBeRemovedFromTree()
{
    view().setHasQuotesNeedingUpdate(true);
    RenderInline::willBeRemovedFromTree();
}

void RenderQuote::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderInline::styleDidChange(diff, oldStyle);
    if (diff >= StyleDifference::Layout) {
        m_needsTextUpdate = true;
        view().setHasQuotesNeedingUpdate(true);
    }
}

const unsigned maxDistinctQuoteCharacters = 16;

#if ASSERT_ENABLED

static void checkNumberOfDistinctQuoteCharacters(UChar character)
{
    ASSERT(character);
    static UChar distinctQuoteCharacters[maxDistinctQuoteCharacters];
    for (unsigned i = 0; i < maxDistinctQuoteCharacters; ++i) {
        if (distinctQuoteCharacters[i] == character)
            return;
        if (!distinctQuoteCharacters[i]) {
            distinctQuoteCharacters[i] = character;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

#endif // ASSERT_ENABLED

struct SubtagComparison {
    size_t keyLength;
    size_t keyContinue;
    size_t rangeLength;
    size_t rangeContinue;
    int comparison;
};

static SubtagComparison subtagCompare(const char* key, const char* range)
{
    SubtagComparison result;

    result.keyLength = strlen(key);
    result.keyContinue = result.keyLength;
    if (auto* hyphenPointer = strchr(key, '-')) {
        result.keyLength = hyphenPointer - key;
        result.keyContinue = result.keyLength + 1;
    }

    result.rangeLength = strlen(range);
    result.rangeContinue = result.rangeLength;
    if (auto* hyphenPointer = strchr(range, '-')) {
        result.rangeLength = hyphenPointer - range;
        result.rangeContinue = result.rangeLength + 1;
    }

    if (result.keyLength == result.rangeLength)
        result.comparison = memcmp(key, range, result.keyLength);
    else
        result.comparison = strcmp(key, range);

    return result;
}

// These strings need to be compared according to "Extended Filtering", as in Section 3.3.2 in RFC4647.
// https://tools.ietf.org/html/rfc4647#page-10
//
// The "checkFurther" field is needed in one specific situation.
// In the quoteTable below, there are lines like:
// { "de"   , 0x201e, 0x201c, 0x201a, 0x2018 },
// { "de-ch", 0x00ab, 0x00bb, 0x2039, 0x203a },
// Let's say the binary search arbitrarily decided to test our key against the upper line "de" first.
// If the key we're testing against is "de-ch", then we should report "greater than",
// so the binary search will keep searching and eventually find the "de-ch" line.
// However, if the key we're testing against is "de-de", then we should report "equal to",
// because these are the quotes we should use for all "de" except for "de-ch".
struct QuotesForLanguage {
    const char* language;
    UChar checkFurther;
    UChar open1;
    UChar close1;
    UChar open2;
    UChar close2;
};

static int quoteTableLanguageComparisonFunction(const void* a, const void* b)
{
    // These strings need to be compared according to "Extended Filtering", as in Section 3.3.2 in RFC4647.
    // https://tools.ietf.org/html/rfc4647#page-10
    //
    // We can exploit a few things here to improve perf:
    // 1. The first subtag must be matched exactly
    // 2. All the ranges have either 1 or 2 subtags
    // 3. None of the subtags in any of the ranges are wildcards
    //
    // Also, see the comment just above the QuotesForLanguage struct.

    auto* key = static_cast<const QuotesForLanguage*>(a);
    auto* range = static_cast<const QuotesForLanguage*>(b);

    auto firstSubtagComparison = subtagCompare(key->language, range->language);

    if (firstSubtagComparison.keyLength != firstSubtagComparison.rangeLength)
        return firstSubtagComparison.comparison;

    if (firstSubtagComparison.comparison)
        return firstSubtagComparison.comparison;

    for (UChar i = 1; i <= range->checkFurther; ++i) {
        if (!quoteTableLanguageComparisonFunction(key, range + i)) {
            // Tell the binary search to check later in the array of ranges, to eventually find the match we just found here.
            return 1;
        }
    }

    for (size_t keyOffset = firstSubtagComparison.keyContinue; ;) {
        auto nextSubtagComparison = subtagCompare(key->language + keyOffset, range->language + firstSubtagComparison.rangeContinue);

        if (!nextSubtagComparison.rangeLength) {
            // E.g. The key is "zh-Hans" and the range is "zh".
            return 0;
        }

        if (!nextSubtagComparison.keyLength) {
            // E.g. the key is "zh" and the range is "zh-Hant".
            return nextSubtagComparison.comparison;
        }

        if (nextSubtagComparison.keyLength == 1) {
            // E.g. the key is "zh-x-Hant" and the range is "zh-Hant".
            // We want to try to find the range "zh", so tell the binary search to check earlier in the array of ranges.
            return -1;
        }

        if (nextSubtagComparison.keyLength == nextSubtagComparison.rangeLength && !nextSubtagComparison.comparison) {
            // E.g. the key is "de-Latn-ch" and the range is "de-ch".
            return 0;
        }

        keyOffset += nextSubtagComparison.keyContinue;
    }
}

static const QuotesForLanguage* quotesForLanguage(const String& language)
{
    // Table of quotes from http://www.whatwg.org/specs/web-apps/current-work/multipage/rendering.html#quotes
    // FIXME: This table is out-of-date.
    static const QuotesForLanguage quoteTable[] = {
        { "af"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "agq"_s,        0, 0x201e, 0x201d, 0x201a, 0x2019 },
        { "ak"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "am"_s,         0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "ar"_s,         0, 0x201d, 0x201c, 0x2019, 0x2018 },
        { "asa"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "az-cyrl"_s,    0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "bas"_s,        0, 0x00ab, 0x00bb, 0x201e, 0x201c },
        { "bem"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "bez"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "bg"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "bm"_s,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "bn"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "br"_s,         0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "brx"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "bs-cyrl"_s,    0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "ca"_s,         0, 0x201c, 0x201d, 0x00ab, 0x00bb },
        { "cgg"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "chr"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "cs"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "da"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "dav"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "de"_s,         1, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "de-ch"_s,      0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "dje"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "dua"_s,        0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
        { "dyo"_s,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "dz"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ebu"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ee"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "el"_s,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "en"_s,         1, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "en-gb"_s,      0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "es"_s,         0, 0x201c, 0x201d, 0x00ab, 0x00bb },
        { "et"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "eu"_s,         0, 0x201c, 0x201d, 0x00ab, 0x00bb },
        { "ewo"_s,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "fa"_s,         0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "ff"_s,         0, 0x201e, 0x201d, 0x201a, 0x2019 },
        { "fi"_s,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
        { "fr"_s,         2, 0x00ab, 0x00bb, 0x00ab, 0x00bb },
        { "fr-ca"_s,      0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "fr-ch"_s,      0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "gsw"_s,        0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "gu"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "guz"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ha"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "he"_s,         0, 0x0022, 0x0022, 0x0027, 0x0027 },
        { "hi"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "hr"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "hu"_s,         0, 0x201e, 0x201d, 0x00bb, 0x00ab },
        { "id"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ig"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "it"_s,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "ja"_s,         0, 0x300c, 0x300d, 0x300e, 0x300f },
        { "jgo"_s,        0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "jmc"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "kab"_s,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "kam"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "kde"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "kea"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "khq"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ki"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "kkj"_s,        0, 0x00ab, 0x00bb, 0x2039, 0x203a },
        { "kln"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "km"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "kn"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ko"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ksb"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ksf"_s,        0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
        { "lag"_s,        0, 0x201d, 0x201d, 0x2019, 0x2019 },
        { "lg"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ln"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "lo"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "lt"_s,         0, 0x201e, 0x201c, 0x201e, 0x201c },
        { "lu"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "luo"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "luy"_s,        0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "lv"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "mas"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "mer"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "mfe"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "mg"_s,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "mgo"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "mk"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "ml"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "mr"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ms"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "mua"_s,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "my"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "naq"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "nb"_s,         0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
        { "nd"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "nl"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "nmg"_s,        0, 0x201e, 0x201d, 0x00ab, 0x00bb },
        { "nn"_s,         0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
        { "nnh"_s,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "nus"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "nyn"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "pl"_s,         0, 0x201e, 0x201d, 0x00ab, 0x00bb },
        { "pt"_s,         1, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "pt-pt"_s,      0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "rn"_s,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
        { "ro"_s,         0, 0x201e, 0x201d, 0x00ab, 0x00bb },
        { "rof"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ru"_s,         0, 0x00ab, 0x00bb, 0x201e, 0x201c },
        { "rw"_s,         0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
        { "rwk"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "saq"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "sbp"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "seh"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ses"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "sg"_s,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
        { "shi"_s,        1, 0x00ab, 0x00bb, 0x201e, 0x201d },
        { "shi-tfng"_s,   0, 0x00ab, 0x00bb, 0x201e, 0x201d },
        { "si"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "sk"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "sl"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "sn"_s,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
        { "so"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "sq"_s,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "sr"_s,         1, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "sr-latn"_s,    0, 0x201e, 0x201c, 0x201a, 0x2018 },
        { "sv"_s,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
        { "sw"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "swc"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ta"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "te"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "teo"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "th"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "ti-er"_s,      0, 0x2018, 0x2019, 0x201c, 0x201d },
        { "to"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "tr"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "twq"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "tzm"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "uk"_s,         0, 0x00ab, 0x00bb, 0x201e, 0x201c },
        { "ur"_s,         0, 0x201d, 0x201c, 0x2019, 0x2018 },
        { "vai"_s,        1, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "vai-latn"_s,   0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "vi"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "vun"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "xh"_s,         0, 0x2018, 0x2019, 0x201c, 0x201d },
        { "xog"_s,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "yav"_s,        0, 0x00ab, 0x00bb, 0x00ab, 0x00bb },
        { "yo"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "zh"_s,         1, 0x201c, 0x201d, 0x2018, 0x2019 },
        { "zh-hant"_s,    0, 0x300c, 0x300d, 0x300e, 0x300f },
        { "zu"_s,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    };

#if ASSERT_ENABLED
    // One time check that the table meets the constraints that the code below relies on.

    static bool didOneTimeCheck = false;
    if (!didOneTimeCheck) {
        didOneTimeCheck = true;

        checkNumberOfDistinctQuoteCharacters(quotationMark);
        checkNumberOfDistinctQuoteCharacters(apostrophe);

        for (unsigned i = 0; i < std::size(quoteTable); ++i) {
            if (i)
                ASSERT(strcmp(quoteTable[i - 1].language, quoteTable[i].language) < 0);

            for (unsigned j = 0; UChar character = quoteTable[i].language[j]; ++j)
                ASSERT(isASCIILower(character) || character == '-');

            checkNumberOfDistinctQuoteCharacters(quoteTable[i].open1);
            checkNumberOfDistinctQuoteCharacters(quoteTable[i].close1);
            checkNumberOfDistinctQuoteCharacters(quoteTable[i].open2);
            checkNumberOfDistinctQuoteCharacters(quoteTable[i].close2);
        }
    }
#endif // ASSERT_ENABLED

    unsigned length = language.length();
    if (!length)
        return nullptr;

    Vector<char> languageKeyBuffer(length + 1);
    for (unsigned i = 0; i < length; ++i) {
        UChar character = toASCIILower(language[i]);
        if (!(isASCIILower(character) || character == '-'))
            return nullptr;
        languageKeyBuffer[i] = static_cast<char>(character);
    }
    languageKeyBuffer[length] = 0;

    QuotesForLanguage languageKey = { languageKeyBuffer.data(), 0, 0, 0, 0, 0 };

    return static_cast<const QuotesForLanguage*>(bsearch(&languageKey,
        quoteTable, std::size(quoteTable), sizeof(quoteTable[0]), quoteTableLanguageComparisonFunction));
}

static StringImpl* stringForQuoteCharacter(UChar character)
{
    // Use linear search because there is a small number of distinct characters, thus binary search is unneeded.
    ASSERT(character);
    struct StringForCharacter {
        UChar character;
        StringImpl* string;
    };
    static StringForCharacter strings[maxDistinctQuoteCharacters];
    for (unsigned i = 0; i < maxDistinctQuoteCharacters; ++i) {
        if (strings[i].character == character)
            return strings[i].string;
        if (!strings[i].character) {
            strings[i].character = character;
            strings[i].string = &StringImpl::create8BitIfPossible(span(character)).leakRef();
            return strings[i].string;
        }
    }
    ASSERT_NOT_REACHED();
    return StringImpl::empty();
}

static inline StringImpl* quotationMarkString()
{
    static StringImpl* quotationMarkString = stringForQuoteCharacter(quotationMark);
    return quotationMarkString;
}

static inline StringImpl* apostropheString()
{
    static StringImpl* apostropheString = stringForQuoteCharacter(apostrophe);
    return apostropheString;
}

void RenderQuote::updateTextRenderer(RenderTreeBuilder& builder)
{
    ASSERT_WITH_SECURITY_IMPLICATION(document().inRenderTreeUpdate());
    String text = computeText();
    if (m_text == text)
        return;
    m_text = text;
    if (auto* renderText = dynamicDowncast<RenderTextFragment>(lastChild())) {
        renderText->setContentString(m_text);
        renderText->dirtyLineBoxes(false);
        return;
    }
    builder.attach(*this, createRenderer<RenderTextFragment>(document(), m_text));
}

String RenderQuote::computeText() const
{
    if (m_depth < 0)
        return emptyString();
    bool isOpenQuote = false;
    switch (m_type) {
    case QuoteType::NoOpenQuote:
    case QuoteType::NoCloseQuote:
        return emptyString();
    case QuoteType::OpenQuote:
        isOpenQuote = true;
        FALLTHROUGH;
    case QuoteType::CloseQuote:
        if (const auto* quotes = style().quotes())
            return isOpenQuote ? quotes->openQuote(m_depth).impl() : quotes->closeQuote(m_depth).impl();
        if (const auto* quotes = quotesForLanguage(style().computedLocale()))
            return stringForQuoteCharacter(isOpenQuote ? (m_depth ? quotes->open2 : quotes->open1) : (m_depth ? quotes->close2 : quotes->close1));
        // FIXME: Should the default be the quotes for "en" rather than straight quotes?
        // (According to https://html.spec.whatwg.org/multipage/rendering.html#quotes, the answer is "yes".)
        return m_depth ? apostropheString() : quotationMarkString();
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

bool RenderQuote::isOpen() const
{
    switch (m_type) {
    case QuoteType::OpenQuote:
    case QuoteType::NoOpenQuote:
        return true;
    case QuoteType::CloseQuote:
    case QuoteType::NoCloseQuote:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void RenderQuote::updateRenderer(RenderTreeBuilder& builder, RenderQuote* previousQuote)
{
    ASSERT_WITH_SECURITY_IMPLICATION(document().inRenderTreeUpdate());
    int depth = -1;
    if (previousQuote) {
        depth = previousQuote->m_depth;
        if (previousQuote->isOpen())
            ++depth;
    }

    if (!isOpen())
        --depth;
    else if (depth < 0)
        depth = 0;

    if (m_depth == depth && !m_needsTextUpdate)
        return;

    m_depth = depth;
    m_needsTextUpdate = false;
    updateTextRenderer(builder);
}

} // namespace WebCore
