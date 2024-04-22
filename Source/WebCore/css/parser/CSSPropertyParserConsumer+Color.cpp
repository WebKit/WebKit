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
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+None.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Percent.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSResolvedColorMix.h"
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorMix.h"
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

// The NormalizePercentage structs are specialized for the color types
// that have specified rules for normalizing percentages by component.
// The structs contains static members for the component that describe
// the normalized value when the percent is 100%. As all treat 0% as
// normalizing to 0, that is not encoded in the struct.
template<typename ColorType> struct NormalizePercentage;

template<> struct NormalizePercentage<Lab<float>> {
    //  for L: 0% = 0.0, 100% = 100.0
    //  for a and b: -100% = -125, 100% = 125 (NOTE: 0% is 0)

    static constexpr double maximumLightnessNumber = 100.0;
    static constexpr double lightnessScaleFactor = maximumLightnessNumber / 100.0;
    static constexpr double abScaleFactor = 125.0 / 100.0;
};

template<> struct NormalizePercentage<OKLab<float>> {
    //  for L: 0% = 0.0, 100% = 1.0
    //  for a and b: -100% = -0.4, 100% = 0.4 (NOTE: 0% is 0)

    static constexpr double maximumLightnessNumber = 1.0;
    static constexpr double lightnessScaleFactor = maximumLightnessNumber / 100.0;
    static constexpr double abScaleFactor = 0.4 / 100.0;
};

template<> struct NormalizePercentage<LCHA<float>> {
    //  for L: 0% = 0.0, 100% = 100.0
    //  for C: 0% = 0, 100% = 150

    static constexpr double maximumLightnessNumber = 100.0;
    static constexpr double lightnessScaleFactor = maximumLightnessNumber / 100.0;
    static constexpr double chromaScaleFactor = 150.0 / 100.0;
};

template<> struct NormalizePercentage<OKLCHA<float>> {
    //  for L: 0% = 0.0, 100% = 1.0
    //  for C: 0% = 0.0 100% = 0.4

    static constexpr double maximumLightnessNumber = 1.0;
    static constexpr double lightnessScaleFactor = maximumLightnessNumber / 100.0;
    static constexpr double chromaScaleFactor = 0.4 / 100.0;
};

template<> struct NormalizePercentage<XYZA<float, WhitePoint::D50>> {
    //  for X,Y,Z: 0% = 0.0, 100% = 1.0

    static constexpr double xyzScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<XYZA<float, WhitePoint::D65>> {
    //  for X,Y,Z: 0% = 0.0, 100% = 1.0

    static constexpr double xyzScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedA98RGB<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedDisplayP3<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedProPhotoRGB<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedRec2020<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedSRGBA<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedLinearSRGBA<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<SRGBA<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<typename ColorType>
static double normalizeLightnessPercent(double percent)
{
    return NormalizePercentage<ColorType>::lightnessScaleFactor * percent;
}

template<typename ColorType>
static double normalizeABPercent(double percent)
{
    return NormalizePercentage<ColorType>::abScaleFactor * percent;
}

template<typename ColorType>
static double normalizeChromaPercent(double percent)
{
    return NormalizePercentage<ColorType>::chromaScaleFactor * percent;
}

template<typename ColorType>
static double normalizeXYZPercent(double percent)
{
    return NormalizePercentage<ColorType>::xyzScaleFactor * percent;
}

template<typename ColorType>
static double normalizeRGBPercent(double percent)
{
    return NormalizePercentage<ColorType>::rgbScaleFactor * percent;
}

static double normalizeAlphaPercent(double percent)
{
    static constexpr double alphaScaleFactor = 1.0 / 100.0;
    return alphaScaleFactor * percent;
}

static Color consumeOriginColorRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    return consumeColorRaw(args, state);
}

static std::optional<double> consumeRGBOrHSLLegacyOptionalAlphaRaw(CSSParserTokenRange& args, double defaultValue = 1.0)
{
    if (!consumeCommaIncludingWhitespace(args))
        return defaultValue;

    if (auto alphaParameter = consumeNumberOrPercentRaw(args)) {
        return WTF::switchOn(*alphaParameter,
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 1.0); },
            [] (PercentRaw percent) { return std::clamp(normalizeAlphaPercent(percent.value), 0.0, 1.0); }
        );
    }

    return std::nullopt;
}

