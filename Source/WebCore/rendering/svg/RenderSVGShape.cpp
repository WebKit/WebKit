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

#if ENABLE(SVG)
#include "RenderSVGShape.h"

#include "FloatPoint.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "LayoutRepainter.h"
#include "PointerEventsHitRules.h"
#include "RenderSVGContainer.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceSolidColor.h"
#include "SVGPathData.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGStyledTransformableElement.h"
#include "SVGSubpathData.h"
#include "SVGTransformList.h"
#include "SVGURIReference.h"
#include "StrokeStyleApplier.h"
#include <wtf/MathExtras.h>

namespace WebCore {

RenderSVGShape::RenderSVGShape(SVGStyledTransformableElement* node)
    : RenderSVGModelObject(node)
    , m_needsBoundariesUpdate(false) // Default is false, the cached rects are empty from the beginning.
    , m_needsShapeUpdate(true) // Default is true, so we grab a Path object once from SVGStyledTransformableElement.
    , m_needsTransformUpdate(true) // Default is true, so we grab a AffineTransform object once from SVGStyledTransformableElement.
    , m_fillFallback(false)
{
}

RenderSVGShape::~RenderSVGShape()
{
}

void RenderSVGShape::createShape()
{
    ASSERT(!m_path);
    m_path = adoptPtr(new Path);
    ASSERT(isEmpty());

    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(node());
    updatePathFromGraphicsElement(element, path());
    processZeroLengthSubpaths();
    processMarkerPositions();
}

bool RenderSVGShape::isEmpty() const
{
    return m_path->isEmpty();
}

void RenderSVGShape::fillShape(GraphicsContext* context) const
{
    context->fillPath(path());
}

FloatRect RenderSVGShape::objectBoundingBox() const
{
    return m_path->fastBoundingRect();
}

void RenderSVGShape::strokeShape(GraphicsContext* context) const
{
    if (style()->svgStyle()->hasVisibleStroke())
        context->strokePath(path());
}

bool RenderSVGShape::shapeDependentStrokeContains(const FloatPoint& point) const
{
    BoundingRectStrokeStyleApplier applier(this, style());
    return m_path->strokeContains(&applier, point);
}

bool RenderSVGShape::shapeDependentFillContains(const FloatPoint& point, const WindRule fillRule) const
{
    return m_path->contains(point, fillRule);
}

bool RenderSVGShape::fillContains(const FloatPoint& point, bool requiresFill, const WindRule fillRule)
{
    if (!m_fillBoundingBox.contains(point))
        return false;

    Color fallbackColor;
    if (requiresFill && !RenderSVGResource::fillPaintingResource(this, style(), fallbackColor))
        return false;

    return shapeDependentFillContains(point, fillRule);
}

bool RenderSVGShape::strokeContains(const FloatPoint& point, bool requiresStroke)
{
    if (!strokeBoundingBox().contains(point))
        return false;

    Color fallbackColor;
    if (requiresStroke && !RenderSVGResource::strokePaintingResource(this, style(), fallbackColor))
        return false;

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i) {
        ASSERT(style()->svgStyle()->hasStroke());
        float strokeWidth = this->strokeWidth();
        if (style()->svgStyle()->capStyle() == SquareCap) {
            if (zeroLengthSubpathRect(m_zeroLengthLinecapLocations[i], strokeWidth).contains(point))
                return true;
        } else {
            ASSERT(style()->svgStyle()->capStyle() == RoundCap);
            FloatPoint radiusVector(point.x() - m_zeroLengthLinecapLocations[i].x(), point.y() -  m_zeroLengthLinecapLocations[i].y());
            if (radiusVector.lengthSquared() < strokeWidth * strokeWidth * .25f)
                return true;
        }
    }

    if (!svgStyle->strokeDashArray().isEmpty() || svgStyle->strokeMiterLimit() != svgStyle->initialStrokeMiterLimit()
        || svgStyle->joinStyle() != svgStyle->initialJoinStyle() || svgStyle->capStyle() != svgStyle->initialCapStyle()) {
        if (!m_path)
            RenderSVGShape::createShape();
        return RenderSVGShape::shapeDependentStrokeContains(point);
    }
    return shapeDependentStrokeContains(point);
}

