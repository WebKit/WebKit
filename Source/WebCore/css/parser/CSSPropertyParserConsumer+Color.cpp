/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "CSSPropertyParserConsumer+Color.h"

#include "CSSAbsoluteColor.h"
#include "CSSAbsoluteColorResolver.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSColor.h"
#include "CSSColorConversion+Normalize.h"
#include "CSSColorDescriptors.h"
#include "CSSColorLayers.h"
#include "CSSColorMix.h"
#include "CSSContrastColor.h"
#include "CSSDynamicRangeLimit.h"
#include "CSSDynamicRangeLimitMix.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSHexColor.h"
#include "CSSKeywordColor.h"
#include "CSSLightDarkColor.h"
#include "CSSParser.h"
#include "CSSParserContext.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes+EvaluateCalc.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+ColorInterpolationMethod.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+KeywordDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+SymbolDefinitions.h"
#include "CSSPropertyParsing.h"
#include "CSSRelativeColor.h"
#include "CSSResolvedColor.h"
#include "CSSTokenizer.h"
#include "CSSValuePool.h"
#include "Color.h"
#include "ColorLuminance.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename T> static CSS::Color makeCSSColor(T&& unresolvedColorKind)
{
    return CSS::Color { std::forward<T>(unresolvedColorKind) };
}

template<typename T> static std::optional<CSS::Color> makeCSSColor(std::optional<T>&& unresolvedColorKind)
{
    return unresolvedColorKind ? std::make_optional(makeCSSColor(std::forward<T>(*unresolvedColorKind))) : std::nullopt;
}

// State passed to internal color consumer functions. Used to pass information
// down the stack and levels of color parsing nesting.
struct ColorParserState {
    ColorParserState(CSS::PropertyParserState& propertyParserState, const CSSColorParsingOptions& options)
        : propertyParserState { propertyParserState }
        , allowedColorTypes { options.allowedColorTypes }
        , acceptsQuirkyColor { propertyParserState.context.mode == HTMLQuirksMode && CSSProperty::acceptsQuirkyColor(propertyParserState.currentProperty) }
    {
    }

    CSS::PropertyParserState& propertyParserState;
    OptionSet<CSS::ColorType> allowedColorTypes;
    bool acceptsQuirkyColor { false };
    unsigned nestingLevel { 0 };
};

// RAII helper to increment/decrement nesting level.
struct ColorParserStateNester {
    ColorParserStateNester(ColorParserState& state)
        : state { state }
    {
        state.nestingLevel++;
    }

    ~ColorParserStateNester()
    {
        state.nestingLevel--;
    }

    ColorParserState& state;
};

// Overload of the exposed root functions that take a `ColorParserState&`. Used to implement nesting level tracking.
static std::optional<CSS::Color> consumeColor(CSSParserTokenRange&, ColorParserState&);

// MARK: - Generic component consumption

// Conveniences that invokes a Consumer operator for the component at `Index`.

template<typename... Ts> using MetaConsumerWrapper = MetaConsumer<Ts...>;

template<typename Descriptor, unsigned Index>
static std::optional<CSS::AbsoluteColorComponent<Descriptor, Index>> consumeAbsoluteComponent(CSSParserTokenRange& range, ColorParserState& state)
{
    using TypeList = GetCSSColorParseTypeWithCalcComponentTypeList<Descriptor, Index>;
    using Consumer = brigand::wrap<TypeList, MetaConsumerWrapper>;

    return Consumer::consume(range, state.propertyParserState);
}

template<typename Descriptor, unsigned Index>
static std::optional<CSS::RelativeColorComponent<Descriptor, Index>> consumeRelativeComponent(CSSParserTokenRange& range, ColorParserState& state, CSSCalcSymbolsAllowed symbolsAllowed)
{
    // Append `CSS::Symbol` to the TypeList to allow unadorned symbols from the symbol
    // table to be consumed.
    using TypeList = GetCSSColorParseTypeWithCalcAndSymbolsComponentTypeList<Descriptor, Index>;
    using Consumer = brigand::wrap<TypeList, MetaConsumerWrapper>;

    return Consumer::consume(range, state.propertyParserState, WTF::move(symbolsAllowed));
}

