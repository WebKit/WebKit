/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ColorSerialization.h"

#include "Color.h"
#include "ColorNormalization.h"
#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static RefPtr<StringImpl> numericComponent(float value)
{
    if (std::isnan(value))
        return "none"_si;
    if (std::isfinite(value))
        return makeString(FormattedCSSNumber::create(value));
    return makeString(
        "calc("_s,
        FormattedCSSNumber::create(value),
        ")"_s
    );
}

static RefPtr<StringImpl> percentageComponent(float value)
{
    if (std::isnan(value))
        return "none"_si;
    if (std::isfinite(value))
        return makeString(FormattedCSSNumber::create(value), '%');
    return makeString(
        "calc("_s,
        FormattedCSSNumber::create(value),
        ")"_s
    );
}

template<auto index, typename ColorType> static RefPtr<StringImpl> normalizedNumericComponent(const ColorType& color)
{
    if constexpr (ColorType::Model::componentInfo[index].type == ColorComponentType::Angle)
        return numericComponent(normalizeHue(get<index>(color.unresolved())));
    else if constexpr (ColorType::Model::componentInfo[index].type == ColorComponentType::Percentage)
        return percentageComponent(get<index>(color.unresolved()));
    else
        return numericComponent(get<index>(color.unresolved()));
}

static RefPtr<StringImpl> normalizedAlphaNumericComponent(const auto& color)
{
    return numericComponent(color.unresolved().alpha);
}

