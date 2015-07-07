/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 *
 */

#include "config.h"
#include "RenderObject.h"

#include "AXObjectCache.h"
#include "AnimationController.h"
#include "EventHandler.h"
#include "FloatQuad.h"
#include "FlowThreadController.h"
#include "FocusController.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLAnchorElement.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "HitTestResult.h"
#include "LogicalSelectionOffsetCaches.h"
#include "Page.h"
#include "PseudoElement.h"
#include "RenderCounter.h"
#include "RenderFlowThread.h"
#include "RenderGeometryMap.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h" 
#include "RenderSVGResourceContainer.h"
#include "RenderScrollbarPart.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SVGRenderSupport.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "TransformState.h"
#include "htmlediting.h"
#include <algorithm>
#include <stdio.h>
#include <wtf/RefCountedLeakCounter.h>

#if PLATFORM(IOS)
#include "SelectionRect.h"
#endif

namespace WebCore {

using namespace HTMLNames;

#ifndef NDEBUG
RenderObject::SetLayoutNeededForbiddenScope::SetLayoutNeededForbiddenScope(RenderObject* renderObject, bool isForbidden)
    : m_renderObject(renderObject)
    , m_preexistingForbidden(m_renderObject->isSetNeedsLayoutForbidden())
{
    m_renderObject->setNeedsLayoutIsForbidden(isForbidden);
}

RenderObject::SetLayoutNeededForbiddenScope::~SetLayoutNeededForbiddenScope()
{
    m_renderObject->setNeedsLayoutIsForbidden(m_preexistingForbidden);
}
#endif

struct SameSizeAsRenderObject {
    virtual ~SameSizeAsRenderObject() { } // Allocate vtable pointer.
    void* pointers[4];
#ifndef NDEBUG
    unsigned m_debugBitfields : 2;
#endif
    unsigned m_bitfields;
};

COMPILE_ASSERT(sizeof(RenderObject) == sizeof(SameSizeAsRenderObject), RenderObject_should_stay_small);

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, renderObjectCounter, ("RenderObject"));

RenderObject::RenderObject(Node& node)
    : CachedImageClient()
    , m_node(node)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
#ifndef NDEBUG
    , m_hasAXObject(false)
    , m_setNeedsLayoutForbidden(false)
#endif
    , m_bitfields(node)
{
    if (!node.isDocumentNode())
        view().didCreateRenderer();
#ifndef NDEBUG
    renderObjectCounter.increment();
#endif
}

RenderObject::~RenderObject()
{
#ifndef NDEBUG
    ASSERT(!m_hasAXObject);
    renderObjectCounter.decrement();
#endif
    view().didDestroyRenderer();
}

RenderTheme& RenderObject::theme() const
{
    ASSERT(document().page());
    return document().page()->theme();
}

bool RenderObject::isDescendantOf(const RenderObject* obj) const
{
    for (const RenderObject* r = this; r; r = r->m_parent) {
        if (r == obj)
            return true;
    }
    return false;
}

bool RenderObject::isLegend() const
{
    return node() && node()->hasTagName(legendTag);
}

bool RenderObject::isHTMLMarquee() const
{
    return node() && node()->renderer() == this && node()->hasTagName(marqueeTag);
}

void RenderObject::setFlowThreadStateIncludingDescendants(FlowThreadState state)
{
    setFlowThreadState(state);

    for (RenderObject* child = firstChildSlow(); child; child = child->nextSibling()) {
        // If the child is a fragmentation context it already updated the descendants flag accordingly.
        if (child->isRenderFlowThread())
            continue;
        ASSERT(state != child->flowThreadState());
        child->setFlowThreadStateIncludingDescendants(state);
    }
}

void RenderObject::setParent(RenderElement* parent)
{
    m_parent = parent;

    // Only update if our flow thread state is different from our new parent and if we're not a RenderFlowThread.
    // A RenderFlowThread is always considered to be inside itself, so it never has to change its state
    // in response to parent changes.
    FlowThreadState newState = parent ? parent->flowThreadState() : NotInsideFlowThread;
    if (newState != flowThreadState() && !isRenderFlowThread())
        setFlowThreadStateIncludingDescendants(newState);
}

void RenderObject::removeFromParent()
{
    if (parent())
        parent()->removeChild(*this);
}

RenderObject* RenderObject::nextInPreOrder() const
{
    if (RenderObject* o = firstChildSlow())
        return o;

    return nextInPreOrderAfterChildren();
}

RenderObject* RenderObject::nextInPreOrderAfterChildren() const
{
    RenderObject* o;
    if (!(o = nextSibling())) {
        o = parent();
        while (o && !o->nextSibling())
            o = o->parent();
        if (o)
            o = o->nextSibling();
    }

    return o;
}

RenderObject* RenderObject::nextInPreOrder(const RenderObject* stayWithin) const
{
    if (RenderObject* o = firstChildSlow())
        return o;

    return nextInPreOrderAfterChildren(stayWithin);
}

RenderObject* RenderObject::nextInPreOrderAfterChildren(const RenderObject* stayWithin) const
{
    if (this == stayWithin)
        return 0;

    const RenderObject* current = this;
    RenderObject* next;
    while (!(next = current->nextSibling())) {
        current = current->parent();
        if (!current || current == stayWithin)
            return 0;
    }
    return next;
}

RenderObject* RenderObject::previousInPreOrder() const
{
    if (RenderObject* o = previousSibling()) {
        while (RenderObject* last = o->lastChildSlow())
            o = last;
        return o;
    }

    return parent();
}

RenderObject* RenderObject::previousInPreOrder(const RenderObject* stayWithin) const
{
    if (this == stayWithin)
        return 0;

    return previousInPreOrder();
}

RenderObject* RenderObject::childAt(unsigned index) const
{
    RenderObject* child = firstChildSlow();
    for (unsigned i = 0; child && i < index; i++)
        child = child->nextSibling();
    return child;
}

RenderObject* RenderObject::firstLeafChild() const
{
    RenderObject* r = firstChildSlow();
    while (r) {
        RenderObject* n = 0;
        n = r->firstChildSlow();
        if (!n)
            break;
        r = n;
    }
    return r;
}

RenderObject* RenderObject::lastLeafChild() const
{
    RenderObject* r = lastChildSlow();
    while (r) {
        RenderObject* n = 0;
        n = r->lastChildSlow();
        if (!n)
            break;
        r = n;
    }
    return r;
}

#if ENABLE(IOS_TEXT_AUTOSIZING)
// Inspired by Node::traverseNextNode.
RenderObject* RenderObject::traverseNext(const RenderObject* stayWithin) const
{
    RenderObject* child = firstChildSlow();
    if (child) {
        ASSERT(!stayWithin || child->isDescendantOf(stayWithin));
        return child;
    }
    if (this == stayWithin)
        return 0;
    if (nextSibling()) {
        ASSERT(!stayWithin || nextSibling()->isDescendantOf(stayWithin));
        return nextSibling();
    }
    const RenderObject* n = this;
    while (n && !n->nextSibling() && (!stayWithin || n->parent() != stayWithin))
        n = n->parent();
    if (n) {
        ASSERT(!stayWithin || !n->nextSibling() || n->nextSibling()->isDescendantOf(stayWithin));
        return n->nextSibling();
    }
    return 0;
}

// Non-recursive version of the DFS search.
RenderObject* RenderObject::traverseNext(const RenderObject* stayWithin, HeightTypeTraverseNextInclusionFunction inclusionFunction, int& currentDepth, int& newFixedDepth) const
{
    BlockContentHeightType overflowType;

    // Check for suitable children.
    for (RenderObject* child = firstChildSlow(); child; child = child->nextSibling()) {
        overflowType = inclusionFunction(child);
        if (overflowType != FixedHeight) {
            currentDepth++;
            if (overflowType == OverflowHeight)
                newFixedDepth = currentDepth;
            ASSERT(!stayWithin || child->isDescendantOf(stayWithin));
            return child;
        }
    }

    if (this == stayWithin)
        return 0;

    // Now we traverse other nodes if they exist, otherwise
    // we go to the parent node and try doing the same.
    const RenderObject* n = this;
    while (n) {
        while (n && !n->nextSibling() && (!stayWithin || n->parent() != stayWithin)) {
            n = n->parent();
            currentDepth--;
        }
        if (!n)
            return 0;
        for (RenderObject* sibling = n->nextSibling(); sibling; sibling = sibling->nextSibling()) {
            overflowType = inclusionFunction(sibling);
            if (overflowType != FixedHeight) {
                if (overflowType == OverflowHeight)
                    newFixedDepth = currentDepth;
                ASSERT(!stayWithin || !n->nextSibling() || n->nextSibling()->isDescendantOf(stayWithin));
                return sibling;
            }
        }
        if (!stayWithin || n->parent() != stayWithin) {
            n = n->parent();
            currentDepth--;
        } else
            return 0;
    }
    return 0;
}

RenderObject* RenderObject::traverseNext(const RenderObject* stayWithin, TraverseNextInclusionFunction inclusionFunction) const
{
    for (RenderObject* child = firstChildSlow(); child; child = child->nextSibling()) {
        if (inclusionFunction(child)) {
            ASSERT(!stayWithin || child->isDescendantOf(stayWithin));
            return child;
        }
    }

    if (this == stayWithin)
        return 0;

    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (inclusionFunction(sibling)) {
            ASSERT(!stayWithin || sibling->isDescendantOf(stayWithin));
            return sibling;
        }
    }

    const RenderObject* n = this;
    while (n) {
        while (n && !n->nextSibling() && (!stayWithin || n->parent() != stayWithin))
            n = n->parent();
        if (n) {
            for (RenderObject* sibling = n->nextSibling(); sibling; sibling = sibling->nextSibling()) {
                if (inclusionFunction(sibling)) {
                    ASSERT(!stayWithin || !n->nextSibling() || n->nextSibling()->isDescendantOf(stayWithin));
                    return sibling;
                }
            }
            if ((!stayWithin || n->parent() != stayWithin))
                n = n->parent();
            else
                return 0;
        }
    }
    return 0;
}

static RenderObject::BlockContentHeightType includeNonFixedHeight(const RenderObject* render)
{
    const RenderStyle& style = render->style();
    if (style.height().type() == Fixed) {
        if (render->isRenderBlock()) {
            const RenderBlock* block = toRenderBlock(render);
            // For fixed height styles, if the overflow size of the element spills out of the specified
            // height, assume we can apply text auto-sizing.
            if (style.overflowY() == OVISIBLE && style.height().value() < block->layoutOverflowRect().maxY())
                return RenderObject::OverflowHeight;
        }
        return RenderObject::FixedHeight;
    }
    return RenderObject::FlexibleHeight;
}


