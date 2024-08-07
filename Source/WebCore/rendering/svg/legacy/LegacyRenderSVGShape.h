/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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
#include "FloatRect.h"
#include "LegacyRenderSVGModelObject.h"
#include "SVGMarkerData.h"
#include <memory>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
class GraphicsContextStateSaver;
class SVGGraphicsElement;

class LegacyRenderSVGShape : public LegacyRenderSVGModelObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(LegacyRenderSVGShape);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LegacyRenderSVGShape);
public:
    friend FloatRect SVGRenderSupport::calculateApproximateStrokeBoundingBox(const RenderElement&);

    enum class ShapeType : uint8_t {
        Empty,
        Path,
        Line,
        Rectangle,
        RoundedRectangle,
        Ellipse,
        Circle,
    };

    enum PointCoordinateSpace {
        GlobalCoordinateSpace,
        LocalCoordinateSpace
    };
    LegacyRenderSVGShape(Type, SVGGraphicsElement&, RenderStyle&&);
    virtual ~LegacyRenderSVGShape();

    inline SVGGraphicsElement& graphicsElement() const;
    inline Ref<SVGGraphicsElement> protectedGraphicsElement() const;

    void setNeedsShapeUpdate() { m_needsShapeUpdate = true; }
    void setNeedsBoundariesUpdate() final { m_needsBoundariesUpdate = true; }
    void setNeedsTransformUpdate() final { m_needsTransformUpdate = true; }
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

    ShapeType shapeType() const { return m_shapeType; }

protected:
    void element() const = delete;

    Path& ensurePath();

    virtual void updateShapeFromElement() = 0;
    virtual bool isEmpty() const;
    virtual bool shapeDependentStrokeContains(const FloatPoint&, PointCoordinateSpace = GlobalCoordinateSpace);
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;
    float strokeWidth() const;

    inline bool hasNonScalingStroke() const;
    AffineTransform nonScalingStrokeTransform() const;
    Path* nonScalingStrokePath(const Path*, const AffineTransform&) const;

    virtual FloatRect adjustStrokeBoundingBoxForMarkersAndZeroLengthLinecaps(RepaintRectCalculation, FloatRect strokeBoundingBox) const { return strokeBoundingBox; }

    FloatRect strokeBoundingBox() const final;
    FloatRect approximateStrokeBoundingBox() const;

    bool fillRequiresClip() const { return m_fillRequiresClip; }

private:
    // Hit-detection separated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true, const WindRule fillRule = WindRule::NonZero);
    bool strokeContains(const FloatPoint&, bool requiresStroke = true);

    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final;
    const AffineTransform& localToParentTransform() const final { return m_localTransform; }
    AffineTransform localTransform() const final { return m_localTransform; }

    bool canHaveChildren() const final { return false; }
    ASCIILiteral renderName() const override { return "RenderSVGShape"_s; }

    void layout() final;
    void paint(PaintInfo&, const LayoutPoint&) final;
    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) const final;

    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) final;

    FloatRect objectBoundingBox() const final { return m_fillBoundingBox; }
    FloatRect calculateStrokeBoundingBox() const;
    void updateRepaintBoundingBox();

    bool setupNonScalingStrokeContext(AffineTransform&, GraphicsContextStateSaver&);

    FloatRect calculateApproximateStrokeBoundingBox() const;
    
    std::unique_ptr<Path> createPath() const;

    void fillShape(const RenderStyle&, GraphicsContext&);
    void strokeShapeInternal(const RenderStyle&, GraphicsContext&);
    void strokeShape(const RenderStyle&, GraphicsContext&);
    void fillStrokeMarkers(PaintInfo&);

    virtual void drawMarkers(PaintInfo&) { }

protected:
    FloatRect m_fillBoundingBox;
    mutable Markable<FloatRect, FloatRect::MarkableTraits> m_strokeBoundingBox;
    mutable Markable<FloatRect, FloatRect::MarkableTraits> m_approximateStrokeBoundingBox;
private:
    FloatRect m_repaintBoundingBox;

    bool m_needsBoundariesUpdate : 1;
    bool m_needsShapeUpdate : 1;
    bool m_needsTransformUpdate : 1;
    bool m_fillRequiresClip : 1 { true };
protected:
    ShapeType m_shapeType : 3 { ShapeType::Empty };
private:
    AffineTransform m_localTransform;
    std::unique_ptr<Path> m_path;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(LegacyRenderSVGShape, isLegacyRenderSVGShape())
