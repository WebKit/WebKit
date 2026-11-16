/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSFontPaletteMix.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace CSS {

void Serialize<FontPaletteMixParameters>::operator()(StringBuilder& builder, const SerializationContext& context, const FontPaletteMixParameters& value)
{
    if (value.colorInterpolationMethod != CSS::defaultInterpolationMethodForPaletteMix) {
        serializationForCSS(builder, context, value.colorInterpolationMethod);
        builder.append(", "_s);
    }

    bool anyComponentHasCalcPercentage = false;
    double specifiedSum = 0;
    size_t numberOfOmittedPercentages = 0;

    double valueToMatch = 100.0 / value.components.size();
    bool canOmitAllPercentages = true;

    for (auto& component : value.components) {
        if (component.percentage) {
            WTF::switchOn(*component.percentage,
                [&](const FontPaletteMixParameters::Component::Percentage::Raw& raw) {
                    if (raw.value != valueToMatch)
                        canOmitAllPercentages = false;
                    specifiedSum += raw.value;
                },
                [&](const FontPaletteMixParameters::Component::Percentage::Calc&) {
                    anyComponentHasCalcPercentage = true;
                    canOmitAllPercentages = false;
                }
            );
        } else
            ++numberOfOmittedPercentages;
    }

    double omittedPercentageValue = 0;
    if (numberOfOmittedPercentages > 0 && !anyComponentHasCalcPercentage) {
        omittedPercentageValue = (100.0 - specifiedSum) / numberOfOmittedPercentages;
        if (omittedPercentageValue != valueToMatch)
            canOmitAllPercentages = false;
    }

    builder.append(interleave(value.components, [&](auto& builder, auto& component) {
        serializationForCSS(builder, context, component.palette);
        if (!canOmitAllPercentages) {
            if (component.percentage) {
                builder.append(' ');
                serializationForCSS(builder, context, *component.percentage);
            } else if (!anyComponentHasCalcPercentage) {
                builder.append(' ');
                serializationForCSS(builder, context, FontPaletteMixParameters::Component::Percentage::Raw { omittedPercentageValue });
            }
        }
    }, ", "_s));
}

} // namespace CSS
} // namespace WebCore
