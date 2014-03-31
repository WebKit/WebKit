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

#ifndef RenderSVGShape_h
#define RenderSVGShape_h

#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderSVGModelObject.h"
#include "SVGGraphicsElement.h"
#include "SVGMarkerData.h"
#include <memory>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
class GraphicsContextStateSaver;
class RenderSVGContainer;
class RenderSVGPath;
class RenderSVGResource;
class SVGGraphicsElement;

class RenderSVGShape : public RenderSVGModelObject {
public:
    RenderSVGShape(SVGGraphicsElement&, PassRef<RenderStyle>);
    RenderSVGShape(SVGGraphicsElement&, PassRef<RenderStyle>, Path*, bool);
    virtual ~RenderSVGShape();

    SVGGraphicsElement& graphicsElement() const { return toSVGGraphicsElement(RenderSVGModelObject::element()); }

    void setNeedsShapeUpdate() { m_needsShapeUpdate = true; }
    virtual void setNeedsBoundariesUpdate() override final { m_needsBoundariesUpdate = true; }
    virtual bool needsBoundariesUpdate() override final { return m_needsBoundariesUpdate; }
    virtual void setNeedsTransformUpdate() override final { m_needsTransformUpdate = true; }
    virtual void fillShape(GraphicsContext*) const;
    virtual void strokeShape(GraphicsContext*) const;

    bool hasPath() const { return m_path.get(); }
    Path& path() const
    {
        ASSERT(m_path);
        return *m_path;
    }

protected:
    void element() const = delete;

    virtual void updateShapeFromElement();
    virtual bool isEmpty() const override;
    virtual bool shapeDependentStrokeContains(const FloatPoint&);
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;
    float strokeWidth() const;
    bool hasSmoothStroke() const;

    bool hasNonScalingStroke() const { return style().svgStyle().vectorEffect() == VE_NON_SCALING_STROKE; }
    AffineTransform nonScalingStrokeTransform() const;
    Path* nonScalingStrokePath(const Path*, const AffineTransform&) const;

    FloatRect m_fillBoundingBox;
    FloatRect m_strokeBoundingBox;

private:
    // Hit-detection separated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true, const WindRule fillRule = RULE_NONZERO);
    bool strokeContains(const FloatPoint&, bool requiresStroke = true);

    virtual FloatRect repaintRectInLocalCoordinates() const override final { return m_repaintBoundingBox; }
    virtual FloatRect repaintRectInLocalCoordinatesExcludingSVGShadow() const override final { return m_repaintBoundingBoxExcludingShadow; }
    virtual const AffineTransform& localToParentTransform() const override final { return m_localTransform; }
    virtual AffineTransform localTransform() const override final { return m_localTransform; }

    virtual bool isSVGShape() const override final { return true; }
    virtual bool canHaveChildren() const override final { return false; }
    virtual const char* renderName() const override { return "RenderSVGShape"; }

    virtual void layout() override final;
    virtual void paint(PaintInfo&, const LayoutPoint&) override final;
    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override final;

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override final;

    virtual FloatRect objectBoundingBox() const override final { return m_fillBoundingBox; }
    virtual FloatRect strokeBoundingBox() const override final { return m_strokeBoundingBox; }
    FloatRect calculateObjectBoundingBox() const;
    FloatRect calculateStrokeBoundingBox() const;
    void updateRepaintBoundingBox();

    bool setupNonScalingStrokeContext(AffineTransform&, GraphicsContextStateSaver&);

    bool shouldGenerateMarkerPositions() const;
    FloatRect markerRect(float strokeWidth) const;
    void processMarkerPositions();

    void fillShape(const RenderStyle&, GraphicsContext*);
    void strokeShape(const RenderStyle&, GraphicsContext*);
    void strokeShape(GraphicsContext*);
    void fillStrokeMarkers(PaintInfo&);
    void drawMarkers(PaintInfo&);

private:
    FloatRect m_repaintBoundingBox;
    FloatRect m_repaintBoundingBoxExcludingShadow;
    AffineTransform m_localTransform;
    std::unique_ptr<Path> m_path;
    Vector<MarkerPosition> m_markerPositions;

    bool m_needsBoundariesUpdate : 1;
    bool m_needsShapeUpdate : 1;
    bool m_needsTransformUpdate : 1;
};

RENDER_OBJECT_TYPE_CASTS(RenderSVGShape, isSVGShape())

}

#endif
