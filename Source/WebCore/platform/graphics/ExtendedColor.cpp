/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ExtendedColor.h"

#include "Color.h"
#include "ColorTypes.h"
#include "ColorUtilities.h"
#include <wtf/Hasher.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

Ref<ExtendedColor> ExtendedColor::create(float c1, float c2, float c3, float alpha, ColorSpace colorSpace)
{
    return adoptRef(*new ExtendedColor(c1, c2, c3, alpha, colorSpace));
}

unsigned ExtendedColor::hash() const
{
    auto [c1, c2, c3, alpha] = components();
    return computeHash(c1, c2, c3, alpha, m_colorSpace);
}

String ExtendedColor::cssText() const
{
    const char* colorSpace;
    switch (m_colorSpace) {
    case ColorSpace::SRGB:
        colorSpace = "srgb";
        break;
    case ColorSpace::DisplayP3:
        colorSpace = "display-p3";
        break;
    default:
        ASSERT_NOT_REACHED();
        return WTF::emptyString();
    }

    auto [c1, c2, c3, existingAlpha] = components();

    if (WTF::areEssentiallyEqual(alpha(), 1.0f))
        return makeString("color(", colorSpace, ' ', c1, ' ', c2, ' ', c3, ')');

    return makeString("color(", colorSpace, ' ', c1, ' ', c2, ' ', c3, " / ", existingAlpha, ')');
}

Ref<ExtendedColor> ExtendedColor::colorWithAlpha(float overrideAlpha) const
{
    auto [c1, c2, c3, existingAlpha] = components();
    return ExtendedColor::create(c1, c2, c3, overrideAlpha, colorSpace());
}

Ref<ExtendedColor> ExtendedColor::invertedColorWithAlpha(float overrideAlpha) const
{
    auto [c1, c2, c3, existingAlpha] = components();
    return ExtendedColor::create(1.0f - c1, 1.0f - c2, 1.0f - c3, overrideAlpha, colorSpace());
}

SRGBA<float> ExtendedColor::toSRGBALossy() const
{
    switch (m_colorSpace) {
    case ColorSpace::SRGB:
        return asSRGBA(m_components);
    case ColorSpace::LinearRGB:
        return toSRGBA(asLinearSRGBA(m_components));
    case ColorSpace::DisplayP3:
        return toSRGBA(asDisplayP3(m_components));
    }
    ASSERT_NOT_REACHED();
    return { 0, 0, 0, 0 };
}

bool ExtendedColor::isWhite() const
{
    auto [c1, c2, c3, alpha] = components();
    return c1 == 1 && c2 == 1 && c3 == 1 && alpha == 1;
}

bool ExtendedColor::isBlack() const
{
    auto [c1, c2, c3, alpha] = components();
    return !c1 && !c2 && !c3 && alpha == 1;
}

Color makeExtendedColor(float c1, float c2, float c3, float alpha, ColorSpace colorSpace)
{
    return ExtendedColor::create(c1, c2, c3, alpha, colorSpace);
}

}
