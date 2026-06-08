/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc. All rights reserved.
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

#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "RenderElementStyleInlines.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGContainerLayout.h"
#include "SVGLayerTransformUpdater.h"
#include "SVGRenderSupport.h"
#include "SVGVisitedRendererTracking.h"
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGContainer);

RenderSVGContainer::RenderSVGContainer(Type type, Document& document, RenderStyle&& style, OptionSet<SVGModelObjectFlag> svgFlags)
    : RenderSVGModelObject(type, document, WTF::move(style), svgFlags | SVGModelObjectFlag::IsContainer)
{
}

RenderSVGContainer::RenderSVGContainer(Type type, SVGElement& element, RenderStyle&& style, OptionSet<SVGModelObjectFlag> svgFlags)
    : RenderSVGModelObject(type, element, WTF::move(style), svgFlags | SVGModelObjectFlag::IsContainer)
{
}

RenderSVGContainer::~RenderSVGContainer() = default;

void RenderSVGContainer::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    auto checkForRepaintOverride = isRenderSVGResourceMarker() ? std::make_optional(LayoutRepainter::CheckForRepaint::No) : std::nullopt;
    LayoutRepainter repainter(*this, checkForRepaintOverride);

    // Update layer transform before laying out children (SVG needs access to the transform matrices during layout for on-screen text font-size calculations).
    // Eventually re-update if the transform reference box, relevant for transform-origin, has changed during layout.
    //
    // FIXME: LBSE should not repeat the same mistake -- remove the on-screen text font-size hacks that predate the modern solutions to this.
    {
        ASSERT(!m_isLayoutSizeChanged);
        SetForScope trackLayoutSizeChanges(m_isLayoutSizeChanged, updateLayoutSizeIfNeeded());

        ASSERT(!m_didTransformToRootUpdate);
        SVGLayerTransformUpdater transformUpdater(*this);
        SetForScope trackTransformChanges(m_didTransformToRootUpdate, transformUpdater.layerTransformChanged() || SVGContainerLayout::transformToRootChanged(parent()));
        layoutChildren();
    }

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

void RenderSVGContainer::layoutChildren()
{
    SVGContainerLayout containerLayout(*this);
    containerLayout.layoutChildren(selfNeedsLayout());

    SVGBoundingBoxComputation boundingBoxComputation(*this);
    // objectBoundingBox / strokeBoundingBox are recomputed lazily (see
    // updateSVGTransformDependentBoundingBoxesIfNeeded). Layout only needs the without-transform
    // box below for currentSVGLayoutRect, so just mark them dirty rather than pay a full subtree
    // walk that is usually never read before the next layout.
    m_transformDependentBoundingBoxesDirty = true;
    m_strokeBoundingBox = std::nullopt;
    m_cachedVisualOverflowRect = std::nullopt;

    if (auto objectBoundingBoxWithoutTransformations = overridenObjectBoundingBoxWithoutTransformations())
        m_objectBoundingBoxWithoutTransformations = objectBoundingBoxWithoutTransformations.value();
    else {
        constexpr auto objectBoundingBoxDecorationWithoutTransformations = SVGBoundingBoxComputation::objectBoundingBoxDecoration | SVGBoundingBoxComputation::DecorationOption::IgnoreTransformations;
        m_objectBoundingBoxWithoutTransformations = boundingBoxComputation.computeDecoratedBoundingBox(objectBoundingBoxDecorationWithoutTransformations);
    }

    setCurrentSVGLayoutRect(enclosingLayoutRect(m_objectBoundingBoxWithoutTransformations));

    containerLayout.positionChildrenRelativeToContainer();
}

void RenderSVGContainer::updateSVGTransformDependentBoundingBoxesIfNeeded() const
{
    SVGBoundingBoxComputation::recomputeTransformDependentBoundingBoxes(*this, m_transformDependentBoundingBoxesDirty, m_objectBoundingBox, m_strokeBoundingBox, &m_objectBoundingBoxValid);
}

