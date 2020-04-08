/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
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

#include "config.h"
#include "RenderSVGShape.h"

#include "FloatPoint.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "PointerEventsHitRules.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceSolidColor.h"
#include "SVGPathData.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGURIReference.h"
#include "StrokeStyleApplier.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGShape);

class BoundingRectStrokeStyleApplier final : public StrokeStyleApplier {
public:
    BoundingRectStrokeStyleApplier(const RenderSVGShape& renderer)
        : m_renderer(renderer)
    {
    }

    void strokeStyle(GraphicsContext* context) override
    {
        SVGRenderSupport::applyStrokeStyleToContext(context, m_renderer.style(), m_renderer);
    }

private:
    const RenderSVGShape& m_renderer;
};

RenderSVGShape::RenderSVGShape(SVGGraphicsElement& element, RenderStyle&& style)
    : RenderSVGModelObject(element, WTFMove(style))
    , m_needsBoundariesUpdate(false) // Default is false, the cached rects are empty from the beginning.
    , m_needsShapeUpdate(true) // Default is true, so we grab a Path object once from SVGGraphicsElement.
    , m_needsTransformUpdate(true) // Default is true, so we grab a AffineTransform object once from SVGGraphicsElement.
{
}

RenderSVGShape::~RenderSVGShape() = default;

void RenderSVGShape::updateShapeFromElement()
{
    m_path = createPath();
    processMarkerPositions();

    m_fillBoundingBox = calculateObjectBoundingBox();
    m_strokeBoundingBox = calculateStrokeBoundingBox();
}

bool RenderSVGShape::isEmpty() const
{
    // This function should never be called before assigning a new Path to m_path.
    // But this bug can happen if this renderer was created and its layout was not
    // done before painting. Assert this did not happen but do not crash.
    ASSERT(hasPath());
    return !hasPath() || path().isEmpty();
}

void RenderSVGShape::fillShape(GraphicsContext& context) const
{
    context.fillPath(path());
}

void RenderSVGShape::strokeShape(GraphicsContext& context) const
{
    ASSERT(m_path);
    Path* usePath = m_path.get();

    if (hasNonScalingStroke())
        usePath = nonScalingStrokePath(usePath, nonScalingStrokeTransform());

    context.strokePath(*usePath);
}

bool RenderSVGShape::shapeDependentStrokeContains(const FloatPoint& point, PointCoordinateSpace pointCoordinateSpace)
{
    ASSERT(m_path);
    BoundingRectStrokeStyleApplier applier(*this);

    if (hasNonScalingStroke() && pointCoordinateSpace != LocalCoordinateSpace) {
        AffineTransform nonScalingTransform = nonScalingStrokeTransform();
        Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);

        return usePath->strokeContains(applier, nonScalingTransform.mapPoint(point));
    }

    return m_path->strokeContains(applier, point);
}

bool RenderSVGShape::shapeDependentFillContains(const FloatPoint& point, const WindRule fillRule) const
{
    return path().contains(point, fillRule);
}

bool RenderSVGShape::fillContains(const FloatPoint& point, bool requiresFill, const WindRule fillRule)
{
    if (m_fillBoundingBox.isEmpty() || !m_fillBoundingBox.contains(point))
        return false;

    Color fallbackColor;
    if (requiresFill && !RenderSVGResource::fillPaintingResource(*this, style(), fallbackColor))
        return false;

    return shapeDependentFillContains(point, fillRule);
}

bool RenderSVGShape::strokeContains(const FloatPoint& point, bool requiresStroke)
{
    if (strokeBoundingBox().isEmpty() || !strokeBoundingBox().contains(point))
        return false;

    Color fallbackColor;
    if (requiresStroke && !RenderSVGResource::strokePaintingResource(*this, style(), fallbackColor))
        return false;

    return shapeDependentStrokeContains(point);
}

