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
#include "CSSUnresolvedColorMix.h"

#include "CSSColorMixResolver.h"
#include "CSSColorMixSerialization.h"
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorResolutionState.h"
#include "CSSUnresolvedStyleColorResolutionState.h"
#include "ColorSerialization.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {

ResolvedColorMixPercentage resolveComponentPercentage(const CSSUnresolvedColorMix::Component::Percentage& percentage, const CSSToLengthConversionData& conversionData)
{
    return Style::toStyle(percentage, conversionData);
}

ResolvedColorMixPercentage resolveComponentPercentageNoConversionDataRequired(const CSSUnresolvedColorMix::Component::Percentage& percentage)
{
    ASSERT(!requiresConversionData(percentage));

    return Style::toStyleNoConversionDataRequired(percentage);
}

static std::optional<ResolvedColorMixPercentage> resolveComponentPercentage(const std::optional<CSSUnresolvedColorMix::Component::Percentage>& percentage, const CSSToLengthConversionData& conversionData)
{
    if (!percentage)
        return std::nullopt;
    return resolveComponentPercentage(*percentage, conversionData);
}

static std::optional<ResolvedColorMixPercentage> resolveComponentPercentageNoConversionDataRequired(const std::optional<CSSUnresolvedColorMix::Component::Percentage>& percentage)
{
    if (!percentage)
        return std::nullopt;
    return resolveComponentPercentageNoConversionDataRequired(*percentage);
}

void serializationForCSS(StringBuilder& builder, const CSSUnresolvedColorMix& colorMix)
{
    serializationForCSSColorMix(builder, colorMix);
}

String serializationForCSS(const CSSUnresolvedColorMix& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

bool CSSUnresolvedColorMix::Component::operator==(const CSSUnresolvedColorMix::Component& other) const
{
    return color->equals(color.get()) && percentage == other.percentage;
}

StyleColor createStyleColor(const CSSUnresolvedColorMix& unresolved, CSSUnresolvedStyleColorResolutionState& state)
{
    CSSUnresolvedStyleColorResolutionNester nester { state };

    auto component1Color = unresolved.mixComponents1.color->createStyleColor(state);
    auto component2Color = unresolved.mixComponents2.color->createStyleColor(state);

    auto percentage1 = resolveComponentPercentage(unresolved.mixComponents1.percentage, state.conversionData);
    auto percentage2 = resolveComponentPercentage(unresolved.mixComponents2.percentage, state.conversionData);

    if (!component1Color.isAbsoluteColor() || !component2Color.isAbsoluteColor()) {
        // If the either component is not absolute, we cannot fully resolve the color yet. Instead, we resolve the calc values using the conversion data, and return a StyleColorMix to be resolved at use time.
        return StyleColor {
            StyleColorMix {
                unresolved.colorInterpolationMethod,
                StyleColorMix::Component {
                    WTFMove(component1Color),
                    WTFMove(percentage1)
                },
                StyleColorMix::Component {
                    WTFMove(component2Color),
                    WTFMove(percentage2)
                }
            }
        };
    }

    return mix(
        CSSColorMixResolver {
            unresolved.colorInterpolationMethod,
            CSSColorMixResolver::Component {
                component1Color.absoluteColor(),
                WTFMove(percentage1)
            },
            CSSColorMixResolver::Component {
                component2Color.absoluteColor(),
                WTFMove(percentage2)
            }
        }
    );
}

Color createColor(const CSSUnresolvedColorMix& unresolved, CSSUnresolvedColorResolutionState& state)
{
    auto component1Color = unresolved.mixComponents1.color->createColor(state);
    if (!component1Color.isValid())
        return { };

    auto component2Color = unresolved.mixComponents2.color->createColor(state);
    if (!component2Color.isValid())
        return { };

    std::optional<ResolvedColorMixPercentage> percentage1;
    std::optional<ResolvedColorMixPercentage> percentage2;
    if (requiresConversionData(unresolved.mixComponents1.percentage) || requiresConversionData(unresolved.mixComponents2.percentage)) {
        if (!state.conversionData)
            return { };

        percentage1 = resolveComponentPercentage(unresolved.mixComponents1.percentage, *state.conversionData);
        percentage2 = resolveComponentPercentage(unresolved.mixComponents2.percentage, *state.conversionData);
    } else {
        percentage1 = resolveComponentPercentageNoConversionDataRequired(unresolved.mixComponents1.percentage);
        percentage2 = resolveComponentPercentageNoConversionDataRequired(unresolved.mixComponents2.percentage);
    }

    return mix(
        CSSColorMixResolver {
            unresolved.colorInterpolationMethod,
            CSSColorMixResolver::Component {
                WTFMove(component1Color),
                WTFMove(percentage1),
            },
            CSSColorMixResolver::Component {
                WTFMove(component2Color),
                WTFMove(percentage2),
            }
        }
    );
}

bool containsCurrentColor(const CSSUnresolvedColorMix& unresolved)
{
    return unresolved.mixComponents1.color->containsCurrentColor()
        || unresolved.mixComponents2.color->containsCurrentColor();
}

bool containsColorSchemeDependentColor(const CSSUnresolvedColorMix& unresolved)
{
    return unresolved.mixComponents1.color->containsColorSchemeDependentColor()
        || unresolved.mixComponents2.color->containsColorSchemeDependentColor();
}

} // namespace WebCore
