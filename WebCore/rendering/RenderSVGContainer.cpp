/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>

    This file is part of the KDE project

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
#include "GraphicsContext.h"
#include "RenderView.h"
#include "SVGRenderSupport.h"
#include "SVGResourceFilter.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

namespace WebCore {

RenderSVGContainer::RenderSVGContainer(SVGStyledElement* node)
    : RenderObject(node)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_width(0)
    , m_height(0)
    , m_drawsContents(true)
{
    setReplaced(true);
}

RenderSVGContainer::~RenderSVGContainer()
{
}

bool RenderSVGContainer::canHaveChildren() const
{
    return true;
}

void RenderSVGContainer::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    insertChildNode(newChild, beforeChild);
}

void RenderSVGContainer::removeChild(RenderObject* oldChild)
{
    // We do this here instead of in removeChildNode, since the only extremely low-level uses of remove/appendChildNode
    // cannot affect the positioned object list, and the floating object list is irrelevant (since the list gets cleared on
    // layout anyway).
    oldChild->removeFromObjectLists();

    removeChildNode(oldChild);
}

void RenderSVGContainer::destroy()
{
    destroyLeftoverChildren();
    RenderObject::destroy();
}

void RenderSVGContainer::destroyLeftoverChildren()
{
    while (m_firstChild) {
        // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
        if (m_firstChild->element())
            m_firstChild->element()->setRenderer(0);

        m_firstChild->destroy();
    }
}

RenderObject* RenderSVGContainer::removeChildNode(RenderObject* oldChild, bool fullRemove)
{
    ASSERT(oldChild->parent() == this);

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked).  We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!documentBeingDestroyed() && fullRemove) {
        oldChild->setNeedsLayoutAndPrefWidthsRecalc();
        oldChild->repaint();
    }

    // If we have a line box wrapper, delete it.
    oldChild->deleteLineBoxWrapper();

    if (!documentBeingDestroyed() && fullRemove) {
        // If oldChild is the start or end of the selection, then clear the selection to
        // avoid problems of invalid pointers.
        // FIXME: The SelectionController should be responsible for this when it
        // is notified of DOM mutations.
        if (oldChild->isSelectionBorder())
            view()->clearSelection();
    }

    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_firstChild == oldChild)
        m_firstChild = oldChild->nextSibling();
    if (m_lastChild == oldChild)
        m_lastChild = oldChild->previousSibling();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);

    return oldChild;
}

void RenderSVGContainer::appendChildNode(RenderObject* newChild, bool)
{
    ASSERT(!newChild->parent());
    ASSERT(newChild->element()->isSVGElement());

    newChild->setParent(this);
    RenderObject* lChild = m_lastChild;

    if (lChild) {
        newChild->setPreviousSibling(lChild);
        lChild->setNextSibling(newChild);
    } else
        m_firstChild = newChild;

    m_lastChild = newChild;

    newChild->setNeedsLayoutAndPrefWidthsRecalc(); // Goes up the containing block hierarchy.
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.

    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);
}

void RenderSVGContainer::insertChildNode(RenderObject* child, RenderObject* beforeChild, bool)
{
    if (!beforeChild) {
        appendChildNode(child);
        return;
    }

    ASSERT(!child->parent());
    ASSERT(beforeChild->parent() == this);
    ASSERT(child->element()->isSVGElement());

    if (beforeChild == m_firstChild)
        m_firstChild = child;

    RenderObject* prev = beforeChild->previousSibling();
    child->setNextSibling(beforeChild);
    beforeChild->setPreviousSibling(child);
    if (prev)
        prev->setNextSibling(child);
    child->setPreviousSibling(prev);

    child->setParent(this);

    child->setNeedsLayoutAndPrefWidthsRecalc();
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.

    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);
}

bool RenderSVGContainer::drawsContents() const
{
    return m_drawsContents;
}

void RenderSVGContainer::setDrawsContents(bool drawsContents)
{
    m_drawsContents = drawsContents;
}

