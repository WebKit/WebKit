/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "AffineTransform.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "RenderSVGBlock.h"

namespace WebCore {

class SVGForeignObjectElement;

// A <foreignObject> sits on the SVG/HTML boundary and therefore lives in two coordinate spaces at once:
//
//  - As an SVG graphics element its geometry is positioned by x/y, exactly like a <rect> or an <image>.
//    x/y is part of objectBoundingBox() (and so of getBBox(), and of everything resolved against
//    *Units="objectBoundingBox"), and paint() / nodeAtFloatPoint() only apply localTransform().
//  - As a CSS box hosting an HTML subtree its border box origin is at location(), which is set to x/y so
//    that the hosted content lays out in the right place. The RenderBox repaint and coordinate-mapping
//    machinery hands us rects in that space, i.e. relative to the border box origin and excluding x/y.
//
// The two spaces differ by exactly x/y, which is why localToParentTransform() carries the x/y translation
// (for repaintRectInLocalCoordinates() and for HTML descendants walking up through SVGRenderSupport) while
// the bounding boxes below carry x/y themselves. SVGRenderSupport maps them with localTransform() alone, so
// that x/y is not counted twice - see boundingBoxToParentTransform() in SVGRenderSupport.cpp.
class LegacyRenderSVGForeignObject final : public RenderSVGBlock {
    WTF_MAKE_TZONE_ALLOCATED(LegacyRenderSVGForeignObject);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LegacyRenderSVGForeignObject);
public:
    LegacyRenderSVGForeignObject(SVGForeignObjectElement&, Style::ComputedStyle&&);
    virtual ~LegacyRenderSVGForeignObject();

    SVGForeignObjectElement& NODELETE foreignObjectElement() const;

    void paint(PaintInfo&, const LayoutPoint&) override;

    bool requiresLayer() const override { return false; }
    void layout() override;

    FloatRect objectBoundingBox() const override { return m_viewport; }
    bool isObjectBoundingBoxValid() const { return !m_viewport.isEmpty(); }
    bool objectBoundingBoxIsEmpty() const final { return !isObjectBoundingBoxValid(); }
    FloatRect strokeBoundingBox() const override { return m_viewport; }
    // Unlike the bounding boxes, this one is in CSS box space - see the class comment.
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    FloatRect decoratedBoundingBox() const override { return m_viewport; }

    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;

    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

private:
    void graphicsElement() const = delete;
    ASCIILiteral renderName() const override { return "RenderSVGForeignObject"_s; }

    void updateLogicalWidth() override;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    const AffineTransform& localToParentTransform() const LIFETIME_BOUND override;
    AffineTransform localTransform() const override { return m_localTransform; }

    LayoutSize offsetFromContainer(const RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const override;

    AffineTransform m_localTransform;
    mutable AffineTransform m_localToParentTransform;
    FloatRect m_viewport;
    bool m_needsTransformUpdate { true };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(LegacyRenderSVGForeignObject, isLegacyRenderSVGForeignObject())
