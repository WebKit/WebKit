/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderSVGModelObject.h"
#include "SVGBoundingBoxComputation.h"
#include "SVGGraphicsElement.h"
#include "SVGMarkerData.h"
#include <memory>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
class GraphicsContextStateSaver;
class RenderSVGResource;
class SVGGraphicsElement;

class RenderSVGShape : public RenderSVGModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGShape);
public:
    enum PointCoordinateSpace {
        GlobalCoordinateSpace,
        LocalCoordinateSpace
    };
    RenderSVGShape(SVGGraphicsElement&, RenderStyle&&);
    virtual ~RenderSVGShape();

    inline SVGGraphicsElement& graphicsElement() const;

    void setNeedsShapeUpdate() { m_needsShapeUpdate = true; }

    virtual void fillShape(GraphicsContext&) const;
    virtual void strokeShape(GraphicsContext&) const;
    virtual bool isRenderingDisabled() const = 0;

    bool isPointInFill(const FloatPoint&);
    bool isPointInStroke(const FloatPoint&);

    float getTotalLength() const;
    FloatPoint getPointAtLength(float distance) const;

    bool hasPath() const { return m_path.get(); }
    Path& path() const
    {
        ASSERT(m_path);
        return *m_path;
    }
    void clearPath() { m_path = nullptr; }

    FloatRect objectBoundingBox() const final { return m_fillBoundingBox; }
    FloatRect strokeBoundingBox() const final { return m_strokeBoundingBox; }
    FloatRect repaintRectInLocalCoordinates() const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }

    FloatRect computeMarkerBoundingBox(const SVGBoundingBoxComputation::DecorationOptions&) const;

    bool needsHasSVGTransformFlags() const final;

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> = RenderStyle::allTransformOperations) const final;

protected:
    void element() const = delete;

    virtual void updateShapeFromElement();
    virtual bool isEmpty() const;
    virtual bool shapeDependentStrokeContains(const FloatPoint&, PointCoordinateSpace = GlobalCoordinateSpace);
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;
    float strokeWidth() const;
    bool hasSmoothStroke() const;

    bool hasNonScalingStroke() const { return style().svgStyle().vectorEffect() == VectorEffect::NonScalingStroke; }
    AffineTransform nonScalingStrokeTransform() const;
    Path* nonScalingStrokePath(const Path*, const AffineTransform&) const;

    FloatRect m_fillBoundingBox;
    FloatRect m_strokeBoundingBox;

private:
    // Hit-detection separated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true, const WindRule fillRule = WindRule::NonZero);
    bool strokeContains(const FloatPoint&, bool requiresStroke = true);

    bool isSVGShape() const final { return true; }
    bool canHaveChildren() const final { return false; }
    ASCIILiteral renderName() const override { return "RenderSVGShape"_s; }

    void layout() final;
    void paint(PaintInfo&, const LayoutPoint&) final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    FloatRect calculateObjectBoundingBox() const;
    FloatRect calculateStrokeBoundingBox() const;

    bool setupNonScalingStrokeContext(AffineTransform&, GraphicsContextStateSaver&);

    bool shouldGenerateMarkerPositions() const;
    
    std::unique_ptr<Path> createPath() const;
    void processMarkerPositions();

    void fillShape(const RenderStyle&, GraphicsContext&);
    void strokeShape(const RenderStyle&, GraphicsContext&);
    void strokeShape(GraphicsContext&);
    void fillStrokeMarkers(PaintInfo&);
    void drawMarkers(PaintInfo&);

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;

private:
    bool m_needsShapeUpdate { true };

    std::unique_ptr<Path> m_path;
    Vector<MarkerPosition> m_markerPositions;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGShape, isSVGShape())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
