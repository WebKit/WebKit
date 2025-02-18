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
#include "CSSBorderImageOutsetValue.h"
#include "CSSBorderImageRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageSourceValue.h"
#include "CSSBorderImageValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSBorderRadius.h"
#include "CSSBoxShadowPropertyValue.h"
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
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSWebKitBoxReflectPropertyValue.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

using namespace CSS::Literals;

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

static std::optional<CSS::BorderImageRepeat> consumeUnresolvedBorderImageRepeat(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-image-repeat'> = [ stretch | repeat | round | space ]{1,2}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-repeat

    auto consumeAxis = [&] {
        return MetaConsumer<
            CSS::Keyword::Stretch,
            CSS::Keyword::Repeat,
            CSS::Keyword::Round,
            CSS::Keyword::Space
        >::consume(range, context, { }, { .parserMode = context.mode });
    };

    auto horizontal = consumeAxis();
    if (!horizontal)
        return { };
    auto vertical = consumeAxis();
    if (!vertical)
        vertical = horizontal;

    return CSS::BorderImageRepeat {
        .x = WTFMove(*horizontal),
        .y = WTFMove(*vertical),
    };
}

RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto repeat = consumeUnresolvedBorderImageRepeat(range, context))
        return CSSBorderImageRepeatValue::create(WTFMove(*repeat));
    return nullptr;
}

static std::optional<CSS::BorderImageSource> consumeUnresolvedBorderImageSource(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-image-source'> = none | <image>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-source

    if (consumeIdentRaw<CSSValueNone>(range).has_value())
        return CSS::BorderImageSource { CSS::Keyword::None { } };

    auto image = consumeImage(range, context);
    if (!image)
        return { };

    return CSS::BorderImageSource { CSS::Image { image.releaseNonNull() } };
}

RefPtr<CSSValue> consumeBorderImageSource(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto source = consumeUnresolvedBorderImageSource(range, context))
        return CSSBorderImageSourceValue::create(WTFMove(*source));
    return nullptr;
}

static std::optional<CSS::BorderImageSlice> consumeUnresolvedBorderImageSlice(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property)
{
    // <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice

    using Slices = CSS::BorderImageSlice::Slices;

    std::optional<CSS::Keyword::Fill> fill;
    std::optional<Slices> slices;

    auto consumeFill = [&] -> bool {
        if (fill)
            return false;
        if (consumeIdentRaw<CSSValueFill>(range).has_value())
            fill = CSS::Keyword::Fill { };
        return fill.has_value();
    };

    auto consumeSlices = [&] -> bool {
        if (slices)
            return false;

        slices = consumeQuad<Slices>([&] -> std::optional<CSS::BorderImageSlice::Slice> {
            return MetaConsumer<
                CSS::Number<CSS::Nonnegative, float>,
                CSS::Percentage<CSS::Nonnegative, float>
            >::consume(range, context, { }, { .parserMode = context.mode });
        });
        return slices.has_value();
    };

    for (unsigned i = 0; i < 2; ++i) {
        if (consumeFill() || consumeSlices())
            continue;
        break;
    }

    if (!slices)
        return { };

    // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect do fill by default.
    if (property == CSSPropertyWebkitBorderImage || property == CSSPropertyWebkitMaskBoxImage || property == CSSPropertyWebkitBoxReflect)
        fill = CSS::Keyword::Fill { };

    return CSS::BorderImageSlice {
        .slices = WTFMove(*slices),
        .fill = WTFMove(fill)
    };
}

RefPtr<CSSValue> consumeBorderImageSlice(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property)
{
    if (auto slice = consumeUnresolvedBorderImageSlice(range, context, property))
        return CSSBorderImageSliceValue::create(WTFMove(*slice));
    return nullptr;
}