void RenderSVGShape::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    LayoutRepainter repainter(*this, SVGRenderSupport::checkForSVGRepaintDuringLayout(*this) && selfNeedsLayout());

    bool updateCachedBoundariesInParents = false;

    if (m_needsShapeUpdate || m_needsBoundariesUpdate) {
        updateShapeFromElement();
        m_needsShapeUpdate = false;
        updateRepaintBoundingBox();
        m_needsBoundariesUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    if (m_needsTransformUpdate) {
        m_localTransform = graphicsElement().animatedLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(*this);

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents)
        RenderSVGModelObject::setNeedsBoundariesUpdate();

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

Path* RenderSVGShape::nonScalingStrokePath(const Path* path, const AffineTransform& strokeTransform) const
{
    static NeverDestroyed<Path> tempPath;

    tempPath.get() = *path;
    tempPath.get().transform(strokeTransform);

    return &tempPath.get();
}

bool RenderSVGShape::setupNonScalingStrokeContext(AffineTransform& strokeTransform, GraphicsContextStateSaver& stateSaver)
{
    Optional<AffineTransform> inverse = strokeTransform.inverse();
    if (!inverse)
        return false;

    stateSaver.save();
    stateSaver.context()->concatCTM(inverse.value());
    return true;
}

AffineTransform RenderSVGShape::nonScalingStrokeTransform() const
{
    return graphicsElement().getScreenCTM(SVGLocatable::DisallowStyleUpdate);
}

bool RenderSVGShape::shouldGenerateMarkerPositions() const
{
    if (!style().svgStyle().hasMarkers())
        return false;

    if (!graphicsElement().supportsMarkers())
        return false;

    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
    if (!resources)
        return false;

    return resources->markerStart() || resources->markerMid() || resources->markerEnd();
}

void RenderSVGShape::fillShape(const RenderStyle& style, GraphicsContext& originalContext)
{
    GraphicsContext* context = &originalContext;
    Color fallbackColor;
    if (RenderSVGResource* fillPaintingResource = RenderSVGResource::fillPaintingResource(*this, style, fallbackColor)) {
        if (fillPaintingResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToFill))
            fillPaintingResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToFill, nullptr, this);
        else if (fallbackColor.isValid()) {
            RenderSVGResourceSolidColor* fallbackResource = RenderSVGResource::sharedSolidPaintingResource();
            fallbackResource->setColor(fallbackColor);
            if (fallbackResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToFill))
                fallbackResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToFill, nullptr, this);
        }
    }
}

void RenderSVGShape::strokeShape(const RenderStyle& style, GraphicsContext& originalContext)
{
    GraphicsContext* context = &originalContext;
    Color fallbackColor;
    if (RenderSVGResource* strokePaintingResource = RenderSVGResource::strokePaintingResource(*this, style, fallbackColor)) {
        if (strokePaintingResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToStroke))
            strokePaintingResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToStroke, nullptr, this);
        else if (fallbackColor.isValid()) {
            RenderSVGResourceSolidColor* fallbackResource = RenderSVGResource::sharedSolidPaintingResource();
            fallbackResource->setColor(fallbackColor);
            if (fallbackResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToStroke))
                fallbackResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToStroke, nullptr, this);
        }
    }
}

void RenderSVGShape::strokeShape(GraphicsContext& context)
{
    if (!style().hasVisibleStroke())
        return;

    GraphicsContextStateSaver stateSaver(context, false);
    if (hasNonScalingStroke()) {
        AffineTransform nonScalingTransform = nonScalingStrokeTransform();
        if (!setupNonScalingStrokeContext(nonScalingTransform, stateSaver))
            return;
    }
    strokeShape(style(), context);
}

void RenderSVGShape::fillStrokeMarkers(PaintInfo& childPaintInfo)
{
    auto paintOrder = RenderStyle::paintTypesForPaintOrder(style().paintOrder());
    for (unsigned i = 0; i < paintOrder.size(); ++i) {
        switch (paintOrder.at(i)) {
        case PaintType::Fill:
            fillShape(style(), childPaintInfo.context());
            break;
        case PaintType::Stroke:
            strokeShape(childPaintInfo.context());
            break;
        case PaintType::Markers:
            if (!m_markerPositions.isEmpty())
                drawMarkers(childPaintInfo);
            break;
        }
    }
}