void RenderObject::adjustComputedFontSizesOnBlocks(float size, float visibleWidth)
{
    Document* document = view().frameView().frame().document();
    if (!document)
        return;

    Vector<int> depthStack;
    int currentDepth = 0;
    int newFixedDepth = 0;

    // We don't apply autosizing to nodes with fixed height normally.
    // But we apply it to nodes which are located deep enough
    // (nesting depth is greater than some const) inside of a parent block
    // which has fixed height but its content overflows intentionally.
    for (RenderObject* descendent = traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth); descendent; descendent = descendent->traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth)) {
        while (depthStack.size() > 0 && currentDepth <= depthStack[depthStack.size() - 1])
            depthStack.remove(depthStack.size() - 1);
        if (newFixedDepth)
            depthStack.append(newFixedDepth);

        int stackSize = depthStack.size();
        if (descendent->isRenderBlockFlow() && !descendent->isListItem() && (!stackSize || currentDepth - depthStack[stackSize - 1] > TextAutoSizingFixedHeightDepth))
            toRenderBlockFlow(descendent)->adjustComputedFontSizes(size, visibleWidth);
        newFixedDepth = 0;
    }

    // Remove style from auto-sizing table that are no longer valid.
    document->validateAutoSizingNodes();
}

void RenderObject::resetTextAutosizing()
{
    Document* document = view().frameView().frame().document();
    if (!document)
        return;

    document->resetAutoSizingNodes();

    Vector<int> depthStack;
    int currentDepth = 0;
    int newFixedDepth = 0;

    for (RenderObject* descendent = traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth); descendent; descendent = descendent->traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth)) {
        while (depthStack.size() > 0 && currentDepth <= depthStack[depthStack.size() - 1])
            depthStack.remove(depthStack.size() - 1);
        if (newFixedDepth)
            depthStack.append(newFixedDepth);

        int stackSize = depthStack.size();
        if (descendent->isRenderBlockFlow() && !descendent->isListItem() && (!stackSize || currentDepth - depthStack[stackSize - 1] > TextAutoSizingFixedHeightDepth))
            toRenderBlockFlow(descendent)->resetComputedFontSize();
        newFixedDepth = 0;
    }
}
#endif // ENABLE(IOS_TEXT_AUTOSIZING)

RenderLayer* RenderObject::enclosingLayer() const
{
    for (auto& renderer : lineageOfType<RenderLayerModelObject>(*this)) {
        if (renderer.hasLayer())
            return renderer.layer();
    }
    return nullptr;
}

bool RenderObject::scrollRectToVisible(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    RenderLayer* enclosingLayer = this->enclosingLayer();
    if (!enclosingLayer)
        return false;

    enclosingLayer->scrollRectToVisible(rect, alignX, alignY);
    return true;
}

RenderBox& RenderObject::enclosingBox() const
{
    return *lineageOfType<RenderBox>(const_cast<RenderObject&>(*this)).first();
}

RenderBoxModelObject& RenderObject::enclosingBoxModelObject() const
{
    return *lineageOfType<RenderBoxModelObject>(const_cast<RenderObject&>(*this)).first();
}

bool RenderObject::fixedPositionedWithNamedFlowContainingBlock() const
{
    return ((flowThreadState() == RenderObject::InsideOutOfFlowThread)
        && (style().position() == FixedPosition)
        && (containingBlock()->isOutOfFlowRenderFlowThread()));
}

static bool hasFixedPosInNamedFlowContainingBlock(const RenderObject* renderer)
{
    ASSERT(renderer->flowThreadState() != RenderObject::NotInsideFlowThread);

    RenderObject* curr = const_cast<RenderObject*>(renderer);
    while (curr) {
        if (curr->fixedPositionedWithNamedFlowContainingBlock())
            return true;
        curr = curr->containingBlock();
    }

    return false;
}

RenderFlowThread* RenderObject::locateFlowThreadContainingBlockNoCache() const
{
    ASSERT(flowThreadState() != NotInsideFlowThread);

    RenderObject* curr = const_cast<RenderObject*>(this);
    while (curr) {
        if (curr->isRenderFlowThread())
            return toRenderFlowThread(curr);
        curr = curr->containingBlock();
    }
    return 0;
}

RenderFlowThread* RenderObject::locateFlowThreadContainingBlock() const
{
    ASSERT(flowThreadState() != NotInsideFlowThread);

    // See if we have the thread cached because we're in the middle of layout.
    RenderFlowThread* flowThread = view().flowThreadController().currentRenderFlowThread();
    if (flowThread && (flowThreadState() == flowThread->flowThreadState())) {
        // Make sure the slow path would return the same result as our cache.
        // FIXME: For the moment, only apply this assertion to regions, as multicol
        // still has some issues and triggers this assert.
        // Created https://bugs.webkit.org/show_bug.cgi?id=132946 for this issue.
        ASSERT(!flowThread->isRenderNamedFlowThread() || flowThread == locateFlowThreadContainingBlockNoCache());
        return flowThread;
    }
    
    // Not in the middle of layout so have to find the thread the slow way.
    return locateFlowThreadContainingBlockNoCache();
}

RenderBlock* RenderObject::firstLineBlock() const
{
    return 0;
}

static inline bool objectIsRelayoutBoundary(const RenderElement* object)
{
    // FIXME: In future it may be possible to broaden these conditions in order to improve performance.
    if (object->isRenderView())
        return true;

    if (object->isTextControl())
        return true;

    if (object->isSVGRoot())
        return true;

    if (!object->hasOverflowClip())
        return false;

    if (object->style().width().isIntrinsicOrAuto() || object->style().height().isIntrinsicOrAuto() || object->style().height().isPercent())
        return false;

    // Table parts can't be relayout roots since the table is responsible for layouting all the parts.
    if (object->isTablePart())
        return false;

    return true;
}

void RenderObject::clearNeedsLayout()
{
    m_bitfields.setNeedsLayout(false);
    setEverHadLayout(true);
    setPosChildNeedsLayoutBit(false);
    setNeedsSimplifiedNormalFlowLayoutBit(false);
    setNormalChildNeedsLayoutBit(false);
    setNeedsPositionedMovementLayoutBit(false);
    if (isRenderElement())
        toRenderElement(this)->setAncestorLineBoxDirty(false);
#ifndef NDEBUG
    checkBlockPositionedObjectsNeedLayout();
#endif
}

static void scheduleRelayoutForSubtree(RenderElement& renderer)
{
    if (!renderer.isRenderView()) {
        if (!renderer.isRooted())
            return;
        renderer.view().frameView().scheduleRelayoutOfSubtree(renderer);
        return;
    }
    toRenderView(renderer).frameView().scheduleRelayout();
}

void RenderObject::markContainingBlocksForLayout(bool scheduleRelayout, RenderElement* newRoot)
{
    ASSERT(!scheduleRelayout || !newRoot);
    ASSERT(!isSetNeedsLayoutForbidden());

    auto ancestor = container();

    bool simplifiedNormalFlowLayout = needsSimplifiedNormalFlowLayout() && !selfNeedsLayout() && !normalChildNeedsLayout();
    bool hasOutOfFlowPosition = !isText() && style().hasOutOfFlowPosition();

    while (ancestor) {
#ifndef NDEBUG
        // FIXME: Remove this once we remove the special cases for counters, quotes and mathml
        // calling setNeedsLayout during preferred width computation.
        SetLayoutNeededForbiddenScope layoutForbiddenScope(ancestor, isSetNeedsLayoutForbidden());
#endif
        // Don't mark the outermost object of an unrooted subtree. That object will be
        // marked when the subtree is added to the document.
        auto container = ancestor->container();
        if (!container && !ancestor->isRenderView())
            return;
        if (hasOutOfFlowPosition) {
            bool willSkipRelativelyPositionedInlines = !ancestor->isRenderBlock() || ancestor->isAnonymousBlock();
            // Skip relatively positioned inlines and anonymous blocks to get to the enclosing RenderBlock.
            while (ancestor && (!ancestor->isRenderBlock() || ancestor->isAnonymousBlock()))
                ancestor = ancestor->container();
            if (!ancestor || ancestor->posChildNeedsLayout())
                return;
            if (willSkipRelativelyPositionedInlines)
                container = ancestor->container();
            ancestor->setPosChildNeedsLayoutBit(true);
            simplifiedNormalFlowLayout = true;
        } else if (simplifiedNormalFlowLayout) {
            if (ancestor->needsSimplifiedNormalFlowLayout())
                return;
            ancestor->setNeedsSimplifiedNormalFlowLayoutBit(true);
        } else {
            if (ancestor->normalChildNeedsLayout())
                return;
            ancestor->setNormalChildNeedsLayoutBit(true);
        }
        ASSERT(!ancestor->isSetNeedsLayoutForbidden());

        if (ancestor == newRoot)
            return;

        if (scheduleRelayout && objectIsRelayoutBoundary(ancestor))
            break;

        hasOutOfFlowPosition = ancestor->style().hasOutOfFlowPosition();
        ancestor = container;
    }

    if (scheduleRelayout && ancestor)
        scheduleRelayoutForSubtree(*ancestor);
}

#ifndef NDEBUG
void RenderObject::checkBlockPositionedObjectsNeedLayout()
{
    ASSERT(!needsLayout());

    if (isRenderBlock())
        toRenderBlock(this)->checkPositionedObjectsNeedLayout();
}
#endif

void RenderObject::setPreferredLogicalWidthsDirty(bool shouldBeDirty, MarkingBehavior markParents)
{
    bool alreadyDirty = preferredLogicalWidthsDirty();
    m_bitfields.setPreferredLogicalWidthsDirty(shouldBeDirty);
    if (shouldBeDirty && !alreadyDirty && markParents == MarkContainingBlockChain && (isText() || !style().hasOutOfFlowPosition()))
        invalidateContainerPreferredLogicalWidths();
}

void RenderObject::invalidateContainerPreferredLogicalWidths()
{
    // In order to avoid pathological behavior when inlines are deeply nested, we do include them
    // in the chain that we mark dirty (even though they're kind of irrelevant).
    auto o = isTableCell() ? containingBlock() : container();
    while (o && !o->preferredLogicalWidthsDirty()) {
        // Don't invalidate the outermost object of an unrooted subtree. That object will be 
        // invalidated when the subtree is added to the document.
        auto container = o->isTableCell() ? o->containingBlock() : o->container();
        if (!container && !o->isRenderView())
            break;

        o->m_bitfields.setPreferredLogicalWidthsDirty(true);
        if (o->style().hasOutOfFlowPosition())
            // A positioned object has no effect on the min/max width of its containing block ever.
            // We can optimize this case and not go up any further.
            break;
        o = container;
    }
}

void RenderObject::setLayerNeedsFullRepaint()
{
    ASSERT(hasLayer());
    toRenderLayerModelObject(this)->layer()->setRepaintStatus(NeedsFullRepaint);
}

void RenderObject::setLayerNeedsFullRepaintForPositionedMovementLayout()
{
    ASSERT(hasLayer());
    toRenderLayerModelObject(this)->layer()->setRepaintStatus(NeedsFullRepaintForPositionedMovementLayout);
}

RenderBlock* RenderObject::containingBlock() const
{
    auto o = parent();
    if (!o && isRenderScrollbarPart())
        o = toRenderScrollbarPart(this)->rendererOwningScrollbar();

    const RenderStyle& style = this->style();
    if (!isText() && style.position() == FixedPosition)
        o = containingBlockForFixedPosition(o);
    else if (!isText() && style.position() == AbsolutePosition)
        o = containingBlockForAbsolutePosition(o);
    else
        o = containingBlockForObjectInFlow(o);

    if (!o || !o->isRenderBlock())
        return 0; // This can still happen in case of an orphaned tree

    return toRenderBlock(o);
}

