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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSLightDarkColor.h"

#include "CSSColor.h"
#include "CSSPlatformColorResolutionState.h"
#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "Document.h"
#include "StyleBuilderState.h"

namespace WebCore {
namespace CSS {

bool LightDarkColor::operator==(const LightDarkColor&) const = default;

WebCore::Color createColor(const LightDarkColor& unresolved, PlatformColorResolutionState& state)
{
    if (!state.appearance)
        return { };

    PlatformColorResolutionStateNester nester { state };

    switch (*state.appearance) {
    case LightDarkColorAppearance::Light:
        return createColor(unresolved.lightColor, state);
    case LightDarkColorAppearance::Dark:
        return createColor(unresolved.darkColor, state);
    }

    ASSERT_NOT_REACHED();
    return { };
}

bool containsCurrentColor(const LightDarkColor& unresolved)
{
    return containsCurrentColor(unresolved.lightColor)
        || containsCurrentColor(unresolved.darkColor);
}

void Serialize<LightDarkColor>::operator()(StringBuilder& builder, const LightDarkColor& value)
{
    builder.append("light-dark("_s);
    serializationForCSS(builder, value.lightColor);
    builder.append(", "_s);
    serializationForCSS(builder, value.darkColor);
    builder.append(')');
}

void ComputedStyleDependenciesCollector<LightDarkColor>::operator()(ComputedStyleDependencies& dependencies, const LightDarkColor& value)
{
    collectComputedStyleDependencies(dependencies, value.lightColor);
    collectComputedStyleDependencies(dependencies, value.darkColor);
}

IterationStatus CSSValueChildrenVisitor<LightDarkColor>::operator()(const Function<IterationStatus(CSSValue&)>& func, const LightDarkColor& value)
{
    if (visitCSSValueChildren(func, value.lightColor) == IterationStatus::Done)
        return IterationStatus::Done;
    if (visitCSSValueChildren(func, value.darkColor) == IterationStatus::Done)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

} // namespace CSS
} // namespace WebCore
