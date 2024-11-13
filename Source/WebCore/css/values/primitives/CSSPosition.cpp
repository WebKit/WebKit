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

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

bool isCenterPosition(const Position& position)
{
    auto isCenter = [](const auto& component) {
        return WTF::switchOn(component.offset,
            [](auto)   { return false; },
            [](Center) { return true;  },
            [](const LengthPercentage<>& value) {
                return WTF::switchOn(value.value,
                    [](const LengthPercentageRaw<>& raw) { return raw.type == CSSUnitType::CSS_PERCENTAGE && raw.value == 50.0; },
                    [](const UnevaluatedCalc<LengthPercentageRaw<>>&) { return false; }
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

// MARK: - TwoComponentPositionHorizontal

void Serialize<TwoComponentPositionHorizontal>::operator()(StringBuilder& builder, const TwoComponentPositionHorizontal& component)
{
    serializationForCSS(builder, component.offset);
}

void ComputedStyleDependenciesCollector<TwoComponentPositionHorizontal>::operator()(ComputedStyleDependencies& dependencies, const TwoComponentPositionHorizontal& component)
{
    collectComputedStyleDependencies(dependencies, component.offset);
}

IterationStatus CSSValueChildrenVisitor<TwoComponentPositionHorizontal>::operator()(const Function<IterationStatus(CSSValue&)>& func, const TwoComponentPositionHorizontal& component)
{
    return visitCSSValueChildren(func, component.offset);
}

// MARK: - TwoComponentPositionVertical

void Serialize<TwoComponentPositionVertical>::operator()(StringBuilder& builder, const TwoComponentPositionVertical& component)
{
    serializationForCSS(builder, component.offset);
}

void ComputedStyleDependenciesCollector<TwoComponentPositionVertical>::operator()(ComputedStyleDependencies& dependencies, const TwoComponentPositionVertical& component)
{
    collectComputedStyleDependencies(dependencies, component.offset);
}

IterationStatus CSSValueChildrenVisitor<TwoComponentPositionVertical>::operator()(const Function<IterationStatus(CSSValue&)>& func, const TwoComponentPositionVertical& component)
{
    return visitCSSValueChildren(func, component.offset);
}

// MARK: - Position

void Serialize<Position>::operator()(StringBuilder& builder, const Position& position)
{
    serializationForCSS(builder, position.value);
}

void ComputedStyleDependenciesCollector<Position>::operator()(ComputedStyleDependencies& dependencies, const Position& position)
{
    collectComputedStyleDependencies(dependencies, position.value);
}

IterationStatus CSSValueChildrenVisitor<Position>::operator()(const Function<IterationStatus(CSSValue&)>& func, const Position& position)
{
    return visitCSSValueChildren(func, position.value);
}

} // namespace CSS
} // namespace WebCore