static std::optional<double> consumeOptionalAlphaRaw(CSSParserTokenRange& range, double defaultValue = 1.0)
{
    if (!consumeSlashIncludingWhitespace(range))
        return defaultValue;

    if (auto alphaParameter = consumeNumberOrPercentOrNoneRaw(range)) {
        return WTF::switchOn(*alphaParameter,
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 1.0); },
            [] (PercentRaw percent) { return std::clamp(normalizeAlphaPercent(percent.value), 0.0, 1.0); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    return std::nullopt;
}

static std::optional<double> consumeOptionalAlphaRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, double defaultValue = 1.0)
{
    if (!consumeSlashIncludingWhitespace(range))
        return defaultValue;

    if (auto alphaParameter = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(range, symbolTable, ValueRange::All)) {
        return WTF::switchOn(*alphaParameter,
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 1.0); },
            [] (PercentRaw percent) { return std::clamp(normalizeAlphaPercent(percent.value), 0.0, 1.0); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    return std::nullopt;
}

// MARK: - rgb() / rgba()

template<typename ColorType>
static typename ColorType::ComponentType normalizeRGBFunctionComponent(NumberRaw value)
{
    if constexpr (std::is_same_v<typename ColorType::ComponentType, uint8_t>)
        return convertPrescaledSRGBAFloatToSRGBAByte(value.value);
    else if constexpr (IsRGBBoundedType<ColorType>)
        return std::clamp<typename ColorType::ComponentType>(value.value / 255.0, 0.0, 1.0);
    else
        return static_cast<typename ColorType::ComponentType>(value.value / 255.0);
}

template<typename ColorType>
static typename ColorType::ComponentType normalizeRGBFunctionComponent(PercentRaw value)
{
    if constexpr (std::is_same_v<typename ColorType::ComponentType, uint8_t>)
        return convertPrescaledSRGBAFloatToSRGBAByte((value.value / 100.0) * 255.0);
    else if constexpr (IsRGBBoundedType<ColorType>)
        return std::clamp<typename ColorType::ComponentType>(normalizeRGBPercent<ColorType>(value.value), 0.0, 1.0);
    else
        return static_cast<typename ColorType::ComponentType>(normalizeRGBPercent<ColorType>(value.value));
}

template<typename ColorType>
static typename ColorType::ComponentType normalizeRGBFunctionComponent(NoneRaw)
{
    if constexpr (std::is_same_v<typename ColorType::ComponentType, uint8_t>) {
        ASSERT_NOT_REACHED("'none' is invalid in contexts converting to bytes");
        return 0;
    } else
        return std::numeric_limits<typename ColorType::ComponentType>::quiet_NaN();
}

template<typename ColorType>
static typename ColorType::ComponentType normalizeRGBFunctionAlpha(double value)
{
    if constexpr (std::is_same_v<typename ColorType::ComponentType, uint8_t>)
        return convertPrescaledSRGBAFloatToSRGBAByte(value * 255.0);
    else
        return static_cast<typename ColorType::ComponentType>(value);
}

template<typename ColorType>
static typename ColorType::ComponentType normalizeRGBFunctionComponent(NumberOrPercentOrNoneRaw value)
{
    return WTF::switchOn(value, [] (auto value) { return normalizeRGBFunctionComponent<ColorType>(value); } );
}

template<typename ColorType>
static ColorType normalizeRGBFunctionComponents(auto red, auto green, auto blue, auto alpha)
{
    return ColorType {
        normalizeRGBFunctionComponent<ColorType>(red),
        normalizeRGBFunctionComponent<ColorType>(green),
        normalizeRGBFunctionComponent<ColorType>(blue),
        normalizeRGBFunctionAlpha<ColorType>(alpha)
    };
}

static Color parseRelativeRGBParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    // <modern-rgb-syntax> = rgb( [ from <color> ]?
    //         [ <number> | <percentage> | none]{3}
    //         [ / [<alpha-value> | none] ]?  )
    // <modern-rgba-syntax> = rgba( [ from <color> ]?
    //         [ <number> | <percentage> | none]{3}
    //         [ / [<alpha-value> | none] ]?  )

    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, state);
    if (!originColor.isValid())
        return { };

    auto originColorAsSRGB = originColor.toColorTypeLossy<ExtendedSRGBA<float>>();
    auto originColorAsSRGBResolved = originColorAsSRGB.resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueR, CSSUnitType::CSS_NUMBER, originColorAsSRGBResolved.red * 255.0 },
        { CSSValueG, CSSUnitType::CSS_NUMBER, originColorAsSRGBResolved.green * 255.0 },
        { CSSValueB, CSSUnitType::CSS_NUMBER, originColorAsSRGBResolved.blue * 255.0 },
        { CSSValueAlpha, CSSUnitType::CSS_NUMBER, originColorAsSRGBResolved.alpha }
    };

    auto red = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!red)
        return { };
    auto green = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!green)
        return { };
    auto blue = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!blue)
        return { };

    // This alpha consumer is a little different than ones for non-relative colors and passes
    // in the alpha value of the origin color so that we can implement the following rule
    // from CSS Color 5 (https://drafts.csswg.org/css-color-5/#rcs-intro):
    //
    //   § 4.1. Processing Model for Relative Colors
    //
    //   "If the alpha value of the relative color is omitted, it defaults to that of the
    //    origin color (rather than defaulting to 100%, as it does in the absolute syntax)."
    //
    auto alpha = consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable, originColorAsSRGB.unresolved().alpha);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    // The `UseColorFunctionSerialization` ensures the relative form serializes as `color(srgb ...)`.
    return { normalizeRGBFunctionComponents<ExtendedSRGBA<float>>(*red, *green, *blue, *alpha), Color::Flags::UseColorFunctionSerialization };
}

