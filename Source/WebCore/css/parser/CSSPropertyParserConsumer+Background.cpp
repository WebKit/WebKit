/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Background.h"

#include "CSSBackgroundRepeatValue.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSBorderRadius.h"
#include "CSSCalcTree+Parser.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+LengthPercentage.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParsing.h"
#include "CSSQuadValue.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename ElementType> static void complete4Sides(std::array<ElementType, 4>& sides)
{
    if (!sides[1])
        sides[1] = sides[0];
    if (!sides[2])
        sides[2] = sides[0];
    if (!sides[3])
        sides[3] = sides[1];
}

// MARK: - Border Radius

enum class SupportWebKitBorderRadiusQuirk : bool { No, Yes };

template<SupportWebKitBorderRadiusQuirk supportQuirk> static std::optional<CSS::BorderRadius> consumeBorderRadius(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-radius

    using OptionalRadiiForAxis = std::array<std::optional<CSS::LengthPercentage<CSS::Nonnegative>>, 4>;

    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    OptionalRadiiForAxis horizontalRadii;
    unsigned i = 0;
    for (; i < 4 && !range.atEnd() && range.peek().type() != DelimiterToken; ++i) {
        horizontalRadii[i] = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, context, { }, options);
        if (!horizontalRadii[i])
            return { };
    }
    if (!horizontalRadii[0])
        return { };

    if (range.atEnd()) {
        if constexpr (supportQuirk == SupportWebKitBorderRadiusQuirk::Yes) {
            // Legacy syntax: `-webkit-border-radius: l1 l2` is equivalent to border-radius: `l1 / l2`.
            if (i == 2) {
                OptionalRadiiForAxis verticalRadii;
                verticalRadii[0] = horizontalRadii[1];
                horizontalRadii[1] = std::nullopt;

                return CSS::BorderRadius {
                    .horizontal = completeQuadFromArray<CSS::BorderRadius::Axis>(WTFMove(horizontalRadii)),
                    .vertical = completeQuadFromArray<CSS::BorderRadius::Axis>(WTFMove(verticalRadii))
                };
            }
        }

        auto horizontal = completeQuadFromArray<CSS::BorderRadius::Axis>(WTFMove(horizontalRadii));
        auto vertical = horizontal; // Copy `horizontal` radii to `vertical`.

        return CSS::BorderRadius {
            .horizontal = WTFMove(horizontal),
            .vertical = WTFMove(vertical)
        };
    }

    if (!consumeSlashIncludingWhitespace(range))
        return { };

    OptionalRadiiForAxis verticalRadii;
    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        verticalRadii[i] = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, context, { }, options);
        if (!verticalRadii[i])
            return { };
    }
    if (!verticalRadii[0] || !range.atEnd())
        return { };

    return CSS::BorderRadius {
        .horizontal = completeQuadFromArray<CSS::BorderRadius::Axis>(WTFMove(horizontalRadii)),
        .vertical = completeQuadFromArray<CSS::BorderRadius::Axis>(WTFMove(verticalRadii))
    };
}

std::optional<CSS::BorderRadius> consumeUnresolvedBorderRadius(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-radius

    return consumeBorderRadius<SupportWebKitBorderRadiusQuirk::No>(range, context);
}

std::optional<CSS::BorderRadius> consumeUnresolvedWebKitBorderRadius(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-radius

    // Includes the legacy syntax quirk where `-webkit-border-radius: l1 l2` is equivalent to border-radius: `l1 / l2`.
    return consumeBorderRadius<SupportWebKitBorderRadiusQuirk::Yes>(range, context);
}

RefPtr<CSSValue> consumeBorderRadiusCorner(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-[top|bottom]-[left|right]-radius,'> = <length-percentage [0,∞]>{1,2}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-top-left-radius

    auto parsedValue1 = consumeLengthPercentage(range, context, ValueRange::NonNegative);
    if (!parsedValue1)
        return nullptr;
    auto parsedValue2 = consumeLengthPercentage(range, context, ValueRange::NonNegative);
    if (!parsedValue2)
        parsedValue2 = parsedValue1;
    return CSSValuePair::create(parsedValue1.releaseNonNull(), parsedValue2.releaseNonNull());
}

// MARK: - Border Image

static RefPtr<CSSPrimitiveValue> consumeBorderImageRepeatKeyword(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueStretch, CSSValueRepeat, CSSValueSpace, CSSValueRound>(range);
}

RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'border-image-repeat'> = [ stretch | repeat | round | space ]{1,2}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-repeat

    auto horizontal = consumeBorderImageRepeatKeyword(range);
    if (!horizontal)
        return nullptr;
    auto vertical = consumeBorderImageRepeatKeyword(range);
    if (!vertical)
        vertical = horizontal;
    return CSSValuePair::create(horizontal.releaseNonNull(), vertical.releaseNonNull());
}

RefPtr<CSSValue> consumeBorderImageSlice(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property)
{
    // <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice

    bool fill = consumeIdentRaw<CSSValueFill>(range).has_value();
    std::array<RefPtr<CSSPrimitiveValue>, 4> slices;

    for (auto& value : slices) {
        value = consumePercentage(range, context, ValueRange::NonNegative);
        if (!value)
            value = consumeNumber(range, context, ValueRange::NonNegative);
        if (!value)
            break;
    }
    if (!slices[0])
        return nullptr;
    if (consumeIdent<CSSValueFill>(range)) {
        if (fill)
            return nullptr;
        fill = true;
    }
    complete4Sides(slices);

    // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect do fill by default.
    if (property == CSSPropertyWebkitBorderImage || property == CSSPropertyWebkitMaskBoxImage || property == CSSPropertyWebkitBoxReflect)
        fill = true;

    return CSSBorderImageSliceValue::create({ slices[0].releaseNonNull(), slices[1].releaseNonNull(), slices[2].releaseNonNull(), slices[3].releaseNonNull() }, fill);
}

RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-image-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-outset

    std::array<RefPtr<CSSPrimitiveValue>, 4> outsets;

    for (auto& value : outsets) {
        value = consumeNumber(range, context, ValueRange::NonNegative);
        if (!value)
            value = consumeLength(range, context, HTMLStandardMode, ValueRange::NonNegative);
        if (!value)
            break;
    }
    if (!outsets[0])
        return nullptr;
    complete4Sides(outsets);

    return CSSQuadValue::create({ outsets[0].releaseNonNull(), outsets[1].releaseNonNull(), outsets[2].releaseNonNull(), outsets[3].releaseNonNull() });
}

RefPtr<CSSValue> consumeBorderImageWidth(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    // <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width

    std::array<RefPtr<CSSPrimitiveValue>, 4> widths;

    bool hasLength = false;
    for (auto& value : widths) {
        value = consumeNumber(range, context, ValueRange::NonNegative);
        if (value)
            continue;
        if (auto numericValue = consumeLengthPercentage(range, context, HTMLStandardMode, ValueRange::NonNegative)) {
            if (numericValue->isLength())
                hasLength = true;
            value = numericValue;
            continue;
        }
        value = consumeIdent<CSSValueAuto>(range);
        if (!value)
            break;
    }
    if (!widths[0])
        return nullptr;
    complete4Sides(widths);

    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = currentShorthand == CSSPropertyWebkitBorderImage && hasLength;

    return CSSBorderImageWidthValue::create({ widths[0].releaseNonNull(), widths[1].releaseNonNull(), widths[2].releaseNonNull(), widths[3].releaseNonNull() }, overridesBorderWidths);
}

bool consumeBorderImageComponents(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, RefPtr<CSSValue>& source,
    RefPtr<CSSValue>& slice, RefPtr<CSSValue>& width, RefPtr<CSSValue>& outset, RefPtr<CSSValue>& repeat)
{
    // <'border-image'> = <'border-image-source'>
    //                 || <'border-image-slice'> [ / <'border-image-width'> | / <'border-image-width'>? / <'border-image-outset'> ]?
    //                 || <'border-image-repeat'>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image

    do {
        if (!source) {
            source = consumeImageOrNone(range, context);
            if (source)
                continue;
        }
        if (!repeat) {
            repeat = consumeBorderImageRepeat(range, context);
            if (repeat)
                continue;
        }
        if (!slice) {
            slice = consumeBorderImageSlice(range, context, property);
            if (slice) {
                ASSERT(!width && !outset);
                if (consumeSlashIncludingWhitespace(range)) {
                    width = consumeBorderImageWidth(range, context, property);
                    if (consumeSlashIncludingWhitespace(range)) {
                        outset = consumeBorderImageOutset(range, context);
                        if (!outset)
                            return false;
                    } else if (!width)
                        return false;
                }
            } else
                return false;
        } else
            return false;
    } while (!range.atEnd());

    // If we're setting from the legacy shorthand, make sure to set the `mask-border-slice` fill to true.
    if (property == CSSPropertyWebkitMaskBoxImage && !slice)
        slice = CSSBorderImageSliceValue::create({ CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0) }, true);

    return true;
}

