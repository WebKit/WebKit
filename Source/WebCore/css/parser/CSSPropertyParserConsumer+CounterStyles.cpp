/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyParserConsumer+CounterStyles.h"

#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+Integer.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static bool isPredefinedCounterStyle(CSSValueID valueID)
{
    // https://drafts.csswg.org/css-counter-styles-3/#predefined-counters

    return valueID >= CSSValueDisc && valueID <= CSSValueEthiopicNumeric;
}

RefPtr<CSSValue> consumeCounterStyle(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <counter-style> = <counter-style-name> | <symbols()>
    // https://drafts.csswg.org/css-counter-styles-3/#typedef-counter-style

    // FIXME: Implement support for `symbols()`.

    if (auto predefinedValues = consumeIdent(range, isPredefinedCounterStyle))
        return predefinedValues;
    if (context.propertySettings.cssCounterStyleAtRulesEnabled)
        return consumeCustomIdent(range);
    return nullptr;
}

AtomString consumeCounterStyleNameInPrelude(CSSParserTokenRange& prelude, CSSParserMode mode)
{
    // In the context of the prelude of an @counter-style rule, a <counter-style-name> must not be an
    // ASCII case-insensitive match for "decimal", "disc", "square", "circle", "disclosure-open" and
    // "disclosure-closed". No <counter-style-name>, prelude or not, may be an ASCII case-insensitive
    // match for "none".
    // https://drafts.csswg.org/css-counter-styles-3/#typedef-counter-style-name

    auto nameToken = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd())
        return AtomString();
    // Ensure this token is a valid <custom-ident>.
    if (nameToken.type() != IdentToken || !isValidCustomIdentifier(nameToken.id()))
        return AtomString();
    auto id = nameToken.id();
    if (identMatches<CSSValueNone>(id) || (!isUASheetBehavior(mode) && identMatches<CSSValueDecimal, CSSValueDisc, CSSValueCircle, CSSValueSquare, CSSValueDisclosureOpen, CSSValueDisclosureClosed>(id)))
        return AtomString();
    auto name = nameToken.value();
    return isPredefinedCounterStyle(nameToken.id()) ? name.convertToASCIILowercaseAtom() : name.toAtomString();
}

RefPtr<CSSValue> consumeCounterStyleName(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <counter-style-name> is a <custom-ident> that is not an ASCII case-insensitive match for "none".
    // https://drafts.csswg.org/css-counter-styles-3/#typedef-counter-style-name

    auto valueID = range.peek().id();
    if (valueID == CSSValueNone)
        return nullptr;
    // If the value is an ASCII case-insensitive match for any of the predefined counter styles, lowercase it.
    if (auto name = consumeCustomIdent(range, isPredefinedCounterStyle(valueID)))
        return name;
    return nullptr;
}

RefPtr<CSSValue> consumeCounterStyleSymbol(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <symbol> = [ <string> | <image> | <custom-ident> ]
    // https://drafts.csswg.org/css-counter-styles-3/#typedef-symbol

    if (auto string = consumeString(range))
        return string;
    if (auto customIdent = consumeCustomIdent(range))
        return customIdent;
    // There are inherent difficulties in supporting <image> symbols in @counter-styles, so gate them behind a
    // flag for now. https://bugs.webkit.org/show_bug.cgi?id=167645
    if (context.counterStyleAtRuleImageSymbolsEnabled) {
        if (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::GeneratedImage }))
            return image;
    }
    return nullptr;
}

RefPtr<CSSValue> consumeCounterStyleSystem(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'system'> = cyclic | numeric | alphabetic | symbolic | additive | [fixed <integer>?] | [ extends <counter-style-name> ]
    // https://drafts.csswg.org/css-counter-styles-3/#counter-style-system

    if (auto ident = consumeIdent<CSSValueCyclic, CSSValueNumeric, CSSValueAlphabetic, CSSValueSymbolic, CSSValueAdditive>(range))
        return ident;

    if (isUASheetBehavior(context.mode)) {
        auto internalKeyword = consumeIdent<
            CSSValueInternalDisclosureClosed,
            CSSValueInternalDisclosureOpen,
            CSSValueInternalSimplifiedChineseInformal,
            CSSValueInternalSimplifiedChineseFormal,
            CSSValueInternalTraditionalChineseInformal,
            CSSValueInternalTraditionalChineseFormal,
            CSSValueInternalEthiopicNumeric
        >(range);
        if (internalKeyword)
            return internalKeyword;
    }

    if (auto ident = consumeIdent<CSSValueFixed>(range)) {
        if (range.atEnd())
            return ident;
        // If we have the `fixed` keyword but the range is not at the end, the next token must be a integer.
        // If it's not, this value is invalid.
        auto firstSymbolValue = consumeInteger(range, context);
        if (!firstSymbolValue)
            return nullptr;
        return CSSValuePair::create(ident.releaseNonNull(), firstSymbolValue.releaseNonNull());
    }

    if (auto ident = consumeIdent<CSSValueExtends>(range)) {
        // There must be a `<counter-style-name>` following the `extends` keyword. If there isn't, this value is invalid.
        auto parsedCounterStyleName = consumeCounterStyleName(range, context);
        if (!parsedCounterStyleName)
            return nullptr;
        return CSSValuePair::create(ident.releaseNonNull(), parsedCounterStyleName.releaseNonNull());
    }
    return nullptr;
}

