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
#include "StyleColor.h"

namespace WebCore {

static inline void serializeColorMixColor(StringBuilder& builder, const CSSUnresolvedColorMix::Component& component)
{
    builder.append(component.color->customCSSText());
}

static inline void serializeColorMixColor(StringBuilder& builder, const StyleColorMix::Component& component)
{
    serializationForCSS(builder, component.color);
}

static inline void serializeColorMixPercentage(StringBuilder& builder, const CSSUnresolvedColorMix::Component& component)
{
    builder.append(component.percentage->customCSSText());
}

static inline void serializeColorMixPercentage(StringBuilder& builder, const StyleColorMix::Component& component)
{
    builder.append(*component.percentage, '%');
}

static inline double percentageDoubleValue(const CSSUnresolvedColorMix::Component& component)
{
    return component.percentage->doubleValue();
}

static inline double percentageDoubleValue(const StyleColorMix::Component& component)
{
    return *component.percentage;
}

// https://drafts.csswg.org/css-color-5/#serial-color-mix
template<typename ColorMixType>
void serializationForCSSColorMix(StringBuilder& builder, const ColorMixType& colorMix)
{
    auto mixComponents1 = colorMix.mixComponents1;
    auto mixComponents2 = colorMix.mixComponents2;

    builder.append("color-mix(in "_s);
    serializationForCSS(builder, colorMix.colorInterpolationMethod);
    builder.append(", "_s);
    serializeColorMixColor(builder, mixComponents1);

    if ((mixComponents1.percentage && percentageDoubleValue(mixComponents1) != 50.0) || (mixComponents2.percentage && percentageDoubleValue(mixComponents2) != 50.0)) {
        if (!mixComponents1.percentage && mixComponents2.percentage)
            builder.append(' ', makeString(100.0 - percentageDoubleValue(mixComponents2)), '%');
        else {
            builder.append(' ');
            serializeColorMixPercentage(builder, mixComponents1);
        }
    }

    builder.append(", "_s);
    serializeColorMixColor(builder, mixComponents2);

    bool percentagesNormalized = !mixComponents1.percentage || !mixComponents2.percentage || (percentageDoubleValue(mixComponents1) + percentageDoubleValue(mixComponents2) == 100.0);
    if (!percentagesNormalized) {
        builder.append(' ');
        serializeColorMixPercentage(builder, mixComponents2);
    }

    builder.append(')');
}

} // namespace WebCore
