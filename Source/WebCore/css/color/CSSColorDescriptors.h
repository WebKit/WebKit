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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include "CSSValueKeywords.h"
#include "Color.h"
#include "ColorTypes.h"
#include <optional>
#include <variant>
#include <variant>
#include <wtf/Brigand.h>
#include <wtf/OptionSet.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

enum class CSSColorFunctionSyntax { Legacy, Modern };
enum class CSSColorFunctionForm { Relative, Absolute };

template<typename... Ts>
struct CSSColorComponent {
    using ResultTypeList = brigand::list<Ts...>;
    using Result = VariantOrSingle<ResultTypeList>;

    // Symbol used to represent component for relative color form.
    CSSValueID symbol;

    // The range parsed <number> and <percentage> values are clamped to
    // after a multiplier has been applied.
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();

    // The value parsed <percentage> values are multiplied by for normalization.
    double percentMultiplier = 1.0 / 100.0;

    // The value parsed <number> values are multiplied by for normalization.
    double numberMultiplier = 1.0;

    // Value the corresponding origin color component is multiplied by
    // for use as a symbol for relative color form.
    double symbolMultiplier = 1.0;

    // Component type for normalization. Angles get normalized using modular
    // arithmetic, numbers get normalized using clamping.
    ColorComponentType type = ColorComponentType::Number;
};

// MARK: Fully resolved parse type

template<typename Descriptor, unsigned Index>
using GetComponent = std::decay_t<decltype(std::get<Index>(Descriptor::components))>;

template<typename Descriptor, unsigned Index>
using GetComponentResult = typename GetComponent<Descriptor, Index>::Result;

template<typename Descriptor, unsigned Index>
using GetComponentResultTypeList = typename GetComponent<Descriptor, Index>::ResultTypeList;

template<typename Descriptor>
using GetColorType = typename Descriptor::ColorType;

template<typename Descriptor>
using GetColorTypeComponentType = typename GetColorType<Descriptor>::ComponentType;

template<typename Descriptor>
using CSSColorParseType = std::tuple<
    GetComponentResult<Descriptor, 0>,
    GetComponentResult<Descriptor, 1>,
    GetComponentResult<Descriptor, 2>,
    std::optional<GetComponentResult<Descriptor, 3>>
>;

// MARK: Parse type + Unevaluated Calc (absolute color parse result)

template<typename Descriptor, unsigned Index>
struct ResultTypeListWithCalcHelper {
    using ResultTypeList = typename TypesPlusUnevaluatedCalc<typename GetComponent<Descriptor, Index>::ResultTypeList>::ResultTypeList;
    using Result = VariantOrSingle<ResultTypeList>;
};

template<typename Descriptor, unsigned Index>
using GetComponentResultWithCalcResult = typename ResultTypeListWithCalcHelper<Descriptor, Index>::Result;

template<typename Descriptor>
using CSSColorParseTypeWithCalc = std::tuple<
    GetComponentResultWithCalcResult<Descriptor, 0>,
    GetComponentResultWithCalcResult<Descriptor, 1>,
    GetComponentResultWithCalcResult<Descriptor, 2>,
    std::optional<GetComponentResultWithCalcResult<Descriptor, 3>>
>;

// MARK: Parse type + Unevaluated Calc + Symbols (relative color parse result).

template<typename Descriptor, unsigned Index>
struct ResultTypeListWithCalcAndSymbolsHelper {
    using ResultTypeList = typename TypesPlusSymbolRaw<typename ResultTypeListWithCalcHelper<Descriptor, Index>::ResultTypeList>::ResultTypeList;
    using Result = VariantOrSingle<ResultTypeList>;
};

template<typename Descriptor, unsigned Index>
using GetComponentResultWithCalcAndSymbolsResult = typename ResultTypeListWithCalcAndSymbolsHelper<Descriptor, Index>::Result;

template<typename Descriptor>
using CSSColorParseTypeWithCalcAndSymbols = std::tuple<
    GetComponentResultWithCalcAndSymbolsResult<Descriptor, 0>,
    GetComponentResultWithCalcAndSymbolsResult<Descriptor, 1>,
    GetComponentResultWithCalcAndSymbolsResult<Descriptor, 2>,
    std::optional<GetComponentResultWithCalcAndSymbolsResult<Descriptor, 3>>
>;

// MARK: - Shared Component Descriptors

constexpr auto AlphaComponent = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw> { .symbol = CSSValueAlpha, .min = 0.0, .max = 1.0 };
constexpr auto AlphaLegacyComponent = CSSColorComponent<PercentRaw, NumberRaw> { .symbol = CSSValueAlpha, .min = 0.0, .max = 1.0 };

// MARK: - Color Function Descriptors

// <legacy-rgb-syntax>  =  rgb( <percentage>#{3} , <alpha-value>? ) |  rgb( <number>#{3} , <alpha-value>? )
// <legacy-rgba-syntax> = rgba( <percentage>#{3} , <alpha-value>? ) | rgba( <number>#{3} , <alpha-value>? )
template<typename Component>
struct RGBFunctionLegacy {
    using ColorType = SRGBA<float>;
    static constexpr bool allowConversionTo8BitSRGB = true;
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Legacy;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "rgb"_s;

