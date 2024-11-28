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

#include "CSSUnresolvedColorMix.h"
#include "StyleColorMix.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool isCalc(const CSSUnresolvedColorMix::Component::Percentage&);
constexpr bool isCalc(const StyleColorMix::Component::Percentage&) { return false; }

bool is50Percent(const CSSUnresolvedColorMix::Component::Percentage&);
bool is50Percent(const StyleColorMix::Component::Percentage&);

bool sumTo100Percent(const CSSUnresolvedColorMix::Component::Percentage&, const CSSUnresolvedColorMix::Component::Percentage&);
bool sumTo100Percent(const StyleColorMix::Component::Percentage&, const StyleColorMix::Component::Percentage&);

std::optional<CSS::PercentageRaw<>> subtractFrom100Percent(const CSSUnresolvedColorMix::Component::Percentage&);
std::optional<CSS::PercentageRaw<>> subtractFrom100Percent(const StyleColorMix::Component::Percentage&);

void serializeColorMixColor(StringBuilder&, const CSSUnresolvedColorMix::Component&);
void serializeColorMixColor(StringBuilder&, const StyleColorMix::Component&);

void serializeColorMixPercentage(StringBuilder&, const CSSUnresolvedColorMix::Component::Percentage&);
void serializeColorMixPercentage(StringBuilder&, const StyleColorMix::Component::Percentage&);

template<typename ColorMixType>
void serializationForCSSColorMixPercentage1(StringBuilder& builder, const ColorMixType& colorMix)
{
    if (colorMix.mixComponents1.percentage && colorMix.mixComponents2.percentage) {
        if (is50Percent(*colorMix.mixComponents1.percentage) && is50Percent(*colorMix.mixComponents2.percentage))
            return;
        builder.append(' ');
        serializeColorMixPercentage(builder, *colorMix.mixComponents1.percentage);
    } else if (colorMix.mixComponents1.percentage) {
        if (is50Percent(*colorMix.mixComponents1.percentage))
            return;
        builder.append(' ');
        serializeColorMixPercentage(builder, *colorMix.mixComponents1.percentage);
    } else if (colorMix.mixComponents2.percentage) {
        if (is50Percent(*colorMix.mixComponents2.percentage))
            return;

        auto subtractedPercent = subtractFrom100Percent(*colorMix.mixComponents2.percentage);
        if (!subtractedPercent)
            return;

        builder.append(' ');
        serializationForCSS(builder, *subtractedPercent);
    }
}

template<typename ColorMixType>
void serializationForCSSColorMixPercentage2(StringBuilder& builder, const ColorMixType& colorMix)
{
    if (colorMix.mixComponents1.percentage && colorMix.mixComponents2.percentage) {
        if (sumTo100Percent(*colorMix.mixComponents1.percentage, *colorMix.mixComponents2.percentage))
            return;

        builder.append(' ');
        serializeColorMixPercentage(builder, *colorMix.mixComponents2.percentage);
    } else if (colorMix.mixComponents2.percentage) {
        if (is50Percent(*colorMix.mixComponents2.percentage))
            return;
        if (!isCalc(*colorMix.mixComponents2.percentage))
            return;

        builder.append(' ');
        serializeColorMixPercentage(builder, *colorMix.mixComponents2.percentage);
    }
}

// https://drafts.csswg.org/css-color-5/#serial-color-mix
template<typename ColorMixType>
void serializationForCSSColorMix(StringBuilder& builder, const ColorMixType& colorMix)
{
    builder.append("color-mix(in "_s);
    serializationForCSS(builder, colorMix.colorInterpolationMethod);
    builder.append(", "_s);
    serializeColorMixColor(builder, colorMix.mixComponents1);
    serializationForCSSColorMixPercentage1(builder, colorMix);
    builder.append(", "_s);
    serializeColorMixColor(builder, colorMix.mixComponents2);
    serializationForCSSColorMixPercentage2(builder, colorMix);
    builder.append(')');
}

} // namespace WebCore
