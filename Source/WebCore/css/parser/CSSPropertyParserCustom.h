// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2022 Apple Inc. All rights reserved.
// Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSBorderRadius.h"
#include "CSSFontStyleRangeValue.h"
#include "CSSFontVariantLigaturesParser.h"
#include "CSSFontVariantNumericParser.h"
#include "CSSFunctionValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveNumericTypes+CSSValueCreation.h"
#include "CSSPropertyParserConsumer+Align.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+Animations.h"
#include "CSSPropertyParserConsumer+AppleVisualEffect.h"
#include "CSSPropertyParserConsumer+Attr.h"
#include "CSSPropertyParserConsumer+Background.h"
#include "CSSPropertyParserConsumer+Box.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+ColorAdjust.h"
#include "CSSPropertyParserConsumer+Content.h"
#include "CSSPropertyParserConsumer+CounterStyles.h"
#include "CSSPropertyParserConsumer+Display.h"
#include "CSSPropertyParserConsumer+Easing.h"
#include "CSSPropertyParserConsumer+Filter.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSPropertyParserConsumer+Grid.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+Inline.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+Lists.h"
#include "CSSPropertyParserConsumer+Masking.h"
#include "CSSPropertyParserConsumer+Motion.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+PositionTry.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+Ratio.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+SVG.h"
#include "CSSPropertyParserConsumer+ScrollSnap.h"
#include "CSSPropertyParserConsumer+Scrollbars.h"
#include "CSSPropertyParserConsumer+Shapes.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserConsumer+Syntax.h"
#include "CSSPropertyParserConsumer+TextDecoration.h"
#include "CSSPropertyParserConsumer+TimeDefinitions.h"
#include "CSSPropertyParserConsumer+Timeline.h"
#include "CSSPropertyParserConsumer+Transform.h"
#include "CSSPropertyParserConsumer+Transitions.h"
#include "CSSPropertyParserConsumer+UI.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserConsumer+UnicodeRange.h"
#include "CSSPropertyParserConsumer+ViewTransition.h"
#include "CSSPropertyParserConsumer+WillChange.h"
#include "CSSPropertyParserResult.h"
#include "CSSPropertyParsing.h"
#include "CSSQuadValue.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "FontFace.h"
#include "Rect.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "StyleURL.h"
#include "TimingFunction.h"
#include <memory>
#include <wtf/IndexedRange.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace CSS {

using namespace CSSPropertyParserHelpers;

class PropertyParserCustom {
public:
    // MARK: - Shorthand Parsing

    static bool consumeStandardSpaceSeparatedShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeSingleShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeCoalescingPairShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeCoalescingQuadShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);

    static bool consumeBorderShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderInlineShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderBlockShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeAnimationShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBackgroundShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBackgroundPositionShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitBackgroundSizeShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeMaskShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeMaskPositionShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeOverflowShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeColumnsShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeGridItemPositionShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeGridTemplateShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeGridShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeGridAreaShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeAlignShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBlockStepShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeFontShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeFontVariantShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeFontSynthesisShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeTextDecorationSkipShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderSpacingShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderRadiusShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitBorderRadiusShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderImageShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitBorderImageShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeMaskBorderShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitMaskBoxImageShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeFlexShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumePageBreakAfterShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumePageBreakBeforeShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumePageBreakInsideShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitColumnBreakAfterShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitColumnBreakBeforeShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitColumnBreakInsideShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitTextOrientationShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeTransformOriginShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumePerspectiveOriginShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitPerspectiveShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeOffsetShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeListStyleShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeOverscrollBehaviorShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeContainerShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeContainIntrinsicSizeShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeAnimationRangeShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeScrollTimelineShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeViewTimelineShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeLineClampShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeTextBoxShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeTextWrapShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWhiteSpaceShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumePositionTryShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeMarkerShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
};

struct BorderShorthandComponents {
    RefPtr<CSSValue> width;
    RefPtr<CSSValue> style;
    RefPtr<CSSValue> color;
};

inline std::optional<BorderShorthandComponents> consumeBorderShorthandComponents(CSSParserTokenRange& range, PropertyParserState& state)
{
    BorderShorthandComponents components { };

    while (!components.width || !components.style || !components.color) {
        if (!components.width) {
            components.width = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyBorderLeftWidth, state);
            if (components.width)
                continue;
        }
        if (!components.style) {
            components.style = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyBorderLeftStyle, state);
            if (components.style)
                continue;
        }
        if (!components.color) {
            components.color = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyBorderLeftColor, state);
            if (components.color)
                continue;
        }
        break;
    }

    if (!components.width && !components.style && !components.color)
        return { };

    if (!range.atEnd())
        return { };

    return components;
}

inline CSSValueID mapFromPageBreakBetween(CSSValueID value)
{
    if (value == CSSValueAlways)
        return CSSValuePage;
    if (value == CSSValueAuto || value == CSSValueAvoid || value == CSSValueLeft || value == CSSValueRight)
        return value;
    return CSSValueInvalid;
}

inline CSSValueID mapFromColumnBreakBetween(CSSValueID value)
{
    if (value == CSSValueAlways)
        return CSSValueColumn;
    if (value == CSSValueAuto)
        return value;
    if (value == CSSValueAvoid)
        return CSSValueAvoidColumn;
    return CSSValueInvalid;
}

inline CSSValueID mapFromColumnRegionOrPageBreakInside(CSSValueID value)
{
    if (value == CSSValueAuto || value == CSSValueAvoid)
        return value;
    return CSSValueInvalid;
}

inline bool PropertyParserCustom::consumeStandardSpaceSeparatedShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(state.currentProperty == shorthand.id());
    ASSERT(shorthand.length() <= 6); // Existing shorthands have at most 6 longhands.
    std::array<RefPtr<CSSValue>, 6> longhands;
    auto shorthandProperties = shorthand.properties();
    do {
        bool foundLonghand = false;
        for (size_t i = 0; !foundLonghand && i < shorthand.length(); ++i) {
            if (longhands[i])
                continue;

            longhands[i] = CSSPropertyParsing::parseStylePropertyLonghand(range, shorthandProperties[i], state);
            if (longhands[i])
                foundLonghand = true;
        }
        if (!foundLonghand)
            return false;
    } while (!range.atEnd());

    for (size_t i = 0; i < shorthand.length(); ++i)
        result.addPropertyForCurrentShorthand(state, shorthandProperties[i], WTFMove(longhands[i]));
    return true;
}

