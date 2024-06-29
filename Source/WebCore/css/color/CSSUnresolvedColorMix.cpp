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
#include "CSSUnresolvedColorResolutionContext.h"
#include "ColorSerialization.h"
#include "StyleBuilderState.h"

namespace WebCore {

PercentRaw resolveComponentPercentage(const CSSUnresolvedColorMix::Component::Percentage& percentage)
{
    return evaluateCalc(percentage, { });
}

static std::optional<PercentRaw> resolveComponentPercentage(const std::optional<CSSUnresolvedColorMix::Component::Percentage>& percentage)
{
    if (!percentage)
        return std::nullopt;
    return resolveComponentPercentage(*percentage);
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

StyleColor createStyleColor(const CSSUnresolvedColorMix& unresolved, const Document& document, RenderStyle& style, Style::ForVisitedLink forVisitedLink)
{
    return StyleColor {
        StyleColorMix {
            unresolved.colorInterpolationMethod,
            StyleColorMix::Component {
                unresolved.mixComponents1.color->createStyleColor(document, style, forVisitedLink),
                resolveComponentPercentage(unresolved.mixComponents1.percentage)
            },
            StyleColorMix::Component {
                unresolved.mixComponents2.color->createStyleColor(document, style, forVisitedLink),
                resolveComponentPercentage(unresolved.mixComponents2.percentage)
            }
        }
    };
}

Color createColor(const CSSUnresolvedColorMix& unresolved, const CSSUnresolvedColorResolutionContext& context)
{
    auto component1Color = unresolved.mixComponents1.color->createColor(context);
    if (!component1Color.isValid())
        return { };

    auto component2Color = unresolved.mixComponents2.color->createColor(context);
    if (!component2Color.isValid())
        return { };

    return mix(
        CSSColorMixResolver {
            unresolved.colorInterpolationMethod,
            CSSColorMixResolver::Component {
                WTFMove(component1Color),
                resolveComponentPercentage(unresolved.mixComponents1.percentage)
            },
            CSSColorMixResolver::Component {
                WTFMove(component2Color),
                resolveComponentPercentage(unresolved.mixComponents2.percentage)
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
