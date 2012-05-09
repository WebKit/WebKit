/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Computer, Inc
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

#if ENABLE(SVG)
#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderSVGModelObject.h"
#include "SVGMarkerLayoutInfo.h"
#include "StrokeStyleApplier.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
class GraphicsContextStateSaver;
class RenderSVGContainer;
class RenderSVGPath;
class RenderSVGResource;
class SVGStyledTransformableElement;

class BoundingRectStrokeStyleApplier : public StrokeStyleApplier {
public:
    BoundingRectStrokeStyleApplier(const RenderObject* object, RenderStyle* style)
        : m_object(object)
        , m_style(style)
    {
        ASSERT(style);
        ASSERT(object);
    }

    void strokeStyle(GraphicsContext* context)
    {
        SVGRenderSupport::applyStrokeStyleToContext(context, m_style, m_object);
    }

private:
    const RenderObject* m_object;
    RenderStyle* m_style;
};

class RenderSVGShape : public RenderSVGModelObject {
public:
    explicit RenderSVGShape(SVGStyledTransformableElement*);
    RenderSVGShape(SVGStyledTransformableElement*, Path*, bool);
    virtual ~RenderSVGShape();

    void setNeedsShapeUpdate() { m_needsShapeUpdate = true; }
    virtual void setNeedsBoundariesUpdate() { m_needsBoundariesUpdate = true; }
    virtual void setNeedsTransformUpdate() { m_needsTransformUpdate = true; }
    virtual void fillShape(GraphicsContext*) const;
    virtual void strokeShape(GraphicsContext*) const;
    bool isPaintingFallback() const { return m_fillFallback; }

    Path& path() const
    {
        ASSERT(m_path);
        return *m_path;
    }

protected:
    virtual void createShape();
    virtual bool isEmpty() const;
    virtual FloatRect objectBoundingBox() const;
    virtual FloatRect strokeBoundingBox() const { return m_strokeAndMarkerBoundingBox; }
    void setStrokeAndMarkerBoundingBox(FloatRect rect) { m_strokeAndMarkerBoundingBox = rect; }
    virtual bool shapeDependentStrokeContains(const FloatPoint&) const;
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;
    float strokeWidth() const;
    void setIsPaintingFallback(bool isFallback) { m_fillFallback = isFallback; }
    FloatRect calculateMarkerBoundsIfNeeded();
    void processZeroLengthSubpaths();

    bool hasPath() const { return m_path.get(); }

private:
    // Hit-detection separated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true, const WindRule fillRule = RULE_NONZERO);
    bool strokeContains(const FloatPoint&, bool requiresStroke = true);

    virtual FloatRect repaintRectInLocalCoordinates() const { return m_repaintBoundingBox; }
    virtual const AffineTransform& localToParentTransform() const { return m_localTransform; }
    virtual AffineTransform localTransform() const { return m_localTransform; }

    virtual bool isSVGShape() const { return true; }
    virtual const char* renderName() const { return "RenderSVGShape"; }

    virtual void layout();
    virtual void paint(PaintInfo&, const LayoutPoint&);
    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint&);

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);

    void updateCachedBoundaries();

    Path* zeroLengthLinecapPath(const FloatPoint&);
    bool setupNonScalingStrokeTransform(AffineTransform&, GraphicsContextStateSaver&);
    Path* nonScalingStrokePath(const Path*, const AffineTransform&);
    bool shouldStrokeZeroLengthSubpath() const;
    FloatRect zeroLengthSubpathRect(const FloatPoint&, float) const;

    void fillShape(RenderStyle*, GraphicsContext*, Path*, RenderSVGShape*);
    void strokePath(RenderStyle*, GraphicsContext*, Path*, RenderSVGResource*,
                    const Color&, bool, const AffineTransform&, int);
    void fillAndStrokePath(GraphicsContext*);
    void inflateWithStrokeAndMarkerBounds();

private:
    FloatRect m_fillBoundingBox;
    FloatRect m_strokeAndMarkerBoundingBox;
    FloatRect m_repaintBoundingBox;
    SVGMarkerLayoutInfo m_markerLayoutInfo;
    AffineTransform m_localTransform;
    OwnPtr<Path> m_path;
    Vector<FloatPoint> m_zeroLengthLinecapLocations;

    bool m_needsBoundariesUpdate : 1;
    bool m_needsShapeUpdate : 1;
    bool m_needsTransformUpdate : 1;
    bool m_fillFallback : 1;
};

inline RenderSVGShape* toRenderSVGShape(RenderObject* object)
{
    ASSERT(!object || object->isSVGShape());
    return static_cast<RenderSVGShape*>(object);
}

inline const RenderSVGShape* toRenderSVGShape(const RenderObject* object)
{
    ASSERT(!object || object->isSVGShape());
    return static_cast<const RenderSVGShape*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderSVGShape(const RenderSVGShape*);

}

#endif // ENABLE(SVG)
#endif
