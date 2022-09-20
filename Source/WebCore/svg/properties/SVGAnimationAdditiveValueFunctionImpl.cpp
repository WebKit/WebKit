/*
 * Copyright (C) 2019-2021 Apple Inc.  All rights reserved.
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
#include "SVGAnimationAdditiveValueFunctionImpl.h"

#include "RenderElement.h"
#include "SVGElement.h"
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

Color SVGAnimationColorFunction::colorFromString(SVGElement& targetElement, const String& string)
{
    static MainThreadNeverDestroyed<const AtomString> currentColor("currentColor"_s);

    if (string != currentColor.get())
        return SVGPropertyTraits<Color>::fromString(string);

    if (auto* renderer = targetElement.renderer())
        return renderer->style().visitedDependentColor(CSSPropertyColor);

    return { };
}

std::optional<float> SVGAnimationColorFunction::calculateDistance(SVGElement&, const String& from, const String& to) const
{
    Color fromColor = CSSParser::parseColorWithoutContext(from.stripWhiteSpace());
    if (RenderStyle::isCurrentColor(fromColor))
        return { };

    Color toColor = CSSParser::parseColorWithoutContext(to.stripWhiteSpace());
    if (RenderStyle::isCurrentColor(toColor))
        return { };

    auto simpleFrom = fromColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    auto simpleTo = toColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();

    float red = simpleFrom.red - simpleTo.red;
    float green = simpleFrom.green - simpleTo.green;
    float blue = simpleFrom.blue - simpleTo.blue;

    return std::hypot(red, green, blue);
}

std::optional<float> SVGAnimationIntegerFunction::calculateDistance(SVGElement&, const String& from, const String& to) const
{
    return std::abs(parseInteger<int>(to).value_or(0) - parseInteger<int>(from).value_or(0));
}

} // namespace WebCore
