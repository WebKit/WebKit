/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "CSSBoxShadowPropertyValue.h"
#include "CSSCalcTree+Parser.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSQuadValue.h"
#include "CSSReflectValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename ElementType> static void NODELETE complete4Sides(std::array<ElementType, 4>& sides)
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

template<SupportWebKitBorderRadiusQuirk supportQuirk> static std::optional<CSS::BorderRadius> consumeBorderRadius(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-radius

    using OptionalRadiiForAxis = std::array<std::optional<CSS::LengthPercentage<CSS::NonnegativeUnzoomed>>, 4>;

    OptionalRadiiForAxis horizontalRadii;
    unsigned i = 0;
    for (; i < 4 && !range.atEnd() && range.peek().type() != DelimiterToken; ++i) {
        horizontalRadii[i] = MetaConsumer<CSS::LengthPercentage<CSS::NonnegativeUnzoomed>>::consume(range, state);
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
                    .horizontal = completeQuadFromArray<CSS::BorderRadius::Axis>(WTF::move(horizontalRadii)),
                    .vertical = completeQuadFromArray<CSS::BorderRadius::Axis>(WTF::move(verticalRadii))
                };
            }
        }

        auto horizontal = completeQuadFromArray<CSS::BorderRadius::Axis>(WTF::move(horizontalRadii));
        auto vertical = horizontal; // Copy `horizontal` radii to `vertical`.

        return CSS::BorderRadius {
            .horizontal = WTF::move(horizontal),
            .vertical = WTF::move(vertical)
        };
    }

    if (!consumeSlashIncludingWhitespace(range))
        return { };

    OptionalRadiiForAxis verticalRadii;
    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        verticalRadii[i] = MetaConsumer<CSS::LengthPercentage<CSS::NonnegativeUnzoomed>>::consume(range, state);
        if (!verticalRadii[i])
            return { };
    }
    if (!verticalRadii[0] || !range.atEnd())
        return { };

    return CSS::BorderRadius {
        .horizontal = completeQuadFromArray<CSS::BorderRadius::Axis>(WTF::move(horizontalRadii)),
        .vertical = completeQuadFromArray<CSS::BorderRadius::Axis>(WTF::move(verticalRadii))
    };
}

std::optional<CSS::BorderRadius> consumeUnresolvedBorderRadius(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-radius

    return consumeBorderRadius<SupportWebKitBorderRadiusQuirk::No>(range, state);
}

std::optional<CSS::BorderRadius> consumeUnresolvedWebKitBorderRadius(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-radius'> = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-radius

    // Includes the legacy syntax quirk where `-webkit-border-radius: l1 l2` is equivalent to border-radius: `l1 / l2`.
    return consumeBorderRadius<SupportWebKitBorderRadiusQuirk::Yes>(range, state);
}

// MARK: - Border Image

