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
#include "SVGFitToViewBox.h"
#include "SVGLength.h"
#include "SVGMarkerElement.h"
#include "SVGRenderSupport.h"
#include "SVGResourceClipper.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMasker.h"
#include "SVGSVGElement.h"
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
    return m_matrix;
}

void RenderSVGContainer::setLocalTransform(const AffineTransform& matrix)
{
    m_matrix = matrix;
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

void RenderSVGContainer::layout()
{
    ASSERT(needsLayout());

    calcViewport();

    // Arbitrary affine transforms are incompatible with LayoutState.
    view()->disableLayoutState();

    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (selfNeedsLayout() && checkForRepaint) {
        oldBounds = m_absoluteBounds;
        oldOutlineBox = absoluteOutlineBox();
    }

    RenderObject* child = firstChild();
    while (child) {
        // FIXME: This check is bogus, see http://bugs.webkit.org/show_bug.cgi?id=14003
        if (!child->isRenderPath() || static_cast<RenderPath*>(child)->hasRelativeValues())
            child->setNeedsLayout(true);

        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
        child = child->nextSibling();
    }

    // Calculate width & height
    m_width = calcReplacedWidth();
    m_height = calcReplacedHeight();
    m_absoluteBounds = absoluteClippedOverflowRect();

    if (selfNeedsLayout() && checkForRepaint)
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
    // Only the root <svg> element should need any translations using the HTML/CSS system
    // parentX, parentY are also non-zero for first-level kids of these
    // CSS-transformed <svg> root-elements (due to RenderBox::paint) for any other element
    // they should be 0.   m_x, m_y should always be 0 for non-root svg containers
    if (!viewport().isEmpty()) {
        if (style()->overflowX() != OVISIBLE)
            paintInfo.context->clip(enclosingIntRect(viewport())); // FIXME: Eventually we'll want float-precision clipping
        
        paintInfo.context->concatCTM(AffineTransform().translate(viewport().x(), viewport().y()));
    }

    if (!localTransform().isIdentity())
        paintInfo.context->concatCTM(localTransform());
}

void RenderSVGContainer::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled() || !drawsContents())
        return;

    // A value of zero disables rendering of the element. 
    if (!viewport().isEmpty() && (viewport().width() <= 0. || viewport().height() <= 0.))
        return;

    if (!firstChild()) {
#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
        // Spec: groups w/o children still may render filter content.
        const SVGRenderStyle* svgStyle = style()->svgStyle();
        AtomicString filterId(SVGURIReference::getTarget(svgStyle->filter()));
        SVGResourceFilter* filter = getFilterById(document(), filterId);
        if (!filter)
#endif
            return;
    }
    
    paintInfo.context->save();
    applyContentTransforms(paintInfo);

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    SVGResourceFilter* filter = 0;
#else
    void* filter = 0;
#endif
    FloatRect boundingBox = relativeBBox(true);
    float opacity = style()->opacity();
    if (paintInfo.phase == PaintPhaseForeground) {
        prepareToRenderSVGContent(this, paintInfo, boundingBox, filter);
        
        if (opacity < 1.0f) {
            paintInfo.context->clip(enclosingIntRect(boundingBox));
            paintInfo.context->beginTransparencyLayer(opacity);
        }
    }

    paintInfo.context->concatCTM(viewportTransform());

    // default implementation. Just pass paint through to the children
    PaintInfo childInfo(paintInfo);
    childInfo.paintingRoot = paintingRootForChildren(paintInfo);
    for (RenderObject* child = firstChild(); child; child = child->nextSibling())
        child->paint(childInfo, 0, 0);

    if (paintInfo.phase == PaintPhaseForeground) {
#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
        if (filter)
            filter->applyFilter(paintInfo.context, boundingBox);
#endif

        if (opacity < 1.0f)
            paintInfo.context->endTransparencyLayer();
    }

    paintInfo.context->restore();
    
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, m_absoluteBounds.x(), m_absoluteBounds.y(), m_absoluteBounds.width(), m_absoluteBounds.height(), style());
}

FloatRect RenderSVGContainer::viewport() const
{
    return m_viewport;
}

