/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved. 
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
#include "RenderSVGForeignObject.h"

#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "RenderView.h"
#include "SVGForeignObjectElement.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGForeignObject);

RenderSVGForeignObject::RenderSVGForeignObject(SVGForeignObjectElement& element, RenderStyle&& style)
    : RenderSVGBlock(element, WTFMove(style))
{
}

RenderSVGForeignObject::~RenderSVGForeignObject() = default;

SVGForeignObjectElement& RenderSVGForeignObject::foreignObjectElement() const
{
    return downcast<SVGForeignObjectElement>(RenderSVGBlock::graphicsElement());
}

void RenderSVGForeignObject::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (paintInfo.context().paintingDisabled())
        return;

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection)
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(childPaintInfo.context());
    childPaintInfo.applyTransform(localTransform());

    if (SVGRenderSupport::isOverflowHidden(*this))
        childPaintInfo.context().clip(m_viewport);

    SVGRenderingContext renderingContext;
    if (paintInfo.phase == PaintPhase::Foreground) {
        renderingContext.prepareToRenderSVGContent(*this, childPaintInfo);
        if (!renderingContext.isRenderingPrepared())
            return;
    }

    LayoutPoint childPoint = IntPoint();
    if (paintInfo.phase == PaintPhase::Selection) {
        RenderBlock::paint(childPaintInfo, childPoint);
        return;
    }

    // Paint all phases of FO elements atomically, as though the FO element established its
    // own stacking context.
    childPaintInfo.phase = PaintPhase::BlockBackground;
    RenderBlock::paint(childPaintInfo, childPoint);
    childPaintInfo.phase = PaintPhase::ChildBlockBackgrounds;
    RenderBlock::paint(childPaintInfo, childPoint);
    childPaintInfo.phase = PaintPhase::Float;
    RenderBlock::paint(childPaintInfo, childPoint);
    childPaintInfo.phase = PaintPhase::Foreground;
    RenderBlock::paint(childPaintInfo, childPoint);
    childPaintInfo.phase = PaintPhase::Outline;
    RenderBlock::paint(childPaintInfo, childPoint);
}

LayoutRect RenderSVGForeignObject::clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(*this, repaintContainer);
}

Optional<FloatRect> RenderSVGForeignObject::computeFloatVisibleRectInContainer(const FloatRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    return SVGRenderSupport::computeFloatVisibleRectInContainer(*this, rect, container, context);
}

Optional<LayoutRect> RenderSVGForeignObject::computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    Optional<FloatRect> adjustedRect = computeFloatVisibleRectInContainer(rect, container, context);
    if (adjustedRect)
        return enclosingLayoutRect(*adjustedRect);
    return WTF::nullopt;
}

const AffineTransform& RenderSVGForeignObject::localToParentTransform() const
{
    m_localToParentTransform = localTransform();
    m_localToParentTransform.translate(m_viewport.location());
    return m_localToParentTransform;
}

void RenderSVGForeignObject::updateLogicalWidth()
{
    // FIXME: Investigate in size rounding issues
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    setWidth(static_cast<int>(roundf(m_viewport.width())));
}

RenderBox::LogicalExtentComputedValues RenderSVGForeignObject::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop) const
{
    // FIXME: Investigate in size rounding issues
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    // FIXME: Is this correct for vertical writing mode?
    return { static_cast<int>(roundf(m_viewport.height())), logicalTop, ComputedMarginValues() };
}

void RenderSVGForeignObject::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());
    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled()); // RenderSVGRoot disables paint offset cache for the SVG rendering tree.

    LayoutRepainter repainter(*this, SVGRenderSupport::checkForSVGRepaintDuringLayout(*this));

    bool updateCachedBoundariesInParents = false;
    if (m_needsTransformUpdate) {
        m_localTransform = foreignObjectElement().animatedLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    FloatRect oldViewport = m_viewport;

    // Cache viewport boundaries
    SVGLengthContext lengthContext(&foreignObjectElement());
    FloatPoint viewportLocation(foreignObjectElement().x().value(lengthContext), foreignObjectElement().y().value(lengthContext));
    m_viewport = FloatRect(viewportLocation, FloatSize(foreignObjectElement().width().value(lengthContext), foreignObjectElement().height().value(lengthContext)));
    if (!updateCachedBoundariesInParents)
        updateCachedBoundariesInParents = oldViewport != m_viewport;

    // Set box origin to the foreignObject x/y translation, so positioned objects in XHTML content get correct
    // positions. A regular RenderBoxModelObject would pull this information from RenderStyle - in SVG those
    // properties are ignored for non <svg> elements, so we mimic what happens when specifying them through CSS.

    // FIXME: Investigate in location rounding issues - only affects RenderSVGForeignObject & RenderSVGText
    setLocation(roundedIntPoint(viewportLocation));

    bool layoutChanged = everHadLayout() && selfNeedsLayout();
    RenderBlock::layout();
    ASSERT(!needsLayout());

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents)
        RenderSVGBlock::setNeedsBoundariesUpdate();

    // Invalidate all resources of this client if our layout changed.
    if (layoutChanged)
        SVGResourcesCache::clientLayoutChanged(*this);

    repainter.repaintAfterLayout();
}

bool RenderSVGForeignObject::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // Embedded content is drawn in the foreground phase.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint = localTransform().inverse().value_or(AffineTransform()).mapPoint(pointInParent);

    // Early exit if local point is not contained in clipped viewport area
    if (SVGRenderSupport::isOverflowHidden(*this) && !m_viewport.contains(localPoint))
        return false;

    // FOs establish a stacking context, so we need to hit-test all layers.
    HitTestLocation hitTestLocation(localPoint);
    return RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), HitTestForeground)
        || RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), HitTestFloat)
        || RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), HitTestChildBlockBackgrounds);
}

bool RenderSVGForeignObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

void RenderSVGForeignObject::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags, bool* wasFixed) const
{
    SVGRenderSupport::mapLocalToContainer(*this, repaintContainer, transformState, wasFixed);
}

const RenderObject* RenderSVGForeignObject::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    return SVGRenderSupport::pushMappingToContainer(*this, ancestorToStopAt, geometryMap);
}

}