RefPtr<CSSValue> consumeBorderImageSlice(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageSliceFillDefault defaultFill)
{
    // <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice

    bool fill = consumeIdentRaw<CSSValueFill>(range).has_value();
    std::array<RefPtr<CSSPrimitiveValue>, 4> slices;

    for (auto& value : slices) {
        value = CSSPrimitiveValueResolver<CSS::Percentage<CSS::Nonnegative>>::consumeAndResolve(range, state);
        if (!value)
            value = CSSPrimitiveValueResolver<CSS::Number<CSS::Nonnegative>>::consumeAndResolve(range, state);
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

    // NOTE: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect set fill unconditionally.
    if (defaultFill == BorderImageSliceFillDefault::Yes)
        fill = true;

    return CSSBorderImageSliceValue::create({ slices[0].releaseNonNull(), slices[1].releaseNonNull(), slices[2].releaseNonNull(), slices[3].releaseNonNull() }, fill);
}

RefPtr<CSSValue> consumeBorderImageWidth(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageWidthOverridesWidthForLength overridesWidth)
{
    // <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width

    std::array<RefPtr<CSSPrimitiveValue>, 4> widths;

    bool hasLength = false;
    for (auto& value : widths) {
        value = CSSPrimitiveValueResolver<CSS::Number<CSS::Nonnegative>>::consumeAndResolve(range, state);
        if (value)
            continue;

        // FIXME: Figure out and document why overrideParserMode is explicitly set to HTMLStandardMode here or remove the special case.
        // FIXME: As this falls into the "<length> ambiguous with <number>" case, this should probably be `.unitlessZeroLength = UnitlessZeroQuirk::Forbid` in case the order of checks ever changes.
        if (auto numericValue = CSSPrimitiveValueResolver<CSS::LengthPercentage<CSS::Nonnegative>>::consumeAndResolve(range, state, { .overrideParserMode = HTMLStandardMode })) {
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

    return CSSBorderImageWidthValue::create(
        {
            widths[0].releaseNonNull(),
            widths[1].releaseNonNull(),
            widths[2].releaseNonNull(),
            widths[3].releaseNonNull()
        },
        overridesWidth == BorderImageWidthOverridesWidthForLength::Yes && hasLength
    );
}

std::optional<CSS::BorderImageComponents> consumeBorderImageComponents(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageSliceFillDefault defaultFill, BorderImageWidthOverridesWidthForLength overridesWidth)
{
    // <'border-image'> = <'border-image-source'>
    //                 || <'border-image-slice'> [ / <'border-image-width'> | / <'border-image-width'>? / <'border-image-outset'> ]?
    //                 || <'border-image-repeat'>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image

    CSS::BorderImageComponents components;

    do {
        if (!components.source) {
            components.source = consumeImageOrNone(range, state);
            if (components.source)
                continue;
        }
        if (!components.repeat) {
            components.repeat = CSSPropertyParsing::consumeBorderImageRepeat(range);
            if (components.repeat)
                continue;
        }
        if (!components.slice) {
            components.slice = consumeBorderImageSlice(range, state, defaultFill);
            if (components.slice) {
                ASSERT(!components.width && !components.outset);
                if (consumeSlashIncludingWhitespace(range)) {
                    components.width = consumeBorderImageWidth(range, state, overridesWidth);
                    if (consumeSlashIncludingWhitespace(range)) {
                        components.outset = CSSPropertyParsing::consumeBorderImageOutset(range, state);
                        if (!components.outset)
                            return { };
                    } else if (!components.width)
                        return { };
                }
            } else
                return { };
        } else
            return { };
    } while (!range.atEnd());

    return components;
}

// MARK: - Background Size

template<CSSPropertyID property> static RefPtr<CSSValue> consumeBackgroundSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
    // https://drafts.csswg.org/css-backgrounds/#propdef-background-size

    if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
        return consumeIdent(range);

    bool shouldCoalesce = true;
    RefPtr<CSSPrimitiveValue> horizontal = consumeIdent<CSSValueAuto>(range);
    if (!horizontal) {
        horizontal = CSSPrimitiveValueResolver<CSS::LengthPercentage<CSS::Nonnegative>>::consumeAndResolve(range, state);
        if (!horizontal)
            return nullptr;
        shouldCoalesce = false;
    }

    RefPtr<CSSPrimitiveValue> vertical;
    if (!range.atEnd()) {
        vertical = consumeIdent<CSSValueAuto>(range);
        if (!vertical)
            vertical = CSSPrimitiveValueResolver<CSS::LengthPercentage<CSS::Nonnegative>>::consumeAndResolve(range, state);
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

RefPtr<CSSValue> consumeSingleBackgroundSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <single-background-size> = <bg-size>
    // https://drafts.csswg.org/css-backgrounds/#background-size

    return consumeBackgroundSize<CSSPropertyBackgroundSize>(range, state);
}

RefPtr<CSSValue> consumeSingleWebkitBackgroundSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // Non-standard.
    return consumeBackgroundSize<CSSPropertyWebkitBackgroundSize>(range, state);
}

RefPtr<CSSValue> consumeSingleMaskSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <single-mask-size> = <bg-size>
    // https://drafts.fxtf.org/css-masking/#the-mask-size

    return consumeBackgroundSize<CSSPropertyMaskSize>(range, state);
}

// MARK: - Background Repeat

RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange& range, CSS::PropertyParserState&)
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

// MARK: - Box Shadows

