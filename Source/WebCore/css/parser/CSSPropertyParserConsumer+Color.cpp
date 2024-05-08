/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSCalcSymbolTable.h"
#include "CSSColorDescriptors.h"
#include "CSSParser.h"
#include "CSSParserContext.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NoneDefinitions.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+RawResolver.h"
#include "CSSPropertyParserConsumer+SymbolDefinitions.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSResolvedColorMix.h"
#include "CSSTokenizer.h"
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorMix.h"
#include "CSSValuePool.h"
#include "Color.h"
#include "ColorConversion.h"
#include "ColorInterpolation.h"
#include "ColorLuminance.h"
#include "ColorNormalization.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

using ColorOrUnresolvedColor = std::variant<Color, CSSUnresolvedColor>;

// State passed to internal color consumer functions. Used to pass information
// down the stack and levels of color parsing nesting.
struct ColorParserState {
    ColorParserState(const CSSParserContext& context, const CSSColorParsingOptions& options)
        : allowedColorTypes { options.allowedColorTypes }
        , clampHSLAtParseTime { options.clampHSLAtParseTime }
        , acceptQuirkyColors { options.acceptQuirkyColors }
        , colorContrastEnabled { context.colorContrastEnabled }
        , lightDarkEnabled { context.lightDarkEnabled }
        , mode { context.mode }
    {
    }

    OptionSet<StyleColor::CSSColorType> allowedColorTypes;

    bool clampHSLAtParseTime;
    bool acceptQuirkyColors;
    bool colorContrastEnabled;
    bool lightDarkEnabled;

    CSSParserMode mode;

    int nestingLevel = 0;
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

// Overloads of exposed root functions that take a `ColorParserState&`. Used to implement nesting level tracking.
static Color consumeColorRaw(CSSParserTokenRange&, ColorParserState&);
static RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange&, ColorParserState&);

// MARK: - Generic post normalization conversion

static bool outsideSRGBGamut(HSLA<float> hsla)
{
    auto unresolved = hsla.unresolved();
    return unresolved.saturation > 100.0 || unresolved.lightness < 0.0 || unresolved.lightness > 100.0;
}

static bool outsideSRGBGamut(HWBA<float> hwba)
{
    auto unresolved = hwba.unresolved();
    return unresolved.whiteness < 0.0 || unresolved.whiteness > 100.0 || unresolved.blackness < 0.0 || unresolved.blackness > 100.0;
}

static bool outsideSRGBGamut(SRGBA<float>)
{
    return false;
}

template<typename Descriptor>
static Color convertAbsoluteFunctionToColor(ColorParserState& state, std::optional<GetColorType<Descriptor>> color)
{
    if constexpr (Descriptor::allowConversionTo8BitSRGB) {
        if (!color)
            return { };

        if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Modern) {
            if (color->unresolved().anyComponentIsNone()) {
                // If any component uses "none", we store the value as is to allow for storage of the special value as NaN.
                return { *color, Descriptor::flagsForAbsolute };
            }
        }

        if (outsideSRGBGamut(*color)) {
            // If any component is outside the reference range, we store the value as is to allow for non-SRGB gamut values.
            return { *color, Descriptor::flagsForAbsolute };
        }

        if (state.nestingLevel > 1) {
            // If the color is being consumed as part of a composition (relative color, color-mix, light-dark, etc.), we
            // store the value as is to allow for maximum precision.
            return { *color, Descriptor::flagsForAbsolute };
        }

        // The explicit conversion to SRGBA<uint8_t> is an intentional performance optimization that allows storing the
        // color with no extra allocation for an extended color object. This is permissible in some case due to the
        // historical requirement that some syntaxes serialize using the legacy color syntax (rgb()/rgba()) and
        // historically have used the 8-bit rgba internal representation in engines.
        return { convertColor<SRGBA<uint8_t>>(*color), Descriptor::flagsForAbsolute };
    } else
        return { color, Descriptor::flagsForAbsolute };
}

template<typename Descriptor>
static Color convertRelativeFunctionToColor(ColorParserState&, std::optional<GetColorType<Descriptor>> color)
{
    return { color, Descriptor::flagsForRelative };
}

// MARK: - Generic component normalization

template<typename Descriptor, unsigned Index>
static GetColorTypeComponentType<Descriptor> normalizeComponent(NumberRaw number)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.type == ColorComponentType::Angle)
        return normalizeHue(number.value);
    else if constexpr (info.min == -std::numeric_limits<double>::infinity() && info.max == std::numeric_limits<double>::infinity())
        return number.value * info.numberMultiplier;
    else if constexpr (info.min == -std::numeric_limits<double>::infinity())
        return std::min(number.value * info.numberMultiplier, info.max);
    else if constexpr (info.max == std::numeric_limits<double>::infinity())
        return std::max(number.value * info.numberMultiplier, info.min);
    else
        return std::clamp(number.value * info.numberMultiplier, info.min, info.max);
}