static std::optional<CSS::BorderImageOutset> consumeUnresolvedBorderImageOutset(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'border-image-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-outset

    using Outsets = CSS::BorderImageOutset::Outsets;

    auto outsets = consumeQuad<Outsets>([&] {
        return MetaConsumer<
            CSS::Length<CSS::Nonnegative>,
            CSS::Number<CSS::Nonnegative>
        >::consume(range, context, { }, { .parserMode = context.mode });
    });
    if (!outsets)
        return { };

    return CSS::BorderImageOutset {
        .outsets = WTFMove(*outsets)
    };
}

RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto outset = consumeUnresolvedBorderImageOutset(range, context))
        return CSSBorderImageOutsetValue::create(WTFMove(*outset));
    return nullptr;
}

static std::optional<CSS::BorderImageWidth> consumeUnresolvedBorderImageWidth(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    // <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width

    using Widths = CSS::BorderImageWidth::Widths;

    auto widths = consumeQuad<Widths>([&] {
        return MetaConsumer<
            CSS::LengthPercentage<CSS::Nonnegative>,
            CSS::Number<CSS::Nonnegative>,
            CSS::Keyword::Auto
        >::consume(range, context, { }, { .parserMode = context.mode });
    });
    if (!widths)
        return { };

    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = currentShorthand == CSSPropertyWebkitBorderImage && CSS::hasLength(*widths);

    return CSS::BorderImageWidth {
        .widths = WTFMove(*widths),
        .overridesBorderWidths = overridesBorderWidths,
    };
}

RefPtr<CSSValue> consumeBorderImageWidth(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    if (auto outset = consumeUnresolvedBorderImageWidth(range, context, currentShorthand))
        return CSSBorderImageWidthValue::create(WTFMove(*outset));
    return nullptr;
}

std::optional<CSS::BorderImage> consumeUnresolvedBorderImage(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentProperty)
{
    // <'border-image'> = <'border-image-source'>
    //                 || <'border-image-slice'> [ / <'border-image-width'> | / <'border-image-width'>? / <'border-image-outset'> ]?
    //                 || <'border-image-repeat'>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image

    std::optional<CSS::BorderImageSource> source;
    std::optional<CSS::BorderImageSlice> slice;
    std::optional<CSS::BorderImageWidth> width;
    std::optional<CSS::BorderImageOutset> outset;
    std::optional<CSS::BorderImageRepeat> repeat;
    bool failed = false;

    auto consumeSource = [&] -> bool {
        if (source)
            return false;

        source = consumeUnresolvedBorderImageSource(range, context);
        return source.has_value();
    };

    auto consumeSlice = [&] -> bool {
        if (slice)
            return false;

        slice = consumeUnresolvedBorderImageSlice(range, context, currentProperty);
        if (!slice)
            return false;

        if (!consumeSlashIncludingWhitespace(range))
            return true;

        width = consumeUnresolvedBorderImageWidth(range, context, currentProperty);

        if (consumeSlashIncludingWhitespace(range)) {
            outset = consumeUnresolvedBorderImageOutset(range, context);
            if (!outset)
                failed = true;
        } else {
            if (!width)
                failed = true;
        }

        return true;
    };

    auto consumeRepeat = [&] -> bool {
        if (repeat)
            return false;
        repeat = consumeUnresolvedBorderImageRepeat(range, context);
        return repeat.has_value();
    };

    for (unsigned i = 0; i < 3; ++i) {
        if (consumeSource() || consumeSlice() || consumeRepeat())
            continue;
        break;
    }

    if (!source && !slice && !repeat)
        return { };
    if (failed)
        return { };

    if (currentProperty == CSSPropertyWebkitMaskBoxImage && !slice)
        slice = CSS::BorderImageSlice { CSS::BorderImageSlice::Slices { 0_css_number }, CSS::Keyword::Fill { } };

    return CSS::BorderImage {
        .source = WTFMove(source),
        .slice = WTFMove(slice),
        .width = WTFMove(width),
        .outset = WTFMove(outset),
        .repeat = WTFMove(repeat),
    };
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
    // <'border-*-color'> = <color>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-top-color

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

static std::optional<CSS::BackgroundRepeatStyle> consumeUnresolvedBackgroundRepeatStyle(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <repeat-style> = repeat-x | repeat-y | [repeat | space | round | no-repeat]{1,2}
    // https://drafts.csswg.org/css-backgrounds/#typedef-repeat-style

    auto consumeAxis = [&] {
        return MetaConsumer<
            CSS::Keyword::Repeat,
            CSS::Keyword::Space,
            CSS::Keyword::Round,
            CSS::Keyword::NoRepeat
        >::consume(range, context, { }, { .parserMode = context.mode });
    };

    if (consumeIdentRaw<CSSValueRepeatX>(range).has_value())
        return CSS::BackgroundRepeatStyle { CSS::Keyword::Repeat { }, CSS::Keyword::NoRepeat { } };

    if (consumeIdentRaw<CSSValueRepeatY>(range).has_value())
        return CSS::BackgroundRepeatStyle { CSS::Keyword::NoRepeat { }, CSS::Keyword::Repeat { } };

    auto value1 = consumeAxis();
    if (!value1)
        return { };

    auto value2 = consumeAxis();
    if (!value2)
        value2 = value1;

    return CSS::BackgroundRepeatStyle { WTFMove(*value1), WTFMove(*value2) };
}

RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <repeat-style> = repeat-x | repeat-y | [repeat | space | round | no-repeat]{1,2}
    // https://drafts.csswg.org/css-backgrounds/#typedef-repeat-style

    if (auto repeat = consumeUnresolvedBackgroundRepeatStyle(range, context))
        return CSSBackgroundRepeatValue::create(WTFMove(*repeat));
    return nullptr;
}

