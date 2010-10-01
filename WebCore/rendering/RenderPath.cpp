/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2008 Rob Buis <buis@kde.org>
                  2005, 2007 Eric Seidel <eric@webkit.org>
                  2009 Google, Inc.
                  2009 Dirk Schulze <krit@webkit.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "RenderPath.h"

#include "FloatPoint.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "PointerEventsHitRules.h"
#include "RenderSVGContainer.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMarker.h"
#include "StrokeStyleApplier.h"
#include "SVGRenderSupport.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTransformList.h"
#include "SVGURIReference.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class BoundingRectStrokeStyleApplier : public StrokeStyleApplier {
public:
    BoundingRectStrokeStyleApplier(const RenderObject* object, RenderStyle* style)
        : m_object(object)
        , m_style(style)
    {
        ASSERT(style);
        ASSERT(object);
    }

    void strokeStyle(GraphicsContext* gc)
    {
        applyStrokeStyleToContext(gc, m_style, m_object);
    }

private:
    const RenderObject* m_object;
    RenderStyle* m_style;
};

RenderPath::RenderPath(SVGStyledTransformableElement* node)
    : RenderSVGModelObject(node)
    , m_needsBoundariesUpdate(false) // default is false, the cached rects are empty from the beginning
    , m_needsPathUpdate(true) // default is true, so we grab a Path object once from SVGStyledTransformableElement
    , m_needsTransformUpdate(true) // default is true, so we grab a AffineTransform object once from SVGStyledTransformableElement
{
}

bool RenderPath::fillContains(const FloatPoint& point, bool requiresFill) const
{
    if (!m_fillBoundingBox.contains(point))
        return false;

    if (requiresFill && !RenderSVGResource::fillPaintingResource(this, style()))
        return false;

    return m_path.contains(point, style()->svgStyle()->fillRule());
}

bool RenderPath::strokeContains(const FloatPoint& point, bool requiresStroke) const
{
    if (!m_strokeAndMarkerBoundingBox.contains(point))
        return false;

    if (requiresStroke && !RenderSVGResource::strokePaintingResource(this, style()))
        return false;

    BoundingRectStrokeStyleApplier strokeStyle(this, style());
    return m_path.strokeContains(&strokeStyle, point);
}

void RenderPath::layout()
{
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() && selfNeedsLayout());
    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(node());

    // We need to update the Path object whenever the underlying SVGStyledTransformableElement uses relative values
    // as the viewport size may have changed. It would be nice to optimize this to detect these changes, and only
    // update when needed, even when using relative values.
    bool needsPathUpdate = m_needsPathUpdate;
    if (!needsPathUpdate && element->hasRelativeValues())
        needsPathUpdate = true;

    if (needsPathUpdate) {
        m_path = element->toPathData();
        m_needsPathUpdate = false;
    }

    if (m_needsTransformUpdate) {
        m_localTransform = element->animatedLocalTransform();
        m_needsTransformUpdate = false;
    }

    // At this point LayoutRepainter already grabbed the old bounds,
    // recalculate them now so repaintAfterLayout() uses the new bounds
    if (needsPathUpdate || m_needsBoundariesUpdate) {
        updateCachedBoundaries();
        m_needsBoundariesUpdate = false;
    }

    repainter.repaintAfterLayout();
    setNeedsLayout(false);
}

static inline void fillAndStrokePath(const Path& path, GraphicsContext* context, RenderPath* object)
{
    context->beginPath();

    if (RenderSVGResource* fillPaintingResource = RenderSVGResource::fillPaintingResource(object, object->style())) {
        context->addPath(path);
        if (fillPaintingResource->applyResource(object, object->style(), context, ApplyToFillMode))
            fillPaintingResource->postApplyResource(object, context, ApplyToFillMode);
    }

    if (RenderSVGResource* strokePaintingResource = RenderSVGResource::strokePaintingResource(object, object->style())) {
        context->addPath(path);
        if (strokePaintingResource->applyResource(object, object->style(), context, ApplyToStrokeMode))
            strokePaintingResource->postApplyResource(object, context, ApplyToStrokeMode);
    }
}

