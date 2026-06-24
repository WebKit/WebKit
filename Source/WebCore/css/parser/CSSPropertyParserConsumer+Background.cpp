/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSBorderImageOutsetValue.h"
#include "CSSBorderImageRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageSourceValue.h"
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
#include "CSSPropertyParserConsumer+Masking.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSWebkitBoxReflectValue.h"

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

std::optional<CSS::BorderImageSource> consumeUnresolvedBorderImageSource(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-image-source'> = none | <image>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-source

    if (auto keyword = consumeSpecificUnresolvedIdent<CSS::Keyword::None>(range))
        return CSS::BorderImageSource { WTF::move(*keyword) };

    if (RefPtr image = consumeImage(range, state))
        return CSS::BorderImageSource { image.releaseNonNull() };

    return std::nullopt;
}

RefPtr<CSSValue> consumeBorderImageSource(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-image-source'> = none | <image>
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-source

    if (auto unresolved = consumeUnresolvedBorderImageSource(range, state))
        return CSSBorderImageSourceValue::create(WTF::move(*unresolved));
    return nullptr;
}

std::optional<CSS::BorderImageOutset> consumeUnresolvedBorderImageOutset(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-image-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-outset

    std::array<std::optional<CSS::BorderImageOutset::Value>, 4> values;

    for (auto& value : values) {
        value = MetaConsumer<CSS::BorderImageOutset::Value::Number>::consume(range, state);
        if (!value)
            value = MetaConsumer<CSS::BorderImageOutset::Value::Length>::consume(range, state);
        if (!value)
            break;
    }
    if (!values[0])
        return std::nullopt;

    return CSS::BorderImageOutset {
        .values = completeQuadFromArray<CSS::BorderImageOutset::Edges>(WTF::move(values)),
    };
}

RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-image-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-outset

    if (auto unresolved = consumeUnresolvedBorderImageOutset(range, state))
        return CSSBorderImageOutsetValue::create(WTF::move(*unresolved));
    return nullptr;
}

std::optional<CSS::BorderImageRepeat> consumeUnresolvedBorderImageRepeat(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <'border-image-repeat'> = [ stretch | repeat | round | space ]{1,2}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-repeat

    std::array<std::optional<CSS::BorderImageRepeat::Value>, 2> values;

    values[0] = consumeSpecificUnresolvedIdent<CSS::BorderImageRepeat::Value>(range);
    if (!values[0])
        return std::nullopt;

    values[1] = consumeSpecificUnresolvedIdent<CSS::BorderImageRepeat::Value>(range);
    if (!values[1])
        values[1] = values[0];

    return CSS::BorderImageRepeat {
        .values = { WTF::move(*values[0]), WTF::move(*values[1]) }
    };
}

RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'border-image-repeat'> = [ stretch | repeat | round | space ]{1,2}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-repeat

    if (auto unresolved = consumeUnresolvedBorderImageRepeat(range, state))
        return CSSBorderImageRepeatValue::create(WTF::move(*unresolved));
    return nullptr;
}

std::optional<CSS::BorderImageSlice> consumeUnresolvedBorderImageSlice(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageSliceOverride overridesSlice)
{
    // <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice

    auto fill = consumeSpecificUnresolvedIdent<CSS::Keyword::Fill>(range);

    std::array<std::optional<CSS::BorderImageSlice::Value>, 4> values;

    for (auto& value : values) {
        value = MetaConsumer<CSS::BorderImageSlice::Value::Percentage>::consume(range, state);
        if (!value)
            value = MetaConsumer<CSS::BorderImageSlice::Value::Number>::consume(range, state);
        if (!value)
            break;
    }
    if (!values[0])
        return std::nullopt;

    if (!fill)
        fill = consumeSpecificUnresolvedIdent<CSS::Keyword::Fill>(range);

    // NOTE: For backwards compatibility, -webkit-border-image sets fill unconditionally.
    if (overridesSlice == BorderImageSliceOverride::AlwaysFill)
        fill = CSS::Keyword::Fill { };

    return CSS::BorderImageSlice {
        .values = completeQuadFromArray<CSS::BorderImageSlice::Edges>(WTF::move(values)),
        .fill = fill,
    };
}

RefPtr<CSSValue> consumeBorderImageSlice(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageSliceOverride overridesSlice)
{
    // <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice

    if (auto unresolved = consumeUnresolvedBorderImageSlice(range, state, overridesSlice))
        return CSSBorderImageSliceValue::create(WTF::move(*unresolved));
    return nullptr;
}

std::optional<CSS::BorderImageWidth> consumeUnresolvedBorderImageWidth(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageWidthOverridesWidthForLength overridesWidth)
{
    // <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width

    bool hasLength = false;
    std::array<std::optional<CSS::BorderImageWidth::Value>, 4> values;

    for (auto& value : values) {
        if (auto number = MetaConsumer<CSS::BorderImageWidth::Value::Number>::consume(range, state)) {
            value = CSS::BorderImageWidth::Value { WTF::move(*number) };
            continue;
        }

        // FIXME: Figure out and document why overrideParserMode is explicitly set to HTMLStandardMode here or remove the special case.
        // FIXME: As this falls into the "<length> ambiguous with <number>" case, this should probably be `.unitlessZeroLength = UnitlessZeroQuirk::Forbid` in case the order of checks ever changes.

        if (auto lengthPercentage = MetaConsumer<CSS::BorderImageWidth::Value::LengthPercentage>::consume(range, state, { .overrideParserMode = HTMLStandardMode })) {
            hasLength = WTF::switchOn(*lengthPercentage,
                [](const CSS::LengthPercentage<CSS::Nonnegative>::Calc& calc) {
                    return calc.primitiveType() == CSSUnitType::CSS_PX;
                },
                [](const CSS::LengthPercentage<CSS::Nonnegative>::Raw& raw) {
                    return raw.unit != CSS::PercentageUnit::Percentage;
                }
            );
            value = CSS::BorderImageWidth::Value { WTF::move(*lengthPercentage) };
            continue;
        }

        if (auto autoKeyword = consumeSpecificUnresolvedIdent<CSS::Keyword::Auto>(range)) {
            value = CSS::BorderImageWidth::Value { WTF::move(*autoKeyword) };
            continue;
        }

        break;
    }

    if (!values[0])
        return std::nullopt;

    return CSS::BorderImageWidth {
        .values = completeQuadFromArray<CSS::BorderImageWidth::Edges>(WTF::move(values)),
        .legacyWebkitBorderImage = (overridesWidth == BorderImageWidthOverridesWidthForLength::Yes && hasLength),
    };
}