void RenderSVGShape::layout()
{
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() && selfNeedsLayout());
    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(node());

    bool updateCachedBoundariesInParents = false;

    bool needsShapeUpdate = m_needsShapeUpdate;
    if (needsShapeUpdate || m_needsBoundariesUpdate) {
        setIsPaintingFallback(false);
        m_path.clear();
        createShape();
        m_needsShapeUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    if (m_needsTransformUpdate) {
        m_localTransform = element->animatedLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    // At this point LayoutRepainter already grabbed the old bounds,
    // recalculate them now so repaintAfterLayout() uses the new bounds.
    if (needsShapeUpdate || m_needsBoundariesUpdate) {
        updateCachedBoundaries();
        m_needsBoundariesUpdate = false;
    }

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents)
        RenderSVGModelObject::setNeedsBoundariesUpdate();

    repainter.repaintAfterLayout();
    setNeedsLayout(false);
}

bool RenderSVGShape::shouldStrokeZeroLengthSubpath() const
{
    // Spec(11.4): Any zero length subpath shall not be stroked if the "stroke-linecap" property has a value of butt
    // but shall be stroked if the "stroke-linecap" property has a value of round or square
    return style()->svgStyle()->hasStroke() && style()->svgStyle()->capStyle() != ButtCap;
}

bool RenderSVGShape::shouldGenerateMarkerPositions() const
{
    if (!style()->svgStyle()->hasMarkers())
        return false;

    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(node());
    if (!element->supportsMarkers())
        return false;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
    if (!resources)
        return false;

    return resources->markerStart() || resources->markerMid() || resources->markerEnd();
}

FloatRect RenderSVGShape::zeroLengthSubpathRect(const FloatPoint& linecapPosition, float strokeWidth) const
{
    return FloatRect(linecapPosition.x() - strokeWidth / 2, linecapPosition.y() - strokeWidth / 2, strokeWidth, strokeWidth);
}

Path* RenderSVGShape::zeroLengthLinecapPath(const FloatPoint& linecapPosition)
{
    // Spec(11.4): Any zero length subpath shall not be stroked if the "stroke-linecap" property has a value of butt
    // but shall be stroked if the "stroke-linecap" property has a value of round or square
    DEFINE_STATIC_LOCAL(Path, tempPath, ());

    tempPath.clear();
    float strokeWidth = this->strokeWidth();
    if (style()->svgStyle()->capStyle() == SquareCap)
        tempPath.addRect(zeroLengthSubpathRect(linecapPosition, strokeWidth));
    else
        tempPath.addEllipse(zeroLengthSubpathRect(linecapPosition, strokeWidth));

    return &tempPath;
}

Path* RenderSVGShape::nonScalingStrokePath(const Path* path, const AffineTransform& strokeTransform)
{
    DEFINE_STATIC_LOCAL(Path, tempPath, ());

    tempPath = *path;
    tempPath.transform(strokeTransform);

    return &tempPath;
}

bool RenderSVGShape::setupNonScalingStrokeTransform(AffineTransform& strokeTransform, GraphicsContextStateSaver& stateSaver)
{
    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(node());
    strokeTransform = element->getScreenCTM(SVGLocatable::DisallowStyleUpdate);
    if (!strokeTransform.isInvertible())
        return false;

    stateSaver.save();
    stateSaver.context()->concatCTM(strokeTransform.inverse());
    return true;
}

void RenderSVGShape::fillShape(RenderStyle* style, GraphicsContext* context, Path* path, RenderSVGShape* shape)
{
    Color fallbackColor;
    if (RenderSVGResource* fillPaintingResource = RenderSVGResource::fillPaintingResource(this, style, fallbackColor)) {
        if (fillPaintingResource->applyResource(this, style, context, ApplyToFillMode))
            fillPaintingResource->postApplyResource(this, context, ApplyToFillMode, path, shape);
        else if (fallbackColor.isValid()) {
            RenderSVGResourceSolidColor* fallbackResource = RenderSVGResource::sharedSolidPaintingResource();
            fallbackResource->setColor(fallbackColor);
            if (fallbackResource->applyResource(this, style, context, ApplyToFillMode))
                fallbackResource->postApplyResource(this, context, ApplyToFillMode, path, shape);
        }
    }
}

