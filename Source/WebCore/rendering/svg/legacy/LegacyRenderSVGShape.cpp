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
#include "LegacyRenderSVGShape.h"

#include "FloatPoint.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "LegacyRenderSVGResourceMarker.h"
#include "LegacyRenderSVGResourceSolidColor.h"
#include "LegacyRenderSVGShapeInlines.h"
#include "PointerEventsHitRules.h"
#include "RenderStyle+GettersInlines.h"
#include "SVGPathData.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGURIReference.h"
#include "SVGVisitedRendererTracking.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyRenderSVGShape);

LegacyRenderSVGShape::LegacyRenderSVGShape(Type type, SVGGraphicsElement& element, RenderStyle&& style)
    : LegacyRenderSVGModelObject(type, element, WTF::move(style), { SVGModelObjectFlag::IsShape, SVGModelObjectFlag::UsesBoundaryCaching })
    , m_needsBoundariesUpdate(false) // Default is false, the cached rects are empty from the beginning.
    , m_needsShapeUpdate(true) // Default is true, so we grab a Path object once from SVGGraphicsElement.
    , m_needsTransformUpdate(true) // Default is true, so we grab a AffineTransform object once from SVGGraphicsElement.
{
}

LegacyRenderSVGShape::~LegacyRenderSVGShape() = default;

bool LegacyRenderSVGShape::isEmpty() const
{
    // This function should never be called before assigning a new Path to m_path.
    // But this bug can happen if this renderer was created and its layout was not
    // done before painting. Assert this did not happen but do not crash.
    ASSERT(hasPath());
    return !hasPath() || path().isEmpty();
}

void LegacyRenderSVGShape::fillShape(GraphicsContext& context) const
{
    context.fillPath(path());
}

void LegacyRenderSVGShape::strokeShape(GraphicsContext& context) const
{
    ASSERT(m_path);
    Path* usePath = m_path.get();

    if (hasNonScalingStroke())
        usePath = nonScalingStrokePath(usePath, nonScalingStrokeTransform());

    context.strokePath(*usePath);
}

bool LegacyRenderSVGShape::shapeDependentStrokeContains(const FloatPoint& point, PointCoordinateSpace pointCoordinateSpace)
{
    ASSERT(m_path);

    if (hasNonScalingStroke() && pointCoordinateSpace != LocalCoordinateSpace) {
        AffineTransform nonScalingTransform = nonScalingStrokeTransform();
        Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);
        return usePath->strokeContains(nonScalingTransform.mapPoint(point), [this] (GraphicsContext& context) {
            SVGRenderSupport::applyStrokeStyleToContext(context, style(), *this);
        });
    }

    return m_path->strokeContains(point, [this] (GraphicsContext& context) {
        SVGRenderSupport::applyStrokeStyleToContext(context, style(), *this);
    });
}

bool LegacyRenderSVGShape::shapeDependentFillContains(const FloatPoint& point, const WindRule fillRule) const
{
    return const_cast<LegacyRenderSVGShape&>(*this).ensurePath().contains(point, fillRule);
}

bool LegacyRenderSVGShape::fillContains(const FloatPoint& point, bool requiresFill, const WindRule fillRule)
{
    if (m_fillBoundingBox.isEmpty() || !m_fillBoundingBox.contains(point))
        return false;

    Color fallbackColor;
    if (requiresFill && !LegacyRenderSVGResource::fillPaintingResource(*this, style(), fallbackColor))
        return false;

    return shapeDependentFillContains(point, fillRule);
}

bool LegacyRenderSVGShape::strokeContains(const FloatPoint& point, bool requiresStroke)
{
    // "A zero value causes no stroke to be painted."
    if (!strokeWidth())
        return false;

    auto approximateStrokeBoundingBox = this->approximateStrokeBoundingBox();
    if (approximateStrokeBoundingBox.isEmpty() || !approximateStrokeBoundingBox.contains(point))
        return false;

    Color fallbackColor;
    if (requiresStroke && !LegacyRenderSVGResource::strokePaintingResource(*this, style(), fallbackColor))
        return false;

    return shapeDependentStrokeContains(point);
}

