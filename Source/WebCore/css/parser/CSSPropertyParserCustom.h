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
#include "CSSBorderRadius.h"
#include "CSSCustomIdentValue.h"
#include "CSSFlexWrapValue.h"
#include "CSSFontStyleRangeValue.h"
#include "CSSFontVariantLigaturesParser.h"
#include "CSSFontVariantNumericParser.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoFlowValue.h"
#include "CSSGridLineValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSGridTemplateListValue.h"
#include "CSSKeywordValueInlines.h"
#include "CSSMaskBorder.h"
#include "CSSOffsetRotateValue.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveNumericTypes+CSSValueCreation.h"
#include "CSSPropertyParserConsumer+Align.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+Animations.h"
#include "CSSPropertyParserConsumer+Background.h"
#include "CSSPropertyParserConsumer+Box.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+CalcSize.h"
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
#include "CSSPropertyParserConsumer+LinkParameters.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+Lists.h"
#include "CSSPropertyParserConsumer+Masking.h"
#include "CSSPropertyParserConsumer+Motion.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+ObjectViewBox.h"
#include "CSSPropertyParserConsumer+Overflow.h"
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
#include "CSSPropertyParserConsumer+UI.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserConsumer+UnicodeRange.h"
#include "CSSPropertyParserConsumer+ViewTransition.h"
#include "CSSPropertyParserResult.h"
#include "CSSPropertyParsing.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "FontFace.h"
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
    static bool consumeCoalescingPairShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeCoalescingQuadShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);

    static bool consumeBorderShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderInlineShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderBlockShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeAnimationShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeTransitionShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
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
    static bool consumeCornerSingleShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeCornerPairShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeCornerQuadShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderRadiusShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeWebkitBorderRadiusShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeBorderRadiusSideShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
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
    static bool consumeTimelineTriggerActivationRangeShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeTimelineTriggerActiveRangeShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeScrollTimelineShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeViewTimelineShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeLineClampShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
    static bool consumeHyphenateLimitCharsShorthand(CSSParserTokenRange&, PropertyParserState&, const StylePropertyShorthand&, PropertyParserResult&);
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
            components.width = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::BorderLeftWidth, state);
            if (components.width)
                continue;
        }
        if (!components.style) {
            components.style = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::BorderLeftStyle, state);
            if (components.style)
                continue;
        }
        if (!components.color) {
            components.color = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::BorderLeftColor, state);
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
    if (value == CSSValueID::Always)
        return CSSValueID::Page;
    if (value == CSSValueID::Auto || value == CSSValueID::Avoid || value == CSSValueID::Left || value == CSSValueID::Right)
        return value;
    return CSSValueID::Invalid;
}

inline CSSValueID mapFromColumnBreakBetween(CSSValueID value)
{
    if (value == CSSValueID::Always)
        return CSSValueID::Column;
    if (value == CSSValueID::Auto)
        return value;
    if (value == CSSValueID::Avoid)
        return CSSValueID::AvoidColumn;
    return CSSValueID::Invalid;
}

inline CSSValueID mapFromColumnRegionOrPageBreakInside(CSSValueID value)
{
    if (value == CSSValueID::Auto || value == CSSValueID::Avoid)
        return value;
    return CSSValueID::Invalid;
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
        result.addPropertyForCurrentShorthand(state, shorthandProperties[i], WTF::move(longhands[i]));
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
        result.addPropertyForAllLonghandsOfCurrentShorthand(state, CSSKeywordValue::create(systemFont), IsImplicit::Yes);
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
        if (consumeIdent<CSSValueID::Normal>(range))
            continue;
        if (!fontStyle && (fontStyle = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontStyle, state)))
            continue;
        if (!fontVariantCaps && (fontVariantCaps = consumeIdent<CSSValueID::SmallCaps>(range)))
            continue;
        if (!fontWeight && (fontWeight = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontWeight, state)))
            continue;
        if (!fontWidth && (fontWidth = CSSPropertyParsing::consumeFontWidthAbsolute(range)))
            continue;
        break;
    }

    if (range.atEnd())
        return false;

    fontSize = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontSize, state);
    if (!fontSize || range.atEnd())
        return false;

    if (consumeSlashIncludingWhitespace(range)) {
        if (!consumeIdent<CSSValueID::Normal>(range)) {
            lineHeight = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::LineHeight, state);
            if (!lineHeight)
                return false;
        }
        if (range.atEnd())
            return false;
    }

    fontFamily = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontFamily, state);
    if (!fontFamily || !range.atEnd())
        return false;

    guard.commit();

    auto shorthandProperties = shorthand.properties();
    for (auto [value, longhand] : zippedRange(values, shorthandProperties.first(values.size())))
        result.addPropertyForCurrentShorthand(state, longhand, WTF::move(value), IsImplicit::Yes);
    for (auto longhand : shorthandProperties.subspan(values.size()))
        result.addPropertyForCurrentShorthand(state, longhand, nullptr, IsImplicit::Yes);

    return true;
}

inline bool PropertyParserCustom::consumeFontVariantShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (identMatches<CSSValueID::Normal, CSSValueID::None>(range.peek().id())) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantLigatures, consumeIdent(range));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantCaps, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantAlternates, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantNumeric, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantEastAsian, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantPosition, nullptr);
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantEmoji, nullptr);
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
        if (range.peek().id() == CSSValueID::Normal)
            return false;

        if (!capsValue && (capsValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontVariantCaps, state)))
            continue;

        if (!positionValue && (positionValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontVariantPosition, state)))
            continue;

        if (!alternatesValue && (alternatesValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontVariantAlternates, state)))
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

        if (!eastAsianValue && (eastAsianValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontVariantEastAsian, state)))
            continue;

        if (state.context.propertySettings.cssFontVariantEmojiEnabled && !emojiValue && (emojiValue = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::FontVariantEmoji, state)))
            continue;

        // Saw some value that didn't match anything else.
        return false;
    } while (!range.atEnd());

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantLigatures, ligaturesParser.finalizeValue().releaseNonNull(), implicitLigatures);
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantCaps, WTF::move(capsValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantAlternates, WTF::move(alternatesValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantNumeric, numericParser.finalizeValue().releaseNonNull(), implicitNumeric);
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantEastAsian, WTF::move(eastAsianValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantPosition, WTF::move(positionValue));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontVariantEmoji, WTF::move(emojiValue));
    return true;
}

inline bool PropertyParserCustom::consumeFontSynthesisShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // none | [ weight || style || small-caps ]
    if (range.peek().id() == CSSValueID::None) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontSynthesisSmallCaps, consumeIdent(range).releaseNonNull());
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontSynthesisStyle, CSSKeywordValue::create(CSSValueID::None));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontSynthesisWeight, CSSKeywordValue::create(CSSValueID::None));
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
        RefPtr ident = consumeIdent<CSSValueID::Weight, CSSValueID::Style, CSSValueID::SmallCaps>(range);
        if (!ident)
            return false;
        switch (ident->valueID()) {
        case CSSValueID::Weight:
            if (!checkAndMarkExistence(&foundWeight))
                return false;
            break;
        case CSSValueID::Style:
            if (!checkAndMarkExistence(&foundStyle))
                return false;
            break;
        case CSSValueID::SmallCaps:
            if (!checkAndMarkExistence(&foundSmallCaps))
                return false;
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontSynthesisWeight, CSSKeywordValue::create(foundWeight ? CSSValueID::Auto : CSSValueID::None));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontSynthesisStyle, CSSKeywordValue::create(foundStyle ? CSSValueID::Auto : CSSValueID::None));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FontSynthesisSmallCaps, CSSKeywordValue::create(foundSmallCaps ? CSSValueID::Auto : CSSValueID::None));
    return true;
}