template<typename Descriptor, unsigned Index>
static GetColorTypeComponentType<Descriptor> normalizeComponent(PercentRaw percent)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.min == -std::numeric_limits<double>::infinity() && info.max == std::numeric_limits<double>::infinity())
        return percent.value * info.percentMultiplier;
    else if constexpr (info.min == -std::numeric_limits<double>::infinity())
        return std::min(percent.value * info.percentMultiplier, info.max);
    else if constexpr (info.max == std::numeric_limits<double>::infinity())
        return std::max(percent.value * info.percentMultiplier, info.min);
    else
        return std::clamp(percent.value * info.percentMultiplier, info.min, info.max);
}

template<typename Descriptor, unsigned Index>
static GetColorTypeComponentType<Descriptor> normalizeComponent(AngleRaw angle)
{
    constexpr auto info = std::get<Index>(Descriptor::components);
    static_assert(info.type == ColorComponentType::Angle);

    return normalizeHue(CSSPrimitiveValue::computeDegrees(angle.type, angle.value));
}

template<typename Descriptor, unsigned Index>
static GetColorTypeComponentType<Descriptor> normalizeComponent(NoneRaw)
{
    return std::numeric_limits<double>::quiet_NaN();
}

template<typename T> struct IsVariantType : std::false_type { };
template<typename ...Args> struct IsVariantType<std::variant<Args...>> : std::true_type { };
template<typename T> inline constexpr bool IsVariant = IsVariantType<T>::value;

template<typename Descriptor, unsigned Index, typename T, typename std::enable_if_t<IsVariant<T>>* = nullptr>
static GetColorTypeComponentType<Descriptor> normalizeComponent(T variant)
{
    return WTF::switchOn(variant, [](auto value) { return normalizeComponent<Descriptor, Index>(value); });
}

template<typename T> struct IsOptionalType : std::false_type { };
template<typename Arg> struct IsOptionalType<std::optional<Arg>> : std::true_type { };
template<typename T> inline constexpr bool IsOptional = IsOptionalType<T>::value;

template<typename Descriptor, unsigned Index, typename T, typename std::enable_if_t<IsOptional<T>>* = nullptr>
static GetColorTypeComponentType<Descriptor> normalizeComponent(T optional, GetColorTypeComponentType<Descriptor> defaultValue)
{
    return optional ? normalizeComponent<Descriptor, Index>(*optional) : defaultValue;
}

template<typename Descriptor>
static GetColorType<Descriptor> normalizeAbsoluteComponents(CSSColorParseType<Descriptor> parsed, ColorParserState&)
{
    return {
        normalizeComponent<Descriptor, 0>(std::get<0>(parsed)),
        normalizeComponent<Descriptor, 1>(std::get<1>(parsed)),
        normalizeComponent<Descriptor, 2>(std::get<2>(parsed)),
        normalizeComponent<Descriptor, 3>(std::get<3>(parsed), 1.0)
    };
}

template<typename Descriptor>
static GetColorType<Descriptor> normalizeRelativeComponents(CSSColorParseType<Descriptor> parsed, ColorParserState&, const GetColorType<Descriptor>& originAsColorType)
{
    return {
        normalizeComponent<Descriptor, 0>(std::get<0>(parsed)),
        normalizeComponent<Descriptor, 1>(std::get<1>(parsed)),
        normalizeComponent<Descriptor, 2>(std::get<2>(parsed)),
        normalizeComponent<Descriptor, 3>(std::get<3>(parsed), originAsColorType.unresolved().alpha)
    };
}

// MARK: - Generic component consumption

// Conveniences that invokes a Consumer operator for the component at `Index`.

template<typename... Ts> using RawResolverWrapper = RawResolver<Ts...>;

template<typename Descriptor, unsigned Index>
static std::optional<GetComponentResult<Descriptor, Index>> consumeAbsoluteComponent(CSSParserTokenRange& range, ColorParserState& state)
{
    using TypeList = GetComponentResultTypeList<Descriptor, Index>;
    using Resolver = brigand::wrap<TypeList, RawResolverWrapper>;

    return Resolver::consumeAndResolve(range, { }, { .parserMode = state.mode });
}

template<typename Descriptor, unsigned Index>
static std::optional<GetComponentResult<Descriptor, Index>> consumeRelativeComponent(CSSParserTokenRange& range, ColorParserState& state, const CSSCalcSymbolTable& symbolTable)
{
    // Append `SymbolRaw` to the TypeList to allow unadorned symbols from the symbol
    // table to be consumed.
    using TypeList = brigand::append<GetComponentResultTypeList<Descriptor, Index>, brigand::list<SymbolRaw>>;
    using Resolver = brigand::wrap<TypeList, RawResolverWrapper>;

    return Resolver::consumeAndResolve(range, symbolTable, { .parserMode = state.mode });
}

template<typename Descriptor>
static bool consumeAlphaDelimiter(CSSParserTokenRange& args)
{
    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy)
        return consumeCommaIncludingWhitespace(args);
    else
        return consumeSlashIncludingWhitespace(args);
}