void LegacyRenderSVGShape::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    auto checkForRepaintOverride = !selfNeedsLayout() ? LayoutRepainter::CheckForRepaint::No : SVGRenderSupport::checkForSVGRepaintDuringLayout(*this);
    LayoutRepainter repainter(*this, checkForRepaintOverride, { }, RepaintOutlineBounds::No);

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
    if (updateCachedBoundariesInParents) {
        if (CheckedPtr parent = this->parent())
            parent->invalidateCachedBoundaries();
    }

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

Path* LegacyRenderSVGShape::nonScalingStrokePath(const Path* path, const AffineTransform& strokeTransform) const
{
    static NeverDestroyed<Path> tempPath;

    tempPath.get() = *path;
    tempPath.get().transform(strokeTransform);

    return &tempPath.get();
}

bool LegacyRenderSVGShape::setupNonScalingStrokeContext(AffineTransform& strokeTransform, GraphicsContextStateSaver& stateSaver)
{
    std::optional<AffineTransform> inverse = strokeTransform.inverse();
    if (!inverse)
        return false;

    stateSaver.save();
    stateSaver.context()->concatCTM(inverse.value());
    return true;
}

AffineTransform LegacyRenderSVGShape::nonScalingStrokeTransform() const
{
    return protectedGraphicsElement()->getScreenCTM(SVGLocatable::DisallowStyleUpdate);
}

void LegacyRenderSVGShape::fillShape(const RenderStyle& style, GraphicsContext& originalContext)
{
    GraphicsContext* context = &originalContext;
    Color fallbackColor;
    if (auto* fillPaintingResource = LegacyRenderSVGResource::fillPaintingResource(*this, style, fallbackColor)) {
        if (resourceWasApplied(fillPaintingResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToFill)))
            fillPaintingResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToFill, nullptr, this);
        else if (fallbackColor.isValid()) {
            auto* fallbackResource = LegacyRenderSVGResource::sharedSolidPaintingResource();
            fallbackResource->setColor(fallbackColor);
            if (resourceWasApplied(fallbackResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToFill)))
                fallbackResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToFill, nullptr, this);
        }
    }
}

void LegacyRenderSVGShape::strokeShapeInternal(const RenderStyle& style, GraphicsContext& originalContext)
{
    GraphicsContext* context = &originalContext;
    Color fallbackColor;
    if (auto* strokePaintingResource = LegacyRenderSVGResource::strokePaintingResource(*this, style, fallbackColor)) {
        if (resourceWasApplied(strokePaintingResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToStroke)))
            strokePaintingResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToStroke, nullptr, this);
        else if (fallbackColor.isValid()) {
            auto* fallbackResource = LegacyRenderSVGResource::sharedSolidPaintingResource();
            fallbackResource->setColor(fallbackColor);
            if (resourceWasApplied(fallbackResource->applyResource(*this, style, context, RenderSVGResourceMode::ApplyToStroke)))
                fallbackResource->postApplyResource(*this, context, RenderSVGResourceMode::ApplyToStroke, nullptr, this);
        }
    }
}

void LegacyRenderSVGShape::strokeShape(const RenderStyle& style, GraphicsContext& context)
{
    if (!style.hasStroke() || !style.strokeWidth().isPossiblyPositive())
        return;

    GraphicsContextStateSaver stateSaver(context, false);
    if (hasNonScalingStroke()) {
        AffineTransform nonScalingTransform = nonScalingStrokeTransform();
        if (!setupNonScalingStrokeContext(nonScalingTransform, stateSaver))
            return;
    }
    strokeShapeInternal(style, context);
}

void LegacyRenderSVGShape::fillStrokeMarkers(PaintInfo& childPaintInfo)
{
    for (auto type : style().paintOrder()) {
        switch (type) {
        case Style::PaintType::Fill:
            fillShape(style(), childPaintInfo.context());
            break;
        case Style::PaintType::Stroke:
            strokeShape(style(), childPaintInfo.context());
            break;
        case Style::PaintType::Markers:
            drawMarkers(childPaintInfo);
            break;
        }
    }
}