// MARK: - Box Shadows

static std::optional<CSS::BoxShadow> consumeSingleUnresolvedBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool isWebkitBoxShadow)
{
    // <box-shadow> = <color>? && [<length>{2} <length [0,∞]>? <length>?] && inset?
    // https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow

    const auto lengthOptions = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    auto rangeCopy = range;

    std::optional<CSS::Color> color;
    std::optional<CSS::Length<>> x;
    std::optional<CSS::Length<>> y;
    std::optional<CSS::Length<CSS::Nonnegative>> blur;
    std::optional<CSS::Length<>> spread;
    std::optional<CSS::Keyword::Inset> inset;

    for (size_t i = 0; i < 3; i++) {
        if (rangeCopy.atEnd())
            break;

        const CSSParserToken& nextToken = rangeCopy.peek();
        // If we have come to a comma (e.g. if this range represents a comma-separated list of <shadow>s), we are done parsing this <shadow>.
        if (nextToken.type() == CommaToken)
            break;

        if (nextToken.id() == CSSValueInset) {
            if (inset)
                return { };

            rangeCopy.consumeIncludingWhitespace();
            inset = CSS::Keyword::Inset { };
            continue;
        }

        auto maybeColor = consumeUnresolvedColor(rangeCopy, context);
        if (maybeColor) {
            // If we just parsed a color but already had one, the given token range is
            // not a valid <shadow>.
            if (color)
                return { };
            color = WTFMove(*maybeColor);
            continue;
        }

        // If the current token is neither a color nor the `inset` keyword, it must be
        // the lengths component of this value.
        if (x || y || blur || spread) {
            // If we've already parsed these lengths, the given value is invalid as there
            // cannot be two lengths components in a single <shadow> value.
            return { };
        }

        x = MetaConsumer<CSS::Length<>>::consume(rangeCopy, context, { }, lengthOptions);
        if (!x)
            return { };
        y = MetaConsumer<CSS::Length<>>::consume(rangeCopy, context, { }, lengthOptions);
        if (!y)
            return { };

        const auto& token = rangeCopy.peek();

        // The explicit check for calc() is unfortunate. This is ensuring that we only fail
        // parsing if there is a length, but it fails the range check.
        if (token.type() == DimensionToken || token.type() == NumberToken || (token.type() == FunctionToken && CSSCalc::isCalcFunction(token.functionId(), context))) {
            blur = MetaConsumer<CSS::Length<CSS::Nonnegative>>::consume(rangeCopy, context, { }, lengthOptions);
            if (!blur)
                return { };
        }

        if (blur)
            spread = MetaConsumer<CSS::Length<>>::consume(rangeCopy, context, { }, lengthOptions);
    }

    if (!y)
        return { };

    range = rangeCopy;

    return CSS::BoxShadow {
        .color = WTFMove(color),
        .location = { WTFMove(*x), WTFMove(*y) },
        .blur = WTFMove(blur),
        .spread = WTFMove(spread),
        .inset = WTFMove(inset),
        .isWebkitBoxShadow = isWebkitBoxShadow
    };
}