template<typename Descriptor>
static std::optional<CSSColorParseType<Descriptor>> consumeAbsoluteComponents(CSSParserTokenRange& args, ColorParserState& state)
{
    auto c1 = consumeAbsoluteComponent<Descriptor, 0>(args, state);
    if (!c1)
        return std::nullopt;

    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy) {
        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
    }

    auto c2 = consumeAbsoluteComponent<Descriptor, 1>(args, state);
    if (!c2)
        return std::nullopt;

    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy) {
        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
    }

    auto c3 = consumeAbsoluteComponent<Descriptor, 2>(args, state);
    if (!c3)
        return std::nullopt;

    std::optional<GetComponentResult<Descriptor, 3>> alpha;
    if (consumeAlphaDelimiter<Descriptor>(args)) {
        alpha = consumeAbsoluteComponent<Descriptor, 3>(args, state);
        if (!alpha)
            return std::nullopt;
    }

    if (!args.atEnd())
        return std::nullopt;

    return {{ WTFMove(*c1), WTFMove(*c2), WTFMove(*c3), WTFMove(alpha) }};
}

// Overload of `consumeAbsoluteComponents` for callers that already have the initial component consumed.
template<typename Descriptor>
static std::optional<CSSColorParseType<Descriptor>> consumeAbsoluteComponents(CSSParserTokenRange& args, ColorParserState& state, GetComponentResult<Descriptor, 0> c1)
{
    auto c2 = consumeAbsoluteComponent<Descriptor, 1>(args, state);
    if (!c2)
        return std::nullopt;

    if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Legacy) {
        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
    }

    auto c3 = consumeAbsoluteComponent<Descriptor, 2>(args, state);
    if (!c3)
        return std::nullopt;

    std::optional<GetComponentResult<Descriptor, 3>> alpha;
    if (consumeAlphaDelimiter<Descriptor>(args)) {
        alpha = consumeAbsoluteComponent<Descriptor, 3>(args, state);
        if (!alpha)
            return std::nullopt;
    }

    if (!args.atEnd())
        return std::nullopt;

    return {{ WTFMove(c1), WTFMove(*c2), WTFMove(*c3), WTFMove(alpha) }};
}

template<typename Descriptor>
static std::optional<CSSColorParseType<Descriptor>> consumeRelativeComponents(CSSParserTokenRange& args, ColorParserState& state, const GetColorType<Descriptor>& originAsColorType)
{
    auto originComponents = asColorComponents(originAsColorType.resolved());

    const CSSCalcSymbolTable symbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[0] * std::get<0>(Descriptor::components).symbolMultiplier },
        { std::get<1>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[1] * std::get<1>(Descriptor::components).symbolMultiplier },
        { std::get<2>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[2] * std::get<2>(Descriptor::components).symbolMultiplier },
        { std::get<3>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[3] * std::get<3>(Descriptor::components).symbolMultiplier }
    };

    auto c1 = consumeRelativeComponent<Descriptor, 0>(args, state, symbolTable);
    if (!c1)
        return std::nullopt;
    auto c2 = consumeRelativeComponent<Descriptor, 1>(args, state, symbolTable);
    if (!c2)
        return std::nullopt;
    auto c3 = consumeRelativeComponent<Descriptor, 2>(args, state, symbolTable);
    if (!c3)
        return std::nullopt;

    std::optional<GetComponentResult<Descriptor, 3>> alpha;
    if (consumeSlashIncludingWhitespace(args)) {
        alpha = consumeRelativeComponent<Descriptor, 3>(args, state, symbolTable);
        if (!alpha)
            return std::nullopt;
    }

    if (!args.atEnd())
        return std::nullopt;

    return {{ WTFMove(*c1), WTFMove(*c2), WTFMove(*c3), WTFMove(alpha) }};
}

// MARK: - Generic component combined consumption/normalization

template<typename Descriptor>
std::optional<GetColorType<Descriptor>> consumeAndNormalizeAbsoluteComponents(CSSParserTokenRange& args, ColorParserState& state)
{
    auto parsed = consumeAbsoluteComponents<Descriptor>(args, state);
    if (!parsed)
        return std::nullopt;
    return normalizeAbsoluteComponents<Descriptor>(*parsed, state);
}

// Overload of `consumeAndNormalizeAbsoluteComponents` for callers that already have the initial component consumed.
template<typename Descriptor>
std::optional<GetColorType<Descriptor>> consumeAndNormalizeAbsoluteComponents(CSSParserTokenRange& args, ColorParserState& state, GetComponentResult<Descriptor, 0> c1)
{
    auto parsed = consumeAbsoluteComponents<Descriptor>(args, state, c1);
    if (!parsed)
        return std::nullopt;
    return normalizeAbsoluteComponents<Descriptor>(*parsed, state);
}

