/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

// SRGBA<uint8_t> overloads

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

String serializationForCSS(SRGBA<uint8_t> color)
{
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

String serializationForHTML(SRGBA<uint8_t> color)
{
    auto [red, green, blue, alpha] = color;
    if (alpha == 0xFF)
        return makeString('#', hex(red, 2, Lowercase), hex(green, 2, Lowercase), hex(blue, 2, Lowercase));
    return serializationForCSS(color);
}

String serializationForRenderTreeAsText(SRGBA<uint8_t> color)
{
    auto [red, green, blue, alpha] = color;
    if (alpha < 0xFF)
        return makeString('#', hex(red, 2), hex(green, 2), hex(blue, 2), hex(alpha, 2));
    return makeString('#', hex(red, 2), hex(green, 2), hex(blue, 2));
}


// ExtendedColor overloads

static ASCIILiteral serialization(ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::SRGB:
        return "srgb"_s;
    case ColorSpace::LinearRGB:
        return "linear-srgb"_s;
    case ColorSpace::DisplayP3:
        return "display-p3"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

template<typename ColorType> static String serialization(const ColorType& color)
{
    auto [c1, c2, c3, alpha] = color;
    if (WTF::areEssentiallyEqual(alpha, 1.0f))
        return makeString("color(", serialization(color.colorSpace), ' ', c1, ' ', c2, ' ', c3, ')');
    return makeString("color(", serialization(color.colorSpace), ' ', c1, ' ', c2, ' ', c3, " / ", alpha, ')');
}

// SRGBA<float> overloads

String serializationForCSS(const SRGBA<float>& color)
{
    return serialization(color);
}

String serializationForHTML(const SRGBA<float>& color)
{
    return serialization(color);
}

String serializationForRenderTreeAsText(const SRGBA<float>& color)
{
    return serialization(color);
}

// LinearSRGBA<float> overloads

String serializationForCSS(const LinearSRGBA<float>& color)
{
    return serialization(color);
}

String serializationForHTML(const LinearSRGBA<float>& color)
{
    return serialization(color);
}

String serializationForRenderTreeAsText(const LinearSRGBA<float>& color)
{
    return serialization(color);
}

// DisplayP3<float> overloads

String serializationForCSS(const DisplayP3<float>& color)
{
    return serialization(color);
}

String serializationForHTML(const DisplayP3<float>& color)
{
    return serialization(color);
}

String serializationForRenderTreeAsText(const DisplayP3<float>& color)
{
    return serialization(color);
}


// Color overloads

String serializationForCSS(const Color& color)
{
    return color.callOnUnderlyingType([] (auto underlyingColor) {
        return serializationForCSS(underlyingColor);
    });
}

String serializationForHTML(const Color& color)
{
    return color.callOnUnderlyingType([] (auto underlyingColor) {
        return serializationForHTML(underlyingColor);
    });
}

String serializationForRenderTreeAsText(const Color& color)
{
    return color.callOnUnderlyingType([] (auto underlyingColor) {
        return serializationForRenderTreeAsText(underlyingColor);
    });
}

}
