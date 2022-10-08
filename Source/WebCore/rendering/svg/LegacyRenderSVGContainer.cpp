/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "LegacyRenderSVGContainer.h"

#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "RenderIterator.h"
#include "RenderSVGResourceFilter.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGContainer);

LegacyRenderSVGContainer::LegacyRenderSVGContainer(SVGElement& element, RenderStyle&& style)
    : LegacyRenderSVGModelObject(element, WTFMove(style))
{
}

LegacyRenderSVGContainer::~LegacyRenderSVGContainer() = default;

void LegacyRenderSVGContainer::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    // LegacyRenderSVGRoot disables paint offset cache for the SVG rendering tree.
    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled());

    LayoutRepainter repainter(*this, SVGRenderSupport::checkForSVGRepaintDuringLayout(*this) || selfWillPaint(), RepaintOutlineBounds::No);

    // Allow LegacyRenderSVGViewportContainer to update its viewport.
    calcViewport();

    // Allow LegacyRenderSVGTransformableContainer to update its transform.
    bool updatedTransform = calculateLocalTransform();

    // LegacyRenderSVGViewportContainer needs to set the 'layout size changed' flag.
    determineIfLayoutSizeChanged();

    SVGRenderSupport::layoutChildren(*this, selfNeedsLayout() || SVGRenderSupport::filtersForceContainerLayout(*this));

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && needsLayout())
        SVGResourcesCache::clientLayoutChanged(*this);

    // At this point LayoutRepainter already grabbed the old bounds,
    // recalculate them now so repaintAfterLayout() uses the new bounds.
    if (m_needsBoundariesUpdate || updatedTransform) {
        updateCachedBoundaries();
        m_needsBoundariesUpdate = false;

        // New bounds can affect transforms, so recompute them here if needed.
        calculateLocalTransform();

        // If our bounds changed, notify the parents.
        LegacyRenderSVGModelObject::setNeedsBoundariesUpdate();
    }

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

bool LegacyRenderSVGContainer::selfWillPaint()
{
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*this);
    return resources && resources->filter();
}

void LegacyRenderSVGContainer::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (paintInfo.context().paintingDisabled())
        return;

    // Spec: groups w/o children still may render filter content.
    if (!firstChild() && !selfWillPaint())
        return;

    FloatRect repaintRect = repaintRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsRepaintRect(repaintRect, localToParentTransform(), paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);
    {
        GraphicsContextStateSaver stateSaver(childPaintInfo.context());

        // Let the LegacyRenderSVGViewportContainer subclass clip if necessary
        applyViewportClip(childPaintInfo);

        childPaintInfo.applyTransform(localToParentTransform());

        SVGRenderingContext renderingContext;
        bool continueRendering = true;
        if (childPaintInfo.phase == PaintPhase::Foreground) {
            renderingContext.prepareToRenderSVGContent(*this, childPaintInfo);
            continueRendering = renderingContext.isRenderingPrepared();
        }

        if (continueRendering) {
            childPaintInfo.updateSubtreePaintRootForChildren(this);
            for (auto& child : childrenOfType<RenderElement>(*this))
                child.paint(childPaintInfo, IntPoint());
        }
    }
    
    // FIXME: This really should be drawn from local coordinates, but currently we hack it
    // to avoid our clip killing our outline rect. Thus we translate our
    // outline rect into parent coords before drawing.
    // FIXME: This means our focus ring won't share our rotation like it should.
    // We should instead disable our clip during PaintPhase::Outline
    if (paintInfo.phase == PaintPhase::SelfOutline && style().outlineWidth() && style().visibility() == Visibility::Visible) {
        IntRect paintRectInParent = enclosingIntRect(localToParentTransform().mapRect(repaintRect));
        paintOutline(paintInfo, paintRectInParent);
    }
}

// addFocusRingRects is called from paintOutline and needs to be in the same coordinates as the paintOuline call
void LegacyRenderSVGContainer::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint&, const RenderLayerModelObject*) const
{
    LayoutRect paintRectInParent = LayoutRect(localToParentTransform().mapRect(repaintRectInLocalCoordinates()));
    if (!paintRectInParent.isEmpty())
        rects.append(paintRectInParent);
}

void LegacyRenderSVGContainer::updateCachedBoundaries()
{
    SVGRenderSupport::computeContainerBoundingBoxes(*this, m_objectBoundingBox, m_objectBoundingBoxValid, m_strokeBoundingBox, m_repaintBoundingBox);
    SVGRenderSupport::intersectRepaintRectWithResources(*this, m_repaintBoundingBox);
}

bool LegacyRenderSVGContainer::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // Give LegacyRenderSVGViewportContainer a chance to apply its viewport clip
    if (!pointIsInsideViewportClip(pointInParent))
        return false;

    FloatPoint localPoint = valueOrDefault(localToParentTransform().inverse()).mapPoint(pointInParent);

    if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
        return false;

    SVGHitTestCycleDetectionScope hitTestScope(*this);

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtFloatPoint(request, result, localPoint, hitTestAction)) {
            updateHitTestResult(result, LayoutPoint(localPoint));
            if (result.addNodeToListBasedTestResult(child->node(), request, flooredLayoutPoint(localPoint)) == HitTestProgress::Stop)
                return true;
        }
    }

    // Accessibility wants to return SVG containers, if appropriate.
    if (request.type() & HitTestRequest::Type::AccessibilityHitTest && m_objectBoundingBox.contains(localPoint)) {
        updateHitTestResult(result, LayoutPoint(localPoint));
        if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, flooredLayoutPoint(localPoint)) == HitTestProgress::Stop)
            return true;
    }
    
    // Spec: Only graphical elements can be targeted by the mouse, period.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

}