static Color parseNonRelativeRGBParametersLegacyRaw(CSSParserTokenRange& args, ColorParserState& state, NumberOrPercentOrNoneRaw redOrNone)
{
    // <legacy-rgb-syntax> =   rgb( <percentage>#{3} , <alpha-value>? ) |  rgb( <number>#{3} , <alpha-value>? )
    // <legacy-rgba-syntax> = rgba( <percentage>#{3} , <alpha-value>? ) | rgba( <number>#{3} , <alpha-value>? )

    return WTF::switchOn(redOrNone,
        [&args, &state] (NumberRaw red) -> Color {
            auto green = consumeNumberRaw(args);
            if (!green)
                return { };

            if (!consumeCommaIncludingWhitespace(args))
                return { };

            auto blue = consumeNumberRaw(args);
            if (!blue)
                return { };
            auto alpha = consumeRGBOrHSLLegacyOptionalAlphaRaw(args);
            if (!alpha)
                return { };

            if (!args.atEnd())
                return { };

            if (state.nestingLevel > 1) {
                // If the color is being consumed as part of a composition (relative color, color-mix, light-dark, etc.), we store the
                // value as a SRGB<float> to allow for maximum precision.
                return normalizeRGBFunctionComponents<SRGBA<float>>(red, *green, *blue, *alpha);
            }

            return normalizeRGBFunctionComponents<SRGBA<uint8_t>>(red, *green, *blue, *alpha);
        },
        [&args, &state] (PercentRaw red) -> Color {
            auto green = consumePercentRaw(args);
            if (!green)
                return { };

            if (!consumeCommaIncludingWhitespace(args))
                return { };

            auto blue = consumePercentRaw(args);
            if (!blue)
                return { };
            auto alpha = consumeRGBOrHSLLegacyOptionalAlphaRaw(args);
            if (!alpha)
                return { };

            if (!args.atEnd())
                return { };

            if (state.nestingLevel > 1) {
                // If the color is being consumed as part of a composition (relative color, color-mix, light-dark, etc.), we store the
                // value as a SRGB<float> to allow for maximum precision.
                return normalizeRGBFunctionComponents<SRGBA<float>>(red, *green, *blue, *alpha);
            }

            return normalizeRGBFunctionComponents<SRGBA<uint8_t>>(red, *green, *blue, *alpha);
        },
        [] (NoneRaw) -> Color {
            // `none` is invalid for the legacy syntax.
            return { };
        }
    );
}

static Color parseNonRelativeRGBParametersModernRaw(CSSParserTokenRange& args, ColorParserState& state, NumberOrPercentOrNoneRaw red)
{
    // <modern-rgb-syntax> =   rgb( [ <number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )
    // <modern-rgba-syntax> = rgba( [ <number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )

    auto green = consumeNumberOrPercentOrNoneRaw(args);
    if (!green)
        return { };
    auto blue = consumeNumberOrPercentOrNoneRaw(args);
    if (!blue)
        return { };
    auto alpha = consumeOptionalAlphaRaw(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    if (std::holds_alternative<NoneRaw>(red) || std::holds_alternative<NoneRaw>(*green) || std::holds_alternative<NoneRaw>(*blue) || std::isnan(*alpha)) {
        // If any component uses "none", we store the value as a SRGBA<float> to allow for storage of the special value as NaN.
        return normalizeRGBFunctionComponents<SRGBA<float>>(red, *green, *blue, *alpha);
    }

    if (state.nestingLevel > 1) {
        // If the color is being consumed as part of a composition (relative color, color-mix, light-dark, etc.), we store the
        // value as a SRGB<float> to allow for maximum precision.
        return normalizeRGBFunctionComponents<SRGBA<float>>(red, *green, *blue, *alpha);
    }

    return normalizeRGBFunctionComponents<SRGBA<uint8_t>>(red, *green, *blue, *alpha);
}

static Color parseNonRelativeRGBParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    // rgb() = [ <legacy-rgb-syntax> | <modern-rgb-syntax> ]
    // rgba() = [ <legacy-rgba-syntax> | <modern-rgba-syntax> ]
    //
    // <legacy-rgb-syntax> =   rgb( <percentage>#{3} , <alpha-value>? ) |  rgb( <number>#{3} , <alpha-value>? )
    // <legacy-rgba-syntax> = rgba( <percentage>#{3} , <alpha-value>? ) | rgba( <number>#{3} , <alpha-value>? )
    //
    // <modern-rgb-syntax> =   rgb( [ <number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )
    // <modern-rgba-syntax> = rgba( [ <number> | <percentage> | none]{3} [ / [<alpha-value> | none] ]? )

    // To determine whether this is going to use the modern or legacy syntax, we need to consume
    // the first component and the separated after it. If the separator is a `comma`, its using
    // the legacy syntax, if the separator is a space, it is using the modern syntax.

    auto red = consumeNumberOrPercentOrNoneRaw(args);
    if (!red)
        return { };

    if (consumeCommaIncludingWhitespace(args)) {
        // A `comma` getting successfully consumed means this is using the legacy syntax.
        return parseNonRelativeRGBParametersLegacyRaw(args, state, *red);
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return parseNonRelativeRGBParametersModernRaw(args, state, *red);
    }
}

static Color parseRGBParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return parseRelativeRGBParametersRaw(args, state);
    return parseNonRelativeRGBParametersRaw(args, state);
}

// MARK: - hsl() / hsla()

