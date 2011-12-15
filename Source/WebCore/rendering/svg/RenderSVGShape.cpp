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
#include "SVGPathElement.h"
#include "SVGRenderSupport.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGStyledTransformableElement.h"
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

RenderSVGShape::RenderSVGShape(SVGStyledTransformableElement* node, Path* path, bool isFillFallback)
    : RenderSVGModelObject(node)
    , m_path(adoptPtr(path))
    , m_fillFallback(isFillFallback)
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

    SVGPathElement* element = static_cast<SVGPathElement*>(node());
    updatePathFromGraphicsElement(element, path());
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
    if (!m_strokeAndMarkerBoundingBox.contains(point))
        return false;

    Color fallbackColor;
    if (requiresStroke && !RenderSVGResource::strokePaintingResource(this, style(), fallbackColor))
        return false;

    if (shouldStrokeZeroLengthSubpath())
        return zeroLengthSubpathRect().contains(point);

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    if (!svgStyle->strokeDashArray().isEmpty() || svgStyle->strokeMiterLimit() != svgStyle->initialStrokeMiterLimit()
        || svgStyle->joinStyle() != svgStyle->initialJoinStyle() || svgStyle->capStyle() != svgStyle->initialCapStyle() || static_cast<SVGElement*>(node())->isStyled()) {
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
    if (needsShapeUpdate) {
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

    if (m_needsBoundariesUpdate) {
        setIsPaintingFallback(false);
        m_path.clear();
        createShape();
        updateCachedBoundariesInParents = true;
    }

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout()) {
        SVGResourcesCache::clientLayoutChanged(this);
        m_markerLayoutInfo.clear();
    }

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
    // Spec(11.4): Any zero length subpath shall not be stroked if the ‘stroke-linecap’ property has a value of butt
    // but shall be stroked if the ‘stroke-linecap’ property has a value of round or square
    return style()->svgStyle()->hasStroke() && style()->svgStyle()->capStyle() != ButtCap && !m_fillBoundingBox.width() && !m_fillBoundingBox.height();
}

FloatRect RenderSVGShape::zeroLengthSubpathRect() const
{
    float strokeWidth = this->strokeWidth();
    return FloatRect(m_fillBoundingBox.x() - strokeWidth / 2, m_fillBoundingBox.y() - strokeWidth / 2, strokeWidth, strokeWidth);
}

void RenderSVGShape::setupSquareCapPath(Path*& usePath, int& applyMode)
{
    // Spec(11.4): Any zero length subpath shall not be stroked if the ‘stroke-linecap’ property has a value of butt
    // but shall be stroked if the ‘stroke-linecap’ property has a value of round or square
    DEFINE_STATIC_LOCAL(Path, tempPath, ());

    applyMode = ApplyToFillMode;
    usePath = &tempPath;
    usePath->clear();
    if (style()->svgStyle()->capStyle() == SquareCap)
        usePath->addRect(zeroLengthSubpathRect());
    else
        usePath->addEllipse(zeroLengthSubpathRect());
}

bool RenderSVGShape::setupNonScalingStrokePath(Path*& usePath, GraphicsContextStateSaver& stateSaver)
{
    DEFINE_STATIC_LOCAL(Path, tempPath, ());

    SVGStyledTransformableElement* element = static_cast<SVGStyledTransformableElement*>(node());
    AffineTransform nonScalingStrokeTransform = element->getScreenCTM(SVGLocatable::DisallowStyleUpdate);
    if (!nonScalingStrokeTransform.isInvertible())
        return false;

    tempPath = *m_path;
    usePath = &tempPath;
    tempPath.transform(nonScalingStrokeTransform);

    stateSaver.save();
    stateSaver.context()->concatCTM(nonScalingStrokeTransform.inverse());
    return true;
}