void RenderSVGShape::strokePath(RenderStyle* style, GraphicsContext* context, Path* path, RenderSVGResource* strokePaintingResource,
                                const Color& fallbackColor, bool nonScalingStroke, const AffineTransform& nonScalingStrokeTransform,
                                int applyMode)
{
    if (!style->svgStyle()->hasVisibleStroke())
        return;
    Path* usePath = path;
    if (nonScalingStroke) {
        usePath = nonScalingStrokePath(path, nonScalingStrokeTransform);
    }
    if (strokePaintingResource->applyResource(this, style, context, applyMode)) {
        strokePaintingResource->postApplyResource(this, context, applyMode, usePath, this);
        return;
    }
    if (!fallbackColor.isValid())
        return;
    RenderSVGResourceSolidColor* fallbackResource = RenderSVGResource::sharedSolidPaintingResource();
    fallbackResource->setColor(fallbackColor);
    if (fallbackResource->applyResource(this, style, context, applyMode))
        fallbackResource->postApplyResource(this, context, applyMode, usePath, this);
}

void RenderSVGShape::fillAndStrokePath(GraphicsContext* context)
{
    RenderStyle* style = this->style();

    fillShape(style, context, 0, this);

    Color fallbackColor = Color();
    RenderSVGResource* strokePaintingResource = RenderSVGResource::strokePaintingResource(this, style, fallbackColor);
    if (!strokePaintingResource)
        return;

    GraphicsContextStateSaver stateSaver(*context, false);
    AffineTransform nonScalingStrokeTransform;
    bool nonScalingStroke = style->svgStyle()->vectorEffect() == VE_NON_SCALING_STROKE;
    if (nonScalingStroke) {
        if (!setupNonScalingStrokeTransform(nonScalingStrokeTransform, stateSaver))
            return;
    }

    strokePath(style, context, m_path.get(), strokePaintingResource, fallbackColor, nonScalingStroke, nonScalingStrokeTransform, ApplyToStrokeMode);

    // Spec(11.4): Any zero length subpath shall not be stroked if the "stroke-linecap" property has a value of butt
    // but shall be stroked if the "stroke-linecap" property has a value of round or square
    for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i) {
        Path* usePath = zeroLengthLinecapPath(m_zeroLengthLinecapLocations[i]);
        strokePath(style, context, usePath, strokePaintingResource, fallbackColor, nonScalingStroke, nonScalingStrokeTransform, ApplyToFillMode);
    }

}

void RenderSVGShape::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (paintInfo.context->paintingDisabled() || style()->visibility() == HIDDEN || isEmpty())
        return;
    FloatRect boundingBox = repaintRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsRepaintRect(boundingBox, m_localTransform, paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);
    bool drawsOutline = style()->outlineWidth() && (childPaintInfo.phase == PaintPhaseOutline || childPaintInfo.phase == PaintPhaseSelfOutline);
    if (drawsOutline || childPaintInfo.phase == PaintPhaseForeground) {
        GraphicsContextStateSaver stateSaver(*childPaintInfo.context);
        childPaintInfo.applyTransform(m_localTransform);

        if (childPaintInfo.phase == PaintPhaseForeground) {
            SVGRenderingContext renderingContext(this, childPaintInfo);

            if (renderingContext.isRenderingPrepared()) {
                const SVGRenderStyle* svgStyle = style()->svgStyle();
                if (svgStyle->shapeRendering() == SR_CRISPEDGES)
                    childPaintInfo.context->setShouldAntialias(false);

                fillAndStrokePath(childPaintInfo.context);
                if (!m_markerPositions.isEmpty())
                    drawMarkers(childPaintInfo);
            }
        }

        if (drawsOutline)
            paintOutline(childPaintInfo.context, IntRect(boundingBox));
    }
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void RenderSVGShape::addFocusRingRects(Vector<IntRect>& rects, const LayoutPoint&)
{
    IntRect rect = enclosingIntRect(repaintRectInLocalCoordinates());
    if (!rect.isEmpty())
        rects.append(rect);
}