template<typename Descriptor>
std::optional<GetColorType<Descriptor>> consumeAndNormalizeRelativeComponents(CSSParserTokenRange& args, ColorParserState& state, Color originColor)
{
    // Missing components are carried forward for this conversion as specified in
    // CSS Color 5 § 4.1 Processing Model for Relative Colors:
    //
    //  "Missing components are handled the same way as with CSS Color 4 § 12.2
    //   Interpolating with Missing Components: the origin colorspace and the
    //   relative function colorspace are checked for analogous components which
    //   are then carried forward as missing."
    //
    // (https://drafts.csswg.org/css-color-5/#rcs-intro)

    auto originColorAsColorType = originColor.toColorTypeLossyCarryingForwardMissing<GetColorType<Descriptor>>();

    auto parsed = consumeRelativeComponents<Descriptor>(args, state, originColorAsColorType);
    if (!parsed)
        return std::nullopt;
    return normalizeRelativeComponents<Descriptor>(*parsed, state, originColorAsColorType);
}

// MARK: - Generic parameter parsing

template<typename Descriptor>
static Color parseGenericAbsoluteFunctionParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    auto result = consumeAndNormalizeAbsoluteComponents<Descriptor>(args, state);
    return convertAbsoluteFunctionToColor<Descriptor>(state, result);
}

template<typename Descriptor>
static Color parseGenericAbsoluteFunctionParametersRaw(CSSParserTokenRange& args, ColorParserState& state, GetComponentResult<Descriptor, 0> c1)
{
    auto result = consumeAndNormalizeAbsoluteComponents<Descriptor>(args, state, c1);
    return convertAbsoluteFunctionToColor<Descriptor>(state, result);
}

template<typename Descriptor>
static Color parseGenericRelativeFunctionParametersRaw(CSSParserTokenRange& args, ColorParserState& state, Color originColor)
{
    auto result = consumeAndNormalizeRelativeComponents<Descriptor>(args, state, WTFMove(originColor));
    return convertRelativeFunctionToColor<Descriptor>(state, result);
}

template<typename Descriptor>
static Color parseGenericRelativeFunctionParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeColorRaw(args, state);
    if (!originColor.isValid())
        return { };

    return parseGenericRelativeFunctionParametersRaw<Descriptor>(args, state, WTFMove(originColor));
}

// MARK: - lch() / lab() / oklch() / oklab() / hwb()

template<typename Descriptor>
static Color parseGenericFunctionParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueLch || range.peek().functionId() == CSSValueOklch || range.peek().functionId() == CSSValueLab || range.peek().functionId() == CSSValueOklab || range.peek().functionId() == CSSValueHwb);

    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return parseGenericRelativeFunctionParametersRaw<Descriptor>(args, state);
    return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state);
}

// MARK: - rgb() / rgba()

static Color parseRGBFunctionParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom) {
        using Descriptor = RGBFunctionModernRelative;

        return parseGenericRelativeFunctionParametersRaw<Descriptor>(args, state);
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
        return WTF::switchOn(*red,
            [&] (NumberRaw red) -> Color {
                using Descriptor = RGBFunctionLegacy<NumberRaw>;

                return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state, red);
            },
            [&] (PercentRaw red) -> Color {
                using Descriptor = RGBFunctionLegacy<PercentRaw>;

                return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state, red);
            },
            [] (NoneRaw) -> Color {
                // `none` is invalid for the legacy syntax, but the initial parameter consumer didn't
                // know we were using the legacy syntax yet, so we need to check for it now.
                return { };
            }
        );
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state, *red);
    }
}

// MARK: - hsl() / hsla()

static Color parseHSLFunctionParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom) {
        using Descriptor = HSLFunctionModern;

        return parseGenericRelativeFunctionParametersRaw<Descriptor>(args, state);
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
        return WTF::switchOn(*hue,
            [&] (AngleRaw hue) -> Color {
                using Descriptor = HSLFunctionLegacy;

                return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state, AngleOrNumberRaw { hue });
            },
            [&] (NumberRaw hue) -> Color {
                using Descriptor = HSLFunctionLegacy;

                return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state, AngleOrNumberRaw { hue });
            },
            [] (NoneRaw) -> Color {
                // `none` is invalid for the legacy syntax, but the initial parameter consumer didn't
                // know we were using the legacy syntax yet, so we need to check for it now.
                return { };
            }
        );
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state, *hue);
    }
}

// MARK: - color()

template<typename Functor>
static Color callWithColorFunction(CSSValueID id, Functor&& functor)
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

static Color parseColorFunctionParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueColor);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom) {
        consumeIdentRaw(args);

        auto originColor = consumeColorRaw(args, state);
        if (!originColor.isValid())
            return { };

        return callWithColorFunction(args.peek().id(), [&]<typename Descriptor>() {
            consumeIdentRaw(args);

            return parseGenericRelativeFunctionParametersRaw<Descriptor>(args, state, WTFMove(originColor));
        });
    }

    return callWithColorFunction(args.peek().id(), [&]<typename Descriptor>() {
        consumeIdentRaw(args);

        return parseGenericAbsoluteFunctionParametersRaw<Descriptor>(args, state);
    });
}