void RenderObject::drawLineForBoxSide(GraphicsContext* graphicsContext, float x1, float y1, float x2, float y2,
    BoxSide side, Color color, EBorderStyle borderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias) const
{
    float deviceScaleFactor = document().deviceScaleFactor();
    float thickness;
    float length;
    if (side == BSTop || side == BSBottom) {
        thickness = y2 - y1;
        length = x2 - x1;
    } else {
        thickness = x2 - x1;
        length = y2 - y1;
    }
    if (borderStyle == DOUBLE && (thickness * deviceScaleFactor) < 3)
        borderStyle = SOLID;

    // FIXME: We really would like this check to be an ASSERT as we don't want to draw empty borders. However
    // nothing guarantees that the following recursive calls to drawLineForBoxSide will have non-null dimensions.
    if (!thickness || !length)
        return;

    const RenderStyle& style = this->style();
    switch (borderStyle) {
        case BNONE:
        case BHIDDEN:
            return;
        case DOTTED:
        case DASHED: {
            if (thickness > 0) {
                bool wasAntialiased = graphicsContext->shouldAntialias();
                StrokeStyle oldStrokeStyle = graphicsContext->strokeStyle();
                graphicsContext->setShouldAntialias(antialias);
                graphicsContext->setStrokeColor(color, style.colorSpace());
                graphicsContext->setStrokeThickness(thickness);
                graphicsContext->setStrokeStyle(borderStyle == DASHED ? DashedStroke : DottedStroke);

                // FIXME: There's some odd adjustment in GraphicsContext::drawLine() that disables device pixel precision line drawing.
                int adjustedX = floorToInt((x1 + x2) / 2);
                int adjustedY = floorToInt((y1 + y2) / 2);

                switch (side) {
                    case BSBottom:
                    case BSTop:
                        graphicsContext->drawLine(FloatPoint(x1, adjustedY), FloatPoint(x2, adjustedY));
                        break;
                    case BSRight:
                    case BSLeft:
                        graphicsContext->drawLine(FloatPoint(adjustedX, y1), FloatPoint(adjustedX, y2));
                        break;
                }
                graphicsContext->setShouldAntialias(wasAntialiased);
                graphicsContext->setStrokeStyle(oldStrokeStyle);
            }
            break;
        }
        case DOUBLE: {
            float thirdOfThickness = ceilToDevicePixel(thickness / 3, deviceScaleFactor);
            ASSERT(thirdOfThickness);

            if (adjacentWidth1 == 0 && adjacentWidth2 == 0) {
                StrokeStyle oldStrokeStyle = graphicsContext->strokeStyle();
                graphicsContext->setStrokeStyle(NoStroke);
                graphicsContext->setFillColor(color, style.colorSpace());
                
                bool wasAntialiased = graphicsContext->shouldAntialias();
                graphicsContext->setShouldAntialias(antialias);

                switch (side) {
                    case BSTop:
                    case BSBottom:
                        graphicsContext->drawRect(pixelSnappedForPainting(x1, y1, length, thirdOfThickness, deviceScaleFactor));
                        graphicsContext->drawRect(pixelSnappedForPainting(x1, y2 - thirdOfThickness, length, thirdOfThickness, deviceScaleFactor));
                        break;
                    case BSLeft:
                    case BSRight:
                        graphicsContext->drawRect(pixelSnappedForPainting(x1, y1, thirdOfThickness, length, deviceScaleFactor));
                        graphicsContext->drawRect(pixelSnappedForPainting(x2 - thirdOfThickness, y1, thirdOfThickness, length, deviceScaleFactor));
                        break;
                }

                graphicsContext->setShouldAntialias(wasAntialiased);
                graphicsContext->setStrokeStyle(oldStrokeStyle);
            } else {
                float adjacent1BigThird = ceilToDevicePixel(adjacentWidth1 / 3, deviceScaleFactor);
                float adjacent2BigThird = ceilToDevicePixel(adjacentWidth2 / 3, deviceScaleFactor);

                float offset1 = floorToDevicePixel(fabs(adjacentWidth1) * 2 / 3, deviceScaleFactor);
                float offset2 = floorToDevicePixel(fabs(adjacentWidth2) * 2 / 3, deviceScaleFactor);

                float mitreOffset1 = adjacentWidth1 < 0 ? offset1 : 0;
                float mitreOffset2 = adjacentWidth1 > 0 ? offset1 : 0;
                float mitreOffset3 = adjacentWidth2 < 0 ? offset2 : 0;
                float mitreOffset4 = adjacentWidth2 > 0 ? offset2 : 0;

                FloatRect paintBorderRect;
                switch (side) {
                    case BSTop:
                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x1 + mitreOffset1, y1, (x2 - mitreOffset3) - (x1 + mitreOffset1), thirdOfThickness), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);

                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x1 + mitreOffset2, y2 - thirdOfThickness, (x2 - mitreOffset4) - (x1 + mitreOffset2), thirdOfThickness), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);
                        break;
                    case BSLeft:
                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x1, y1 + mitreOffset1, thirdOfThickness, (y2 - mitreOffset3) - (y1 + mitreOffset1)), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);

                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x2 - thirdOfThickness, y1 + mitreOffset2, thirdOfThickness, (y2 - mitreOffset4) - (y1 + mitreOffset2)), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);
                        break;
                    case BSBottom:
                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x1 + mitreOffset2, y1, (x2 - mitreOffset4) - (x1 + mitreOffset2), thirdOfThickness), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);

                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x1 + mitreOffset1, y2 - thirdOfThickness, (x2 - mitreOffset3) - (x1 + mitreOffset1), thirdOfThickness), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);
                        break;
                    case BSRight:
                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x1, y1 + mitreOffset2, thirdOfThickness, (y2 - mitreOffset4) - (y1 + mitreOffset2)), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);

                        paintBorderRect = pixelSnappedForPainting(LayoutRect(x2 - thirdOfThickness, y1 + mitreOffset1, thirdOfThickness, (y2 - mitreOffset3) - (y1 + mitreOffset1)), deviceScaleFactor);
                        drawLineForBoxSide(graphicsContext, paintBorderRect.x(), paintBorderRect.y(), paintBorderRect.maxX(), paintBorderRect.maxY(), side, color, SOLID,
                            adjacent1BigThird, adjacent2BigThird, antialias);
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case RIDGE:
        case GROOVE: {
            EBorderStyle s1;
            EBorderStyle s2;
            if (borderStyle == GROOVE) {
                s1 = INSET;
                s2 = OUTSET;
            } else {
                s1 = OUTSET;
                s2 = INSET;
            }

            float adjacent1BigHalf = ceilToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);
            float adjacent2BigHalf = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

            float adjacent1SmallHalf = floorToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);
            float adjacent2SmallHalf = floorToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

            float offset1 = 0;
            float offset2 = 0;
            float offset3 = 0;
            float offset4 = 0;

            if (((side == BSTop || side == BSLeft) && adjacentWidth1 < 0) || ((side == BSBottom || side == BSRight) && adjacentWidth1 > 0))
                offset1 = floorToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);

            if (((side == BSTop || side == BSLeft) && adjacentWidth2 < 0) || ((side == BSBottom || side == BSRight) && adjacentWidth2 > 0))
                offset2 = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

            if (((side == BSTop || side == BSLeft) && adjacentWidth1 > 0) || ((side == BSBottom || side == BSRight) && adjacentWidth1 < 0))
                offset3 = floorToDevicePixel(fabs(adjacentWidth1) / 2, deviceScaleFactor);

            if (((side == BSTop || side == BSLeft) && adjacentWidth2 > 0) || ((side == BSBottom || side == BSRight) && adjacentWidth2 < 0))
                offset4 = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

            float adjustedX = ceilToDevicePixel((x1 + x2) / 2, deviceScaleFactor);
            float adjustedY = ceilToDevicePixel((y1 + y2) / 2, deviceScaleFactor);
            /// Quads can't use the default snapping rect functions.
            x1 = roundToDevicePixel(x1, deviceScaleFactor);
            x2 = roundToDevicePixel(x2, deviceScaleFactor);
            y1 = roundToDevicePixel(y1, deviceScaleFactor);
            y2 = roundToDevicePixel(y2, deviceScaleFactor);

            switch (side) {
                case BSTop:
                    drawLineForBoxSide(graphicsContext, x1 + offset1, y1, x2 - offset2, adjustedY, side, color, s1, adjacent1BigHalf, adjacent2BigHalf, antialias);
                    drawLineForBoxSide(graphicsContext, x1 + offset3, adjustedY, x2 - offset4, y2, side, color, s2, adjacent1SmallHalf, adjacent2SmallHalf, antialias);
                    break;
                case BSLeft:
                    drawLineForBoxSide(graphicsContext, x1, y1 + offset1, adjustedX, y2 - offset2, side, color, s1, adjacent1BigHalf, adjacent2BigHalf, antialias);
                    drawLineForBoxSide(graphicsContext, adjustedX, y1 + offset3, x2, y2 - offset4, side, color, s2, adjacent1SmallHalf, adjacent2SmallHalf, antialias);
                    break;
                case BSBottom:
                    drawLineForBoxSide(graphicsContext, x1 + offset1, y1, x2 - offset2, adjustedY, side, color, s2, adjacent1BigHalf, adjacent2BigHalf, antialias);
                    drawLineForBoxSide(graphicsContext, x1 + offset3, adjustedY, x2 - offset4, y2, side, color, s1, adjacent1SmallHalf, adjacent2SmallHalf, antialias);
                    break;
                case BSRight:
                    drawLineForBoxSide(graphicsContext, x1, y1 + offset1, adjustedX, y2 - offset2, side, color, s2, adjacent1BigHalf, adjacent2BigHalf, antialias);
                    drawLineForBoxSide(graphicsContext, adjustedX, y1 + offset3, x2, y2 - offset4, side, color, s1, adjacent1SmallHalf, adjacent2SmallHalf, antialias);
                    break;
            }
            break;
        }
        case INSET:
            // FIXME: Maybe we should lighten the colors on one side like Firefox.
            // https://bugs.webkit.org/show_bug.cgi?id=58608
            if (side == BSTop || side == BSLeft)
                color = color.dark();
            FALLTHROUGH;
        case OUTSET:
            if (borderStyle == OUTSET && (side == BSBottom || side == BSRight))
                color = color.dark();
            FALLTHROUGH;
        case SOLID: {
            StrokeStyle oldStrokeStyle = graphicsContext->strokeStyle();
            ASSERT(x2 >= x1);
            ASSERT(y2 >= y1);
            if (!adjacentWidth1 && !adjacentWidth2) {
                // Turn off antialiasing to match the behavior of drawConvexPolygon();
                // this matters for rects in transformed contexts.
                graphicsContext->setStrokeStyle(NoStroke);
                graphicsContext->setFillColor(color, style.colorSpace());
                bool wasAntialiased = graphicsContext->shouldAntialias();
                graphicsContext->setShouldAntialias(antialias);
                graphicsContext->drawRect(pixelSnappedForPainting(x1, y1, x2 - x1, y2 - y1, deviceScaleFactor));
                graphicsContext->setShouldAntialias(wasAntialiased);
                graphicsContext->setStrokeStyle(oldStrokeStyle);
                return;
            }

            // FIXME: These roundings should be replaced by ASSERT(device pixel positioned) when all the callers transitioned to device pixels.
            x1 = roundToDevicePixel(x1, deviceScaleFactor);
            y1 = roundToDevicePixel(y1, deviceScaleFactor);
            x2 = roundToDevicePixel(x2, deviceScaleFactor);
            y2 = roundToDevicePixel(y2, deviceScaleFactor);

            FloatPoint quad[4];
            switch (side) {
                case BSTop:
                    quad[0] = FloatPoint(x1 + std::max<float>(-adjacentWidth1, 0), y1);
                    quad[1] = FloatPoint(x1 + std::max<float>(adjacentWidth1, 0), y2);
                    quad[2] = FloatPoint(x2 - std::max<float>(adjacentWidth2, 0), y2);
                    quad[3] = FloatPoint(x2 - std::max<float>(-adjacentWidth2, 0), y1);
                    break;
                case BSBottom:
                    quad[0] = FloatPoint(x1 + std::max<float>(adjacentWidth1, 0), y1);
                    quad[1] = FloatPoint(x1 + std::max<float>(-adjacentWidth1, 0), y2);
                    quad[2] = FloatPoint(x2 - std::max<float>(-adjacentWidth2, 0), y2);
                    quad[3] = FloatPoint(x2 - std::max<float>(adjacentWidth2, 0), y1);
                    break;
                case BSLeft:
                    quad[0] = FloatPoint(x1, y1 + std::max<float>(-adjacentWidth1, 0));
                    quad[1] = FloatPoint(x1, y2 - std::max<float>(-adjacentWidth2, 0));
                    quad[2] = FloatPoint(x2, y2 - std::max<float>(adjacentWidth2, 0));
                    quad[3] = FloatPoint(x2, y1 + std::max<float>(adjacentWidth1, 0));
                    break;
                case BSRight:
                    quad[0] = FloatPoint(x1, y1 + std::max<float>(adjacentWidth1, 0));
                    quad[1] = FloatPoint(x1, y2 - std::max<float>(adjacentWidth2, 0));
                    quad[2] = FloatPoint(x2, y2 - std::max<float>(-adjacentWidth2, 0));
                    quad[3] = FloatPoint(x2, y1 + std::max<float>(-adjacentWidth1, 0));
                    break;
            }

            graphicsContext->setStrokeStyle(NoStroke);
            graphicsContext->setFillColor(color, style.colorSpace());
            graphicsContext->drawConvexPolygon(4, quad, antialias);
            graphicsContext->setStrokeStyle(oldStrokeStyle);
            break;
        }
    }
}