inline bool PropertyParserCustom::consumeTextDecorationSkipShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (auto skip = consumeIdentRaw<CSSValueID::None, CSSValueID::Auto, CSSValueID::Ink>(range)) {
        switch (*skip) {
        case CSSValueID::None:
            result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextDecorationSkipInk, CSSKeywordValue::create(CSSValueID::None));
            return range.atEnd();
        case CSSValueID::Auto:
        case CSSValueID::Ink:
            result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextDecorationSkipInk, CSSKeywordValue::create(CSSValueID::Auto));
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

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::WebkitBorderHorizontalSpacing, horizontalSpacing.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::WebkitBorderVerticalSpacing, verticalSpacing.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeColumnsShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr<CSSValue> columnWidth;
    RefPtr<CSSValue> columnCount;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !range.atEnd(); ++propertiesParsed) {
        if (range.peek().id() == CSSValueID::Auto) {
            // 'auto' is a valid value for any of the two longhands, and at this point
            // we don't know which one(s) it is meant for. We need to see if there are other values first.
            consumeIdent(range);
        } else {
            if (!columnWidth && (columnWidth = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::ColumnWidth, state)))
                continue;
            if (!columnCount && (columnCount = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::ColumnCount, state)))
                continue;
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ColumnWidth, WTF::move(columnWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ColumnCount, WTF::move(columnCount));
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
        case CSSValueID::Auto:
        case CSSValueID::Content:
        case CSSValueID::Intrinsic:
        case CSSValueID::MinIntrinsic:
        case CSSValueID::MinContent:
        case CSSValueID::WebkitMinContent:
        case CSSValueID::MaxContent:
        case CSSValueID::WebkitMaxContent:
        case CSSValueID::WebkitFillAvailable:
        case CSSValueID::Stretch:
        case CSSValueID::FitContent:
        case CSSValueID::WebkitFitContent:
            return true;
        default:
            return false;
        }
    };

    RefPtr<CSSValue> flexGrow;
    RefPtr<CSSValue> flexShrink;
    RefPtr<CSSValue> flexBasis;

    if (range.peek().id() == CSSValueID::None) {
        flexGrow = CSSPrimitiveValue::create(0);
        flexShrink = CSSPrimitiveValue::create(0);
        flexBasis = CSSKeywordValue::create(CSSValueID::Auto);
        range.consumeIncludingWhitespace();
    } else {
        unsigned index = 0;
        while (!range.atEnd() && index++ < 3) {
            if (auto number = CSSPrimitiveValueResolver<Number<Nonnegative>>::consumeAndResolve(range, state)) {
                if (!flexGrow)
                    flexGrow = WTF::move(number);
                else if (!flexShrink)
                    flexShrink = WTF::move(number);
                else if (number->isZero() == true) // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                    flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::Px);
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
            flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::Percentage);
    }

    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FlexGrow, flexGrow.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FlexShrink, flexShrink.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::FlexBasis, flexBasis.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeBorderShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderShorthandComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderWidth, WTF::move(components->width), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderStyle, WTF::move(components->style), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderColor, WTF::move(components->color), state.important);

    for (auto longhand : borderImageShorthand())
        result.addPropertyForCurrentShorthand(state, longhand, nullptr);
    return true;
}

inline bool PropertyParserCustom::consumeBorderInlineShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderShorthandComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderInlineWidth, WTF::move(components->width), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderInlineStyle, WTF::move(components->style), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderInlineColor, WTF::move(components->color), state.important);
    return true;
}

inline bool PropertyParserCustom::consumeBorderBlockShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeBorderShorthandComponents(range, state);
    if (!components)
        return false;

    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderBlockWidth, WTF::move(components->width), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderBlockStyle, WTF::move(components->style), state.important);
    result.addPropertyForAllLonghandsOfShorthand(state, CSSPropertyID::BorderBlockColor, WTF::move(components->color), state.important);
    return true;
}

inline bool PropertyParserCustom::consumeBorderRadiusShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto borderRadius = consumeUnresolvedBorderRadius(range, state);
    if (!borderRadius)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderTopLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topLeft()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderTopRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderBottomRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderBottomLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomLeft()));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitBorderRadiusShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto borderRadius = consumeUnresolvedWebKitBorderRadius(range, state);
    if (!borderRadius)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderTopLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topLeft()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderTopRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->topRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderBottomRightRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomRight()));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderBottomLeftRadius, WebCore::CSS::createCSSValue(state.pool, borderRadius->bottomLeft()));
    return true;
}

// MARK: - Corner / Corner-* shorthands (combined border-radius + corner-shape)
//
// Grammar (per corner): normal | <corner-shape>? <border-radius-corner> <corner-shape>?
// Multiple corners are separated by '/' and follow the standard 1-to-N expansion.

namespace CornerShorthandHelpers {

inline Ref<CSSValue> zeroRadius()
{
    auto zero = CSSPrimitiveValue::create(0, CSSUnitType::Px);
    return CSSValuePair::create(zero.copyRef(), WTF::move(zero));
}

inline Ref<CSSValue> roundShape()
{
    return CSSKeywordValue::create(CSSValueID::Round);
}

inline bool consumeOneCorner(CSSParserTokenRange& range, PropertyParserState& state, CSSPropertyID radiusProperty, CSSPropertyID shapeProperty, RefPtr<CSSValue>& radiusOut, RefPtr<CSSValue>& shapeOut)
{
    if (range.peek().id() == CSSValueID::Normal) {
        range.consumeIncludingWhitespace();
        radiusOut = zeroRadius();
        shapeOut = roundShape();
        return true;
    }
    shapeOut = CSSPropertyParsing::parseStylePropertyLonghand(range, shapeProperty, state);
    radiusOut = CSSPropertyParsing::parseStylePropertyLonghand(range, radiusProperty, state);
    if (!radiusOut)
        return false;
    if (!shapeOut)
        shapeOut = CSSPropertyParsing::parseStylePropertyLonghand(range, shapeProperty, state);
    return shapeOut != nullptr;
}

} // namespace CornerShorthandHelpers

inline bool PropertyParserCustom::consumeCornerSingleShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 2);
    auto longhands = shorthand.properties();
    RefPtr<CSSValue> radius;
    RefPtr<CSSValue> shape;
    if (!CornerShorthandHelpers::consumeOneCorner(range, state, longhands[0], longhands[1], radius, shape))
        return false;
    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, longhands[0], radius.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], shape.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeCornerPairShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    // Two-corner side shorthand: longhands are [radius0, shape0, radius1, shape1].
    ASSERT(shorthand.length() == 4);
    auto longhands = shorthand.properties();
    std::array<RefPtr<CSSValue>, 2> radii;
    std::array<RefPtr<CSSValue>, 2> shapes;

    size_t count = 0;
    for (size_t i = 0; i < 2; ++i) {
        if (!CornerShorthandHelpers::consumeOneCorner(range, state, longhands[i * 2], longhands[i * 2 + 1], radii[i], shapes[i]))
            return false;
        ++count;
        if (i == 1)
            break;
        if (!consumeSlashIncludingWhitespace(range))
            break;
    }
    if (!range.atEnd() || !count)
        return false;

    if (count < 2) {
        radii[1] = radii[0];
        shapes[1] = shapes[0];
    }

    for (size_t i = 0; i < 2; ++i) {
        result.addPropertyForCurrentShorthand(state, longhands[i * 2], radii[i].releaseNonNull());
        result.addPropertyForCurrentShorthand(state, longhands[i * 2 + 1], shapes[i].releaseNonNull());
    }
    return true;
}

