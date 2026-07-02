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
#include "CSSRelativeAlphaColor.h"

#include "CSSPlatformColorResolutionState.h"
#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSRelativeAlphaColorResolver.h"

namespace WebCore {
namespace CSS {

bool RelativeAlphaColor::operator==(const RelativeAlphaColor&) const = default;

WebCore::Color createColor(const RelativeAlphaColor& unresolved, PlatformColorResolutionState& state)
{
    PlatformColorResolutionStateNester nester { state };

    auto origin = createColor(unresolved.origin, state);
    if (!origin.isValid())
        return { };

    auto resolver = RelativeAlphaColorResolver {
        .origin = WTF::move(origin),
        .alpha = unresolved.alpha
    };

    if (state.conversionData)
        return resolve(WTF::move(resolver), *state.conversionData);

    if (!requiresConversionData(resolver.alpha))
        return resolveNoConversionDataRequired(WTF::move(resolver));

    return { };
}

bool containsCurrentColor(const RelativeAlphaColor& unresolved)
{
    return containsCurrentColor(unresolved.origin);
}

bool containsColorSchemeDependentColor(const RelativeAlphaColor& unresolved)
{
    return containsColorSchemeDependentColor(unresolved.origin);
}

void Serialize<RelativeAlphaColor>::operator()(StringBuilder& builder, const SerializationContext& context, const RelativeAlphaColor& value)
{
    builder.append("alpha(from "_s);
    serializationForCSS(builder, context, value.origin);

    if (value.alpha) {
        builder.append(" / "_s);
        serializationForCSS(builder, context, *value.alpha);
    }

    builder.append(')');
}

void ComputedStyleDependenciesCollector<RelativeAlphaColor>::operator()(ComputedStyleDependencies& dependencies, const RelativeAlphaColor& value)
{
    collectComputedStyleDependencies(dependencies, value.origin);
    collectComputedStyleDependencies(dependencies, value.alpha);
}

IterationStatus CSSValueChildrenVisitor<RelativeAlphaColor>::operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const RelativeAlphaColor& value)
{
    if (visitCSSValueChildren(func, value.origin) == IterationStatus::Done)
        return IterationStatus::Done;
    if (visitCSSValueChildren(func, value.alpha) == IterationStatus::Done)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

} // namespace CSS
} // namespace WebCore