static RefPtr<StringImpl> serializationForCSS(const A98RGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const A98RGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const A98RGB<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const DisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const DisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const DisplayP3<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ExtendedA98RGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ExtendedA98RGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedA98RGB<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ExtendedDisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ExtendedDisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedDisplayP3<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ExtendedLinearDisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ExtendedLinearDisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedLinearDisplayP3<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ExtendedLinearSRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ExtendedLinearSRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedLinearSRGBA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ExtendedProPhotoRGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ExtendedProPhotoRGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedProPhotoRGB<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ExtendedRec2020<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ExtendedRec2020<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedRec2020<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ExtendedSRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ExtendedSRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedSRGBA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const HSLA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const HSLA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const HSLA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const HWBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const HWBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const HWBA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const LCHA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const LCHA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const LCHA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const Lab<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const Lab<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const Lab<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const LinearDisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const LinearDisplayP3<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const LinearDisplayP3<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const LinearSRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const LinearSRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const LinearSRGBA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const OKLCHA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const OKLCHA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const OKLCHA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const OKLab<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const OKLab<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const OKLab<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const ProPhotoRGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const ProPhotoRGB<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const ProPhotoRGB<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const Rec2020<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const Rec2020<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const Rec2020<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const SRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const SRGBA<float>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const SRGBA<float>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(SRGBA<uint8_t>, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(SRGBA<uint8_t>, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(SRGBA<uint8_t>, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization);

static RefPtr<StringImpl> serializationForCSS(const XYZA<float, WhitePoint::D65>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForHTML(const XYZA<float, WhitePoint::D65>&, bool useColorFunctionSerialization);
static RefPtr<StringImpl> serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D65>&, bool useColorFunctionSerialization);

RefPtr<StringImpl> serializationForCSS(const Color& color)
{
    return color.callOnUnderlyingType([&] (auto underlyingColor) {
        return serializationForCSS(underlyingColor, color.usesColorFunctionSerialization());
    });
}

RefPtr<StringImpl> serializationForHTML(const Color& color)
{
    return color.callOnUnderlyingType([&] (auto underlyingColor) {
        return serializationForHTML(underlyingColor, color.usesColorFunctionSerialization());
    });
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const Color& color)
{
    return color.callOnUnderlyingType([&] (auto underlyingColor) {
        return serializationForRenderTreeAsText(underlyingColor, color.usesColorFunctionSerialization());
    });
}

ASCIILiteral serialization(ColorSpaceName colorSpace)
{
    switch (colorSpace) {
    case ColorSpaceName::A98RGB:
    case ColorSpaceName::ExtendedA98RGB:
        return "a98-rgb"_s;
    case ColorSpaceName::DisplayP3:
    case ColorSpaceName::ExtendedDisplayP3:
        return "display-p3"_s;
    case ColorSpaceName::HSL:
        return "hsl"_s;
    case ColorSpaceName::HWB:
        return "hwb"_s;
    case ColorSpaceName::LCH:
        return "lch"_s;
    case ColorSpaceName::Lab:
        return "lab"_s;
    case ColorSpaceName::LinearDisplayP3:
    case ColorSpaceName::ExtendedLinearDisplayP3:
        return "display-p3-linear"_s;
    case ColorSpaceName::LinearSRGB:
    case ColorSpaceName::ExtendedLinearSRGB:
        return "srgb-linear"_s;
    case ColorSpaceName::OKLCH:
        return "oklch"_s;
    case ColorSpaceName::OKLab:
        return "oklab"_s;
    case ColorSpaceName::ProPhotoRGB:
    case ColorSpaceName::ExtendedProPhotoRGB:
        return "prophoto-rgb"_s;
    case ColorSpaceName::Rec2020:
    case ColorSpaceName::ExtendedRec2020:
        return "rec2020"_s;
    case ColorSpaceName::SRGB:
    case ColorSpaceName::ExtendedSRGB:
        return "srgb"_s;
    case ColorSpaceName::XYZ_D50:
        return "xyz-d50"_s;
    case ColorSpaceName::XYZ_D65:
        return "xyz-d65"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

static RefPtr<StringImpl> serializationUsingColorFunction(IsColorTypeWithComponentType<float> auto const& color)
{
    if (WTF::areEssentiallyEqual(color.unresolved().alpha, 1.0f))
        return makeString("color("_s, serialization(colorSpaceFor(color)), ' ', normalizedNumericComponent<0>(color), ' ', normalizedNumericComponent<1>(color), ' ', normalizedNumericComponent<2>(color), ')');
    return makeString("color("_s, serialization(colorSpaceFor(color)), ' ', normalizedNumericComponent<0>(color), ' ', normalizedNumericComponent<1>(color), ' ', normalizedNumericComponent<2>(color), " / "_s, normalizedAlphaNumericComponent(color), ')');
}

static RefPtr<StringImpl> serializationUsingColorFunction(const SRGBA<uint8_t>& color)
{
    return serializationUsingColorFunction(convertColor<SRGBA<float>>(color));
}

static RefPtr<StringImpl> serializationUsingNamedFunction(IsColorTypeWithComponentType<float> auto const& color)
{
    if (WTF::areEssentiallyEqual(color.unresolved().alpha, 1.0f))
        return makeString(serialization(colorSpaceFor(color)), '(', normalizedNumericComponent<0>(color), ' ', normalizedNumericComponent<1>(color), ' ', normalizedNumericComponent<2>(color), ')');
    return makeString(serialization(colorSpaceFor(color)), '(', normalizedNumericComponent<0>(color), ' ', normalizedNumericComponent<1>(color), ' ', normalizedNumericComponent<2>(color), " / "_s, normalizedAlphaNumericComponent(color), ')');
}

// MARK: A98RGB<float> overloads

RefPtr<StringImpl> serializationForCSS(const A98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const A98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const A98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: DisplayP3<float> overloads

RefPtr<StringImpl> serializationForCSS(const DisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const DisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const DisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ExtendedA98RGB<float> overloads

RefPtr<StringImpl> serializationForCSS(const ExtendedA98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ExtendedA98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedA98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ExtendedDisplayP3<float> overloads

RefPtr<StringImpl> serializationForCSS(const ExtendedDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ExtendedDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ExtendedLinearDisplayP3<float> overloads

RefPtr<StringImpl> serializationForCSS(const ExtendedLinearDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ExtendedLinearDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedLinearDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ExtendedLinearSRGBA<float> overloads

RefPtr<StringImpl> serializationForCSS(const ExtendedLinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ExtendedLinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedLinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ExtendedProPhotoRGB<float> overloads

RefPtr<StringImpl> serializationForCSS(const ExtendedProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ExtendedProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ExtendedRec2020<float> overloads

RefPtr<StringImpl> serializationForCSS(const ExtendedRec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ExtendedRec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedRec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ExtendedSRGBA<float> overloads

RefPtr<StringImpl> serializationForCSS(const ExtendedSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ExtendedSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ExtendedSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: HSLA<float> overloads

RefPtr<StringImpl> serializationForCSS(const HSLA<float>& color, bool useColorFunctionSerialization)
{
    if (color.unresolved().anyComponentIsNone())
        return serializationUsingNamedFunction(color);

    if (useColorFunctionSerialization)
        return serializationForCSS(convertColorCarryingForwardMissing<ExtendedSRGBA<float>>(color), true);

    return serializationForCSS(convertColor<SRGBA<uint8_t>>(color), false);
}

RefPtr<StringImpl> serializationForHTML(const HSLA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForHTML(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const HSLA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForRenderTreeAsText(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization);
}

// MARK: HWBA<float> overloads

RefPtr<StringImpl> serializationForCSS(const HWBA<float>& color, bool useColorFunctionSerialization)
{
    if (color.unresolved().anyComponentIsNone())
        return serializationUsingNamedFunction(color);

    if (useColorFunctionSerialization)
        return serializationForCSS(convertColorCarryingForwardMissing<ExtendedSRGBA<float>>(color), true);

    return serializationForCSS(convertColor<SRGBA<uint8_t>>(color), false);
}

RefPtr<StringImpl> serializationForHTML(const HWBA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForHTML(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const HWBA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForRenderTreeAsText(convertColor<SRGBA<uint8_t>>(color), useColorFunctionSerialization);
}

// MARK: LCHA<float> overloads

RefPtr<StringImpl> serializationForCSS(const LCHA<float>& color, bool)
{
    return serializationUsingNamedFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const LCHA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const LCHA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

// MARK: Lab<float> overloads

RefPtr<StringImpl> serializationForCSS(const Lab<float>& color, bool)
{
    return serializationUsingNamedFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const Lab<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const Lab<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

// MARK: LinearDisplayP3<float> overloads

RefPtr<StringImpl> serializationForCSS(const LinearDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const LinearDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const LinearDisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: LinearSRGBA<float> overloads

RefPtr<StringImpl> serializationForCSS(const LinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const LinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const LinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: OKLCHA<float> overloads

RefPtr<StringImpl> serializationForCSS(const OKLCHA<float>& color, bool)
{
    return serializationUsingNamedFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const OKLCHA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const OKLCHA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

// MARK: OKLab<float> overloads

RefPtr<StringImpl> serializationForCSS(const OKLab<float>& color, bool)
{
    return serializationUsingNamedFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const OKLab<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const OKLab<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

// MARK: ProPhotoRGB<float> overloads

RefPtr<StringImpl> serializationForCSS(const ProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const ProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const ProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: Rec2020<float> overloads

RefPtr<StringImpl> serializationForCSS(const Rec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const Rec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const Rec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: SRGBA<float> overloads

RefPtr<StringImpl> serializationForCSS(const SRGBA<float>& color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization || color.unresolved().anyComponentIsNone())
        return serializationUsingColorFunction(color);

    return serializationForCSS(convertColor<SRGBA<uint8_t>>(color), false);
}

RefPtr<StringImpl> serializationForHTML(const SRGBA<float>& color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    return serializationForHTML(convertColor<SRGBA<uint8_t>>(color), false);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const SRGBA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

// MARK: SRGBA<uint8_t> overloads

NODELETE static char decimalDigit(unsigned number)
{
    ASSERT(number < 10);
    return '0' + number;
}

NODELETE static std::array<char, 4> fractionDigitsForFractionalAlphaValue(uint8_t alpha)
{
    ASSERT(alpha > 0);
    ASSERT(alpha < 0xFF);
    if (((alpha * 100 + 0x7F) / 0xFF * 0xFF + 50) / 100 != alpha)
        return { { decimalDigit(alpha * 10 / 0xFF % 10), decimalDigit(alpha * 100 / 0xFF % 10), decimalDigit((alpha * 1000 + 0x7F) / 0xFF % 10), '\0' } };
    if (int thirdDigit = (alpha * 100 + 0x7F) / 0xFF % 10)
        return { { decimalDigit(alpha * 10 / 0xFF), decimalDigit(thirdDigit), '\0', '\0' } };
    return { { decimalDigit((alpha * 10 + 0x7F) / 0xFF), '\0', '\0', '\0' } };
}

RefPtr<StringImpl> serializationForCSS(SRGBA<uint8_t> color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [red, green, blue, alpha] = color.resolved();
    switch (alpha) {
    case 0:
        return makeString("rgba("_s, red, ", "_s, green, ", "_s, blue, ", 0)"_s);
    case 0xFF:
        return makeString("rgb("_s, red, ", "_s, green, ", "_s, blue, ')');
    default:
        return makeString("rgba("_s, red, ", "_s, green, ", "_s, blue, ", 0."_s, unsafeSpan(fractionDigitsForFractionalAlphaValue(alpha).data()), ')');
    }
}

RefPtr<StringImpl> serializationForHTML(SRGBA<uint8_t> color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [red, green, blue, alpha] = color.resolved();
    if (alpha == 0xFF)
        return makeString('#', hex(red, 2, Lowercase), hex(green, 2, Lowercase), hex(blue, 2, Lowercase));
    return serializationForCSS(Color { color } );
}

RefPtr<StringImpl> serializationForRenderTreeAsText(SRGBA<uint8_t> color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [red, green, blue, alpha] = color.resolved();
    if (alpha < 0xFF)
        return makeString('#', hex(red, 2), hex(green, 2), hex(blue, 2), hex(alpha, 2));
    return makeString('#', hex(red, 2), hex(green, 2), hex(blue, 2));
}

// MARK: XYZA<float, WhitePoint::D50> overloads

RefPtr<StringImpl> serializationForCSS(const XYZA<float, WhitePoint::D50>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const XYZA<float, WhitePoint::D50>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D50>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: XYZA<float, WhitePoint::D65> overloads

RefPtr<StringImpl> serializationForCSS(const XYZA<float, WhitePoint::D65>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForHTML(const XYZA<float, WhitePoint::D65>& color, bool)
{
    return serializationUsingColorFunction(color);
}

RefPtr<StringImpl> serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D65>& color, bool)
{
    return serializationUsingColorFunction(color);
}

}