inline bool PropertyParserCustom::consumeCornerQuadShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    // Master `corner`: longhands are [radius0, shape0, radius1, shape1, radius2, shape2, radius3, shape3]
    // in order top-left, top-right, bottom-right, bottom-left.
    ASSERT(shorthand.length() == 8);
    auto longhands = shorthand.properties();
    std::array<RefPtr<CSSValue>, 4> radii;
    std::array<RefPtr<CSSValue>, 4> shapes;

    size_t count = 0;
    for (size_t i = 0; i < 4; ++i) {
        if (!CornerShorthandHelpers::consumeOneCorner(range, state, longhands[i * 2], longhands[i * 2 + 1], radii[i], shapes[i]))
            return false;
        ++count;
        if (i == 3)
            break;
        if (!consumeSlashIncludingWhitespace(range))
            break;
    }
    if (!range.atEnd() || !count)
        return false;

    // 1-to-4 expansion (mirrors complete4Sides on a CSS quad).
    if (count < 2) {
        radii[1] = radii[0];
        shapes[1] = shapes[0];
    }
    if (count < 3) {
        radii[2] = radii[0];
        shapes[2] = shapes[0];
    }
    if (count < 4) {
        radii[3] = radii[1];
        shapes[3] = shapes[1];
    }

    for (size_t i = 0; i < 4; ++i) {
        result.addPropertyForCurrentShorthand(state, longhands[i * 2], radii[i].releaseNonNull());
        result.addPropertyForCurrentShorthand(state, longhands[i * 2 + 1], shapes[i].releaseNonNull());
    }
    return true;
}

inline bool PropertyParserCustom::consumeBorderRadiusSideShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 2);
    auto side = consumeUnresolvedBorderRadiusSide(range, state);
    if (!side)
        return false;

    auto longhands = shorthand.properties();
    result.addPropertyForCurrentShorthand(state, longhands[0], WebCore::CSS::createCSSValue(state.pool, side->first()));
    result.addPropertyForCurrentShorthand(state, longhands[1], WebCore::CSS::createCSSValue(state.pool, side->second()));
    return true;
}

inline bool PropertyParserCustom::consumeBorderImageShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeUnresolvedBorderImage(range, state);
    if (!components)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageSource, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageSource));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageSlice, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageSlice));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageWidth, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageOutset, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageOutset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageRepeat, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageRepeat));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitBorderImageShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // NOTE: -webkit-border-image has a legacy behavior that makes border image slices default to `fill`.
    // NOTE: -webkit-border-image has a legacy behavior that makes border image widths with length values also set the border widths.

    auto components = consumeUnresolvedBorderImage(range, state, BorderImageSliceOverride::AlwaysFill, BorderImageWidthOverridesWidthForLength::Yes);
    if (!components)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageSource, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageSource));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageSlice, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageSlice));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageWidth, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageOutset, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageOutset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BorderImageRepeat, WebCore::CSS::tryCreateCSSValue(state.pool, components->borderImageRepeat));
    return true;
}

inline bool PropertyParserCustom::consumeMaskBorderShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto components = consumeUnresolvedMaskBorder(range, state);
    if (!components)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderSource, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderSource));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderSlice, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderSlice));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderWidth, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderOutset, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderOutset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderRepeat, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderRepeat));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitMaskBoxImageShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // NOTE: -webkit-mask-box-image has a legacy behavior that makes border image slices default to `fill`.

    using namespace CSS::Literals;

    auto components = consumeUnresolvedMaskBorder(range, state, MaskBorderSliceOverride::AlwaysFill);
    if (!components)
        return false;

    auto defaultSlice = [] {
        return CSS::MaskBorderSlice {
            .values = { CSS::MaskBorderSlice::Value { 0_css_number } },
            .fill = CSS::Keyword::Fill { },
        };
    };

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderSource, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderSource));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderSlice, WebCore::CSS::createCSSValue(state.pool, components->maskBorderSlice.value_or(defaultSlice())));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderWidth, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderOutset, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderOutset));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaskBorderRepeat, WebCore::CSS::tryCreateCSSValue(state.pool, components->maskBorderRepeat));
    return true;
}

inline bool PropertyParserCustom::consumePageBreakAfterShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromPageBreakBetween(*keyword);
    if (value == CSSValueID::Invalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BreakAfter, CSSKeywordValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumePageBreakBeforeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromPageBreakBetween(*keyword);
    if (value == CSSValueID::Invalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BreakBefore, CSSKeywordValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumePageBreakInsideShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto keyword = consumeIdentRaw(range);
    if (!keyword || !range.atEnd())
        return false;
    auto value = mapFromColumnRegionOrPageBreakInside(*keyword);
    if (value == CSSValueID::Invalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BreakInside, CSSKeywordValue::create(value));
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
    if (value == CSSValueID::Invalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BreakAfter, CSSKeywordValue::create(value));
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
    if (value == CSSValueID::Invalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BreakBefore, CSSKeywordValue::create(value));
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
    if (value == CSSValueID::Invalid)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BreakInside, CSSKeywordValue::create(value));
    return true;
}

