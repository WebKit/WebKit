/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Color.h"

#include "CSSAbsoluteColorResolver.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSColorConversion+Normalize.h"
#include "CSSColorDescriptors.h"
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
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NoneDefinitions.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+SymbolDefinitions.h"
#include "CSSPropertyParsing.h"
#include "CSSTokenizer.h"
#include "CSSUnresolvedAbsoluteColor.h"
#include "CSSUnresolvedAbsoluteResolvedColor.h"
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorLayers.h"
#include "CSSUnresolvedColorMix.h"
#include "CSSUnresolvedColorResolutionState.h"
#include "CSSUnresolvedContrastColor.h"
#include "CSSUnresolvedLightDark.h"
#include "CSSUnresolvedRelativeColor.h"
#include "CSSValuePool.h"
#include "Color.h"
#include "ColorLuminance.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename T> static CSSUnresolvedColor makeCSSUnresolvedColor(T&& unresolvedColorKind)
{
    return CSSUnresolvedColor { std::forward<T>(unresolvedColorKind) };
}

template<typename T> static std::optional<CSSUnresolvedColor> makeCSSUnresolvedColor(std::optional<T>&& unresolvedColorKind)
{
    return unresolvedColorKind ? std::make_optional(makeCSSUnresolvedColor(std::forward<T>(*unresolvedColorKind))) : std::nullopt;
}

// State passed to internal color consumer functions. Used to pass information
// down the stack and levels of color parsing nesting.
struct ColorParserState {
    ColorParserState(const CSSParserContext& context, const CSSColorParsingOptions& options)
        : context { context }
        , allowedColorTypes { options.allowedColorTypes }
        , acceptQuirkyColors { options.acceptQuirkyColors }
    {
    }

    const CSSParserContext& context;
    OptionSet<StyleColor::CSSColorType> allowedColorTypes;
    bool acceptQuirkyColors { false };
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
static std::optional<CSSUnresolvedColor> consumeColor(CSSParserTokenRange&, ColorParserState&);

// MARK: - Generic component consumption

// Conveniences that invokes a Consumer operator for the component at `Index`.

template<typename... Ts> using MetaConsumerWrapper = MetaConsumer<Ts...>;

template<typename Descriptor, unsigned Index>
static std::optional<CSSUnresolvedAbsoluteColorComponent<Descriptor, Index>> consumeAbsoluteComponent(CSSParserTokenRange& range, ColorParserState& state)
{
    using TypeList = GetCSSColorParseTypeWithCalcComponentTypeList<Descriptor, Index>;
    using Consumer = brigand::wrap<TypeList, MetaConsumerWrapper>;

    return Consumer::consume(range, state.context, { }, { .parserMode = state.context.mode });
}

template<typename Descriptor, unsigned Index>
static std::optional<CSSUnresolvedRelativeColorComponent<Descriptor, Index>> consumeRelativeComponent(CSSParserTokenRange& range, ColorParserState& state, CSSCalcSymbolsAllowed symbolsAllowed)
{
    // Append `CSS::Symbol` to the TypeList to allow unadorned symbols from the symbol
    // table to be consumed.
    using TypeList = GetCSSColorParseTypeWithCalcAndSymbolsComponentTypeList<Descriptor, Index>;
    using Consumer = brigand::wrap<TypeList, MetaConsumerWrapper>;

    return Consumer::consume(range, state.context, WTFMove(symbolsAllowed), { .parserMode = state.context.mode });
}

template<typename Descriptor>
static bool consumeAlphaDelimiter(CSSParserTokenRange& args)
{
    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy)
        return consumeCommaIncludingWhitespace(args);
    else
        return consumeSlashIncludingWhitespace(args);
}

