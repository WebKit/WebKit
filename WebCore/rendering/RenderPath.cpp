/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005, 2007 Eric Seidel <eric@webkit.org>

    This file is part of the KDE project

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

#include <math.h>

#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "PointerEventsHitRules.h"
#include "RenderSVGContainer.h"
#include "SVGPaintServer.h"
#include "SVGRenderSupport.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMarker.h"
#include "SVGResourceMasker.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTransformList.h"
#include "SVGURIReference.h"

#include <wtf/MathExtras.h>

namespace WebCore {

// RenderPath
RenderPath::RenderPath(RenderStyle* style, SVGStyledTransformableElement* node)
    : RenderObject(node)
{
    ASSERT(style != 0);
    ASSERT(static_cast<SVGElement*>(node)->isStyledTransformable());
}

RenderPath::~RenderPath()
{
}

AffineTransform RenderPath::localTransform() const
{
    return m_localTransform;
}

FloatPoint RenderPath::mapAbsolutePointToLocal(const FloatPoint& point) const
{
    // FIXME: does it make sense to map incoming points with the inverse of the
    // absolute transform? 
    double localX;
    double localY;
    absoluteTransform().inverse().map(point.x(), point.y(), &localX, &localY);
    return FloatPoint::narrowPrecision(localX, localY);
}

bool RenderPath::fillContains(const FloatPoint& point, bool requiresFill) const
{
    if (m_path.isEmpty())
        return false;

    if (requiresFill && !SVGPaintServer::fillPaintServer(style(), this))
        return false;

    return m_path.contains(point, style()->svgStyle()->fillRule());
}

FloatRect RenderPath::relativeBBox(bool includeStroke) const
{
    if (m_path.isEmpty())
        return FloatRect();

    if (includeStroke) {
        if (m_strokeBbox.isEmpty())
            m_strokeBbox = strokeBBox();

        return m_strokeBbox;
    }

    if (m_fillBBox.isEmpty())
        m_fillBBox = m_path.boundingRect();

    return m_fillBBox;
}

void RenderPath::setPath(const Path& newPath)
{
    m_path = newPath;
    m_strokeBbox = FloatRect();
    m_fillBBox = FloatRect();
}

const Path& RenderPath::path() const
{
    return m_path;
}

bool RenderPath::calculateLocalTransform()
{
    AffineTransform oldTransform = m_localTransform;
    m_localTransform = static_cast<SVGStyledTransformableElement*>(element())->animatedLocalTransform();
    return (m_localTransform != oldTransform);
}

void RenderPath::layout()
{
    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout() && selfNeedsLayout();
    if (checkForRepaint) {
        oldBounds = m_absoluteBounds;
        oldOutlineBox = absoluteOutlineBox();
    }
        
    calculateLocalTransform();

    setPath(static_cast<SVGStyledTransformableElement*>(element())->toPathData());

    m_absoluteBounds = absoluteClippedOverflowRect();

    setWidth(m_absoluteBounds.width());
    setHeight(m_absoluteBounds.height());

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);

    setNeedsLayout(false);
}

IntRect RenderPath::absoluteClippedOverflowRect()
{
    FloatRect repaintRect = absoluteTransform().mapRect(relativeBBox(true));

    // Markers can expand the bounding box
    repaintRect.unite(m_markerBounds);

#if ENABLE(SVG_FILTERS)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));
#endif

    if (!repaintRect.isEmpty())
        repaintRect.inflate(1); // inflate 1 pixel for antialiasing

    return enclosingIntRect(repaintRect);
}

bool RenderPath::requiresLayer()
{
    return false;
}

short RenderPath::lineHeight(bool b, bool isRootLineBox) const
{
    return static_cast<short>(relativeBBox(true).height());
}

short RenderPath::baselinePosition(bool b, bool isRootLineBox) const
{
    return static_cast<short>(relativeBBox(true).height());
}

static inline void fillAndStrokePath(const Path& path, GraphicsContext* context, RenderStyle* style, RenderPath* object)
{
    context->beginPath();

    SVGPaintServer* fillPaintServer = SVGPaintServer::fillPaintServer(style, object);
    if (fillPaintServer) {
        context->addPath(path);
        fillPaintServer->draw(context, object, ApplyToFillTargetType);
    }
    
    SVGPaintServer* strokePaintServer = SVGPaintServer::strokePaintServer(style, object);
    if (strokePaintServer) {
        context->addPath(path); // path is cleared when filled.
        strokePaintServer->draw(context, object, ApplyToStrokeTargetType);
    }
}