inline bool PropertyParserCustom::consumeWebkitTextOrientationShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    // -webkit-text-orientation is a legacy shorthand for text-orientation.
    // The only difference is that it accepts 'sideways-right', which is mapped into 'sideways'.
    RefPtr<CSSValue> keyword;
    auto valueID = range.peek().id();
    if (valueID == CSSValueID::SidewaysRight) {
        keyword = CSSKeywordValue::create(CSSValueID::Sideways);
        consumeIdentRaw(range);
    } else if (CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyID::TextOrientation, valueID, state))
        keyword = consumeIdent(range);
    if (!keyword || !range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextOrientation, keyword.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeAnimationShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    auto consumeAnimationValueForShorthand = [&](CSSPropertyID property) -> RefPtr<CSSValue> {
        switch (property) {
        case CSSPropertyID::AnimationDelay:
            return CSSPrimitiveValueResolver<Time<>>::consumeAndResolve(range, state);
        case CSSPropertyID::AnimationDirection:
            return CSSPropertyParsing::consumeSingleAnimationDirection(range);
        case CSSPropertyID::AnimationDuration:
            return CSSPropertyParsing::consumeSingleAnimationDuration(range, state);
        case CSSPropertyID::AnimationFillMode:
            return CSSPropertyParsing::consumeSingleAnimationFillMode(range);
        case CSSPropertyID::AnimationIterationCount:
            return CSSPropertyParsing::consumeSingleAnimationIterationCount(range, state);
        case CSSPropertyID::AnimationName:
            return CSSPropertyParsing::consumeSingleAnimationName(range, state);
        case CSSPropertyID::AnimationPlayState:
            return CSSPropertyParsing::consumeSingleAnimationPlayState(range);
        case CSSPropertyID::AnimationComposition:
            return CSSPropertyParsing::consumeSingleAnimationComposition(range);
        case CSSPropertyID::AnimationTimeline:
        case CSSPropertyID::AnimationRangeStart:
        case CSSPropertyID::AnimationRangeEnd:
            return nullptr; // reset-only longhands
        case CSSPropertyID::AnimationTimingFunction:
            return consumeEasingFunction(range, state);
        case CSSPropertyID::TransitionBehavior:
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
        case CSSPropertyID::AnimationTimeline:
        case CSSPropertyID::AnimationRangeStart:
        case CSSPropertyID::AnimationRangeEnd:
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
                longhands[i].append(protect(CSSKeywordValue::implicitInitialValue()));
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(range));

    for (size_t i = 0; i < longhandCount; ++i) {
        auto& list = longhands[i];
        if (list.isEmpty()) // reset-only property
            result.addPropertyForCurrentShorthand(state, shorthandProperties[i], nullptr);
        else
            result.addPropertyForCurrentShorthand(state, shorthandProperties[i], CSSValueList::createCommaSeparated(WTF::move(list)));
    }

    return range.atEnd();
}

inline bool PropertyParserCustom::consumeTransitionShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    auto isValidTransitionPropertyList = [](CSSPropertyID property, const CSSValueListBuilder& valueList) {
        // If there is more than one <single-transition> in the shorthand, and any of the transitions
        // has none as the <single-transition-property>, then the declaration is invalid.
        if (property != CSSPropertyID::TransitionProperty || valueList.size() < 2)
            return true;
        for (auto& value : valueList) {
            if (isValueID(value, CSSValueID::None))
                return false;
        }
        return true;
    };

    auto consumeTransitionValueForShorthand = [&](CSSPropertyID property) -> RefPtr<CSSValue> {
        switch (property) {
        case CSSPropertyID::TransitionDelay:
            return CSSPrimitiveValueResolver<Time<>>::consumeAndResolve(range, state);
        case CSSPropertyID::TransitionDuration:
            return CSSPrimitiveValueResolver<Time<Nonnegative>>::consumeAndResolve(range, state);
        case CSSPropertyID::TransitionProperty:
            return CSSPropertyParsing::consumeSingleTransitionPropertyOrNone(range, state);
        case CSSPropertyID::TransitionTimingFunction:
            return consumeEasingFunction(range, state);
        case CSSPropertyID::TransitionBehavior:
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

    do {
        std::array<bool, maxLonghandCount> parsedLonghand = { };
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                if (auto value = consumeTransitionValueForShorthand(shorthandProperties[i])) {
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
            if (!parsedLonghand[i])
                longhands[i].append(protect(CSSKeywordValue::implicitInitialValue()));
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(range));

    for (size_t i = 0; i < longhandCount; ++i) {
        if (!isValidTransitionPropertyList(shorthandProperties[i], longhands[i]))
            return false;
    }

    for (size_t i = 0; i < longhandCount; ++i)
        result.addPropertyForCurrentShorthand(state, shorthandProperties[i], CSSValueList::createCommaSeparated(WTF::move(longhands[i])));

    return range.atEnd();
}


inline bool PropertyParserCustom::consumeBackgroundShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.id() == state.currentProperty);

    auto consumeBackgroundComponent = [&](CSSPropertyID property) -> RefPtr<CSSValue> {
        switch (property) {
        // background-*
        case CSSPropertyID::BackgroundClip:
            return CSSPropertyParsing::consumeSingleBackgroundClip(range);
        case CSSPropertyID::BackgroundBlendMode:
            return CSSPropertyParsing::consumeSingleBackgroundBlendMode(range);
        case CSSPropertyID::BackgroundAttachment:
            return CSSPropertyParsing::consumeSingleBackgroundAttachment(range);
        case CSSPropertyID::BackgroundOrigin:
            return CSSPropertyParsing::consumeSingleBackgroundOrigin(range);
        case CSSPropertyID::BackgroundImage:
            return CSSPropertyParsing::consumeSingleBackgroundImage(range, state);
        case CSSPropertyID::BackgroundRepeat:
            return CSSPropertyParsing::consumeSingleBackgroundRepeat(range, state);
        case CSSPropertyID::BackgroundPositionX:
            return CSSPropertyParsing::consumeSingleBackgroundPositionX(range, state);
        case CSSPropertyID::BackgroundPositionY:
            return CSSPropertyParsing::consumeSingleBackgroundPositionY(range, state);
        case CSSPropertyID::BackgroundSize:
            return consumeSingleBackgroundSize(range, state);
        case CSSPropertyID::BackgroundColor:
            return consumeColor(range, state);

        // mask-*
        case CSSPropertyID::MaskComposite:
            return CSSPropertyParsing::consumeSingleMaskComposite(range);
        case CSSPropertyID::MaskOrigin:
            return CSSPropertyParsing::consumeSingleMaskOrigin(range);
        case CSSPropertyID::MaskClip:
            return CSSPropertyParsing::consumeSingleMaskClip(range);
        case CSSPropertyID::MaskImage:
            return CSSPropertyParsing::consumeSingleMaskImage(range, state);
        case CSSPropertyID::MaskMode:
            return CSSPropertyParsing::consumeSingleMaskMode(range);
        case CSSPropertyID::MaskRepeat:
            return CSSPropertyParsing::consumeSingleMaskRepeat(range, state);
        case CSSPropertyID::MaskSize:
            return consumeSingleMaskSize(range, state);

        // -webkit-background-*
        case CSSPropertyID::WebkitBackgroundSize:
            return consumeSingleWebkitBackgroundSize(range, state);
        case CSSPropertyID::WebkitBackgroundClip:
            return CSSPropertyParsing::consumeSingleWebkitBackgroundClip(range);
        case CSSPropertyID::WebkitBackgroundOrigin:
            return CSSPropertyParsing::consumeSingleWebkitBackgroundOrigin(range);

        // -webkit-mask-*
        case CSSPropertyID::WebkitMaskClip:
            return CSSPropertyParsing::consumeSingleWebkitMaskClip(range);
        case CSSPropertyID::WebkitMaskComposite:
            return CSSPropertyParsing::consumeSingleWebkitMaskComposite(range);
        case CSSPropertyID::WebkitMaskSourceType:
            return CSSPropertyParsing::consumeSingleWebkitMaskSourceType(range);
        case CSSPropertyID::WebkitMaskPositionX:
            return CSSPropertyParsing::consumeSingleWebkitMaskPositionX(range, state);
        case CSSPropertyID::WebkitMaskPositionY:
            return CSSPropertyParsing::consumeSingleWebkitMaskPositionY(range, state);

        default:
            return nullptr;
        };
    };

    auto shorthandProperties = shorthand.properties();
    unsigned longhandCount = shorthand.length();

    // mask resets mask-border properties outside of this method.
    if (shorthand.id() == CSSPropertyID::Mask)
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

                if (property == CSSPropertyID::BackgroundPositionX || property == CSSPropertyID::WebkitMaskPositionX) {
                    // Note: This assumes y properties (for example background-position-y) follow the x properties in the shorthand array.
                    auto position = consumeBackgroundPositionUnresolved(range, state);
                    if (!position)
                        continue;
                    auto [positionX, positionY] = split(WTF::move(*position));
                    value = CSSPositionXValue::create(WTF::move(positionX));
                    valueY = CSSPositionYValue::create(WTF::move(positionY));
                } else if (property == CSSPropertyID::BackgroundSize) {
                    if (!consumeSlashIncludingWhitespace(range))
                        continue;
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleBackgroundSize(range, state);
                    if (!value)
                        return false;
                } else if (property == CSSPropertyID::MaskSize) {
                    if (!consumeSlashIncludingWhitespace(range))
                        continue;
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleMaskSize(range, state);
                    if (!value)
                        return false;
                } else if (property == CSSPropertyID::BackgroundPositionY || property == CSSPropertyID::WebkitMaskPositionY) {
                    continue;
                } else {
                    value = consumeBackgroundComponent(property);
                }
                if (value) {
                    if (property == CSSPropertyID::BackgroundOrigin || property == CSSPropertyID::MaskOrigin)
                        originValue = value;
                    else if (property == CSSPropertyID::BackgroundClip)
                        clipIsBorderArea = isValueID(value, CSSValueID::BorderArea);
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
            if (property == CSSPropertyID::BackgroundColor && !range.atEnd()) {
                if (parsedLonghand[i])
                    return false; // Colors are only allowed in the last layer.
                continue;
            }
            if ((property == CSSPropertyID::BackgroundClip || property == CSSPropertyID::MaskClip || property == CSSPropertyID::WebkitMaskClip) && !parsedLonghand[i] && originValue) {
                longhands[i].append(originValue.releaseNonNull());
                continue;
            }
            if (clipIsBorderArea && (property == CSSPropertyID::BackgroundOrigin) && !parsedLonghand[i]) {
                longhands[i].append(CSSKeywordValue::create(CSSValueID::BorderBox));
                continue;
            }
            if (!parsedLonghand[i])
                longhands[i].append(protect(CSSKeywordValue::implicitInitialValue()));
        }
    } while (consumeCommaIncludingWhitespace(range));
    if (!range.atEnd())
        return false;

    for (size_t i = 0; i < longhandCount; ++i) {
        auto property = shorthandProperties[i];
        if (longhands[i].size() == 1)
            result.addPropertyForCurrentShorthand(state, property, WTF::move(longhands[i][0]));
        else
            result.addPropertyForCurrentShorthand(state, property, CSSValueList::createCommaSeparated(WTF::move(longhands[i])));
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
        auto [positionX, positionY] = split(WTF::move(*position));
        x.append(CSSPositionXValue::create(WTF::move(positionX)));
        y.append(CSSPositionYValue::create(WTF::move(positionY)));
    } while (consumeCommaIncludingWhitespace(range));

    if (!range.atEnd())
        return false;

    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;
    if (x.size() == 1) {
        resultX = WTF::move(x[0]);
        resultY = WTF::move(y[0]);
    } else {
        resultX = CSSValueList::createCommaSeparated(WTF::move(x));
        resultY = CSSValueList::createCommaSeparated(WTF::move(y));
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
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BackgroundSize, backgroundSize.releaseNonNull());
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
        auto [positionX, positionY] = split(WTF::move(*position));
        x.append(CSSPositionXValue::create(WTF::move(positionX)));
        y.append(CSSPositionYValue::create(WTF::move(positionY)));
    } while (consumeCommaIncludingWhitespace(range));

    if (!range.atEnd())
        return false;

    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;
    if (x.size() == 1) {
        resultX = WTF::move(x[0]);
        resultY = WTF::move(y[0]);
    } else {
        resultX = CSSValueList::createCommaSeparated(WTF::move(x));
        resultY = CSSValueList::createCommaSeparated(WTF::move(y));
    }

    auto longhands = shorthand.properties();
    result.addPropertyForCurrentShorthand(state, longhands[0], resultX.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], resultY.releaseNonNull());
    return true;
}