void RenderSVGShape::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (paintInfo.context().paintingDisabled() || paintInfo.phase != PaintPhase::Foreground
        || style().visibility() == Visibility::Hidden || isEmpty())
        return;
    FloatRect boundingBox = repaintRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsRepaintRect(boundingBox, m_localTransform, paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(childPaintInfo.context());
    childPaintInfo.applyTransform(m_localTransform);

    if (childPaintInfo.phase == PaintPhase::Foreground) {
        SVGRenderingContext renderingContext(*this, childPaintInfo);

        if (renderingContext.isRenderingPrepared()) {
            const SVGRenderStyle& svgStyle = style().svgStyle();
            if (svgStyle.shapeRendering() == ShapeRendering::CrispEdges)
                childPaintInfo.context().setShouldAntialias(false);

            fillStrokeMarkers(childPaintInfo);
        }
    }

    if (style().outlineWidth())
        paintOutline(childPaintInfo, IntRect(boundingBox));
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void RenderSVGShape::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint&, const RenderLayerModelObject*)
{
    LayoutRect rect = LayoutRect(repaintRectInLocalCoordinates());
    if (!rect.isEmpty())
        rects.append(rect);
}

bool RenderSVGShape::isPointInFill(const FloatPoint& point)
{
    return shapeDependentFillContains(point, style().svgStyle().fillRule());
}

bool RenderSVGShape::isPointInStroke(const FloatPoint& point)
{
    if (!style().svgStyle().hasStroke())
        return false;

    return shapeDependentStrokeContains(point, LocalCoordinateSpace);
}

float RenderSVGShape::getTotalLength() const
{
    return hasPath() ? path().length() : createPath()->length();
}

FloatPoint RenderSVGShape::getPointAtLength(float distance) const
{
    return hasPath() ? path().pointAtLength(distance) : createPath()->pointAtLength(distance);
}

bool RenderSVGShape::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint = m_localTransform.inverse().valueOr(AffineTransform()).mapPoint(pointInParent);

    if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
        return false;

    SVGHitTestCycleDetectionScope hitTestScope(*this);

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_PATH_HITTESTING, request, style().pointerEvents());
    bool isVisible = (style().visibility() == Visibility::Visible);
    if (isVisible || !hitRules.requireVisible) {
        const SVGRenderStyle& svgStyle = style().svgStyle();
        WindRule fillRule = svgStyle.fillRule();
        if (request.svgClipContent())
            fillRule = svgStyle.clipRule();
        if ((hitRules.canHitStroke && (svgStyle.hasStroke() || !hitRules.requireStroke) && strokeContains(localPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (svgStyle.hasFill() || !hitRules.requireFill) && fillContains(localPoint, hitRules.requireFill, fillRule))
            || (hitRules.canHitBoundingBox && objectBoundingBox().contains(localPoint))) {
            updateHitTestResult(result, LayoutPoint(localPoint));
            if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, localPoint) == HitTestProgress::Stop)
                return true;
        }
    }
    return false;
}

