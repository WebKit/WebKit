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
#include "CSSUnresolvedColorLayers.h"

#include "CSSColorLayersResolver.h"
#include "CSSColorLayersSerialization.h"
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorResolutionState.h"
#include "CSSUnresolvedStyleColorResolutionState.h"
#include "ColorSerialization.h"
#include "StyleBuilderState.h"

namespace WebCore {

void serializationForCSS(StringBuilder& builder, const CSSUnresolvedColorLayers& colorLayers)
{
    serializationForCSSColorLayers(builder, colorLayers);
}

String serializationForCSS(const CSSUnresolvedColorLayers& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

StyleColor createStyleColor(const CSSUnresolvedColorLayers& unresolved, CSSUnresolvedStyleColorResolutionState& state)
{
    CSSUnresolvedStyleColorResolutionNester nester { state };

    auto colors = unresolved.colors.map([&](auto& color) -> StyleColor {
        return color->createStyleColor(state);
    });

    if (std::ranges::any_of(colors, [](auto& color) { return color.isAbsoluteColor(); })) {
        // If the any of the layer's colors is not absolute, we cannot fully resolve the color yet. Instead we return a StyleColorLayers to be resolved at use time.
        return StyleColor {
            StyleColorLayers {
                .blendMode = unresolved.blendMode,
                .colors = WTFMove(colors)
            }
        };
    }

    auto resolver = CSSColorLayersResolver {
        .blendMode = unresolved.blendMode,
        // FIXME: This should be made into a lazy transformed range to avoid the unnecessary temporary allocation.
        .colors = colors.map([&](const auto& color) {
            return color.absoluteColor();
        })
    };


    return blendSourceOver(WTFMove(resolver));
}

Color createColor(const CSSUnresolvedColorLayers& unresolved, CSSUnresolvedColorResolutionState& state)
{
    auto resolver = CSSColorLayersResolver {
        .blendMode = unresolved.blendMode,
        // FIXME: This should be made into a lazy transformed range to avoid the unnecessary temporary allocation.
        .colors = unresolved.colors.map([&](const auto& color) {
            return color->createColor(state);
        })
    };

    return blendSourceOver(WTFMove(resolver));
}

bool containsCurrentColor(const CSSUnresolvedColorLayers& unresolved)
{
    return unresolved.colors.containsIf([](const auto& color) {
        return color->containsCurrentColor();
    });
}

bool containsColorSchemeDependentColor(const CSSUnresolvedColorLayers& unresolved)
{
    return unresolved.colors.containsIf([](const auto& color) {
        return color->containsColorSchemeDependentColor();
    });
}

} // namespace WebCore