inline bool PropertyParserCustom::consumeOverflowShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSValueID xValueID = range.consumeIncludingWhitespace().id();
    if (!CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyID::OverflowY, xValueID, state))
        return false;

    CSSValueID yValueID;
    if (range.atEnd()) {
        yValueID = xValueID;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (xValueID == CSSValueID::WebkitPagedX || xValueID == CSSValueID::WebkitPagedY)
            xValueID = CSSValueID::Auto;
    } else
        yValueID = range.consumeIncludingWhitespace().id();

    if (!CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyID::OverflowY, yValueID, state))
        return false;
    if (!range.atEnd())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OverflowX, CSSKeywordValue::create(xValueID));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OverflowY, CSSKeywordValue::create(yValueID));
    return true;
}

inline bool PropertyParserCustom::consumeGridItemPositionShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.id() == state.currentProperty);
    ASSERT(shorthand.length() == 2);

    auto startValue = consumeUnresolvedGridLine(range, state);
    if (!startValue)
        return false;

    std::optional<CSS::GridLine> endValue;
    if (consumeSlashIncludingWhitespace(range)) {
        endValue = consumeUnresolvedGridLine(range, state);
        if (!endValue)
            return false;
    } else {
        endValue = startValue->isCustomIdent() ? startValue : CSS::GridLine { CSS::Keyword::Auto { } };
    }
    if (!range.atEnd())
        return false;

    auto longhands = shorthand.properties();
    result.addPropertyForCurrentShorthand(state, longhands[0], CSSGridLineValue::create(WTF::move(*startValue)));
    result.addPropertyForCurrentShorthand(state, longhands[1], CSSGridLineValue::create(WTF::move(*endValue)));
    return true;
}

inline bool PropertyParserCustom::consumeGridAreaShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto rowStart = consumeUnresolvedGridLine(range, state);
    if (!rowStart)
        return false;
    std::optional<CSS::GridLine> columnStart;
    std::optional<CSS::GridLine> rowEnd;
    std::optional<CSS::GridLine> columnEnd;
    if (consumeSlashIncludingWhitespace(range)) {
        columnStart = consumeUnresolvedGridLine(range, state);
        if (!columnStart)
            return false;
        if (consumeSlashIncludingWhitespace(range)) {
            rowEnd = consumeUnresolvedGridLine(range, state);
            if (!rowEnd)
                return false;
            if (consumeSlashIncludingWhitespace(range)) {
                columnEnd = consumeUnresolvedGridLine(range, state);
                if (!columnEnd)
                    return false;
            }
        }
    }
    if (!range.atEnd())
        return false;
    if (!columnStart)
        columnStart = rowStart->isCustomIdent() ? rowStart : CSS::GridLine { CSS::Keyword::Auto { } };
    if (!rowEnd)
        rowEnd = rowStart->isCustomIdent() ? rowStart : CSS::GridLine { CSS::Keyword::Auto { } };
    if (!columnEnd)
        columnEnd = columnStart->isCustomIdent() ? columnStart : CSS::GridLine { CSS::Keyword::Auto { } };

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridRowStart, CSSGridLineValue::create(WTF::move(*rowStart)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridColumnStart, CSSGridLineValue::create(WTF::move(*columnStart)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridRowEnd, CSSGridLineValue::create(WTF::move(*rowEnd)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridColumnEnd, CSSGridLineValue::create(WTF::move(*columnEnd)));
    return true;
}

inline bool PropertyParserCustom::consumeGridTemplateShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSParserTokenRange rangeCopy = range;
    RefPtr<CSSValue> rowsValue = consumeIdent<CSSValueID::None>(range);

    // 1- 'none' case.
    if (rowsValue && range.atEnd()) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateRows, CSSKeywordValue::create(CSSValueID::None));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateColumns, CSSKeywordValue::create(CSSValueID::None));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateAreas, CSSKeywordValue::create(CSSValueID::None));
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns>
    if (!rowsValue)
        rowsValue = consumeGridTemplateList(range, state);

    if (rowsValue) {
        if (!consumeSlashIncludingWhitespace(range))
            return false;
        RefPtr columnsValue = consumeGridTemplateList(range, state);
        if (!columnsValue || !range.atEnd())
            return false;

        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateRows, rowsValue.releaseNonNull());
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateColumns, columnsValue.releaseNonNull());
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateAreas, CSSKeywordValue::create(CSSValueID::None));
        return true;
    }

    // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <explicit-track-list> ]?

    range = rangeCopy;

    CSS::GridNamedAreaMap gridAreaMap;
    CSS::GridTrackList templateRowsTrackList;

    // Persists between loop iterations so we can use the same value for
    // consecutive <line-names> values
    std::optional<CSS::GridLineNames> lineNames;

    do {
        // Handle leading <line-names>?.
        auto previousLineNames = std::exchange(lineNames, consumeUnresolvedGridLineNames(range, state));
        if (lineNames && !lineNames->isEmpty()) {
            if (!previousLineNames)
                templateRowsTrackList.value.value.append(CSS::GridTrackList::Track { WTF::move(*lineNames) });
            else {
                CSS::GridLineNames combinedLineNames;
                combinedLineNames.value.value.reserveInitialCapacity(previousLineNames->size() + lineNames->size());
                combinedLineNames.value.value.appendVector(previousLineNames->value.value);
                combinedLineNames.value.value.appendVector(lineNames->value.value);

                templateRowsTrackList.value.value.last() = CSS::GridTrackList::Track { WTF::move(combinedLineNames) };
            }
        }

        // Handle a template-area's row <string>.
        auto row = consumeUnresolvedGridTemplateAreasRow(range, state);
        if (!row || !CSS::addRow(gridAreaMap, *row))
            return false;

        // Handle template-rows's <track-size>.
        if (auto trackSize = consumeUnresolvedGridTrackSize(range, state))
            templateRowsTrackList.value.value.append(CSS::GridTrackList::Track { WTF::move(*trackSize) });
        else
            templateRowsTrackList.value.value.append(CSS::GridTrackList::Track { CSS::GridTrackSize { CSS::GridTrackBreadth { CSS::Keyword::Auto { } } } });

        // This will handle the trailing/leading <line-names>? in the grammar.
        lineNames = consumeUnresolvedGridLineNames(range, state);
        if (lineNames && !lineNames->isEmpty())
            templateRowsTrackList.value.value.append(CSS::GridTrackList::Track { *lineNames });
    } while (!range.atEnd() && !(range.peek().type() == DelimiterToken && range.peek().delimiter() == '/'));

    RefPtr<CSSValue> templateColumnsValue;
    if (!range.atEnd()) {
        if (!consumeSlashIncludingWhitespace(range))
            return false;
        auto templateColumnsTrackList = consumeUnresolvedGridExplicitTrackList(range, state);
        if (!templateColumnsTrackList || !range.atEnd())
            return false;
        templateColumnsValue = CSSGridTemplateListValue::create(CSS::GridTemplateList { WTF::move(*templateColumnsTrackList) });
    } else
        templateColumnsValue = CSSKeywordValue::create(CSSValueID::None);

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateRows, CSSGridTemplateListValue::create(CSS::GridTemplateList { WTF::move(templateRowsTrackList) }));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateColumns, templateColumnsValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateAreas, CSSGridTemplateAreasValue::create(WTF::move(gridAreaMap)));
    return true;
}