void RenderSVGContainer::calcViewport()
{
    SVGElement* svgelem = static_cast<SVGElement*>(element());
    if (svgelem->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());

        if (!selfNeedsLayout() && !svg->hasRelativeValues())
            return;

        float x = 0.0f;
        float y = 0.0f;
        if (parent()->isSVGContainer()) {
            x = svg->x().value();
            y = svg->y().value();
        }
        float w = svg->width().value();
        float h = svg->height().value();
        m_viewport = FloatRect(x, y, w, h);
    } else if (svgelem->hasTagName(SVGNames::markerTag)) {
        if (!selfNeedsLayout())
            return;

        SVGMarkerElement* svg = static_cast<SVGMarkerElement*>(element());
        float w = svg->markerWidth().value();
        float h = svg->markerHeight().value();
        m_viewport = FloatRect(0.0f, 0.0f, w, h);
    }
}

AffineTransform RenderSVGContainer::viewportTransform() const
{
    if (element()->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
        return svg->viewBoxToViewTransform(viewport().width(), viewport().height());
    } else if (element()->hasTagName(SVGNames::markerTag)) {
        SVGMarkerElement* marker = static_cast<SVGMarkerElement*>(element());
        return marker->viewBoxToViewTransform(viewport().width(), viewport().height());
    }
 
     return AffineTransform();
}

IntRect RenderSVGContainer::absoluteClippedOverflowRect()
{
    IntRect repaintRect;

    for (RenderObject* current = firstChild(); current != 0; current = current->nextSibling())
        repaintRect.unite(current->absoluteClippedOverflowRect());

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), SVGURIReference::getTarget(style()->svgStyle()->filter()));
    if (filter)
        repaintRect.unite(enclosingIntRect(filter->filterBBoxForItemBBox(repaintRect)));
#endif

    return repaintRect;
}

void RenderSVGContainer::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    graphicsContext->addFocusRingRect(m_absoluteBounds);
}

void RenderSVGContainer::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(absoluteClippedOverflowRect());
}

AffineTransform RenderSVGContainer::absoluteTransform() const
{
    AffineTransform ctm = RenderObject::absoluteTransform();
    ctm.translate(viewport().x(), viewport().y());
    return viewportTransform() * ctm;
}

bool RenderSVGContainer::fillContains(const FloatPoint& p) const
{
    RenderObject* current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath*>(current)->fillContains(p))
            return true;

        current = current->nextSibling();
    }

    return false;
}

bool RenderSVGContainer::strokeContains(const FloatPoint& p) const
{
    RenderObject* current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath*>(current)->strokeContains(p))
            return true;

        current = current->nextSibling();
    }

    return false;
}

FloatRect RenderSVGContainer::relativeBBox(bool includeStroke) const
{
    FloatRect rect;
    
    RenderObject* current = firstChild();
    for (; current != 0; current = current->nextSibling()) {
        FloatRect childBBox = current->relativeBBox(includeStroke);
        FloatRect mappedBBox = current->localTransform().mapRect(childBBox);
        rect.unite(mappedBBox);
    }

    return rect;
}

bool RenderSVGContainer::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    if (!viewport().isEmpty()
        && style()->overflowX() == OHIDDEN
        && style()->overflowY() == OHIDDEN) {
        // Check if we need to do anything at all.
        IntRect overflowBox = overflowRect(false);
        overflowBox.move(_tx, _ty);
        AffineTransform ctm = RenderObject::absoluteTransform();
        ctm.translate(viewport().x(), viewport().y());
        double localX, localY;
        ctm.inverse().map(_x - _tx, _y - _ty, &localX, &localY);
        if (!overflowBox.contains((int)localX, (int)localY))
            return false;
    }

    int sx = 0;
    int sy = 0;

    // Respect parent translation offset for non-outermost <svg> elements.
    // Outermost <svg> element is handled by RenderSVGRoot.
    if (element()->hasTagName(SVGNames::svgTag)) {
        sx = _tx;
        sy = _ty;
    }

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtPoint(request, result, _x - sx, _y - sy, _tx, _ty, hitTestAction)) {
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