    using R = CSSColorComponent<Component>;
    using G = CSSColorComponent<Component>;
    using B = CSSColorComponent<Component>;

    static constexpr auto components = std::make_tuple(
        R { .symbol = CSSValueR, .min = 0.0, .max = 1.0, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        G { .symbol = CSSValueG, .min = 0.0, .max = 1.0, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        B { .symbol = CSSValueB, .min = 0.0, .max = 1.0, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        AlphaLegacyComponent
    );
};

// <modern-rgb-syntax>  =  rgb( [<number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )
// <modern-rgba-syntax> = rgba( [<number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )
struct RGBFunctionModernAbsolute {
    using ColorType = SRGBA<float>;
    static constexpr bool allowConversionTo8BitSRGB = true;
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "rgb"_s;

    using R = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using G = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using B = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        R { .symbol = CSSValueR, .min = 0.0, .max = 1.0, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        G { .symbol = CSSValueG, .min = 0.0, .max = 1.0, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        B { .symbol = CSSValueB, .min = 0.0, .max = 1.0, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        AlphaComponent
    );
};

// <modern-rgb-syntax>  =  rgb( [from <color>] [<number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )
// <modern-rgba-syntax> = rgba( [from <color>] [<number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )
struct RGBFunctionModernRelative {
    using ColorType = ExtendedSRGBA<float>;
    static constexpr bool allowConversionTo8BitSRGB = true;
    static constexpr OptionSet<Color::Flags> flagsForRelative = Color::Flags::UseColorFunctionSerialization;
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "rgb"_s;

    using R = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using G = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using B = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        R { .symbol = CSSValueR, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        G { .symbol = CSSValueG, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        B { .symbol = CSSValueB, .numberMultiplier = 1.0 / 255.0, .symbolMultiplier = 255.0 },
        AlphaComponent
    );
};

// <legacy-hsl-syntax>  =  hsl( <hue>, <percentage>, <percentage>, <alpha-value>? )
// <legacy-hsla-syntax> = hsla( <hue>, <percentage>, <percentage>, <alpha-value>? )
struct HSLFunctionLegacy {
    using ColorType = HSLA<float>;
    static constexpr bool allowConversionTo8BitSRGB = true;
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Legacy;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "hsl"_s;

    using H = CSSColorComponent<AngleRaw, NumberRaw>;
    using S = CSSColorComponent<PercentRaw>;
    using L = CSSColorComponent<PercentRaw>;

    static constexpr auto components = std::make_tuple(
        H { .symbol = CSSValueH, .type = ColorComponentType::Angle    },
        S { .symbol = CSSValueS, .min = 0.0, .percentMultiplier = 1.0 },
        L { .symbol = CSSValueL,             .percentMultiplier = 1.0 },
        AlphaLegacyComponent
    );
};

// <modern-hsl-syntax>  =  hsl( [from <color>]? [<hue> | none] [<percentage> | <number> | none]{2} [ / [<alpha-value> | none] ]? )
// <modern-hsla-syntax> = hsla( [from <color>]? [<hue> | none] [<percentage> | <number> | none]{2} [ / [<alpha-value> | none] ]? )
struct HSLFunctionModern {
    using ColorType = HSLA<float>;
    static constexpr bool allowConversionTo8BitSRGB = true;
    static constexpr OptionSet<Color::Flags> flagsForRelative = Color::Flags::UseColorFunctionSerialization;
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "hsl"_s;

    using H = CSSColorComponent<AngleRaw, NumberRaw, NoneRaw>;
    using S = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using L = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        H { .symbol = CSSValueH, .type = ColorComponentType::Angle    },
        S { .symbol = CSSValueS, .min = 0.0, .percentMultiplier = 1.0 },
        L { .symbol = CSSValueL,             .percentMultiplier = 1.0 },
        AlphaComponent
    );
};

// hwb() = hwb( [from <color>]? [<hue> | none] [<percentage> | <number> | none]{2} [ / [<alpha-value> | none] ]? )
struct HWBFunction {
    using ColorType = HWBA<float>;
    static constexpr bool allowConversionTo8BitSRGB = true;
    static constexpr OptionSet<Color::Flags> flagsForRelative = Color::Flags::UseColorFunctionSerialization;
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "hwb"_s;

    using H = CSSColorComponent<AngleRaw, NumberRaw, NoneRaw>;
    using W = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using B = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        H { .symbol = CSSValueH, .type = ColorComponentType::Angle },
        W { .symbol = CSSValueW, .percentMultiplier = 1.0          },
        B { .symbol = CSSValueB, .percentMultiplier = 1.0          },
        AlphaComponent
    );
};

// lab() = lab( [from <color>]? [<percentage> | <number> | none]{3} [ / [<alpha-value> | none] ]? )
struct LabFunction {
    using ColorType = Lab<float>;
    static constexpr bool allowConversionTo8BitSRGB = false;
    static constexpr OptionSet<Color::Flags> flagsForRelative = { };
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "lab"_s;

    using L = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using A = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using B = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        L { .symbol = CSSValueL, .min = 0.0, .max = 100.0, .percentMultiplier = 1.0           },
        A { .symbol = CSSValueA,                           .percentMultiplier = 125.0 / 100.0 },
        B { .symbol = CSSValueB,                           .percentMultiplier = 125.0 / 100.0 },
        AlphaComponent
    );
};

// lch() = lch( [from <color>]? [<percentage> | <number> | none]{2} [<hue> | none] [ / [<alpha-value> | none] ]? )
struct LCHFunction {
    using ColorType = LCHA<float>;
    static constexpr bool allowConversionTo8BitSRGB = false;
    static constexpr OptionSet<Color::Flags> flagsForRelative = { };
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "lch"_s;

    using L = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using C = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using H = CSSColorComponent<AngleRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        L { .symbol = CSSValueL, .min = 0.0, .max = 100.0, .percentMultiplier = 1.0           },
        C { .symbol = CSSValueC, .min = 0.0,               .percentMultiplier = 150.0 / 100.0 },
        H { .symbol = CSSValueH, .type = ColorComponentType::Angle                            },
        AlphaComponent
    );
};

// oklab() = oklab( [from <color>]? [<percentage> | <number> | none]{3} [ / [<alpha-value> | none] ]? )
struct OKLabFunction {
    using ColorType = OKLab<float>;
    static constexpr bool allowConversionTo8BitSRGB = false;
    static constexpr OptionSet<Color::Flags> flagsForRelative = { };
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "oklab"_s;

    using L = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using A = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using B = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        L { .symbol = CSSValueL, .min = 0.0, .max = 1.0                                   },
        A { .symbol = CSSValueA,                         .percentMultiplier = 0.4 / 100.0 },
        B { .symbol = CSSValueB,                         .percentMultiplier = 0.4 / 100.0 },
        AlphaComponent
    );
};

// oklch() = oklch( [from <color>]? [<percentage> | <number> | none]{2} [<hue> | none] [ / [<alpha-value> | none] ]? )
struct OKLCHFunction {
    using ColorType = OKLCHA<float>;
    static constexpr bool allowConversionTo8BitSRGB = false;
    static constexpr OptionSet<Color::Flags> flagsForRelative = { };
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = { };
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = false;
    static constexpr ASCIILiteral serializationFunctionName = "oklch"_s;