void LegacyRenderSVGShape::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (style().usedVisibility() == Visibility::Hidden || isEmpty())
        return;

    if (paintInfo.phase == PaintPhase::EventRegion) {
        paintInfo.eventRegionContext()->unite(FloatRoundedRect(m_fillBoundingBox), *this, style(), false);
        return;
    }

    if (paintInfo.context().paintingDisabled() || paintInfo.phase != PaintPhase::Foreground)
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
            if (style().shapeRendering() == ShapeRendering::CrispEdges)
                childPaintInfo.context().setShouldAntialias(false);

            m_fillRequiresClip = !renderingContext.pathClippingIsEntirelyWithinRendererContents();
            fillStrokeMarkers(childPaintInfo);
            m_fillRequiresClip = true;
        }
    }

    if (style().usedOutlineWidth())
        paintOutline(childPaintInfo, IntRect(boundingBox));
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void LegacyRenderSVGShape::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint&, const RenderLayerModelObject*) const
{
    LayoutRect rect = LayoutRect(repaintRectInLocalCoordinates());
    if (!rect.isEmpty())
        rects.append(rect);
}

bool LegacyRenderSVGShape::isPointInFill(const FloatPoint& point)
{
    return shapeDependentFillContains(point, style().fillRule());
}

bool LegacyRenderSVGShape::isPointInStroke(const FloatPoint& point)
{
    if (!style().hasStroke())
        return false;

    return shapeDependentStrokeContains(point, LocalCoordinateSpace);
}

float LegacyRenderSVGShape::getTotalLength() const
{
    return hasPath() ? path().length() : createPath()->length();
}

FloatPoint LegacyRenderSVGShape::getPointAtLength(float distance) const
{
    return hasPath() ? path().pointAtLength(distance) : createPath()->pointAtLength(distance);
}

bool LegacyRenderSVGShape::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return false;

    FloatPoint localPoint = valueOrDefault(m_localTransform.inverse()).mapPoint(pointInParent);

    if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
        return false;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGPath, request, usedPointerEvents());
    if (isVisibleToHitTesting(style(), request) || !hitRules.requireVisible) {
        WindRule fillRule = style().fillRule();
        if (request.svgClipContent())
            fillRule = style().clipRule();
        if ((hitRules.canHitStroke && (style().hasStroke() || !hitRules.requireStroke) && strokeContains(localPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (style().hasFill() || !hitRules.requireFill) && fillContains(localPoint, hitRules.requireFill, fillRule))
            || (hitRules.canHitBoundingBox && objectBoundingBox().contains(localPoint))) {
            updateHitTestResult(result, LayoutPoint(localPoint));
            if (result.addNodeToListBasedTestResult(protectedNodeForHitTest().get(), request, flooredLayoutPoint(localPoint)) == HitTestProgress::Stop)
                return true;
        }
    }
    return false;
}

FloatRect LegacyRenderSVGShape::strokeBoundingBox() const
{
    if (m_shapeType == ShapeType::Empty)
        return { };
    if (!m_strokeBoundingBox) {
        // Initialize m_strokeBoundingBox before calling calculateStrokeBoundingBox, since recursively referenced markers can cause us to re-enter here.
        m_strokeBoundingBox = FloatRect { };
        m_strokeBoundingBox = calculateStrokeBoundingBox();
    }
    return *m_strokeBoundingBox;
}