template<typename Descriptor> static CSSUnresolvedAbsoluteColor<typename Descriptor::Canonical> normalizeNonCalcComponents(const CSSUnresolvedAbsoluteColor<Descriptor>& unresolved, ColorParserState& state)
{
    ASSERT(containsUnevaluatedCalc<Descriptor>(unresolved.components));

    // The canonical descriptor is normally the descriptor itself, except for legacy rgb and hsl, which use the modern counterparts.
    using CanonicalDescriptor = typename Descriptor::Canonical;

    if (state.nestingLevel > 1) {
        // If this is a nested color, we want to only normalize the numeric color channel components, leaving them unclamped. The alpha channel can be both normalized and clamped.
        //
        // This behavior is described in section on the processing of relative colors: https://drafts.csswg.org/css-color-5/#rcs-intro.
        return CSSUnresolvedAbsoluteColor<CanonicalDescriptor> {
            CSSColorParseTypeWithCalc<CanonicalDescriptor> {
                normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, 0>(std::get<0>(unresolved.components)),
                normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, 1>(std::get<1>(unresolved.components)),
                normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, 2>(std::get<2>(unresolved.components)),
                normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 3>(std::get<3>(unresolved.components))
            }
        };
    }

    // For non-nested colors, we want to normalize and clamp the numeric color and alpha channel components.
    return CSSUnresolvedAbsoluteColor<CanonicalDescriptor> {
        CSSColorParseTypeWithCalc<CanonicalDescriptor> {
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 0>(std::get<0>(unresolved.components)),
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 1>(std::get<1>(unresolved.components)),
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 2>(std::get<2>(unresolved.components)),
            normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, 3>(std::get<3>(unresolved.components))
        }
    };
}

template<typename Descriptor> static CSSUnresolvedAbsoluteColor<typename Descriptor::Canonical> normalizeNonCalcRequiringConversionDataComponents(const CSSUnresolvedAbsoluteColor<Descriptor>& unresolved, ColorParserState& state)
{
    ASSERT(containsUnevaluatedCalc<Descriptor>(unresolved.components));

    // Evaluated any calc values that don't require conversion data.
    auto partiallyResolved = CSSUnresolvedAbsoluteColor<Descriptor> {
        CSSColorParseTypeWithCalc<Descriptor> {
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<0>(unresolved.components), CSSCalcSymbolTable { }),
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<1>(unresolved.components), CSSCalcSymbolTable { }),
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<2>(unresolved.components), CSSCalcSymbolTable { }),
            CSS::evaluateCalcIfNoConversionDataRequired(std::get<3>(unresolved.components), CSSCalcSymbolTable { })
        }
    };

    return normalizeNonCalcComponents(WTFMove(partiallyResolved), state);
}

// Overload of `consumeAbsoluteFunctionParameters` for callers that already have the initial component consumed.
template<typename Descriptor>
static std::optional<CSSUnresolvedColor> consumeAbsoluteFunctionParameters(CSSParserTokenRange& args, ColorParserState& state, CSSUnresolvedAbsoluteColorComponent<Descriptor, 0> c1)
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

    std::optional<CSSUnresolvedAbsoluteColorComponent<Descriptor, 3>> alpha;
    if (consumeAlphaDelimiter<Descriptor>(args)) {
        alpha = consumeAbsoluteComponent<Descriptor, 3>(args, state);
        if (!alpha)
            return { };
    }

    if (!args.atEnd())
        return { };

    auto unresolved = CSSUnresolvedAbsoluteColor<Descriptor> {
        .components = { WTFMove(c1), WTFMove(*c2), WTFMove(*c3), WTFMove(alpha) },
    };

    if constexpr (Descriptor::allowEagerEvaluationOfResolvableCalc) {
        // From CSS Color 4:
        //    "For historical reasons, when calc() in sRGB colors resolves to a single
        //     value, the declared value serializes without the calc() wrapper."
        //       - https://drafts.csswg.org/css-color-4/#resolving-sRGB-values
        //
        // calc() will "resolve to a single value" when no conversion data is required.

        // For this legacy / eager evaluating case, we want to preserve any calc() components that require conversion data.
        if (requiresConversionData<Descriptor>(unresolved.components))
            return makeCSSUnresolvedColor(normalizeNonCalcRequiringConversionDataComponents(unresolved, state));
    } else {
        // For the non-legacy / non-eager evaluating cases, we want preserve any calc(), not just calc() requiring conversion data, so the check is a bit more permissive.
        if (containsUnevaluatedCalc<Descriptor>(unresolved.components))
            return makeCSSUnresolvedColor(normalizeNonCalcComponents(unresolved, state));
    }

    ASSERT(!requiresConversionData<Descriptor>(unresolved.components));

    // In all other cases, we can fully resolve the color all the way to an absolute Color value.

    auto resolver = CSSAbsoluteColorResolver<Descriptor> {
        .components = unresolved.components,
        .nestingLevel = state.nestingLevel
    };

    auto unresolvedResolved = CSSUnresolvedAbsoluteResolvedColor {
        resolveNoConversionDataRequired(WTFMove(resolver))
    };

    return makeCSSUnresolvedColor(WTFMove(unresolvedResolved));
}

