/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleContrastColor.h"

#include "CSSContrastColorResolver.h"
#include "CSSSerializationContext.h"
#include "ColorSerialization.h"
#include "StyleBuilderState.h"
#include "StyleColorResolutionState.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

Color toStyleColor(const CSS::ContrastColor& unresolved, ColorResolutionState& state)
{
    ColorResolutionStateNester nester { state };

    auto color = toStyleColor(unresolved.color, state);
    if (!color.isResolvedColor()) {
        return Color {
            ContrastColor {
                WTFMove(color),
            }
        };
    }

    return resolve(
        CSS::ContrastColorResolver {
            color.resolvedColor(),
        }
    );
}


// MARK: - Resolve

WebCore::Color resolveColor(const ContrastColor& contrastColor, const WebCore::Color& currentColor)
{
    return resolve(
        CSS::ContrastColorResolver {
            contrastColor.color.resolveColor(currentColor),
        }
    );
}

// MARK: - Current Color

bool containsCurrentColor(const ContrastColor& contrastColor)
{
    return WebCore::Style::containsCurrentColor(contrastColor.color);
}

// MARK: - Serialization

void serializationForCSSTokenization(StringBuilder& builder, const CSS::SerializationContext& context, const ContrastColor& contrastColor)
{
    builder.append("contrast-color("_s);
    serializationForCSSTokenization(builder, context, contrastColor.color);
    builder.append(')');
}

String serializationForCSSTokenization(const CSS::SerializationContext& context, const ContrastColor& contrastColor)
{
    StringBuilder builder;
    serializationForCSSTokenization(builder, context, contrastColor);
    return builder.toString();
}

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream& ts, const ContrastColor& contrastColor)
{
    return ts << "contrast-color("_s << contrastColor.color << ")"_s;
}

} // namespace Style
} // namespace WebCore