RefPtr<CSSValue> consumeBorderImageWidth(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageWidthOverridesWidthForLength overridesWidth)
{
    // <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
    // https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width

    if (auto unresolved = consumeUnresolvedBorderImageWidth(range, state, overridesWidth))
        return CSSBorderImageWidthValue::create(WTF::move(*unresolved));
    return nullptr;
}

std::optional<CSS::BorderImage> consumeUnresolvedBorderImage(CSSParserTokenRange& range, CSS::PropertyParserState& state, BorderImageSliceOverride overridesSlice, BorderImageWidthOverridesWidthForLength overridesWidth)
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

    do {
        if (!source) {
            source = consumeUnresolvedBorderImageSource(range, state);
            if (source)
                continue;
        }
        if (!repeat) {
            repeat = consumeUnresolvedBorderImageRepeat(range, state);
            if (repeat)
                continue;
        }
        if (!slice) {
            slice = consumeUnresolvedBorderImageSlice(range, state, overridesSlice);
            if (slice) {
                ASSERT(!width && !outset);
                if (consumeSlashIncludingWhitespace(range)) {
                    width = consumeUnresolvedBorderImageWidth(range, state, overridesWidth);
                    if (consumeSlashIncludingWhitespace(range)) {
                        outset = consumeUnresolvedBorderImageOutset(range, state);
                        if (!outset)
                            return { };
                    } else if (!width)
                        return { };
                }
            } else
                return { };
        } else
            return { };
    } while (!range.atEnd());

    return CSS::BorderImage {
        .borderImageSource = WTF::move(source),
        .borderImageSlice = WTF::move(slice),
        .borderImageWidth = WTF::move(width),
        .borderImageOutset = WTF::move(outset),
        .borderImageRepeat = WTF::move(repeat),
    };
}

// MARK: - Background Size

template<CSSPropertyID property> static RefPtr<CSSValue> consumeBackgroundSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
    // https://drafts.csswg.org/css-backgrounds/#propdef-background-size

    if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
        return consumeIdent(range);

    bool shouldCoalesce = true;
    RefPtr<CSSValue> horizontal = consumeIdent<CSSValueAuto>(range);
    if (!horizontal) {
        horizontal = CSSPrimitiveValueResolver<CSS::LengthPercentage<CSS::Nonnegative>>::consumeAndResolve(range, state);
        if (!horizontal)
            return nullptr;
        shouldCoalesce = false;
    }

    RefPtr<CSSValue> vertical;
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
            vertical = CSSKeywordValue::create(CSSValueAuto);
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

static std::optional<CSS::WebkitBoxReflection> consumeUnresolvedWebkitBoxReflection(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <-webkit-box-reflection> = [ above | below | left | right ] <length-percentage>? <mask-border>?

    using namespace CSS::Literals;

    auto direction = consumeSpecificUnresolvedIdent<CSS::WebkitBoxReflection::Direction>(range);
    if (!direction)
        return std::nullopt;

    std::optional<CSS::WebkitBoxReflection::Offset> offset;
    if (!range.atEnd()) {
        offset = MetaConsumer<CSS::WebkitBoxReflection::Offset>::consume(range, state);
        if (!offset)
            return std::nullopt;
    }

    std::optional<CSS::WebkitBoxReflection::Mask> mask;
    if (!range.atEnd()) {
        mask = consumeUnresolvedMaskBorder(range, state, MaskBorderSliceOverride::AlwaysFill);
        if (!mask)
            return std::nullopt;
    }

    return CSS::WebkitBoxReflection {
        .direction = WTF::move(*direction),
        .offset = offset.value_or(CSS::WebkitBoxReflection::Offset { 0_css_px }),
        .mask = mask.value_or(CSS::WebkitBoxReflection::Mask { }),
    };
}

static std::optional<CSS::WebkitBoxReflect> consumeUnresolvedWebkitBoxReflect(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'-webkit-box-reflect'> = none | [ [ above | below | left | right ] <length-percentage>? <mask-border>? ]
    // NOTE: There is no standard associated with this property.

    if (auto keyword = consumeSpecificUnresolvedIdent<CSS::Keyword::None>(range))
        return CSS::WebkitBoxReflect { WTF::move(*keyword) };
    return consumeUnresolvedWebkitBoxReflection(range, state);
}

RefPtr<CSSValue> consumeWebkitBoxReflect(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'-webkit-box-reflect'> = none | [ [ above | below | left | right ] <length-percentage>? <mask-border>? ]
    // NOTE: There is no standard associated with this property.

    if (auto unresolved = consumeUnresolvedWebkitBoxReflect(range, state))
        return CSSWebkitBoxReflectValue::create(WTF::move(*unresolved));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
