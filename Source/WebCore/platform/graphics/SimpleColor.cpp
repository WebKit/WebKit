/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SimpleColor.h"

#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline unsigned premultipliedChannel(unsigned c, unsigned a, bool ceiling = true)
{
    return fastDivideBy255(ceiling ? c * a + 254 : c * a);
}

static inline unsigned unpremultipliedChannel(unsigned c, unsigned a)
{
    return (fastMultiplyBy255(c) + a - 1) / a;
}

SimpleColor makePremultipliedSimpleColor(int r, int g, int b, int a, bool ceiling)
{
    return makeSimpleColor(premultipliedChannel(r, a, ceiling), premultipliedChannel(g, a, ceiling), premultipliedChannel(b, a, ceiling), a);
}

SimpleColor makePremultipliedSimpleColor(SimpleColor pixelColor)
{
    if (pixelColor.isOpaque())
        return pixelColor;
    return makePremultipliedSimpleColor(pixelColor.redComponent(), pixelColor.greenComponent(), pixelColor.blueComponent(), pixelColor.alphaComponent());
}

SimpleColor makeUnpremultipliedSimpleColor(int r, int g, int b, int a)
{
    return makeSimpleColor(unpremultipliedChannel(r, a), unpremultipliedChannel(g, a), unpremultipliedChannel(b, a), a);
}

SimpleColor makeUnpremultipliedSimpleColor(SimpleColor pixelColor)
{
    if (pixelColor.isVisible() && !pixelColor.isOpaque())
        return makeUnpremultipliedSimpleColor(pixelColor.redComponent(), pixelColor.greenComponent(), pixelColor.blueComponent(), pixelColor.alphaComponent());
    return pixelColor;
}

SimpleColor makeSimpleColorFromFloats(float r, float g, float b, float a)
{
    return makeSimpleColor(colorFloatToSimpleColorByte(r), colorFloatToSimpleColorByte(g), colorFloatToSimpleColorByte(b), colorFloatToSimpleColorByte(a));
}

SimpleColor makeSimpleColorFromHSLA(float hue, float saturation, float lightness, float alpha)
{
    const float scaleFactor = 255.0;
    FloatComponents floatResult = hslToSRGB({ hue, saturation, lightness, alpha });
    return makeSimpleColor(
        round(floatResult.components[0] * scaleFactor),
        round(floatResult.components[1] * scaleFactor),
        round(floatResult.components[2] * scaleFactor),
        round(floatResult.components[3] * scaleFactor));
}

SimpleColor makeSimpleColorFromCMYKA(float c, float m, float y, float k, float a)
{
    double colors = 1 - k;
    int r = static_cast<int>(nextafter(256, 0) * (colors * (1 - c)));
    int g = static_cast<int>(nextafter(256, 0) * (colors * (1 - m)));
    int b = static_cast<int>(nextafter(256, 0) * (colors * (1 - y)));
    return makeSimpleColor(r, g, b, static_cast<float>(nextafter(256, 0) * a));
}

String SimpleColor::serializationForHTML() const
{
    if (isOpaque())
        return makeString('#', hex(redComponent(), 2, Lowercase), hex(greenComponent(), 2, Lowercase), hex(blueComponent(), 2, Lowercase));
    return serializationForCSS();
}

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

String SimpleColor::serializationForCSS() const
{
    switch (alphaComponent()) {
    case 0:
        return makeString("rgba(", redComponent(), ", ", greenComponent(), ", ", blueComponent(), ", 0)");
    case 0xFF:
        return makeString("rgb(", redComponent(), ", ", greenComponent(), ", ", blueComponent(), ')');
    default:
        return makeString("rgba(", redComponent(), ", ", greenComponent(), ", ", blueComponent(), ", 0.", fractionDigitsForFractionalAlphaValue(alphaComponent()).data(), ')');
    }
}

String SimpleColor::serializationForRenderTreeAsText() const
{
    if (alphaComponent() < 0xFF)
        return makeString('#', hex(redComponent(), 2), hex(greenComponent(), 2), hex(blueComponent(), 2), hex(alphaComponent(), 2));
    return makeString('#', hex(redComponent(), 2), hex(greenComponent(), 2), hex(blueComponent(), 2));
}

} // namespace WebCore