// MARK: - color-contrast()

static Color selectFirstColorThatMeetsOrExceedsTargetContrast(const Color& originBackgroundColor, Vector<Color>&& colorsToCompareAgainst, double targetContrast)
{
    auto originBackgroundColorLuminance = originBackgroundColor.luminance();

    for (auto& color : colorsToCompareAgainst) {
        if (contrastRatio(originBackgroundColorLuminance, color.luminance()) >= targetContrast)
            return WTFMove(color);
    }

    // If there is a target contrast, and the end of the list is reached without meeting that target,
    // either white or black is returned, whichever has the higher contrast.
    auto contrastWithWhite = contrastRatio(originBackgroundColorLuminance, 1.0);
    auto contrastWithBlack = contrastRatio(originBackgroundColorLuminance, 0.0);
    return contrastWithWhite > contrastWithBlack ? Color::white : Color::black;
}

static Color selectFirstColorWithHighestContrast(const Color& originBackgroundColor, Vector<Color>&& colorsToCompareAgainst)
{
    auto originBackgroundColorLuminance = originBackgroundColor.luminance();

    auto* colorWithGreatestContrast = &colorsToCompareAgainst[0];
    double greatestContrastSoFar = 0;
    for (auto& color : colorsToCompareAgainst) {
        auto contrast = contrastRatio(originBackgroundColorLuminance, color.luminance());
        if (contrast > greatestContrastSoFar) {
            greatestContrastSoFar = contrast;
            colorWithGreatestContrast = &color;
        }
    }

    return WTFMove(*colorWithGreatestContrast);
}

static Color parseColorContrastFunctionParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueColorContrast);

    if (!state.colorContrastEnabled)
        return { };

    auto args = consumeFunction(range);

    auto originBackgroundColor = consumeColorRaw(args, state);
    if (!originBackgroundColor.isValid())
        return { };

    if (!consumeIdentRaw<CSSValueVs>(args))
        return { };

    Vector<Color> colorsToCompareAgainst;
    bool consumedTo = false;
    do {
        auto colorToCompareAgainst = consumeColorRaw(args, state);
        if (!colorToCompareAgainst.isValid())
            return { };

        colorsToCompareAgainst.append(WTFMove(colorToCompareAgainst));

        if (consumeIdentRaw<CSSValueTo>(args)) {
            consumedTo = true;
            break;
        }
    } while (consumeCommaIncludingWhitespace(args));

    // Handle the invalid case where there is only one color in the "compare against" color list.
    if (colorsToCompareAgainst.size() == 1)
        return { };

    if (consumedTo) {
        auto targetContrast = [&] () -> std::optional<NumberRaw> {
            if (args.peek().type() == IdentToken) {
                static constexpr std::pair<CSSValueID, NumberRaw> targetContrastMappings[] {
                    { CSSValueAA, NumberRaw { 4.5 } },
                    { CSSValueAALarge, NumberRaw { 3.0 } },
                    { CSSValueAAA, NumberRaw { 7.0 } },
                    { CSSValueAAALarge, NumberRaw { 4.5 } },
                };
                static constexpr SortedArrayMap targetContrastMap { targetContrastMappings };
                auto value = targetContrastMap.tryGet(args.consumeIncludingWhitespace().id());
                return value ? std::make_optional(*value) : std::nullopt;
            }
            return consumeNumberRaw(args);
        }();

        if (!targetContrast)
            return { };

        // Handle the invalid case where there are additional tokens after the target contrast.
        if (!args.atEnd())
            return { };

        // When a target constast is specified, we select "the first color color to meet or exceed the target contrast."
        return selectFirstColorThatMeetsOrExceedsTargetContrast(originBackgroundColor, WTFMove(colorsToCompareAgainst), targetContrast->value);
    }

    // Handle the invalid case where there are additional tokens after the "compare against" color list that are not the 'to' identifier.
    if (!args.atEnd())
        return { };

    // When a target constast is NOT specified, we select "the first color with the highest contrast to the single color."
    return selectFirstColorWithHighestContrast(originBackgroundColor, WTFMove(colorsToCompareAgainst));
}

// MARK: - color-mix()

static std::optional<HueInterpolationMethod> consumeHueInterpolationMethod(CSSParserTokenRange& range)
{
    static constexpr std::pair<CSSValueID, HueInterpolationMethod> hueInterpolationMethodMappings[] {
        { CSSValueShorter, HueInterpolationMethod::Shorter },
        { CSSValueLonger, HueInterpolationMethod::Longer },
        { CSSValueIncreasing, HueInterpolationMethod::Increasing },
        { CSSValueDecreasing, HueInterpolationMethod::Decreasing },
    };
    static constexpr SortedArrayMap hueInterpolationMethodMap { hueInterpolationMethodMappings };

    return consumeIdentUsingMapping(range, hueInterpolationMethodMap);
}

