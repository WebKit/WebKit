/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSColorMix.h"

#include "CSSColorMixResolver.h"
#include "CSSPlatformColorResolutionState.h"
#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "ColorSerialization.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace CSS {

using namespace CSS::Literals;

WebCore::Color createColor(const ColorMix& unresolved, PlatformColorResolutionState& state)
{
    PlatformColorResolutionStateNester nester { state };

    bool hasInvalidValue = false;
    auto components = unresolved.components.map([&](const auto& component) -> ColorMixResolver::Component {
        auto color = createColor(component.color, state);
        if (!color.isValid()) {
            hasInvalidValue = true;
            return ColorMixResolver::Component { WebCore::Color(), std::nullopt };
        }

        std::optional<Style::Percentage<Range{0, 100}>> percentage;
        if (requiresConversionData(component.percentage)) {
            if (!state.conversionData) {
                hasInvalidValue = true;
                return ColorMixResolver::Component { WebCore::Color(), std::nullopt };
            }
            percentage = Style::toStyle(component.percentage, *state.conversionData);
        } else
            percentage = Style::toStyleNoConversionDataRequired(component.percentage);

        return ColorMixResolver::Component { WTF::move(color), WTF::move(percentage) };
    });
    if (hasInvalidValue)
        return { };

    return mix(
        ColorMixResolver {
            unresolved.colorInterpolationMethod,
            WTF::move(components),
        }
    );
}

bool containsCurrentColor(const ColorMix& unresolved)
{
    return std::ranges::any_of(unresolved.components, [](const auto& component) {
        return containsCurrentColor(component.color);
    });
}

bool containsColorSchemeDependentColor(const ColorMix& unresolved)
{
    return std::ranges::any_of(unresolved.components, [](const auto& component) {
        return containsColorSchemeDependentColor(component.color);
    });
}

void Serialize<ColorMix>::operator()(StringBuilder& builder, const SerializationContext& context, const ColorMix& value)
{
    builder.append("color-mix("_s);
    if (value.colorInterpolationMethod != CSS::defaultInterpolationMethodForColorMix) {
        builder.append("in "_s);
        WebCore::serializationForCSS(builder, value.colorInterpolationMethod);
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
                [&](const ColorMix::Component::Percentage::Raw& raw) {
                    if (raw.value != valueToMatch)
                        canOmitAllPercentages = false;
                    specifiedSum += raw.value;
                },
                [&](const ColorMix::Component::Percentage::Calc&) {
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
        serializationForCSS(builder, context, component.color);
        if (!canOmitAllPercentages) {
            if (component.percentage) {
                builder.append(' ');
                serializationForCSS(builder, context, *component.percentage);
            } else if (!anyComponentHasCalcPercentage) {
                builder.append(' ');
                serializationForCSS(builder, context, ColorMix::Component::Percentage::Raw { omittedPercentageValue });
            }
        }
    }, ", "_s));
    builder.append(')');
}

void ComputedStyleDependenciesCollector<ColorMix>::operator()(ComputedStyleDependencies& dependencies, const ColorMix& value)
{
    for (auto& component : value.components) {
        collectComputedStyleDependencies(dependencies, component.color);
        collectComputedStyleDependencies(dependencies, component.percentage);
    }
}

IterationStatus CSSValueChildrenVisitor<ColorMix>::operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const ColorMix& value)
{
    for (auto& component : value.components) {
        if (visitCSSValueChildren(func, component.color) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, component.percentage) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

} // namespace CSS
} // namespace WebCore