template<typename Descriptor>
static bool consumeAlphaDelimiter(CSSParserTokenRange& args)
{
    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy)
        return consumeCommaIncludingWhitespace(args);
    else
        return consumeSlashIncludingWhitespace(args);
}

template<typename Descriptor> static CSS::AbsoluteColor<typename Descriptor::Canonical> normalizeNonCalcComponents(const CSS::AbsoluteColor<Descriptor>& unresolved, ColorParserState& state)
{
    ASSERT(componentsContainsUnevaluatedCalc<Descriptor>(unresolved.components));

    // The canonical descriptor is normally the descriptor itself, except for legacy rgb and hsl, which use the modern counterparts.
    using CanonicalDescriptor = typename Descriptor::Canonical;

    if (state.nestingLevel > 1) {
        // If this is a nested color, we want to only normalize the numeric color channel components, leaving them unclamped. The alpha channel can be both normalized and clamped.
        //
        // This behavior is described in section on the processing of relative colors: https://drafts.csswg.org/css-color-5/#rcs-intro.
        return CSS::AbsoluteColor<CanonicalDescriptor> {
            CSSColorParseTypeWithCalc<CanonicalDescriptor> {
                normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, 0>(std::get<0>(unresolved.components)),
                normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, 1>(std::get<1>(unresolved.components)),
                normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, 2>(std::get<2>(unresolved.components)),
                normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 3>(std::get<3>(unresolved.components))
            }
        };
    }

    // For non-nested colors, we want to normalize and clamp the numeric color and alpha channel components.
    return CSS::AbsoluteColor<CanonicalDescriptor> {
        CSSColorParseTypeWithCalc<CanonicalDescriptor> {
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 0>(std::get<0>(unresolved.components)),
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 1>(std::get<1>(unresolved.components)),
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 2>(std::get<2>(unresolved.components)),
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 3>(std::get<3>(unresolved.components))
        }
    };
}

template<typename Descriptor> static CSS::AbsoluteColor<typename Descriptor::Canonical> normalizeNonCalcRequiringConversionDataComponents(const CSS::AbsoluteColor<Descriptor>& unresolved, ColorParserState& state)
{
    ASSERT(componentsContainsUnevaluatedCalc<Descriptor>(unresolved.components));

    // Evaluated any calc values that don't require conversion data.
    auto partiallyResolved = CSS::AbsoluteColor<Descriptor> {
        CSSColorParseTypeWithCalc<Descriptor> {
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<0>(unresolved.components), CSSCalcSymbolTable { }),
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<1>(unresolved.components), CSSCalcSymbolTable { }),
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<2>(unresolved.components), CSSCalcSymbolTable { }),
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<3>(unresolved.components), CSSCalcSymbolTable { })
        }
    };

    return normalizeNonCalcComponents(WTF::move(partiallyResolved), state);
}

// Overload of `consumeAbsoluteFunctionParameters` for callers that already have the initial component consumed.
template<typename Descriptor>
static std::optional<CSS::Color> consumeAbsoluteFunctionParameters(CSSParserTokenRange& args, ColorParserState& state, CSS::AbsoluteColorComponent<Descriptor, 0> c1)
{
    auto c2 = consumeAbsoluteComponent<Descriptor, 1>(args, state);
    if (!c2)
        return { };

    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy) {
        if (!consumeCommaIncludingWhitespace(args))
            return { };
    }

    auto c3 = consumeAbsoluteComponent<Descriptor, 2>(args, state);
    if (!c3)
        return { };

    std::optional<CSS::AbsoluteColorComponent<Descriptor, 3>> alpha;
    if (consumeAlphaDelimiter<Descriptor>(args)) {
        alpha = consumeAbsoluteComponent<Descriptor, 3>(args, state);
        if (!alpha)
            return { };
    }

    if (!args.atEnd())
        return { };

    auto unresolved = CSS::AbsoluteColor<Descriptor> {
        .components = { WTF::move(c1), WTF::move(*c2), WTF::move(*c3), WTF::move(alpha) },
    };

    if constexpr (Descriptor::allowEagerEvaluationOfResolvableCalc) {
        // From CSS Color 4:
        //    "For historical reasons, when calc() in sRGB colors resolves to a single
        //     value, the declared value serializes without the calc() wrapper."
        //       - https://drafts.csswg.org/css-color-4/#resolving-sRGB-values
        //
        // calc() will "resolve to a single value" when no conversion data is required.

        // For this legacy / eager evaluating case, we want to preserve any calc() components that require conversion data.
        if (componentsRequireConversionData<Descriptor>(unresolved.components))
            return makeCSSColor(normalizeNonCalcRequiringConversionDataComponents(unresolved, state));
    } else {
        // For the non-legacy / non-eager evaluating cases, we want preserve any calc(), not just calc() requiring conversion data, so the check is a bit more permissive.
        if (componentsContainsUnevaluatedCalc<Descriptor>(unresolved.components))
            return makeCSSColor(normalizeNonCalcComponents(unresolved, state));
    }

    ASSERT(!componentsRequireConversionData<Descriptor>(unresolved.components));

    // In all other cases, we can fully resolve the color all the way to an absolute Color value.

    auto resolver = CSS::AbsoluteColorResolver<Descriptor> {
        .components = unresolved.components,
        .nestingLevel = state.nestingLevel
    };

    auto resolved = CSS::ResolvedColor {
        resolveNoConversionDataRequired(WTF::move(resolver))
    };

    return makeCSSColor(WTF::move(resolved));
}