static HSLA<float> colorByResolvingHSLComponentsModern(ColorParserState& state, AngleOrNumberOrNoneRaw hue, NumberOrPercentOrNoneRaw saturation, NumberOrPercentOrNoneRaw lightness, double alpha)
{
    auto resolvedHue = WTF::switchOn(hue,
        [] (AngleRaw angle) { return normalizeHue(CSSPrimitiveValue::computeDegrees(angle.type, angle.value)); },
        [] (NumberRaw number) { return normalizeHue(number.value); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    double resolvedSaturation;
    double resolvedLightness;

    if (state.clampHSLAtParseTime) {
        resolvedSaturation = WTF::switchOn(saturation,
            [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 100.0); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
        resolvedLightness = WTF::switchOn(lightness,
            [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 100.0); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    } else {
        resolvedSaturation = WTF::switchOn(saturation,
            [] (PercentRaw percent) { return std::max(0.0, percent.value); },
            [] (NumberRaw number) { return std::max(0.0, number.value); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
        resolvedLightness = WTF::switchOn(lightness,
            [] (PercentRaw percent) { return percent.value; },
            [] (NumberRaw number) { return number.value; },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    return HSLA<float> {
        static_cast<float>(resolvedHue),
        static_cast<float>(resolvedSaturation),
        static_cast<float>(resolvedLightness),
        static_cast<float>(alpha)
    };
}

static HSLA<float> colorByResolvingHSLComponentsLegacy(ColorParserState& state, AngleOrNumberRaw hue, PercentRaw saturation, PercentRaw lightness, double alpha)
{
    auto resolvedHue = WTF::switchOn(hue,
        [] (AngleRaw angle) { return normalizeHue(CSSPrimitiveValue::computeDegrees(angle.type, angle.value)); },
        [] (NumberRaw number) { return normalizeHue(number.value); }
    );

    double resolvedSaturation;
    double resolvedLightness;
    if (state.clampHSLAtParseTime) {
        resolvedSaturation = std::clamp(saturation.value, 0.0, 100.0);
        resolvedLightness = std::clamp(lightness.value, 0.0, 100.0);
    } else {
        resolvedSaturation = std::max(0.0, saturation.value);
        resolvedLightness = lightness.value;
    }

    return HSLA<float> {
        static_cast<float>(resolvedHue),
        static_cast<float>(resolvedSaturation),
        static_cast<float>(resolvedLightness),
        static_cast<float>(alpha)
    };
}

static Color parseRelativeHSLParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    // <modern-hsl-syntax> = hsl([from <color>]?
    //           [<hue> | none]
    //           [<percentage> | <number> | none]
    //           [<percentage> | <number> | none]
    //           [ / [<alpha-value> | none] ]? )
    // <modern-hsla-syntax> = hsla([from <color>]?
    //         [<hue> | none]
    //         [<percentage> | <number> | none]
    //         [<percentage> | <number> | none]
    //         [ / [<alpha-value> | none] ]? )

    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, state);
    if (!originColor.isValid())
        return { };

    auto originColorAsHSL = originColor.toColorTypeLossy<HSLA<float>>();
    auto originColorAsHSLResolved = originColorAsHSL.resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueH, CSSUnitType::CSS_NUMBER, originColorAsHSLResolved.hue },
        { CSSValueS, CSSUnitType::CSS_NUMBER, originColorAsHSLResolved.saturation },
        { CSSValueL, CSSUnitType::CSS_NUMBER, originColorAsHSLResolved.lightness },
        { CSSValueAlpha, CSSUnitType::CSS_NUMBER, originColorAsHSLResolved.alpha }
    };

    auto hue = consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(args, symbolTable, state.mode);
    if (!hue)
        return { };

    auto saturation = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!saturation)
        return { };

    auto lightness = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!lightness)
        return { };

    // This alpha consumer is a little different than ones for non-relative colors and passes
    // in the alpha value of the origin color so that we can implement the following rule
    // from CSS Color 5 (https://drafts.csswg.org/css-color-5/#rcs-intro):
    //
    //   § 4.1. Processing Model for Relative Colors
    //
    //   "If the alpha value of the relative color is omitted, it defaults to that of the
    //    origin color (rather than defaulting to 100%, as it does in the absolute syntax)."
    //
    auto alpha = consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable, originColorAsHSL.unresolved().alpha);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    // The `UseColorFunctionSerialization` ensures the relative form serializes as `color(srgb ...)`.
    return { colorByResolvingHSLComponentsModern(state, *hue, *saturation, *lightness, *alpha), Color::Flags::UseColorFunctionSerialization };
}

static Color parseNonRelativeHSLParametersLegacyRaw(CSSParserTokenRange& args, ColorParserState& state, AngleOrNumberOrNoneRaw hueOrNone)
{
    // <legacy-hsl-syntax>   = hsl( <hue>, <percentage>, <percentage>, <alpha-value>? )
    // <legacy-hsla-syntax> = hsla( <hue>, <percentage>, <percentage>, <alpha-value>? )

    auto hue = WTF::switchOn(hueOrNone,
        [] (AngleRaw angle) -> std::optional<AngleOrNumberRaw> {
            return AngleOrNumberRaw { angle };
        },
        [] (NumberRaw number) -> std::optional<AngleOrNumberRaw> {
            return AngleOrNumberRaw { number };
        },
        [] (NoneRaw) -> std::optional<AngleOrNumberRaw> {
            // `none` is invalid for the legacy syntax.
            return std::nullopt;
        }
    );
    if (!hue)
        return { };

    auto saturation = consumePercentRaw(args);
    if (!saturation)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    auto lightness = consumePercentRaw(args);
    if (!lightness)
        return { };

    auto alpha = consumeRGBOrHSLLegacyOptionalAlphaRaw(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto hsla = colorByResolvingHSLComponentsLegacy(state, *hue, *saturation, *lightness, *alpha);
    auto unresolved = hsla.unresolved();

    if (unresolved.saturation > 100.0 || unresolved.lightness < 0.0 || unresolved.lightness > 100.0) {
        // If any component is outside the reference range, we store the value as a HSLA<float> to allow for non-SRGB gamut values.
        return hsla;
    }

    if (state.nestingLevel > 1) {
        // If the color is being consumed as part of a composition (relative color, color-mix, light-dark, etc.), we store the value as a HSLA<float> to allow for maximum precision.
        return hsla;
    }

    // The explicit conversion to SRGBA<uint8_t> is an intentional performance optimization that allows storing the
    // color with no extra allocation for an extended color object. This is permissible due to the historical requirement
    // that HSLA colors serialize using the legacy color syntax (rgb()/rgba()) and historically have used the 8-bit rgba
    // internal representation in engines.
    return convertColor<SRGBA<uint8_t>>(hsla);
}

static Color parseNonRelativeHSLParametersModernRaw(CSSParserTokenRange& args, ColorParserState& state, AngleOrNumberOrNoneRaw hue)
{
    // <modern-hsl-syntax> = hsl(
    //     [<hue> | none]
    //     [<percentage> | <number> | none]
    //     [<percentage> | <number> | none]
    //     [ / [<alpha-value> | none] ]? )
    // <modern-hsla-syntax> = hsla(
    //     [<hue> | none]
    //     [<percentage> | <number> | none]
    //     [<percentage> | <number> | none]
    //     [ / [<alpha-value> | none] ]? )

    auto saturation = consumeNumberOrPercentOrNoneRaw(args);
    if (!saturation)
        return { };

    auto lightness = consumeNumberOrPercentOrNoneRaw(args);
    if (!lightness)
        return { };

    auto alpha = consumeOptionalAlphaRaw(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto hsla = colorByResolvingHSLComponentsModern(state, hue, *saturation, *lightness, *alpha);
    auto unresolved = hsla.unresolved();

    if (unresolved.anyComponentIsNone()) {
        // If any component uses "none", we store the value as a HSLA<float> to allow for storage of the special value as NaN.
        return hsla;
    }

    if (unresolved.saturation > 100.0 || unresolved.lightness < 0.0 || unresolved.lightness > 100.0) {
        // If any component is outside the reference range, we store the value as a HSLA<float> to allow for non-SRGB gamut values.
        return hsla;
    }

    if (state.nestingLevel > 1) {
        // If the color is being consumed as part of a composition (relative color, color-mix, light-dark, etc.), we store the value as a HSLA<float> to allow for maximum precision.
        return hsla;
    }

    // The explicit conversion to SRGBA<uint8_t> is an intentional performance optimization that allows storing the
    // color with no extra allocation for an extended color object. This is permissible due to the historical requirement
    // that HSLA colors serialize using the legacy color syntax (rgb()/rgba()) and historically have used the 8-bit rgba
    // internal representation in engines.
    return convertColor<SRGBA<uint8_t>>(hsla);
}

static Color parseNonRelativeHSLParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    // hsl() = [ <legacy-hsl-syntax> | <modern-hsl-syntax> ]
    // hsla() = [ <legacy-hsla-syntax> | <modern-hsla-syntax> ]
    //
    // <legacy-hsl-syntax>   = hsl( <hue>, <percentage>, <percentage>, <alpha-value>? )
    // <legacy-hsla-syntax> = hsla( <hue>, <percentage>, <percentage>, <alpha-value>? )
    //
    // <modern-hsl-syntax> = hsl(
    //     [<hue> | none]
    //     [<percentage> | <number> | none]
    //     [<percentage> | <number> | none]
    //     [ / [<alpha-value> | none] ]? )
    // <modern-hsla-syntax> = hsla(
    //     [<hue> | none]
    //     [<percentage> | <number> | none]
    //     [<percentage> | <number> | none]
    //     [ / [<alpha-value> | none] ]? )

    // To determine whether this is going to use the modern or legacy syntax, we need to consume
    // the first component and the separated after it. If the separator is a `comma`, its using
    // the legacy syntax, if the separator is a space, it is using the modern syntax.

    auto hue = consumeAngleOrNumberOrNoneRaw(args, state.mode);
    if (!hue)
        return { };

    if (consumeCommaIncludingWhitespace(args)) {
        // A `comma` getting successfully consumed means this is using the legacy syntax.
        return parseNonRelativeHSLParametersLegacyRaw(args, state, *hue);
    } else {
        // A `comma` NOT getting successfully consumed means this is using the modern syntax.
        return parseNonRelativeHSLParametersModernRaw(args, state, *hue);
    }
}

static Color parseHSLParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return parseRelativeHSLParametersRaw(args, state);
    return parseNonRelativeHSLParametersRaw(args, state);
}

template<typename ConsumerForHue, typename ConsumerForWhitenessAndBlackness, typename ConsumerForAlpha>
static Color parseHWBParametersRaw(CSSParserTokenRange& args, ConsumerForHue&& hueConsumer, ConsumerForWhitenessAndBlackness&& whitenessAndBlacknessConsumer, ConsumerForAlpha&& alphaConsumer)
{
    auto hue = hueConsumer(args);
    if (!hue)
        return { };

    auto whiteness = whitenessAndBlacknessConsumer(args);
    if (!whiteness)
        return { };

    auto blackness = whitenessAndBlacknessConsumer(args);
    if (!blackness)
        return { };

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedHue = WTF::switchOn(*hue,
        [] (AngleRaw angle) { return CSSPrimitiveValue::computeDegrees(angle.type, angle.value); },
        [] (NumberRaw number) { return number.value; },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto clampedWhiteness = WTF::switchOn(*whiteness,
        [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto clampedBlackness = WTF::switchOn(*blackness,
        [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    if (std::isnan(normalizedHue) || std::isnan(clampedWhiteness) || std::isnan(clampedBlackness) || std::isnan(*alpha)) {
        auto [normalizedWhitness, normalizedBlackness] = normalizeClampedWhitenessBlacknessAllowingNone(clampedWhiteness, clampedBlackness);

        // If any component uses "none", we store the value as a HWBA<float> to allow for storage of the special value as NaN.
        return HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) };
    }

    auto [normalizedWhitness, normalizedBlackness] = normalizeClampedWhitenessBlacknessDisallowingNone(clampedWhiteness, clampedBlackness);

    if (normalizedHue < 0.0 || normalizedHue > 360.0) {
        // If 'hue' is not in the [0, 360] range, we store the value as a HWBA<float> to allow for correct interpolation
        // using the "specified" hue interpolation method.
        return HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) };
    }

    // The explicit conversion to SRGBA<uint8_t> is an intentional performance optimization that allows storing the
    // color with no extra allocation for an extended color object. This is permissible due to the historical requirement
    // that HWBA colors serialize using the legacy color syntax (rgb()/rgba()) and historically have used the 8-bit rgba
    // internal representation in engines.
    return convertColor<SRGBA<uint8_t>>(HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) });
}