static std::optional<CSS::BoxShadow> consumeSingleUnresolvedBoxShadow(CSSParserTokenRange& range, CSS::PropertyParserState& state, bool isWebkitBoxShadow)
{
    // <box-shadow> = <color>? && [<length>{2} <length [0,∞]>? <length>?] && inset?
    // https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow

    auto rangeCopy = range;

    std::optional<CSS::Color> color;
    std::optional<CSS::Length<CSS::AllUnzoomed>> x;
    std::optional<CSS::Length<CSS::AllUnzoomed>> y;
    std::optional<CSS::Length<CSS::NonnegativeUnzoomed>> blur;
    std::optional<CSS::Length<CSS::AllUnzoomed>> spread;
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

        auto maybeColor = consumeUnresolvedColor(rangeCopy, state);
        if (maybeColor) {
            // If we just parsed a color but already had one, the given token range is
            // not a valid <shadow>.
            if (color)
                return { };
            color = WTF::move(*maybeColor);
            continue;
        }

        // If the current token is neither a color nor the `inset` keyword, it must be
        // the lengths component of this value.
        if (x || y || blur || spread) {
            // If we've already parsed these lengths, the given value is invalid as there
            // cannot be two lengths components in a single <shadow> value.
            return { };
        }

        x = MetaConsumer<CSS::Length<CSS::AllUnzoomed>>::consume(rangeCopy, state);
        if (!x)
            return { };
        y = MetaConsumer<CSS::Length<CSS::AllUnzoomed>>::consume(rangeCopy, state);
        if (!y)
            return { };

        const auto& token = rangeCopy.peek();

        // The explicit check for calc() is unfortunate. This is ensuring that we only fail
        // parsing if there is a length, but it fails the range check.
        if (token.type() == DimensionToken || token.type() == NumberToken || (token.type() == FunctionToken && CSSCalc::isCalcFunction(token.functionId()))) {
            blur = MetaConsumer<CSS::Length<CSS::NonnegativeUnzoomed>>::consume(rangeCopy, state);
            if (!blur)
                return { };
        }

        if (blur)
            spread = MetaConsumer<CSS::Length<CSS::AllUnzoomed>>::consume(rangeCopy, state);
    }

    if (!y)
        return { };

    range = rangeCopy;

    return CSS::BoxShadow {
        .color = WTF::move(color),
        .location = { WTF::move(*x), WTF::move(*y) },
        .blur = WTF::move(blur),
        .spread = WTF::move(spread),
        .inset = WTF::move(inset),
        .isWebkitBoxShadow = isWebkitBoxShadow
    };
}

static std::optional<CSS::BoxShadowProperty::List> consumeUnresolvedBoxShadowList(CSSParserTokenRange& range, CSS::PropertyParserState& state, bool isWebkitBoxShadow)
{
    auto rangeCopy = range;

    CSS::BoxShadowProperty::List list;

    do {
        auto shadow = consumeSingleUnresolvedBoxShadow(rangeCopy, state, isWebkitBoxShadow);
        if (!shadow)
            return { };
        list.value.append(WTF::move(*shadow));
    } while (consumeCommaIncludingWhitespace(rangeCopy));

    range = rangeCopy;

    return list;
}

static std::optional<CSS::BoxShadowProperty> consumeUnresolvedBoxShadow(CSSParserTokenRange& range, CSS::PropertyParserState& state, bool isWebkitBoxShadow)
{
    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::BoxShadowProperty { CSS::Keyword::None { } };
    }
    if (auto boxShadowList = consumeUnresolvedBoxShadowList(range, state, isWebkitBoxShadow))
        return CSS::BoxShadowProperty { WTF::move(*boxShadowList) };
    return { };
}

RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'box-shadow'> = none | <shadow>#
    // https://drafts.csswg.org/css-backgrounds/#propdef-box-shadow

    if (auto property = consumeUnresolvedBoxShadow(range, state, false))
        return CSSBoxShadowPropertyValue::create({ WTF::move(*property) });
    return nullptr;
}

RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto property = consumeUnresolvedBoxShadow(range, state, true))
        return CSSBoxShadowPropertyValue::create({ WTF::move(*property) });
    return nullptr;
}

// MARK: - Reflect (non-standard)

RefPtr<CSSValue> consumeWebkitBoxReflect(CSSParserTokenRange& range, CSS::PropertyParserState& state)
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
        offset = CSSPrimitiveValueResolver<CSS::LengthPercentage<>>::consumeAndResolve(range, state);
        if (!offset)
            return nullptr;
    }

    RefPtr<CSSValue> mask;
    if (!range.atEnd()) {
        auto components = consumeBorderImageComponents(range, state, BorderImageSliceFillDefault::Yes);
        if (!components)
            return nullptr;
        mask = createBorderImageValue(WTF::move(*components));
    }
    return CSSReflectValue::create(*direction, offset.releaseNonNull(), WTF::move(mask));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