void RenderObject::paintFocusRing(PaintInfo& paintInfo, const LayoutPoint& paintOffset, RenderStyle* style)
{
    ASSERT(style->outlineStyleIsAuto());

    Vector<IntRect> focusRingRects;
    addFocusRingRects(focusRingRects, paintOffset, paintInfo.paintContainer);
#if PLATFORM(MAC)
    bool needsRepaint;
    paintInfo.context->drawFocusRing(focusRingRects, style->outlineWidth(), style->outlineOffset(), document().page()->focusController().timeSinceFocusWasSet(), needsRepaint);
    if (needsRepaint)
        document().page()->focusController().setFocusedElementNeedsRepaint();
#else
    paintInfo.context->drawFocusRing(focusRingRects, style->outlineWidth(), style->outlineOffset(), style->visitedDependentColor(CSSPropertyOutlineColor));
#endif
}

void RenderObject::addPDFURLRect(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    Vector<IntRect> focusRingRects;
    addFocusRingRects(focusRingRects, paintOffset, paintInfo.paintContainer);
    IntRect urlRect = unionRect(focusRingRects);

    if (urlRect.isEmpty())
        return;
    Node* n = node();
    if (!n || !n->isLink() || !n->isElementNode())
        return;
    const AtomicString& href = toElement(n)->getAttribute(hrefAttr);
    if (href.isNull())
        return;
    paintInfo.context->setURLForRect(n->document().completeURL(href), pixelSnappedIntRect(urlRect));
}

void RenderObject::paintOutline(PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    if (!hasOutline())
        return;

    RenderStyle& styleToUse = style();
    LayoutUnit outlineWidth = styleToUse.outlineWidth();

    int outlineOffset = styleToUse.outlineOffset();

    // Only paint the focus ring by hand if the theme isn't able to draw it.
    if (styleToUse.outlineStyleIsAuto() && !theme().supportsFocusRing(styleToUse))
        paintFocusRing(paintInfo, paintRect.location(), &styleToUse);

    if (hasOutlineAnnotation() && !styleToUse.outlineStyleIsAuto() && !theme().supportsFocusRing(styleToUse))
        addPDFURLRect(paintInfo, paintRect.location());

    if (styleToUse.outlineStyleIsAuto() || styleToUse.outlineStyle() == BNONE)
        return;

    IntRect inner = pixelSnappedIntRect(paintRect);
    inner.inflate(outlineOffset);

    IntRect outer = pixelSnappedIntRect(inner);
    outer.inflate(outlineWidth);

    // FIXME: This prevents outlines from painting inside the object. See bug 12042
    if (outer.isEmpty())
        return;

    EBorderStyle outlineStyle = styleToUse.outlineStyle();
    Color outlineColor = styleToUse.visitedDependentColor(CSSPropertyOutlineColor);

    GraphicsContext* graphicsContext = paintInfo.context;
    bool useTransparencyLayer = outlineColor.hasAlpha();
    if (useTransparencyLayer) {
        if (outlineStyle == SOLID) {
            Path path;
            path.addRect(outer);
            path.addRect(inner);
            graphicsContext->setFillRule(RULE_EVENODD);
            graphicsContext->setFillColor(outlineColor, styleToUse.colorSpace());
            graphicsContext->fillPath(path);
            return;
        }
        graphicsContext->beginTransparencyLayer(static_cast<float>(outlineColor.alpha()) / 255);
        outlineColor = Color(outlineColor.red(), outlineColor.green(), outlineColor.blue());
    }

    int leftOuter = outer.x();
    int leftInner = inner.x();
    int rightOuter = outer.maxX();
    int rightInner = inner.maxX();
    int topOuter = outer.y();
    int topInner = inner.y();
    int bottomOuter = outer.maxY();
    int bottomInner = inner.maxY();
    
    drawLineForBoxSide(graphicsContext, leftOuter, topOuter, leftInner, bottomOuter, BSLeft, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, leftOuter, topOuter, rightOuter, topInner, BSTop, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, rightInner, topOuter, rightOuter, bottomOuter, BSRight, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, leftOuter, bottomInner, rightOuter, bottomOuter, BSBottom, outlineColor, outlineStyle, outlineWidth, outlineWidth);

    if (useTransparencyLayer)
        graphicsContext->endTransparencyLayer();
}

#if PLATFORM(IOS)
// This function is similar in spirit to RenderText::absoluteRectsForRange, but returns rectangles
// which are annotated with additional state which helps iOS draw selections in its unique way.
// No annotations are added in this class.
// FIXME: Move to RenderText with absoluteRectsForRange()?
void RenderObject::collectSelectionRects(Vector<SelectionRect>& rects, unsigned start, unsigned end)
{
    Vector<FloatQuad> quads;

    if (!firstChildSlow()) {
        // FIXME: WebKit's position for an empty span after a BR is incorrect, so we can't trust 
        // quads for them. We don't need selection rects for those anyway though, since they 
        // are just empty containers. See <https://bugs.webkit.org/show_bug.cgi?id=49358>.
        RenderObject* previous = previousSibling();
        Node* node = this->node();
        if (!previous || !previous->isBR() || !node || !node->isContainerNode() || !isInline()) {
            // For inline elements we don't use absoluteQuads, since it takes into account continuations and leads to wrong results.
            absoluteQuadsForSelection(quads);
        }
    } else {
        unsigned offset = start;
        for (RenderObject* child = childAt(start); child && offset < end; child = child->nextSibling(), ++offset)
            child->absoluteQuads(quads);
    }

    unsigned numberOfQuads = quads.size();
    for (unsigned i = 0; i < numberOfQuads; ++i)
        rects.append(SelectionRect(quads[i].enclosingBoundingBox(), isHorizontalWritingMode(), view().pageNumberForBlockProgressionOffset(quads[i].enclosingBoundingBox().x())));
}
#endif

IntRect RenderObject::absoluteBoundingBoxRect(bool useTransforms) const
{
    if (useTransforms) {
        Vector<FloatQuad> quads;
        absoluteQuads(quads);

        size_t n = quads.size();
        if (!n)
            return IntRect();
    
        IntRect result = quads[0].enclosingBoundingBox();
        for (size_t i = 1; i < n; ++i)
            result.unite(quads[i].enclosingBoundingBox());
        return result;
    }

    FloatPoint absPos = localToAbsolute();
    Vector<IntRect> rects;
    absoluteRects(rects, flooredLayoutPoint(absPos));

    size_t n = rects.size();
    if (!n)
        return IntRect();

    LayoutRect result = rects[0];
    for (size_t i = 1; i < n; ++i)
        result.unite(rects[i]);
    return pixelSnappedIntRect(result);
}

void RenderObject::absoluteFocusRingQuads(Vector<FloatQuad>& quads)
{
    Vector<IntRect> rects;
    // FIXME: addFocusRingRects() needs to be passed this transform-unaware
    // localToAbsolute() offset here because RenderInline::addFocusRingRects()
    // implicitly assumes that. This doesn't work correctly with transformed
    // descendants.
    FloatPoint absolutePoint = localToAbsolute();
    addFocusRingRects(rects, flooredLayoutPoint(absolutePoint));
    size_t count = rects.size();
    for (size_t i = 0; i < count; ++i) {
        IntRect rect = rects[i];
        rect.move(-absolutePoint.x(), -absolutePoint.y());
        quads.append(localToAbsoluteQuad(FloatQuad(rect)));
    }
}

FloatRect RenderObject::absoluteBoundingBoxRectForRange(const Range* range)
{
    if (!range || !range->startContainer())
        return FloatRect();

    range->ownerDocument().updateLayout();

    Vector<FloatQuad> quads;
    range->textQuads(quads);

    if (quads.isEmpty())
        return FloatRect();

    FloatRect result = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.uniteEvenIfEmpty(quads[i].boundingBox());

    return result;
}