static Color parseRelativeHWBParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, state);
    if (!originColor.isValid())
        return { };

    auto originColorAsHWB = originColor.toColorTypeLossy<HWBA<float>>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsHWB.hue },
        { CSSValueW, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.whiteness },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.blackness },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.alpha * 100.0 }
    };

    auto hueConsumer = [&symbolTable, &state](auto& args) { return consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(args, symbolTable, state.mode); };
    auto whitenessAndBlacknessConsumer = [&symbolTable](auto& args) { return consumePercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseHWBParametersRaw(args, WTFMove(hueConsumer), WTFMove(whitenessAndBlacknessConsumer), WTFMove(alphaConsumer));
}

static Color parseNonRelativeHWBParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    auto hueConsumer = [&state](auto& args) { return consumeAngleOrNumberOrNoneRaw(args, state.mode); };
    auto whitenessAndBlacknessConsumer = [](auto& args) { return consumePercentOrNoneRaw(args); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseHWBParametersRaw(args, WTFMove(hueConsumer), WTFMove(whitenessAndBlacknessConsumer), WTFMove(alphaConsumer));
}

static Color parseHWBParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueHwb);

    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return parseRelativeHWBParametersRaw(args, state);
    return parseNonRelativeHWBParametersRaw(args, state);
}

