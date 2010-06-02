/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2009 Google, Inc.  All rights reserved.
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
#include "RenderSVGContainer.h"

#include "GraphicsContext.h"
#include "RenderSVGResourceFilter.h"
#include "RenderView.h"
#include "SVGRenderSupport.h"
#include "SVGStyledElement.h"

namespace WebCore {

RenderSVGContainer::RenderSVGContainer(SVGStyledElement* node)
    : RenderSVGModelObject(node)
    , m_drawsContents(true)
{
}

bool RenderSVGContainer::drawsContents() const
{
    return m_drawsContents;
}

void RenderSVGContainer::setDrawsContents(bool drawsContents)
{
    m_drawsContents = drawsContents;
}

void RenderSVGContainer::layout()
{
    ASSERT(needsLayout());
    ASSERT(!view()->layoutStateEnabled()); // RenderSVGRoot disables layoutState for the SVG rendering tree.

    calcViewport(); // Allow RenderSVGViewportContainer to update its viewport

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() || selfWillPaint());
    calculateLocalTransform(); // Allow RenderSVGTransformableContainer to update its transform

    layoutChildren(this, selfNeedsLayout());
    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

bool RenderSVGContainer::selfWillPaint() const
{
#if ENABLE(FILTERS)
    const SVGRenderStyle* svgStyle = style()->svgStyle();
    RenderSVGResourceFilter* filter = getRenderSVGResourceById<RenderSVGResourceFilter>(document(), svgStyle->filterResource());
    if (filter)
        return true;
#endif
    return false;
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled() || !drawsContents())
        return;

    // Spec: groups w/o children still may render filter content.
    if (!firstChild() && !selfWillPaint())
        return;

    PaintInfo childPaintInfo(paintInfo);

    childPaintInfo.context->save();

    // Let the RenderSVGViewportContainer subclass clip if necessary
    applyViewportClip(childPaintInfo);

    applyTransformToPaintInfo(childPaintInfo, localToParentTransform());

    RenderSVGResourceFilter* filter = 0;
    FloatRect boundingBox = repaintRectInLocalCoordinates();

    bool continueRendering = true;
    if (childPaintInfo.phase == PaintPhaseForeground)
        continueRendering = prepareToRenderSVGContent(this, childPaintInfo, boundingBox, filter);

    if (continueRendering) {
        childPaintInfo.paintingRoot = paintingRootForChildren(childPaintInfo);
        for (RenderObject* child = firstChild(); child; child = child->nextSibling())
            child->paint(childPaintInfo, 0, 0);
    }

    if (paintInfo.phase == PaintPhaseForeground)
        finishRenderSVGContent(this, childPaintInfo, filter, paintInfo.context);

    childPaintInfo.context->restore();

    // FIXME: This really should be drawn from local coordinates, but currently we hack it
    // to avoid our clip killing our outline rect.  Thus we translate our
    // outline rect into parent coords before drawing.
    // FIXME: This means our focus ring won't share our rotation like it should.
    // We should instead disable our clip during PaintPhaseOutline
    IntRect paintRectInParent = enclosingIntRect(localToParentTransform().mapRect(repaintRectInLocalCoordinates()));
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, paintRectInParent.x(), paintRectInParent.y(), paintRectInParent.width(), paintRectInParent.height());
}

// addFocusRingRects is called from paintOutline and needs to be in the same coordinates as the paintOuline call
void RenderSVGContainer::addFocusRingRects(Vector<IntRect>& rects, int, int)
{
    IntRect paintRectInParent = enclosingIntRect(localToParentTransform().mapRect(repaintRectInLocalCoordinates()));
    if (!paintRectInParent.isEmpty())
        rects.append(paintRectInParent);
}

FloatRect RenderSVGContainer::objectBoundingBox() const
{
    return computeContainerBoundingBox(this, false);
}

FloatRect RenderSVGContainer::strokeBoundingBox() const
{
    return computeContainerBoundingBox(this, true);
}

// RenderSVGContainer is used for <g> elements which do not themselves have a
// width or height, so we union all of our child rects as our repaint rect.
FloatRect RenderSVGContainer::repaintRectInLocalCoordinates() const
{
    FloatRect repaintRect = computeContainerBoundingBox(this, true);
    intersectRepaintRectWithResources(this, repaintRect);

    return repaintRect;
}

bool RenderSVGContainer::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // Give RenderSVGViewportContainer a chance to apply its viewport clip
    if (!pointIsInsideViewportClip(pointInParent))
        return false;

    FloatPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtFloatPoint(request, result, localPoint, hitTestAction)) {
            updateHitTestResult(result, roundedIntPoint(localPoint));
            return true;
        }
    }

    // Spec: Only graphical elements can be targeted by the mouse, period.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

}

#endif // ENABLE(SVG)