static inline RenderSVGResourceMarker* markerForType(SVGMarkerType type, RenderSVGResourceMarker* markerStart, RenderSVGResourceMarker* markerMid, RenderSVGResourceMarker* markerEnd)
{
    switch (type) {
    case StartMarker:
        return markerStart;
    case MidMarker:
        return markerMid;
    case EndMarker:
        return markerEnd;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

FloatRect RenderSVGShape::markerRect(float strokeWidth) const
{
    ASSERT(!m_markerPositions.isEmpty());

    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
    ASSERT(resources);

    RenderSVGResourceMarker* markerStart = resources->markerStart();
    RenderSVGResourceMarker* markerMid = resources->markerMid();
    RenderSVGResourceMarker* markerEnd = resources->markerEnd();
    ASSERT(markerStart || markerMid || markerEnd);

    FloatRect boundaries;
    unsigned size = m_markerPositions.size();
    for (unsigned i = 0; i < size; ++i) {
        if (RenderSVGResourceMarker* marker = markerForType(m_markerPositions[i].type, markerStart, markerMid, markerEnd))
            boundaries.unite(marker->markerBoundaries(marker->markerTransformation(m_markerPositions[i].origin, m_markerPositions[i].angle, strokeWidth)));
    }
    return boundaries;
}

FloatRect RenderSVGShape::calculateObjectBoundingBox() const
{
    return path().boundingRect();
}

FloatRect RenderSVGShape::calculateStrokeBoundingBox() const
{
    ASSERT(m_path);
    FloatRect strokeBoundingBox = m_fillBoundingBox;

    const SVGRenderStyle& svgStyle = style().svgStyle();
    if (svgStyle.hasStroke()) {
        BoundingRectStrokeStyleApplier strokeStyle(*this);
        if (hasNonScalingStroke()) {
            AffineTransform nonScalingTransform = nonScalingStrokeTransform();
            if (Optional<AffineTransform> inverse = nonScalingTransform.inverse()) {
                Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);
                FloatRect strokeBoundingRect = usePath->strokeBoundingRect(&strokeStyle);
                strokeBoundingRect = inverse.value().mapRect(strokeBoundingRect);
                strokeBoundingBox.unite(strokeBoundingRect);
            }
        } else
            strokeBoundingBox.unite(path().strokeBoundingRect(&strokeStyle));
    }

    if (!m_markerPositions.isEmpty())
        strokeBoundingBox.unite(markerRect(strokeWidth()));

    return strokeBoundingBox;
}

void RenderSVGShape::updateRepaintBoundingBox()
{
    m_repaintBoundingBoxExcludingShadow = strokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(*this, m_repaintBoundingBoxExcludingShadow);

    m_repaintBoundingBox = m_repaintBoundingBoxExcludingShadow;
}

float RenderSVGShape::strokeWidth() const
{
    SVGLengthContext lengthContext(&graphicsElement());
    return lengthContext.valueForLength(style().strokeWidth());
}

bool RenderSVGShape::hasSmoothStroke() const
{
    const SVGRenderStyle& svgStyle = style().svgStyle();
    return svgStyle.strokeDashArray().isEmpty()
        && style().strokeMiterLimit() == style().initialStrokeMiterLimit()
        && style().joinStyle() == style().initialJoinStyle()
        && style().capStyle() == style().initialCapStyle();
}

void RenderSVGShape::drawMarkers(PaintInfo& paintInfo)
{
    ASSERT(!m_markerPositions.isEmpty());

    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
    if (!resources)
        return;

    RenderSVGResourceMarker* markerStart = resources->markerStart();
    RenderSVGResourceMarker* markerMid = resources->markerMid();
    RenderSVGResourceMarker* markerEnd = resources->markerEnd();
    if (!markerStart && !markerMid && !markerEnd)
        return;

    float strokeWidth = this->strokeWidth();
    unsigned size = m_markerPositions.size();
    for (unsigned i = 0; i < size; ++i) {
        if (RenderSVGResourceMarker* marker = markerForType(m_markerPositions[i].type, markerStart, markerMid, markerEnd))
            marker->draw(paintInfo, marker->markerTransformation(m_markerPositions[i].origin, m_markerPositions[i].angle, strokeWidth));
    }
}

std::unique_ptr<Path> RenderSVGShape::createPath() const
{
    return makeUnique<Path>(pathFromGraphicsElement(&graphicsElement()));
}

void RenderSVGShape::processMarkerPositions()
{
    m_markerPositions.clear();

    if (!shouldGenerateMarkerPositions())
        return;

    ASSERT(m_path);

    SVGMarkerData markerData(m_markerPositions, SVGResourcesCache::cachedResourcesForRenderer(*this)->markerReverseStart());
    m_path->apply([&markerData](const PathElement& pathElement) {
        SVGMarkerData::updateFromPathElement(markerData, pathElement);
    });
    markerData.pathIsDone();
}

}