template<typename Descriptor>
static std::optional<CSSUnresolvedColor> consumeAbsoluteFunctionParameters(CSSParserTokenRange& args, ColorParserState& state)
{
    auto c1 = consumeAbsoluteComponent<Descriptor, 0>(args, state);
    if (!c1)
        return { };

    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy) {
        if (!consumeCommaIncludingWhitespace(args))
            return { };
    }

    return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTFMove(*c1));
}

template<typename Descriptor>
static std::optional<CSSUnresolvedColor> consumeRelativeFunctionParameters(CSSParserTokenRange& args, ColorParserState& state, CSSUnresolvedColor&& originColor)
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

    std::optional<CSSUnresolvedRelativeColorComponent<Descriptor, 3>> alpha;
    if (consumeSlashIncludingWhitespace(args)) {
        alpha = consumeRelativeComponent<Descriptor, 3>(args, state, symbolsAllowed);
        if (!alpha)
            return { };
    }

    if (!args.atEnd())
        return { };

    auto unresolved = CSSUnresolvedRelativeColor<Descriptor> {
        .origin = makeUniqueRef<CSSUnresolvedColor>(WTFMove(originColor)),
        .components = { WTFMove(*c1), WTFMove(*c2), WTFMove(*c3), WTFMove(alpha) }
    };

    return makeCSSUnresolvedColor(WTFMove(unresolved));
}

template<typename Descriptor>
static std::optional<CSSUnresolvedColor> consumeRelativeFunctionParameters(CSSParserTokenRange& args, ColorParserState& state)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeColor(args, state);
    if (!originColor)
        return { };

    return consumeRelativeFunctionParameters<Descriptor>(args, state, WTFMove(*originColor));
}

// MARK: - Generic parameter parsing

// MARK: - lch() / lab() / oklch() / oklab() / hwb()

template<typename Descriptor>
static std::optional<CSSUnresolvedColor> consumeGenericFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueLch || range.peek().functionId() == CSSValueOklch || range.peek().functionId() == CSSValueLab || range.peek().functionId() == CSSValueOklab || range.peek().functionId() == CSSValueHwb);

    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return consumeRelativeFunctionParameters<Descriptor>(args, state);
    return consumeAbsoluteFunctionParameters<Descriptor>(args, state);
}

// MARK: - rgb() / rgba()

static std::optional<CSSUnresolvedColor> consumeRGBFunction(CSSParserTokenRange& range, ColorParserState& state)
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

        return WTF::switchOn(WTFMove(*red),
            [&]<typename T>(T red) -> std::optional<CSSUnresolvedColor> {
                using Descriptor = RGBFunctionLegacy<T>;

                return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTFMove(red));
            },
            [](CSS::None) -> std::optional<CSSUnresolvedColor> {
                // `none` is invalid for the legacy syntax, but the initial parameter consumer didn't
                // know we were using the legacy syntax yet, so we need to check for it now.
                return { };
            }
        );
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTFMove(*red));
    }
}

// MARK: - hsl() / hsla()

static std::optional<CSSUnresolvedColor> consumeHSLFunction(CSSParserTokenRange& range, ColorParserState& state)
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

        return WTF::switchOn(WTFMove(*hue),
            [&](auto hue) -> std::optional<CSSUnresolvedColor> {
                using Descriptor = HSLFunctionLegacy;

                return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTFMove(hue));
            },
            [](CSS::None) -> std::optional<CSSUnresolvedColor> {
                // `none` is invalid for the legacy syntax, but the initial parameter consumer didn't
                // know we were using the legacy syntax yet, so we need to check for it now.
                return { };
            }
        );
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return consumeAbsoluteFunctionParameters<Descriptor>(args, state, WTFMove(*hue));
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

