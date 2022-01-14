/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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
#include "RenderSVGContainer.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGContainerLayout.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGContainer);

RenderSVGContainer::RenderSVGContainer(SVGElement& element, RenderStyle&& style)
    : RenderSVGModelObject(element, WTFMove(style))
{
}

RenderSVGContainer::~RenderSVGContainer() = default;

void RenderSVGContainer::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    calculateViewport();

    // Update layer transform before laying out children (SVG needs access to the transform matrices during layout for on-screen text font-size calculations).
    // Eventually re-update if the transform reference box, relevant for transform-origin, has changed during layout.
    {
        // FIXME: [LBSE] Upstream SVGLayerTransformUpdater
        // SVGLayerTransformUpdater transformUpdater(*this);
        layoutChildren();
    }

    updateLayerInformation();

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && needsLayout())
        SVGResourcesCache::clientLayoutChanged(*this);

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGContainer::calculateViewport()
{
    // FIXME: [LBSE] Upstream SVGLengthContext changes
    // element().updateLengthContext();
}

void RenderSVGContainer::layoutChildren()
{
    SVGContainerLayout containerLayout(*this);
    containerLayout.layoutChildren(selfNeedsLayout() || SVGRenderSupport::filtersForceContainerLayout(*this));

    SVGBoundingBoxComputation boundingBoxComputation(*this);
    m_objectBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::objectBoundingBoxDecoration, &m_objectBoundingBoxValid);
    m_strokeBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::strokeBoundingBoxDecoration);
    setLayoutRect(enclosingLayoutRect(m_objectBoundingBox));

    containerLayout.positionChildrenRelativeToContainer();
}

void RenderSVGContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderSVGModelObject::styleDidChange(diff, oldStyle);

    // FIXME: [LBSE] Upstream RenderLayer changes
    // if (hasLayer())
    //     layer()->setIsOpportunisticStackingContext(true);
}

bool RenderSVGContainer::selfWillPaint()
{
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
    return resources && resources->filter();
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.context().paintingDisabled())
        return;

    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    if (style().display() == DisplayType::None)
        return;

    // FIXME: [LBSE] Upstream SVGRenderSupport changes
    // if (!SVGRenderSupport::shouldPaintHiddenRenderer(*this))
    //     return;

    // Spec: groups w/o children still may render filter content.
    if (!firstChild() && !selfWillPaint())
        return;

    if (paintInfo.phase == PaintPhase::ClippingMask) {
        // FIXME: [LBSE] Upstream SVGRenderSupport changes
        // SVGRenderSupport::paintSVGClippingMask(*this, paintInfo);
        return;
    }

    auto adjustedPaintOffset = paintOffset + layoutLocation();
    if (paintInfo.phase == PaintPhase::Mask) {
        // FIXME: [LBSE] Upstream SVGRenderSupport changes
        // SVGRenderSupport::paintSVGMask(*this, paintInfo, adjustedPaintOffset);
        return;
    }

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedPaintOffset);
    if (!visualOverflowRect.intersects(paintInfo.rect))
        return;

    if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
        // FIXME: [LBSE] Upstream outline painting
        // paintSVGOutline(paintInfo, adjustedPaintOffset);
    }
}

bool RenderSVGContainer::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    auto adjustedLocation = accumulatedOffset + layoutLocation();

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedLocation);
    if (!locationInContainer.intersects(visualOverflowRect))
        return false;

    auto localPoint = locationInContainer.point();
    auto boundingBoxTopLeftCorner = flooredLayoutPoint(objectBoundingBox().minXMinYCorner());
    auto coordinateSystemOriginTranslation = boundingBoxTopLeftCorner - adjustedLocation;
    localPoint.move(coordinateSystemOriginTranslation);

    if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
        return false;

    // Give RenderSVGViewportContainer a chance to apply its viewport clip.
    if (!pointIsInsideViewportClip(localPoint))
        return false;

    SVGHitTestCycleDetectionScope hitTestScope(*this);
    for (auto* child = lastChild(); child; child = child->previousSibling()) {
        if (!child->hasLayer() && child->nodeAtPoint(request, result, locationInContainer, adjustedLocation, hitTestAction)) {
            updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
            if (result.addNodeToListBasedTestResult(child->node(), request, locationInContainer, visualOverflowRect) == HitTestProgress::Stop)
                return true;
        }
    }

    // Accessibility wants to return SVG containers, if appropriate.
    if (request.type() & HitTestRequest::Type::AccessibilityHitTest && m_objectBoundingBox.contains(localPoint)) {
        updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer, visualOverflowRect) == HitTestProgress::Stop)
            return true;
    }
    
    // Spec: Only graphical elements can be targeted by the mouse, period.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