inline bool PropertyParserCustom::consumeSingleShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 1);

    auto longhands = shorthand.properties();
    auto value = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[0], state);
    auto line = CSSPropertyParsing::consumeTextDecorationLine(range);
    if (!value || !range.atEnd())
        return false;
    result.addPropertyForCurrentShorthand(state, longhands[0], value.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeCoalescingPairShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(state.currentProperty == shorthand.id());
    ASSERT(shorthand.length() == 2);
    auto longhands = shorthand.properties();
    RefPtr start = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[0], state);
    if (!start)
        return false;

    RefPtr end = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[1], state);
    auto endImplicit = !end ? IsImplicit::Yes : IsImplicit::No;
    if (endImplicit == IsImplicit::Yes)
        end = start;

    result.addPropertyForCurrentShorthand(state, longhands[0], start.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], end.releaseNonNull(), endImplicit);
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeCoalescingQuadShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(state.currentProperty == shorthand.id());
    ASSERT(shorthand.length() == 4);
    auto longhands = shorthand.properties();
    RefPtr top = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[0], state);
    if (!top)
        return false;

    RefPtr right = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[1], state);
    RefPtr<CSSValue> bottom;
    RefPtr<CSSValue> left;
    if (right) {
        bottom = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[2], state);
        if (bottom)
            left = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[3], state);
    }

    auto rightImplicit = !right ? IsImplicit::Yes : IsImplicit::No;
    auto bottomImplicit = !bottom ? IsImplicit::Yes : IsImplicit::No;
    auto leftImplicit = !left ? IsImplicit::Yes : IsImplicit::No;

    if (rightImplicit == IsImplicit::Yes)
        right = top;
    if (bottomImplicit == IsImplicit::Yes)
        bottom = top;
    if (leftImplicit == IsImplicit::Yes)
        left = right;

    result.addPropertyForCurrentShorthand(state, longhands[0], top.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], right.releaseNonNull(), rightImplicit);
    result.addPropertyForCurrentShorthand(state, longhands[2], bottom.releaseNonNull(), bottomImplicit);
    result.addPropertyForCurrentShorthand(state, longhands[3], left.releaseNonNull(), leftImplicit);
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeFontShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(range.peek().id())) {
        auto systemFont = range.consumeIncludingWhitespace().id();
        if (!range.atEnd())
            return false;

        // We can't store properties (weight, size, etc.) of the system font here,
        // since those values can change (e.g. accessibility font sizes, or accessibility bold).
        // Parsing (correctly) doesn't re-run in response to updateStyleAfterChangeInEnvironment().
        // Instead, we store sentinel values, later replaced by environment-sensitive values
        // inside Style::BuilderCustom and Style::BuilderConverter.
        result.addPropertyForAllLonghandsOfCurrentShorthand(state, CSSPrimitiveValue::create(systemFont), IsImplicit::Yes);
        return true;
    }

    CSSParserTokenRangeGuard guard { range };

    std::array<RefPtr<CSSValue>, 7> values;
    auto& fontStyle = values[0];
    auto& fontVariantCaps = values[1];
    auto& fontWeight = values[2];
    auto& fontWidth = values[3];
    auto& fontSize = values[4];
    auto& lineHeight = values[5];
    auto& fontFamily = values[6];

    // Optional font-style, font-variant, font-width and font-weight, in any order.
    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        if (consumeIdent<CSSValueNormal>(range))
            continue;
        if (!fontStyle && (fontStyle = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontStyle, state)))
            continue;
        if (!fontVariantCaps && (fontVariantCaps = consumeIdent<CSSValueSmallCaps>(range)))
            continue;
        if (!fontWeight && (fontWeight = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontWeight, state)))
            continue;
        if (!fontWidth && (fontWidth = CSSPropertyParsing::consumeFontWidthAbsolute(range)))
            continue;
        break;
    }

    if (range.atEnd())
        return false;

    fontSize = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontSize, state);
    if (!fontSize || range.atEnd())
        return false;

    if (consumeSlashIncludingWhitespace(range)) {
        if (!consumeIdent<CSSValueNormal>(range)) {
            lineHeight = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyLineHeight, state);
            if (!lineHeight)
                return false;
        }
        if (range.atEnd())
            return false;
    }

    fontFamily = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontFamily, state);
    if (!fontFamily || !range.atEnd())
        return false;

    guard.commit();

    auto shorthandProperties = shorthand.properties();
    for (auto [value, longhand] : zippedRange(values, shorthandProperties.first(values.size())))
        result.addPropertyForCurrentShorthand(state, longhand, WTFMove(value), IsImplicit::Yes);
    for (auto longhand : shorthandProperties.subspan(values.size()))
        result.addPropertyForCurrentShorthand(state, longhand, nullptr, IsImplicit::Yes);

    return true;
}

inline bool PropertyParserCustom::consumeFontVariantShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (identMatches<CSSValueNormal, CSSValueNone>(range.peek().id())) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantLigatures, consumeIdent(range));
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantCaps, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantAlternates, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantNumeric, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEastAsian, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantPosition, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEmoji, nullptr);
        return range.atEnd();
    }

    RefPtr<CSSValue> capsValue;
    RefPtr<CSSValue> alternatesValue;
    RefPtr<CSSValue> positionValue;
    RefPtr<CSSValue> eastAsianValue;
    RefPtr<CSSValue> emojiValue;
    CSSFontVariantLigaturesParser ligaturesParser;
    CSSFontVariantNumericParser numericParser;
    auto implicitLigatures = IsImplicit::Yes;
    auto implicitNumeric = IsImplicit::Yes;
    do {
        if (range.peek().id() == CSSValueNormal)
            return false;

        if (!capsValue && (capsValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontVariantCaps, state)))
            continue;

        if (!positionValue && (positionValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontVariantPosition, state)))
            continue;

        if (!alternatesValue && (alternatesValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontVariantAlternates, state)))
            continue;

        auto ligaturesParseResult = ligaturesParser.consumeLigature(range);
        auto numericParseResult = numericParser.consumeNumeric(range);
        if (ligaturesParseResult == CSSFontVariantLigaturesParser::ParseResult::ConsumedValue) {
            implicitLigatures = IsImplicit::No;
            continue;
        }
        if (numericParseResult == CSSFontVariantNumericParser::ParseResult::ConsumedValue) {
            implicitNumeric = IsImplicit::No;
            continue;
        }

        if (ligaturesParseResult == CSSFontVariantLigaturesParser::ParseResult::DisallowedValue
            || numericParseResult == CSSFontVariantNumericParser::ParseResult::DisallowedValue)
            return false;

        if (!eastAsianValue && (eastAsianValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontVariantEastAsian, state)))
            continue;

        if (state.context.propertySettings.cssFontVariantEmojiEnabled && !emojiValue && (emojiValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyFontVariantEmoji, state)))
            continue;

        // Saw some value that didn't match anything else.
        return false;
    } while (!range.atEnd());

    result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantLigatures, ligaturesParser.finalizeValue().releaseNonNull(), implicitLigatures);
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantCaps, WTFMove(capsValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantAlternates, WTFMove(alternatesValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantNumeric, numericParser.finalizeValue().releaseNonNull(), implicitNumeric);
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEastAsian, WTFMove(eastAsianValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantPosition, WTFMove(positionValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEmoji, WTFMove(emojiValue));
    return true;
}

inline bool PropertyParserCustom::consumeFontSynthesisShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // none | [ weight || style || small-caps ]
    if (range.peek().id() == CSSValueNone) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisSmallCaps, consumeIdent(range).releaseNonNull());
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisStyle, CSSPrimitiveValue::create(CSSValueNone));
        result.addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisWeight, CSSPrimitiveValue::create(CSSValueNone));
        return range.atEnd();
    }

    bool foundWeight = false;
    bool foundStyle = false;
    bool foundSmallCaps = false;

    auto checkAndMarkExistence = [](bool* found) {
        if (*found)
            return false;
        return *found = true;
    };

    while (!range.atEnd()) {
        RefPtr ident = consumeIdent<CSSValueWeight, CSSValueStyle, CSSValueSmallCaps>(range);
        if (!ident)
            return false;
        switch (ident->valueID()) {
        case CSSValueWeight:
            if (!checkAndMarkExistence(&foundWeight))
                return false;
            break;
        case CSSValueStyle:
            if (!checkAndMarkExistence(&foundStyle))
                return false;
            break;
        case CSSValueSmallCaps:
            if (!checkAndMarkExistence(&foundSmallCaps))
                return false;
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisWeight, CSSPrimitiveValue::create(foundWeight ? CSSValueAuto : CSSValueNone));
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisStyle, CSSPrimitiveValue::create(foundStyle ? CSSValueAuto : CSSValueNone));
    result.addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisSmallCaps, CSSPrimitiveValue::create(foundSmallCaps ? CSSValueAuto : CSSValueNone));
    return true;
}