void RenderObject::addAbsoluteRectForLayer(LayoutRect& result)
{
    if (hasLayer())
        result.unite(absoluteBoundingBoxRectIgnoringTransforms());
    for (RenderObject* current = firstChildSlow(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
}

// FIXME: change this to use the subtreePaint terminology
LayoutRect RenderObject::paintingRootRect(LayoutRect& topLevelRect)
{
    LayoutRect result = absoluteBoundingBoxRectIgnoringTransforms();
    topLevelRect = result;
    for (RenderObject* current = firstChildSlow(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
    return result;
}

RenderLayerModelObject* RenderObject::containerForRepaint() const
{
    RenderLayerModelObject* repaintContainer = nullptr;

    if (view().usesCompositing()) {
        if (RenderLayer* parentLayer = enclosingLayer()) {
            RenderLayer* compLayer = parentLayer->enclosingCompositingLayerForRepaint();
            if (compLayer)
                repaintContainer = &compLayer->renderer();
        }
    }
    
#if ENABLE(CSS_FILTERS)
    if (view().hasSoftwareFilters()) {
        if (RenderLayer* parentLayer = enclosingLayer()) {
            RenderLayer* enclosingFilterLayer = parentLayer->enclosingFilterLayer();
            if (enclosingFilterLayer)
                return &enclosingFilterLayer->renderer();
        }
    }
#endif

    // If we have a flow thread, then we need to do individual repaints within the RenderRegions instead.
    // Return the flow thread as a repaint container in order to create a chokepoint that allows us to change
    // repainting to do individual region repaints.
    RenderFlowThread* parentRenderFlowThread = flowThreadContainingBlock();
    if (parentRenderFlowThread) {
        // If the element has a fixed positioned element with named flow as CB along the CB chain
        // then the repaint container is not the flow thread.
        if (hasFixedPosInNamedFlowContainingBlock(this))
            return repaintContainer;
        // If we have already found a repaint container then we will repaint into that container only if it is part of the same
        // flow thread. Otherwise we will need to catch the repaint call and send it to the flow thread.
        RenderFlowThread* repaintContainerFlowThread = repaintContainer ? repaintContainer->flowThreadContainingBlock() : 0;
        if (!repaintContainerFlowThread || repaintContainerFlowThread != parentRenderFlowThread)
            repaintContainer = parentRenderFlowThread;
    }
    return repaintContainer;
}

void RenderObject::repaintUsingContainer(const RenderLayerModelObject* repaintContainer, const LayoutRect& r, bool shouldClipToLayer) const
{
    if (!repaintContainer) {
        view().repaintViewRectangle(r);
        return;
    }

    if (repaintContainer->isRenderFlowThread()) {
        toRenderFlowThread(repaintContainer)->repaintRectangleInRegions(r);
        return;
    }

#if ENABLE(CSS_FILTERS)
    if (repaintContainer->hasFilter() && repaintContainer->layer() && repaintContainer->layer()->requiresFullLayerImageForFilters()) {
        repaintContainer->layer()->setFilterBackendNeedsRepaintingInRect(r);
        return;
    }
#endif

    RenderView& v = view();
    if (repaintContainer->isRenderView()) {
        ASSERT(repaintContainer == &v);
        bool viewHasCompositedLayer = v.hasLayer() && v.layer()->isComposited();
        if (!viewHasCompositedLayer || v.layer()->backing()->paintsIntoWindow()) {
            v.repaintViewRectangle(viewHasCompositedLayer && v.layer()->transform() ? LayoutRect(v.layer()->transform()->mapRect(pixelSnappedForPainting(r, document().deviceScaleFactor()))) : r);
            return;
        }
    }
    
    if (v.usesCompositing()) {
        ASSERT(repaintContainer->hasLayer() && repaintContainer->layer()->isComposited());
        repaintContainer->layer()->setBackingNeedsRepaintInRect(r, shouldClipToLayer ? GraphicsLayer::ClipToLayer : GraphicsLayer::DoNotClipToLayer);
    }
}

void RenderObject::repaint() const
{
    // Don't repaint if we're unrooted (note that view() still returns the view when unrooted)
    RenderView* view;
    if (!isRooted(&view))
        return;

    if (view->printing())
        return; // Don't repaint if we're printing.

    RenderLayerModelObject* repaintContainer = containerForRepaint();
    repaintUsingContainer(repaintContainer ? repaintContainer : view, clippedOverflowRectForRepaint(repaintContainer));
}

void RenderObject::repaintRectangle(const LayoutRect& r, bool shouldClipToLayer) const
{
    // Don't repaint if we're unrooted (note that view() still returns the view when unrooted)
    RenderView* view;
    if (!isRooted(&view))
        return;

    if (view->printing())
        return; // Don't repaint if we're printing.

    LayoutRect dirtyRect(r);

    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    dirtyRect.move(view->layoutDelta());

    RenderLayerModelObject* repaintContainer = containerForRepaint();
    computeRectForRepaint(repaintContainer, dirtyRect);
    repaintUsingContainer(repaintContainer ? repaintContainer : view, dirtyRect, shouldClipToLayer);
}

void RenderObject::repaintSlowRepaintObject() const
{
    // Don't repaint if we're unrooted (note that view() still returns the view when unrooted)
    RenderView* view;
    if (!isRooted(&view))
        return;

    // Don't repaint if we're printing.
    if (view->printing())
        return;

    RenderLayerModelObject* repaintContainer = containerForRepaint();
    if (!repaintContainer)
        repaintContainer = view;

    bool shouldClipToLayer = true;
    IntRect repaintRect;

    // If this is the root background, we need to check if there is an extended background rect. If
    // there is, then we should not allow painting to clip to the layer size.
    if (isRoot() || isBody()) {
        shouldClipToLayer = !view->frameView().hasExtendedBackgroundRectForPainting();
        repaintRect = pixelSnappedIntRect(view->backgroundRect(view));
    } else
        repaintRect = pixelSnappedIntRect(clippedOverflowRectForRepaint(repaintContainer));

    repaintUsingContainer(repaintContainer, repaintRect, shouldClipToLayer);
}

IntRect RenderObject::pixelSnappedAbsoluteClippedOverflowRect() const
{
    return pixelSnappedIntRect(absoluteClippedOverflowRect());
}

bool RenderObject::checkForRepaintDuringLayout() const
{
    return !document().view()->needsFullRepaint() && !hasLayer() && everHadLayout();
}

LayoutRect RenderObject::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(clippedOverflowRectForRepaint(repaintContainer));
    r.inflate(outlineWidth);
    return r;
}

LayoutRect RenderObject::clippedOverflowRectForRepaint(const RenderLayerModelObject*) const
{
    ASSERT_NOT_REACHED();
    return LayoutRect();
}

void RenderObject::computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect& rect, bool fixed) const
{
    if (repaintContainer == this)
        return;

    if (auto o = parent()) {
        if (o->hasOverflowClip()) {
            RenderBox* boxParent = toRenderBox(o);
            boxParent->applyCachedClipAndScrollOffsetForRepaint(rect);
            if (rect.isEmpty())
                return;
        }

        o->computeRectForRepaint(repaintContainer, rect, fixed);
    }
}

void RenderObject::computeFloatRectForRepaint(const RenderLayerModelObject*, FloatRect&, bool) const
{
    ASSERT_NOT_REACHED();
}

#ifndef NDEBUG

void RenderObject::showTreeForThis() const
{
    if (node())
        node()->showTreeForThis();
}

void RenderObject::showRenderTreeForThis() const
{
    showRenderTree(this, 0);
}

void RenderObject::showLineTreeForThis() const
{
    if (containingBlock())
        containingBlock()->showLineTreeAndMark(0, 0, 0, 0, this);
}

void RenderObject::showRenderObject() const
{
    showRenderObject(0);
}

void RenderObject::showRegionsInformation(int& printedCharacters) const
{
    CurrentRenderFlowThreadDisabler flowThreadDisabler(&view());

    if (RenderFlowThread* flowThread = flowThreadContainingBlock()) {
        const RenderBox* box = isBox() ? toRenderBox(this) : nullptr;
        if (box) {
            RenderRegion* startRegion = nullptr;
            RenderRegion* endRegion = nullptr;
            flowThread->getRegionRangeForBox(box, startRegion, endRegion);
            printedCharacters += fprintf(stderr, " [Rs:%p Re:%p]", startRegion, endRegion);
        }
    }
}

void RenderObject::showRenderObject(int printedCharacters) const
{
    // As this function is intended to be used when debugging, the
    // this pointer may be 0.
    if (!this) {
        fputs("(null)\n", stderr);
        return;
    }

    printedCharacters += fprintf(stderr, "%s %p", renderName(), this);
    showRegionsInformation(printedCharacters);

    if (node()) {
        if (printedCharacters)
            for (; printedCharacters < showTreeCharacterOffset; printedCharacters++)
                fputc(' ', stderr);
        fputc('\t', stderr);
        node()->showNode();
    } else
        fputc('\n', stderr);
}

void RenderObject::showRenderTreeAndMark(const RenderObject* markedObject1, const char* markedLabel1, const RenderObject* markedObject2, const char* markedLabel2, int depth) const
{
    int printedCharacters = 0;
    if (markedObject1 == this && markedLabel1)
        printedCharacters += fprintf(stderr, "%s", markedLabel1);
    if (markedObject2 == this && markedLabel2)
        printedCharacters += fprintf(stderr, "%s", markedLabel2);
    for (; printedCharacters < depth * 2; printedCharacters++)
        fputc(' ', stderr);

    showRenderObject(printedCharacters);
    if (!this)
        return;

    for (const RenderObject* child = firstChildSlow(); child; child = child->nextSibling())
        child->showRenderTreeAndMark(markedObject1, markedLabel1, markedObject2, markedLabel2, depth + 1);
}

#endif // NDEBUG

Color RenderObject::selectionBackgroundColor() const
{
    Color color;
    if (style().userSelect() != SELECT_NONE) {
        if (frame().selection().shouldShowBlockCursor() && frame().selection().isCaret())
            color = style().visitedDependentColor(CSSPropertyColor).blendWithWhite();
        else {
            RefPtr<RenderStyle> pseudoStyle = selectionPseudoStyle();
            if (pseudoStyle && pseudoStyle->visitedDependentColor(CSSPropertyBackgroundColor).isValid())
                color = pseudoStyle->visitedDependentColor(CSSPropertyBackgroundColor).blendWithWhite();
            else
                color = frame().selection().isFocusedAndActive() ? theme().activeSelectionBackgroundColor() : theme().inactiveSelectionBackgroundColor();
        }
    }

    return color;
}

Color RenderObject::selectionColor(int colorProperty) const
{
    Color color;
    // If the element is unselectable, or we are only painting the selection,
    // don't override the foreground color with the selection foreground color.
    if (style().userSelect() == SELECT_NONE
        || (view().frameView().paintBehavior() & PaintBehaviorSelectionOnly))
        return color;

    if (RefPtr<RenderStyle> pseudoStyle = selectionPseudoStyle()) {
        color = pseudoStyle->visitedDependentColor(colorProperty);
        if (!color.isValid())
            color = pseudoStyle->visitedDependentColor(CSSPropertyColor);
    } else
        color = frame().selection().isFocusedAndActive() ? theme().activeSelectionForegroundColor() : theme().inactiveSelectionForegroundColor();

    return color;
}

PassRefPtr<RenderStyle> RenderObject::selectionPseudoStyle() const
{
    if (isAnonymous())
        return nullptr;

    if (ShadowRoot* root = m_node.containingShadowRoot()) {
        if (root->type() == ShadowRoot::UserAgentShadowRoot) {
            if (Element* shadowHost = m_node.shadowHost())
                return shadowHost->renderer()->getUncachedPseudoStyle(PseudoStyleRequest(SELECTION));
        }
    }

    return getUncachedPseudoStyle(PseudoStyleRequest(SELECTION));
}

Color RenderObject::selectionForegroundColor() const
{
    return selectionColor(CSSPropertyWebkitTextFillColor);
}

Color RenderObject::selectionEmphasisMarkColor() const
{
    return selectionColor(CSSPropertyWebkitTextEmphasisColor);
}

SelectionSubtreeRoot& RenderObject::selectionRoot() const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (flowThread && flowThread->isRenderNamedFlowThread())
        return *toRenderNamedFlowThread(flowThread);

    return view();
}

void RenderObject::selectionStartEnd(int& spos, int& epos) const
{
    selectionRoot().selectionStartEndPositions(spos, epos);
}

void RenderObject::handleDynamicFloatPositionChange()
{
    // We have gone from not affecting the inline status of the parent flow to suddenly
    // having an impact.  See if there is a mismatch between the parent flow's
    // childrenInline() state and our state.
    setInline(style().isDisplayInlineType());
    if (isInline() != parent()->childrenInline()) {
        if (!isInline())
            toRenderBoxModelObject(parent())->childBecameNonInline(this);
        else {
            // An anonymous block must be made to wrap this inline.
            RenderBlock* block = toRenderBlock(parent())->createAnonymousBlock();
            parent()->insertChildInternal(block, this, RenderElement::NotifyChildren);
            parent()->removeChildInternal(*this, RenderElement::NotifyChildren);
            block->insertChildInternal(this, nullptr, RenderElement::NotifyChildren);
        }
    }
}

void RenderObject::removeAnonymousWrappersForInlinesIfNecessary()
{
    RenderBlock* parentBlock = toRenderBlock(parent());
    if (!parentBlock->canCollapseAnonymousBlockChild())
        return;

    // We have changed to floated or out-of-flow positioning so maybe all our parent's
    // children can be inline now. Bail if there are any block children left on the line,
    // otherwise we can proceed to stripping solitary anonymous wrappers from the inlines.
    // FIXME: We should also handle split inlines here - we exclude them at the moment by returning
    // if we find a continuation.
    RenderObject* curr = parent()->firstChild();
    while (curr && ((curr->isAnonymousBlock() && !toRenderBlock(curr)->isAnonymousBlockContinuation()) || curr->style().isFloating() || curr->style().hasOutOfFlowPosition()))
        curr = curr->nextSibling();

    if (curr)
        return;

    curr = parent()->firstChild();
    while (curr) {
        RenderObject* next = curr->nextSibling();
        if (curr->isAnonymousBlock())
            parentBlock->collapseAnonymousBoxChild(parentBlock, toRenderBlock(curr));
        curr = next;
    }
}

FloatPoint RenderObject::localToAbsolute(const FloatPoint& localPoint, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(0, transformState, mode | ApplyContainerFlip);
    transformState.flatten();
    
    return transformState.lastPlanarPoint();
}

FloatPoint RenderObject::absoluteToLocal(const FloatPoint& containerPoint, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, containerPoint);
    mapAbsoluteToLocalPoint(mode, transformState);
    transformState.flatten();
    
    return transformState.lastPlanarPoint();
}