FloatRect RenderSVGContainer::strokeBoundingBox() const
{
    updateSVGTransformDependentBoundingBoxesIfNeeded();
    if (!m_strokeBoundingBox) {
        // Initialize m_strokeBoundingBox before calling computeDecoratedBoundingBox, since recursively referenced markers can cause us to re-enter here.
        m_strokeBoundingBox = FloatRect { };
        SVGBoundingBoxComputation boundingBoxComputation(*this);
        m_strokeBoundingBox = boundingBoxComputation.computeDecoratedBoundingBox(SVGBoundingBoxComputation::strokeBoundingBoxDecoration);
    }
    return *m_strokeBoundingBox;
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhase::EventRegion && paintInfo.context().paintingDisabled())
        return;

    static constexpr OptionSet<PaintPhase> relevantPaintPhases { PaintPhase::Foreground, PaintPhase::ClippingMask, PaintPhase::Mask, PaintPhase::Outline, PaintPhase::SelfOutline, PaintPhase::EventRegion };
    if (!relevantPaintPhases.contains(paintInfo.phase))
        return;

    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    if (style().display() == Style::DisplayType::None)
        return;

    // Children can override with "visibility: visible", per SVG spec.
    if (paintInfo.phase != PaintPhase::Foreground && style().usedVisibility() == Visibility::Hidden)
        return;

    if (paintInfo.phase == PaintPhase::ClippingMask) {
        paintSVGClippingMask(paintInfo, objectBoundingBox());
        return;
    }

    auto adjustedPaintOffset = paintOffset + currentSVGLayoutLocation();
    if (paintInfo.phase == PaintPhase::Mask) {
        paintSVGMask(paintInfo, adjustedPaintOffset);
        return;
    }

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedPaintOffset);
    if (!visualOverflowRect.intersects(paintInfo.rect))
        return;

    if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
        // Children's outlines are painted per-child during the Foreground phase, so later
        // DOM siblings paint on top of earlier siblings' outlines.
        paintSVGOutline(paintInfo, adjustedPaintOffset);
        return;
    }

    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::EventRegion);

    // With a self-painting layer, children are painted by paintChildrenInDOMOrderForSVG().
    if (hasSelfPaintingLayer())
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(childPaintInfo.context());

    // For layer-backed containers, clipping is handled by RenderLayer::calculateClipRects().
    if (isRenderSVGViewportContainer() && SVGRenderSupport::isOverflowHidden(*this))
        childPaintInfo.context().clip(FloatRect(overflowClipRect(adjustedPaintOffset)));

    childPaintInfo.updateSubtreePaintRootForChildren(this);
    for (CheckedRef child : childrenOfType<RenderElement>(*this)) {
        if (child->hasSelfPaintingLayer())
            continue;

        child->paint(childPaintInfo, adjustedPaintOffset);

        if (paintInfo.phase == PaintPhase::Foreground) {
            // Paint each child's outline immediately so later DOM siblings paint on top of it.
            PaintInfo outlinePaintInfo(childPaintInfo);
            outlinePaintInfo.phase = PaintPhase::Outline;
            child->paint(outlinePaintInfo, adjustedPaintOffset);
        }
    }
}

bool RenderSVGContainer::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    auto adjustedLocation = accumulatedOffset + currentSVGLayoutLocation();

    auto visualOverflowRect = visualOverflowRectEquivalent();
    visualOverflowRect.moveBy(adjustedLocation);
    if (!locationInContainer.intersects(visualOverflowRect))
        return false;

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return false;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    auto localPoint = locationInContainer.point();
    auto coordinateSystemOriginTranslation = nominalSVGLayoutLocation() - adjustedLocation;
    localPoint.move(coordinateSystemOriginTranslation);

    if (!pointInSVGClippingArea(localPoint))
        return false;

    // Give RenderSVGViewportContainer a chance to apply its viewport clip.
    if (!pointIsInsideViewportClip(localPoint))
        return false;


    for (CheckedPtr child = lastChild(); child; child = child->previousSibling()) {
        if (!child->hasLayer() && child->nodeAtPoint(request, result, locationInContainer, adjustedLocation, hitTestAction)) {
            updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
            if (result.addNodeToListBasedTestResult(protect(child->node()).get(), request, locationInContainer, visualOverflowRect) == HitTestProgress::Stop)
                return true;
        }
    }

    // Accessibility wants to return SVG containers, if appropriate.
    if (request.type() & HitTestRequest::Type::AccessibilityHitTest && objectBoundingBox().contains(localPoint)) {
        updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(protect(nodeForHitTest()).get(), request, locationInContainer, visualOverflowRect) == HitTestProgress::Stop)
            return true;
    }

    // pointer-events=bounding-box makes it possible for containers to be direct targets.
    if (style().pointerEvents() == PointerEvents::BoundingBox) {
        if (!isObjectBoundingBoxValid())
            return false;
        if (objectBoundingBox().contains(localPoint)) {
            updateHitTestResult(result, LayoutPoint(localPoint));
            return true;
        }
    }
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

void RenderSVGContainer::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* container) const
{
    if (needsHasSVGTransformFlags())
        return RenderSVGModelObject::addFocusRingRects(rects, additionalOffset, container);
    auto repaintBoundingBox = enclosingLayoutRect(repaintRectInLocalCoordinates());
    if (repaintBoundingBox.size().isEmpty())
        return;
    rects.append(repaintBoundingBox);
}

}