inline bool PropertyParserCustom::consumeTextDecorationSkipShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (auto skip = consumeIdentRaw<CSSValueNone, CSSValueAuto, CSSValueInk>(range)) {
        switch (*skip) {
        case CSSValueNone:
            result.addPropertyForCurrentShorthand(state, CSSPropertyTextDecorationSkipInk, CSSPrimitiveValue::create(CSSValueNone));
            return range.atEnd();
        case CSSValueAuto:
        case CSSValueInk:
            result.addPropertyForCurrentShorthand(state, CSSPropertyTextDecorationSkipInk, CSSPrimitiveValue::create(CSSValueAuto));
            return range.atEnd();
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

inline bool PropertyParserCustom::consumeBorderSpacingShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr horizontalSpacing = CSSPrimitiveValueResolver<Length<Nonnegative>>::consumeAndResolve(range, state);
    if (!horizontalSpacing)
        return false;
    RefPtr verticalSpacing = horizontalSpacing;
    if (!range.atEnd())
        verticalSpacing = CSSPrimitiveValueResolver<Length<Nonnegative>>::consumeAndResolve(range, state);
    if (!verticalSpacing || !range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyWebkitBorderHorizontalSpacing, horizontalSpacing.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyWebkitBorderVerticalSpacing, verticalSpacing.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeColumnsShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr<CSSValue> columnWidth;
    RefPtr<CSSValue> columnCount;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !range.atEnd(); ++propertiesParsed) {
        if (range.peek().id() == CSSValueAuto) {
            // 'auto' is a valid value for any of the two longhands, and at this point
            // we don't know which one(s) it is meant for. We need to see if there are other values first.
            consumeIdent(range);
        } else {
            if (!columnWidth && (columnWidth = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyColumnWidth, state)))
                continue;
            if (!columnCount && (columnCount = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyColumnCount, state)))
                continue;
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyColumnWidth, WTFMove(columnWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyColumnCount, WTFMove(columnCount));
    return true;
}

inline bool PropertyParserCustom::consumeFlexShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // <'flex'>        = none | [ <'flex-grow'> <'flex-shrink'>? || <'flex-basis'> ]
    // <'flex-grow'>   = <number [0,∞]>
    //     NOTE: When omitted from shorthand, it is set to 1.
    // <'flex-shrink'> = <number [0,∞]>
    //     NOTE: When omitted from shorthand, it is set to 1.
    // <'flex-basis'>  = content | <'width'>
    //    NOTE: When omitted from shorthand, it is set to 0.
    // https://drafts.csswg.org/css-flexbox/#propdef-flex

    auto isFlexBasisIdent = [](CSSValueID id) {
        switch (id) {
        case CSSValueAuto:
        case CSSValueContent:
        case CSSValueIntrinsic:
        case CSSValueMinIntrinsic:
        case CSSValueMinContent:
        case CSSValueWebkitMinContent:
        case CSSValueMaxContent:
        case CSSValueWebkitMaxContent:
        case CSSValueWebkitFillAvailable:
        case CSSValueFitContent:
        case CSSValueWebkitFitContent:
            return true;
        default:
            return false;
        }
    };

    RefPtr<CSSPrimitiveValue> flexGrow;
    RefPtr<CSSPrimitiveValue> flexShrink;
    RefPtr<CSSPrimitiveValue> flexBasis;

    if (range.peek().id() == CSSValueNone) {
        flexGrow = CSSPrimitiveValue::create(0);
        flexShrink = CSSPrimitiveValue::create(0);
        flexBasis = CSSPrimitiveValue::create(CSSValueAuto);
        range.consumeIncludingWhitespace();
    } else {
        unsigned index = 0;
        while (!range.atEnd() && index++ < 3) {
            if (auto number = CSSPrimitiveValueResolver<Number<Nonnegative>>::consumeAndResolve(range, state)) {
                if (!flexGrow)
                    flexGrow = WTFMove(number);
                else if (!flexShrink)
                    flexShrink = WTFMove(number);
                else if (number->isZero() == true) // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                    flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
                else
                    return false;
            } else if (!flexBasis) {
                if (isFlexBasisIdent(range.peek().id()))
                    flexBasis = consumeIdent(range);
                if (!flexBasis)
                    flexBasis = CSSPrimitiveValueResolver<LengthPercentage<Nonnegative>>::consumeAndResolve(range, state);
                if (index == 2 && !range.atEnd())
                    return false;
            }
        }
        if (index == 0)
            return false;
        if (!flexGrow)
            flexGrow = CSSPrimitiveValue::create(1);
        if (!flexShrink)
            flexShrink = CSSPrimitiveValue::create(1);

        // FIXME: Using % here is a hack to work around intrinsic sizing implementation being
        // a mess (e.g., turned off for nested column flexboxes, failing to relayout properly even
        // if turned back on for nested columns, etc.). We have layout test coverage of both
        // scenarios.
        if (!flexBasis)
            flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PERCENTAGE);
    }

    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyFlexGrow, flexGrow.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyFlexShrink, flexShrink.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyFlexBasis, flexBasis.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeBorderShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderShorthandComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderWidth, WTFMove(components->width), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderStyle, WTFMove(components->style), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderColor, WTFMove(components->color), state.important);

    for (auto longhand : borderImageShorthand())
        result.addPropertyForCurrentShorthand(state, longhand, nullptr);
    return true;
}

inline bool PropertyParserCustom::consumeBorderInlineShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderShorthandComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderInlineWidth, WTFMove(components->width), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderInlineStyle, WTFMove(components->style), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderInlineColor, WTFMove(components->color), state.important);
    return true;
}

inline bool PropertyParserCustom::consumeBorderBlockShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderShorthandComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderBlockWidth, WTFMove(components->width), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderBlockStyle, WTFMove(components->style), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyBorderBlockColor, WTFMove(components->color), state.important);
    return true;
}

inline bool PropertyParserCustom::consumeBorderRadiusShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto borderRadius = consumeUnresolvedBorderRadius(range, state);
    if (!borderRadius)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderTopLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topLeft()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderTopRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomLeft()));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitBorderRadiusShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto borderRadius = consumeUnresolvedWebKitBorderRadius(range, state);
    if (!borderRadius)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderTopLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topLeft()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderTopRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomLeft()));
    return true;
}

inline bool PropertyParserCustom::consumeBorderImageShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderImageComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSource, WTFMove(components->source));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSlice, WTFMove(components->slice));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageWidth, WTFMove(components->width));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageOutset, WTFMove(components->outset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageRepeat, WTFMove(components->repeat));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitBorderImageShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // NOTE: -webkit-border-image has a legacy behavior that makes border image slices default to `fill`.
    // NOTE: -webkit-border-image has a legacy behavior that makes border image widths with length values also set the border widths.

    auto components = consumeBorderImageComponents(range, state, BorderImageSliceFillDefault::Yes, BorderImageWidthOverridesWidthForLength::Yes);
    if (!components)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSource, WTFMove(components->source));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSlice, WTFMove(components->slice));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageWidth, WTFMove(components->width));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageOutset, WTFMove(components->outset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBorderImageRepeat, WTFMove(components->repeat));
    return true;
}

inline bool PropertyParserCustom::consumeMaskBorderShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderImageComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSource, WTFMove(components->source));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSlice, WTFMove(components->slice));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderWidth, WTFMove(components->width));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderOutset, WTFMove(components->outset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderRepeat, WTFMove(components->repeat));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitMaskBoxImageShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // NOTE: -webkit-mask-box-image has a legacy behavior that makes border image slices default to `fill`.

    auto components = consumeBorderImageComponents(range, state, BorderImageSliceFillDefault::Yes);
    if (!components)
        return false;

    if (!components->slice)
        components->slice = CSSBorderImageSliceValue::create({ CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0) }, true);

    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSource, WTFMove(components->source));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSlice, WTFMove(components->slice));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderWidth, WTFMove(components->width));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderOutset, WTFMove(components->outset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderRepeat, WTFMove(components->repeat));
    return true;
}

