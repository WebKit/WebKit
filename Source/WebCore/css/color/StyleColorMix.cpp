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
#include "StyleColorMix.h"

#include "CSSColorMixSerialization.h"
#include "CSSResolvedColorMix.h"
#include "ColorSerialization.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

bool containsCurrentColor(const StyleColorMix& colorMix)
{
    return colorMix.mixComponents1.color.containsCurrentColor()
        || colorMix.mixComponents2.color.containsCurrentColor();
}

// MARK: Resolve

std::optional<Color> resolveAbsoluteComponents(const StyleColorMix& colorMix)
{
    if (!colorMix.mixComponents1.color.isAbsoluteColor() || !colorMix.mixComponents2.color.isAbsoluteColor())
        return std::nullopt;

    return mix(CSSResolvedColorMix {
        colorMix.colorInterpolationMethod,
        CSSResolvedColorMix::Component {
            colorMix.mixComponents1.color.absoluteColor(),
            colorMix.mixComponents1.percentage
        },
        CSSResolvedColorMix::Component {
            colorMix.mixComponents2.color.absoluteColor(),
            colorMix.mixComponents2.percentage
        }
    });
}

Color resolveColor(const StyleColorMix& colorMix, const Color& currentColor)
{
    return mix(CSSResolvedColorMix {
        colorMix.colorInterpolationMethod,
        CSSResolvedColorMix::Component {
            colorMix.mixComponents1.color.resolveColor(currentColor),
            colorMix.mixComponents1.percentage
        },
        CSSResolvedColorMix::Component {
            colorMix.mixComponents2.color.resolveColor(currentColor),
            colorMix.mixComponents2.percentage
        }
    });
}

// MARK: - Serialization

void serializationForCSS(StringBuilder& builder, const StyleColorMix& colorMix)
{
    serializationForCSSColorMix(builder, colorMix);
}

String serializationForCSS(const StyleColorMix& colorMix)
{
    StringBuilder builder;
    serializationForCSS(builder, colorMix);
    return builder.toString();
}

// MARK: - TextStream.

static WTF::TextStream& operator<<(WTF::TextStream& ts, const StyleColorMix::Component& component)
{
    ts << component.color;
    if (component.percentage)
        ts << " " << *component.percentage << "%";
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const StyleColorMix& colorMix)
{
    ts << "color-mix(";
    ts << "in " << colorMix.colorInterpolationMethod;
    ts << ", " << colorMix.mixComponents1;
    ts << ", " << colorMix.mixComponents2;
    ts << ")";

    return ts;
}

} // namespace WebCore