void RenderPath::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled() || style()->visibility() == HIDDEN || m_path.isEmpty())
        return;

    FloatRect boundingBox = repaintRectInLocalCoordinates();
    FloatRect nonLocalBoundingBox = m_localTransform.mapRect(boundingBox);
    if (!nonLocalBoundingBox.intersects(paintInfo.rect))
        return;

    PaintInfo childPaintInfo(paintInfo);
    bool drawsOutline = style()->outlineWidth() && (childPaintInfo.phase == PaintPhaseOutline || childPaintInfo.phase == PaintPhaseSelfOutline);
    if (drawsOutline || childPaintInfo.phase == PaintPhaseForeground) {
        childPaintInfo.context->save();
        applyTransformToPaintInfo(childPaintInfo, m_localTransform);
        RenderSVGResourceFilter* filter = 0;

        if (childPaintInfo.phase == PaintPhaseForeground) {
            PaintInfo savedInfo(childPaintInfo);

            if (prepareToRenderSVGContent(this, childPaintInfo, boundingBox, filter)) {
                const SVGRenderStyle* svgStyle = style()->svgStyle();
                if (svgStyle->shapeRendering() == SR_CRISPEDGES)
                    childPaintInfo.context->setShouldAntialias(false);

                fillAndStrokePath(m_path, childPaintInfo.context, this);

                if (svgStyle->hasMarkers())
                    m_markerLayoutInfo.drawMarkers(childPaintInfo);
            }

            finishRenderSVGContent(this, childPaintInfo, filter, savedInfo.context);
        }

        if (drawsOutline)
            paintOutline(childPaintInfo.context, static_cast<int>(boundingBox.x()), static_cast<int>(boundingBox.y()),
                static_cast<int>(boundingBox.width()), static_cast<int>(boundingBox.height()));
        
        childPaintInfo.context->restore();
    }
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void RenderPath::addFocusRingRects(Vector<IntRect>& rects, int, int) 
{
    IntRect rect = enclosingIntRect(repaintRectInLocalCoordinates());
    if (!rect.isEmpty())
        rects.append(rect);
}

bool RenderPath::nodeAtFloatPoint(const HitTestRequest&, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint = m_localTransform.inverse().mapPoint(pointInParent);

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_PATH_HITTESTING, style()->pointerEvents());

    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (style()->svgStyle()->hasStroke() || !hitRules.requireStroke) && strokeContains(localPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (style()->svgStyle()->hasFill() || !hitRules.requireFill) && fillContains(localPoint, hitRules.requireFill))) {
            updateHitTestResult(result, roundedIntPoint(localPoint));
            return true;
        }
    }

    return false;
}

FloatRect RenderPath::calculateMarkerBoundsIfNeeded()
{
    Document* doc = document();

    SVGElement* svgElement = static_cast<SVGElement*>(node());
    ASSERT(svgElement && svgElement->document());
    if (!svgElement->isStyled())
        return FloatRect();

    SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(svgElement);
    if (!styledElement->supportsMarkers())
        return FloatRect();

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    ASSERT(svgStyle->hasMarkers());

    AtomicString startMarkerId(svgStyle->markerStartResource());
    AtomicString midMarkerId(svgStyle->markerMidResource());
    AtomicString endMarkerId(svgStyle->markerEndResource());

    RenderSVGResourceMarker* startMarker = getRenderSVGResourceById<RenderSVGResourceMarker>(doc, startMarkerId);
    RenderSVGResourceMarker* midMarker = getRenderSVGResourceById<RenderSVGResourceMarker>(doc, midMarkerId);
    RenderSVGResourceMarker* endMarker = getRenderSVGResourceById<RenderSVGResourceMarker>(doc, endMarkerId);

    if (!startMarker && !startMarkerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(startMarkerId, styledElement);
    else if (startMarker)
        startMarker->addClient(this);

    if (!midMarker && !midMarkerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(midMarkerId, styledElement);
    else if (midMarker)
        midMarker->addClient(this);

    if (!endMarker && !endMarkerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(endMarkerId, styledElement);
    else if (endMarker)
        endMarker->addClient(this);

    if (!startMarker && !midMarker && !endMarker)
        return FloatRect();

    float strokeWidth = SVGRenderStyle::cssPrimitiveToLength(this, svgStyle->strokeWidth(), 1.0f);
    return m_markerLayoutInfo.calculateBoundaries(startMarker, midMarker, endMarker, strokeWidth, m_path);
}

void RenderPath::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    setNeedsBoundariesUpdate();
    RenderSVGModelObject::styleWillChange(diff, newStyle);
}

void RenderPath::updateCachedBoundaries()
{
    if (m_path.isEmpty()) {
        m_fillBoundingBox = FloatRect();
        m_strokeAndMarkerBoundingBox = FloatRect();
        m_repaintBoundingBox = FloatRect();
        return;
    }

    // Cache _unclipped_ fill bounding box, used for calculations in resources
    m_fillBoundingBox = m_path.boundingRect();

    // Cache _unclipped_ stroke bounding box, used for calculations in resources (includes marker boundaries)
    m_strokeAndMarkerBoundingBox = m_fillBoundingBox;

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    if (svgStyle->hasStroke()) {
        BoundingRectStrokeStyleApplier strokeStyle(this, style());
        m_strokeAndMarkerBoundingBox.unite(m_path.strokeBoundingRect(&strokeStyle));
    }

    if (svgStyle->hasMarkers()) {
        FloatRect markerBounds = calculateMarkerBoundsIfNeeded();
        if (!markerBounds.isEmpty())
            m_strokeAndMarkerBoundingBox.unite(markerBounds);
    }

    // Cache smallest possible repaint rectangle
    m_repaintBoundingBox = m_strokeAndMarkerBoundingBox;
    intersectRepaintRectWithResources(this, m_repaintBoundingBox);
}

}

#endif // ENABLE(SVG)