static std::optional<CSSUnresolvedColor> consumeColorFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueColor);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom) {
        consumeIdentRaw(args);

        auto originColor = consumeColor(args, state);
        if (!originColor)
            return { };

        return consumeColorSpace(args, [&]<typename Descriptor>() {
            return consumeRelativeFunctionParameters<Descriptor>(args, state, WTFMove(*originColor));
        });
    }

    return consumeColorSpace(args, [&]<typename Descriptor>() {
        return consumeAbsoluteFunctionParameters<Descriptor>(args, state);
    });
}

// MARK: - color-layers()

static std::optional<CSSUnresolvedColor> consumeColorLayersFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // color-layers() = color-layers([ <blend-mode>, ]? <color># )

    ASSERT(range.peek().functionId() == CSSValueColorLayers);

    if (!state.context.colorLayersEnabled)
        return std::nullopt;

    auto args = consumeFunction(range);

    // FIXME: Parse blend mode.

    Vector<UniqueRef<CSSUnresolvedColor>> colors;
    do {
        auto color = consumeColor(args, state);
        if (!color)
            return std::nullopt;

        colors.append(makeUniqueRef<CSSUnresolvedColor>(WTFMove(*color)));
    } while (consumeCommaIncludingWhitespace(args));

    if (!args.atEnd())
        return std::nullopt;

    return CSSUnresolvedColor {
        CSSUnresolvedColorLayers {
            .blendMode = BlendMode::Normal,
            .colors = WTFMove(colors)
        }
    };
}

// MARK: - color-mix()

static std::optional<CSSUnresolvedColorMix::Component> consumeColorMixComponent(CSSParserTokenRange& args, ColorParserState& state)
{
    // [ <color> && <percentage [0,100]>? ]

    auto percentage = MetaConsumer<CSSUnresolvedColorMix::Component::Percentage>::consume(args, state.context, { }, { });

    auto originColor = consumeColor(args, state);
    if (!originColor)
        return std::nullopt;

    if (!percentage) {
        if (auto percent = MetaConsumer<CSSUnresolvedColorMix::Component::Percentage>::consume(args, state.context, { }, { }))
            percentage = percent;
    }

    return CSSUnresolvedColorMix::Component {
        .color = makeUniqueRef<CSSUnresolvedColor>(WTFMove(*originColor)),
        .percentage = WTFMove(percentage)
    };
}

static bool hasNonCalculatedZeroPercentage(const CSSUnresolvedColorMix::Component& mixComponent)
{
    if (auto percentage = mixComponent.percentage)
        return percentage->isKnownZero();
    return false;
}

static std::optional<CSSUnresolvedColor> consumeColorMixFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // color-mix() = color-mix( <color-interpolation-method> , [ <color> && <percentage [0,100]>? ]#{2})
    // https://drafts.csswg.org/css-color-5/#color-mix

    ASSERT(range.peek().functionId() == CSSValueColorMix);

    auto args = consumeFunction(range);

    if (args.peek().id() != CSSValueIn)
        return std::nullopt;

    auto colorInterpolationMethod = consumeColorInterpolationMethod(args, state.context);
    if (!colorInterpolationMethod)
        return std::nullopt;

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

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

    return CSSUnresolvedColor {
        CSSUnresolvedColorMix {
            .colorInterpolationMethod = WTFMove(*colorInterpolationMethod),
            .mixComponents1 = WTFMove(*mixComponent1),
            .mixComponents2 = WTFMove(*mixComponent2)
        }
    };
}

// MARK: - contrast-color()

