/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2009 Google, Inc.  All rights reserved.

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

#include "AXObjectCache.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "RenderView.h"
#include "SVGRenderSupport.h"
#include "SVGResourceFilter.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

namespace WebCore {

RenderSVGContainer::RenderSVGContainer(SVGStyledElement* node)
    : RenderSVGModelObject(node)
    , m_drawsContents(true)
{
}

RenderSVGContainer::~RenderSVGContainer()
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

bool RenderSVGContainer::calculateLocalTransform()
{
    // subclasses can override this to add transform support
    return false;
}

void RenderSVGContainer::layout()
{
    ASSERT(needsLayout());

    // Arbitrary affine transforms are incompatible with LayoutState.
    view()->disableLayoutState();

    // FIXME: using m_absoluteBounds breaks if containerForRepaint() is not the root
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() && selfWillPaint(), &m_absoluteBounds);

    calculateLocalTransform();

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        // Only force our kids to layout if we're being asked to relayout as a result of a parent changing
        // FIXME: We should be able to skip relayout of non-relative kids when only bounds size has changed
        // that's a possible future optimization using LayoutState
        // http://bugs.webkit.org/show_bug.cgi?id=15391
        if (selfNeedsLayout())
            child->setNeedsLayout(true);

        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
    }

    m_absoluteBounds = absoluteClippedOverflowRect();

    repainter.repaintAfterLayout();

    view()->enableLayoutState();
    setNeedsLayout(false);
}

void RenderSVGContainer::applyContentTransforms(PaintInfo& paintInfo)
{
    if (!localTransform().isIdentity())
        paintInfo.context->concatCTM(localTransform());
}

void RenderSVGContainer::applyAdditionalTransforms(PaintInfo&)
{
    // no-op
}

bool RenderSVGContainer::selfWillPaint() const
{
#if ENABLE(SVG_FILTERS)
    const SVGRenderStyle* svgStyle = style()->svgStyle();
    SVGResourceFilter* filter = getFilterById(document(), svgStyle->filter());
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
    
    paintInfo.context->save();
    applyContentTransforms(paintInfo);

    SVGResourceFilter* filter = 0;
    PaintInfo savedInfo(paintInfo);

    FloatRect boundingBox = repaintRectInLocalCoordinates();
    if (paintInfo.phase == PaintPhaseForeground)
        prepareToRenderSVGContent(this, paintInfo, boundingBox, filter); 

    applyAdditionalTransforms(paintInfo);

    // default implementation. Just pass paint through to the children
    PaintInfo childInfo(paintInfo);
    childInfo.paintingRoot = paintingRootForChildren(paintInfo);
    for (RenderObject* child = firstChild(); child; child = child->nextSibling())
        child->paint(childInfo, 0, 0);

    if (paintInfo.phase == PaintPhaseForeground)
        finishRenderSVGContent(this, paintInfo, boundingBox, filter, savedInfo.context);

    paintInfo.context->restore();

    // FIXME: This really should be drawn from local coordinates, but currently we hack it
    // to avoid our clip killing our outline rect.  Thus we translate our
    // outline rect into parent coords before drawing.
    // FIXME: This means our focus ring won't share our rotation like it should.
    // We should instead disable our clip during PaintPhaseOutline
    IntRect paintRectInParent = enclosingIntRect(localToParentTransform().mapRect(repaintRectInLocalCoordinates()));
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, paintRectInParent.x(), paintRectInParent.y(), paintRectInParent.width(), paintRectInParent.height(), style());
}

// addFocusRingRects is called from paintOutline and needs to be in the same coordinates as the paintOuline call
void RenderSVGContainer::addFocusRingRects(GraphicsContext* graphicsContext, int, int)
{
    IntRect paintRectInParent = enclosingIntRect(localToParentTransform().mapRect(repaintRectInLocalCoordinates()));
    graphicsContext->addFocusRingRect(paintRectInParent);
}

FloatRect RenderSVGContainer::objectBoundingBox() const
{
    return computeContainerBoundingBox(this, false);
}

// RenderSVGContainer is used for <g> elements which do not themselves have a
// width or height, so we union all of our child rects as our repaint rect.
FloatRect RenderSVGContainer::repaintRectInLocalCoordinates() const
{
    FloatRect repaintRect = computeContainerBoundingBox(this, true);

    // A filter on this container can paint outside of the union of the child repaint rects
    repaintRect.unite(filterBoundingBox());

    return repaintRect;
}

bool RenderSVGContainer::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtPoint(request, result, _x, _y, _tx, _ty, hitTestAction)) {
            updateHitTestResult(result, IntPoint(_x - _tx, _y - _ty));
            return true;
        }
    }

    // Spec: Only graphical elements can be targeted by the mouse, period.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

IntRect RenderSVGContainer::outlineBoundsForRepaint(RenderBoxModelObject* /*repaintContainer*/) const
{
    // FIXME: handle non-root repaintContainer
    IntRect result = m_absoluteBounds;
    adjustRectForOutlineAndShadow(result);
    return result;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