inline bool PropertyParserCustom::consumePageBreakAfterShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromPageBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBreakAfter, CSSPrimitiveValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumePageBreakBeforeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromPageBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBreakBefore, CSSPrimitiveValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumePageBreakInsideShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromColumnRegionOrPageBreakInside(*keyword);
    if (value == CSSValueInvalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBreakInside, CSSPrimitiveValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitColumnBreakAfterShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).

    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromColumnBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBreakAfter, CSSPrimitiveValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitColumnBreakBeforeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).

    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromColumnBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBreakBefore, CSSPrimitiveValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitColumnBreakInsideShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).

    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromColumnRegionOrPageBreakInside(*keyword);
    if (value == CSSValueInvalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyBreakInside, CSSPrimitiveValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitTextOrientationShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // -webkit-text-orientation is a legacy shorthand for text-orientation.
    // The only difference is that it accepts 'sideways-right', which is mapped into 'sideways'.
    RefPtr<CSSPrimitiveValue> keyword;
    auto valueID = range.peek().id();
    if (valueID == CSSValueSidewaysRight) {
        keyword = CSSPrimitiveValue::create(CSSValueSideways);
        consumeIdentRaw(range);
    } else if (CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyTextOrientation, valueID, state))
        keyword = consumeIdent(range);
    if (!keyword || !range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyTextOrientation, keyword.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeAnimationShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    auto isValidAnimationPropertyList = [](CSSPropertyID property, const CSSValueListBuilder& valueList) {
        // If there is more than one <single-transition> in the shorthand, and any of the transitions
        // has none as the <single-transition-property>, then the declaration is invalid.
        if (property != CSSPropertyTransitionProperty || valueList.size() < 2)
            return true;
        for (auto& value : valueList) {
            if (isValueID(value, CSSValueNone))
                return false;
        }
        return true;
    };

    auto consumeAnimationValueForShorthand = [&](CSSPropertyID property) -> RefPtr<CSSValue> {
        switch (property) {
        case CSSPropertyAnimationDelay:
        case CSSPropertyTransitionDelay:
            return CSSPrimitiveValueResolver<Time<>>::consumeAndResolve(range, state);
        case CSSPropertyAnimationDirection:
            return CSSPropertyParsing::consumeSingleAnimationDirection(range);
        case CSSPropertyAnimationDuration:
            return CSSPropertyParsing::consumeSingleAnimationDuration(range, state);
        case CSSPropertyTransitionDuration:
            return CSSPrimitiveValueResolver<Time<Nonnegative>>::consumeAndResolve(range, state);
        case CSSPropertyAnimationFillMode:
            return CSSPropertyParsing::consumeSingleAnimationFillMode(range);
        case CSSPropertyAnimationIterationCount:
            return CSSPropertyParsing::consumeSingleAnimationIterationCount(range, state);
        case CSSPropertyAnimationName:
            return CSSPropertyParsing::consumeSingleAnimationName(range, state);
        case CSSPropertyAnimationPlayState:
            return CSSPropertyParsing::consumeSingleAnimationPlayState(range);
        case CSSPropertyAnimationComposition:
            return CSSPropertyParsing::consumeSingleAnimationComposition(range);
        case CSSPropertyAnimationTimeline:
        case CSSPropertyAnimationRangeStart:
        case CSSPropertyAnimationRangeEnd:
            return nullptr; // reset-only longhands
        case CSSPropertyTransitionProperty:
            return consumeSingleTransitionPropertyOrNone(range, state);
        case CSSPropertyAnimationTimingFunction:
        case CSSPropertyTransitionTimingFunction:
            return consumeEasingFunction(range, state);
        case CSSPropertyTransitionBehavior:
            return CSSPropertyParsing::consumeTransitionBehaviorValue(range);
        default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    };

    auto shorthandProperties = shorthand.properties();

    const size_t longhandCount = shorthand.length();
    const size_t maxLonghandCount = 11;
    std::array<CSSValueListBuilder, maxLonghandCount> longhands;
    ASSERT(longhandCount <= maxLonghandCount);

    auto isResetOnlyLonghand = [](CSSPropertyID longhand) {
        switch (longhand) {
        case CSSPropertyAnimationTimeline:
        case CSSPropertyAnimationRangeStart:
        case CSSPropertyAnimationRangeEnd:
            return true;
        default:
            return false;
        }
    };

    do {
        std::array<bool, maxLonghandCount> parsedLonghand = { };
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                if (auto value = consumeAnimationValueForShorthand(shorthandProperties[i])) {
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    longhands[i].append(*value);
                    break;
                }
            }
            if (!foundProperty)
                return false;
        } while (!range.atEnd() && range.peek().type() != CommaToken);

        for (size_t i = 0; i < longhandCount; ++i) {
            if (!parsedLonghand[i] && !isResetOnlyLonghand(shorthandProperties[i]))
                longhands[i].append(Ref { CSSPrimitiveValue::implicitInitialValue() });
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(range));

    for (size_t i = 0; i < longhandCount; ++i) {
        if (!isValidAnimationPropertyList(shorthandProperties[i], longhands[i]))
            return false;
    }

    for (size_t i = 0; i < longhandCount; ++i) {
        auto& list = longhands[i];
        if (list.isEmpty()) // reset-only property
            result.addPropertyForCurrentShorthand(state, shorthandProperties[i], nullptr);
        else
            result.addPropertyForCurrentShorthand(state, shorthandProperties[i], CSSValueList::createCommaSeparated(WTFMove(list)));
    }

    return range.atEnd();
}

inline bool PropertyParserCustom::consumeBackgroundShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.id() == state.currentProperty);

    auto consumeBackgroundComponent = [&](CSSPropertyID property) -> RefPtr<CSSValue> {
        switch (property) {
        // background-*
        case CSSPropertyBackgroundClip:
            return CSSPropertyParsing::consumeSingleBackgroundClip(range, state);
        case CSSPropertyBackgroundBlendMode:
            return CSSPropertyParsing::consumeSingleBackgroundBlendMode(range);
        case CSSPropertyBackgroundAttachment:
            return CSSPropertyParsing::consumeSingleBackgroundAttachment(range);
        case CSSPropertyBackgroundOrigin:
            return CSSPropertyParsing::consumeSingleBackgroundOrigin(range);
        case CSSPropertyBackgroundImage:
            return CSSPropertyParsing::consumeSingleBackgroundImage(range, state);
        case CSSPropertyBackgroundRepeat:
            return CSSPropertyParsing::consumeSingleBackgroundRepeat(range, state);
        case CSSPropertyBackgroundPositionX:
            return CSSPropertyParsing::consumeSingleBackgroundPositionX(range, state);
        case CSSPropertyBackgroundPositionY:
            return CSSPropertyParsing::consumeSingleBackgroundPositionY(range, state);
        case CSSPropertyBackgroundSize:
            return consumeSingleBackgroundSize(range, state);
        case CSSPropertyBackgroundColor:
            return consumeColor(range, state);

        // mask-*
        case CSSPropertyMaskComposite:
            return CSSPropertyParsing::consumeSingleMaskComposite(range);
        case CSSPropertyMaskOrigin:
            return CSSPropertyParsing::consumeSingleMaskOrigin(range);
        case CSSPropertyMaskClip:
            return CSSPropertyParsing::consumeSingleMaskClip(range);
        case CSSPropertyMaskImage:
            return CSSPropertyParsing::consumeSingleMaskImage(range, state);
        case CSSPropertyMaskMode:
            return CSSPropertyParsing::consumeSingleMaskMode(range);
        case CSSPropertyMaskRepeat:
            return CSSPropertyParsing::consumeSingleMaskRepeat(range, state);
        case CSSPropertyMaskSize:
            return consumeSingleMaskSize(range, state);

        // -webkit-background-*
        case CSSPropertyWebkitBackgroundSize:
            return consumeSingleWebkitBackgroundSize(range, state);
        case CSSPropertyWebkitBackgroundClip:
            return CSSPropertyParsing::consumeSingleWebkitBackgroundClip(range);
        case CSSPropertyWebkitBackgroundOrigin:
            return CSSPropertyParsing::consumeSingleWebkitBackgroundOrigin(range);

        // -webkit-mask-*
        case CSSPropertyWebkitMaskClip:
            return CSSPropertyParsing::consumeSingleWebkitMaskClip(range);
        case CSSPropertyWebkitMaskComposite:
            return CSSPropertyParsing::consumeSingleWebkitMaskComposite(range);
        case CSSPropertyWebkitMaskSourceType:
            return CSSPropertyParsing::consumeSingleWebkitMaskSourceType(range);
        case CSSPropertyWebkitMaskPositionX:
            return CSSPropertyParsing::consumeSingleWebkitMaskPositionX(range, state);
        case CSSPropertyWebkitMaskPositionY:
            return CSSPropertyParsing::consumeSingleWebkitMaskPositionY(range, state);

        default:
            return nullptr;
        };
    };

    auto shorthandProperties = shorthand.properties();
    unsigned longhandCount = shorthand.length();

    // mask resets mask-border properties outside of this method.
    if (shorthand.id() == CSSPropertyMask)
        longhandCount -= maskBorderShorthand().length();

    std::array<CSSValueListBuilder, 10> longhands;
    ASSERT(longhandCount <= 10);

    do {
        std::array<bool, 10> parsedLonghand = { };
        bool lastParsedWasPosition = false;
        bool clipIsBorderArea = false;
        RefPtr<CSSValue> originValue;
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                RefPtr<CSSValue> value;
                RefPtr<CSSValue> valueY;
                CSSPropertyID property = shorthandProperties[i];

                if (property == CSSPropertyBackgroundPositionX || property == CSSPropertyWebkitMaskPositionX) {
                    // Note: This assumes y properties (for example background-position-y) follow the x properties in the shorthand array.
                    auto position = consumeBackgroundPositionUnresolved(range, state);
                    if (!position)
                        continue;
                    auto [positionX, positionY] = split(WTFMove(*position));
                    value = CSSPositionXValue::create(WTFMove(positionX));
                    valueY = CSSPositionYValue::create(WTFMove(positionY));
                } else if (property == CSSPropertyBackgroundSize) {
                    if (!consumeSlashIncludingWhitespace(range))
                        continue;
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleBackgroundSize(range, state);
                    if (!value)
                        return false;
                } else if (property == CSSPropertyMaskSize) {
                    if (!consumeSlashIncludingWhitespace(range))
                        continue;
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleMaskSize(range, state);
                    if (!value)
                        return false;
                } else if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY) {
                    continue;
                } else {
                    value = consumeBackgroundComponent(property);
                }
                if (value) {
                    if (property == CSSPropertyBackgroundOrigin || property == CSSPropertyMaskOrigin)
                        originValue = value;
                    else if (property == CSSPropertyBackgroundClip)
                        clipIsBorderArea = value->valueID() == CSSValueBorderArea;
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    longhands[i].append(value.releaseNonNull());
                    lastParsedWasPosition = valueY;
                    if (valueY) {
                        parsedLonghand[i + 1] = true;
                        longhands[i + 1].append(valueY.releaseNonNull());
                    }
                }
            }
            if (!foundProperty)
                return false;
        } while (!range.atEnd() && range.peek().type() != CommaToken);

        for (size_t i = 0; i < longhandCount; ++i) {
            auto property = shorthandProperties[i];
            if (property == CSSPropertyBackgroundColor && !range.atEnd()) {
                if (parsedLonghand[i])
                    return false; // Colors are only allowed in the last layer.
                continue;
            }
            if ((property == CSSPropertyBackgroundClip || property == CSSPropertyMaskClip || property == CSSPropertyWebkitMaskClip) && !parsedLonghand[i] && originValue) {
                longhands[i].append(originValue.releaseNonNull());
                continue;
            }
            if (clipIsBorderArea && (property == CSSPropertyBackgroundOrigin) && !parsedLonghand[i]) {
                longhands[i].append(CSSPrimitiveValue::create(CSSValueBorderBox));
                continue;
            }
            if (!parsedLonghand[i])
                longhands[i].append(Ref { CSSPrimitiveValue::implicitInitialValue() });
        }
    } while (consumeCommaIncludingWhitespace(range));
    if (!range.atEnd())
        return false;

    for (size_t i = 0; i < longhandCount; ++i) {
        auto property = shorthandProperties[i];
        if (longhands[i].size() == 1)
            result.addPropertyForCurrentShorthand(state, property, WTFMove(longhands[i][0]));
        else
            result.addPropertyForCurrentShorthand(state, property, CSSValueList::createCommaSeparated(WTFMove(longhands[i])));
    }
    return true;
}