static std::optional<CSSUnresolvedColor> consumeContrastColorFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // contrast-color() = contrast-color( <color> max? )
    // https://drafts.csswg.org/css-color-5/#funcdef-contrast-color

    ASSERT(range.peek().functionId() == CSSValueContrastColor);

    if (!state.context.contrastColorEnabled)
        return std::nullopt;

    auto args = consumeFunction(range);

    auto color = consumeColor(args, state);
    if (!color)
        return std::nullopt;

    bool max = consumeIdentRaw<CSSValueMax>(args).has_value();

    if (!args.atEnd())
        return std::nullopt;

    return CSSUnresolvedColor {
        CSSUnresolvedContrastColor {
            .color = makeUniqueRef<CSSUnresolvedColor>(WTFMove(*color)),
            .max = max
        }
    };
}

// MARK: - light-dark()

static std::optional<CSSUnresolvedColor> consumeLightDarkFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    // light-dark() = light-dark( <color>, <color> )
    // https://drafts.csswg.org/css-color-5/#light-dark

    ASSERT(range.peek().functionId() == CSSValueLightDark);

    if (!state.context.lightDarkEnabled)
        return std::nullopt;

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

    return CSSUnresolvedColor {
        CSSUnresolvedLightDark {
            .lightColor = makeUniqueRef<CSSUnresolvedColor>(WTFMove(*lightColor)),
            .darkColor = makeUniqueRef<CSSUnresolvedColor>(WTFMove(*darkColor))
        }
    };
}

// MARK: - Color function dispatch

// NOTE: This is named "consume*A*ColorFunction" to differentiate if from the
// the function that consumes a `color()` explicitly.
static std::optional<CSSUnresolvedColor> consumeAColorFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    std::optional<CSSUnresolvedColor> color;
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
        if (!state.acceptQuirkyColors)
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
    auto result = CSSParser::parseHexColor(view);
    if (!result)
        return std::nullopt;
    range.consumeIncludingWhitespace();
    return *result;
}

// MARK: Color consumer bottleneck

std::optional<CSSUnresolvedColor> consumeColor(CSSParserTokenRange& range, ColorParserState& state)
{
    ColorParserStateNester nester { state };

    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword, state.allowedColorTypes)) {
        if (!isColorKeywordAllowedInMode(keyword, state.context.mode))
            return { };

        consumeIdentRaw(range);
        return CSSUnresolvedColor { CSSUnresolvedColorKeyword { keyword } };
    }

    if (auto hexColor = consumeHexColor(range, state))
        return CSSUnresolvedColor { CSSUnresolvedColorHex { *hexColor } };

    return consumeAColorFunction(range, state);
}

// MARK: - CSSUnresolvedColor consuming entry points

std::optional<CSSUnresolvedColor> consumeUnresolvedColor(CSSParserTokenRange& range, const CSSParserContext& context, const CSSColorParsingOptions& options)
{
    ColorParserState state { context, options };
    return consumeColor(range, state);
}

// MARK: - CSSPrimitiveValue consuming entry points

RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange& range, const CSSParserContext& context, const CSSColorParsingOptions& options)
{
    ColorParserState state { context, options };

    if (auto color = consumeColor(range, state)) {
        // Special case top level color keywords, hex (SRGBA<uint8_t>) and absolute color values to maintain
        // backwards compatibility and existing optimizations.
        if (auto keyword = color->keyword())
            return CSSPrimitiveValue::create(keyword->valueID);
        if (auto hex = color->hex())
            return CSSValuePool::singleton().createColorValue(Color { hex->value });
        if (auto absolute = color->absolute())
            return CSSValuePool::singleton().createColorValue(WTFMove(absolute->value));

        return CSSPrimitiveValue::create(WTFMove(*color));
    }

    return nullptr;
}

// MARK: - Raw consuming entry points

Color consumeColorRaw(CSSParserTokenRange& range, const CSSParserContext& context, const CSSColorParsingOptions& options, CSSUnresolvedColorResolutionState& eagerResolutionState)
{
    ColorParserState state { context, options };

    if (auto color = consumeColor(range, state))
        return color->createColor(eagerResolutionState);
    return { };
}

// MARK: - Raw parsing entry points

Color parseColorRawSlow(const String& string, const CSSParserContext& context, const CSSColorParsingOptions& options, CSSUnresolvedColorResolutionState& eagerResolutionState)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto result = consumeColorRaw(range, context, options, eagerResolutionState);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return result;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