template<typename Descriptor>
static std::optional<CSS::Color> consumeAbsoluteFunctionParameters(CSSParserTokenRange& args, ColorParserState& state)
{
    auto c1 = consumeAbsoluteComponent<Descriptor, 0>(args, state);
    if (!c1)
        return { };

    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy) {
        if (!consumeCommaIncludingWhitespace(args))
            return { };
    }

    return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTF::move(*c1));
}

template<typename Descriptor>
static std::optional<CSS::Color> consumeRelativeFunctionParameters(CSSParserTokenRange& args, ColorParserState& state, CSS::Color&& originColor)
{
    const CSSCalcSymbolsAllowed symbolsAllowed {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER },
        { std::get<1>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER },
        { std::get<2>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER },
        { std::get<3>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER }
    };

    auto c1 = consumeRelativeComponent<Descriptor, 0>(args, state, symbolsAllowed);
    if (!c1)
        return { };
    auto c2 = consumeRelativeComponent<Descriptor, 1>(args, state, symbolsAllowed);
    if (!c2)
        return { };
    auto c3 = consumeRelativeComponent<Descriptor, 2>(args, state, symbolsAllowed);
    if (!c3)
        return { };

    std::optional<CSS::RelativeColorComponent<Descriptor, 3>> alpha;
    if (consumeSlashIncludingWhitespace(args)) {
        alpha = consumeRelativeComponent<Descriptor, 3>(args, state, symbolsAllowed);
        if (!alpha)
            return { };
    }

    if (!args.atEnd())
        return { };

    auto unresolved = CSS::RelativeColor<Descriptor> {
        .origin = WTF::move(originColor),
        .components = { WTF::move(*c1), WTF::move(*c2), WTF::move(*c3), WTF::move(alpha) }
    };

    return makeCSSColor(WTF::move(unresolved));
}

template<typename Descriptor>
static std::optional<CSS::Color> consumeRelativeFunctionParameters(CSSParserTokenRange& args, ColorParserState& state)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeColor(args, state);
    if (!originColor)
        return { };

    return consumeRelativeFunctionParameters<Descriptor>(args, state, WTF::move(*originColor));
}

// MARK: - Generic parameter parsing

// MARK: - lch() / lab() / oklch() / oklab() / hwb()

template<typename Descriptor>
static std::optional<CSS::Color> consumeGenericFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueLch || range.peek().functionId() == CSSValueOklch || range.peek().functionId() == CSSValueLab || range.peek().functionId() == CSSValueOklab || range.peek().functionId() == CSSValueHwb);

    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return consumeRelativeFunctionParameters<Descriptor>(args, state);
    return consumeAbsoluteFunctionParameters<Descriptor>(args, state);
}

// MARK: - rgb() / rgba()