RefPtr<CSSValue> consumeCounterStyleNegative(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'negative'> = <symbol> <symbol>?
    // https://drafts.csswg.org/css-counter-styles-3/#counter-style-negative

    auto prependValue = consumeCounterStyleSymbol(range, context);
    if (!prependValue || range.atEnd())
        return prependValue;

    auto appendValue = consumeCounterStyleSymbol(range, context);
    if (!appendValue || !range.atEnd())
        return nullptr;

    return CSSValueList::createSpaceSeparated(prependValue.releaseNonNull(), appendValue.releaseNonNull());
}

RefPtr<CSSValue> consumeCounterStyleRange(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'range'> = [ [ <integer> | infinite ]{2} ]# | auto
    // https://drafts.csswg.org/css-counter-styles-3/#counter-style-range

    auto consumeCounterStyleRangeBound = [&](CSSParserTokenRange& range) -> RefPtr<CSSPrimitiveValue> {
        if (auto infinite = consumeIdent<CSSValueInfinite>(range))
            return infinite;
        if (auto integer = consumeInteger(range, context))
            return integer;
        return nullptr;
    };

    if (auto autoValue = consumeIdent<CSSValueAuto>(range))
        return autoValue;

    auto rangeList = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [&](auto& range) -> RefPtr<CSSValue> {
        auto lowerBound = consumeCounterStyleRangeBound(range);
        if (!lowerBound)
            return nullptr;
        auto upperBound = consumeCounterStyleRangeBound(range);
        if (!upperBound)
            return nullptr;

        // If the lower bound of any range is higher than the upper bound, the entire descriptor is invalid and must be
        // ignored.
        if (lowerBound->isInteger() && upperBound->isInteger() && lowerBound->resolveAsIntegerDeprecated() > upperBound->resolveAsIntegerDeprecated())
            return nullptr;

        return CSSValuePair::createNoncoalescing(lowerBound.releaseNonNull(), upperBound.releaseNonNull());
    });

    if (!range.atEnd() || !rangeList || !rangeList->length())
        return nullptr;
    return rangeList;
}

RefPtr<CSSValue> consumeCounterStylePad(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'pad'> = <integer [0,∞]> && <symbol>
    // https://drafts.csswg.org/css-counter-styles-3/#counter-style-pad

    RefPtr<CSSPrimitiveValue> integer;
    RefPtr<CSSValue> symbol;
    while (!integer || !symbol) {
        if (!integer) {
            integer = consumeNonNegativeInteger(range, context);
            if (integer)
                continue;
        }
        if (!symbol) {
            symbol = consumeCounterStyleSymbol(range, context);
            if (symbol)
                continue;
        }
        return nullptr;
    }
    if (!range.atEnd())
        return nullptr;
    return CSSValueList::createSpaceSeparated(integer.releaseNonNull(), symbol.releaseNonNull());
}

RefPtr<CSSValue> consumeCounterStyleSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'symbols'> = <symbol>+
    // https://drafts.csswg.org/css-counter-styles-3/#descdef-counter-style-symbols

    CSSValueListBuilder symbols;
    while (!range.atEnd()) {
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!symbol)
            return nullptr;
        symbols.append(symbol.releaseNonNull());
    }
    if (symbols.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(symbols));
}

RefPtr<CSSValue> consumeCounterStyleAdditiveSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'additive-symbols'> = [ <integer [0,∞]> && <symbol> ]#
    // https://drafts.csswg.org/css-counter-styles-3/#descdef-counter-style-additive-symbols

    std::optional<int> lastWeight;
    auto values = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [&lastWeight](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto integer = consumeNonNegativeInteger(range, context);
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!integer) {
            if (!symbol)
                return nullptr;
            integer = consumeNonNegativeInteger(range, context);
            if (!integer)
                return nullptr;
        }
        if (!symbol)
            return nullptr;

        // Additive tuples must be specified in order of strictly descending weight.
        auto weight = integer->resolveAsIntegerDeprecated();
        if (lastWeight && !(weight < lastWeight))
            return nullptr;
        lastWeight = weight;

        return CSSValuePair::create(integer.releaseNonNull(), symbol.releaseNonNull());
    }, context);

    if (!range.atEnd() || !values || !values->length())
        return nullptr;
    return values;
}

RefPtr<CSSValue> consumeCounterStyleSpeakAs(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'speak-as'> = auto | bullets | numbers | words | spell-out | <counter-style-name>
    // https://drafts.csswg.org/css-counter-styles-3/#counter-style-speak-as

    if (auto speakAsIdent = consumeIdent<CSSValueAuto, CSSValueBullets, CSSValueNumbers, CSSValueWords, CSSValueSpellOut>(range))
        return speakAsIdent;
    return consumeCounterStyleName(range, context);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