inline bool PropertyParserCustom::consumeGridShorthand(CSSParserTokenRange& range, PropertyParserState& state, [[maybe_unused]] const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 6);

    auto consumeImplicitGridAutoFlow = [](CSSParserTokenRange& range, auto flowDirection) -> RefPtr<CSSValue> {
        // [ auto-flow && dense? ]
        bool autoFlow = consumeIdentRaw<CSSValueID::AutoFlow>(range).has_value();
        bool dense = consumeIdentRaw<CSSValueID::Dense>(range).has_value();
        if (!autoFlow && (!dense || !consumeIdentRaw<CSSValueID::AutoFlow>(range)))
            return nullptr;
        if (!dense)
            return CSSGridAutoFlowValue::create(CSS::GridAutoFlow { flowDirection });
        if (flowDirection.value == CSSValueID::Row)
            return CSSGridAutoFlowValue::create(CSS::GridAutoFlow { CSS::Keyword::Dense { } });
        return CSSGridAutoFlowValue::create(CSS::GridAutoFlow { flowDirection, CSS::Keyword::Dense { } });
    };

    auto rangeCopy = range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(range, state, gridTemplateShorthand(), result)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridAutoFlow, CSSKeywordValue::create(CSSValueID::Row));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridAutoColumns, CSSKeywordValue::create(CSSValueID::Auto));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridAutoRows, CSSKeywordValue::create(CSSValueID::Auto));

        return true;
    }

    range = rangeCopy;

    RefPtr<CSSValue> autoColumnsValue;
    RefPtr<CSSValue> autoRowsValue;
    RefPtr<CSSValue> templateRows;
    RefPtr<CSSValue> templateColumns;
    RefPtr<CSSValue> gridAutoFlow;

    if (range.peek().id() == CSSValueID::AutoFlow || range.peek().id() == CSSValueID::Dense) {
        // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
        gridAutoFlow = consumeImplicitGridAutoFlow(range, CSS::Keyword::Row { });
        if (!gridAutoFlow || range.atEnd())
            return false;
        if (consumeSlashIncludingWhitespace(range))
            autoRowsValue = CSSKeywordValue::create(CSSValueID::Auto);
        else {
            autoRowsValue = consumeGridTrackSizes(range, state);
            if (!autoRowsValue)
                return false;
            if (!consumeSlashIncludingWhitespace(range))
                return false;
        }
        if (range.atEnd())
            return false;
        templateColumns = consumeGridTemplateList(range, state);
        if (!templateColumns)
            return false;
        templateRows = CSSKeywordValue::create(CSSValueID::None);
        autoColumnsValue = CSSKeywordValue::create(CSSValueID::Auto);
    } else {
        // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
        templateRows = consumeGridTemplateList(range, state);
        if (!templateRows)
            return false;
        if (!consumeSlashIncludingWhitespace(range) || range.atEnd())
            return false;
        gridAutoFlow = consumeImplicitGridAutoFlow(range, CSS::Keyword::Column { });
        if (!gridAutoFlow)
            return false;
        if (range.atEnd())
            autoColumnsValue = CSSKeywordValue::create(CSSValueID::Auto);
        else {
            autoColumnsValue = consumeGridTrackSizes(range, state);
            if (!autoColumnsValue)
                return false;
        }
        templateColumns = CSSKeywordValue::create(CSSValueID::None);
        autoRowsValue = CSSKeywordValue::create(CSSValueID::Auto);
    }

    if (!range.atEnd())
        return false;

    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateColumns, templateColumns.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateRows, templateRows.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridTemplateAreas, CSSKeywordValue::create(CSSValueID::None));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridAutoFlow, gridAutoFlow.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridAutoColumns, autoColumnsValue.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::GridAutoRows, autoRowsValue.releaseNonNull());

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
        size = CSSKeywordValue::create(CSSValueID::None);
    if (!insert)
        insert = CSSKeywordValue::create(CSSValueID::MarginBox);
    if (!align)
        align = CSSKeywordValue::create(CSSValueID::Auto);
    if (!round)
        round = CSSKeywordValue::create(CSSValueID::Up);

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BlockStepSize, WTF::move(size));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BlockStepInsert, WTF::move(insert));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BlockStepAlign, WTF::move(align));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BlockStepRound, WTF::move(round));
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

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OverscrollBehaviorX, WTF::move(overscrollBehaviorX));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OverscrollBehaviorY, WTF::move(overscrollBehaviorY));
    return true;
}

inline bool PropertyParserCustom::consumeContainerShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr name = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::ContainerName, state);
    if (!name)
        return false;

    bool sawSlash = false;

    auto consumeSlashType = [&]() -> RefPtr<CSSValue> {
        if (range.atEnd())
            return nullptr;
        if (!consumeSlashIncludingWhitespace(range))
            return nullptr;
        sawSlash = true;
        return CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::ContainerType, state);
    };

    auto type = consumeSlashType();

    if (!range.atEnd() || (sawSlash && !type))
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ContainerName, name.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ContainerType, WTF::move(type));
    return true;
}

inline bool PropertyParserCustom::consumeContainIntrinsicSizeShorthand(CSSParserTokenRange& range, PropertyParserState& state, [[maybe_unused]] const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(shorthand.length() == 2);
    ASSERT(isExposed(CSSPropertyID::ContainIntrinsicSize, &state.context.propertySettings));

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

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ContainIntrinsicWidth, WTF::move(containIntrinsicWidth));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ContainIntrinsicHeight, WTF::move(containIntrinsicHeight));
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

        auto [positionX, positionY] = split(WTF::move(*position));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::TransformOriginX, CSSPositionXValue::create(WTF::move(positionX)));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::TransformOriginY, CSSPositionYValue::create(WTF::move(positionY)));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::TransformOriginZ, resultZ);
        return true;
    }
    return false;
}

inline bool PropertyParserCustom::consumePerspectiveOriginShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (auto position = consumePositionUnresolved(range, state)) {
        if (!range.atEnd())
            return false;
        auto [positionX, positionY] = split(WTF::move(*position));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::PerspectiveOriginX, CSSPositionXValue::create(WTF::move(positionX)));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::PerspectiveOriginY, CSSPositionYValue::create(WTF::move(positionY)));
        return true;
    }

    return false;
}