std::optional<ColorInterpolationMethod> consumeColorInterpolationMethod(CSSParserTokenRange& args)
{
    // <rectangular-color-space> = srgb | srgb-linear | display-p3 | a98-rgb | prophoto-rgb | rec2020 | lab | oklab | xyz | xyz-d50 | xyz-d65
    // <polar-color-space> = hsl | hwb | lch | oklch
    // <hue-interpolation-method> = [ shorter | longer | increasing | decreasing ] hue
    // <color-interpolation-method> = in [ <rectangular-color-space> | <polar-color-space> <hue-interpolation-method>? ]

    ASSERT(args.peek().id() == CSSValueIn);
    consumeIdentRaw(args);

    auto consumePolarColorSpace = [](CSSParserTokenRange& args, auto colorInterpolationMethod) -> std::optional<ColorInterpolationMethod> {
        // Consume the color space identifier.
        args.consumeIncludingWhitespace();

        // <hue-interpolation-method> is optional, so if it is not provided, we just use the default value
        // specified in the passed in 'colorInterpolationMethod' parameter.
        auto hueInterpolationMethod = consumeHueInterpolationMethod(args);
        if (!hueInterpolationMethod)
            return {{ colorInterpolationMethod, AlphaPremultiplication::Premultiplied }};

        // If the hue-interpolation-method was provided it must be followed immediately by the 'hue' identifier.
        if (!consumeIdentRaw<CSSValueHue>(args))
            return { };

        colorInterpolationMethod.hueInterpolationMethod = *hueInterpolationMethod;

        return {{ colorInterpolationMethod, AlphaPremultiplication::Premultiplied }};
    };

    auto consumeRectangularColorSpace = [](CSSParserTokenRange& args, auto colorInterpolationMethod) -> std::optional<ColorInterpolationMethod> {
        // Consume the color space identifier.
        args.consumeIncludingWhitespace();

        return {{ colorInterpolationMethod, AlphaPremultiplication::Premultiplied }};
    };

    switch (args.peek().id()) {
    case CSSValueHsl:
        return consumePolarColorSpace(args, ColorInterpolationMethod::HSL { });
    case CSSValueHwb:
        return consumePolarColorSpace(args, ColorInterpolationMethod::HWB { });
    case CSSValueLch:
        return consumePolarColorSpace(args, ColorInterpolationMethod::LCH { });
    case CSSValueLab:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::Lab { });
    case CSSValueOklch:
        return consumePolarColorSpace(args, ColorInterpolationMethod::OKLCH { });
    case CSSValueOklab:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::OKLab { });
    case CSSValueSRGB:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::SRGB { });
    case CSSValueSrgbLinear:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::SRGBLinear { });
    case CSSValueDisplayP3:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::DisplayP3 { });
    case CSSValueA98Rgb:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::A98RGB { });
    case CSSValueProphotoRgb:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::ProPhotoRGB { });
    case CSSValueRec2020:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::Rec2020 { });
    case CSSValueXyzD50:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::XYZD50 { });
    case CSSValueXyz:
    case CSSValueXyzD65:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::XYZD65 { });
    default:
        return { };
    }
}

static std::optional<CSSResolvedColorMix::Component> consumeColorMixComponentRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    CSSResolvedColorMix::Component result;

    if (auto percentage = consumePercentRaw(args)) {
        if (percentage->value < 0.0 || percentage->value > 100.0)
            return { };
        result.percentage = percentage->value;
    }

    result.color = consumeColorRaw(args, state);
    if (!result.color.isValid())
        return std::nullopt;

    if (!result.percentage) {
        if (auto percentage = consumePercentRaw(args)) {
            if (percentage->value < 0.0 || percentage->value > 100.0)
                return { };
            result.percentage = percentage->value;
        }
    }

    return result;
}

static std::optional<CSSUnresolvedColorMix::Component> consumeColorMixComponent(CSSParserTokenRange& args, ColorParserState& state)
{
    // [ <color> && <percentage [0,100]>? ]

    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> percentage;

    if (auto percent = consumePercent(args)) {
        if (!percent->isCalculated()) {
            auto value = percent->doubleValue();
            if (value < 0.0 || value > 100.0)
                return std::nullopt;
        }
        percentage = percent;
    }

    auto originColor = consumeColor(args, state);
    if (!originColor)
        return std::nullopt;

    if (!percentage) {
        if (auto percent = consumePercent(args)) {
            if (!percent->isCalculated()) {
                auto value = percent->doubleValue();
                if (value < 0.0 || value > 100.0)
                    return std::nullopt;
            }
            percentage = percent;
        }
    }

    return CSSUnresolvedColorMix::Component {
        originColor.releaseNonNull(),
        WTFMove(percentage)
    };
}