FloatQuad RenderObject::absoluteToLocalQuad(const FloatQuad& quad, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, quad.boundingBox().center(), quad);
    mapAbsoluteToLocalPoint(mode, transformState);
    transformState.flatten();
    return transformState.lastPlanarQuad();
}

void RenderObject::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    if (repaintContainer == this)
        return;

    auto o = parent();
    if (!o)
        return;

    // FIXME: this should call offsetFromContainer to share code, but I'm not sure it's ever called.
    LayoutPoint centerPoint = roundedLayoutPoint(transformState.mappedPoint());
    if (mode & ApplyContainerFlip && o->isBox()) {
        if (o->style().isFlippedBlocksWritingMode())
            transformState.move(toRenderBox(o)->flipForWritingMode(roundedLayoutPoint(transformState.mappedPoint())) - centerPoint);
        mode &= ~ApplyContainerFlip;
    }

    if (o->isBox())
        transformState.move(-toRenderBox(o)->scrolledContentOffset());

    o->mapLocalToContainer(repaintContainer, transformState, mode, wasFixed);
}

const RenderObject* RenderObject::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT_UNUSED(ancestorToStopAt, ancestorToStopAt != this);

    auto container = parent();
    if (!container)
        return 0;

    // FIXME: this should call offsetFromContainer to share code, but I'm not sure it's ever called.
    LayoutSize offset;
    if (container->isBox())
        offset = -toRenderBox(container)->scrolledContentOffset();

    geometryMap.push(this, offset, false);
    
    return container;
}

void RenderObject::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    auto o = parent();
    if (o) {
        o->mapAbsoluteToLocalPoint(mode, transformState);
        if (o->isBox())
            transformState.move(toRenderBox(o)->scrolledContentOffset());
    }
}

bool RenderObject::shouldUseTransformFromContainer(const RenderObject* containerObject) const
{
#if ENABLE(3D_RENDERING)
    // hasTransform() indicates whether the object has transform, transform-style or perspective. We just care about transform,
    // so check the layer's transform directly.
    return (hasLayer() && toRenderLayerModelObject(this)->layer()->transform()) || (containerObject && containerObject->style().hasPerspective());
#else
    UNUSED_PARAM(containerObject);
    return hasTransform();
#endif
}

void RenderObject::getTransformFromContainer(const RenderObject* containerObject, const LayoutSize& offsetInContainer, TransformationMatrix& transform) const
{
    transform.makeIdentity();
    transform.translate(offsetInContainer.width(), offsetInContainer.height());
    RenderLayer* layer;
    if (hasLayer() && (layer = toRenderLayerModelObject(this)->layer()) && layer->transform())
        transform.multiply(layer->currentTransform());
    
#if ENABLE(3D_RENDERING)
    if (containerObject && containerObject->hasLayer() && containerObject->style().hasPerspective()) {
        // Perpsective on the container affects us, so we have to factor it in here.
        ASSERT(containerObject->hasLayer());
        FloatPoint perspectiveOrigin = toRenderLayerModelObject(containerObject)->layer()->perspectiveOrigin();

        TransformationMatrix perspectiveMatrix;
        perspectiveMatrix.applyPerspective(containerObject->style().perspective());
        
        transform.translateRight3d(-perspectiveOrigin.x(), -perspectiveOrigin.y(), 0);
        transform = perspectiveMatrix * transform;
        transform.translateRight3d(perspectiveOrigin.x(), perspectiveOrigin.y(), 0);
    }
#else
    UNUSED_PARAM(containerObject);
#endif
}

FloatQuad RenderObject::localToContainerQuad(const FloatQuad& localQuad, const RenderLayerModelObject* repaintContainer, MapCoordinatesFlags mode, bool* wasFixed) const
{
    // Track the point at the center of the quad's bounding box. As mapLocalToContainer() calls offsetFromContainer(),
    // it will use that point as the reference point to decide which column's transform to apply in multiple-column blocks.
    TransformState transformState(TransformState::ApplyTransformDirection, localQuad.boundingBox().center(), localQuad);
    mapLocalToContainer(repaintContainer, transformState, mode | ApplyContainerFlip | UseTransforms, wasFixed);
    transformState.flatten();
    
    return transformState.lastPlanarQuad();
}

FloatPoint RenderObject::localToContainerPoint(const FloatPoint& localPoint, const RenderLayerModelObject* repaintContainer, MapCoordinatesFlags mode, bool* wasFixed) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(repaintContainer, transformState, mode | ApplyContainerFlip | UseTransforms, wasFixed);
    transformState.flatten();

    return transformState.lastPlanarPoint();
}

LayoutSize RenderObject::offsetFromContainer(RenderObject* o, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    ASSERT(o == container());

    LayoutSize offset;
    if (o->isBox())
        offset -= toRenderBox(o)->scrolledContentOffset();

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = o->isRenderFlowThread();

    return offset;
}

LayoutSize RenderObject::offsetFromAncestorContainer(RenderObject* container) const
{
    LayoutSize offset;
    LayoutPoint referencePoint;
    const RenderObject* currContainer = this;
    do {
        auto nextContainer = currContainer->container();
        ASSERT(nextContainer);  // This means we reached the top without finding container.
        if (!nextContainer)
            break;
        ASSERT(!currContainer->hasTransform());
        LayoutSize currentOffset = currContainer->offsetFromContainer(nextContainer, referencePoint);
        offset += currentOffset;
        referencePoint.move(currentOffset);
        currContainer = nextContainer;
    } while (currContainer != container);

    return offset;
}

LayoutRect RenderObject::localCaretRect(InlineBox*, int, LayoutUnit* extraWidthToEndOfLine)
{
    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = 0;

    return LayoutRect();
}

bool RenderObject::isRooted(RenderView** view) const
{
    const RenderObject* o = this;
    while (o->parent())
        o = o->parent();

    if (!o->isRenderView())
        return false;

    if (view)
        *view = &const_cast<RenderView&>(toRenderView(*o));

    return true;
}

RespectImageOrientationEnum RenderObject::shouldRespectImageOrientation() const
{
#if USE(CG) || USE(CAIRO)
    // This can only be enabled for ports which honor the orientation flag in their drawing code.
    if (document().isImageDocument())
        return RespectImageOrientation;
#endif
    // Respect the image's orientation if it's being used as a full-page image or it's
    // an <img> and the setting to respect it everywhere is set.
    return (frame().settings().shouldRespectImageOrientation() && node() && isHTMLImageElement(node())) ? RespectImageOrientation : DoNotRespectImageOrientation;
}

bool RenderObject::hasOutlineAnnotation() const
{
    return node() && node()->isLink() && document().printing();
}

bool RenderObject::hasEntirelyFixedBackground() const
{
    return style().hasEntirelyFixedBackground();
}

RenderElement* RenderObject::container(const RenderLayerModelObject* repaintContainer, bool* repaintContainerSkipped) const
{
    if (repaintContainerSkipped)
        *repaintContainerSkipped = false;

    // This method is extremely similar to containingBlock(), but with a few notable
    // exceptions.
    // (1) It can be used on orphaned subtrees, i.e., it can be called safely even when
    // the object is not part of the primary document subtree yet.
    // (2) For normal flow elements, it just returns the parent.
    // (3) For absolute positioned elements, it will return a relative positioned inline.
    // containingBlock() simply skips relpositioned inlines and lets an enclosing block handle
    // the layout of the positioned object.  This does mean that computePositionedLogicalWidth and
    // computePositionedLogicalHeight have to use container().
    auto o = parent();

    if (isText())
        return o;

    EPosition pos = style().position();
    if (pos == FixedPosition) {
        // container() can be called on an object that is not in the
        // tree yet.  We don't call view() since it will assert if it
        // can't get back to the canvas.  Instead we just walk as high up
        // as we can.  If we're in the tree, we'll get the root.  If we
        // aren't we'll get the root of our little subtree (most likely
        // we'll just return 0).
        // FIXME: The definition of view() has changed to not crawl up the render tree.  It might
        // be safe now to use it.
        while (o && o->parent() && !(o->hasTransform() && o->isRenderBlock())) {
            // foreignObject is the containing block for its contents.
            if (o->isSVGForeignObject())
                break;

            // The render flow thread is the top most containing block
            // for the fixed positioned elements.
            if (o->isOutOfFlowRenderFlowThread())
                break;

            if (repaintContainerSkipped && o == repaintContainer)
                *repaintContainerSkipped = true;

            o = o->parent();
        }
    } else if (pos == AbsolutePosition) {
        // Same goes here.  We technically just want our containing block, but
        // we may not have one if we're part of an uninstalled subtree.  We'll
        // climb as high as we can though.
        while (o && o->style().position() == StaticPosition && !o->isRenderView() && !(o->hasTransform() && o->isRenderBlock())) {
            if (o->isSVGForeignObject()) // foreignObject is the containing block for contents inside it
                break;

            if (repaintContainerSkipped && o == repaintContainer)
                *repaintContainerSkipped = true;

            o = o->parent();
        }
    }

    return o;
}