FloatRect LegacyRenderSVGShape::calculateStrokeBoundingBox() const
{
    ASSERT(m_path);
    FloatRect strokeBoundingBox = m_fillBoundingBox;

    if (style().hasStroke()) {
        if (hasNonScalingStroke()) {
            AffineTransform nonScalingTransform = nonScalingStrokeTransform();
            if (std::optional<AffineTransform> inverse = nonScalingTransform.inverse()) {
                Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);
                FloatRect strokeBoundingRect = usePath->strokeBoundingRect(Function<void(GraphicsContext&)> { [this] (GraphicsContext& context) {
                    SVGRenderSupport::applyStrokeStyleToContext(context, style(), *this);
                } });
                strokeBoundingRect = inverse.value().mapRect(strokeBoundingRect);
                if (!strokeBoundingRect.isNaN())
                    strokeBoundingBox.unite(strokeBoundingRect);
            }
        } else {
            auto strokeBoundingRect = path().strokeBoundingRect(Function<void(GraphicsContext&)> { [this] (GraphicsContext& context) {
                SVGRenderSupport::applyStrokeStyleToContext(context, style(), *this);
            } });
            if (!strokeBoundingRect.isNaN())
                strokeBoundingBox.unite(strokeBoundingRect);
        }
    }

    return adjustStrokeBoundingBoxForMarkersAndZeroLengthLinecaps(RepaintRectCalculation::Accurate, strokeBoundingBox);
}

FloatRect LegacyRenderSVGShape::approximateStrokeBoundingBox() const
{
    if (m_shapeType == ShapeType::Empty)
        return { };
    if (!m_approximateStrokeBoundingBox) {
        // Initialize m_approximateStrokeBoundingBox before calling calculateApproximateStrokeBoundingBox, since recursively referenced markers can cause us to re-enter here.
        m_approximateStrokeBoundingBox = FloatRect { };
        m_approximateStrokeBoundingBox = calculateApproximateStrokeBoundingBox();
    }
    return *m_approximateStrokeBoundingBox;
}

FloatRect LegacyRenderSVGShape::calculateApproximateStrokeBoundingBox() const
{
    if (m_shapeType == ShapeType::Empty)
        return { };

    if (m_strokeBoundingBox)
        return *m_strokeBoundingBox;

    return SVGRenderSupport::calculateApproximateStrokeBoundingBox(*this);
}

void LegacyRenderSVGShape::updateRepaintBoundingBox()
{
    m_repaintBoundingBox = approximateStrokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(*this, m_repaintBoundingBox);
}

FloatRect LegacyRenderSVGShape::repaintRectInLocalCoordinates(RepaintRectCalculation repaintRectCalculation) const
{
    if (repaintRectCalculation == RepaintRectCalculation::Fast)
        return m_repaintBoundingBox;

    FloatRect strokeBoundingBox = this->strokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(*this, strokeBoundingBox, RepaintRectCalculation::Accurate);
    return strokeBoundingBox;
}

FloatRect LegacyRenderSVGShape::decoratedBoundingBox() const
{
    // FIXME: strokeBoundingBox currently includes markers via adjustStrokeBoundingBoxForMarkersAndZeroLengthLinecaps.
    // Ideally, strokeBoundingBox should only include stroke. We should refactor to compute
    // decoratedBoundingBox = strokeBoundingBox() + markers explicitly.
    return strokeBoundingBox();
}

float LegacyRenderSVGShape::strokeWidth() const
{
    Ref graphicsElement = this->graphicsElement();
    SVGLengthContext lengthContext(graphicsElement.ptr());
    auto strokeWidth = lengthContext.valueForLength(style().strokeWidth(), style().usedZoomForLength());
    return std::isnan(strokeWidth) ? 0 : strokeWidth;
}

float LegacyRenderSVGShape::strokeWidthForMarkerUnits() const
{
    float strokeWidth = this->strokeWidth();
    if (!hasNonScalingStroke())
        return strokeWidth;

    auto nonScalingTransform = nonScalingStrokeTransform();
    if (!nonScalingTransform.isInvertible())
        return 0.f;

    double xScale = nonScalingTransform.xScale();
    double yScale = nonScalingTransform.yScale();
    if (xScale == yScale) [[likely]]
        return strokeWidth / xScale;

    float scaleFactor = clampTo<float>(std::sqrt((xScale * xScale + yScale * yScale) / 2));
    return strokeWidth / scaleFactor;
}

Path& LegacyRenderSVGShape::ensurePath()
{
    if (!hasPath()) {
        m_path = createPath();
        m_path->setNotTransient();
    }
    return path();
}

std::unique_ptr<Path> LegacyRenderSVGShape::createPath() const
{
    return makeUnique<Path>(pathFromGraphicsElement(protectedGraphicsElement()));
}

}
