/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSColorLayers.h"

#include "CSSColorLayersResolver.h"
#include "CSSColorLayersSerialization.h"
#include "CSSPlatformColorResolutionState.h"
#include "ColorSerialization.h"

namespace WebCore {
namespace CSS {

WebCore::Color createColor(const ColorLayers& value, PlatformColorResolutionState& state)
{
    PlatformColorResolutionStateNester nester { state };

    auto resolver = ColorLayersResolver {
        .blendMode = value.blendMode,
        // FIXME: This should be made into a lazy transformed range to avoid the unnecessary temporary allocation.
        .colors = value.colors.map([&](const auto& color) {
            return createColor(color, state);
        })
    };

    return blendSourceOver(WTFMove(resolver));
}

bool containsCurrentColor(const ColorLayers& value)
{
    return std::ranges::any_of(value.colors, [](const auto& color) {
        return containsCurrentColor(color);
    });
}

bool containsColorSchemeDependentColor(const ColorLayers& value)
{
    return std::ranges::any_of(value.colors, [](const auto& color) {
        return containsColorSchemeDependentColor(color);
    });
}

void Serialize<ColorLayers>::operator()(StringBuilder& builder, const ColorLayers& value)
{
    serializationForCSSColorLayers(builder, value);
}

void ComputedStyleDependenciesCollector<ColorLayers>::operator()(ComputedStyleDependencies& dependencies, const ColorLayers& value)
{
    collectComputedStyleDependenciesOnRangeLike(dependencies, value.colors);
}

IterationStatus CSSValueChildrenVisitor<ColorLayers>::operator()(const Function<IterationStatus(CSSValue&)>& func, const ColorLayers& value)
{
    return visitCSSValueChildrenOnRangeLike(func, value.colors);
}

} // namespace CSS
} // namespace WebCore
