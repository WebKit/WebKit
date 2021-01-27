/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayBoxDecorationData.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "RenderStyle.h"

namespace WebCore {
namespace Display {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BoxDecorationData);

BorderEdge::BorderEdge(float width, float innerWidth, float outerWidth, Color color, BorderStyle style, bool isTransparent, bool isPresent)
    : m_color(color)
    , m_width(width)
    , m_innerWidth(innerWidth)
    , m_outerWidth(outerWidth)
    , m_style(style)
    , m_isTransparent(isTransparent)
    , m_isPresent(isPresent)
{
}

bool BorderEdge::obscuresBackground() const
{
    if (!m_isPresent || m_isTransparent || !m_color.isOpaque() || m_style == BorderStyle::Hidden)
        return false;

    if (m_style == BorderStyle::Dotted || m_style == BorderStyle::Dashed || m_style == BorderStyle::Double)
        return false;

    return true;
}

bool BorderEdge::obscuresBackgroundEdge(float scale) const
{
    if (!m_isPresent || m_isTransparent || (m_width * scale) < floorToDevicePixel(2, scale) || !m_color.isOpaque() || m_style == BorderStyle::Hidden)
        return false;

    if (m_style == BorderStyle::Dotted || m_style == BorderStyle::Dashed)
        return false;

    if (m_style == BorderStyle::Double)
        return m_width >= scale * floorToDevicePixel(5, scale); // The outer band needs to be >= 2px wide at unit scale.

    return true;
}

RectEdges<BorderEdge> calculateBorderEdges(const RenderStyle& style, float pixelSnappingFactor, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    bool horizontal = style.isHorizontalWritingMode();

    auto constructBorderEdge = [&](float width, CSSPropertyID colorProperty, BorderStyle borderStyle, bool isTransparent, bool isPresent) ->BorderEdge {
        float innerWidth = 0.f;
        float outerWidth = 0.f;

        if (borderStyle == BorderStyle::Double) {
            if (borderStyle == BorderStyle::Double && width < floorToDevicePixel(3, pixelSnappingFactor))
                borderStyle = BorderStyle::Solid;
            else {
                innerWidth = ceilToDevicePixel(width * 2 / 3, pixelSnappingFactor);
                outerWidth = floorToDevicePixel(width / 3, pixelSnappingFactor);
            }
        }

        float snappedWidth = floorToDevicePixel(width, pixelSnappingFactor);
        return { snappedWidth, innerWidth, outerWidth, style.visitedDependentColorWithColorFilter(colorProperty), borderStyle, isTransparent, isPresent };
    };

    return {
        constructBorderEdge(style.borderTopWidth(), CSSPropertyBorderTopColor, style.borderTopStyle(), style.borderTopIsTransparent(), !horizontal || includeLogicalRightEdge),
        constructBorderEdge(style.borderRightWidth(), CSSPropertyBorderRightColor, style.borderRightStyle(), style.borderRightIsTransparent(), !horizontal || includeLogicalRightEdge),
        constructBorderEdge(style.borderBottomWidth(), CSSPropertyBorderBottomColor, style.borderBottomStyle(), style.borderBottomIsTransparent(), horizontal || includeLogicalRightEdge),
        constructBorderEdge(style.borderLeftWidth(), CSSPropertyBorderLeftColor, style.borderLeftStyle(), style.borderLeftIsTransparent(), !horizontal || includeLogicalLeftEdge)
    };
}

std::pair<BoxSide, BoxSide> adjacentSidesForSide(BoxSide side)
{
    switch (side) {
    case BoxSide::Top: return { BoxSide::Left, BoxSide::Right };
    case BoxSide::Bottom: return { BoxSide::Left, BoxSide::Right };
    case BoxSide::Left: return { BoxSide::Top, BoxSide::Bottom };
    case BoxSide::Right: return { BoxSide::Top, BoxSide::Bottom };
    }
    return { BoxSide::Left, BoxSide::Right };
}

BoxDecorationData::BoxDecorationData() = default;

bool BoxDecorationData::hasBorder() const
{
    // FIXME: There's a tricky interaction between borders and border-image here: webkit.org/b/217900.
    if (hasBorderImage())
        return true;

    for (auto side : allBoxSides) {
        auto& edge = m_borderEdges[side];
        if (edge.style() != BorderStyle::None && edge.width())
            return true;
    }
    return false;
}

bool BoxDecorationData::borderObscuresBackground() const
{
    if (!hasBorder())
        return false;

    // Bail if we have any border-image for now. We could look at the image alpha to improve this.
    // FIXME: Check border-image.

    for (auto side : allBoxSides) {
        if (!m_borderEdges.at(side).obscuresBackground())
            return false;
    }

    return true;
}

bool BoxDecorationData::borderObscuresBackgroundEdge(const FloatSize& contextScale) const
{
    for (auto side : allBoxSides) {
        auto& currEdge = m_borderEdges.at(side);
        // FIXME: for vertical text
        float axisScale = (side == BoxSide::Top || side == BoxSide::Bottom) ? contextScale.height() : contextScale.width();
        if (!currEdge.obscuresBackgroundEdge(axisScale))
            return false;
    }

    return true;
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
