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
#include "ExtendedColor.h"
#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static String serializationForCSS(const A98RGB<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const A98RGB<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const A98RGB<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(const DisplayP3<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const DisplayP3<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const DisplayP3<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(const LCHA<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const LCHA<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const LCHA<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(const Lab<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const Lab<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const Lab<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(const LinearSRGBA<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const LinearSRGBA<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const LinearSRGBA<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(const ProPhotoRGB<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const ProPhotoRGB<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const ProPhotoRGB<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(const Rec2020<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const Rec2020<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const Rec2020<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(const SRGBA<float>&, bool useColorFunctionSerialization);
static String serializationForHTML(const SRGBA<float>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const SRGBA<float>&, bool useColorFunctionSerialization);

static String serializationForCSS(SRGBA<uint8_t>, bool useColorFunctionSerialization);
static String serializationForHTML(SRGBA<uint8_t>, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(SRGBA<uint8_t>, bool useColorFunctionSerialization);

static String serializationForCSS(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization);
static String serializationForHTML(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization);
static String serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D50>&, bool useColorFunctionSerialization);


String serializationForCSS(const Color& color)
{
    return color.callOnUnderlyingType([&] (auto underlyingColor) {
        return serializationForCSS(underlyingColor, color.usesColorFunctionSerialization());
    });
}

String serializationForHTML(const Color& color)
{
    return color.callOnUnderlyingType([&] (auto underlyingColor) {
        return serializationForHTML(underlyingColor, color.usesColorFunctionSerialization());
    });
}

String serializationForRenderTreeAsText(const Color& color)
{
    return color.callOnUnderlyingType([&] (auto underlyingColor) {
        return serializationForRenderTreeAsText(underlyingColor, color.usesColorFunctionSerialization());
    });
}

