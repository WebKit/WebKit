/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2008 Rob Buis <buis@kde.org>
                  2005, 2007 Eric Seidel <eric@webkit.org>
                  2009 Google, Inc.
                  2009 Dirk Schulze <krit@webkit.org>

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
#include "SVGPaintServer.h"
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
    , m_needsBoundariesUpdate(false) // default is false, as this is only used when a RenderSVGResource tells us that the boundaries need to be recached
    , m_needsPathUpdate(true) // default is true, so we grab a Path object once from SVGStyledTransformableElement
    , m_needsTransformUpdate(true) // default is true, so we grab a AffineTransform object once from SVGStyledTransformableElement
{
}

bool RenderPath::fillContains(const FloatPoint& point, bool requiresFill) const
{
    if (m_path.isEmpty())
        return false;

    if (requiresFill && !SVGPaintServer::fillPaintServer(style(), this))
        return false;

    return m_path.contains(point, style()->svgStyle()->fillRule());
}

bool RenderPath::strokeContains(const FloatPoint& point, bool requiresStroke) const
{
    if (m_path.isEmpty())
        return false;

    if (requiresStroke && !SVGPaintServer::strokePaintServer(style(), this))
        return false;

    BoundingRectStrokeStyleApplier strokeStyle(this, style());
    return m_path.strokeContains(&strokeStyle, point);
}

FloatRect RenderPath::objectBoundingBox() const
{
    if (m_path.isEmpty())
        return FloatRect();

    if (m_cachedLocalFillBBox.isEmpty())
        m_cachedLocalFillBBox = m_path.boundingRect();

    return m_cachedLocalFillBBox;
}

FloatRect RenderPath::strokeBoundingBox() const
{
    if (m_path.isEmpty())
        return FloatRect();

    if (!m_cachedLocalStrokeBBox.isEmpty())
        return m_cachedLocalStrokeBBox;

    m_cachedLocalStrokeBBox = objectBoundingBox();
    if (style()->svgStyle()->hasStroke()) {
        BoundingRectStrokeStyleApplier strokeStyle(this, style());
        m_cachedLocalStrokeBBox.unite(m_path.strokeBoundingRect(&strokeStyle));
    }

    return m_cachedLocalStrokeBBox;
}

FloatRect RenderPath::markerBoundingBox() const
{
    if (m_path.isEmpty())
        return FloatRect();

    if (m_cachedLocalMarkerBBox.isEmpty())
        calculateMarkerBoundsIfNeeded();

    return m_cachedLocalMarkerBBox;
}

FloatRect RenderPath::repaintRectInLocalCoordinates() const
{
    if (m_path.isEmpty())
        return FloatRect();

    // If we already have a cached repaint rect, return that
    if (!m_cachedLocalRepaintRect.isEmpty())
        return m_cachedLocalRepaintRect;

    // FIXME: We need to be careful here. We assume that there is no filter,
    // clipper, marker or masker if the rects are empty.
    FloatRect rect = filterBoundingBoxForRenderer(this);
    if (!rect.isEmpty())
        m_cachedLocalRepaintRect = rect;
    else {
        m_cachedLocalRepaintRect = strokeBoundingBox();
        m_cachedLocalRepaintRect.unite(markerBoundingBox());
    }

    rect = clipperBoundingBoxForRenderer(this);
    if (!rect.isEmpty())
        m_cachedLocalRepaintRect.intersect(rect);

    rect = maskerBoundingBoxForRenderer(this);
    if (!rect.isEmpty())
        m_cachedLocalRepaintRect.intersect(rect);

    style()->svgStyle()->inflateForShadow(m_cachedLocalRepaintRect);

    return m_cachedLocalRepaintRect;
}

void RenderPath::layout()
{
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() && selfNeedsLayout());
    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(node());

    // We need to update the Path object whenever the underlying SVGStyledTransformableElement uses relative values
    // as the viewport size may have changed. It would be nice to optimize this to detect these changes, and only
    // update when needed, even when using relative values.
    if (!m_needsPathUpdate && element->hasRelativeValues())
        m_needsPathUpdate = true;

    bool needsUpdate = m_needsPathUpdate || m_needsTransformUpdate || m_needsBoundariesUpdate;

    if (m_needsBoundariesUpdate)
        m_needsBoundariesUpdate = false;

    if (m_needsPathUpdate) {
        m_path = element->toPathData();
        m_needsPathUpdate = false;
    }

    if (m_needsTransformUpdate) {
        m_localTransform = element->animatedLocalTransform();
        m_needsTransformUpdate = false;
    }

    if (needsUpdate)
        invalidateCachedBoundaries();

    repainter.repaintAfterLayout();
    setNeedsLayout(false);
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

    FloatRect boundingBox = repaintRectInLocalCoordinates();
    FloatRect nonLocalBoundingBox = m_localTransform.mapRect(boundingBox);
    // FIXME: The empty rect check is to deal with incorrect initial clip in renderSubtreeToImage
    // unfortunately fixing that problem is fairly complex unless we were willing to just futz the
    // rect to something "close enough"
    if (!nonLocalBoundingBox.intersects(paintInfo.rect) && !paintInfo.rect.isEmpty())
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
                if (style()->svgStyle()->shapeRendering() == SR_CRISPEDGES)
                    childPaintInfo.context->setShouldAntialias(false);
                fillAndStrokePath(m_path, childPaintInfo.context, style(), this);

                if (static_cast<SVGStyledElement*>(node())->supportsMarkers())
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

void RenderPath::calculateMarkerBoundsIfNeeded() const
{
    Document* doc = document();

    SVGElement* svgElement = static_cast<SVGElement*>(node());
    ASSERT(svgElement && svgElement->document());
    if (!svgElement->isStyled())
        return;

    SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(svgElement);
    if (!styledElement->supportsMarkers())
        return;

    const SVGRenderStyle* svgStyle = style()->svgStyle();
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
        return;

    float strokeWidth = SVGRenderStyle::cssPrimitiveToLength(this, svgStyle->strokeWidth(), 1.0f);
    m_cachedLocalMarkerBBox = m_markerLayoutInfo.calculateBoundaries(startMarker, midMarker, endMarker, strokeWidth, m_path);
}

void RenderPath::invalidateCachedBoundaries()
{
    m_cachedLocalRepaintRect = FloatRect();
    m_cachedLocalStrokeBBox = FloatRect();
    m_cachedLocalFillBBox = FloatRect();
    m_cachedLocalMarkerBBox = FloatRect();
}

void RenderPath::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    invalidateCachedBoundaries();
    RenderSVGModelObject::styleWillChange(diff, newStyle);
}

}

#endif // ENABLE(SVG)