static std::optional<CSS::Color> consumeRGBFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom) {
        using Descriptor = RGBFunctionModernRelative;

        return consumeRelativeFunctionParameters<Descriptor>(args, state);
    }

    // To determine whether this is going to use the modern or legacy syntax, we need to consume
    // the first component and the separated after it. If the separator is a `comma`, its using
    // the legacy syntax, if the separator is a space, it is using the modern syntax.
    //
    // We consume using the more accepting syntax, the modern one, and if it turns out that we
    // are actually parsing a legacy syntax function (by virtue of a `comma`), we explicitly
    // check the parsed parameter to see if it was the unsupported type, `none`, and reject
    // the whole function.

    using Descriptor = RGBFunctionModernAbsolute;

    auto red = consumeAbsoluteComponent<Descriptor, 0>(args, state);
    if (!red)
        return { };

    if (consumeCommaIncludingWhitespace(args)) {
        // A `comma` getting successfully consumed means this is using the legacy syntax.

        return WTF::switchOn(WTF::move(*red),
            [&]<typename T>(T red) -> std::optional<CSS::Color> {
                using Descriptor = RGBFunctionLegacy<T>;

                return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTF::move(red));
            },
            [](CSS::Keyword::None) -> std::optional<CSS::Color> {
                // `none` is invalid for the legacy syntax, but the initial parameter consumer didn't
                // know we were using the legacy syntax yet, so we need to check for it now.
                return { };
            }
        );
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTF::move(*red));
    }
}

// MARK: - hsl() / hsla()

static std::optional<CSS::Color> consumeHSLFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom) {
        using Descriptor = HSLFunctionModern;

        return consumeRelativeFunctionParameters<Descriptor>(args, state);
    }

    // To determine whether this is going to use the modern or legacy syntax, we need to consume
    // the first component and the separated after it. If the separator is a `comma`, its using
    // the legacy syntax, if the separator is a space, it is using the modern syntax.
    //
    // We consume using the more accepting syntax, the modern one, and if it turns out that we
    // are actually parsing a legacy syntax function (by virtue of a `comma`), we explicitly
    // check the parsed parameter to see if it was the unsupported type, `none`, and reject
    // the whole function.

    using Descriptor = HSLFunctionModern;

    auto hue = consumeAbsoluteComponent<Descriptor, 0>(args, state);
    if (!hue)
        return { };

    if (consumeCommaIncludingWhitespace(args)) {
        // A `comma` getting successfully consumed means this is using the legacy syntax.

        return WTF::switchOn(WTF::move(*hue),
            [&](auto hue) -> std::optional<CSS::Color> {
                using Descriptor = HSLFunctionLegacy;

                return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTF::move(hue));
            },
            [](CSS::Keyword::None) -> std::optional<CSS::Color> {
                // `none` is invalid for the legacy syntax, but the initial parameter consumer didn't
                // know we were using the legacy syntax yet, so we need to check for it now.
                return { };
            }
        );
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTF::move(*hue));
    }
}

// MARK: - color()

template<typename Functor>
static auto callWithColorFunction(CSSValueID id, Functor&& functor) -> decltype(functor.template operator()<ColorRGBFunction<ExtendedSRGBA<float>>>())
{
    switch (id) {
    case CSSValueA98Rgb:
        return functor.template operator()<ColorRGBFunction<ExtendedA98RGB<float>>>();
    case CSSValueDisplayP3:
        return functor.template operator()<ColorRGBFunction<ExtendedDisplayP3<float>>>();
    case CSSValueDisplayP3Linear:
        return functor.template operator()<ColorRGBFunction<ExtendedLinearDisplayP3<float>>>();
    case CSSValueProphotoRgb:
        return functor.template operator()<ColorRGBFunction<ExtendedProPhotoRGB<float>>>();
    case CSSValueRec2020:
        return functor.template operator()<ColorRGBFunction<ExtendedRec2020<float>>>();
    case CSSValueSRGB:
        return functor.template operator()<ColorRGBFunction<ExtendedSRGBA<float>>>();
    case CSSValueSrgbLinear:
        return functor.template operator()<ColorRGBFunction<ExtendedLinearSRGBA<float>>>();
    case CSSValueXyzD50:
        return functor.template operator()<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>();
    case CSSValueXyz:
    case CSSValueXyzD65:
        return functor.template operator()<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>();
    default:
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

template<typename Functor>
static auto consumeColorSpace(CSSParserTokenRange& args, Functor&& functor) -> decltype(functor.template operator()<ColorRGBFunction<ExtendedSRGBA<float>>>())
{
    return callWithColorFunction(args.peek().id(), [&]<typename Descriptor>() {
        consumeIdentRaw(args);

        return functor.template operator()<Descriptor>();
    });
}

static std::optional<CSS::Color> consumeColorFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueColor);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom) {
        consumeIdentRaw(args);

        auto originColor = consumeColor(args, state);
        if (!originColor)
            return { };

        return consumeColorSpace(args, [&]<typename Descriptor>() {
            return consumeRelativeFunctionParameters<Descriptor>(args, state, WTF::move(*originColor));
        });
    }

    return consumeColorSpace(args, [&]<typename Descriptor>() {
        return consumeAbsoluteFunctionParameters<Descriptor>(args, state);
    });
}