static ASCIILiteral serialization(ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
        return "a98-rgb"_s;
    case ColorSpace::DisplayP3:
        return "display-p3"_s;
    case ColorSpace::LCH:
        return "lch"_s;
    case ColorSpace::Lab:
        return "lab"_s;
    case ColorSpace::LinearSRGB:
        return "linear-srgb"_s;
    case ColorSpace::ProPhotoRGB:
        return "prophoto-rgb"_s;
    case ColorSpace::Rec2020:
        return "rec2020"_s;
    case ColorSpace::SRGB:
        return "srgb"_s;
    case ColorSpace::XYZ_D50:
        return "xyz"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

template<typename ColorType> static String serializationUsingColorFunction(const ColorType& color)
{
    static_assert(std::is_same_v<typename ColorType::ComponentType, float>);

    auto [c1, c2, c3, alpha] = color;
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return makeString("color(", serialization(ColorSpaceFor<ColorType>), ' ', c1, ' ', c2, ' ', c3, ')');
    return makeString("color(", serialization(ColorSpaceFor<ColorType>), ' ', c1, ' ', c2, ' ', c3, " / ", alpha, ')');
}

static String serializationUsingColorFunction(const Lab<float>& color)
{
    auto [c1, c2, c3, alpha] = color;
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return makeString("color(", serialization(ColorSpaceFor<Lab<float>>), ' ', c1, "% ", c2, ' ', c3, ')');
    return makeString("color(", serialization(ColorSpaceFor<Lab<float>>), ' ', c1, "% ", c2, ' ', c3, " / ", alpha, ')');
}

static String serializationUsingColorFunction(const SRGBA<uint8_t>& color)
{
    return serializationUsingColorFunction(convertColor<SRGBA<float>>(color));
}

// MARK: A98RGB<float> overloads

String serializationForCSS(const A98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForHTML(const A98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForRenderTreeAsText(const A98RGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: DisplayP3<float> overloads

String serializationForCSS(const DisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForHTML(const DisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForRenderTreeAsText(const DisplayP3<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: LCHA<float> overloads

String serializationForCSS(const LCHA<float>& color, bool useColorFunctionSerialization)
{
    // https://www.w3.org/TR/css-color-4/#serializing-lab-lch
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [c1, c2, c3, alpha] = color;
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return makeString("lch(", c1, "% ", c2, ' ', c3, ')');
    return makeString("lch(", c1, "% ", c2, ' ', c3, " / ", alpha, ')');
}

String serializationForHTML(const LCHA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

String serializationForRenderTreeAsText(const LCHA<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

// MARK: Lab<float> overloads

String serializationForCSS(const Lab<float>& color, bool useColorFunctionSerialization)
{
    // https://www.w3.org/TR/css-color-4/#serializing-lab-lch
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [c1, c2, c3, alpha] = color;
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return makeString("lab(", c1, "% ", c2, ' ', c3, ')');
    return makeString("lab(", c1, "% ", c2, ' ', c3, " / ", alpha, ')');
}

String serializationForHTML(const Lab<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

String serializationForRenderTreeAsText(const Lab<float>& color, bool useColorFunctionSerialization)
{
    return serializationForCSS(color, useColorFunctionSerialization);
}

// MARK: LinearSRGBA<float> overloads

String serializationForCSS(const LinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForHTML(const LinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForRenderTreeAsText(const LinearSRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: ProPhotoRGB<float> overloads

String serializationForCSS(const ProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForHTML(const ProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForRenderTreeAsText(const ProPhotoRGB<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: Rec2020<float> overloads

String serializationForCSS(const Rec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForHTML(const Rec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForRenderTreeAsText(const Rec2020<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: SRGBA<float> overloads

String serializationForCSS(const SRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForHTML(const SRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForRenderTreeAsText(const SRGBA<float>& color, bool)
{
    return serializationUsingColorFunction(color);
}

// MARK: SRGBA<uint8_t> overloads

static char decimalDigit(unsigned number)
{
    ASSERT(number < 10);
    return '0' + number;
}

static std::array<char, 4> fractionDigitsForFractionalAlphaValue(uint8_t alpha)
{
    ASSERT(alpha > 0);
    ASSERT(alpha < 0xFF);
    if (((alpha * 100 + 0x7F) / 0xFF * 0xFF + 50) / 100 != alpha)
        return { { decimalDigit(alpha * 10 / 0xFF % 10), decimalDigit(alpha * 100 / 0xFF % 10), decimalDigit((alpha * 1000 + 0x7F) / 0xFF % 10), '\0' } };
    if (int thirdDigit = (alpha * 100 + 0x7F) / 0xFF % 10)
        return { { decimalDigit(alpha * 10 / 0xFF), decimalDigit(thirdDigit), '\0', '\0' } };
    return { { decimalDigit((alpha * 10 + 0x7F) / 0xFF), '\0', '\0', '\0' } };
}

String serializationForCSS(SRGBA<uint8_t> color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [red, green, blue, alpha] = color;
    switch (alpha) {
    case 0:
        return makeString("rgba(", red, ", ", green, ", ", blue, ", 0)");
    case 0xFF:
        return makeString("rgb(", red, ", ", green, ", ", blue, ')');
    default:
        return makeString("rgba(", red, ", ", green, ", ", blue, ", 0.", fractionDigitsForFractionalAlphaValue(alpha).data(), ')');
    }
}

String serializationForHTML(SRGBA<uint8_t> color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [red, green, blue, alpha] = color;
    if (alpha == 0xFF)
        return makeString('#', hex(red, 2, Lowercase), hex(green, 2, Lowercase), hex(blue, 2, Lowercase));
    return serializationForCSS(color);
}

String serializationForRenderTreeAsText(SRGBA<uint8_t> color, bool useColorFunctionSerialization)
{
    if (useColorFunctionSerialization)
        return serializationUsingColorFunction(color);

    auto [red, green, blue, alpha] = color;
    if (alpha < 0xFF)
        return makeString('#', hex(red, 2), hex(green, 2), hex(blue, 2), hex(alpha, 2));
    return makeString('#', hex(red, 2), hex(green, 2), hex(blue, 2));
}

// MARK: XYZA<float, WhitePoint::D50> overloads

String serializationForCSS(const XYZA<float, WhitePoint::D50>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForHTML(const XYZA<float, WhitePoint::D50>& color, bool)
{
    return serializationUsingColorFunction(color);
}

String serializationForRenderTreeAsText(const XYZA<float, WhitePoint::D50>& color, bool)
{
    return serializationUsingColorFunction(color);
}

}
