/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BorderEdge.h"

#include "Color.h"
#include "LayoutUnit.h"
#include "RenderObject.h"
#include "RenderStyle.h"

namespace WebCore {

BorderEdge::BorderEdge(float edgeWidth, Color edgeColor, BorderStyle edgeStyle, bool edgeIsTransparent, bool edgeIsPresent, float devicePixelRatio)
    : m_color(edgeColor)
    , m_width(edgeWidth)
    , m_devicePixelRatio(devicePixelRatio)
    , m_style(edgeStyle)
    , m_isTransparent(edgeIsTransparent)
    , m_isPresent(edgeIsPresent)
{
    if (edgeStyle == BorderStyle::Double && edgeWidth  < borderWidthInDevicePixel(3))
        m_style = BorderStyle::Solid;
    m_flooredToDevicePixelWidth = floorf(edgeWidth * devicePixelRatio) / devicePixelRatio;
}

BorderEdges borderEdges(const RenderStyle& style, float deviceScaleFactor, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    bool horizontal = style.isHorizontalWritingMode();

    auto constructBorderEdge = [&](float width, CSSPropertyID borderColorProperty, BorderStyle borderStyle, bool isTransparent, bool isPresent) {
        return BorderEdge(width, style.visitedDependentColorWithColorFilter(borderColorProperty), borderStyle, isTransparent, isPresent, deviceScaleFactor);
    };

    return {
        constructBorderEdge(style.borderTopWidth(), CSSPropertyBorderTopColor, style.borderTopStyle(), style.borderTopIsTransparent(), horizontal || includeLogicalLeftEdge),
        constructBorderEdge(style.borderRightWidth(), CSSPropertyBorderRightColor, style.borderRightStyle(), style.borderRightIsTransparent(), !horizontal || includeLogicalRightEdge),
        constructBorderEdge(style.borderBottomWidth(), CSSPropertyBorderBottomColor, style.borderBottomStyle(), style.borderBottomIsTransparent(), horizontal || includeLogicalRightEdge),
        constructBorderEdge(style.borderLeftWidth(), CSSPropertyBorderLeftColor, style.borderLeftStyle(), style.borderLeftIsTransparent(), !horizontal || includeLogicalLeftEdge)
    };
}

BorderEdges borderEdgesForOutline(const RenderStyle& style, float deviceScaleFactor)
{
    auto color = style.visitedDependentColorWithColorFilter(CSSPropertyOutlineColor);
    auto isTransparent = color.isValid() && !color.isVisible();
    auto size = style.outlineWidth();
    auto outlineStyle = style.outlineStyle();
    return {
        BorderEdge { size, color, outlineStyle, isTransparent, true, deviceScaleFactor },
        BorderEdge { size, color, outlineStyle, isTransparent, true, deviceScaleFactor },
        BorderEdge { size, color, outlineStyle, isTransparent, true, deviceScaleFactor },
        BorderEdge { size, color, outlineStyle, isTransparent, true, deviceScaleFactor } 
    };
}

bool BorderEdge::obscuresBackgroundEdge(float scale) const
{
    if (!m_isPresent || m_isTransparent || (m_width * scale) < borderWidthInDevicePixel(2) || !m_color.isOpaque() || m_style == BorderStyle::Hidden)
        return false;

    if (m_style == BorderStyle::Dotted || m_style == BorderStyle::Dashed)
        return false;

    if (m_style == BorderStyle::Double)
        return m_width >= scale * borderWidthInDevicePixel(5); // The outer band needs to be >= 2px wide at unit scale.

    return true;
}

bool BorderEdge::obscuresBackground() const
{
    if (!m_isPresent || m_isTransparent || !m_color.isOpaque() || m_style == BorderStyle::Hidden)
        return false;

    if (m_style == BorderStyle::Dotted || m_style == BorderStyle::Dashed || m_style == BorderStyle::Double)
        return false;

    return true;
}

void BorderEdge::getDoubleBorderStripeWidths(LayoutUnit& outerWidth, LayoutUnit& innerWidth) const
{
    LayoutUnit fullWidth { widthForPainting() };
    innerWidth = ceilToDevicePixel(fullWidth * 2 / 3, m_devicePixelRatio);
    outerWidth = floorToDevicePixel(fullWidth / 3, m_devicePixelRatio);
}

} // namespace WebCore
