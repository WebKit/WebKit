/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "CSSColorMix.h"
#include "StyleColorMix.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

bool isCalc(const ColorMix::Component::Percentage&);
constexpr bool isCalc(const Style::ColorMix::Component::Percentage&) { return false; }

bool is50Percent(const ColorMix::Component::Percentage&);
bool is50Percent(const Style::ColorMix::Component::Percentage&);

bool sumTo100Percent(const ColorMix::Component::Percentage&, const ColorMix::Component::Percentage&);
bool sumTo100Percent(const Style::ColorMix::Component::Percentage&, const Style::ColorMix::Component::Percentage&);

std::optional<PercentageRaw<>> subtractFrom100Percent(const ColorMix::Component::Percentage&);
std::optional<PercentageRaw<>> subtractFrom100Percent(const Style::ColorMix::Component::Percentage&);

void serializeColorMixColor(StringBuilder&, const CSS::SerializationContext&, const ColorMix::Component&);
void serializeColorMixColor(StringBuilder&, const CSS::SerializationContext&, const Style::ColorMix::Component&);

void serializeColorMixPercentage(StringBuilder&, const CSS::SerializationContext&, const ColorMix::Component::Percentage&);
void serializeColorMixPercentage(StringBuilder&, const CSS::SerializationContext&, const Style::ColorMix::Component::Percentage&);

template<typename ColorMixType>
void serializationForColorMixPercentage1(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMixType& colorMix)
{
    if (colorMix.mixComponents1.percentage && colorMix.mixComponents2.percentage) {
        if (is50Percent(*colorMix.mixComponents1.percentage) && is50Percent(*colorMix.mixComponents2.percentage))
            return;
        builder.append(' ');
        serializeColorMixPercentage(builder, context, *colorMix.mixComponents1.percentage);
    } else if (colorMix.mixComponents1.percentage) {
        if (is50Percent(*colorMix.mixComponents1.percentage))
            return;
        builder.append(' ');
        serializeColorMixPercentage(builder, context, *colorMix.mixComponents1.percentage);
    } else if (colorMix.mixComponents2.percentage) {
        if (is50Percent(*colorMix.mixComponents2.percentage))
            return;

        auto subtractedPercent = subtractFrom100Percent(*colorMix.mixComponents2.percentage);
        if (!subtractedPercent)
            return;

        builder.append(' ');
        serializationForCSS(builder, context, *subtractedPercent);
    }
}

template<typename ColorMixType>
void serializationForColorMixPercentage2(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMixType& colorMix)
{
    if (colorMix.mixComponents1.percentage && colorMix.mixComponents2.percentage) {
        if (sumTo100Percent(*colorMix.mixComponents1.percentage, *colorMix.mixComponents2.percentage))
            return;

        builder.append(' ');
        serializeColorMixPercentage(builder, context, *colorMix.mixComponents2.percentage);
    } else if (colorMix.mixComponents2.percentage) {
        if (is50Percent(*colorMix.mixComponents2.percentage))
            return;
        if (!isCalc(*colorMix.mixComponents2.percentage))
            return;

        builder.append(' ');
        serializeColorMixPercentage(builder, context, *colorMix.mixComponents2.percentage);
    }
}

// https://drafts.csswg.org/css-color-5/#serial-color-mix
template<typename ColorMixType>
void serializationForCSSColorMix(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMixType& colorMix)
{
    builder.append("color-mix(in "_s);
    serializationForCSS(builder, colorMix.colorInterpolationMethod);
    builder.append(", "_s);
    serializeColorMixColor(builder, context, colorMix.mixComponents1);
    serializationForColorMixPercentage1(builder, context, colorMix);
    builder.append(", "_s);
    serializeColorMixColor(builder, context, colorMix.mixComponents2);
    serializationForColorMixPercentage2(builder, context, colorMix);
    builder.append(')');
}

} // namespace CSS
} // namespace WebCore
