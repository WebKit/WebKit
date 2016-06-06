/*
 * Copyright (C) 2006 Apple Inc.
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

#ifndef RenderSVGForeignObject_h
#define RenderSVGForeignObject_h

#include "AffineTransform.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "RenderSVGBlock.h"

namespace WebCore {

class SVGForeignObjectElement;

class RenderSVGForeignObject final : public RenderSVGBlock {
public:
    RenderSVGForeignObject(SVGForeignObjectElement&, RenderStyle&&);
    virtual ~RenderSVGForeignObject();

    SVGForeignObjectElement& foreignObjectElement() const;

    void paint(PaintInfo&, const LayoutPoint&) override;

    LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override;
    FloatRect computeFloatRectForRepaint(const FloatRect&, const RenderLayerModelObject* repaintContainer, bool fixed = false) const override;
    LayoutRect computeRectForRepaint(const LayoutRect&, const RenderLayerModelObject* repaintContainer, RepaintContext = { }) const override;

    bool requiresLayer() const override { return false; }
    void layout() override;

    FloatRect objectBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    FloatRect strokeBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    FloatRect repaintRectInLocalCoordinates() const override { return FloatRect(FloatPoint(), m_viewport.size()); }

    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const override;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;
    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

private:
    bool isSVGForeignObject() const override { return true; }
    void graphicsElement() const = delete;
    const char* renderName() const override { return "RenderSVGForeignObject"; }

    void updateLogicalWidth() override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    const AffineTransform& localToParentTransform() const override;
    AffineTransform localTransform() const override { return m_localTransform; }

    bool m_needsTransformUpdate : 1;
    FloatRect m_viewport;
    AffineTransform m_localTransform;
    mutable AffineTransform m_localToParentTransform;
};

}

#endif