inline bool PropertyParserCustom::consumeWebkitPerspectiveShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (RefPtr value = CSSPropertyParsing::consumePerspective(range, state)) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::Perspective, value.releaseNonNull());
        return range.atEnd();
    }

    if (auto perspective = CSSPrimitiveValueResolver<Number<Nonnegative>>::consumeAndResolve(range, state)) {
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::Perspective, WTF::move(perspective));
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
    auto offsetPosition = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::OffsetPosition, state);

    // Parse out offset-path.
    auto offsetPath = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::OffsetPath, state);

    // Either one of offset-position and offset-path must be present.
    if (!offsetPosition && !offsetPath)
        return false;

    // Only parse offset-distance and offset-rotate if offset-path is specified.
    RefPtr<CSSValue> offsetDistance;
    RefPtr<CSSValue> offsetRotate;
    if (offsetPath) {
        // Try to parse offset-distance first. If successful, parse the following offset-rotate.
        // Otherwise, parse in the reverse order.
        if ((offsetDistance = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::OffsetDistance, state)))
            offsetRotate = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::OffsetRotate, state);
        else {
            offsetRotate = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::OffsetRotate, state);
            offsetDistance = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::OffsetDistance, state);
        }
    }

    // Parse out offset-anchor. Only parse if the prefix slash is present.
    RefPtr<CSSValue> offsetAnchor;
    if (consumeSlashIncludingWhitespace(range)) {
        // offset-anchor must follow the slash.
        if (!(offsetAnchor = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::OffsetAnchor, state)))
            return false;
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OffsetPath, WTF::move(offsetPath));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OffsetDistance, WTF::move(offsetDistance));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OffsetPosition, WTF::move(offsetPosition));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OffsetAnchor, WTF::move(offsetAnchor));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::OffsetRotate, WTF::move(offsetRotate));

    return range.atEnd();
}

inline bool PropertyParserCustom::consumeListStyleShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr<CSSValue> position;
    RefPtr<CSSValue> image;
    RefPtr<CSSValue> type;
    unsigned noneCount = 0;

    while (!range.atEnd()) {
        if (range.peek().id() == CSSValueID::None) {
            ++noneCount;
            consumeIdent(range);
            continue;
        }
        if (!position && (position = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::ListStylePosition, state)))
            continue;

        if (!image && (image = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::ListStyleImage, state)))
            continue;

        if (!type && (type = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::ListStyleType, state)))
            continue;

        return false;
    }

    if (noneCount > (static_cast<unsigned>(!image + !type)))
        return false;

    if (noneCount == 2) {
        // Using implicit none for list-style-image is how we serialize "none" instead of "none none".
        image = nullptr;
        type = CSSKeywordValue::create(CSSValueID::None);
    } else if (noneCount == 1) {
        // Use implicit none for list-style-image, but non-implicit for type.
        if (!type)
            type = CSSKeywordValue::create(CSSValueID::None);
    }

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ListStylePosition, WTF::move(position));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ListStyleImage, WTF::move(image));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ListStyleType, WTF::move(type));
    return range.atEnd();
}

bool PropertyParserCustom::consumeLineClampShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    bool isLegacyLineClamp = shorthand.id() == CSSPropertyID::WebkitLineClamp;
    ASSERT(state.context.propertySettings.cssLineClampEnabled || isLegacyLineClamp);

    if (range.peek().id() == CSSValueID::None) {
        // Sets max-lines to none, continue to auto, and block-ellipsis to none.
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaxLines, CSSKeywordValue::create(CSSValueID::Auto));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::Continue, CSSKeywordValue::create(CSSValueID::Auto));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::BlockEllipsis, CSSKeywordValue::create(CSSValueID::NoEllipsis));
        consumeIdent(range);
        return range.atEnd();
    }

    RefPtr<CSSValue> numLines;
    RefPtr<CSSValue> blockEllipsis;
    RefPtr<CSSKeywordValue> autoKeyword;
    RefPtr<CSSValue> webkitLegacy;

    do {
        if (!webkitLegacy && (webkitLegacy = consumeIdent<CSSValueID::WebkitLegacy>(range)))
            break;
        if (!autoKeyword && (autoKeyword = consumeIdent<CSSValueID::Auto>(range))) {
            if (blockEllipsis && numLines)
                return false;
            continue;
        }
        if (!blockEllipsis && (blockEllipsis = CSSPropertyParsing::consumeBlockEllipsis(range)))
            continue;
        if (!numLines && (numLines = CSSPrimitiveValueResolver<CSS::Integer<CSS::Range { 1, CSS::Range::infinity } >>::consumeAndResolve(range, state))) {
            if (blockEllipsis && autoKeyword)
                return false;
            continue;
        }
        break;
    } while (!range.atEnd());

    if (isLegacyLineClamp && (autoKeyword || blockEllipsis || webkitLegacy))
        return false;
    if (!numLines && !autoKeyword && !blockEllipsis)
        return false;

    RefPtr<CSSValue> maxLines;
    if (numLines && autoKeyword) {
        CSSValueListBuilder list;
        list.append(numLines.releaseNonNull());
        list.append(autoKeyword.releaseNonNull());
        maxLines = CSSValueList::createSpaceSeparated(WTF::move(list));
    } else if (numLines)
        maxLines = numLines;
    else if (autoKeyword)
        maxLines = autoKeyword;
    else
        maxLines = CSSKeywordValue::create(CSSValueID::Auto);

    if (!blockEllipsis)
        blockEllipsis = CSSKeywordValue::create(CSSValueID::Ellipsis);

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MaxLines, WTF::move(maxLines));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::BlockEllipsis, WTF::move(blockEllipsis));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::Continue, CSSKeywordValue::create(isLegacyLineClamp || webkitLegacy ? CSSValueID::WebkitLegacy : CSSValueID::Discard));
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeHyphenateLimitCharsShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    ASSERT(state.currentProperty == shorthand.id());
    ASSERT(shorthand.length() == 3);
    auto longhands = shorthand.properties();

    // <'hyphenate-limit-chars'> = [ auto | <integer [0,∞]> ]{1,3}
    // First value is the total minimum, second is the minimum before the hyphen (defaulting
    // to auto if omitted), and third is the minimum after the hyphen (defaulting to the
    // before value if omitted).
    RefPtr total = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[0], state);
    if (!total)
        return false;

    RefPtr before = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[1], state);
    auto beforeImplicit = !before ? IsImplicit::Yes : IsImplicit::No;
    if (beforeImplicit == IsImplicit::Yes)
        before = CSSKeywordValue::create(CSSValueID::Auto);

    RefPtr after = CSSPropertyParsing::parseStylePropertyLonghand(range, longhands[2], state);
    auto afterImplicit = !after ? IsImplicit::Yes : IsImplicit::No;
    if (afterImplicit == IsImplicit::Yes)
        after = before;

    result.addPropertyForCurrentShorthand(state, longhands[0], total.releaseNonNull());
    result.addPropertyForCurrentShorthand(state, longhands[1], before.releaseNonNull(), beforeImplicit);
    result.addPropertyForCurrentShorthand(state, longhands[2], after.releaseNonNull(), afterImplicit);
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeTextBoxShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    if (range.peek().id() == CSSValueID::Normal) {
        // if the single keyword normal is specified, it sets text-box-trim to none and text-box-edge to auto.
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextBoxTrim, CSSKeywordValue::create(CSSValueID::None));
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextBoxEdge, CSSKeywordValue::create(CSSValueID::Auto));
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
        textBoxEdge = CSSKeywordValue::create(CSSValueID::Auto);

    // Omitting the text-box-trim value sets it to both (not the initial value)
    if (!textBoxTrim)
        textBoxTrim = CSSKeywordValue::create(CSSValueID::TrimBoth);

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextBoxTrim, WTF::move(textBoxTrim));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextBoxEdge, WTF::move(textBoxEdge));
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
        mode = CSSKeywordValue::create(CSSValueID::Wrap);
    if (!style)
        style = CSSKeywordValue::create(CSSValueID::Auto);

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextWrapMode, WTF::move(mode));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextWrapStyle, WTF::move(style));
    return true;
}