// MARK: - color-layers()

static std::optional<CSS::Color> consumeColorLayersFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // color-layers() = color-layers([ <blend-mode>, ]? <color># )

    ASSERT(range.peek().functionId() == CSSValueColorLayers);

    if (!state.propertyParserState.context.colorLayersEnabled)
        return std::nullopt;

    auto args = consumeFunction(range);

    // FIXME: Parse blend mode.

    Vector<CSS::Color> colors;
    do {
        auto color = consumeColor(args, state);
        if (!color)
            return std::nullopt;

        colors.append(WTF::move(*color));
    } while (consumeCommaIncludingWhitespace(args));

    if (!args.atEnd())
        return std::nullopt;

    return CSS::Color {
        CSS::ColorLayers {
            .blendMode = BlendMode::Normal,
            .colors = WTF::move(colors)
        }
    };
}

// MARK: - color-mix()

static std::optional<CSS::ColorMix::Component> consumeColorMixComponent(CSSParserTokenRange& args, ColorParserState& state)
{
    // [ <color> && <percentage [0,100]>? ]

    auto percentage = MetaConsumer<CSS::ColorMix::Component::Percentage>::consume(args, state.propertyParserState);

    auto originColor = consumeColor(args, state);
    if (!originColor)
        return std::nullopt;

    if (!percentage) {
        if (auto percent = MetaConsumer<CSS::ColorMix::Component::Percentage>::consume(args, state.propertyParserState))
            percentage = percent;
    }

    return CSS::ColorMix::Component {
        .color = WTF::move(*originColor),
        .percentage = WTF::move(percentage)
    };
}

static bool hasNonCalculatedZeroPercentage(const CSS::ColorMix::Component& mixComponent)
{
    if (auto percentage = mixComponent.percentage)
        return percentage->isKnownZero();
    return false;
}

static std::optional<CSS::Color> consumeColorMixFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // color-mix() = color-mix( <color-interpolation-method> , [ <color> && <percentage [0,100]>? ]#{2})
    // https://drafts.csswg.org/css-color-5/#color-mix

    ASSERT(range.peek().functionId() == CSSValueColorMix);

    auto args = consumeFunction(range);

    std::optional<ColorInterpolationMethod> colorInterpolationMethod = CSS::defaultInterpolationMethodForColorMix;
    if (args.peek().id() == CSSValueIn) {
        colorInterpolationMethod = consumeColorInterpolationMethod(args, state.propertyParserState);
        if (!colorInterpolationMethod)
            return std::nullopt;

        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
    }

    auto mixComponent1 = consumeColorMixComponent(args, state);
    if (!mixComponent1)
        return std::nullopt;

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    auto mixComponent2 = consumeColorMixComponent(args, state);
    if (!mixComponent2)
        return std::nullopt;

    if (!args.atEnd())
        return std::nullopt;

    if (hasNonCalculatedZeroPercentage(*mixComponent1) && hasNonCalculatedZeroPercentage(*mixComponent2)) {
        // This eagerly marks the parse as invalid if both percentage components are non-calc
        // and equal to 0. This satisfies step 5 of section 2.1. Percentage Normalization in
        // https://w3c.github.io/csswg-drafts/css-color-5/#color-mix which states:
        //    "If the percentages sum to zero, the function is invalid."
        //
        // The only way the percentages can sum to zero is both are zero, since we reject any
        // percentages less than zero, and missing percentages are treated as "100% - p(other)".
        //
        // FIXME: Should we also do this for calculated values? Or should we let the parse be
        // valid and fail to produce a valid color at style building time.
        return std::nullopt;
    }

    return CSS::Color {
        CSS::ColorMix {
            .colorInterpolationMethod = WTF::move(*colorInterpolationMethod),
            .mixComponents1 = WTF::move(*mixComponent1),
            .mixComponents2 = WTF::move(*mixComponent2)
        }
    };
}

