/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

    auto component1Color = createColor(unresolved.mixComponents1.color, state);
    if (!component1Color.isValid())
        return { };

    auto component2Color = createColor(unresolved.mixComponents2.color, state);
    if (!component2Color.isValid())
        return { };

    std::optional<Style::Percentage<Range{0, 100}>> percentage1;
    std::optional<Style::Percentage<Range{0, 100}>> percentage2;
    if (requiresConversionData(unresolved.mixComponents1.percentage) || requiresConversionData(unresolved.mixComponents2.percentage)) {
        if (!state.conversionData)
            return { };

        percentage1 = Style::toStyle(unresolved.mixComponents1.percentage, *state.conversionData);
        percentage2 = Style::toStyle(unresolved.mixComponents2.percentage, *state.conversionData);
    } else {
        percentage1 = Style::toStyleNoConversionDataRequired(unresolved.mixComponents1.percentage);
        percentage2 = Style::toStyleNoConversionDataRequired(unresolved.mixComponents2.percentage);
    }

    return mix(
        ColorMixResolver {
            unresolved.colorInterpolationMethod,
            ColorMixResolver::Component {
                WTFMove(component1Color),
                WTFMove(percentage1),
            },
            ColorMixResolver::Component {
                WTFMove(component2Color),
                WTFMove(percentage2),
            }
        }
    );
}

bool containsCurrentColor(const ColorMix& unresolved)
{
    return containsCurrentColor(unresolved.mixComponents1.color)
        || containsCurrentColor(unresolved.mixComponents2.color);
}

bool containsColorSchemeDependentColor(const ColorMix& unresolved)
{
    return containsColorSchemeDependentColor(unresolved.mixComponents1.color)
        || containsColorSchemeDependentColor(unresolved.mixComponents2.color);
}

namespace ColorMixSerializationDetails {

static bool sumTo100Percent(const ColorMix::Component::Percentage& a, const ColorMix::Component::Percentage& b)
{
    return a.isRaw() && b.isRaw() && a.raw()->value + b.raw()->value == 100.0;
}

static std::optional<PercentageRaw<>> subtractFrom100Percent(const ColorMix::Component::Percentage& percentage)
{
    return percentage.isRaw() ? std::make_optional(PercentageRaw<> { 100.0 - percentage.raw()->value }) : std::nullopt;
}

static void serializationForColorMixPercentage1(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMix& value)
{
    if (value.mixComponents1.percentage && value.mixComponents2.percentage) {
        if (*value.mixComponents1.percentage == 50_css_percentage && *value.mixComponents2.percentage == 50_css_percentage)
            return;
        builder.append(' ');
        serializationForCSS(builder, context, *value.mixComponents1.percentage);
    } else if (value.mixComponents1.percentage) {
        if (*value.mixComponents1.percentage == 50_css_percentage)
            return;
        builder.append(' ');
        serializationForCSS(builder, context, *value.mixComponents1.percentage);
    } else if (value.mixComponents2.percentage) {
        if (*value.mixComponents2.percentage == 50_css_percentage)
            return;

        auto subtractedPercent = subtractFrom100Percent(*value.mixComponents2.percentage);
        if (!subtractedPercent)
            return;

        builder.append(' ');
        serializationForCSS(builder, context, *subtractedPercent);
    }
}

static void serializationForColorMixPercentage2(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMix& value)
{
    if (value.mixComponents1.percentage && value.mixComponents2.percentage) {
        if (sumTo100Percent(*value.mixComponents1.percentage, *value.mixComponents2.percentage))
            return;

        builder.append(' ');
        serializationForCSS(builder, context, *value.mixComponents2.percentage);
    } else if (value.mixComponents2.percentage) {
        if (*value.mixComponents2.percentage == 50_css_percentage)
            return;
        if (!value.mixComponents2.percentage->isCalc())
            return;

        builder.append(' ');
        serializationForCSS(builder, context, *value.mixComponents2.percentage);
    }
}

} // namespace ColorMixSerializationDetails

void Serialize<ColorMix>::operator()(StringBuilder& builder, const SerializationContext& context, const ColorMix& value)
{

    builder.append("color-mix("_s);
    if (value.colorInterpolationMethod != CSS::defaultInterpolationMethodForColorMix) {
        builder.append("in "_s);
        WebCore::serializationForCSS(builder, value.colorInterpolationMethod);
        builder.append(", "_s);
    }
    serializationForCSS(builder, context, value.mixComponents1.color);
    ColorMixSerializationDetails::serializationForColorMixPercentage1(builder, context, value);
    builder.append(", "_s);
    serializationForCSS(builder, context, value.mixComponents2.color);
    ColorMixSerializationDetails::serializationForColorMixPercentage2(builder, context, value);
    builder.append(')');
}

void ComputedStyleDependenciesCollector<ColorMix>::operator()(ComputedStyleDependencies& dependencies, const ColorMix& value)
{
    collectComputedStyleDependencies(dependencies, value.mixComponents1.color);
    collectComputedStyleDependencies(dependencies, value.mixComponents1.percentage);
    collectComputedStyleDependencies(dependencies, value.mixComponents2.color);
    collectComputedStyleDependencies(dependencies, value.mixComponents2.percentage);
}

IterationStatus CSSValueChildrenVisitor<ColorMix>::operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const ColorMix& value)
{
    if (visitCSSValueChildren(func, value.mixComponents1.color) == IterationStatus::Done)
        return IterationStatus::Done;
    if (visitCSSValueChildren(func, value.mixComponents1.percentage) == IterationStatus::Done)
        return IterationStatus::Done;
    if (visitCSSValueChildren(func, value.mixComponents2.color) == IterationStatus::Done)
        return IterationStatus::Done;
    if (visitCSSValueChildren(func, value.mixComponents2.percentage) == IterationStatus::Done)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

} // namespace CSS
} // namespace WebCore