static Color parseColorMixFunctionParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    // color-mix() = color-mix( <color-interpolation-method> , [ <color> && <percentage [0,100]>? ]#{2})

    ASSERT(range.peek().functionId() == CSSValueColorMix);

    auto args = consumeFunction(range);

    if (args.peek().id() != CSSValueIn)
        return { };

    auto colorInterpolationMethod = consumeColorInterpolationMethod(args);
    if (!colorInterpolationMethod)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    auto mixComponent1 = consumeColorMixComponentRaw(args, state);
    if (!mixComponent1)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    auto mixComponent2 = consumeColorMixComponentRaw(args, state);
    if (!mixComponent2)
        return { };

    if (!args.atEnd())
        return { };

    return mix(CSSResolvedColorMix {
        WTFMove(*colorInterpolationMethod),
        WTFMove(*mixComponent1),
        WTFMove(*mixComponent2)
    });
}

static bool hasNonCalculatedZeroPercentage(const CSSUnresolvedColorMix::Component& mixComponent)
{
    return mixComponent.percentage && !mixComponent.percentage->isCalculated() && mixComponent.percentage->doubleValue() == 0.0;
}

static std::optional<ColorOrUnresolvedColor> parseColorMixFunctionParameters(CSSParserTokenRange& range, ColorParserState& state)
{
    // color-mix() = color-mix( <color-interpolation-method> , [ <color> && <percentage [0,100]>? ]#{2})

    ASSERT(range.peek().functionId() == CSSValueColorMix);

    auto args = consumeFunction(range);

    if (args.peek().id() != CSSValueIn)
        return std::nullopt;

    auto colorInterpolationMethod = consumeColorInterpolationMethod(args);
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

    return { CSSUnresolvedColor { CSSUnresolvedColorMix {
        WTFMove(*colorInterpolationMethod),
        WTFMove(*mixComponent1),
        WTFMove(*mixComponent2)
    } } };
}

// MARK: - light-dark()

static std::optional<ColorOrUnresolvedColor> parseLightDarkFunctionParameters(CSSParserTokenRange& range, ColorParserState& state)
{
    // light-dark() = light-dark( <color>, <color> )

    ASSERT(range.peek().functionId() == CSSValueLightDark);

    if (!state.lightDarkEnabled)
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

    return { CSSUnresolvedColor { CSSUnresolvedLightDark {
        lightColor.releaseNonNull(),
        darkColor.releaseNonNull()
    } } };
}

// MARK: - Color function dispatch

static Color parseColorFunctionRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    Color color;
    switch (functionId) {
    case CSSValueRgb:
    case CSSValueRgba:
        color = parseRGBFunctionParametersRaw(colorRange, state);
        break;
    case CSSValueHsl:
    case CSSValueHsla:
        color = parseHSLFunctionParametersRaw(colorRange, state);
        break;
    case CSSValueHwb:
        color = parseGenericFunctionParametersRaw<HWBFunction>(colorRange, state);
        break;
    case CSSValueLab:
        color = parseGenericFunctionParametersRaw<LabFunction>(colorRange, state);
        break;
    case CSSValueLch:
        color = parseGenericFunctionParametersRaw<LCHFunction>(colorRange, state);
        break;
    case CSSValueOklab:
        color = parseGenericFunctionParametersRaw<OKLabFunction>(colorRange, state);
        break;
    case CSSValueOklch:
        color = parseGenericFunctionParametersRaw<OKLCHFunction>(colorRange, state);
        break;
    case CSSValueColor:
        color = parseColorFunctionParametersRaw(colorRange, state);
        break;
    case CSSValueColorContrast:
        color = parseColorContrastFunctionParametersRaw(colorRange, state);
        break;
    case CSSValueColorMix:
        color = parseColorMixFunctionParametersRaw(colorRange, state);
        break;
    case CSSValueLightDark:
        // FIXME: Need a worker-safe way to compute light-dark colors.
        return { };
    default:
        return { };
    }
    if (color.isValid())
        range = colorRange;
    return color;
}

static std::optional<ColorOrUnresolvedColor> parseColorFunction(CSSParserTokenRange& range, ColorParserState& state)
{
    auto checkColor = [] (Color&& color) -> std::optional<ColorOrUnresolvedColor> {
        if (!color.isValid())
            return std::nullopt;
        return ColorOrUnresolvedColor { WTFMove(color) };
    };

    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    std::optional<ColorOrUnresolvedColor> color;
    switch (functionId) {
    case CSSValueRgb:
    case CSSValueRgba:
        color = checkColor(parseRGBFunctionParametersRaw(colorRange, state));
        break;
    case CSSValueHsl:
    case CSSValueHsla:
        color = checkColor(parseHSLFunctionParametersRaw(colorRange, state));
        break;
    case CSSValueHwb:
        color = checkColor(parseGenericFunctionParametersRaw<HWBFunction>(colorRange, state));
        break;
    case CSSValueLab:
        color = checkColor(parseGenericFunctionParametersRaw<LabFunction>(colorRange, state));
        break;
    case CSSValueLch:
        color = checkColor(parseGenericFunctionParametersRaw<LCHFunction>(colorRange, state));
        break;
    case CSSValueOklab:
        color = checkColor(parseGenericFunctionParametersRaw<OKLabFunction>(colorRange, state));
        break;
    case CSSValueOklch:
        color = checkColor(parseGenericFunctionParametersRaw<OKLCHFunction>(colorRange, state));
        break;
    case CSSValueColor:
        color = checkColor(parseColorFunctionParametersRaw(colorRange, state));
        break;
    case CSSValueColorContrast:
        color = checkColor(parseColorContrastFunctionParametersRaw(colorRange, state));
        break;
    case CSSValueColorMix:
        color = parseColorMixFunctionParameters(colorRange, state);
        break;
    case CSSValueLightDark:
        color = parseLightDarkFunctionParameters(colorRange, state);
        break;
    default:
        return { };
    }
    if (color)
        range = colorRange;
    return color;
}