bool RenderObject::isSelectionBorder() const
{
    SelectionState st = selectionState();
    return st == SelectionStart
        || st == SelectionEnd
        || st == SelectionBoth
        || view().selectionStart() == this
        || view().selectionEnd() == this;
}

inline void RenderObject::clearLayoutRootIfNeeded() const
{
    if (documentBeingDestroyed())
        return;

    if (view().frameView().layoutRoot() == this) {
        ASSERT_NOT_REACHED();
        // This indicates a failure to layout the child, which is why
        // the layout root is still set to |this|. Make sure to clear it
        // since we are getting destroyed.
        view().frameView().clearLayoutRoot();
    }
}

void RenderObject::willBeDestroyed()
{
    // For accessibility management, notify the parent of the imminent change to its child set.
    // We do it now, before remove(), while the parent pointer is still available.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(this->parent());

    removeFromParent();

    ASSERT(documentBeingDestroyed() || !isRenderElement() || !view().frameView().hasSlowRepaintObject(toRenderElement(this)));

    // The remove() call above may invoke axObjectCache()->childrenChanged() on the parent, which may require the AX render
    // object for this renderer. So we remove the AX render object now, after the renderer is removed.
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->remove(this);

    // If this renderer had a parent, remove should have destroyed any counters
    // attached to this renderer and marked the affected other counters for
    // reevaluation. This apparently redundant check is here for the case when
    // this renderer had no parent at the time remove() was called.

    if (hasCounterNodeMap())
        RenderCounter::destroyCounterNodes(this);

    // FIXME: Would like to do this in RenderBoxModelObject, but the timing is so complicated that this can't easily
    // be moved into RenderBoxModelObject::destroy.
    if (hasLayer()) {
        setHasLayer(false);
        toRenderLayerModelObject(this)->destroyLayer();
    }

    clearLayoutRootIfNeeded();
}

void RenderObject::insertedIntoTree()
{
    // FIXME: We should ASSERT(isRooted()) here but generated content makes some out-of-order insertion.

    if (!isFloating() && parent()->childrenInline())
        parent()->dirtyLinesFromChangedChild(this);
    
    // We have to unset the current layout RenderFlowThread here, since insertedIntoTree() can happen in
    // the middle of layout but for objects inside a nested flow thread that is still being populated. This
    // will cause an accurate crawl to happen in order to ensure that the right flow thread is notified.
    RenderFlowThread* previousThread = view().flowThreadController().currentRenderFlowThread();
    view().flowThreadController().setCurrentRenderFlowThread(nullptr);
    if (parent()->isRenderFlowThread())
        toRenderFlowThread(parent())->flowThreadDescendantInserted(this);
    else if (RenderFlowThread* flowThread = parent()->flowThreadContainingBlock())
        flowThread->flowThreadDescendantInserted(this);
    view().flowThreadController().setCurrentRenderFlowThread(previousThread);
}

void RenderObject::willBeRemovedFromTree()
{
    // FIXME: We should ASSERT(isRooted()) but we have some out-of-order removals which would need to be fixed first.

    removeFromRenderFlowThread();

    // Update cached boundaries in SVG renderers, if a child is removed.
    parent()->setNeedsBoundariesUpdate();
}

void RenderObject::removeFromRenderFlowThread()
{
    if (flowThreadState() == NotInsideFlowThread)
        return;
    
    // Sometimes we remove the element from the flow, but it's not destroyed at that time.
    // It's only until later when we actually destroy it and remove all the children from it. 
    // Currently, that happens for firstLetter elements and list markers.
    // Pass in the flow thread so that we don't have to look it up for all the children.
    removeFromRenderFlowThreadRecursive(flowThreadContainingBlock());
}

void RenderObject::removeFromRenderFlowThreadRecursive(RenderFlowThread* renderFlowThread)
{
    for (RenderObject* child = firstChildSlow(); child; child = child->nextSibling())
        child->removeFromRenderFlowThreadRecursive(renderFlowThread);

    RenderFlowThread* localFlowThread = renderFlowThread;
    if (flowThreadState() == InsideInFlowThread)
        localFlowThread = flowThreadContainingBlock(); // We have to ask. We can't just assume we are in the same flow thread.
    if (localFlowThread)
        localFlowThread->removeFlowChildInfo(this);
    setFlowThreadState(NotInsideFlowThread);
}

void RenderObject::destroyAndCleanupAnonymousWrappers()
{
    // If the tree is destroyed, there is no need for a clean-up phase.
    if (documentBeingDestroyed()) {
        destroy();
        return;
    }

    RenderObject* destroyRoot = this;
    for (auto destroyRootParent = destroyRoot->parent(); destroyRootParent && destroyRootParent->isAnonymous(); destroyRoot = destroyRootParent, destroyRootParent = destroyRootParent->parent()) {
        // Currently we only remove anonymous cells' and table sections' wrappers but we should remove all unneeded
        // wrappers. See http://webkit.org/b/52123 as an example where this is needed.
        if (!destroyRootParent->isTableCell() && !destroyRootParent->isTableSection())
            break;

        if (destroyRootParent->firstChild() != this || destroyRootParent->lastChild() != this)
            break;
    }

    destroyRoot->destroy();

    // WARNING: |this| is deleted here.
}

void RenderObject::destroy()
{
#if PLATFORM(IOS)
    if (hasLayer())
        toRenderBoxModelObject(this)->layer()->willBeDestroyed();
#endif

    willBeDestroyed();
    delete this;
}

VisiblePosition RenderObject::positionForPoint(const LayoutPoint&, const RenderRegion*)
{
    return createVisiblePosition(caretMinOffset(), DOWNSTREAM);
}

void RenderObject::updateDragState(bool dragOn)
{
    bool valueChanged = (dragOn != isDragging());
    setIsDragging(dragOn);
    if (valueChanged && node() && (style().affectedByDrag() || (node()->isElementNode() && toElement(node())->childrenAffectedByDrag())))
        node()->setNeedsStyleRecalc();
    for (RenderObject* curr = firstChildSlow(); curr; curr = curr->nextSibling())
        curr->updateDragState(dragOn);
}

bool RenderObject::isComposited() const
{
    return hasLayer() && toRenderLayerModelObject(this)->layer()->isComposited();
}

bool RenderObject::hitTest(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestFilter hitTestFilter)
{
    bool inside = false;
    if (hitTestFilter != HitTestSelf) {
        // First test the foreground layer (lines and inlines).
        inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestForeground);

        // Test floats next.
        if (!inside)
            inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestFloat);

        // Finally test to see if the mouse is in the background (within a child block's background).
        if (!inside)
            inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestChildBlockBackgrounds);
    }

    // See if the mouse is inside us but not any of our descendants
    if (hitTestFilter != HitTestDescendants && !inside)
        inside = nodeAtPoint(request, result, locationInContainer, accumulatedOffset, HitTestBlockBackground);

    return inside;
}

void RenderObject::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    Node* node = this->node();

    // If we hit the anonymous renderers inside generated content we should
    // actually hit the generated content so walk up to the PseudoElement.
    if (!node && parent() && parent()->isBeforeOrAfterContent()) {
        for (auto renderer = parent(); renderer && !node; renderer = renderer->parent())
            node = renderer->element();
    }

    if (node) {
        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);
        result.setLocalPoint(point);
    }
}

bool RenderObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& /*locationInContainer*/, const LayoutPoint& /*accumulatedOffset*/, HitTestAction)
{
    return false;
}

int RenderObject::innerLineHeight() const
{
    return style().computedLineHeight();
}

RenderStyle* RenderObject::getCachedPseudoStyle(PseudoId pseudo, RenderStyle* parentStyle) const
{
    if (pseudo < FIRST_INTERNAL_PSEUDOID && !style().hasPseudoStyle(pseudo))
        return 0;

    RenderStyle* cachedStyle = style().getCachedPseudoStyle(pseudo);
    if (cachedStyle)
        return cachedStyle;
    
    RefPtr<RenderStyle> result = getUncachedPseudoStyle(PseudoStyleRequest(pseudo), parentStyle);
    if (result)
        return style().addCachedPseudoStyle(result.release());
    return 0;
}

PassRefPtr<RenderStyle> RenderObject::getUncachedPseudoStyle(const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle, RenderStyle* ownStyle) const
{
    if (pseudoStyleRequest.pseudoId < FIRST_INTERNAL_PSEUDOID && !ownStyle && !style().hasPseudoStyle(pseudoStyleRequest.pseudoId))
        return 0;
    
    if (!parentStyle) {
        ASSERT(!ownStyle);
        parentStyle = &style();
    }

    // FIXME: This "find nearest element parent" should be a helper function.
    Node* n = node();
    while (n && !n->isElementNode())
        n = n->parentNode();
    if (!n)
        return 0;
    Element* element = toElement(n);

    if (pseudoStyleRequest.pseudoId == FIRST_LINE_INHERITED) {
        RefPtr<RenderStyle> result = document().ensureStyleResolver().styleForElement(element, parentStyle, DisallowStyleSharing);
        result->setStyleType(FIRST_LINE_INHERITED);
        return result.release();
    }

    return document().ensureStyleResolver().pseudoStyleForElement(element, pseudoStyleRequest, parentStyle);
}

static Color decorationColor(RenderStyle* style)
{
    Color result;
    // Check for text decoration color first.
    result = style->visitedDependentColor(CSSPropertyWebkitTextDecorationColor);
    if (result.isValid())
        return result;
    if (style->textStrokeWidth() > 0) {
        // Prefer stroke color if possible but not if it's fully transparent.
        result = style->visitedDependentColor(CSSPropertyWebkitTextStrokeColor);
        if (result.alpha())
            return result;
    }
    
    result = style->visitedDependentColor(CSSPropertyWebkitTextFillColor);
    return result;
}

