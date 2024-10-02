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
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorMix.h"
#include "StyleColor.h"
#include "StyleColorMix.h"

namespace WebCore {

bool isCalc(const CSSUnresolvedColorMix::Component::Percentage& percentage)
{
    return std::holds_alternative<CSS::UnevaluatedCalc<CSS::PercentageRaw>>(percentage.value);
}

bool is50Percent(const CSSUnresolvedColorMix::Component::Percentage& percentage)
{
    return WTF::switchOn(percentage.value,
        [](const CSS::PercentageRaw& raw) { return raw.value == 50.0; },
        [](const CSS::UnevaluatedCalc<CSS::PercentageRaw>&) { return false; }
    );
}

bool is50Percent(const StyleColorMix::Component::Percentage& percentage)
{
    return percentage.value == 50.0;
}

bool sumTo100Percent(const CSSUnresolvedColorMix::Component::Percentage& a, const CSSUnresolvedColorMix::Component::Percentage& b)
{
    auto visitor = WTF::makeVisitor(
        [](const CSS::PercentageRaw& a, const CSS::PercentageRaw& b) {
            return a.value + b.value == 100.0;
        },
        [](const CSS::PercentageRaw&, const CSS::UnevaluatedCalc<CSS::PercentageRaw>&) {
            return false;
        },
        [](const CSS::UnevaluatedCalc<CSS::PercentageRaw>&, const CSS::PercentageRaw&) {
            return false;
        },
        [](const CSS::UnevaluatedCalc<CSS::PercentageRaw>&, const CSS::UnevaluatedCalc<CSS::PercentageRaw>&) {
            return false;
        }
    );

    return std::visit(visitor, a.value, b.value);
}

bool sumTo100Percent(const StyleColorMix::Component::Percentage& a, const StyleColorMix::Component::Percentage& b)
{
    return a.value + b.value == 100.0;
}

std::optional<CSS::PercentageRaw> subtractFrom100Percent(const CSSUnresolvedColorMix::Component::Percentage& percentage)
{
    return WTF::switchOn(percentage.value,
        [&](const CSS::PercentageRaw& raw) -> std::optional<CSS::PercentageRaw> {
            return CSS::PercentageRaw { 100.0 - raw.value };
        },
        [&](const CSS::UnevaluatedCalc<CSS::PercentageRaw>&) -> std::optional<CSS::PercentageRaw> {
            return std::nullopt;
        }
    );
}

std::optional<CSS::PercentageRaw> subtractFrom100Percent(const StyleColorMix::Component::Percentage& percentage)
{
    return CSS::PercentageRaw { 100.0 - percentage.value };
}

void serializeColorMixColor(StringBuilder& builder, const CSSUnresolvedColorMix::Component& component)
{
    serializationForCSS(builder, component.color);
}

void serializeColorMixColor(StringBuilder& builder, const StyleColorMix::Component& component)
{
    serializationForCSS(builder, component.color);
}

void serializeColorMixPercentage(StringBuilder& builder, const CSSUnresolvedColorMix::Component::Percentage& percentage)
{
    serializationForCSS(builder, percentage);
}

void serializeColorMixPercentage(StringBuilder& builder, const StyleColorMix::Component::Percentage& percentage)
{
    serializationForCSS(builder, CSS::PercentageRaw { percentage.value });
}

} // namespace WebCore