inline bool PropertyParserCustom::consumeBackgroundPositionShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.id() == state.currentProperty);

    CSSValueListBuilder x;
    CSSValueListBuilder y;
    do {
        auto position = consumeBackgroundPositionUnresolved(range, state);
        if (!position)
            return false;
        auto [positionX, positionY] = split(WTFMove(*position));
        x.append(CSSPositionXValue::create(WTFMove(positionX)));
        y.append(CSSPositionYValue::create(WTFMove(positionY)));
    } while (consumeCommaIncludingWhitespace(range));

    if (!range.atEnd())
        return false;

    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;
    if (x.size() == 1) {
        resultX = WTFMove(x[0]);
        resultY = WTFMove(y[0]);
    } else {
        resultX = CSSValueList::createCommaSeparated(WTFMove(x));
        resultY = CSSValueList::createCommaSeparated(WTFMove(y));
    }

    auto longhands = shorthand.properties();
    result.addPropertyForCurrentShorthand(state, longhands[0], resultX.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], resultY.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeWebkitBackgroundSizeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto backgroundSize = consumeListSeparatedBy<',', OneOrMore, ListOptimization::SingleValue>(range, [](auto& range, auto& state) {
        return consumeSingleWebkitBackgroundSize(range, state);
    }, state);
    if (!backgroundSize || !range.atEnd())
        return false;
    result.addPropertyForCurrentShorthand(state, CSSPropertyBackgroundSize, backgroundSize.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeMaskShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    if (!consumeBackgroundShorthand(range, state, shorthand, result))
        return false;
    for (auto longhand : maskBorderShorthand())
        result.addPropertyForCurrentShorthand(state, longhand, nullptr);
    return true;
}

inline bool PropertyParserCustom::consumeMaskPositionShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    CSSValueListBuilder x;
    CSSValueListBuilder y;
    do {
        auto position = consumePositionUnresolved(range, state);
        if (!position)
            return false;
        auto [positionX, positionY] = split(WTFMove(*position));
        x.append(CSSPositionXValue::create(WTFMove(positionX)));
        y.append(CSSPositionYValue::create(WTFMove(positionY)));
    } while (consumeCommaIncludingWhitespace(range));

    if (!range.atEnd())
        return false;

    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;
    if (x.size() == 1) {
        resultX = WTFMove(x[0]);
        resultY = WTFMove(y[0]);
    } else {
        resultX = CSSValueList::createCommaSeparated(WTFMove(x));
        resultY = CSSValueList::createCommaSeparated(WTFMove(y));
    }

    auto longhands = shorthand.properties();
    result.addPropertyForCurrentShorthand(state, longhands[0], resultX.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], resultY.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeOverflowShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSValueID xValueID = range.consumeIncludingWhitespace().id();
    if (!CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyOverflowY, xValueID, state))
        return false;

    CSSValueID yValueID;
    if (range.atEnd()) {
        yValueID = xValueID;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (xValueID == CSSValueWebkitPagedX || xValueID == CSSValueWebkitPagedY)
            xValueID = CSSValueAuto;
    } else
        yValueID = range.consumeIncludingWhitespace().id();

    if (!CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyOverflowY, yValueID, state))
        return false;
    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyOverflowX, CSSPrimitiveValue::create(xValueID));
    result.addPropertyForCurrentShorthand(state, CSSPropertyOverflowY, CSSPrimitiveValue::create(yValueID));
    return true;
}

inline bool PropertyParserCustom::consumeGridItemPositionShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.id() == state.currentProperty);
    ASSERT(shorthand.length() == 2);

    RefPtr startValue = consumeGridLine(range, state);
    if (!startValue)
        return false;

    RefPtr<CSSValue> endValue;
    if (consumeSlashIncludingWhitespace(range)) {
        endValue = consumeGridLine(range, state);
        if (!endValue)
            return false;
    } else {
        endValue = isCustomIdentValue(*startValue) ? startValue : CSSPrimitiveValue::create(CSSValueAuto);
    }
    if (!range.atEnd())
        return false;

    auto longhands = shorthand.properties();
    result.addPropertyForCurrentShorthand(state, longhands[0], startValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], endValue.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeGridAreaShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr rowStartValue = consumeGridLine(range, state);
    if (!rowStartValue)
        return false;
    RefPtr<CSSValue> columnStartValue;
    RefPtr<CSSValue> rowEndValue;
    RefPtr<CSSValue> columnEndValue;
    if (consumeSlashIncludingWhitespace(range)) {
        columnStartValue = consumeGridLine(range, state);
        if (!columnStartValue)
            return false;
        if (consumeSlashIncludingWhitespace(range)) {
            rowEndValue = consumeGridLine(range, state);
            if (!rowEndValue)
                return false;
            if (consumeSlashIncludingWhitespace(range)) {
                columnEndValue = consumeGridLine(range, state);
                if (!columnEndValue)
                    return false;
            }
        }
    }
    if (!range.atEnd())
        return false;
    if (!columnStartValue)
        columnStartValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSPrimitiveValue::create(CSSValueAuto);
    if (!rowEndValue)
        rowEndValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSPrimitiveValue::create(CSSValueAuto);
    if (!columnEndValue)
        columnEndValue = isCustomIdentValue(*columnStartValue) ? columnStartValue : CSSPrimitiveValue::create(CSSValueAuto);

    result.addPropertyForCurrentShorthand(state, CSSPropertyGridRowStart, rowStartValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridColumnStart, columnStartValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridRowEnd, rowEndValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridColumnEnd, columnEndValue.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeGridTemplateShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSParserTokenRange rangeCopy = range;
    RefPtr<CSSValue> rowsValue = consumeIdent<CSSValueNone>(range);

    // 1- 'none' case.
    if (rowsValue && range.atEnd()) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, CSSPrimitiveValue::create(CSSValueNone));
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, CSSPrimitiveValue::create(CSSValueNone));
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSPrimitiveValue::create(CSSValueNone));
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns>
    if (!rowsValue)
        rowsValue = consumeGridTrackList(range, state, GridTemplate);

    if (rowsValue) {
        if (!consumeSlashIncludingWhitespace(range))
            return false;
        RefPtr columnsValue = consumeGridTemplatesRowsOrColumns(range, state);
        if (!columnsValue || !range.atEnd())
            return false;

        result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, rowsValue.releaseNonNull());
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, columnsValue.releaseNonNull());
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSPrimitiveValue::create(CSSValueNone));
        return true;
    }

    // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <track-list> ]?

    range = rangeCopy;

    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;
    CSSValueListBuilder templateRows;

    // Persists between loop iterations so we can use the same value for
    // consecutive <line-names> values
    RefPtr<CSSGridLineNamesValue> lineNames;

    do {
        // Handle leading <custom-ident>*.
        auto previousLineNames = std::exchange(lineNames, consumeGridLineNames(range, state));
        if (lineNames) {
            if (!previousLineNames)
                templateRows.append(lineNames.releaseNonNull());
            else {
                Vector<String> combinedLineNames;
                combinedLineNames.append(previousLineNames->names());
                combinedLineNames.append(lineNames->names());
                templateRows.last() = CSSGridLineNamesValue::create(combinedLineNames);
            }
        }

        // Handle a template-area's row.
        if (range.peek().type() != StringToken || !parseGridTemplateAreasRow(range.consumeIncludingWhitespace().value(), gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        if (RefPtr value = consumeGridTrackSize(range, state))
            templateRows.append(value.releaseNonNull());
        else
            templateRows.append(CSSPrimitiveValue::create(CSSValueAuto));

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        lineNames = consumeGridLineNames(range, state);
        if (lineNames)
            templateRows.append(*lineNames);
    } while (!range.atEnd() && !(range.peek().type() == DelimiterToken && range.peek().delimiter() == '/'));

    RefPtr<CSSValue> columnsValue;
    if (!range.atEnd()) {
        if (!consumeSlashIncludingWhitespace(range))
            return false;
        columnsValue = consumeGridTrackList(range, state, GridTemplateNoRepeat);
        if (!columnsValue || !range.atEnd())
            return false;
    } else {
        columnsValue = CSSPrimitiveValue::create(CSSValueNone);
    }
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, CSSValueList::createSpaceSeparated(WTFMove(templateRows)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, columnsValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount));
    return true;
}

