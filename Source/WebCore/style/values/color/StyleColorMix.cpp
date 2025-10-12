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
#include "StyleColorMix.h"

#include "CSSColorMixResolver.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "ColorSerialization.h"
#include "StyleColorResolutionState.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

Color toStyleColor(const CSS::ColorMix& unresolved, ColorResolutionState& state)
{
    ColorResolutionStateNester nester { state };

    auto component1Color = toStyleColor(unresolved.mixComponents1.color, state);
    auto component2Color = toStyleColor(unresolved.mixComponents2.color, state);

    auto percentage1 = toStyle(unresolved.mixComponents1.percentage, state.conversionData);
    auto percentage2 = toStyle(unresolved.mixComponents2.percentage, state.conversionData);

    if (!component1Color.isResolvedColor() || !component2Color.isResolvedColor()) {
        // If the either component is not resolved, we cannot fully resolve the color
        // yet. Instead, we resolve the calc values using the conversion data, and return
        // a Style::ColorMix to be resolved at use time.
        return Color {
            ColorMix {
                unresolved.colorInterpolationMethod,
                ColorMix::Component {
                    WTFMove(component1Color),
                    WTFMove(percentage1)
                },
                ColorMix::Component {
                    WTFMove(component2Color),
                    WTFMove(percentage2)
                }
            }
        };
    }

    return mix(
        CSS::ColorMixResolver {
            unresolved.colorInterpolationMethod,
            CSS::ColorMixResolver::Component {
                component1Color.resolvedColor(),
                WTFMove(percentage1)
            },
            CSS::ColorMixResolver::Component {
                component2Color.resolvedColor(),
                WTFMove(percentage2)
            }
        }
    );
}


// MARK: - Resolve

WebCore::Color resolveColor(const ColorMix& colorMix, const WebCore::Color& currentColor)
{
    return mix(
        CSS::ColorMixResolver {
            colorMix.colorInterpolationMethod,
            CSS::ColorMixResolver::Component {
                colorMix.mixComponents1.color.resolveColor(currentColor),
                colorMix.mixComponents1.percentage
            },
            CSS::ColorMixResolver::Component {
                colorMix.mixComponents2.color.resolveColor(currentColor),
                colorMix.mixComponents2.percentage
            }
        }
    );
}

// MARK: - Current Color

bool containsCurrentColor(const ColorMix& colorMix)
{
    return WebCore::Style::containsCurrentColor(colorMix.mixComponents1.color)
        || WebCore::Style::containsCurrentColor(colorMix.mixComponents2.color);
}

// MARK: - Serialization

namespace ColorMixSerializationDetails {

static bool sumTo100Percent(const ColorMix::Component::Percentage& a, const ColorMix::Component::Percentage& b)
{
    return a.value + b.value == 100.0;
}

static std::optional<ColorMix::Component::Percentage> subtractFrom100Percent(const ColorMix::Component::Percentage& percentage)
{
    return ColorMix::Component::Percentage { 100.0 - percentage.value };
}

static void serializeColorMixPercentage(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMix::Component::Percentage& percentage)
{
    CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { percentage.value });
}

static void serializationForColorMixPercentage1(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMix& value)
{
    if (value.mixComponents1.percentage && value.mixComponents2.percentage) {
        if (*value.mixComponents1.percentage == 50_css_percentage && *value.mixComponents2.percentage == 50_css_percentage)
            return;
        builder.append(' ');
        serializeColorMixPercentage(builder, context, *value.mixComponents1.percentage);
    } else if (value.mixComponents1.percentage) {
        if (*value.mixComponents1.percentage == 50_css_percentage)
            return;
        builder.append(' ');
        serializeColorMixPercentage(builder, context, *value.mixComponents1.percentage);
    } else if (value.mixComponents2.percentage) {
        if (*value.mixComponents2.percentage == 50_css_percentage)
            return;

        auto subtractedPercent = subtractFrom100Percent(*value.mixComponents2.percentage);
        if (!subtractedPercent)
            return;

        builder.append(' ');
        serializeColorMixPercentage(builder, context, *subtractedPercent);
    }
}

static void serializationForColorMixPercentage2(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMix& value)
{
    if (value.mixComponents1.percentage && value.mixComponents2.percentage) {
        if (sumTo100Percent(*value.mixComponents1.percentage, *value.mixComponents2.percentage))
            return;

        builder.append(' ');
        serializeColorMixPercentage(builder, context, *value.mixComponents2.percentage);
    }
}

} // namespace ColorMixSerializationDetails

void serializationForCSSTokenization(StringBuilder& builder, const CSS::SerializationContext& context, const ColorMix& colorMix)
{
    builder.append("color-mix("_s);
    if (colorMix.colorInterpolationMethod != CSS::defaultInterpolationMethodForColorMix) {
        builder.append("in "_s);
        WebCore::serializationForCSS(builder, colorMix.colorInterpolationMethod);
        builder.append(", "_s);
    }
    serializationForCSSTokenization(builder, context, colorMix.mixComponents1.color);
    ColorMixSerializationDetails::serializationForColorMixPercentage1(builder, context, colorMix);
    builder.append(", "_s);
    serializationForCSSTokenization(builder, context, colorMix.mixComponents2.color);
    ColorMixSerializationDetails::serializationForColorMixPercentage2(builder, context, colorMix);
    builder.append(')');
}

String serializationForCSSTokenization(const CSS::SerializationContext& context, const ColorMix& colorMix)
{
    StringBuilder builder;
    serializationForCSSTokenization(builder, context, colorMix);
    return builder.toString();
}

// MARK: - TextStream

static WTF::TextStream& operator<<(WTF::TextStream& ts, const ColorMix::Component& component)
{
    ts << component.color;
    if (component.percentage)
        ts << ' ' << component.percentage->value << '%';
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const ColorMix& colorMix)
{
    ts << "color-mix("_s;
    ts << "in "_s << colorMix.colorInterpolationMethod;
    ts << ", "_s << colorMix.mixComponents1;
    ts << ", "_s << colorMix.mixComponents2;
    ts << ')';

    return ts;
}

} // namespace Style
} // namespace WebCore