static std::optional<CSS::BoxShadowProperty::List> consumeUnresolvedBoxShadowList(CSSParserTokenRange& range, const CSSParserContext& context, bool isWebkitBoxShadow)
{
    auto rangeCopy = range;

    CSS::BoxShadowProperty::List list;

    do {
        auto shadow = consumeSingleUnresolvedBoxShadow(rangeCopy, context, isWebkitBoxShadow);
        if (!shadow)
            return { };
        list.value.append(WTFMove(*shadow));
    } while (consumeCommaIncludingWhitespace(rangeCopy));

    range = rangeCopy;

    return list;
}

static std::optional<CSS::BoxShadowProperty> consumeUnresolvedBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool isWebkitBoxShadow)
{
    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::BoxShadowProperty { CSS::Keyword::None { } };
    }
    if (auto boxShadowList = consumeUnresolvedBoxShadowList(range, context, isWebkitBoxShadow))
        return CSS::BoxShadowProperty { WTFMove(*boxShadowList) };
    return { };
}

RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'box-shadow'> = none | <shadow>#
    // https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow

    if (auto property = consumeUnresolvedBoxShadow(range, context, false))
        return CSSBoxShadowPropertyValue::create({ WTFMove(*property) });
    return nullptr;
}

RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto property = consumeUnresolvedBoxShadow(range, context, true))
        return CSSBoxShadowPropertyValue::create({ WTFMove(*property) });
    return nullptr;
}

// MARK: - Reflect (non-standard)

static std::optional<CSS::WebKitBoxReflectProperty> consumeUnresolvedWebKitBoxReflect(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::WebKitBoxReflectProperty { CSS::Keyword::None { } };
    }

    auto direction = MetaConsumer<
        CSS::Keyword::Above,
        CSS::Keyword::Below,
        CSS::Keyword::Left,
        CSS::Keyword::Right
    >::consume(range, context, { }, { .parserMode = context.mode });

    if (!direction)
        return { };

    // FIXME: Does not seem right to create "0px" here. We'd like to omit "0px" when serializing if there is also no image.
    std::optional<CSS::LengthPercentage<>> offset;
    if (range.atEnd())
        offset = 0_css_px;
    else {
        offset = MetaConsumer<
            CSS::LengthPercentage<>
        >::consume(range, context, { }, { .parserMode = context.mode, .unitlessZero = UnitlessZeroQuirk::Allow });
        if (!offset)
            return { };
    }

    std::optional<CSS::BorderImage> mask;
    if (!range.atEnd()) {
        mask = consumeUnresolvedBorderImage(range, context, CSSPropertyWebkitBoxReflect);
        if (!mask)
            return { };
    }

    return CSS::WebKitBoxReflectProperty {
        CSS::WebKitBoxReflect {
            .direction = WTFMove(*direction),
            .offset = WTFMove(*offset),
            .mask = WTFMove(mask),
        }
    };
}

RefPtr<CSSValue> consumeWebKitBoxReflect(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto reflection = consumeUnresolvedWebKitBoxReflect(range, context))
        return CSSWebKitBoxReflectPropertyValue::create(WTFMove(*reflection));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