void RenderPath::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled() || style()->visibility() == HIDDEN || m_path.isEmpty())
        return;
            
    paintInfo.context->save();
    paintInfo.context->concatCTM(localTransform());

    SVGResourceFilter* filter = 0;

    FloatRect boundingBox = relativeBBox(true);
    if (paintInfo.phase == PaintPhaseForeground) {
        PaintInfo savedInfo(paintInfo);

        prepareToRenderSVGContent(this, paintInfo, boundingBox, filter);
        if (style()->svgStyle()->shapeRendering() == SR_CRISPEDGES)
            paintInfo.context->setUseAntialiasing(false);
        fillAndStrokePath(m_path, paintInfo.context, style(), this);

        if (static_cast<SVGStyledElement*>(element())->supportsMarkers())
            m_markerBounds = drawMarkersIfNeeded(paintInfo.context, paintInfo.rect, m_path);

        finishRenderSVGContent(this, paintInfo, boundingBox, filter, savedInfo.context);
    }

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth())
        paintOutline(paintInfo.context, static_cast<int>(boundingBox.x()), static_cast<int>(boundingBox.y()),
            static_cast<int>(boundingBox.width()), static_cast<int>(boundingBox.height()), style());
    
    paintInfo.context->restore();
}

void RenderPath::addFocusRingRects(GraphicsContext* graphicsContext, int, int) 
{
    graphicsContext->addFocusRingRect(enclosingIntRect(relativeBBox(true)));
}

void RenderPath::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(absoluteClippedOverflowRect());
}

bool RenderPath::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int, int, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;
    
    IntPoint absolutePoint(_x, _y);

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_PATH_HITTESTING, style()->svgStyle()->pointerEvents());

    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        FloatPoint hitPoint = mapAbsolutePointToLocal(absolutePoint);
        if ((hitRules.canHitStroke && (style()->svgStyle()->hasStroke() || !hitRules.requireStroke) && strokeContains(hitPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (style()->svgStyle()->hasFill() || !hitRules.requireFill) && fillContains(hitPoint, hitRules.requireFill))) {
            updateHitTestResult(result, absolutePoint);
            return true;
        }
    }

    return false;
}

enum MarkerType {
    Start,
    Mid,
    End
};

struct MarkerData {
    FloatPoint origin;
    FloatPoint subpathStart;
    double strokeWidth;
    FloatPoint inslopePoints[2];
    FloatPoint outslopePoints[2];
    MarkerType type;
    SVGResourceMarker* marker;
};

struct DrawMarkersData {
    DrawMarkersData(GraphicsContext*, SVGResourceMarker* startMarker, SVGResourceMarker* midMarker, double strokeWidth);
    GraphicsContext* context;
    int elementIndex;
    MarkerData previousMarkerData;
    SVGResourceMarker* midMarker;
};

DrawMarkersData::DrawMarkersData(GraphicsContext* c, SVGResourceMarker *start, SVGResourceMarker *mid, double strokeWidth)
    : context(c)
    , elementIndex(0)
    , midMarker(mid)
{
    previousMarkerData.origin = FloatPoint();
    previousMarkerData.subpathStart = FloatPoint();
    previousMarkerData.strokeWidth = strokeWidth;
    previousMarkerData.marker = start;
    previousMarkerData.type = Start;
}

static void drawMarkerWithData(GraphicsContext* context, MarkerData &data)
{
    if (!data.marker)
        return;

    FloatPoint inslopeChange = data.inslopePoints[1] - FloatSize(data.inslopePoints[0].x(), data.inslopePoints[0].y());
    FloatPoint outslopeChange = data.outslopePoints[1] - FloatSize(data.outslopePoints[0].x(), data.outslopePoints[0].y());

    double inslope = rad2deg(atan2(inslopeChange.y(), inslopeChange.x()));
    double outslope = rad2deg(atan2(outslopeChange.y(), outslopeChange.x()));

    double angle = 0.0;
    switch (data.type) {
        case Start:
            angle = outslope;
            break;
        case Mid:
            angle = (inslope + outslope) / 2;
            break;
        case End:
            angle = inslope;
    }

    data.marker->draw(context, FloatRect(), data.origin.x(), data.origin.y(), data.strokeWidth, angle);
}

