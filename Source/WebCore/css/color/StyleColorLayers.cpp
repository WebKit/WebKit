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
#include "CSSColorLayersSerialization.h"
#include "ColorSerialization.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

// MARK: Resolve

std::optional<Color> resolveAbsoluteComponents(const StyleColorLayers& colorLayers)
{
    if (containsNonAbsoluteColor(colorLayers))
        return std::nullopt;

    return blendSourceOver(
        CSSColorLayersResolver {
            .blendMode = colorLayers.blendMode,
            .colors = colorLayers.colors.map([&](auto& color) {
                return color.absoluteColor();
            })
        }
    );
}

Color resolveColor(const StyleColorLayers& colorLayers, const Color& currentColor)
{
    return blendSourceOver(
        CSSColorLayersResolver {
            .blendMode = colorLayers.blendMode,
            .colors = colorLayers.colors.map([&](auto& color) {
                return color.resolveColor(currentColor);
            })
        }
    );
}

bool containsNonAbsoluteColor(const StyleColorLayers& colorLayers)
{
    return colorLayers.colors.containsIf([&](auto& color) {
        return !color.isAbsoluteColor();
    });
}

// MARK: - Current Color

bool containsCurrentColor(const StyleColorLayers& colorLayers)
{
    return colorLayers.colors.containsIf([&](auto& color) {
        return color.containsCurrentColor();
    });
}

// MARK: - Serialization

void serializationForCSS(StringBuilder& builder, const StyleColorLayers& colorLayers)
{
    serializationForCSSColorLayers(builder, colorLayers);
}

String serializationForCSS(const StyleColorLayers& colorLayers)
{
    StringBuilder builder;
    serializationForCSS(builder, colorLayers);
    return builder.toString();
}

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream& ts, const StyleColorLayers& colorLayers)
{
    return ts << serializationForCSS(colorLayers);
}

} // namespace WebCore
