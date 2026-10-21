/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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
#include "RenderTreeUpdaterGeneratedContent.h"

#include "ContainerNodeInlines.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "InspectorInstrumentation.h"
#include "KeyframeEffectStack.h"
#include "PseudoElement.h"
#include "PseudoElementUtilitiesInlines.h"
#include "RenderBoxModelObject.h"
#include "RenderCombineText.h"
#include "RenderCounter.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderGlyph.h"
#include "RenderImage.h"
#include "RenderTextFragment.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleTreeResolver.h"
#include "WritingSuggestionData.h"
#include <array>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeUpdater::GeneratedContent);

RenderTreeUpdater::GeneratedContent::GeneratedContent(RenderTreeUpdater& updater)
    : m_updater(updater)
{
}

// These strings need to be compared according to "Extended Filtering", as in Section 3.3.2 in RFC4647.
// https://tools.ietf.org/html/rfc4647#page-10
//
// The "checkFurther" field is needed in one specific situation.
// In the quoteTable below, there are lines like:
// { "de"_span   , 0x201e, 0x201c, 0x201a, 0x2018 },
// { "de-ch"_span, 0x00ab, 0x00bb, 0x2039, 0x203a },
// Let's say the binary search arbitrarily decided to test our key against the upper line "de" first.
// If the key we're testing against is "de-ch", then we should report "greater than",
// so the binary search will keep searching and eventually find the "de-ch" line.
// However, if the key we're testing against is "de-de", then we should report "equal to",
// because these are the quotes we should use for all "de" except for "de-ch".
struct QuotesForLanguage {
    std::span<const char> language;
    uint8_t checkFurther { 0 };
    char16_t open1 { 0 };
    char16_t close1 { 0 };
    char16_t open2 { 0 };
    char16_t close2 { 0 };
};