void RenderSVGShape::fillAndStrokePath(GraphicsContext* context)
{
    RenderStyle* style = this->style();

    Color fallbackColor;
    if (RenderSVGResource* fillPaintingResource = RenderSVGResource::fillPaintingResource(this, style, fallbackColor)) {
        if (fillPaintingResource->applyResource(this, style, context, ApplyToFillMode))
            fillPaintingResource->postApplyResource(this, context, ApplyToFillMode, 0, this);
        else if (fallbackColor.isValid()) {
            RenderSVGResourceSolidColor* fallbackResource = RenderSVGResource::sharedSolidPaintingResource();
            fallbackResource->setColor(fallbackColor);
            if (fallbackResource->applyResource(this, style, context, ApplyToFillMode))
                fallbackResource->postApplyResource(this, context, ApplyToFillMode, 0, this);
        }
    }

    fallbackColor = Color();
    RenderSVGResource* strokePaintingResource = RenderSVGResource::strokePaintingResource(this, style, fallbackColor);
    if (!strokePaintingResource)
        return;

    Path* usePath = m_path.get();
    int applyMode = ApplyToStrokeMode;

    bool nonScalingStroke = style->svgStyle()->vectorEffect() == VE_NON_SCALING_STROKE;
    OwnPtr<Path> savePath;

    GraphicsContextStateSaver stateSaver(*context, false);

    // Spec(11.4): Any zero length subpath shall not be stroked if the ‘stroke-linecap’ property has a value of butt
    // but shall be stroked if the ‘stroke-linecap’ property has a value of round or square
    // FIXME: this does not work for zero-length subpaths, only when total path is zero-length
    if (shouldStrokeZeroLengthSubpath()) {
        if (!m_path) {
            RenderSVGShape::createShape();
            setIsPaintingFallback(true);
            usePath = m_path.get();
        }
        setupSquareCapPath(usePath, applyMode);
    } else if (nonScalingStroke) {
        if (!setupNonScalingStrokePath(usePath, stateSaver))
            return;
    }
    if (strokePaintingResource->applyResource(this, style, context, applyMode)) {
        strokePaintingResource->postApplyResource(this, context, applyMode, usePath, this);
        if (savePath)
            m_path = savePath.release();
        return;
    }
    if (!fallbackColor.isValid()) {
        if (savePath)
            m_path = savePath.release();
        return;
    }
    RenderSVGResourceSolidColor* fallbackResource = RenderSVGResource::sharedSolidPaintingResource();
    fallbackResource->setColor(fallbackColor);
    if (fallbackResource->applyResource(this, style, context, applyMode))
        fallbackResource->postApplyResource(this, context, applyMode, usePath, this);
    if (savePath)
        m_path = savePath.release();
}

void RenderSVGShape::paint(PaintInfo& paintInfo, const IntPoint&)
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
            PaintInfo savedInfo(childPaintInfo);

            if (SVGRenderSupport::prepareToRenderSVGContent(this, childPaintInfo)) {
                const SVGRenderStyle* svgStyle = style()->svgStyle();
                if (svgStyle->shapeRendering() == SR_CRISPEDGES)
                    childPaintInfo.context->setShouldAntialias(false);

                fillAndStrokePath(childPaintInfo.context);

                if (svgStyle->hasMarkers())
                    m_markerLayoutInfo.drawMarkers(childPaintInfo);
            }

            SVGRenderSupport::finishRenderSVGContent(this, childPaintInfo, savedInfo.context);
        }

        if (drawsOutline)
            paintOutline(childPaintInfo.context, IntRect(boundingBox));
    }
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void RenderSVGShape::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint&)
{
    LayoutRect rect = enclosingLayoutRect(repaintRectInLocalCoordinates());
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

FloatRect RenderSVGShape::calculateMarkerBoundsIfNeeded()
{
    SVGElement* svgElement = static_cast<SVGElement*>(node());
    ASSERT(svgElement && svgElement->document());
    if (!svgElement->isStyled())
        return FloatRect();

    SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(svgElement);
    if (!styledElement->supportsMarkers())
        return FloatRect();

    ASSERT(style()->svgStyle()->hasMarkers());

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
    if (!resources)
        return FloatRect();

    RenderSVGResourceMarker* markerStart = resources->markerStart();
    RenderSVGResourceMarker* markerMid = resources->markerMid();
    RenderSVGResourceMarker* markerEnd = resources->markerEnd();
    if (!markerStart && !markerMid && !markerEnd)
        return FloatRect();

    return m_markerLayoutInfo.calculateBoundaries(markerStart, markerMid, markerEnd, strokeWidth(), path());
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

    // Spec(11.4): Any zero length subpath shall not be stroked if the ‘stroke-linecap’ property has a value of butt
    // but shall be stroked if the ‘stroke-linecap’ property has a value of round or square
    if (shouldStrokeZeroLengthSubpath()) {
        m_strokeAndMarkerBoundingBox = zeroLengthSubpathRect();
        // Cache smallest possible repaint rectangle
        m_repaintBoundingBox = m_strokeAndMarkerBoundingBox;
        SVGRenderSupport::intersectRepaintRectWithResources(this, m_repaintBoundingBox);
        return;
    }

    // Cache _unclipped_ stroke bounding box, used for calculations in resources (includes marker boundaries)
    m_strokeAndMarkerBoundingBox = m_fillBoundingBox;
    if (hasPath())
        inflateWithStrokeAndMarkerBounds();
    // Cache smallest possible repaint rectangle
    m_repaintBoundingBox = strokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(this, m_repaintBoundingBox);
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
    FloatRect strokeRect;
    if (svgStyle->hasStroke()) {
        BoundingRectStrokeStyleApplier strokeStyle(this, style());
        m_strokeAndMarkerBoundingBox.unite(path().strokeBoundingRect(&strokeStyle));
    }
    if (svgStyle->hasMarkers()) {
        FloatRect markerBounds = calculateMarkerBoundsIfNeeded();
        if (!markerBounds.isEmpty())
            m_strokeAndMarkerBoundingBox.unite(markerBounds);
    }
}

}

#endif // ENABLE(SVG)
