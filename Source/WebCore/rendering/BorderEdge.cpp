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

BorderEdge::BorderEdge(LayoutUnit edgeWidth, Color edgeColor, EBorderStyle edgeStyle, bool edgeIsTransparent, bool edgeIsPresent, float devicePixelRatio)
    : m_width(edgeWidth)
    , m_color(edgeColor)
    , m_style(edgeStyle)
    , m_isTransparent(edgeIsTransparent)
    , m_isPresent(edgeIsPresent)
    , m_devicePixelRatio(devicePixelRatio)
{
    if (edgeStyle == DOUBLE && edgeWidth  < borderWidthInDevicePixel(3))
        m_style = SOLID;
    m_flooredToDevicePixelWidth = floorToDevicePixel(edgeWidth, devicePixelRatio);
}

BorderEdge::BorderEdge()
    : m_width(LayoutUnit::fromPixel(0))
    , m_style(BHIDDEN)
    , m_isTransparent(false)
    , m_isPresent(false)
    , m_flooredToDevicePixelWidth(0)
    , m_devicePixelRatio(1)
{
}

void BorderEdge::getBorderEdgeInfo(BorderEdge edges[], const RenderStyle& style, float deviceScaleFactor, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    bool horizontal = style.isHorizontalWritingMode();

    edges[BSTop] = BorderEdge(style.borderTopWidth(), style.visitedDependentColor(CSSPropertyBorderTopColor), style.borderTopStyle(), style.borderTopIsTransparent(),
        horizontal || includeLogicalLeftEdge, deviceScaleFactor);
    edges[BSRight] = BorderEdge(style.borderRightWidth(), style.visitedDependentColor(CSSPropertyBorderRightColor), style.borderRightStyle(), style.borderRightIsTransparent(),
        !horizontal || includeLogicalRightEdge, deviceScaleFactor);
    edges[BSBottom] = BorderEdge(style.borderBottomWidth(), style.visitedDependentColor(CSSPropertyBorderBottomColor), style.borderBottomStyle(), style.borderBottomIsTransparent(),
        horizontal || includeLogicalRightEdge, deviceScaleFactor);
    edges[BSLeft] = BorderEdge(style.borderLeftWidth(), style.visitedDependentColor(CSSPropertyBorderLeftColor), style.borderLeftStyle(), style.borderLeftIsTransparent(),
        !horizontal || includeLogicalLeftEdge, deviceScaleFactor);
    }

bool BorderEdge::obscuresBackgroundEdge(float scale) const
{
    if (!m_isPresent || m_isTransparent || (m_width * scale) < borderWidthInDevicePixel(2) || m_color.hasAlpha() || m_style == BHIDDEN)
        return false;

    if (m_style == DOTTED || m_style == DASHED)
        return false;

    if (m_style == DOUBLE)
        return m_width >= scale * borderWidthInDevicePixel(5); // The outer band needs to be >= 2px wide at unit scale.

    return true;
}

bool BorderEdge::obscuresBackground() const
{
    if (!m_isPresent || m_isTransparent || m_color.hasAlpha() || m_style == BHIDDEN)
        return false;

    if (m_style == DOTTED || m_style == DASHED || m_style == DOUBLE)
        return false;

    return true;
}

void BorderEdge::getDoubleBorderStripeWidths(LayoutUnit outerWidth, LayoutUnit innerWidth) const
{
    LayoutUnit fullWidth = widthForPainting();
    innerWidth = ceilToDevicePixel(fullWidth * 2 / 3, m_devicePixelRatio);
    outerWidth = floorToDevicePixel(fullWidth / 3, m_devicePixelRatio);
}

} // namespace WebCore