bool RenderSVGShape::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint = m_localTransform.inverse().mapPoint(pointInParent);

    if (!SVGRenderSupport::pointInClippingArea(this, localPoint))
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_PATH_HITTESTING, request, style()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        const SVGRenderStyle* svgStyle = style()->svgStyle();
        WindRule fillRule = svgStyle->fillRule();
        if (request.svgClipContent())
            fillRule = svgStyle->clipRule();
        if ((hitRules.canHitStroke && (svgStyle->hasStroke() || !hitRules.requireStroke) && strokeContains(localPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (svgStyle->hasFill() || !hitRules.requireFill) && fillContains(localPoint, hitRules.requireFill, fillRule))) {
            updateHitTestResult(result, roundedLayoutPoint(localPoint));
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

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
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

void RenderSVGShape::updateCachedBoundaries()
{
    if (isEmpty()) {
        m_fillBoundingBox = FloatRect();
        m_strokeAndMarkerBoundingBox = FloatRect();
        m_repaintBoundingBox = FloatRect();
        return;
    }

    // Cache _unclipped_ fill bounding box, used for calculations in resources
    m_fillBoundingBox = objectBoundingBox();

    // Add zero-length sub-path linecaps to the fill box
    float strokeWidth = this->strokeWidth();
    for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i)
        m_fillBoundingBox.unite(zeroLengthSubpathRect(m_zeroLengthLinecapLocations[i], strokeWidth));

    // Cache _unclipped_ stroke bounding box, used for calculations in resources (includes marker boundaries)
    m_strokeAndMarkerBoundingBox = m_fillBoundingBox;
    if (hasPath())
        inflateWithStrokeAndMarkerBounds();
    // Cache smallest possible repaint rectangle
    m_repaintBoundingBoxExcludingShadow = strokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(this, m_repaintBoundingBoxExcludingShadow);

    m_repaintBoundingBox = m_repaintBoundingBoxExcludingShadow;
    SVGRenderSupport::intersectRepaintRectWithShadows(this, m_repaintBoundingBox);
}

float RenderSVGShape::strokeWidth() const
{
    SVGElement* svgElement = static_cast<SVGElement*>(node());
    SVGLengthContext lengthContext(svgElement);
    return style()->svgStyle()->strokeWidth().value(lengthContext);
}

void RenderSVGShape::inflateWithStrokeAndMarkerBounds()
{
    const SVGRenderStyle* svgStyle = style()->svgStyle();
    if (svgStyle->hasStroke()) {
        BoundingRectStrokeStyleApplier strokeStyle(this, style());
        m_strokeAndMarkerBoundingBox.unite(path().strokeBoundingRect(&strokeStyle));
    }
    if (!m_markerPositions.isEmpty())
        m_strokeAndMarkerBoundingBox.unite(markerRect(strokeWidth()));
}

void RenderSVGShape::drawMarkers(PaintInfo& paintInfo)
{
    ASSERT(!m_markerPositions.isEmpty());

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
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

void RenderSVGShape::processZeroLengthSubpaths()
{
    m_zeroLengthLinecapLocations.clear();

    float strokeWidth = this->strokeWidth();
    if (!strokeWidth || !shouldStrokeZeroLengthSubpath())
        return;

    ASSERT(m_path);

    SVGSubpathData subpathData(m_zeroLengthLinecapLocations);
    m_path->apply(&subpathData, SVGSubpathData::updateFromPathElement);
    subpathData.pathIsDone();
}

void RenderSVGShape::processMarkerPositions()
{
    m_markerPositions.clear();

    if (!shouldGenerateMarkerPositions())
        return;

    ASSERT(m_path);

    SVGMarkerData markerData(m_markerPositions);
    m_path->apply(&markerData, SVGMarkerData::updateFromPathElement);
    markerData.pathIsDone();
}

}

#endif // ENABLE(SVG)