AffineTransform RenderSVGContainer::localTransform() const
{
    return m_localTransform;
}

bool RenderSVGContainer::requiresLayer()
{
    // Only allow an <svg> element to generate a layer when it's positioned in a non-SVG context
    return false;
}

short RenderSVGContainer::lineHeight(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
}

short RenderSVGContainer::baselinePosition(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
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

    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout() && selfWillPaint();
    if (checkForRepaint) {
        oldBounds = m_absoluteBounds;
        oldOutlineBox = absoluteOutlineBox();
    }
    
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

    calcBounds();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);

    view()->enableLayoutState();
    setNeedsLayout(false);
}

int RenderSVGContainer::calcReplacedWidth() const
{
    switch (style()->width().type()) {
    case Fixed:
        return max(0, style()->width().value());
    case Percent:
    {
        const int cw = containingBlockWidth();
        return cw > 0 ? max(0, style()->width().calcMinValue(cw)) : 0;
    }
    default:
        return 0;
    }
}

int RenderSVGContainer::calcReplacedHeight() const
{
    switch (style()->height().type()) {
    case Fixed:
        return max(0, style()->height().value());
    case Percent:
    {
        RenderBlock* cb = containingBlock();
        return style()->height().calcValue(cb->availableHeight());
    }
    default:
        return 0;
    }
}

void RenderSVGContainer::applyContentTransforms(PaintInfo& paintInfo)
{
    if (!localTransform().isIdentity())
        paintInfo.context->concatCTM(localTransform());
}

void RenderSVGContainer::applyAdditionalTransforms(PaintInfo& paintInfo)
{
    // no-op
}

void RenderSVGContainer::calcBounds()
{
    m_width = calcReplacedWidth();
    m_height = calcReplacedHeight();
    m_absoluteBounds = absoluteClippedOverflowRect();
}

bool RenderSVGContainer::selfWillPaint() const
{
#if ENABLE(SVG_FILTERS)
    const SVGRenderStyle* svgStyle = style()->svgStyle();
    AtomicString filterId(SVGURIReference::getTarget(svgStyle->filter()));
    SVGResourceFilter* filter = getFilterById(document(), filterId);
    if (filter)
        return true;
#endif
    return false;
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, int parentX, int parentY)
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

    FloatRect boundingBox = relativeBBox(true);
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
    
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, m_absoluteBounds.x(), m_absoluteBounds.y(), m_absoluteBounds.width(), m_absoluteBounds.height(), style());
}

AffineTransform RenderSVGContainer::viewportTransform() const
{
     return AffineTransform();
}

IntRect RenderSVGContainer::absoluteClippedOverflowRect()
{
    FloatRect repaintRect;

    for (RenderObject* current = firstChild(); current != 0; current = current->nextSibling())
        repaintRect.unite(current->absoluteClippedOverflowRect());

#if ENABLE(SVG_FILTERS)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));
#endif

    if (!repaintRect.isEmpty())
        repaintRect.inflate(1); // inflate 1 pixel for antialiasing

    return enclosingIntRect(repaintRect);
}

void RenderSVGContainer::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    graphicsContext->addFocusRingRect(m_absoluteBounds);
}

void RenderSVGContainer::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(absoluteClippedOverflowRect());
}

FloatRect RenderSVGContainer::relativeBBox(bool includeStroke) const
{
    FloatRect rect;
    
    RenderObject* current = firstChild();
    for (; current != 0; current = current->nextSibling()) {
        FloatRect childBBox = current->relativeBBox(includeStroke);
        FloatRect mappedBBox = current->localTransform().mapRect(childBBox);

        // <svg> can have a viewBox contributing to the bbox
        if (current->isSVGContainer())
            mappedBBox = static_cast<RenderSVGContainer*>(current)->viewportTransform().mapRect(mappedBBox);

        rect.unite(mappedBBox);
    }

    return rect;
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

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
