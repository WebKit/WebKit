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

#include "LayoutRoundedRect.h"
#include "RectEdges.h"
#include "RenderStyleConstants.h"

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
    static BorderShape shapeForBorderRect(const RenderStyle&, const LayoutRect& borderRect, RectEdges<bool> closedEdges = { true });
    // overrideBorderWidths describe custom insets from the border box, used instead of the border widths from the style.
    static BorderShape shapeForBorderRect(const RenderStyle&, const LayoutRect& borderRect, const RectEdges<LayoutUnit>& overrideBorderWidths, RectEdges<bool> closedEdges = { true });

    // Create a BorderShape suitable for rendering an outline or outset shadow. borderRect is provided to allow for scaling the corner radii.
    static BorderShape shapeForOutsetRect(const RenderStyle&, const LayoutRect& borderRect, const LayoutRect& outlineBoxRect, const RectEdges<LayoutUnit>& outlineWidths, RectEdges<bool> closedEdges = { true });

    // Create a BorderShape suitable for rendering a shape inset from the box. borderRect is provided to allow for scaling the corner radii.
    static BorderShape shapeForInsetRect(const RenderStyle&, const LayoutRect& borderRect, const LayoutRect& insetRect);

    BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths);
    BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const LayoutRoundedRectRadii&);

    BorderShape(const BorderShape&) = default;

    BorderShape shapeWithBorderWidths(const RectEdges<LayoutUnit>&) const;

    LayoutRect borderRect() const { return m_borderRect.rect(); }
    LayoutRect innerEdgeRect() const { return m_innerEdgeRect.rect(); }

    // Takes `closedEdges` into account.
    const RectEdges<LayoutUnit>& borderWidths() const { return m_borderWidths; }

    LayoutRoundedRect deprecatedRoundedRect() const;
    LayoutRoundedRect deprecatedInnerRoundedRect() const;
    FloatRoundedRect deprecatedPixelSnappedRoundedRect(float deviceScaleFactor) const;
    FloatRoundedRect deprecatedPixelSnappedInnerRoundedRect(float deviceScaleFactor) const;

    // Returns true if the given rect is entirely inside the shape, without impinging on any of the corners.
    bool innerShapeContains(const LayoutRect&) const;
    bool outerShapeContains(const LayoutRect&) const;

    const LayoutRoundedRectRadii& radii() const { return m_borderRect.radii(); }
    void setRadii(const LayoutRoundedRectRadii& radii) { m_borderRect.setRadii(radii); }

    // Note that the inner edge isn't necessarily a rounded rect, but the radii still represent where the straight edge sections terminate.
    const LayoutRoundedRectRadii& innerEdgeRadii() const { return m_innerEdgeRect.radii(); }

    FloatRect snappedOuterRect(float deviceScaleFactor) const;
    FloatRect snappedInnerRect(float deviceScaleFactor) const;

    bool isRounded() const { return m_borderRect.isRounded(); }

    bool outerShapeIsRectangular() const;
    bool innerShapeIsRectangular() const;

    bool isEmpty() const { return m_borderRect.rect().isEmpty(); }

    void move(LayoutSize);

    // This will inflate the m_borderRect, and scale the radii up accordingly. Note that this changes the meaning of "inner shape" which will no longer correspond to the padding box.
    void inflate(LayoutUnit);

    Path pathForOuterShape(float deviceScaleFactor) const;
    Path pathForInnerShape(float deviceScaleFactor) const;

    void addOuterShapeToPath(Path&, float deviceScaleFactor) const;
    void addInnerShapeToPath(Path&, float deviceScaleFactor) const;

    Path pathForBorderArea(float deviceScaleFactor) const;

    void clipToOuterShape(GraphicsContext&, float deviceScaleFactor) const;
    void clipToInnerShape(GraphicsContext&, float deviceScaleFactor) const;

    void clipOutOuterShape(GraphicsContext&, float deviceScaleFactor) const;
    void clipOutInnerShape(GraphicsContext&, float deviceScaleFactor) const;

    void fillOuterShape(GraphicsContext&, const Color&, float deviceScaleFactor) const;
    void fillInnerShape(GraphicsContext&, const Color&, float deviceScaleFactor) const;

    void fillRectWithInnerHoleShape(GraphicsContext&, const LayoutRect& outerRect, const Color&, float deviceScaleFactor) const;

private:
    static LayoutRoundedRect computeInnerEdgeRoundedRect(const LayoutRoundedRect& borderRoundedRect, const RectEdges<LayoutUnit>& borderWidths);

    LayoutRoundedRect m_borderRect;
    LayoutRoundedRect m_innerEdgeRect;
    RectEdges<LayoutUnit> m_borderWidths;
};

} // namespace WebCore