inline bool PropertyParserCustom::consumeWhiteSpaceShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr<CSSValue> whiteSpaceCollapse;
    RefPtr<CSSValue> textWrapMode;
    RefPtr<CSSValue> whiteSpaceTrim;

    // Single value syntax.
    auto singleValueKeyword = consumeIdentRaw<
        CSSValueID::Normal,
        CSSValueID::Pre,
        CSSValueID::PreLine,
        CSSValueID::PreWrap
    >(range);

    if (singleValueKeyword) {
        switch (*singleValueKeyword) {
        case CSSValueID::Normal:
            whiteSpaceCollapse = CSSKeywordValue::create(CSSValueID::Collapse);
            textWrapMode = CSSKeywordValue::create(CSSValueID::Wrap);
            break;
        case CSSValueID::Pre:
            whiteSpaceCollapse = CSSKeywordValue::create(CSSValueID::Preserve);
            textWrapMode = CSSKeywordValue::create(CSSValueID::Nowrap);
            break;
        case CSSValueID::PreLine:
            whiteSpaceCollapse = CSSKeywordValue::create(CSSValueID::PreserveBreaks);
            textWrapMode = CSSKeywordValue::create(CSSValueID::Wrap);
            break;
        case CSSValueID::PreWrap:
            whiteSpaceCollapse = CSSKeywordValue::create(CSSValueID::Preserve);
            textWrapMode = CSSKeywordValue::create(CSSValueID::Wrap);
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
        whiteSpaceTrim = CSSKeywordValue::create(CSSValueID::None);
    } else {
        // Multi-value syntax.
        for (unsigned propertiesParsed = 0; propertiesParsed < 3 && !range.atEnd(); ++propertiesParsed) {
            if (!whiteSpaceCollapse && (whiteSpaceCollapse = CSSPropertyParsing::consumeWhiteSpaceCollapse(range)))
                continue;
            if (!textWrapMode && (textWrapMode = CSSPropertyParsing::consumeTextWrapMode(range)))
                continue;
            if (!whiteSpaceTrim && state.context.propertySettings.cssWhiteSpaceTrimEnabled && (whiteSpaceTrim = CSSPropertyParsing::consumeWhiteSpaceTrim(range)))
                continue;
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    if (!range.atEnd())
        return false;

    // Fill in default values if one was missing from the multi-value syntax.
    if (!whiteSpaceCollapse)
        whiteSpaceCollapse = CSSKeywordValue::create(CSSValueID::Collapse);
    if (!textWrapMode)
        textWrapMode = CSSKeywordValue::create(CSSValueID::Wrap);
    if (!whiteSpaceTrim)
        whiteSpaceTrim = CSSKeywordValue::create(CSSValueID::None);

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::WhiteSpaceCollapse, WTF::move(whiteSpaceCollapse));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::TextWrapMode, WTF::move(textWrapMode));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::WhiteSpaceTrim, WTF::move(whiteSpaceTrim));
    return true;
}

inline bool PropertyParserCustom::consumeAnimationRangeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
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
                auto isRangeOffset = [](const CSSPrimitiveValue& value) {
                    return value.isLength() || value.isPercentage() || value.isCalculatedPercentageWithLength();
                };

                if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value); primitiveValue && isRangeOffset(*primitiveValue))
                    return CSSKeywordValue::create(CSSValueID::Normal);
                return CSSKeywordValue::create(valueID(value));
            };

            if (RefPtr startPair = dynamicDowncast<CSSValuePair>(start))
                end = rangeEndValueForStartValue(startPair->first());
            else
                end = rangeEndValueForStartValue(*start);
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

    ASSERT(shorthand.properties().size() == 2);
    result.addPropertyForCurrentShorthand(state, shorthand.properties()[0], CSSValueList::createCommaSeparated(WTF::move(startList)));
    result.addPropertyForCurrentShorthand(state, shorthand.properties()[1], CSSValueList::createCommaSeparated(WTF::move(endList)));
    return true;
}

inline bool PropertyParserCustom::consumeTimelineTriggerActivationRangeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    return consumeAnimationRangeShorthand(range, state, shorthand, result);
}

inline bool PropertyParserCustom::consumeTimelineTriggerActiveRangeShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand& shorthand, PropertyParserResult& result)
{
    return consumeAnimationRangeShorthand(range, state, shorthand, result);
}

inline bool PropertyParserCustom::consumeScrollTimelineShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSValueListBuilder namesList;
    CSSValueListBuilder axesList;

    do {
        // A valid scroll-timeline-name is required.
        if (RefPtr name = CSSPropertyParsing::consumeSingleScrollTimelineName(range, state))
            namesList.append(name.releaseNonNull());
        else
            return false;

        // A scroll-timeline-axis is optional.
        if (range.peek().type() == CommaToken || range.atEnd())
            axesList.append(CSSKeywordValue::create(CSSValueID::Block));
        else if (auto axis = CSSPropertyParsing::consumeAxis(range))
            axesList.append(axis.releaseNonNull());
        else
            return false;
    } while (consumeCommaIncludingWhitespace(range));

    if (namesList.isEmpty())
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ScrollTimelineName, CSSValueList::createCommaSeparated(WTF::move(namesList)));
    if (!axesList.isEmpty())
        result.addPropertyForCurrentShorthand(state, CSSPropertyID::ScrollTimelineAxis, CSSValueList::createCommaSeparated(WTF::move(axesList)));
    return true;
}

inline bool PropertyParserCustom::consumeViewTimelineShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    CSSValueListBuilder namesList;
    CSSValueListBuilder axesList;
    CSSValueListBuilder insetsList;

    auto defaultAxis = [] -> Ref<CSSValue> { return CSSKeywordValue::create(CSSValueID::Block); };
    auto defaultInsets = [] -> Ref<CSSValue> { return CSSKeywordValue::create(CSSValueID::Auto); };

    do {
        // A valid view-timeline-name is required.
        if (RefPtr name = CSSPropertyParsing::consumeSingleScrollTimelineName(range, state))
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

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ViewTimelineName, CSSValueList::createCommaSeparated(WTF::move(namesList)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ViewTimelineAxis, CSSValueList::createCommaSeparated(WTF::move(axesList)));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::ViewTimelineInset, CSSValueList::createCommaSeparated(WTF::move(insetsList)));
    return true;
}

inline bool PropertyParserCustom::consumePositionTryShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    auto order = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::PositionTryOrder, state);
    auto fallbacks = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::PositionTryFallbacks, state);
    if (!fallbacks)
        return false;

    result.addPropertyForCurrentShorthand(state, CSSPropertyID::PositionTryOrder, WTF::move(order));
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::PositionTryFallbacks, WTF::move(fallbacks));
    return range.atEnd();
}

inline bool PropertyParserCustom::consumeMarkerShorthand(CSSParserTokenRange& range, PropertyParserState& state, const StylePropertyShorthand&, PropertyParserResult& result)
{
    RefPtr marker = CSSPropertyParsing::parseStylePropertyLonghand(range, CSSPropertyID::MarkerStart, state);
    if (!marker || !range.atEnd())
        return false;

    Ref markerRef = marker.releaseNonNull();
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MarkerStart, markerRef.copyRef());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MarkerMid, markerRef.copyRef());
    result.addPropertyForCurrentShorthand(state, CSSPropertyID::MarkerEnd, WTF::move(markerRef));
    return true;
}

} // namespace CSS
} // namespace WebCore