template<typename ColorType, typename ConsumerForLightness, typename ConsumerForAB, typename ConsumerForAlpha>
static Color parseLabParametersRaw(CSSParserTokenRange& args, ConsumerForLightness&& lightnessConsumer, ConsumerForAB&& abConsumer, ConsumerForAlpha&& alphaConsumer)
{
    auto lightness = lightnessConsumer(args);
    if (!lightness)
        return { };

    auto aValue = abConsumer(args);
    if (!aValue)
        return { };

    auto bValue = abConsumer(args);
    if (!bValue)
        return { };

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedLightness = WTF::switchOn(*lightness,
        [] (NumberRaw number) { return std::clamp(number.value, 0.0, NormalizePercentage<ColorType>::maximumLightnessNumber); },
        [] (PercentRaw percent) { return std::clamp(normalizeLightnessPercent<ColorType>(percent.value), 0.0, NormalizePercentage<ColorType>::maximumLightnessNumber); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedA = WTF::switchOn(*aValue,
        [] (NumberRaw number) { return number.value; },
        [] (PercentRaw percent) { return normalizeABPercent<ColorType>(percent.value); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedB = WTF::switchOn(*bValue,
        [] (NumberRaw number) { return number.value; },
        [] (PercentRaw percent) { return normalizeABPercent<ColorType>(percent.value); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    return ColorType { static_cast<float>(normalizedLightness), static_cast<float>(normalizedA), static_cast<float>(normalizedB), static_cast<float>(*alpha) };
}

template<typename ColorType>
static Color parseRelativeLabParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, state);
    if (!originColor.isValid())
        return { };

    auto originColorAsLab = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueL, CSSUnitType::CSS_NUMBER, originColorAsLab.lightness },
        { CSSValueA, CSSUnitType::CSS_NUMBER, originColorAsLab.a },
        { CSSValueB, CSSUnitType::CSS_NUMBER, originColorAsLab.b },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsLab.alpha * 100.0 }
    };

    auto lightnessConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto abConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseLabParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseNonRelativeLabParametersRaw(CSSParserTokenRange& args)
{
    auto lightnessConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto abConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseLabParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseLabParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueLab || range.peek().functionId() == CSSValueOklab);

    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return parseRelativeLabParametersRaw<ColorType>(args, state);
    return parseNonRelativeLabParametersRaw<ColorType>(args);
}

template<typename ColorType, typename ConsumerForLightness, typename ConsumerForChroma, typename ConsumerForHue, typename ConsumerForAlpha>
static Color parseLCHParametersRaw(CSSParserTokenRange& args, ConsumerForLightness&& lightnessConsumer, ConsumerForChroma&& chromaConsumer, ConsumerForHue&& hueConsumer, ConsumerForAlpha&& alphaConsumer)
{
    auto lightness = lightnessConsumer(args);
    if (!lightness)
        return { };

    auto chroma = chromaConsumer(args);
    if (!chroma)
        return { };

    auto hue = hueConsumer(args);
    if (!hue)
        return { };

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedLightness = WTF::switchOn(*lightness,
        [] (NumberRaw number) { return std::clamp(number.value, 0.0, NormalizePercentage<ColorType>::maximumLightnessNumber); },
        [] (PercentRaw percent) { return std::clamp(normalizeLightnessPercent<ColorType>(percent.value), 0.0, NormalizePercentage<ColorType>::maximumLightnessNumber); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedChroma = WTF::switchOn(*chroma,
        [] (NumberRaw number) { return std::max(0.0, number.value); },
        [] (PercentRaw percent) { return std::max(0.0, normalizeChromaPercent<ColorType>(percent.value)); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedHue = WTF::switchOn(*hue,
        [] (AngleRaw angle) { return CSSPrimitiveValue::computeDegrees(angle.type, angle.value); },
        [] (NumberRaw number) { return number.value; },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    return ColorType { static_cast<float>(normalizedLightness), static_cast<float>(normalizedChroma), static_cast<float>(normalizedHue), static_cast<float>(*alpha) };
}

template<typename ColorType>
static Color parseRelativeLCHParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, state);
    if (!originColor.isValid())
        return { };

    auto originColorAsLCH = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueL, CSSUnitType::CSS_NUMBER, originColorAsLCH.lightness },
        { CSSValueC, CSSUnitType::CSS_NUMBER, originColorAsLCH.chroma },
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsLCH.hue },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsLCH.alpha * 100.0 }
    };

    auto lightnessConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto chromaConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto hueConsumer = [&symbolTable, &state](auto& args) { return consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(args, symbolTable, state.mode); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseLCHParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseNonRelativeLCHParametersRaw(CSSParserTokenRange& args, ColorParserState& state)
{
    auto lightnessConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto chromaConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto hueConsumer = [&state](auto& args) { return consumeAngleOrNumberOrNoneRaw(args, state.mode); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseLCHParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseLCHParametersRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    ASSERT(range.peek().functionId() == CSSValueLch || range.peek().functionId() == CSSValueOklch);

    auto args = consumeFunction(range);

    if (args.peek().id() == CSSValueFrom)
        return parseRelativeLCHParametersRaw<ColorType>(args, state);
    return parseNonRelativeLCHParametersRaw<ColorType>(args, state);
}

template<typename ColorType, typename ConsumerForRGB, typename ConsumerForAlpha>
static Color parseColorFunctionForRGBTypesRaw(CSSParserTokenRange& args, ConsumerForRGB&& rgbConsumer, ConsumerForAlpha&& alphaConsumer)
{
    double channels[3] = { 0, 0, 0 };
    for (auto& channel : channels) {
        auto value = rgbConsumer(args);
        if (!value)
            return { };

        channel = WTF::switchOn(*value,
            [] (NumberRaw number) { return number.value; },
            [] (PercentRaw percent) { return normalizeRGBPercent<ColorType>(percent.value); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return { ColorType { static_cast<float>(channels[0]), static_cast<float>(channels[1]), static_cast<float>(channels[2]), static_cast<float>(*alpha) }, Color::Flags::UseColorFunctionSerialization };
}

template<typename ColorType> static Color parseRelativeColorFunctionForRGBTypes(CSSParserTokenRange& args, Color originColor)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB || args.peek().id() == CSSValueSrgbLinear);

    consumeIdentRaw(args);

    auto originColorAsColorType = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueR, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.red * 100.0 },
        { CSSValueG, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.green * 100.0 },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.blue * 100.0 },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.alpha * 100.0 }
    };

    auto consumeRGB = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable, ValueRange::All); };
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForRGBTypesRaw<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ColorType> static Color parseColorFunctionForRGBTypesRaw(CSSParserTokenRange& args)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB || args.peek().id() == CSSValueSrgbLinear);

    consumeIdentRaw(args);

    auto consumeRGB = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseColorFunctionForRGBTypesRaw<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ColorType, typename ConsumerForXYZ, typename ConsumerForAlpha>