void RenderObject::getTextDecorationColors(int decorations, Color& underline, Color& overline, Color& linethrough, bool firstlineStyle)
{
    RenderObject* curr = this;
    RenderStyle* styleToUse = 0;
    TextDecoration currDecs = TextDecorationNone;
    Color resultColor;
    do {
        styleToUse = firstlineStyle ? &curr->firstLineStyle() : &curr->style();
        currDecs = styleToUse->textDecoration();
        resultColor = decorationColor(styleToUse);
        // Parameter 'decorations' is cast as an int to enable the bitwise operations below.
        if (currDecs) {
            if (currDecs & TextDecorationUnderline) {
                decorations &= ~TextDecorationUnderline;
                underline = resultColor;
            }
            if (currDecs & TextDecorationOverline) {
                decorations &= ~TextDecorationOverline;
                overline = resultColor;
            }
            if (currDecs & TextDecorationLineThrough) {
                decorations &= ~TextDecorationLineThrough;
                linethrough = resultColor;
            }
        }
        if (curr->isRubyText())
            return;
        curr = curr->parent();
        if (curr && curr->isAnonymousBlock() && toRenderBlock(curr)->continuation())
            curr = toRenderBlock(curr)->continuation();
    } while (curr && decorations && (!curr->node() || (!isHTMLAnchorElement(curr->node()) && !curr->node()->hasTagName(fontTag))));

    // If we bailed out, use the element we bailed out at (typically a <font> or <a> element).
    if (decorations && curr) {
        styleToUse = firstlineStyle ? &curr->firstLineStyle() : &curr->style();
        resultColor = decorationColor(styleToUse);
        if (decorations & TextDecorationUnderline)
            underline = resultColor;
        if (decorations & TextDecorationOverline)
            overline = resultColor;
        if (decorations & TextDecorationLineThrough)
            linethrough = resultColor;
    }
}

#if ENABLE(DASHBOARD_SUPPORT)
void RenderObject::addAnnotatedRegions(Vector<AnnotatedRegionValue>& regions)
{
    // Convert the style regions to absolute coordinates.
    if (style().visibility() != VISIBLE || !isBox())
        return;
    
    RenderBox* box = toRenderBox(this);
    FloatPoint absPos = localToAbsolute();

    const Vector<StyleDashboardRegion>& styleRegions = style().dashboardRegions();
    unsigned i, count = styleRegions.size();
    for (i = 0; i < count; i++) {
        StyleDashboardRegion styleRegion = styleRegions[i];

        LayoutUnit w = box->width();
        LayoutUnit h = box->height();

        AnnotatedRegionValue region;
        region.label = styleRegion.label;
        region.bounds = LayoutRect(styleRegion.offset.left().value(),
                                   styleRegion.offset.top().value(),
                                   w - styleRegion.offset.left().value() - styleRegion.offset.right().value(),
                                   h - styleRegion.offset.top().value() - styleRegion.offset.bottom().value());
        region.type = styleRegion.type;

        region.clip = region.bounds;
        computeAbsoluteRepaintRect(region.clip);
        if (region.clip.height() < 0) {
            region.clip.setHeight(0);
            region.clip.setWidth(0);
        }

        region.bounds.setX(absPos.x() + styleRegion.offset.left().value());
        region.bounds.setY(absPos.y() + styleRegion.offset.top().value());

        regions.append(region);
    }
}

void RenderObject::collectAnnotatedRegions(Vector<AnnotatedRegionValue>& regions)
{
    // RenderTexts don't have their own style, they just use their parent's style,
    // so we don't want to include them.
    if (isText())
        return;

    addAnnotatedRegions(regions);
    for (RenderObject* curr = toRenderElement(this)->firstChild(); curr; curr = curr->nextSibling())
        curr->collectAnnotatedRegions(regions);
}
#endif

int RenderObject::maximalOutlineSize(PaintPhase p) const
{
    if (p != PaintPhaseOutline && p != PaintPhaseSelfOutline && p != PaintPhaseChildOutlines)
        return 0;
    return view().maximalOutlineSize();
}

int RenderObject::caretMinOffset() const
{
    return 0;
}

int RenderObject::caretMaxOffset() const
{
    if (isReplaced())
        return node() ? std::max(1U, node()->childNodeCount()) : 1;
    if (isHR())
        return 1;
    return 0;
}

int RenderObject::previousOffset(int current) const
{
    return current - 1;
}

int RenderObject::previousOffsetForBackwardDeletion(int current) const
{
    return current - 1;
}

int RenderObject::nextOffset(int current) const
{
    return current + 1;
}

void RenderObject::adjustRectForOutlineAndShadow(LayoutRect& rect) const
{
    int outlineSize = outlineStyleForRepaint().outlineSize();
    if (const ShadowData* boxShadow = style().boxShadow()) {
        boxShadow->adjustRectForShadow(rect, outlineSize);
        return;
    }

    rect.inflate(outlineSize);
}

void RenderObject::imageChanged(CachedImage* image, const IntRect* rect)
{
    imageChanged(static_cast<WrappedImagePtr>(image), rect);
}

RenderBoxModelObject* RenderObject::offsetParent() const
{
    // If any of the following holds true return null and stop this algorithm:
    // A is the root element.
    // A is the HTML body element.
    // The computed value of the position property for element A is fixed.
    if (isRoot() || isBody() || (isOutOfFlowPositioned() && style().position() == FixedPosition))
        return 0;

    // If A is an area HTML element which has a map HTML element somewhere in the ancestor
    // chain return the nearest ancestor map HTML element and stop this algorithm.
    // FIXME: Implement!
    
    // Return the nearest ancestor element of A for which at least one of the following is
    // true and stop this algorithm if such an ancestor is found:
    //     * The computed value of the position property is not static.
    //     * It is the HTML body element.
    //     * The computed value of the position property of A is static and the ancestor
    //       is one of the following HTML elements: td, th, or table.
    //     * Our own extension: if there is a difference in the effective zoom

    bool skipTables = isPositioned();
    float currZoom = style().effectiveZoom();
    auto curr = parent();
    while (curr && (!curr->element() || (!curr->isPositioned() && !curr->isBody())) && !curr->isRenderNamedFlowThread()) {
        Element* element = curr->element();
        if (!skipTables && element && (isHTMLTableElement(element) || element->hasTagName(tdTag) || element->hasTagName(thTag)))
            break;
 
        float newZoom = curr->style().effectiveZoom();
        if (currZoom != newZoom)
            break;
        currZoom = newZoom;
        curr = curr->parent();
    }
    
    // CSS regions specification says that region flows should return the body element as their offsetParent.
    if (curr && curr->isRenderNamedFlowThread())
        curr = document().body() ? document().body()->renderer() : 0;
    
    return curr && curr->isBoxModelObject() ? toRenderBoxModelObject(curr) : 0;
}

VisiblePosition RenderObject::createVisiblePosition(int offset, EAffinity affinity) const
{
    // If this is a non-anonymous renderer in an editable area, then it's simple.
    if (Node* node = nonPseudoNode()) {
        if (!node->hasEditableStyle()) {
            // If it can be found, we prefer a visually equivalent position that is editable. 
            Position position = createLegacyEditingPosition(node, offset);
            Position candidate = position.downstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->hasEditableStyle())
                return VisiblePosition(candidate, affinity);
            candidate = position.upstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->hasEditableStyle())
                return VisiblePosition(candidate, affinity);
        }
        // FIXME: Eliminate legacy editing positions
        return VisiblePosition(createLegacyEditingPosition(node, offset), affinity);
    }

    // We don't want to cross the boundary between editable and non-editable
    // regions of the document, but that is either impossible or at least
    // extremely unlikely in any normal case because we stop as soon as we
    // find a single non-anonymous renderer.

    // Find a nearby non-anonymous renderer.
    const RenderObject* child = this;
    while (const auto parent = child->parent()) {
        // Find non-anonymous content after.
        const RenderObject* renderer = child;
        while ((renderer = renderer->nextInPreOrder(parent))) {
            if (Node* node = renderer->nonPseudoNode())
                return VisiblePosition(firstPositionInOrBeforeNode(node), DOWNSTREAM);
        }

        // Find non-anonymous content before.
        renderer = child;
        while ((renderer = renderer->previousInPreOrder())) {
            if (renderer == parent)
                break;
            if (Node* node = renderer->nonPseudoNode())
                return VisiblePosition(lastPositionInOrAfterNode(node), DOWNSTREAM);
        }

        // Use the parent itself unless it too is anonymous.
        if (Element* element = parent->nonPseudoElement())
            return VisiblePosition(firstPositionInOrBeforeNode(element), DOWNSTREAM);

        // Repeat at the next level up.
        child = parent;
    }

    // Everything was anonymous. Give up.
    return VisiblePosition();
}

VisiblePosition RenderObject::createVisiblePosition(const Position& position) const
{
    if (position.isNotNull())
        return VisiblePosition(position);

    ASSERT(!node());
    return createVisiblePosition(0, DOWNSTREAM);
}

CursorDirective RenderObject::getCursor(const LayoutPoint&, Cursor&) const
{
    return SetCursorBasedOnStyle;
}

bool RenderObject::canUpdateSelectionOnRootLineBoxes()
{
    if (needsLayout())
        return false;

    RenderBlock* containingBlock = this->containingBlock();
    return containingBlock ? !containingBlock->needsLayout() : true;
}

// We only create "generated" child renderers like one for first-letter if:
// - the firstLetterBlock can have children in the DOM and
// - the block doesn't have any special assumption on its text children.
// This correctly prevents form controls from having such renderers.
bool RenderObject::canHaveGeneratedChildren() const
{
    return canHaveChildren();
}

Node* RenderObject::generatingPseudoHostElement() const
{
    return toPseudoElement(node())->hostElement();
}

void RenderObject::setNeedsBoundariesUpdate()
{
    if (auto renderer = parent())
        renderer->setNeedsBoundariesUpdate();
}

FloatRect RenderObject::objectBoundingBox() const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

FloatRect RenderObject::strokeBoundingBox() const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

// Returns the smallest rectangle enclosing all of the painted content
// respecting clipping, masking, filters, opacity, stroke-width and markers
FloatRect RenderObject::repaintRectInLocalCoordinates() const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

AffineTransform RenderObject::localTransform() const
{
    static const AffineTransform identity;
    return identity;
}

const AffineTransform& RenderObject::localToParentTransform() const
{
    static const AffineTransform identity;
    return identity;
}

bool RenderObject::nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

RenderNamedFlowFragment* RenderObject::currentRenderNamedFlowFragment() const
{
    if (flowThreadState() == NotInsideFlowThread)
        return nullptr;

    RenderFlowThread* flowThread = view().flowThreadController().currentRenderFlowThread();
    if (!flowThread)
        return nullptr;

    ASSERT(flowThread == flowThreadContainingBlock());

    // FIXME: Once regions are fully integrated with the compositing system we should uncomment this assert.
    // This assert needs to be disabled because it's possible to ask for the ancestor clipping rectangle of
    // a layer without knowing the containing region in advance.
    // ASSERT(flowThread->currentRegion() && flowThread->currentRegion()->isRenderNamedFlowFragment());

    RenderNamedFlowFragment* namedFlowFragment = toRenderNamedFlowFragment(flowThread->currentRegion());
    return namedFlowFragment;
}

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::RenderObject* object)
{
    if (object)
        object->showTreeForThis();
}

void showLineTree(const WebCore::RenderObject* object)
{
    if (object)
        object->showLineTreeForThis();
}

void showRenderTree(const WebCore::RenderObject* object1)
{
    showRenderTree(object1, 0);
}

void showRenderTree(const WebCore::RenderObject* object1, const WebCore::RenderObject* object2)
{
    if (object1) {
        const WebCore::RenderObject* root = object1;
        while (root->parent())
            root = root->parent();
        root->showRenderTreeAndMark(object1, "*", object2, "-", 0);
    }
}

#endif