inline bool PropertyParserCustom::consumeGridShorthand(CSSParserTokenRange& range, PropertyParserState& state, [[maybe_unused]] const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 6);

    auto consumeImplicitGridAutoFlow = [](CSSParserTokenRange& range, CSSValueID flowDirection) -> RefPtr<CSSValue> {
        // [ auto-flow && dense? ]
        bool autoFlow = consumeIdentRaw<CSSValueAutoFlow>(range).has_value();
        bool dense = consumeIdentRaw<CSSValueDense>(range).has_value();
        if (!autoFlow && (!dense || !consumeIdentRaw<CSSValueAutoFlow>(range)))
            return nullptr;
        if (!dense)
            return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(flowDirection));
        if (flowDirection == CSSValueRow)
            return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueDense));
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(flowDirection),
            CSSPrimitiveValue::create(CSSValueDense));
    };

    CSSParserTokenRange rangeCopy = range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(range, state, gridTemplateShorthand(), result)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridAutoFlow, CSSPrimitiveValue::create(CSSValueRow));
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridAutoColumns, CSSPrimitiveValue::create(CSSValueAuto));
        result.addPropertyForCurrentShorthand(state, CSSPropertyGridAutoRows, CSSPrimitiveValue::create(CSSValueAuto));

        return true;
    }

    range = rangeCopy;

    RefPtr<CSSValue> autoColumnsValue;
    RefPtr<CSSValue> autoRowsValue;
    RefPtr<CSSValue> templateRows;
    RefPtr<CSSValue> templateColumns;
    RefPtr<CSSValue> gridAutoFlow;

    if (range.peek().id() == CSSValueAutoFlow || range.peek().id() == CSSValueDense) {
        // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
        gridAutoFlow = consumeImplicitGridAutoFlow(range, CSSValueRow);
        if (!gridAutoFlow || range.atEnd())
            return false;
        if (consumeSlashIncludingWhitespace(range))
            autoRowsValue = CSSPrimitiveValue::create(CSSValueAuto);
        else {
            autoRowsValue = consumeGridTrackList(range, state, GridAuto);
            if (!autoRowsValue)
                return false;
            if (!consumeSlashIncludingWhitespace(range))
                return false;
        }
        if (range.atEnd())
            return false;
        templateColumns = consumeGridTemplatesRowsOrColumns(range, state);
        if (!templateColumns)
            return false;
        templateRows = CSSPrimitiveValue::create(CSSValueNone);
        autoColumnsValue = CSSPrimitiveValue::create(CSSValueAuto);
    } else {
        // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
        templateRows = consumeGridTemplatesRowsOrColumns(range, state);
        if (!templateRows)
            return false;
        if (!consumeSlashIncludingWhitespace(range) || range.atEnd())
            return false;
        gridAutoFlow = consumeImplicitGridAutoFlow(range, CSSValueColumn);
        if (!gridAutoFlow)
            return false;
        if (range.atEnd())
            autoColumnsValue = CSSPrimitiveValue::create(CSSValueAuto);
        else {
            autoColumnsValue = consumeGridTrackList(range, state, GridAuto);
            if (!autoColumnsValue)
                return false;
        }
        templateColumns = CSSPrimitiveValue::create(CSSValueNone);
        autoRowsValue = CSSPrimitiveValue::create(CSSValueAuto);
    }

    if (!range.atEnd())
        return false;

    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, templateColumns.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, templateRows.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSPrimitiveValue::create(CSSValueNone));
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridAutoFlow, gridAutoFlow.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridAutoColumns, autoColumnsValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyGridAutoRows, autoRowsValue.releaseNonNull());

    return true;
}

inline bool PropertyParserCustom::consumeAlignShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    // Used to implement the rules in CSS Align for the following shorthands:
    //   <'place-content'> https://drafts.csswg.org/css-align/#propdef-place-content
    //   <'place-items'>   https://drafts.csswg.org/css-align/#propdef-place-items
    //   <'place-self'>    https://drafts.csswg.org/css-align/#propdef-place-self
    //   <'gap'>           https://drafts.csswg.org/css-align/#propdef-gap

    ASSERT(shorthand.id() == state.currentProperty);
    ASSERT(shorthand.length() == 2);
    auto longhands = shorthand.properties();

    auto rangeCopy = range;

    RefPtr prop1 = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[0], state);
    if (!prop1)
        return false;

    // If there are no more tokens, that prop2 should use re-use the original range. This is the equivalent of copying and validating prop1.
    if (range.atEnd())
        range = rangeCopy;

    RefPtr prop2 = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[1], state);
    if (!prop2 || !range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, longhands[0], prop1.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], prop2.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeBlockStepShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // https://drafts.csswg.org/css-rhythm/#block-step
    RefPtr<CSSValue> size;
    RefPtr<CSSValue> insert;
    RefPtr<CSSValue> align;
    RefPtr<CSSValue> round;

    for (unsigned propertiesParsed = 0; propertiesParsed < 4 && !range.atEnd(); ++propertiesParsed) {
        if (!size && (size = CSSPropertyParsing::consumeBlockStepSize(range, state)))
            continue;
        if (!insert && (insert = CSSPropertyParsing::consumeBlockStepInsert(range)))
            continue;
        if (!align && (align = CSSPropertyParsing::consumeBlockStepAlign(range)))
            continue;
        if (!round && (round = CSSPropertyParsing::consumeBlockStepRound(range)))
            continue;

        // There has to be at least one valid longhand.
        return false;
    }

    if (!range.atEnd())
        return false;

    // Fill in default values if one was missing.
    if (!size)
        size = CSSPrimitiveValue::create(CSSValueNone);
    if (!insert)
        insert = CSSPrimitiveValue::create(CSSValueMarginBox);
    if (!align)
        align = CSSPrimitiveValue::create(CSSValueAuto);
    if (!round)
        round = CSSPrimitiveValue::create(CSSValueUp);

    result.addPropertyForCurrentShorthand(state, CSSPropertyBlockStepSize, WTFMove(size));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBlockStepInsert, WTFMove(insert));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBlockStepAlign, WTFMove(align));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBlockStepRound, WTFMove(round));
    return true;
}

inline bool PropertyParserCustom::consumeOverscrollBehaviorShorthand(CSSParserTokenRange& range, PropertyParserState& state, [[maybe_unused]] const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 2);

    if (range.atEnd())
        return false;

    RefPtr overscrollBehaviorX = CSSPropertyParsing::consumeOverscrollBehaviorX(range);
    if (!overscrollBehaviorX)
        return false;

    RefPtr<CSSValue> overscrollBehaviorY;
    range.consumeWhitespace();
    if (range.atEnd())
        overscrollBehaviorY = overscrollBehaviorX;
    else {
        overscrollBehaviorY = CSSPropertyParsing::consumeOverscrollBehaviorY(range);
        range.consumeWhitespace();
        if (!range.atEnd())
            return false;
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyOverscrollBehaviorX, WTFMove(overscrollBehaviorX));
    result.addPropertyForCurrentShorthand(state, CSSPropertyOverscrollBehaviorY, WTFMove(overscrollBehaviorY));
    return true;
}

inline bool PropertyParserCustom::consumeContainerShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr name = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyContainerName, state);
    if (!name)
        return false;

    bool sawSlash = false;

    auto consumeSlashType = [&]() -> RefPtr<CSSValue> {
        if (range.atEnd())
            return nullptr;
        if (!consumeSlashIncludingWhitespace(range))
            return nullptr;
        sawSlash = true;
        return CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyContainerType, state);
    };

    auto type = consumeSlashType();

    if (!range.atEnd() || (sawSlash && !type))
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyContainerName, name.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyContainerType, WTFMove(type));
    return true;
}

