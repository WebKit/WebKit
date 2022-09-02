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

#pragma once

#include "Color.h"
#include "DisplayFillLayerImageGeometry.h"
#include "FloatRoundedRect.h"
#include "RectEdges.h"
#include <utility>
#include <wtf/IsoMalloc.h>

namespace WebCore {

class RenderStyle;

namespace Layout {
class Box;
class BoxGeometry;
}

namespace Display {

// Pixel-snapped border edge.
class BorderEdge {
public:
    BorderEdge() = default;
    BorderEdge(float width, float innerWidth, float outerWidth, Color, BorderStyle, bool isTransparent, bool isPresent);

    float width() const { return m_width; }
    BorderStyle style() const { return m_style; }
    const Color& color() const { return m_color; }
    bool isTransparent() const { return m_isTransparent; }
    bool isPresent() const { return m_isPresent; }

    bool hasVisibleColorAndStyle() const { return m_style > BorderStyle::Hidden && !m_isTransparent; }
    bool shouldRender() const { return widthForPainting() && hasVisibleColorAndStyle(); }
    bool presentButInvisible() const { return widthForPainting() && !hasVisibleColorAndStyle(); }
    float widthForPainting() const { return m_isPresent ? m_width : 0; }

    float innerWidth() const { return m_innerWidth; }
    float outerWidth() const { return m_outerWidth; }

    bool obscuresBackground() const;
    bool obscuresBackgroundEdge(float scale) const;

private:
    Color m_color;
    float m_width { 0 };
    float m_innerWidth { 0 }; // Only used for double borders.
    float m_outerWidth { 0 }; // Only used for double borders.
    BorderStyle m_style { BorderStyle::Hidden };
    bool m_isTransparent { false };
    bool m_isPresent { false };
};

RectEdges<BorderEdge> calculateBorderEdges(const RenderStyle&, float pixelSnappingFactor, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);

std::pair<BoxSide, BoxSide> adjacentSidesForSide(BoxSide);

inline RectEdges<float> borderWidths(const RectEdges<BorderEdge>& edges)
{
    return {
        edges.top().width(),
        edges.right().width(),
        edges.bottom().width(),
        edges.left().width()
    };
}

// Per-box data with pixel-snapped geometry for background images and border-radius.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BoxDecorationData);
class BoxDecorationData {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(BoxDecorationData);
public:
    BoxDecorationData();

    const Vector<FillLayerImageGeometry, 1> backgroundImageGeometry() const { return m_backgroundImageGeometry; }
    void setBackgroundImageGeometry(Vector<FillLayerImageGeometry, 1>&& geometry) { m_backgroundImageGeometry = WTFMove(geometry); }

    const RectEdges<BorderEdge>& borderEdges() const { return m_borderEdges; }
    void setBorderEdges(RectEdges<BorderEdge>&& edges) { m_borderEdges = WTFMove(edges); }

    bool hasBorderImage() const { return false; } // FIXME: Implement border-image.
    bool hasBorder() const;

    bool borderObscuresBackground() const;
    bool borderObscuresBackgroundEdge(const FloatSize& contextScale) const;

private:
    Vector<FillLayerImageGeometry, 1> m_backgroundImageGeometry;
    RectEdges<BorderEdge> m_borderEdges;
};

} // namespace Display
} // namespace WebCore


