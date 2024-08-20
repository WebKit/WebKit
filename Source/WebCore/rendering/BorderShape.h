/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "RectEdges.h"
#include "RenderStyleConstants.h"
#include "RoundedRect.h"

namespace WebCore {

class Color;
class GraphicsContext;
class FloatRect;
class FloatRoundedRect;
class Path;
class RenderStyle;

// BorderShape is used to fill and clip to the shape formed by the border and padding boxes with border-radius.
// In future, this may be a more complex shape than a rounded rect, so accessors that return rounded rects
// are deprecated.
class BorderShape {
public:
    static BorderShape shapeForBorderRect(const RenderStyle&, const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    // overrideBorderWidths describe custom insets from the border box, used instead of the border widths from the style.
    static BorderShape shapeForBorderRect(const RenderStyle&, const LayoutRect& borderRect, const RectEdges<LayoutUnit>& overrideBorderWidths, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);

    BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths);
    BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const RoundedRectRadii&);

    RoundedRect deprecatedRoundedRect() const;
    RoundedRect deprecatedInnerRoundedRect() const;
    FloatRoundedRect deprecatedPixelSnappedRoundedRect(float deviceScaleFactor) const;
    FloatRoundedRect deprecatedPixelSnappedInnerRoundedRect(float deviceScaleFactor) const;

    const RoundedRectRadii& radii() const { return m_borderRect.radii(); }
    void setRadii(const RoundedRectRadii& radii) { m_borderRect.setRadii(radii); }

    FloatRect snappedOuterRect(float deviceScaleFactor) const;
    FloatRect snappedInnerRect(float deviceScaleFactor) const;

    bool isRounded() const { return m_borderRect.isRounded(); }

    Path pathForOuterShape(float deviceScaleFactor) const;
    Path pathForInnerShape(float deviceScaleFactor) const;

    Path pathForBorderArea(float deviceScaleFactor) const;

    void clipToOuterShape(GraphicsContext&, float deviceScaleFactor);
    void clipToInnerShape(GraphicsContext&, float deviceScaleFactor);
    void clipOutInnerShape(GraphicsContext&, float deviceScaleFactor);

    void fillOuterShape(GraphicsContext&, const Color&, float deviceScaleFactor);

private:
    RoundedRect innerEdgeRoundedRect() const;
    LayoutRect innerEdgeRect() const;

    RoundedRect m_borderRect;
    RectEdges<LayoutUnit> m_borderWidths;
};

} // namespace WebCore
