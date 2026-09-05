/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BorderShape.h"
#include "FloatRoundedRect.h"
#include "LayoutShape.h"
#include "RenderStyleConstants.h"

namespace WebCore {

class RenderBox;

struct BoxShapeGeometry {
    enum class Edge : bool { Outer, Inner };

    BorderShape borderShape;
    Edge edge { Edge::Outer };

    LayoutRoundedRect roundedRect() const;
    // Empty when every corner is round, in which case roundedRect() describes the shape.
    Vector<FloatPoint> contour() const;
};

BoxShapeGeometry computeGeometryForBoxShape(CSSBoxType, const RenderBox&);
LayoutRoundedRect computeRoundedRectForBoxShape(CSSBoxType, const RenderBox&);

class BoxLayoutShape final : public LayoutShape {
public:
    BoxLayoutShape(const FloatRoundedRect& bounds, Vector<FloatPoint>&& contour = { })
        : m_bounds(bounds)
        , m_contour(WTF::move(contour))
    {
    }

    LayoutRect shapeMarginLogicalBoundingBox() const override;
    bool isEmpty() const override { return m_bounds.isEmpty(); }
    LineSegment getExcludedInterval(LayoutUnit logicalTop, LayoutUnit logicalHeight) const override;

    void buildDisplayPaths(DisplayPaths&) const override;

private:
    FloatRoundedRect shapeMarginBounds() const;

    FloatRoundedRect m_bounds;
    // Implicitly closed; see BorderShape::outerShapeAsPolygon().
    Vector<FloatPoint> m_contour;
};

} // namespace WebCore