// MARK: - contrast-color()

static std::optional<CSS::Color> consumeContrastColorFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // contrast-color() = contrast-color( <color> )
    // https://drafts.csswg.org/css-color-5/#funcdef-contrast-color

    ASSERT(range.peek().functionId() == CSSValueContrastColor);

    auto args = consumeFunction(range);

    auto color = consumeColor(args, state);
    if (!color)
        return std::nullopt;

    if (!args.atEnd())
        return std::nullopt;

    return CSS::Color {
        CSS::ContrastColor {
            .color = WTF::move(*color)
        }
    };
}

// MARK: - light-dark()

static std::optional<CSS::Color> consumeLightDarkFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // light-dark() = light-dark( <color>, <color> )
    // https://drafts.csswg.org/css-color-5/#light-dark

    ASSERT(range.peek().functionId() == CSSValueLightDark);

    auto args = consumeFunction(range);

    auto lightColor = consumeColor(args, state);
    if (!lightColor)
        return std::nullopt;

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    auto darkColor = consumeColor(args, state);
    if (!darkColor)
        return std::nullopt;

    if (!args.atEnd())
        return std::nullopt;

    return CSS::Color {
        CSS::LightDarkColor {
            .lightColor = WTF::move(*lightColor),
            .darkColor = WTF::move(*darkColor)
        }
    };
}

// MARK: - Color function dispatch

// NOTE: This is named "consume*A*ColorFunction" to differentiate if from the
// the function that consumes a `color()` explicitly.
static std::optional<CSS::Color> consumeAColorFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    std::optional<CSS::Color> color;
    switch (functionId) {
    case CSSValueRgb:
    case CSSValueRgba:
        color = consumeRGBFunction(colorRange, state);
        break;
    case CSSValueHsl:
    case CSSValueHsla:
        color = consumeHSLFunction(colorRange, state);
        break;
    case CSSValueHwb:
        color = consumeGenericFunction<HWBFunction>(colorRange, state);
        break;
    case CSSValueLab:
        color = consumeGenericFunction<LabFunction>(colorRange, state);
        break;
    case CSSValueLch:
        color = consumeGenericFunction<LCHFunction>(colorRange, state);
        break;
    case CSSValueOklab:
        color = consumeGenericFunction<OKLabFunction>(colorRange, state);
        break;
    case CSSValueOklch:
        color = consumeGenericFunction<OKLCHFunction>(colorRange, state);
        break;
    case CSSValueColor:
        color = consumeColorFunction(colorRange, state);
        break;
    case CSSValueColorLayers:
        color = consumeColorLayersFunction(colorRange, state);
        break;
    case CSSValueColorMix:
        color = consumeColorMixFunction(colorRange, state);
        break;
    case CSSValueContrastColor:
        color = consumeContrastColorFunction(colorRange, state);
        break;
    case CSSValueLightDark:
        color = consumeLightDarkFunction(colorRange, state);
        break;
    default:
        return { };
    }
    if (color)
        range = colorRange;
    return color;
}

// MARK: - Hex

static std::optional<SRGBA<uint8_t>> consumeHexColor(CSSParserTokenRange& range, ColorParserState& state)
{
    String string;
    StringView view;
    auto& token = range.peek();
    if (token.type() == HashToken)
        view = token.value();
    else {
        // <quirky-color> should only match for properties that explicitly opt-in and when not nested.
        // https://drafts.csswg.org/css-color-4/#quirky-color
        if (!state.acceptsQuirkyColor || state.nestingLevel > 1)
            return std::nullopt;
        if (token.type() == IdentToken) {
            view = token.value(); // e.g. FF0000
            if (view.length() != 3 && view.length() != 6)
                return std::nullopt;
        } else if (token.type() == NumberToken || token.type() == DimensionToken) {
            if (token.numericValueType() != IntegerValueType)
                return std::nullopt;
            auto numericValue = token.numericValue();
            if (!(numericValue >= 0 && numericValue < 1000000))
                return std::nullopt;
            auto integerValue = static_cast<int>(token.numericValue());
            if (token.type() == NumberToken)
                string = String::number(integerValue); // e.g. 112233
            else
                string = makeString(integerValue, token.unitString()); // e.g. 0001FF
            if (string.length() < 6)
                string = makeString("000000"_span.subspan(string.length()), string);

            if (string.length() != 3 && string.length() != 6)
                return std::nullopt;
            view = string;
        } else
            return std::nullopt;
    }
    auto result = CSSParserFastPaths::parseHexColor(view);
    if (!result)
        return std::nullopt;
    range.consumeIncludingWhitespace();
    return *result;
}

