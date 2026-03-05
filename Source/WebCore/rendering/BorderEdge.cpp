/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "RenderStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

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

BorderEdges borderEdges(const RenderStyle& style, float deviceScaleFactor, RectEdges<bool> closedEdges, LayoutSize inflation, bool setColorsToBlack)
{
    auto constructBorderEdge = [&]<CSSPropertyID borderColorProperty>(const RenderStyle& style, Style::LineWidth width, float inflation, BorderStyle borderStyle, bool isTransparent, bool isPresent) {
        auto color = setColorsToBlack ? Color::black : Style::ColorPropertyResolver<Style::ColorPropertyTraits<PropertyNameConstant<borderColorProperty>>> { style }.visitedDependentColorApplyingColorFilter();
        auto evaluatedWidth = Style::evaluate<float>(width, Style::ZoomNeeded { });
        auto inflatedWidth = evaluatedWidth ? evaluatedWidth + inflation : evaluatedWidth;
        return BorderEdge(inflatedWidth, color, borderStyle, !setColorsToBlack && isTransparent, isPresent, deviceScaleFactor);
    };

    return {
        constructBorderEdge.template operator()<CSSPropertyBorderTopColor>(style, style.usedBorderTopWidth(), inflation.height().toFloat(), style.borderTopStyle(), style.borderTopColor().isKnownTransparent(), closedEdges.top()),
        constructBorderEdge.template operator()<CSSPropertyBorderRightColor>(style, style.usedBorderRightWidth(), inflation.width().toFloat(), style.borderRightStyle(), style.borderRightColor().isKnownTransparent(), closedEdges.right()),
        constructBorderEdge.template operator()<CSSPropertyBorderBottomColor>(style, style.usedBorderBottomWidth(), inflation.height().toFloat(), style.borderBottomStyle(), style.borderBottomColor().isKnownTransparent(), closedEdges.bottom()),
        constructBorderEdge.template operator()<CSSPropertyBorderLeftColor>(style, style.usedBorderLeftWidth(), inflation.width().toFloat(), style.borderLeftStyle(), style.borderLeftColor().isKnownTransparent(), closedEdges.left())
    };
}

BorderEdges borderEdgesForOutline(const RenderStyle& style, BorderStyle borderStyle, float deviceScaleFactor)
{
    auto color = style.visitedDependentOutlineColorApplyingColorFilter();
    auto isTransparent = color.isValid() && !color.isVisible();
    auto size = Style::evaluate<float>(style.usedOutlineWidth(), Style::ZoomNeeded { });
    return {
        BorderEdge { size, color, borderStyle, isTransparent, true, deviceScaleFactor },
        BorderEdge { size, color, borderStyle, isTransparent, true, deviceScaleFactor },
        BorderEdge { size, color, borderStyle, isTransparent, true, deviceScaleFactor },
        BorderEdge { size, color, borderStyle, isTransparent, true, deviceScaleFactor },
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
