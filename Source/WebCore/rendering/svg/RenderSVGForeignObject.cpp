/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#if ENABLE(SVG)
#include "RenderSVGForeignObject.h"

#include "GraphicsContext.h"
#include "LayoutRepainter.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "RenderView.h"
#include "SVGForeignObjectElement.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include "SVGSVGElement.h"
#include "TransformState.h"

namespace WebCore {

RenderSVGForeignObject::RenderSVGForeignObject(SVGForeignObjectElement* node) 
    : RenderSVGBlock(node)
    , m_needsTransformUpdate(true)
{
}

RenderSVGForeignObject::~RenderSVGForeignObject()
{
}

void RenderSVGForeignObject::paint(PaintInfo& paintInfo, const LayoutPoint&)
{
    if (paintInfo.context->paintingDisabled()
        || (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection))
        return;

    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(*childPaintInfo.context);
    childPaintInfo.applyTransform(localTransform());

    if (SVGRenderSupport::isOverflowHidden(this))
        childPaintInfo.context->clip(m_viewport);

    SVGRenderingContext renderingContext;
    bool continueRendering = true;
    if (paintInfo.phase == PaintPhaseForeground) {
        renderingContext.prepareToRenderSVGContent(this, childPaintInfo);
        continueRendering = renderingContext.isRenderingPrepared();
    }

    if (continueRendering) {
        // Paint all phases of FO elements atomically, as though the FO element established its
        // own stacking context.
        bool preservePhase = paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip;
        LayoutPoint childPoint = IntPoint();
        childPaintInfo.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
        RenderBlock::paint(childPaintInfo, IntPoint());
        if (!preservePhase) {
            childPaintInfo.phase = PaintPhaseChildBlockBackgrounds;
            RenderBlock::paint(childPaintInfo, childPoint);
            childPaintInfo.phase = PaintPhaseFloat;
            RenderBlock::paint(childPaintInfo, childPoint);
            childPaintInfo.phase = PaintPhaseForeground;
            RenderBlock::paint(childPaintInfo, childPoint);
            childPaintInfo.phase = PaintPhaseOutline;
            RenderBlock::paint(childPaintInfo, childPoint);
        }
    }
}

LayoutRect RenderSVGForeignObject::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer) const
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderSVGForeignObject::computeFloatRectForRepaint(RenderBoxModelObject* repaintContainer, FloatRect& repaintRect, bool fixed) const
{
    SVGRenderSupport::computeFloatRectForRepaint(this, repaintContainer, repaintRect, fixed);
}

const AffineTransform& RenderSVGForeignObject::localToParentTransform() const
{
    m_localToParentTransform = localTransform();
    m_localToParentTransform.translate(m_viewport.x(), m_viewport.y());
    return m_localToParentTransform;
}

void RenderSVGForeignObject::computeLogicalWidth()
{
    // FIXME: Investigate in size rounding issues
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    setWidth(static_cast<int>(roundf(m_viewport.width())));
}

void RenderSVGForeignObject::computeLogicalHeight()
{
    // FIXME: Investigate in size rounding issues
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    setHeight(static_cast<int>(roundf(m_viewport.height())));
}

void RenderSVGForeignObject::layout()
{
    ASSERT(needsLayout());
    ASSERT(!view()->layoutStateEnabled()); // RenderSVGRoot disables layoutState for the SVG rendering tree.

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    SVGForeignObjectElement* foreign = static_cast<SVGForeignObjectElement*>(node());

    bool updateCachedBoundariesInParents = false;
    if (m_needsTransformUpdate) {
        m_localTransform = foreign->animatedLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    FloatRect oldViewport = m_viewport;

    // Cache viewport boundaries
    SVGLengthContext lengthContext(foreign);
    FloatPoint viewportLocation(foreign->x().value(lengthContext), foreign->y().value(lengthContext));
    m_viewport = FloatRect(viewportLocation, FloatSize(foreign->width().value(lengthContext), foreign->height().value(lengthContext)));
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
        SVGResourcesCache::clientLayoutChanged(this);

    repainter.repaintAfterLayout();
}

bool RenderSVGForeignObject::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // Embedded content is drawn in the foreground phase.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint = localTransform().inverse().mapPoint(pointInParent);

    // Early exit if local point is not contained in clipped viewport area
    if (SVGRenderSupport::isOverflowHidden(this) && !m_viewport.contains(localPoint))
        return false;

    // FOs establish a stacking context, so we need to hit-test all layers.
    return RenderBlock::nodeAtPoint(request, result, roundedLayoutPoint(localPoint), LayoutPoint(), HitTestForeground)
        || RenderBlock::nodeAtPoint(request, result, roundedLayoutPoint(localPoint), LayoutPoint(), HitTestFloat)
        || RenderBlock::nodeAtPoint(request, result, roundedLayoutPoint(localPoint), LayoutPoint(), HitTestChildBlockBackgrounds);
}

bool RenderSVGForeignObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, const LayoutPoint&, const LayoutPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

void RenderSVGForeignObject::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool /* fixed */, bool /* useTransforms */, TransformState& transformState, ApplyContainerFlipOrNot, bool* wasFixed) const
{
    SVGRenderSupport::mapLocalToContainer(this, repaintContainer, transformState, wasFixed);
}

const RenderObject* RenderSVGForeignObject::pushMappingToContainer(const RenderBoxModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    return SVGRenderSupport::pushMappingToContainer(this, ancestorToStopAt, geometryMap);
}

}

#endif