bool isColorKeywordAllowed(CSSValueID id, const CSSParserContext& context)
{
    switch (id) {
    case CSSValueWebkitFocusRingColor:
        switch (context.mode) {
        case UASheetMode:
        case HTMLQuirksMode:
            return true;
        default:
            return false;
        }

#if PLATFORM(COCOA)
    case CSSValueAppleSystemTertiaryFill:
#endif
#if PLATFORM(IOS_FAMILY)
    case CSSValueAppleSystemQuaternaryFill:
#endif
#if PLATFORM(MAC)
    case CSSValueAppleSystemOpaqueFill:
    case CSSValueAppleSystemOpaqueSecondaryFill:
#endif
    case CSSValueInternalDocumentTextColor:
        switch (context.mode) {
        case UASheetMode:
            return true;
        default:
            return false;
        }

    default:
        return true;
    }
}

// MARK: Color consumer bottleneck

std::optional<CSS::Color> consumeColor(CSSParserTokenRange& range, ColorParserState& state)
{
    ColorParserStateNester nester { state };

    auto keyword = range.peek().id();
    if (CSS::isColorKeyword(keyword, state.allowedColorTypes)) {
        if (!isColorKeywordAllowed(keyword, state.propertyParserState.context))
            return { };

        consumeIdentRaw(range);
        return CSS::Color { CSS::KeywordColor { keyword } };
    }

    if (auto hexColor = consumeHexColor(range, state))
        return CSS::Color { CSS::HexColor { *hexColor } };

    return consumeAColorFunction(range, state);
}

// MARK: - CSS::Color consuming entry points

std::optional<CSS::Color> consumeUnresolvedColor(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState, const CSSColorParsingOptions& options)
{
    ColorParserState state { propertyParserState, options };

    return consumeColor(range, state);
}

// MARK: - CSSValue consuming entry points

RefPtr<CSSValue> consumeColor(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState, const CSSColorParsingOptions& options)
{
    ColorParserState state { propertyParserState, options };

    if (auto color = consumeColor(range, state)) {
        // Special case top level color keywords, hex (SRGBA<uint8_t>) and absolute color values to maintain
        // backwards compatibility and existing optimizations.
        if (auto keyword = color->keyword())
            return CSSPrimitiveValue::create(keyword->valueID);
        if (auto hex = color->hex())
            return propertyParserState.pool.createColorValue(Color { hex->value });
        if (auto resolved = color->resolved())
            return propertyParserState.pool.createColorValue(WTF::move(resolved->value));

        return CSSColorValue::create(WTF::move(*color));
    }

    return nullptr;
}

// MARK: - Raw consuming entry points

Color consumeColorRaw(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState, const CSSColorParsingOptions& options, CSS::PlatformColorResolutionState& eagerResolutionState)
{
    ColorParserState state { propertyParserState, options };

    if (auto color = consumeColor(range, state))
        return createColor(*color, eagerResolutionState);
    return { };
}

// MARK: - Raw parsing entry points

Color parseColorRawGeneral(const String& string, const CSSParserContext& context, ScriptExecutionContext& scriptExecutionContext, const CSSColorParsingOptions& options, CSS::PlatformColorResolutionState& eagerResolutionState)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto state = CSS::PropertyParserState { .context = context, .pool = scriptExecutionContext.cssValuePool() };
    auto result = consumeUnresolvedColor(range, state, options);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd() || !result)
        return { };

    return createColor(*result, eagerResolutionState);
}