    using L = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using C = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using H = CSSColorComponent<AngleRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        L { .symbol = CSSValueL, .min = 0.0, .max = 1.0                                   },
        C { .symbol = CSSValueC, .min = 0.0,            .percentMultiplier = 0.4 / 100.0  },
        H { .symbol = CSSValueH, .type = ColorComponentType::Angle },
        AlphaComponent
    );
};

// color() = color( [from <color>]? <rgb-params> [ / [ <alpha-value> | none ] ]? )
// <rgb-params> = <rgb-color-space> [ <number> | <percentage> | none ]{3}
// <rgb-color-space> = srgb | srgb-linear | display-p3 | a98-rgb | prophoto-rgb | rec2020
template<typename T>
struct ColorRGBFunction {
    using ColorType = T;
    static constexpr bool allowConversionTo8BitSRGB = false;
    static constexpr OptionSet<Color::Flags> flagsForRelative = Color::Flags::UseColorFunctionSerialization;
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = Color::Flags::UseColorFunctionSerialization;
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = true;
    static constexpr ASCIILiteral serializationFunctionName = "color"_s;

    using R = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using G = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using B = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        R { .symbol = CSSValueR },
        G { .symbol = CSSValueG },
        B { .symbol = CSSValueB },
        AlphaComponent
    );
};

// color() = color( [from <color>]? <xyz-params> [ / [ <alpha-value> | none ] ]? )
// <xyz-params> = <xyz-color-space> [ <number> | <percentage> | none ]{3}
// <xyz-color-space> = xyz | xyz-d50 | xyz-d65
template<typename T>
struct ColorXYZFunction {
    using ColorType = T;
    static constexpr bool allowConversionTo8BitSRGB = false;
    static constexpr OptionSet<Color::Flags> flagsForRelative = Color::Flags::UseColorFunctionSerialization;
    static constexpr OptionSet<Color::Flags> flagsForAbsolute = Color::Flags::UseColorFunctionSerialization;
    static constexpr auto syntax = CSSColorFunctionSyntax::Modern;
    static constexpr bool usesColorFunctionForSerialization = true;
    static constexpr ASCIILiteral serializationFunctionName = "color"_s;

    using X = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using Y = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;
    using Z = CSSColorComponent<PercentRaw, NumberRaw, NoneRaw>;

    static constexpr auto components = std::make_tuple(
        X { .symbol = CSSValueX },
        Y { .symbol = CSSValueY },
        Z { .symbol = CSSValueZ },
        AlphaComponent
    );
};

} // namespace WebCore