inline bool PropertyParserCustom::consumeContainIntrinsicSizeShorthand(CSSParserTokenRange& range, PropertyParserState& state, [[maybe_unused]] const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 2);
    ASSERT(isExposed(CSSPropertyContainIntrinsicSize, &state.context.propertySettings));

    if (range.atEnd())
        return false;

    RefPtr containIntrinsicWidth = CSSPropertyParsing::consumeContainIntrinsicWidth(range, state);
    if (!containIntrinsicWidth)
        return false;

    RefPtr<CSSValue> containIntrinsicHeight;
    range.consumeWhitespace();
    if (range.atEnd())
        containIntrinsicHeight = containIntrinsicWidth;
    else {
        containIntrinsicHeight = CSSPropertyParsing::consumeContainIntrinsicHeight(range, state);
        range.consumeWhitespace();
        if (!range.atEnd() || !containIntrinsicHeight)
            return false;
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyContainIntrinsicWidth, WTFMove(containIntrinsicWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyContainIntrinsicHeight, WTFMove(containIntrinsicHeight));
    return true;
}

inline bool PropertyParserCustom::consumeTransformOriginShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (auto position = consumeOneOrTwoComponentPositionUnresolved(range, state)) {
        range.consumeWhitespace();
        bool atEnd = range.atEnd();
        auto resultZ = CSSPrimitiveValueResolver<Length<>>::consumeAndResolve(range, state);
        if ((!resultZ && !atEnd) || !range.atEnd())
            return false;

        auto [positionX, positionY] = split(WTFMove(*position));
        result.addPropertyForCurrentShorthand(state, CSSPropertyTransformOriginX, CSSPositionXValue::create(WTFMove(positionX)));
        result.addPropertyForCurrentShorthand(state, CSSPropertyTransformOriginY, CSSPositionYValue::create(WTFMove(positionY)));
        result.addPropertyForCurrentShorthand(state, CSSPropertyTransformOriginZ, resultZ);
        return true;
    }
    return false;
}

inline bool PropertyParserCustom::consumePerspectiveOriginShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (auto position = consumePositionUnresolved(range, state)) {
        auto [positionX, positionY] = split(WTFMove(*position));
        result.addPropertyForCurrentShorthand(state, CSSPropertyPerspectiveOriginX, CSSPositionXValue::create(WTFMove(positionX)));
        result.addPropertyForCurrentShorthand(state, CSSPropertyPerspectiveOriginY, CSSPositionYValue::create(WTFMove(positionY)));
        return true;
    }

    return false;
}

inline bool PropertyParserCustom::consumeWebkitPerspectiveShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (RefPtr value = CSSPropertyParsing::consumePerspective(range, state)) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyPerspective, value.releaseNonNull());
        return range.atEnd();
    }

    if (auto perspective = CSSPrimitiveValueResolver<Number<Nonnegative>>::consumeAndResolve(range, state)) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyPerspective, WTFMove(perspective));
        return range.atEnd();
    }

    return false;
}

inline bool PropertyParserCustom::consumeOffsetShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // The offset shorthand is defined as:
    // [ <'offset-position'>?
    //   [ <'offset-path'>
    //     [ <'offset-distance'> || <'offset-rotate'> ]?
    //   ]?
    // ]!
    // [ / <'offset-anchor'> ]?

    // Parse out offset-position.
    auto offsetPosition = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyOffsetPosition, state);

    // Parse out offset-path.
    auto offsetPath = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyOffsetPath, state);

    // Either one of offset-position and offset-path must be present.
    if (!offsetPosition && !offsetPath)
        return false;

    // Only parse offset-distance and offset-rotate if offset-path is specified.
    RefPtr<CSSValue> offsetDistance;
    RefPtr<CSSValue> offsetRotate;
    if (offsetPath) {
        // Try to parse offset-distance first. If successful, parse the following offset-rotate.
        // Otherwise, parse in the reverse order.
        if ((offsetDistance = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyOffsetDistance, state)))
            offsetRotate = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyOffsetRotate, state);
        else {
            offsetRotate = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyOffsetRotate, state);
            offsetDistance = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyOffsetDistance, state);
        }
    }

    // Parse out offset-anchor. Only parse if the prefix slash is present.
    RefPtr<CSSValue> offsetAnchor;
    if (consumeSlashIncludingWhitespace(range)) {
        // offset-anchor must follow the slash.
        if (!(offsetAnchor = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyOffsetAnchor, state)))
            return false;
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyOffsetPath, WTFMove(offsetPath));
    result.addPropertyForCurrentShorthand(state, CSSPropertyOffsetDistance, WTFMove(offsetDistance));
    result.addPropertyForCurrentShorthand(state, CSSPropertyOffsetPosition, WTFMove(offsetPosition));
    result.addPropertyForCurrentShorthand(state, CSSPropertyOffsetAnchor, WTFMove(offsetAnchor));
    result.addPropertyForCurrentShorthand(state, CSSPropertyOffsetRotate, WTFMove(offsetRotate));

    return range.atEnd();
}

inline bool PropertyParserCustom::consumeListStyleShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr<CSSValue> position;
    RefPtr<CSSValue> image;
    RefPtr<CSSValue> type;
    unsigned noneCount = 0;

    while (!range.atEnd()) {
        if (range.peek().id() == CSSValueNone) {
            ++noneCount;
            consumeIdent(range);
            continue;
        }
        if (!position && (position = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyListStylePosition, state)))
            continue;

        if (!image && (image = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyListStyleImage, state)))
            continue;

        if (!type && (type = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyListStyleType, state)))
            continue;

        return false;
    }

    if (noneCount > (static_cast<unsigned>(!image + !type)))
        return false;

    if (noneCount == 2) {
        // Using implicit none for list-style-image is how we serialize "none" instead of "none none".
        image = nullptr;
        type = CSSPrimitiveValue::create(CSSValueNone);
    } else if (noneCount == 1) {
        // Use implicit none for list-style-image, but non-implicit for type.
        if (!type)
            type = CSSPrimitiveValue::create(CSSValueNone);
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyListStylePosition, WTFMove(position));
    result.addPropertyForCurrentShorthand(state, CSSPropertyListStyleImage, WTFMove(image));
    result.addPropertyForCurrentShorthand(state, CSSPropertyListStyleType, WTFMove(type));
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeLineClampShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    ASSERT(state.context.propertySettings.cssLineClampEnabled);

    if (range.peek().id() == CSSValueNone) {
        // Sets max-lines to none, continue to auto, and block-ellipsis to none.
        result.addPropertyForCurrentShorthand(state, CSSPropertyMaxLines, CSSPrimitiveValue::create(CSSValueNone));
        result.addPropertyForCurrentShorthand(state, CSSPropertyContinue, CSSPrimitiveValue::create(CSSValueAuto));
        result.addPropertyForCurrentShorthand(state, CSSPropertyBlockEllipsis, CSSPrimitiveValue::create(CSSValueNone));
        consumeIdent(range);
        return range.atEnd();
    }

    RefPtr<CSSValue> maxLines;
    RefPtr<CSSValue> blockEllipsis;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !range.atEnd(); ++propertiesParsed) {
        if (!maxLines && (maxLines = CSSPropertyParsing::consumeMaxLines(range, state)))
            continue;
        if (!blockEllipsis && (blockEllipsis = CSSPropertyParsing::consumeBlockEllipsis(range)))
            continue;
        // There has to be at least one valid longhand.
        return false;
    }

    if (!blockEllipsis)
        blockEllipsis = CSSPrimitiveValue::create(CSSValueAuto);

    if (!maxLines)
        maxLines = CSSPrimitiveValue::create(CSSValueNone);

    result.addPropertyForCurrentShorthand(state, CSSPropertyMaxLines, WTFMove(maxLines));
    result.addPropertyForCurrentShorthand(state, CSSPropertyContinue, CSSPrimitiveValue::create(CSSValueDiscard));
    result.addPropertyForCurrentShorthand(state, CSSPropertyBlockEllipsis, WTFMove(blockEllipsis));
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeTextBoxShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (range.peek().id() == CSSValueNormal) {
        // if the single keyword normal is specified, it sets text-box-trim to none and text-box-edge to auto.
        result.addPropertyForCurrentShorthand(state, CSSPropertyTextBoxTrim, CSSPrimitiveValue::create(CSSValueNone));
        result.addPropertyForCurrentShorthand(state, CSSPropertyTextBoxEdge, CSSPrimitiveValue::create(CSSValueAuto));
        consumeIdent(range);
        return range.atEnd();
    }

    RefPtr<CSSValue> textBoxTrim;
    RefPtr<CSSValue> textBoxEdge;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !range.atEnd(); ++propertiesParsed) {
        if (!textBoxTrim && (textBoxTrim = CSSPropertyParsing::consumeTextBoxTrim(range)))
            continue;
        if (!textBoxEdge && (textBoxEdge = consumeTextBoxEdge(range, state)))
            continue;
        // There has to be at least one valid longhand.
        return false;
    }

    if (!range.atEnd())
        return false;

    // Omitting the text-box-edge value sets it to auto (the initial value)
    if (!textBoxEdge)
        textBoxEdge = CSSPrimitiveValue::create(CSSValueAuto);

    // Omitting the text-box-trim value sets it to both (not the initial value)
    if (!textBoxTrim)
        textBoxTrim = CSSPrimitiveValue::create(CSSValueTrimBoth);

    result.addPropertyForCurrentShorthand(state, CSSPropertyTextBoxTrim, WTFMove(textBoxTrim));
    result.addPropertyForCurrentShorthand(state, CSSPropertyTextBoxEdge, WTFMove(textBoxEdge));
    return true;
}

