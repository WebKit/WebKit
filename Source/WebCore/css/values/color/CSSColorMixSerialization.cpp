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

#include "config.h"
#include "CSSColorMixSerialization.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace CSS {

bool isCalc(const ColorMix::Component::Percentage& percentage)
{
    return percentage.isCalc();
}

bool is50Percent(const ColorMix::Component::Percentage& percentage)
{
    return WTF::switchOn(percentage,
        [](const ColorMix::Component::Percentage::Raw& raw) { return raw.value == 50.0; },
        [](const ColorMix::Component::Percentage::Calc&) { return false; }
    );
}

bool is50Percent(const Style::ColorMix::Component::Percentage& percentage)
{
    return percentage.value == 50.0;
}

bool sumTo100Percent(const ColorMix::Component::Percentage& a, const ColorMix::Component::Percentage& b)
{
    if (a.isCalc() || b.isCalc())
        return false;

    return a.raw()->value + b.raw()->value == 100.0;
}

bool sumTo100Percent(const Style::ColorMix::Component::Percentage& a, const Style::ColorMix::Component::Percentage& b)
{
    return a.value + b.value == 100.0;
}

std::optional<PercentageRaw<>> subtractFrom100Percent(const ColorMix::Component::Percentage& percentage)
{
    using Percentage = ColorMix::Component::Percentage;

    return WTF::switchOn(percentage,
        [&](const Percentage::Raw& raw) -> std::optional<PercentageRaw<>> {
            return PercentageRaw<> { 100.0 - raw.value };
        },
        [&](const Percentage::Calc&) -> std::optional<PercentageRaw<>> {
            return std::nullopt;
        }
    );
}

std::optional<PercentageRaw<>> subtractFrom100Percent(const Style::ColorMix::Component::Percentage& percentage)
{
    return PercentageRaw<> { 100.0 - percentage.value };
}

void serializeColorMixColor(StringBuilder& builder, const ColorMix::Component& component)
{
    serializationForCSS(builder, component.color);
}

void serializeColorMixColor(StringBuilder& builder, const Style::ColorMix::Component& component)
{
    serializationForCSS(builder, component.color);
}

void serializeColorMixPercentage(StringBuilder& builder, const ColorMix::Component::Percentage& percentage)
{
    serializationForCSS(builder, percentage);
}

void serializeColorMixPercentage(StringBuilder& builder, const Style::ColorMix::Component::Percentage& percentage)
{
    serializationForCSS(builder, PercentageRaw<> { percentage.value });
}

} // namespace CSS
} // namespace WebCore