static Color parseColorFunctionForXYZTypesRaw(CSSParserTokenRange& args, ConsumerForXYZ&& xyzConsumer, ConsumerForAlpha&& alphaConsumer)
{
    double channels[3] = { 0, 0, 0 };
    for (auto& channel : channels) {
        auto value = xyzConsumer(args);
        if (!value)
            return { };

        channel = WTF::switchOn(*value,
            [] (NumberRaw number) { return number.value; },
            [] (PercentRaw percent) { return normalizeXYZPercent<ColorType>(percent.value); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return { ColorType { static_cast<float>(channels[0]), static_cast<float>(channels[1]), static_cast<float>(channels[2]), static_cast<float>(*alpha) }, Color::Flags::UseColorFunctionSerialization };
}

template<typename ColorType> static Color parseRelativeColorFunctionForXYZTypes(CSSParserTokenRange& args, Color originColor)
{
    ASSERT(args.peek().id() == CSSValueXyz || args.peek().id() == CSSValueXyzD50 || args.peek().id() == CSSValueXyzD65);

    consumeIdentRaw(args);

    auto originColorAsXYZ = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueX, CSSUnitType::CSS_NUMBER, originColorAsXYZ.x },
        { CSSValueY, CSSUnitType::CSS_NUMBER, originColorAsXYZ.y },
        { CSSValueZ, CSSUnitType::CSS_NUMBER, originColorAsXYZ.z },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsXYZ.alpha * 100.0 }
    };

    auto consumeXYZ = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForXYZTypesRaw<ColorType>(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
}

template<typename ColorType> static Color parseColorFunctionForXYZTypesRaw(CSSParserTokenRange& args)
{
    ASSERT(args.peek().id() == CSSValueXyz || args.peek().id() == CSSValueXyzD50 || args.peek().id() == CSSValueXyzD65);

    consumeIdentRaw(args);

    auto consumeXYZ = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseColorFunctionForXYZTypesRaw<ColorType>(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
}

static Color parseRelativeColorFunctionParameters(CSSParserTokenRange& args, ColorParserState& state)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, state);
    if (!originColor.isValid())
        return { };

    switch (args.peek().id()) {
    case CSSValueA98Rgb:
        return parseRelativeColorFunctionForRGBTypes<ExtendedA98RGB<float>>(args, WTFMove(originColor));
    case CSSValueDisplayP3:
        return parseRelativeColorFunctionForRGBTypes<ExtendedDisplayP3<float>>(args, WTFMove(originColor));
    case CSSValueProphotoRgb:
        return parseRelativeColorFunctionForRGBTypes<ExtendedProPhotoRGB<float>>(args, WTFMove(originColor));
    case CSSValueRec2020:
        return parseRelativeColorFunctionForRGBTypes<ExtendedRec2020<float>>(args, WTFMove(originColor));
    case CSSValueSRGB:
        return parseRelativeColorFunctionForRGBTypes<ExtendedSRGBA<float>>(args, WTFMove(originColor));
    case CSSValueSrgbLinear:
        return parseRelativeColorFunctionForRGBTypes<ExtendedLinearSRGBA<float>>(args, WTFMove(originColor));
    case CSSValueXyzD50:
        return parseRelativeColorFunctionForXYZTypes<XYZA<float, WhitePoint::D50>>(args, WTFMove(originColor));
    case CSSValueXyz:
    case CSSValueXyzD65:
        return parseRelativeColorFunctionForXYZTypes<XYZA<float, WhitePoint::D65>>(args, WTFMove(originColor));
    default:
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

static Color parseNonRelativeColorFunctionParameters(CSSParserTokenRange& args)
{
    switch (args.peek().id()) {
    case CSSValueA98Rgb:
        return parseColorFunctionForRGBTypesRaw<ExtendedA98RGB<float>>(args);
    case CSSValueDisplayP3:
        return parseColorFunctionForRGBTypesRaw<ExtendedDisplayP3<float>>(args);
    case CSSValueProphotoRgb:
        return parseColorFunctionForRGBTypesRaw<ExtendedProPhotoRGB<float>>(args);
    case CSSValueRec2020:
        return parseColorFunctionForRGBTypesRaw<ExtendedRec2020<float>>(args);
    case CSSValueSRGB:
        return parseColorFunctionForRGBTypesRaw<ExtendedSRGBA<float>>(args);
    case CSSValueSrgbLinear:
        return parseColorFunctionForRGBTypesRaw<ExtendedLinearSRGBA<float>>(args);
    case CSSValueXyzD50:
        return parseColorFunctionForXYZTypesRaw<XYZA<float, WhitePoint::D50>>(args);
    case CSSValueXyz:
    case CSSValueXyzD65:
        return parseColorFunctionForXYZTypesRaw<XYZA<float, WhitePoint::D65>>(args);
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

    auto color = [&] {
        if (args.peek().id() == CSSValueFrom)
            return parseRelativeColorFunctionParameters(args, state);
        return parseNonRelativeColorFunctionParameters(args);
    }();

    ASSERT(!color.isValid() || color.usesColorFunctionSerialization());
    return color;
}

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

    auto originBackgroundColor = consumeOriginColorRaw(args, state);
    if (!originBackgroundColor.isValid())
        return { };

    if (!consumeIdentRaw<CSSValueVs>(args))
        return { };

    Vector<Color> colorsToCompareAgainst;
    bool consumedTo = false;
    do {
        auto colorToCompareAgainst = consumeOriginColorRaw(args, state);
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

    result.color = consumeOriginColorRaw(args, state);
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

    if (auto percent = consumePercent(args, ValueRange::All)) {
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
        if (auto percent = consumePercent(args, ValueRange::All)) {
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

static Color parseColorFunctionRaw(CSSParserTokenRange& range, ColorParserState& state)
{
    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    Color color;
    switch (functionId) {
    case CSSValueRgb:
        color = parseRGBParametersRaw(colorRange, state);
        break;
    case CSSValueRgba:
        color = parseRGBParametersRaw(colorRange, state);
        break;
    case CSSValueHsl:
        color = parseHSLParametersRaw(colorRange, state);
        break;
    case CSSValueHsla:
        color = parseHSLParametersRaw(colorRange, state);
        break;
    case CSSValueHwb:
        color = parseHWBParametersRaw(colorRange, state);
        break;
    case CSSValueLab:
        color = parseLabParametersRaw<Lab<float>>(colorRange, state);
        break;
    case CSSValueLch:
        color = parseLCHParametersRaw<LCHA<float>>(colorRange, state);
        break;
    case CSSValueOklab:
        color = parseLabParametersRaw<OKLab<float>>(colorRange, state);
        break;
    case CSSValueOklch:
        color = parseLCHParametersRaw<OKLCHA<float>>(colorRange, state);
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
        color = checkColor(parseRGBParametersRaw(colorRange, state));
        break;
    case CSSValueRgba:
        color = checkColor(parseRGBParametersRaw(colorRange, state));
        break;
    case CSSValueHsl:
        color = checkColor(parseHSLParametersRaw(colorRange, state));
        break;
    case CSSValueHsla:
        color = checkColor(parseHSLParametersRaw(colorRange, state));
        break;
    case CSSValueHwb:
        color = checkColor(parseHWBParametersRaw(colorRange, state));
        break;
    case CSSValueLab:
        color = checkColor(parseLabParametersRaw<Lab<float>>(colorRange, state));
        break;
    case CSSValueLch:
        color = checkColor(parseLCHParametersRaw<LCHA<float>>(colorRange, state));
        break;
    case CSSValueOklab:
        color = checkColor(parseLabParametersRaw<OKLab<float>>(colorRange, state));
        break;
    case CSSValueOklch:
        color = checkColor(parseLCHParametersRaw<OKLCHA<float>>(colorRange, state));
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