// MARK: - Border Style

RefPtr<CSSValue> consumeBorderWidth(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    // <'border-*-width'> = <line-width>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-top-width

    // <line-width> = <length [0,∞]> | thin | medium | thick

    CSSValueID id = range.peek().id();
    if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
        return consumeIdent(range);

    bool allowQuirkyLengths = (context.mode == HTMLQuirksMode) && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderWidth);
    UnitlessQuirk unitless = allowQuirkyLengths ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    return consumeLength(range, context, ValueRange::NonNegative, unitless);
}

RefPtr<CSSValue> consumeBorderColor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    // <'border-*-width'> = <line-width>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-top-width

    bool acceptQuirkyColors = (context.mode == HTMLQuirksMode) && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderColor);
    return consumeColor(range, context, { .acceptQuirkyColors = acceptQuirkyColors });
}

// MARK: - Background Clip

RefPtr<CSSValue> consumeSingleBackgroundClip(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-background-clip> = <visual-box>
    // https://drafts.csswg.org/css-backgrounds/#propdef-background-clip

    switch (auto keyword = range.peek().id(); keyword) {
    case CSSValueID::CSSValueBorderBox:
    case CSSValueID::CSSValuePaddingBox:
    case CSSValueID::CSSValueContentBox:
    case CSSValueID::CSSValueText:
    case CSSValueID::CSSValueWebkitText:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(keyword);
    case CSSValueID::CSSValueBorderArea:
        if (!context.cssBackgroundClipBorderAreaEnabled)
            return nullptr;
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(keyword);

    default:
        return nullptr;
    }
}

RefPtr<CSSValue> consumeBackgroundClip(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'background-clip'> = <visual-box>#
    // https://drafts.csswg.org/css-backgrounds/#propdef-background-clip

    auto lambda = [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        return consumeSingleBackgroundClip(range, context);
    };
    return consumeCommaSeparatedListWithSingleValueOptimization(range, lambda);
}

// MARK: - Background Size

template<CSSPropertyID property> static RefPtr<CSSValue> consumeBackgroundSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
    // https://drafts.csswg.org/css-backgrounds/#propdef-background-size

    if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
        return consumeIdent(range);

    bool shouldCoalesce = true;
    RefPtr<CSSPrimitiveValue> horizontal = consumeIdent<CSSValueAuto>(range);
    if (!horizontal) {
        horizontal = consumeLengthPercentage(range, context, ValueRange::NonNegative, UnitlessQuirk::Forbid);
        if (!horizontal)
            return nullptr;
        shouldCoalesce = false;
    }

    RefPtr<CSSPrimitiveValue> vertical;
    if (!range.atEnd()) {
        vertical = consumeIdent<CSSValueAuto>(range);
        if (!vertical)
            vertical = consumeLengthPercentage(range, context, ValueRange::NonNegative, UnitlessQuirk::Forbid);
    }
    if (!vertical) {
        if constexpr (property == CSSPropertyWebkitBackgroundSize) {
            // Legacy syntax: "-webkit-background-size: 10px" is equivalent to "background-size: 10px 10px".
            vertical = horizontal;
        } else if constexpr (property == CSSPropertyBackgroundSize) {
            vertical = CSSPrimitiveValue::create(CSSValueAuto);
        } else if constexpr (property == CSSPropertyMaskSize) {
            return horizontal;
        }
    }

    if (shouldCoalesce)
        return CSSValuePair::create(horizontal.releaseNonNull(), vertical.releaseNonNull());
    return CSSValuePair::createNoncoalescing(horizontal.releaseNonNull(), vertical.releaseNonNull());
}

RefPtr<CSSValue> consumeSingleBackgroundSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-background-size> = <bg-size>
    // https://drafts.csswg.org/css-backgrounds/#background-size

    return consumeBackgroundSize<CSSPropertyBackgroundSize>(range, context);
}

RefPtr<CSSValue> consumeSingleWebkitBackgroundSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // Non-standard.
    return consumeBackgroundSize<CSSPropertyWebkitBackgroundSize>(range, context);
}

RefPtr<CSSValue> consumeSingleMaskSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-mask-size> = <bg-size>
    // https://drafts.fxtf.org/css-masking/#the-mask-size

    return consumeBackgroundSize<CSSPropertyMaskSize>(range, context);
}

// MARK: - Background Repeat

RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <repeat-style> = repeat-x | repeat-y | [repeat | space | round | no-repeat]{1,2}
    // https://drafts.csswg.org/css-backgrounds/#typedef-repeat-style

    if (consumeIdentRaw<CSSValueRepeatX>(range))
        return CSSBackgroundRepeatValue::create(CSSValueRepeat, CSSValueNoRepeat);
    if (consumeIdentRaw<CSSValueRepeatY>(range))
        return CSSBackgroundRepeatValue::create(CSSValueNoRepeat, CSSValueRepeat);
    auto value1 = consumeIdentRaw<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value1)
        return nullptr;
    auto value2 = consumeIdentRaw<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value2)
        value2 = value1;
    return CSSBackgroundRepeatValue::create(*value1, *value2);
}

// MARK: - Shadows

static RefPtr<CSSValue> consumeSingleShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool isWebkitBoxShadow)
{
    // <shadow> = <color>? && [<length>{2} <length [0,∞]>? <length>?] && inset?
    // https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow

    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> horizontalOffset;
    RefPtr<CSSPrimitiveValue> verticalOffset;
    RefPtr<CSSPrimitiveValue> blurRadius;
    RefPtr<CSSPrimitiveValue> spreadDistance;

    for (size_t i = 0; i < 3; i++) {
        if (range.atEnd())
            break;

        const CSSParserToken& nextToken = range.peek();
        // If we have come to a comma (e.g. if this range represents a comma-separated list of <shadow>s), we are done parsing this <shadow>.
        if (nextToken.type() == CommaToken)
            break;

        if (nextToken.id() == CSSValueInset) {
            if (style)
                return nullptr;
            style = consumeIdent(range);
            continue;
        }

        auto maybeColor = consumeColor(range, context);
        if (maybeColor) {
            // If we just parsed a color but already had one, the given token range is not a valid <shadow>.
            if (color)
                return nullptr;
            color = maybeColor;
            continue;
        }

        // If the current token is neither a color nor the `inset` keyword, it must be the lengths component of this value.
        if (horizontalOffset || verticalOffset || blurRadius || spreadDistance) {
            // If we've already parsed these lengths, the given value is invalid as there cannot be two lengths components in a single <shadow> value.
            return nullptr;
        }
        horizontalOffset = consumeLength(range, context);
        if (!horizontalOffset)
            return nullptr;
        verticalOffset = consumeLength(range, context);
        if (!verticalOffset)
            return nullptr;

        const CSSParserToken& token = range.peek();
        // The explicit check for calc() is unfortunate. This is ensuring that we only fail parsing if there is a length, but it fails the range check.
        if (token.type() == DimensionToken || token.type() == NumberToken || (token.type() == FunctionToken && CSSCalc::isCalcFunction(token.functionId(), context))) {
            blurRadius = consumeLength(range, context, ValueRange::NonNegative);
            if (!blurRadius)
                return nullptr;
        }

        if (blurRadius)
            spreadDistance = consumeLength(range, context);
    }

    // In order for this to be a valid <shadow>, at least these lengths must be present.
    if (!horizontalOffset || !verticalOffset)
        return nullptr;
    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(blurRadius), WTFMove(spreadDistance), WTFMove(style), WTFMove(color), isWebkitBoxShadow);
}

RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'box-shadow'> = none | <shadow>#
    // https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, consumeSingleShadow, context, false);
}

RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, consumeSingleShadow, context, true);
}

// MARK: - Reflect (non-standard)

RefPtr<CSSValue> consumeReflect(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    auto direction = consumeIdentRaw<CSSValueAbove, CSSValueBelow, CSSValueLeft, CSSValueRight>(range);
    if (!direction)
        return nullptr;

    // FIXME: Does not seem right to create "0px" here. We'd like to omit "0px" when serializing if there is also no image.
    RefPtr<CSSPrimitiveValue> offset;
    if (range.atEnd())
        offset = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
    else {
        offset = consumeLengthPercentage(range, context);
        if (!offset)
            return nullptr;
    }

    RefPtr<CSSValue> mask;
    if (!range.atEnd()) {
        RefPtr<CSSValue> source;
        RefPtr<CSSValue> slice;
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> outset;
        RefPtr<CSSValue> repeat;
        if (!consumeBorderImageComponents(range, context, CSSPropertyWebkitBoxReflect, source, slice, width, outset, repeat))
            return nullptr;
        mask = createBorderImageValue(WTFMove(source), WTFMove(slice), WTFMove(width), WTFMove(outset), WTFMove(repeat));
    }
    return CSSReflectValue::create(*direction, offset.releaseNonNull(), WTFMove(mask));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