Color deprecatedParseColorRawWithoutContext(const String& string, const CSSColorParsingOptions& options)
{
    auto& context = strictCSSParserContext();
    if (auto color = CSSParserFastPaths::parseSimpleColor(string, context))
        return *color;

    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto state = CSS::PropertyParserState { .context = context };
    auto result = consumeUnresolvedColor(range, state, options);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd() || !result)
        return { };

    return result->absoluteColor();
}

// MARK: - <dynamic-range-limit-mix()> (unresolved)

static std::optional<CSS::DynamicRangeLimitMixComponent> consumeUnresolvedDynamicRangeLimitMixComponent(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState)
{
    // <dynamic-range-limit-mix-component> = <'dynamic-range-limit'> && <percentage [0,100]>

    auto rangeCopy = range;

    auto percentage = MetaConsumer<CSS::DynamicRangeLimitMixPercentage>::consume(rangeCopy, propertyParserState);
    auto limit = consumeUnresolvedDynamicRangeLimit(rangeCopy, propertyParserState);
    if (!limit)
        return std::nullopt;

    if (!percentage) {
        percentage = MetaConsumer<CSS::DynamicRangeLimitMixPercentage>::consume(rangeCopy, propertyParserState);
        if (!percentage)
            return { };
    }

    range = rangeCopy;

    return CSS::DynamicRangeLimitMixComponent {
        WTF::move(*limit),
        WTF::move(*percentage)
    };
}

static std::optional<CSS::DynamicRangeLimit> consumeUnresolvedDynamicRangeLimitMix(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState)
{
    // dynamic-range-limit-mix() = dynamic-range-limit-mix( [ <'dynamic-range-limit'> && <percentage [0,100]> ]#{2,} )

    ASSERT(range.peek().functionId() == CSSValueDynamicRangeLimitMix);

    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);

    CSS::DynamicRangeLimitMixParameters::Container resultBuilder;

    do {
        auto component = consumeUnresolvedDynamicRangeLimitMixComponent(args, propertyParserState);
        if (!component)
            return { };
        resultBuilder.append(WTF::move(*component));
    } while (consumeCommaIncludingWhitespace(args));

    if (!args.atEnd())
        return { };

    if (resultBuilder.size() < 2)
        return { };

    range = rangeCopy;
    return CSS::DynamicRangeLimit {
        CSS::DynamicRangeLimitMixFunction {
            CSS::DynamicRangeLimitMixFunctionValue { CSS::DynamicRangeLimitMixParameters { WTF::move(resultBuilder) } }
        }
    };
}

// MARK: - <'dynamic-range-limit'> (unresolved)

std::optional<CSS::DynamicRangeLimit> consumeUnresolvedDynamicRangeLimit(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState)
{
    // <'dynamic-range-limit'> = standard | high | constrained | <dynamic-range-limit-mix()>
    // https://drafts.csswg.org/css-color-hdr/#propdef-dynamic-range-limit

    switch (range.peek().id()) {
    case CSSValueStandard:
        range.consumeIncludingWhitespace();
        return CSS::DynamicRangeLimit { CSS::Keyword::Standard { } };
    case CSSValueConstrained:
        if (!propertyParserState.context.cssConstrainedDynamicRangeLimitEnabled)
            return { };
        range.consumeIncludingWhitespace();
        return CSS::DynamicRangeLimit { CSS::Keyword::Constrained { } };
    case CSSValueNoLimit:
        range.consumeIncludingWhitespace();
        return CSS::DynamicRangeLimit { CSS::Keyword::NoLimit { } };
    default:
        break;
    }

    if (!propertyParserState.context.cssDynamicRangeLimitMixEnabled)
        return { };

    if (range.peek().functionId() == CSSValueDynamicRangeLimitMix) {
        if (auto mix = consumeUnresolvedDynamicRangeLimitMix(range, propertyParserState))
            return CSS::DynamicRangeLimit { WTF::move(*mix) };
    }

    return { };
}

// MARK: -  <'dynamic-range-limit'> (CSSValue)

RefPtr<CSSValue> consumeDynamicRangeLimit(CSSParserTokenRange& range, CSS::PropertyParserState& propertyParserState)
{
    auto dynamicRangeLimit = consumeUnresolvedDynamicRangeLimit(range, propertyParserState);
    if (!dynamicRangeLimit)
        return { };
    return CSSDynamicRangeLimitValue::create(WTF::move(*dynamicRangeLimit));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