static inline void updateMarkerDataForElement(MarkerData& previousMarkerData, const PathElement* element)
{
    FloatPoint* points = element->points;
    
    switch (element->type) {
    case PathElementAddQuadCurveToPoint:
        // TODO
        previousMarkerData.origin = points[1];
        break;
    case PathElementAddCurveToPoint:
        previousMarkerData.inslopePoints[0] = points[1];
        previousMarkerData.inslopePoints[1] = points[2];
        previousMarkerData.origin = points[2];
        break;
    case PathElementMoveToPoint:
        previousMarkerData.subpathStart = points[0];
    case PathElementAddLineToPoint:
        previousMarkerData.inslopePoints[0] = previousMarkerData.origin;
        previousMarkerData.inslopePoints[1] = points[0];
        previousMarkerData.origin = points[0];
        break;
    case PathElementCloseSubpath:
        previousMarkerData.inslopePoints[0] = previousMarkerData.origin;
        previousMarkerData.inslopePoints[1] = points[0];
        previousMarkerData.origin = previousMarkerData.subpathStart;
        previousMarkerData.subpathStart = FloatPoint();
    }
}

static void drawStartAndMidMarkers(void* info, const PathElement* element)
{
    DrawMarkersData& data = *reinterpret_cast<DrawMarkersData*>(info);

    int elementIndex = data.elementIndex;
    MarkerData& previousMarkerData = data.previousMarkerData;

    FloatPoint* points = element->points;

    // First update the outslope for the previous element
    previousMarkerData.outslopePoints[0] = previousMarkerData.origin;
    previousMarkerData.outslopePoints[1] = points[0];

    // Draw the marker for the previous element
    if (elementIndex != 0)
        drawMarkerWithData(data.context, previousMarkerData);

    // Update our marker data for this element
    updateMarkerDataForElement(previousMarkerData, element);

    if (elementIndex == 1) {
        // After drawing the start marker, switch to drawing mid markers
        previousMarkerData.marker = data.midMarker;
        previousMarkerData.type = Mid;
    }

    data.elementIndex++;
}

FloatRect RenderPath::drawMarkersIfNeeded(GraphicsContext* context, const FloatRect& rect, const Path& path) const
{
    Document* doc = document();

    SVGElement* svgElement = static_cast<SVGElement*>(element());
    ASSERT(svgElement && svgElement->document() && svgElement->isStyled());

    SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(svgElement);
    const SVGRenderStyle* svgStyle = style()->svgStyle();

    AtomicString startMarkerId(SVGURIReference::getTarget(svgStyle->startMarker()));
    AtomicString midMarkerId(SVGURIReference::getTarget(svgStyle->midMarker()));
    AtomicString endMarkerId(SVGURIReference::getTarget(svgStyle->endMarker()));

    SVGResourceMarker* startMarker = getMarkerById(doc, startMarkerId);
    SVGResourceMarker* midMarker = getMarkerById(doc, midMarkerId);
    SVGResourceMarker* endMarker = getMarkerById(doc, endMarkerId);

    if (!startMarker && !startMarkerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(startMarkerId, styledElement);
    else if (startMarker)
        startMarker->addClient(styledElement);

    if (!midMarker && !midMarkerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(midMarkerId, styledElement);
    else if (midMarker)
        midMarker->addClient(styledElement);

    if (!endMarker && !endMarkerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(endMarkerId, styledElement);
    else if (endMarker)
        endMarker->addClient(styledElement);

    if (!startMarker && !midMarker && !endMarker)
        return FloatRect();

    double strokeWidth = SVGRenderStyle::cssPrimitiveToLength(this, svgStyle->strokeWidth(), 1.0f);
    DrawMarkersData data(context, startMarker, midMarker, strokeWidth);

    path.apply(&data, drawStartAndMidMarkers);

    data.previousMarkerData.marker = endMarker;
    data.previousMarkerData.type = End;
    drawMarkerWithData(context, data.previousMarkerData);

    // We know the marker boundaries, only after they're drawn!
    // Otherwhise we'd need to do all the marker calculation twice
    // once here (through paint()) and once in absoluteClippedOverflowRect().
    FloatRect bounds;

    if (startMarker)
        bounds.unite(startMarker->cachedBounds());

    if (midMarker)
        bounds.unite(midMarker->cachedBounds());

    if (endMarker)
        bounds.unite(endMarker->cachedBounds());

    return bounds;
}

}

#endif // ENABLE(SVG)
