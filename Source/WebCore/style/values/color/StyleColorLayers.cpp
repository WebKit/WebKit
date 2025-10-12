/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "StyleColorLayers.h"

#include "CSSColorLayersResolver.h"
#include "CSSPrimitiveValueMappings.h"
#include "ColorSerialization.h"
#include "StyleBuilderState.h"
#include "StyleColorResolutionState.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

Color toStyleColor(const CSS::ColorLayers& unresolved, ColorResolutionState& state)
{
    ColorResolutionStateNester nester { state };

    auto colors = CommaSeparatedVector<Color> { unresolved.colors.map([&](auto& color) -> Color {
        return toStyleColor(color, state);
    }) };

    if (std::ranges::any_of(colors, [](auto& color) { return color.isResolvedColor(); })) {
        // If the any of the layer's colors are not resolved, we cannot fully resolve the
        // color yet. Instead we return a Style::ColorLayers to be resolved at use time.
        return Color {
            ColorLayers {
                .blendMode = unresolved.blendMode,
                .colors = WTFMove(colors)
            }
        };
    }

    auto resolver = CSS::ColorLayersResolver {
        .blendMode = unresolved.blendMode,
        // FIXME: This should be made into a lazy transformed range to avoid the unnecessary temporary allocation.
        .colors = colors.map([&](const auto& color) {
            return color.resolvedColor();
        })
    };

    return blendSourceOver(WTFMove(resolver));
}

// MARK: Resolve

WebCore::Color resolveColor(const ColorLayers& colorLayers, const WebCore::Color& currentColor)
{
    return blendSourceOver(
        CSS::ColorLayersResolver {
            .blendMode = colorLayers.blendMode,
            .colors = colorLayers.colors.map([&](auto& color) {
                return color.resolveColor(currentColor);
            })
        }
    );
}

// MARK: - Current Color

bool containsCurrentColor(const ColorLayers& colorLayers)
{
    return std::ranges::any_of(colorLayers.colors, [&](auto& color) {
        return WebCore::Style::containsCurrentColor(color);
    });
}

// MARK: - Serialization

void serializationForCSSTokenization(StringBuilder& builder, const CSS::SerializationContext& context, const ColorLayers& value)
{
    builder.append("color-layers("_s);

    if (value.blendMode != BlendMode::Normal)
        builder.append(nameLiteralForSerialization(toCSSValueID(value.blendMode)), ", "_s);

    builder.append(interleave(value.colors, [&](auto& builder, auto& color) {
        serializationForCSSTokenization(builder, context, color);
    }, ", "_s));

    builder.append(')');
}

String serializationForCSSTokenization(const CSS::SerializationContext& context, const ColorLayers& colorLayers)
{
    StringBuilder builder;
    serializationForCSSTokenization(builder, context, colorLayers);
    return builder.toString();
}

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream& ts, const ColorLayers& colorLayers)
{
    ts << "color-layers(";
    if (colorLayers.blendMode != BlendMode::Normal)
        ts << colorLayers.blendMode << ", "_s;
    ts << colorLayers.colors << ')';

    return ts;
}

} // namespace Style
} // namespace WebCore