// MARK: - Hex

static std::optional<SRGBA<uint8_t>> parseHexColor(CSSParserTokenRange& range, ColorParserState& state)
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
                string = makeString(&"000000"[string.length()], string);

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

// MARK: - CSSPrimitiveValue consuming entry points

RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange& range, ColorParserState& state)
{
    ColorParserStateNester nester { state };

    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword, state.allowedColorTypes)) {
        if (!isColorKeywordAllowedInMode(keyword, state.mode))
            return nullptr;
        return consumeIdent(range);
    }

    if (auto parsedColor = parseHexColor(range, state))
        return CSSValuePool::singleton().createColorValue(Color { *parsedColor });

    if (auto colorOrUnresolvedColor = parseColorFunction(range, state)) {
        return WTF::switchOn(WTFMove(*colorOrUnresolvedColor),
            [] (Color&& color) -> RefPtr<CSSPrimitiveValue> {
                return CSSValuePool::singleton().createColorValue(WTFMove(color));
            },
            [] (CSSUnresolvedColor&& color) -> RefPtr<CSSPrimitiveValue> {
                return CSSPrimitiveValue::create(WTFMove(color));
            }
        );
    }

    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange& range, const CSSParserContext& context, const CSSColorParsingOptions& options)
{
    ColorParserState state { context, options };
    return consumeColor(range, state);
}

RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange& range, const CSSParserContext& context, bool acceptQuirkyColors, OptionSet<StyleColor::CSSColorType> allowedColorTypes)
{
    return consumeColor(range, context, {
        .acceptQuirkyColors = acceptQuirkyColors,
        .allowedColorTypes = allowedColorTypes
    });
}

// MARK: - Raw consuming entry points

Color consumeColorRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ColorParserStateNester nester { state };

    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword, state.allowedColorTypes)) {
        if (!isColorKeywordAllowedInMode(keyword, state.mode))
            return { };

        // FIXME: Add support for passing pre-resolved system keywords.
        if (StyleColor::isSystemColorKeyword(keyword))
            return { };

        consumeIdentRaw(range);
        return StyleColor::colorFromKeyword(keyword, { });
    }

    if (auto parsedColor = parseHexColor(range, state))
        return Color { *parsedColor };

    return parseColorFunctionRaw(range, state);
}

Color consumeColorRaw(CSSParserTokenRange& range, const CSSParserContext& context, const CSSColorParsingOptions& options)
{
    ColorParserState state { context, options };
    return consumeColorRaw(range, state);
}

// MARK: - Raw parsing entry points

Color parseColorRawWorkerSafe(const String& string, const CSSParserContext& context, const CSSColorParsingOptions& options)
{
    bool strict = !isQuirksModeBehavior(context.mode);
    if (auto color = CSSParserFastPaths::parseSimpleColor(string, strict))
        return *color;

    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());
    range.consumeWhitespace();

    Color result;
    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword, options.allowedColorTypes)) {
        // FIXME: Need a worker-safe way to compute the system colors.
        //        For now, we detect the system color, but then intentionally fail parsing.
        if (StyleColor::isSystemColorKeyword(keyword))
            return { };
        if (!isColorKeywordAllowedInMode(keyword, context.mode))
            return { };
        result = StyleColor::colorFromKeyword(keyword, { });
        range.consumeIncludingWhitespace();
    }

    ColorParserState state { context, options };
    ColorParserStateNester nester { state };

    if (auto parsedColor = parseHexColor(range, state))
        result = *parsedColor;
    else
        result = parseColorFunctionRaw(range, state);

    if (!range.atEnd())
        return { };

    return result;
}

Color parseColorRaw(const String& string, const CSSParserContext& context, const CSSColorParsingOptions& options)
{
    bool strict = !isQuirksModeBehavior(context.mode);
    if (auto color = CSSParserFastPaths::parseSimpleColor(string, strict))
        return *color;

    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto result = consumeColorRaw(range, context, options);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return result;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