// Table of quotes from http://www.whatwg.org/specs/web-apps/current-work/multipage/rendering.html#quotes
// FIXME: This table is out-of-date.
static constexpr std::array quoteTable {
    QuotesForLanguage { "af"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "agq"_span,        0, 0x201e, 0x201d, 0x201a, 0x2019 },
    QuotesForLanguage { "ak"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "am"_span,         0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "ar"_span,         0, 0x201d, 0x201c, 0x2019, 0x2018 },
    QuotesForLanguage { "asa"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "az-cyrl"_span,    0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "bas"_span,        0, 0x00ab, 0x00bb, 0x201e, 0x201c },
    QuotesForLanguage { "bem"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "bez"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "bg"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "bm"_span,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "bn"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "br"_span,         0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "brx"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "bs-cyrl"_span,    0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "ca"_span,         0, 0x201c, 0x201d, 0x00ab, 0x00bb },
    QuotesForLanguage { "cgg"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "chr"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "cs"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "da"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "dav"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "de"_span,         1, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "de-ch"_span,      0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "dje"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "dua"_span,        0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
    QuotesForLanguage { "dyo"_span,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "dz"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ebu"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ee"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "el"_span,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "en"_span,         1, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "en-gb"_span,      0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "es"_span,         0, 0x201c, 0x201d, 0x00ab, 0x00bb },
    QuotesForLanguage { "et"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "eu"_span,         0, 0x201c, 0x201d, 0x00ab, 0x00bb },
    QuotesForLanguage { "ewo"_span,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "fa"_span,         0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "ff"_span,         0, 0x201e, 0x201d, 0x201a, 0x2019 },
    QuotesForLanguage { "fi"_span,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
    QuotesForLanguage { "fr"_span,         2, 0x00ab, 0x00bb, 0x00ab, 0x00bb },
    QuotesForLanguage { "fr-ca"_span,      0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "fr-ch"_span,      0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "gsw"_span,        0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "gu"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "guz"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ha"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "he"_span,         0, 0x0022, 0x0022, 0x0027, 0x0027 },
    QuotesForLanguage { "hi"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "hr"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "hu"_span,         0, 0x201e, 0x201d, 0x00bb, 0x00ab },
    QuotesForLanguage { "id"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ig"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "it"_span,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "ja"_span,         0, 0x300c, 0x300d, 0x300e, 0x300f },
    QuotesForLanguage { "jgo"_span,        0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "jmc"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "kab"_span,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "kam"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "kde"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "kea"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "khq"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ki"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "kkj"_span,        0, 0x00ab, 0x00bb, 0x2039, 0x203a },
    QuotesForLanguage { "kln"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "km"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "kn"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ko"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ksb"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ksf"_span,        0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
    QuotesForLanguage { "lag"_span,        0, 0x201d, 0x201d, 0x2019, 0x2019 },
    QuotesForLanguage { "lg"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ln"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "lo"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "lt"_span,         0, 0x201e, 0x201c, 0x201e, 0x201c },
    QuotesForLanguage { "lu"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "luo"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "luy"_span,        0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "lv"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "mas"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "mer"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "mfe"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "mg"_span,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "mgo"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "mk"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "ml"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "mr"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ms"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "mua"_span,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "my"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "naq"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "nb"_span,         0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
    QuotesForLanguage { "nd"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "nl"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "nmg"_span,        0, 0x201e, 0x201d, 0x00ab, 0x00bb },
    QuotesForLanguage { "nn"_span,         0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
    QuotesForLanguage { "nnh"_span,        0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "nus"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "nyn"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "pl"_span,         0, 0x201e, 0x201d, 0x00ab, 0x00bb },
    QuotesForLanguage { "pt"_span,         1, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "pt-pt"_span,      0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "rn"_span,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
    QuotesForLanguage { "ro"_span,         0, 0x201e, 0x201d, 0x00ab, 0x00bb },
    QuotesForLanguage { "rof"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ru"_span,         0, 0x00ab, 0x00bb, 0x201e, 0x201c },
    QuotesForLanguage { "rw"_span,         0, 0x00ab, 0x00bb, 0x2018, 0x2019 },
    QuotesForLanguage { "rwk"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "saq"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "sbp"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "seh"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ses"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "sg"_span,         0, 0x00ab, 0x00bb, 0x201c, 0x201d },
    QuotesForLanguage { "shi"_span,        1, 0x00ab, 0x00bb, 0x201e, 0x201d },
    QuotesForLanguage { "shi-tfng"_span,   0, 0x00ab, 0x00bb, 0x201e, 0x201d },
    QuotesForLanguage { "si"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "sk"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "sl"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "sn"_span,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
    QuotesForLanguage { "so"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "sq"_span,         0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "sr"_span,         1, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "sr-latn"_span,    0, 0x201e, 0x201c, 0x201a, 0x2018 },
    QuotesForLanguage { "sv"_span,         0, 0x201d, 0x201d, 0x2019, 0x2019 },
    QuotesForLanguage { "sw"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "swc"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ta"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "te"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "teo"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "th"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "ti-er"_span,      0, 0x2018, 0x2019, 0x201c, 0x201d },
    QuotesForLanguage { "to"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "tr"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "twq"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "tzm"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "uk"_span,         0, 0x00ab, 0x00bb, 0x201e, 0x201c },
    QuotesForLanguage { "ur"_span,         0, 0x201d, 0x201c, 0x2019, 0x2018 },
    QuotesForLanguage { "vai"_span,        1, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "vai-latn"_span,   0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "vi"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "vun"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "xh"_span,         0, 0x2018, 0x2019, 0x201c, 0x201d },
    QuotesForLanguage { "xog"_span,        0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "yav"_span,        0, 0x00ab, 0x00bb, 0x00ab, 0x00bb },
    QuotesForLanguage { "yo"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "zh"_span,         1, 0x201c, 0x201d, 0x2018, 0x2019 },
    QuotesForLanguage { "zh-hant"_span,    0, 0x300c, 0x300d, 0x300e, 0x300f },
    QuotesForLanguage { "zu"_span,         0, 0x201c, 0x201d, 0x2018, 0x2019 },
};

constexpr unsigned maxDistinctQuoteCharacters = 16;

#if ASSERT_ENABLED

static void checkNumberOfDistinctQuoteCharacters(char16_t character)
{
    ASSERT(character);
    static std::array<char16_t, maxDistinctQuoteCharacters> distinctQuoteCharacters;
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
    std::strong_ordering comparison { std::strong_ordering::equal };
};

static SubtagComparison LEGACY_NODELETE subtagCompare(std::span<const char> key, std::span<const char> range)
{
    SubtagComparison result;

    result.keyLength = key.size();
    result.keyContinue = result.keyLength;
    if (size_t hyphenIndex = find(key, '-'); hyphenIndex != notFound) {
        result.keyLength = hyphenIndex;
        result.keyContinue = result.keyLength + 1;
    }

    result.rangeLength = range.size();
    result.rangeContinue = result.rangeLength;
    if (size_t hyphenIndex = find(range, '-'); hyphenIndex != notFound) {
        result.rangeLength = hyphenIndex;
        result.rangeContinue = result.rangeLength + 1;
    }

    if (result.keyLength == result.rangeLength)
        result.comparison = compareSpans(key.first(result.keyLength), range.first(result.keyLength));
    else
        result.comparison = compareSpans(key, range);

    return result;
}

static std::strong_ordering LEGACY_NODELETE quoteTableLanguageComparisonFunction(const QuotesForLanguage& key, std::span<const QuotesForLanguage> range)
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

    auto firstSubtagComparison = subtagCompare(key.language, range[0].language);

    if (firstSubtagComparison.keyLength != firstSubtagComparison.rangeLength)
        return firstSubtagComparison.comparison;

    if (is_neq(firstSubtagComparison.comparison))
        return firstSubtagComparison.comparison;

    for (auto& checkFurtherRange : range.subspan(1)) {
        if (is_eq(quoteTableLanguageComparisonFunction(key, singleElementSpan(checkFurtherRange)))) {
            // Tell the binary search to check later in the array of ranges, to eventually find the match we just found here.
            return std::strong_ordering::greater;
        }
    }

    for (size_t keyOffset = firstSubtagComparison.keyContinue; ;) {
        auto nextSubtagComparison = subtagCompare(key.language.subspan(keyOffset), range[0].language.subspan(firstSubtagComparison.rangeContinue));

        if (!nextSubtagComparison.rangeLength) {
            // E.g. The key is "zh-Hans" and the range is "zh".
            return std::strong_ordering::equal;
        }

        if (!nextSubtagComparison.keyLength) {
            // E.g. the key is "zh" and the range is "zh-Hant".
            return nextSubtagComparison.comparison;
        }

        if (nextSubtagComparison.keyLength == 1) {
            // E.g. the key is "zh-x-Hant" and the range is "zh-Hant".
            // We want to try to find the range "zh", so tell the binary search to check earlier in the array of ranges.
            return std::strong_ordering::less;
        }

        if (nextSubtagComparison.keyLength == nextSubtagComparison.rangeLength && is_eq(nextSubtagComparison.comparison)) {
            // E.g. the key is "de-Latn-ch" and the range is "de-ch".
            return std::strong_ordering::equal;
        }

        keyOffset += nextSubtagComparison.keyContinue;
    }
}

static const QuotesForLanguage* LEGACY_NODELETE binaryFindQuotes(const QuotesForLanguage& key, std::span<const QuotesForLanguage> subrange = quoteTable)
{
    if (subrange.empty())
        return nullptr;

    auto& middle = subrange[subrange.size() / 2];
    auto comparison = quoteTableLanguageComparisonFunction(key, std::span { quoteTable }.subspan(&middle - quoteTable.data(), 1 + middle.checkFurther));
    if (is_eq(comparison))
        return &middle;
    if (is_lt(comparison))
        return binaryFindQuotes(key, subrange.first(subrange.size() / 2));
    return binaryFindQuotes(key, subrange.subspan(subrange.size() / 2 + 1));
}

static const QuotesForLanguage* quotesForLanguage(const String& language)
{
#if ASSERT_ENABLED
    // One time check that the table meets the constraints that the code below relies on.

    static bool didOneTimeCheck = false;
    if (!didOneTimeCheck) {
        didOneTimeCheck = true;

        checkNumberOfDistinctQuoteCharacters(quotationMark);
        checkNumberOfDistinctQuoteCharacters(apostrophe);

        for (unsigned i = 0; i < std::size(quoteTable); ++i) {
            if (i)
                ASSERT(is_lt(compareSpans(quoteTable[i - 1].language, quoteTable[i].language)));

            for (auto character : quoteTable[i].language)
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

    Vector<char> languageKeyBuffer(length);
    for (unsigned i = 0; i < length; ++i) {
        char16_t character = toASCIILower(language[i]);
        if (!(isASCIILower(character) || character == '-'))
            return nullptr;
        languageKeyBuffer[i] = static_cast<char>(character);
    }

    return binaryFindQuotes({ languageKeyBuffer.span() });
}

static StringImpl* stringForQuoteCharacter(char16_t character)
{
    // Use linear search because there is a small number of distinct characters, thus binary search is unneeded.
    ASSERT(character);
    struct StringForCharacter {
        char16_t character;
        StringImpl* string;
    };
    static std::array<StringForCharacter, maxDistinctQuoteCharacters> strings;
    for (auto& string : strings) {
        if (string.character == character)
            return string.string;
        if (!string.character) {
            string.character = character;
            string.string = &StringImpl::create8BitIfPossible(span(character)).leakRef();
            return string.string;
        }
    }
    ASSERT_NOT_REACHED();
    return StringImpl::empty();
}

static inline StringImpl* quotationMarkString()
{
    static NeverDestroyed<Ref<StringImpl>> quotationMarkString { *stringForQuoteCharacter(quotationMark) };
    return quotationMarkString->ptr();
}

static inline StringImpl* apostropheString()
{
    static NeverDestroyed<Ref<StringImpl>> apostropheString { *stringForQuoteCharacter(apostrophe) };
    return apostropheString->ptr();
}

// css-content-3: "If the depth is 0, the first pair is used, if the depth is 1, the second pair is used, etc."
static String quoteMark(QuoteType quoteType, unsigned quoteDepth, const Style::ComputedStyle& style)
{
    ASSERT(quoteType == QuoteType::OpenQuote || quoteType == QuoteType::CloseQuote);
    bool isOpenQuote = quoteType == QuoteType::OpenQuote;
    if (!style.quotes().isAuto())
        return isOpenQuote ? style.quotes().openQuote(quoteDepth) : style.quotes().closeQuote(quoteDepth);
    if (const auto* quotes = quotesForLanguage(Style::toPlatform(style.usedLocale())))
        return stringForQuoteCharacter(isOpenQuote ? (quoteDepth ? quotes->open2 : quotes->open1) : (quoteDepth ? quotes->close2 : quotes->close1));
    // FIXME: Should the default be the quotes for "en" rather than straight quotes?
    // (According to https://html.spec.whatwg.org/multipage/rendering.html#quotes, the answer is "yes".)
    return quoteDepth ? apostropheString() : quotationMarkString();
}

// css-content-3: the quote depth is "the number of occurrences of open-quote in all generated text before the current
// occurrence, minus the number of occurrences of close-quote in all generated text before and including the current occurence."
// "A close-quote or no-close-quote that would make the quote depth negative is in error and is ignored (at rendering time):
// the depth stays at 0 and no quote mark is inserted".
static String quoteMarkAndAdvanceQuoteDepth(QuoteType quoteType, unsigned& quoteDepth, const Style::ComputedStyle& style)
{
    switch (quoteType) {
    case QuoteType::OpenQuote:
        return quoteMark(quoteType, quoteDepth++, style);
    case QuoteType::NoOpenQuote:
        ++quoteDepth;
        return emptyString();
    case QuoteType::CloseQuote:
        if (!quoteDepth)
            return emptyString();
        return quoteMark(quoteType, --quoteDepth, style);
    case QuoteType::NoCloseQuote:
        if (quoteDepth)
            --quoteDepth;
        return emptyString();
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

void RenderTreeUpdater::GeneratedContent::updateRemainingQuotes()
{
    if (!m_updater.renderView().hasQuotesNeedingUpdate())
        return;
    updateQuotesUpTo(nullptr);
    m_previousUpdatedQuote = nullptr;
    m_quoteScopeStack.clear();
    m_updater.renderView().setHasQuotesNeedingUpdate(false);
}

static RenderElement* findQuoteScopeRoot(const RenderObject& renderer)
{
    for (auto* ancestor = renderer.parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor->shouldApplyStyleContainment())
            return ancestor;
    }
    return nullptr;
}

RenderElement* RenderTreeUpdater::GeneratedContent::popExitedQuoteScopes(const RenderTextFragment& quote)
{
    auto* scopeRoot = findQuoteScopeRoot(quote);
    auto isCurrentScopeAncestorOfQuote = [&] {
        auto* topScope = m_quoteScopeStack.last().scopeRoot.get();
        return topScope == scopeRoot || (topScope && quote.isDescendantOf(topScope));
    };
    while (m_quoteScopeStack.size() > 1 && !isCurrentScopeAncestorOfQuote())
        m_quoteScopeStack.removeLast();
    return scopeRoot;
}

// Quotes are looked up among RenderText descendants because RenderTextFragment can't be a descendant iterator type:
// RenderObjectTraversal::firstChild() only knows that RenderText itself has no children.
static CheckedPtr<RenderTextFragment> quoteTextRenderer(RenderText& textRenderer)
{
    CheckedPtr textFragment = dynamicDowncast<RenderTextFragment>(textRenderer);
    if (!textFragment || !textFragment->quoteType())
        return nullptr;
    return textFragment;
}

void RenderTreeUpdater::GeneratedContent::updateQuotesUpTo(RenderTextFragment* lastQuote)
{
    // ::first-letter destroys the text it splits, so the quote we stopped at may be gone. Start over from the first quote then.
    if (!m_previousUpdatedQuote)
        m_quoteScopeStack.clear();

    auto textRenderers = descendantsOfType<RenderText>(m_updater.renderView());
    auto it = m_previousUpdatedQuote ? ++textRenderers.at(*m_previousUpdatedQuote) : textRenderers.begin();
    auto end = textRenderers.end();

    if (m_quoteScopeStack.isEmpty())
        m_quoteScopeStack.append({ nullptr, 0 });

    for (; it != end; ++it) {
        CheckedPtr quote = quoteTextRenderer(*it);
        if (!quote)
            continue;

        auto* scopeRoot = popExitedQuoteScopes(*quote);

        bool hasEnteredNewContainmentScope = scopeRoot != m_quoteScopeStack.last().scopeRoot.get();
        if (hasEnteredNewContainmentScope)
            m_quoteScopeStack.append({ scopeRoot, m_quoteScopeStack.last().quoteDepth });

        auto newQuoteMark = quoteMarkAndAdvanceQuoteDepth(*quote->quoteType(), m_quoteScopeStack.last().quoteDepth, quote->style());
        if (!equal(quote->contentString(), newQuoteMark.impl()))
            quote->setContentString(newQuoteMark);

        m_previousUpdatedQuote = *quote;
        if (quote.get() == lastQuote)
            return;
    }
    ASSERT_UNUSED(lastQuote, !lastQuote);
}

void RenderTreeUpdater::GeneratedContent::updateCounters()
{
    if (m_updater.renderView().hasCounterTreeNeedingUpdate()) {
        for (auto& renderer : descendantsOfType<RenderElement>(m_updater.renderView())) {
            if (renderer.hasCounterNodeMap())
                RenderCounter::destroyCounterNodes(renderer);
        }
        m_updater.renderView().setHasCounterTreeNeedingUpdate(false);
    }

    auto update = [&] {
        auto counters = m_updater.renderView().takeCountersNeedingUpdate();
        for (auto& counter : counters)
            counter.updateCounter();
    };
    // Update twice and hope it stabilizes.
    update();
    update();
}

void RenderTreeUpdater::GeneratedContent::updateListMarkers()
{
    // The markers were collected by the builder as it changed the tree, since a marker's text is made of its list
    // item's list-item counter value and that is only settled once the tree is done changing.
    m_updater.m_builder.updateListMarkerContents();
}

static KeyframeEffectStack* LEGACY_NODELETE keyframeEffectStackForPseudoElement(const Element& element, PseudoElementType pseudoElementType)
{
    if (!element.mayHaveKeyframeEffects())
        return nullptr;

    return element.keyframeEffectStack(std::optional(Style::PseudoElementIdentifier { pseudoElementType }));
}

static bool needsPseudoElementForAnimation(const Element& element, PseudoElementType pseudoElementType)
{
    auto* stack = keyframeEffectStackForPseudoElement(element, pseudoElementType);
    if (!stack)
        return false;

    return stack->requiresPseudoElement() || stack->containsProperty(CSSPropertyID::Display);
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Text& value, const String& altText, Document& document, const Style::ComputedStyle& pseudoStyle)
{
    if (value.text.value.isEmpty() && altText.isEmpty())
        return { };

    if (pseudoStyle.isListMarkerStyle() && pseudoStyle.textCombine() != TextCombine::None)
        return createRenderer<RenderCombineText>(document, value.text.value);

    auto contentRenderer = createRenderer<RenderTextFragment>(document, value.text.value);
    contentRenderer->setAltText(altText);
    return contentRenderer;
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Image& value, const String& altText, Document& document, const Style::ComputedStyle& pseudoStyle)
{
    auto contentRenderer = createRenderer<RenderImage>(RenderObject::Type::Image, document, Style::ComputedStyle::createStyleInheritingFromPseudoStyle(pseudoStyle), value.image.value.ptr());
    contentRenderer->initializeStyle();
    contentRenderer->setAltText(altText);
    return contentRenderer;
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Counter& value, const String&, Document& document, const Style::ComputedStyle&)
{
    return createRenderer<RenderCounter>(document, value);
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Quote& value, const String&, Document& document, const Style::ComputedStyle&)
{
    // The quote mark is only known once the quotes before this one are in the tree, see updateQuotesUpTo().
    auto contentRenderer = createRenderer<RenderTextFragment>(document, emptyString());
    contentRenderer->setQuoteType(value.quote);
    return contentRenderer;
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Glyph& value, const String& altText, Document& document, const Style::ComputedStyle&)
{
    auto contentRenderer = createRenderer<RenderGlyph>(document, value.glyph);
    contentRenderer->setAltText(altText);
    return contentRenderer;
}

void RenderTreeUpdater::GeneratedContent::createContentRenderers(RenderTreeBuilder& builder, RenderElement& pseudoRenderer, const Style::ComputedStyle& style, PseudoElementType pseudoElementType)
{
    if (auto* contentData = style.content().tryData()) {
        auto altText = contentData->alt.value_or(String { nullString() });
        for (auto& contentItem : contentData->visible) {
            WTF::switchOn(contentItem,
                [&](const auto& item) {
                    if (auto child = createContentRenderer(item, altText.value, pseudoRenderer.document(), style); child && pseudoRenderer.isChildAllowed(*child, style))
                        builder.attach(pseudoRenderer, WTF::move(child));
                }
            );
        }
    }
#if ASSERT_ENABLED
    else {
        auto elementIsTargetedByKeyframeEffectRequiringPseudoElement = [](const Element& element, PseudoElementType pseudoElementType) {
            if (auto* stack = keyframeEffectStackForPseudoElement(element, pseudoElementType))
                return stack->requiresPseudoElement();

            return false;
        };

        // The only valid scenario where this method is called without the "content" property being set
        // is the case where a pseudo-element has animations set on it via the Web Animations API.
        if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(pseudoRenderer.element())) {
            RefPtr hostElement = pseudoElement->hostElement();
            ASSERT(!is<PseudoElement>(hostElement));
            ASSERT(elementIsTargetedByKeyframeEffectRequiringPseudoElement(*hostElement, pseudoElementType));
        }
    }
#else
    UNUSED_PARAM(pseudoElementType);
#endif
}

void RenderTreeUpdater::GeneratedContent::updateStyleForContentRenderers(RenderElement& pseudoRenderer, const Style::ComputedStyle& style)
{
    for (auto& contentRenderer : descendantsOfType<RenderElement>(pseudoRenderer)) {
        // Generated text uses the style of its parent, so only images need theirs updated.
        if (!is<RenderImage>(contentRenderer))
            continue;
        contentRenderer.setStyle(Style::ComputedStyle::createStyleInheritingFromPseudoStyle(style));
    }
}

void RenderTreeUpdater::GeneratedContent::updateBeforeOrAfterPseudoElement(Element& current, const Style::ElementUpdate& elementUpdate, PseudoElementType pseudoElementType)
{
    ASSERT(pseudoElementType == PseudoElementType::Before || pseudoElementType == PseudoElementType::After);

    RefPtr pseudoElement = pseudoElementType == PseudoElementType::Before ? current.beforePseudoElement() : current.afterPseudoElement();

    if (auto* renderer = pseudoElement ? pseudoElement->renderer() : nullptr)
        m_updater.renderTreePosition().invalidateNextSibling(*renderer);

    auto* updateStyle = (elementUpdate.style && elementUpdate.style->hasPseudoElementStyles()) ? elementUpdate.style->pseudoElementStyle({ pseudoElementType }) : nullptr;

    // If we end up losing a previous pseudo because the style got removed we need to
    // cancel any animations that were on it so we do not end up thinking we need to keep
    // the renderer around. We cannot completely rely on needsPseudoElement because
    // we may need to animate a box with display: none.
    if (!updateStyle)
        Styleable { current, Style::PseudoElementIdentifier { pseudoElementType } }.cancelStyleOriginatedAnimations();

    ASSERT(!is<PseudoElement>(current));
    if (!needsPseudoElement(updateStyle) && !needsPseudoElementForAnimation(current, pseudoElementType)) {
        if (pseudoElement) {
            if (pseudoElementType == PseudoElementType::Before)
                removeBeforePseudoElement(current, m_updater.m_builder);
            else
                removeAfterPseudoElement(current, m_updater.m_builder);
        }
        return;
    }

    if (!updateStyle)
        return;

    auto* existingStyle = pseudoElement ? pseudoElement->renderOrDisplayContentsStyle() : nullptr;

    auto styleChanges = existingStyle ? Style::determineChanges(*updateStyle, *existingStyle) : Style::Change::Renderer;
    if (!styleChanges)
        return;

    pseudoElement = &current.ensurePseudoElement(pseudoElementType);

    if (updateStyle->display() == Style::DisplayType::Contents) {
        // For display:contents we create an inline wrapper that inherits its
        // style from the display:contents style.
        auto contentsStyle = Style::ComputedStyle::createPtr();
        contentsStyle->setPseudoElementIdentifier({ { pseudoElementType } });
        contentsStyle->inheritFrom(*updateStyle);
        contentsStyle->copyContentFrom(*updateStyle);
        contentsStyle->copyPseudoElementsFrom(*updateStyle);

        Style::ElementUpdate contentsUpdate { WTF::move(contentsStyle), styleChanges, elementUpdate.recompositeLayer };
        m_updater.updateElementRenderer(*pseudoElement, WTF::move(contentsUpdate));
        auto pseudoElementUpdateStyle = Style::ComputedStyle::cloneIncludingPseudoElements(*updateStyle);
        pseudoElement->storeDisplayContentsOrNoneStyle(makeUnique<Style::ComputedStyle>(WTF::move(pseudoElementUpdateStyle)));
    } else {
        auto pseudoElementUpdateStyle = Style::ComputedStyle::cloneIncludingPseudoElements(*updateStyle);
        Style::ElementUpdate pseudoElementUpdate { makeUnique<Style::ComputedStyle>(WTF::move(pseudoElementUpdateStyle)), styleChanges, elementUpdate.recompositeLayer };
        m_updater.updateElementRenderer(*pseudoElement, WTF::move(pseudoElementUpdate));
    }

    auto* pseudoElementRenderer = pseudoElement->renderer();
    if (!pseudoElementRenderer)
        return;

    if (styleChanges.contains(Style::Change::Renderer))
        createContentRenderers(m_updater.m_builder, *pseudoElementRenderer, *updateStyle, pseudoElementType);
    else
        updateStyleForContentRenderers(*pseudoElementRenderer, *updateStyle);

    if (m_updater.renderView().hasQuotesNeedingUpdate()) {
        for (auto& textRenderer : descendantsOfType<RenderText>(*pseudoElementRenderer)) {
            if (CheckedPtr quote = quoteTextRenderer(textRenderer))
                updateQuotesUpTo(quote.get());
        }
    }
    m_updater.m_builder.updateAfterDescendants(*pseudoElementRenderer);
}

void RenderTreeUpdater::GeneratedContent::updateBackdropRenderer(RenderElement& renderer, Style::DifferenceResult minimalStyleDifference)
{
    auto destroyBackdropIfNeeded = [&renderer, this]() {
        if (WeakPtr backdropRenderer = renderer.pseudoElementRenderer(PseudoElementType::Backdrop))
            m_updater.m_builder.destroy(*backdropRenderer);
    };

    // Intentionally bail out early here to avoid computing the style.
    if (!renderer.element() || !renderer.element()->isInTopLayer()) {
        destroyBackdropIfNeeded();
        return;
    }

    auto style = renderer.style().pseudoElementStyle({ PseudoElementType::Backdrop });
    if (!style || style->display() == Style::DisplayType::None) {
        destroyBackdropIfNeeded();
        return;
    }

    auto newStyle = Style::ComputedStyle::clone(*style);
    if (auto backdropRenderer = renderer.pseudoElementRenderer(PseudoElementType::Backdrop))
        backdropRenderer->setStyle(WTF::move(newStyle), minimalStyleDifference);
    else {
        auto newBackdropRenderer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, renderer.document(), WTF::move(newStyle));
        newBackdropRenderer->initializeStyle();
        renderer.setPseudoElementRenderer(PseudoElementType::Backdrop, *newBackdropRenderer.get());
        m_updater.m_builder.attach(renderer.view(), WTF::move(newBackdropRenderer));
    }
}

bool RenderTreeUpdater::GeneratedContent::needsPseudoElement(const Style::ComputedStyle* style)
{
    if (!style)
        return false;
    if (!m_updater.renderTreePosition().parent().canHaveGeneratedChildren())
        return false;
    if (!Style::pseudoElementRendererIsNeeded(*style))
        return false;
    return true;
}

void RenderTreeUpdater::GeneratedContent::removeBeforePseudoElement(Element& element, RenderTreeBuilder& builder)
{
    RefPtr pseudoElement = element.beforePseudoElement();
    if (!pseudoElement)
        return;
    tearDownRenderers(*pseudoElement, TeardownType::Full, builder);
    element.clearBeforePseudoElement();
}

void RenderTreeUpdater::GeneratedContent::removeAfterPseudoElement(Element& element, RenderTreeBuilder& builder)
{
    RefPtr pseudoElement = element.afterPseudoElement();
    if (!pseudoElement)
        return;
    tearDownRenderers(*pseudoElement, TeardownType::Full, builder);
    element.clearAfterPseudoElement();
}

void RenderTreeUpdater::GeneratedContent::updateWritingSuggestionsRenderer(RenderElement& renderer, Style::DifferenceResult minimalStyleDifference)
{
    auto destroyWritingSuggestionsIfNeeded = [&renderer, this]() {
        if (!renderer.element())
            return;

        Ref editor = renderer.element()->document().editor();

        if (WeakPtr writingSuggestionsRenderer = editor->writingSuggestionRenderer())
            m_updater.m_builder.destroy(*writingSuggestionsRenderer);
    };

    if (!renderer.canHaveChildren())
        return;

    if (!renderer.element())
        return;

    Ref editor = renderer.element()->document().editor();
    RefPtr nodeBeforeWritingSuggestions = editor->nodeBeforeWritingSuggestions();
    if (!nodeBeforeWritingSuggestions)
        return;

    if (renderer.element() != nodeBeforeWritingSuggestions->parentElement())
        return;

    auto* writingSuggestionData = editor->writingSuggestionData();
    if (!writingSuggestionData) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    auto style = renderer.lazyPseudoElementStyle({ PseudoElementType::InternalWritingSuggestions });
    if (!style || style->display() == Style::DisplayType::None) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    WeakPtr nodeBeforeWritingSuggestionsTextRenderer = dynamicDowncast<RenderText>(nodeBeforeWritingSuggestions->renderer());
    if (!nodeBeforeWritingSuggestionsTextRenderer) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    WeakPtr parentForWritingSuggestions = nodeBeforeWritingSuggestionsTextRenderer->parent();
    if (!parentForWritingSuggestions) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    auto textWithoutSuggestion = nodeBeforeWritingSuggestionsTextRenderer->text();

    auto [prefix, suffix] = [&] -> std::pair<String, String> {
        if (!writingSuggestionData->supportsSuffix())
            return { textWithoutSuggestion, emptyString() };

        auto offset = writingSuggestionData->offset();
        return { textWithoutSuggestion.substring(0, offset), textWithoutSuggestion.substring(offset) };
    }();

    nodeBeforeWritingSuggestionsTextRenderer->setText(prefix);

    auto newStyle = Style::ComputedStyle::clone(*style);
    newStyle.setDisplay(Style::DisplayType::InlineFlow);

    if (auto writingSuggestionsRenderer = editor->writingSuggestionRenderer()) {
        writingSuggestionsRenderer->setStyle(WTF::move(newStyle), minimalStyleDifference);

        auto* writingSuggestionsText = dynamicDowncast<RenderText>(writingSuggestionsRenderer->firstChild());
        if (!writingSuggestionsText) {
            ASSERT_NOT_REACHED();
            destroyWritingSuggestionsIfNeeded();
            return;
        }

        writingSuggestionsText->setText(writingSuggestionData->content());

        if (!suffix.isEmpty()) {
            auto* suffixText = dynamicDowncast<RenderText>(writingSuggestionsRenderer->nextSibling());
            if (!suffixText) {
                ASSERT_NOT_REACHED();
                destroyWritingSuggestionsIfNeeded();
                return;
            }

            suffixText->setText(suffix);
        }
    } else {
        auto newWritingSuggestionsRenderer = WebCore::createRenderer<RenderBoxModelObject>(RenderObject::Type::Inline, renderer.document(), WTF::move(newStyle));
        newWritingSuggestionsRenderer->initializeStyle();

        WeakPtr rendererAfterWritingSuggestions = nodeBeforeWritingSuggestionsTextRenderer->nextSibling();

        auto writingSuggestionsText = WebCore::createRenderer<RenderText>(RenderObject::Type::Text, renderer.document(), writingSuggestionData->content());
        m_updater.m_builder.attach(*newWritingSuggestionsRenderer, WTF::move(writingSuggestionsText));

        editor->setWritingSuggestionRenderer(*newWritingSuggestionsRenderer.get());
        m_updater.m_builder.attach(*parentForWritingSuggestions, WTF::move(newWritingSuggestionsRenderer), rendererAfterWritingSuggestions.get());

        if (!parentForWritingSuggestions) {
            destroyWritingSuggestionsIfNeeded();
            return;
        }

        RefPtr prefixNode = nodeBeforeWritingSuggestionsTextRenderer->textNode();
        if (!prefixNode) {
            ASSERT_NOT_REACHED();
            destroyWritingSuggestionsIfNeeded();
            return;
        }

        if (!suffix.isEmpty()) {
            auto suffixRenderer = WebCore::createRenderer<RenderText>(RenderObject::Type::Text, *prefixNode, suffix);
            m_updater.m_builder.attach(*parentForWritingSuggestions, WTF::move(suffixRenderer), rendererAfterWritingSuggestions.get());
        }
    }
}

}
