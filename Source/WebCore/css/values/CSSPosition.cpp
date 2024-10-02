/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPosition.h"

#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

bool isCenterPosition(const Position& position)
{
    auto isCenter = [](const auto& component) {
        return WTF::switchOn(component,
            [](auto)   { return false; },
            [](Center) { return true;  },
            [](const LengthPercentage& value) {
                return WTF::switchOn(value.value,
                    [](const LengthPercentageRaw& raw) { return raw.type == CSSUnitType::CSS_PERCENTAGE && raw.value == 50.0; },
                    [](const UnevaluatedCalc<LengthPercentageRaw>&) { return false; }
                );
            }
        );
    };

    return WTF::switchOn(position,
        [&](const TwoComponentPosition& twoComponent) {
            return isCenter(get<0>(twoComponent)) && isCenter(get<1>(twoComponent));
        },
        [&](const FourComponentPosition&) {
            return false;
        }
    );
}

void serializationForCSS(StringBuilder& builder, const Left&)
{
    builder.append(nameLiteralForSerialization(CSSValueLeft));
}

void serializationForCSS(StringBuilder& builder, const Right&)
{
    builder.append(nameLiteralForSerialization(CSSValueRight));
}

void serializationForCSS(StringBuilder& builder, const Top&)
{
    builder.append(nameLiteralForSerialization(CSSValueTop));
}

void serializationForCSS(StringBuilder& builder, const Bottom&)
{
    builder.append(nameLiteralForSerialization(CSSValueBottom));
}

void serializationForCSS(StringBuilder& builder, const Center&)
{
    builder.append(nameLiteralForSerialization(CSSValueCenter));
}

void serializationForCSS(StringBuilder& builder, const Position& position)
{
    serializationForCSS(builder, position.value);
}

void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const Position& position)
{
    collectComputedStyleDependencies(dependencies, position.value);
}

IterationStatus visitCSSValueChildren(const Position& position, const Function<IterationStatus(CSSValue&)>& func)
{
    return visitCSSValueChildren(position.value, func);
}

} // namespace CSS
} // namespace WebCore