inline bool PropertyParserCustom::consumeTextWrapShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr<CSSValue> mode;
    RefPtr<CSSValue> style;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !range.atEnd(); ++propertiesParsed) {
        if (!mode && (mode = CSSPropertyParsing::consumeTextWrapMode(range)))
            continue;
        if (!style && (style = CSSPropertyParsing::consumeTextWrapStyle(range, state)))
            continue;
        // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
        return false;
    }

    if (!range.atEnd())
        return false;

    // Fill in default values if one was missing from the multi-value syntax.
    if (!mode)
        mode = CSSPrimitiveValue::create(CSSValueWrap);
    if (!style)
        style = CSSPrimitiveValue::create(CSSValueAuto);

    result.addPropertyForCurrentShorthand(state, CSSPropertyTextWrapMode, WTFMove(mode));
    result.addPropertyForCurrentShorthand(state, CSSPropertyTextWrapStyle, WTFMove(style));
    return true;
}

inline bool PropertyParserCustom::consumeWhiteSpaceShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr<CSSValue> whiteSpaceCollapse;
    RefPtr<CSSValue> textWrapMode;

    // Single value syntax.
    auto singleValueKeyword = consumeIdentRaw<
        CSSValueNormal,
        CSSValuePre,
        CSSValuePreLine,
        CSSValuePreWrap
    >(range);

    if (singleValueKeyword) {
        switch (*singleValueKeyword) {
        case CSSValueNormal:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValueCollapse);
            textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);
            break;
        case CSSValuePre:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValuePreserve);
            textWrapMode = CSSPrimitiveValue::create(CSSValueNowrap);
            break;
        case CSSValuePreLine:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValuePreserveBreaks);
            textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);
            break;
        case CSSValuePreWrap:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValuePreserve);
            textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    } else {
        // Multi-value syntax.
        for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !range.atEnd(); ++propertiesParsed) {
            if (!whiteSpaceCollapse && (whiteSpaceCollapse = CSSPropertyParsing::consumeWhiteSpaceCollapse(range)))
                continue;
            if (!textWrapMode && (textWrapMode = CSSPropertyParsing::consumeTextWrapMode(range)))
                continue;
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    if (!range.atEnd())
        return false;

    // Fill in default values if one was missing from the multi-value syntax.
    if (!whiteSpaceCollapse)
        whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValueCollapse);
    if (!textWrapMode)
        textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);

    result.addPropertyForCurrentShorthand(state, CSSPropertyWhiteSpaceCollapse, WTFMove(whiteSpaceCollapse));
    result.addPropertyForCurrentShorthand(state, CSSPropertyTextWrapMode, WTFMove(textWrapMode));
    return true;
}


inline bool PropertyParserCustom::consumeAnimationRangeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSValueListBuilder startList;
    CSSValueListBuilder endList;
    do {
        RefPtr start = consumeSingleAnimationRangeStart(range, state);
        if (!start)
            return false;

        RefPtr<CSSValue> end;
        range.consumeWhitespace();
        if (range.atEnd() || range.peek().type() == CommaToken) {
            // From the spec: If <'animation-range-end'> is omitted and <'animation-range-start'> includes a component, then
            // animation-range-end is set to that same and 100%. Otherwise, any omitted longhand is set to its initial value.
            auto rangeEndValueForStartValue = [](const CSSValue& value) {
                RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
                if (primitiveValue && SingleTimelineRange::isOffsetValue(downcast<CSSPrimitiveValue>(value)))
                    return CSSPrimitiveValue::create(CSSValueNormal);
                return CSSPrimitiveValue::create(value.valueID());
            };

            if (RefPtr startPrimitiveValue = dynamicDowncast<CSSPrimitiveValue>(start))
                end = rangeEndValueForStartValue(*startPrimitiveValue);
            else {
                RefPtr startPair = downcast<CSSValuePair>(start);
                end = rangeEndValueForStartValue(startPair->first());
            }
        } else {
            end = consumeSingleAnimationRangeEnd(range, state);
            range.consumeWhitespace();
            if (!end)
                return false;
        }
        startList.append(start.releaseNonNull());
        endList.append(end.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(range));

    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyAnimationRangeStart, CSSValueList::createCommaSeparated(WTFMove(startList)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyAnimationRangeEnd, CSSValueList::createCommaSeparated(WTFMove(endList)));
    return true;
}

inline bool PropertyParserCustom::consumeScrollTimelineShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSValueListBuilder namesList;
    CSSValueListBuilder axesList;

    do {
        // A valid scroll-timeline-name is required.
        if (RefPtr name = CSSPropertyParsing::consumeSingleScrollTimelineName(range))
            namesList.append(name.releaseNonNull());
        else
            return false;

        // A scroll-timeline-axis is optional.
        if (range.peek().type() == CommaToken || range.atEnd())
            axesList.append(CSSPrimitiveValue::create(CSSValueBlock));
        else if (auto axis = CSSPropertyParsing::consumeAxis(range))
            axesList.append(axis.releaseNonNull());
        else
            return false;
    } while (consumeCommaIncludingWhitespace(range));

    if (namesList.isEmpty())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyScrollTimelineName, CSSValueList::createCommaSeparated(WTFMove(namesList)));
    if (!axesList.isEmpty())
        result.addPropertyForCurrentShorthand(state, CSSPropertyScrollTimelineAxis, CSSValueList::createCommaSeparated(WTFMove(axesList)));
    return true;
}

inline bool PropertyParserCustom::consumeViewTimelineShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSValueListBuilder namesList;
    CSSValueListBuilder axesList;
    CSSValueListBuilder insetsList;

    auto defaultAxis = [] -> Ref<CSSValue> { return CSSPrimitiveValue::create(CSSValueBlock); };
    auto defaultInsets = [] -> Ref<CSSValue> { return CSSPrimitiveValue::create(CSSValueAuto); };

    do {
        // A valid view-timeline-name is required.
        if (RefPtr name = CSSPropertyParsing::consumeSingleScrollTimelineName(range))
            namesList.append(name.releaseNonNull());
        else
            return false;

        // Both a view-timeline-axis and a view-timeline-inset are optional.
        if (range.peek().type() != CommaToken && !range.atEnd()) {
            RefPtr axis = CSSPropertyParsing::consumeAxis(range);
            RefPtr insets = consumeSingleViewTimelineInsetItem(range, state);
            // Since the order of view-timeline-axis and view-timeline-inset is not guaranteed, let's try view-timeline-axis again.
            if (!axis)
                axis = CSSPropertyParsing::consumeAxis(range);
            if (!axis && !insets)
                return false;
            axesList.append(axis ? axis.releaseNonNull() : defaultAxis());
            insetsList.append(insets ? insets.releaseNonNull() : defaultInsets());
        } else {
            axesList.append(defaultAxis());
            insetsList.append(defaultInsets());
        }
    } while (consumeCommaIncludingWhitespace(range));

    if (namesList.isEmpty())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyViewTimelineName, CSSValueList::createCommaSeparated(WTFMove(namesList)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyViewTimelineAxis, CSSValueList::createCommaSeparated(WTFMove(axesList)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyViewTimelineInset, CSSValueList::createCommaSeparated(WTFMove(insetsList)));
    return true;
}

inline bool PropertyParserCustom::consumePositionTryShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto order = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyPositionTryOrder, state);
    auto fallbacks = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyPositionTryFallbacks, state);
    if (!fallbacks)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyPositionTryOrder, WTFMove(order));
    result.addPropertyForCurrentShorthand(state, CSSPropertyPositionTryFallbacks, WTFMove(fallbacks));
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeMarkerShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr marker = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyMarkerStart, state);
    if (!marker || !range.atEnd())
        return false;

    Ref markerRef = marker.releaseNonNull();
    result.addPropertyForCurrentShorthand(state, CSSPropertyMarkerStart, markerRef.copyRef());
    result.addPropertyForCurrentShorthand(state, CSSPropertyMarkerMid, markerRef.copyRef());
    result.addPropertyForCurrentShorthand(state, CSSPropertyMarkerEnd, WTFMove(markerRef));
    return true;
}

} // namespace CSS
} // namespace WebCore
