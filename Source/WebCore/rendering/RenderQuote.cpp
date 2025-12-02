/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
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

#include "RenderBoxModelObjectInlines.h"
#include "RenderObjectInlines.h"
#include "RenderTextFragment.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include <array>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
using namespace WTF::Unicode;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderQuote);

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

RenderQuote::RenderQuote(Document& document, RenderStyle&& style, QuoteType quote)
    : RenderInline(Type::Quote, document, WTFMove(style))
    , m_type(quote)
    , m_text(emptyString())
{
    ASSERT(isRenderQuote());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderQuote::~RenderQuote() = default;

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

static SubtagComparison subtagCompare(std::span<const char> key, std::span<const char> range)
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

static std::strong_ordering quoteTableLanguageComparisonFunction(const QuotesForLanguage& key, std::span<const QuotesForLanguage> range)
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

static const QuotesForLanguage* binaryFindQuotes(const QuotesForLanguage& key, std::span<const QuotesForLanguage> subrange = quoteTable)
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
        [[fallthrough]];
    case QuoteType::CloseQuote:
        if (!style().quotes().isAuto())
            return isOpenQuote ? style().quotes().openQuote(m_depth).impl() : style().quotes().closeQuote(m_depth).impl();
        if (const auto* quotes = quotesForLanguage(Style::toPlatform(style().computedLocale())))
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
