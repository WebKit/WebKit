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

void serializeColorMixColor(StringBuilder&, const CSSUnresolvedColorMix::Component&);
void serializeColorMixColor(StringBuilder&, const StyleColorMix::Component&);

void serializeColorMixPercentage(StringBuilder&, const CSSUnresolvedColorMix::Component::Percentage&);
void serializeColorMixPercentage(StringBuilder&, const StyleColorMix::Component::Percentage&);
void serializeColorMixPercentage(StringBuilder&, double);

double percentageDoubleValue(const CSSUnresolvedColorMix::Component::Percentage&);
double percentageDoubleValue(const StyleColorMix::Component::Percentage&);

// https://drafts.csswg.org/css-color-5/#serial-color-mix
template<typename ColorMixType>
void serializationForCSSColorMix(StringBuilder& builder, const ColorMixType& colorMix)
{
    builder.append("color-mix(in "_s);
    serializationForCSS(builder, colorMix.colorInterpolationMethod);
    builder.append(", "_s);
    serializeColorMixColor(builder, colorMix.mixComponents1);

    if ((colorMix.mixComponents1.percentage && percentageDoubleValue(*colorMix.mixComponents1.percentage) != 50.0) || (colorMix.mixComponents2.percentage && percentageDoubleValue(*colorMix.mixComponents2.percentage) != 50.0)) {
        builder.append(' ');

        if (!colorMix.mixComponents1.percentage && colorMix.mixComponents2.percentage)
            serializeColorMixPercentage(builder, 100.0 - percentageDoubleValue(*colorMix.mixComponents2.percentage));
        else
            serializeColorMixPercentage(builder, *colorMix.mixComponents1.percentage);
    }

    builder.append(", "_s);
    serializeColorMixColor(builder, colorMix.mixComponents2);

    bool percentagesNormalized = !colorMix.mixComponents1.percentage || !colorMix.mixComponents2.percentage || (percentageDoubleValue(*colorMix.mixComponents1.percentage) + percentageDoubleValue(*colorMix.mixComponents2.percentage) == 100.0);
    if (!percentagesNormalized) {
        builder.append(' ');
        serializeColorMixPercentage(builder, *colorMix.mixComponents2.percentage);
    }

    builder.append(')');
}

} // namespace WebCore
