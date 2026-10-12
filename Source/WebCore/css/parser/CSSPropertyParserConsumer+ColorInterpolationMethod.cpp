/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSPropertyParserConsumer+ColorInterpolationMethod.h"

#include "CSSColorInterpolationMethod.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - color-mix()

static std::optional<HueInterpolationMethod> consumeHueInterpolationMethod(CSSParserTokenRange& range)
{
    static constexpr SortedArrayMap hueInterpolationMethodMap { WTF::toArray<std::pair<CSSValueID, HueInterpolationMethod>>({
        { CSSValueID::Shorter, HueInterpolationMethod::Shorter },
        { CSSValueID::Longer, HueInterpolationMethod::Longer },
        { CSSValueID::Increasing, HueInterpolationMethod::Increasing },
        { CSSValueID::Decreasing, HueInterpolationMethod::Decreasing },
    }) };

    return consumeIdentUsingMapping(range, hueInterpolationMethodMap);
}

std::optional<CSS::ColorInterpolationMethod> consumeColorInterpolationMethod(CSSParserTokenRange& args, CSS::PropertyParserState&)
{
    // <rectangular-color-space> = srgb | srgb-linear | display-p3 | a98-rgb | prophoto-rgb | rec2020 | lab | oklab | xyz | xyz-d50 | xyz-d65
    // <polar-color-space> = hsl | hwb | lch | oklch
    // <hue-interpolation-method> = [ shorter | longer | increasing | decreasing ] hue
    // <color-interpolation-method> = in [ <rectangular-color-space> | <polar-color-space> <hue-interpolation-method>? ]
    // https://drafts.csswg.org/css-color-5/#color-interpolation-method

    ASSERT(args.peek().id() == CSSValueID::In);
    consumeIdentRaw(args);

    auto consumePolarColorSpace = [](CSSParserTokenRange& args, auto colorInterpolationMethod) -> std::optional<CSS::ColorInterpolationMethod> {
        // Consume the color space identifier.
        args.consumeIncludingWhitespace();

        // <hue-interpolation-method> is optional, so if it is not provided, we just use the default value
        // specified in the passed in 'colorInterpolationMethod' parameter.
        auto hueInterpolationMethod = consumeHueInterpolationMethod(args);
        if (!hueInterpolationMethod)
            return CSS::ColorInterpolationMethod { .value = { colorInterpolationMethod, AlphaPremultiplication::Premultiplied } };

        // If the hue-interpolation-method was provided it must be followed immediately by the 'hue' identifier.
        if (!consumeIdentRaw<CSSValueID::Hue>(args))
            return { };

        colorInterpolationMethod.hueInterpolationMethod = *hueInterpolationMethod;

        return CSS::ColorInterpolationMethod { .value = { colorInterpolationMethod, AlphaPremultiplication::Premultiplied } };
    };

    auto consumeRectangularColorSpace = [](CSSParserTokenRange& args, auto colorInterpolationMethod) -> std::optional<CSS::ColorInterpolationMethod> {
        // Consume the color space identifier.
        args.consumeIncludingWhitespace();

        return CSS::ColorInterpolationMethod { .value = { colorInterpolationMethod, AlphaPremultiplication::Premultiplied } };
    };

    switch (args.peek().id()) {
    case CSSValueID::Hsl:
        return consumePolarColorSpace(args, ColorInterpolationMethod::HSL { });
    case CSSValueID::Hwb:
        return consumePolarColorSpace(args, ColorInterpolationMethod::HWB { });
    case CSSValueID::Lch:
        return consumePolarColorSpace(args, ColorInterpolationMethod::LCH { });
    case CSSValueID::Lab:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::Lab { });
    case CSSValueID::Oklch:
        return consumePolarColorSpace(args, ColorInterpolationMethod::OKLCH { });
    case CSSValueID::Oklab:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::OKLab { });
    case CSSValueID::SRGB:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::SRGB { });
    case CSSValueID::SrgbLinear:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::SRGBLinear { });
    case CSSValueID::DisplayP3:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::DisplayP3 { });
    case CSSValueID::DisplayP3Linear:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::DisplayP3Linear { });
    case CSSValueID::A98Rgb:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::A98RGB { });
    case CSSValueID::ProphotoRgb:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::ProPhotoRGB { });
    case CSSValueID::Rec2020:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::Rec2020 { });
    case CSSValueID::XyzD50:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::XYZD50 { });
    case CSSValueID::Xyz:
    case CSSValueID::XyzD65:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::XYZD65 { });
    default:
        return { };
    }
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
