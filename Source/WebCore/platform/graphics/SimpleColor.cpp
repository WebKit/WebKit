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

SimpleColor premultiplyFlooring(SimpleColor color)
{
    auto [r, g, b, a] = color;
    if (!a || a == 255)
        return color;
    return makeSimpleColor(fastDivideBy255(r * a), fastDivideBy255(g * a), fastDivideBy255(b * a), a);
}

SimpleColor premultiplyCeiling(SimpleColor color)
{
    auto [r, g, b, a] = color;
    if (!a || a == 255)
        return color;
    return makeSimpleColor(fastDivideBy255(r * a + 254), fastDivideBy255(g * a + 254), fastDivideBy255(b * a + 254), a);
}

static inline uint16_t unpremultiplyChannel(uint8_t c, uint8_t a)
{
    return (fastMultiplyBy255(c) + a - 1) / a;
}

SimpleColor unpremultiply(SimpleColor color)
{
    auto [r, g, b, a] = color;
    if (!a || a == 255)
        return color;
    return makeSimpleColor(unpremultiplyChannel(r, a), unpremultiplyChannel(g, a), unpremultiplyChannel(b, a), a);
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
