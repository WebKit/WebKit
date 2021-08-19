/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "Editing.h"
#include "ElementAncestorIterator.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLBRElement.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HitTestResult.h"
#include "LayoutIntegrationLineLayout.h"
#include "LogicalSelectionOffsetCaches.h"
#include "Page.h"
#include "PseudoElement.h"
#include "RenderChildIterator.h"
#include "RenderCounter.h"
#include "RenderFragmentedFlow.h"
#include "RenderGeometryMap.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLineBreak.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderRuby.h"
#include "RenderSVGBlock.h"
#include "RenderSVGInline.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGRoot.h"
#include "RenderScrollbarPart.h"
#include "RenderTableRow.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGRenderSupport.h"
#include "StyleResolver.h"
#include "TransformState.h"
#include <algorithm>
#include <stdio.h>
#include <wtf/HexNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "SelectionGeometry.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderObject);

#if ASSERT_ENABLED

RenderObject::SetLayoutNeededForbiddenScope::SetLayoutNeededForbiddenScope(const RenderObject& renderObject, bool isForbidden)
    : m_renderObject(renderObject)
    , m_preexistingForbidden(m_renderObject.isSetNeedsLayoutForbidden())
{
    m_renderObject.setNeedsLayoutIsForbidden(isForbidden);
}

RenderObject::SetLayoutNeededForbiddenScope::~SetLayoutNeededForbiddenScope()
{
    m_renderObject.setNeedsLayoutIsForbidden(m_preexistingForbidden);
}

#endif

struct SameSizeAsRenderObject {
    virtual ~SameSizeAsRenderObject() = default; // Allocate vtable pointer.
#if ASSERT_ENABLED
    bool weakPtrFactorWasConstructedOnMainThread;
#endif
    void* pointers[5];
#if ASSERT_ENABLED
    unsigned m_debugBitfields : 2;
#endif
    unsigned m_bitfields;
};

COMPILE_ASSERT(sizeof(RenderObject) == sizeof(SameSizeAsRenderObject), RenderObject_should_stay_small);

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, renderObjectCounter, ("RenderObject"));

void RenderObjectDeleter::operator() (RenderObject* renderer) const
{
    renderer->destroy();
}

RenderObject::RenderObject(Node& node)
    : CachedImageClient()
    , m_node(node)
    , m_parent(nullptr)
    , m_previous(nullptr)
    , m_next(nullptr)
#if ASSERT_ENABLED
    , m_hasAXObject(false)
    , m_setNeedsLayoutForbidden(false)
#endif
    , m_bitfields(node)
{
    if (RenderView* renderView = node.document().renderView())
        renderView->didCreateRenderer();
#ifndef NDEBUG
    renderObjectCounter.increment();
#endif
}

RenderObject::~RenderObject()
{
    view().didDestroyRenderer();
    ASSERT(!m_hasAXObject);
#ifndef NDEBUG
    renderObjectCounter.decrement();
#endif
    ASSERT(!hasRareData());
}

RenderTheme& RenderObject::theme() const
{
    return RenderTheme::singleton();
}

bool RenderObject::isDescendantOf(const RenderObject* ancestor) const
{
    for (const RenderObject* renderer = this; renderer; renderer = renderer->m_parent) {
        if (renderer == ancestor)
            return true;
    }
    return false;
}

RenderElement* RenderObject::firstNonAnonymousAncestor() const
{
    auto* ancestor = parent();
    while (ancestor && ancestor->isAnonymous())
        ancestor = ancestor->parent();
    return ancestor;
}

bool RenderObject::isLegend() const
{
    return node() && node()->hasTagName(legendTag);
}

    
bool RenderObject::isFieldset() const
{
    return node() && node()->hasTagName(fieldsetTag);
}

bool RenderObject::isHTMLMarquee() const
{
    return node() && node()->renderer() == this && node()->hasTagName(marqueeTag);
}

void RenderObject::setFragmentedFlowStateIncludingDescendants(FragmentedFlowState state, const RenderElement* fragmentedFlowRoot)
{
    setFragmentedFlowState(state);

    if (!is<RenderElement>(*this))
        return;

    for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(*this))) {
        // If the child is a fragmentation context it already updated the descendants flag accordingly.
        if (child.isRenderFragmentedFlow())
            continue;
        if (fragmentedFlowRoot && child.isOutOfFlowPositioned()) {
            // Fragmented status propagation stops at out-of-flow boundary.
            auto isInsideMulticolumnFlow = [&] {
                auto* containingBlock = child.containingBlock();
                if (!containingBlock) {
                    ASSERT_NOT_REACHED();
                    return false;
                }
                // It's ok to only check the first level containing block (as opposed to the containing block chain) as setFragmentedFlowStateIncludingDescendants is top to down.
                return containingBlock->isDescendantOf(fragmentedFlowRoot);
            };
            if (!isInsideMulticolumnFlow())
                continue;
        }
        ASSERT(state != child.fragmentedFlowState());
        child.setFragmentedFlowStateIncludingDescendants(state, fragmentedFlowRoot);
    }
}

RenderObject::FragmentedFlowState RenderObject::computedFragmentedFlowState(const RenderObject& renderer)
{
    if (!renderer.parent())
        return renderer.fragmentedFlowState();

    if (is<RenderMultiColumnFlow>(renderer)) {
        // Multicolumn flows do not inherit the flow state.
        return InsideInFragmentedFlow;
    }

    auto inheritedFlowState = RenderObject::NotInsideFragmentedFlow;
    if (is<RenderText>(renderer))
        inheritedFlowState = renderer.parent()->fragmentedFlowState();
    else if (is<RenderSVGBlock>(renderer) || is<RenderSVGInline>(renderer) || is<RenderSVGModelObject>(renderer)) {
        // containingBlock() skips svg boundary (SVG root is a RenderReplaced).
        if (auto* svgRoot = SVGRenderSupport::findTreeRootObject(downcast<RenderElement>(renderer)))
            inheritedFlowState = svgRoot->fragmentedFlowState();
    } else if (auto* container = renderer.container())
        inheritedFlowState = container->fragmentedFlowState();
    else {
        // Splitting lines or doing continuation, so just keep the current state.
        inheritedFlowState = renderer.fragmentedFlowState();
    }
    return inheritedFlowState;
}

void RenderObject::initializeFragmentedFlowStateOnInsertion()
{
    ASSERT(parent());

    // A RenderFragmentedFlow is always considered to be inside itself, so it never has to change its state in response to parent changes.
    if (isRenderFragmentedFlow())
        return;

    auto computedState = computedFragmentedFlowState(*this);
    if (fragmentedFlowState() == computedState)
        return;

    auto* enclosingFragmentedFlow = locateEnclosingFragmentedFlow();
    setFragmentedFlowStateIncludingDescendants(computedState, enclosingFragmentedFlow ? enclosingFragmentedFlow->parent() : nullptr);
}

void RenderObject::resetFragmentedFlowStateOnRemoval()
{
    if (fragmentedFlowState() == NotInsideFragmentedFlow)
        return;

    if (!renderTreeBeingDestroyed() && is<RenderElement>(*this)) {
        downcast<RenderElement>(*this).removeFromRenderFragmentedFlow();
        return;
    }

    // A RenderFragmentedFlow is always considered to be inside itself, so it never has to change its state in response to parent changes.
    if (isRenderFragmentedFlow())
        return;

    auto* enclosingFragmentedFlow = this->enclosingFragmentedFlow();
    setFragmentedFlowStateIncludingDescendants(NotInsideFragmentedFlow, enclosingFragmentedFlow ? enclosingFragmentedFlow->parent() : nullptr);
}

void RenderObject::setParent(RenderElement* parent)
{
    m_parent = parent;
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
        return nullptr;

    const RenderObject* current = this;
    RenderObject* next;
    while (!(next = current->nextSibling())) {
        current = current->parent();
        if (!current || current == stayWithin)
            return nullptr;
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
        return nullptr;

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
        RenderObject* n = nullptr;
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
        RenderObject* n = nullptr;
        n = r->lastChildSlow();
        if (!n)
            break;
        r = n;
    }
    return r;
}

#if ENABLE(TEXT_AUTOSIZING)

// Non-recursive version of the DFS search.
RenderObject* RenderObject::traverseNext(const RenderObject* stayWithin, HeightTypeTraverseNextInclusionFunction inclusionFunction, int& currentDepth, int& newFixedDepth) const
{
    BlockContentHeightType overflowType;

    // Check for suitable children.
    for (RenderObject* child = firstChildSlow(); child; child = child->nextSibling()) {
        overflowType = inclusionFunction(*child);
        if (overflowType != FixedHeight) {
            currentDepth++;
            if (overflowType == OverflowHeight)
                newFixedDepth = currentDepth;
            ASSERT(!stayWithin || child->isDescendantOf(stayWithin));
            return child;
        }
    }

    if (this == stayWithin)
        return nullptr;

    // Now we traverse other nodes if they exist, otherwise
    // we go to the parent node and try doing the same.
    const RenderObject* n = this;
    while (n) {
        while (n && !n->nextSibling() && (!stayWithin || n->parent() != stayWithin)) {
            n = n->parent();
            currentDepth--;
        }
        if (!n)
            return nullptr;
        for (RenderObject* sibling = n->nextSibling(); sibling; sibling = sibling->nextSibling()) {
            overflowType = inclusionFunction(*sibling);
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
            return nullptr;
    }
    return nullptr;
}

#endif // ENABLE(TEXT_AUTOSIZING)

RenderLayer* RenderObject::enclosingLayer() const
{
    for (auto& renderer : lineageOfType<RenderLayerModelObject>(*this)) {
        if (renderer.hasLayer())
            return renderer.layer();
    }
    return nullptr;
}

bool RenderObject::scrollRectToVisible(const LayoutRect& absoluteRect, bool insideFixed, const ScrollRectToVisibleOptions& options)
{
    if (options.revealMode == SelectionRevealMode::DoNotReveal)
        return false;

    RenderLayer* enclosingLayer = this->enclosingLayer();
    if (!enclosingLayer)
        return false;

    enclosingLayer->scrollRectToVisible(absoluteRect, insideFixed, options);
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

RenderBox* RenderObject::enclosingScrollableContainerForSnapping() const
{
    // Walk up the container chain to find the scrollable container that contains
    // this RenderObject. The important thing here is that `container()` respects
    // the containing block chain for positioned elements. This is important because
    // scrollable overflow does not establish a new containing block for children.
    for (auto* candidate = container(); candidate; candidate = candidate->container()) {
        // Currently the RenderView can look like it has scrollable overflow, but we never
        // want to return this as our container. Instead we should use the root element.
        if (candidate->isRenderView())
            break;
        if (candidate->hasPotentiallyScrollableOverflow())
            return downcast<RenderBox>(candidate);
    }

    // If we reach the root, then the root element is the scrolling container.
    return document().documentElement() ? document().documentElement()->renderBox() : nullptr;
}

RenderBlock* RenderObject::firstLineBlock() const
{
    return nullptr;
}

static inline bool objectIsRelayoutBoundary(const RenderElement* object)
{
    // FIXME: In future it may be possible to broaden these conditions in order to improve performance.
    if (object->isRenderView())
        return true;

    if (object->isTextControl())
        return true;

    if (shouldApplyLayoutContainment(*object))
        return true;

    if (object->isSVGRoot())
        return true;

    if (!object->hasNonVisibleOverflow())
        return false;

    if (object->style().width().isIntrinsicOrAuto() || object->style().height().isIntrinsicOrAuto() || object->style().height().isPercentOrCalculated())
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
    if (is<RenderElement>(*this))
        downcast<RenderElement>(*this).setAncestorLineBoxDirty(false);
#if ASSERT_ENABLED
    checkBlockPositionedObjectsNeedLayout();
#endif
}

static void scheduleRelayoutForSubtree(RenderElement& renderer)
{
    if (is<RenderView>(renderer)) {
        downcast<RenderView>(renderer).frameView().layoutContext().scheduleLayout();
        return;
    }

    if (renderer.isRooted())
        renderer.view().frameView().layoutContext().scheduleSubtreeLayout(renderer);
}

void RenderObject::markContainingBlocksForLayout(ScheduleRelayout scheduleRelayout, RenderElement* newRoot)
{
    ASSERT(scheduleRelayout == ScheduleRelayout::No || !newRoot);
    ASSERT(!isSetNeedsLayoutForbidden());

    auto ancestor = container();

    bool simplifiedNormalFlowLayout = needsSimplifiedNormalFlowLayout() && !selfNeedsLayout() && !normalChildNeedsLayout();
    bool hasOutOfFlowPosition = !isText() && style().hasOutOfFlowPosition();

    while (ancestor) {
        // FIXME: Remove this once we remove the special cases for counters, quotes and mathml calling setNeedsLayout during preferred width computation.
        SetLayoutNeededForbiddenScope layoutForbiddenScope(*ancestor, isSetNeedsLayoutForbidden());

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

        if (scheduleRelayout == ScheduleRelayout::Yes && objectIsRelayoutBoundary(ancestor))
            break;

        hasOutOfFlowPosition = ancestor->style().hasOutOfFlowPosition();
        ancestor = container;
    }

    if (scheduleRelayout == ScheduleRelayout::Yes && ancestor)
        scheduleRelayoutForSubtree(*ancestor);
}

#if ASSERT_ENABLED
void RenderObject::checkBlockPositionedObjectsNeedLayout()
{
    ASSERT(!needsLayout());

    if (is<RenderBlock>(*this))
        downcast<RenderBlock>(*this).checkPositionedObjectsNeedLayout();
}
#endif // ASSERT_ENABLED

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
    downcast<RenderLayerModelObject>(*this).layer()->setRepaintStatus(NeedsFullRepaint);
}

void RenderObject::setLayerNeedsFullRepaintForPositionedMovementLayout()
{
    ASSERT(hasLayer());
    downcast<RenderLayerModelObject>(*this).layer()->setRepaintStatus(NeedsFullRepaintForPositionedMovementLayout);
}

RenderBlock* RenderObject::containingBlock() const
{
    auto containingBlockForRenderer = [](const RenderElement& renderer)
    {
        if (renderer.isAbsolutelyPositioned())
            return renderer.containingBlockForAbsolutePosition();
        if (renderer.isFixedPositioned())
            return renderer.containingBlockForFixedPosition();
        return renderer.containingBlockForObjectInFlow();
    };

    if (is<RenderText>(*this))
        return containingBlockForObjectInFlow();

    if (!parent() && is<RenderScrollbarPart>(*this)) {
        if (auto* scrollbarPart = downcast<RenderScrollbarPart>(*this).rendererOwningScrollbar())
            return containingBlockForRenderer(*scrollbarPart);
        return nullptr;
    }
    return containingBlockForRenderer(downcast<RenderElement>(*this));
}

RenderBlock* RenderObject::containingBlockForObjectInFlow() const
{
    auto* renderer = parent();
    while (renderer && ((renderer->isInline() && !renderer->isReplaced()) || !renderer->isRenderBlock()))
        renderer = renderer->parent();
    return downcast<RenderBlock>(renderer);
}

void RenderObject::addPDFURLRect(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    Vector<LayoutRect> focusRingRects;
    addFocusRingRects(focusRingRects, paintOffset, paintInfo.paintContainer);
    LayoutRect urlRect = unionRect(focusRingRects);

    if (urlRect.isEmpty())
        return;
    Node* node = this->node();
    if (!is<Element>(node) || !node->isLink())
        return;
    Element& element = downcast<Element>(*node);
    const AtomString& href = element.getAttribute(hrefAttr);
    if (href.isNull())
        return;

    if (paintInfo.context().supportsInternalLinks()) {
        String outAnchorName;
        Element* linkTarget = element.findAnchorElementForLink(outAnchorName);
        if (linkTarget) {
            paintInfo.context().setDestinationForRect(outAnchorName, urlRect);
            return;
        }
    }

    paintInfo.context().setURLForRect(element.document().completeURL(href), urlRect);

}

#if PLATFORM(IOS_FAMILY)
// This function is similar in spirit to RenderText::absoluteRectsForRange, but returns rectangles
// which are annotated with additional state which helps iOS draw selections in its unique way.
// No annotations are added in this class.
// FIXME: Move to RenderText with absoluteRectsForRange()?
void RenderObject::collectSelectionGeometries(Vector<SelectionGeometry>& geometries, unsigned start, unsigned end)
{
    Vector<FloatQuad> quads;

    if (!firstChildSlow()) {
        // FIXME: WebKit's position for an empty span after a BR is incorrect, so we can't trust 
        // quads for them. We don't need selection geometries for those anyway though, since they 
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

    for (auto& quad : quads)
        geometries.append(SelectionGeometry(quad, HTMLElement::selectionRenderingBehavior(node()), isHorizontalWritingMode(), view().pageNumberForBlockProgressionOffset(quad.enclosingBoundingBox().x())));
}
#endif

IntRect RenderObject::absoluteBoundingBoxRect(bool useTransforms, bool* wasFixed) const
{
    if (useTransforms) {
        Vector<FloatQuad> quads;
        absoluteQuads(quads, wasFixed);
        return enclosingIntRect(unitedBoundingBoxes(quads));
    }

    FloatPoint absPos = localToAbsolute(FloatPoint(), { } /* ignore transforms */, wasFixed);
    Vector<IntRect> rects;
    absoluteRects(rects, flooredLayoutPoint(absPos));

    size_t n = rects.size();
    if (!n)
        return IntRect();

    LayoutRect result = rects[0];
    for (size_t i = 1; i < n; ++i)
        result.unite(rects[i]);
    return snappedIntRect(result);
}

void RenderObject::absoluteFocusRingQuads(Vector<FloatQuad>& quads)
{
    Vector<LayoutRect> rects;
    // FIXME: addFocusRingRects() needs to be passed this transform-unaware
    // localToAbsolute() offset here because RenderInline::addFocusRingRects()
    // implicitly assumes that. This doesn't work correctly with transformed
    // descendants.
    FloatPoint absolutePoint = localToAbsolute();
    addFocusRingRects(rects, flooredLayoutPoint(absolutePoint));
    float deviceScaleFactor = document().deviceScaleFactor();
    for (auto rect : rects) {
        rect.moveBy(LayoutPoint(-absolutePoint));
        quads.append(localToAbsoluteQuad(FloatQuad(snapRectToDevicePixels(rect, deviceScaleFactor))));
    }
}

void RenderObject::addAbsoluteRectForLayer(LayoutRect& result)
{
    if (hasLayer())
        result.unite(absoluteBoundingBoxRectIgnoringTransforms());

    if (!is<RenderElement>(*this))
        return;

    for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(*this)))
        child.addAbsoluteRectForLayer(result);
}

// FIXME: change this to use the subtreePaint terminology
LayoutRect RenderObject::paintingRootRect(LayoutRect& topLevelRect)
{
    LayoutRect result = absoluteBoundingBoxRectIgnoringTransforms();
    topLevelRect = result;
    if (is<RenderElement>(*this)) {
        for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(*this)))
            child.addAbsoluteRectForLayer(result);
    }
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
    if (view().hasSoftwareFilters()) {
        if (RenderLayer* parentLayer = enclosingLayer()) {
            RenderLayer* enclosingFilterLayer = parentLayer->enclosingFilterLayer();
            if (enclosingFilterLayer)
                return &enclosingFilterLayer->renderer();
        }
    }

    // If we have a flow thread, then we need to do individual repaints within the RenderFragmentContainers instead.
    // Return the flow thread as a repaint container in order to create a chokepoint that allows us to change
    // repainting to do individual region repaints.
    RenderFragmentedFlow* parentRenderFragmentedFlow = enclosingFragmentedFlow();
    if (parentRenderFragmentedFlow) {
        // If we have already found a repaint container then we will repaint into that container only if it is part of the same
        // flow thread. Otherwise we will need to catch the repaint call and send it to the flow thread.
        RenderFragmentedFlow* repaintContainerFragmentedFlow = repaintContainer ? repaintContainer->enclosingFragmentedFlow() : nullptr;
        if (!repaintContainerFragmentedFlow || repaintContainerFragmentedFlow != parentRenderFragmentedFlow)
            repaintContainer = parentRenderFragmentedFlow;
    }
    return repaintContainer;
}

void RenderObject::propagateRepaintToParentWithOutlineAutoIfNeeded(const RenderLayerModelObject& repaintContainer, const LayoutRect& repaintRect) const
{
    if (!hasOutlineAutoAncestor())
        return;

    // FIXME: We should really propagate only when the child renderer sticks out.
    bool repaintRectNeedsConverting = false;
    // Issue repaint on the renderer with outline: auto.
    for (const auto* renderer = this; renderer; renderer = renderer->parent()) {
        bool rendererHasOutlineAutoAncestor = renderer->hasOutlineAutoAncestor();
        ASSERT(rendererHasOutlineAutoAncestor
            || renderer->outlineStyleForRepaint().outlineStyleIsAuto() == OutlineIsAuto::On
            || (is<RenderBoxModelObject>(*renderer) && downcast<RenderBoxModelObject>(*renderer).isContinuation()));
        if (renderer == &repaintContainer && rendererHasOutlineAutoAncestor)
            repaintRectNeedsConverting = true;
        if (rendererHasOutlineAutoAncestor)
            continue;
        // Issue repaint on the correct repaint container.
        LayoutRect adjustedRepaintRect = repaintRect;
        adjustedRepaintRect.inflate(renderer->outlineStyleForRepaint().outlineSize());
        if (!repaintRectNeedsConverting)
            repaintContainer.repaintRectangle(adjustedRepaintRect);
        else if (is<RenderLayerModelObject>(renderer)) {
            const auto& rendererWithOutline = downcast<RenderLayerModelObject>(*renderer);
            adjustedRepaintRect = LayoutRect(repaintContainer.localToContainerQuad(FloatRect(adjustedRepaintRect), &rendererWithOutline).boundingBox());
            rendererWithOutline.repaintRectangle(adjustedRepaintRect);
        }
        return;
    }
    ASSERT_NOT_REACHED();
}

void RenderObject::repaintUsingContainer(const RenderLayerModelObject* repaintContainer, const LayoutRect& r, bool shouldClipToLayer) const
{
    if (r.isEmpty())
        return;

    if (!repaintContainer)
        repaintContainer = &view();

    if (is<RenderFragmentedFlow>(*repaintContainer)) {
        downcast<RenderFragmentedFlow>(*repaintContainer).repaintRectangleInFragments(r);
        return;
    }

    propagateRepaintToParentWithOutlineAutoIfNeeded(*repaintContainer, r);

    if (repaintContainer->hasFilter() && repaintContainer->layer() && repaintContainer->layer()->requiresFullLayerImageForFilters()) {
        repaintContainer->layer()->setFilterBackendNeedsRepaintingInRect(r);
        return;
    }

    if (repaintContainer->isRenderView()) {
        RenderView& view = this->view();
        ASSERT(repaintContainer == &view);
        bool viewHasCompositedLayer = view.isComposited();
        if (!viewHasCompositedLayer || view.layer()->backing()->paintsIntoWindow()) {
            LayoutRect rect = r;
            if (viewHasCompositedLayer && view.layer()->transform())
                rect = LayoutRect(view.layer()->transform()->mapRect(snapRectToDevicePixels(rect, document().deviceScaleFactor())));
            view.repaintViewRectangle(rect);
            return;
        }
    }

    if (view().usesCompositing()) {
        ASSERT(repaintContainer->isComposited());
        repaintContainer->layer()->setBackingNeedsRepaintInRect(r, shouldClipToLayer ? GraphicsLayer::ClipToLayer : GraphicsLayer::DoNotClipToLayer);
    }
}

void RenderObject::repaint() const
{
    // Don't repaint if we're unrooted (note that view() still returns the view when unrooted)
    if (!isRooted())
        return;

    const RenderView& view = this->view();
    if (view.printing())
        return;

    RenderLayerModelObject* repaintContainer = containerForRepaint();
    repaintUsingContainer(repaintContainer, clippedOverflowRectForRepaint(repaintContainer));
}

void RenderObject::repaintRectangle(const LayoutRect& r, bool shouldClipToLayer) const
{
    // Don't repaint if we're unrooted (note that view() still returns the view when unrooted)
    if (!isRooted())
        return;

    const RenderView& view = this->view();
    if (view.printing())
        return;

    LayoutRect dirtyRect(r);
    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    dirtyRect.move(view.frameView().layoutContext().layoutDelta());

    RenderLayerModelObject* repaintContainer = containerForRepaint();
    repaintUsingContainer(repaintContainer, computeRectForRepaint(dirtyRect, repaintContainer), shouldClipToLayer);
}

void RenderObject::repaintSlowRepaintObject() const
{
    // Don't repaint if we're unrooted (note that view() still returns the view when unrooted)
    if (!isRooted())
        return;

    const RenderView& view = this->view();
    if (view.printing())
        return;

    const RenderLayerModelObject* repaintContainer = containerForRepaint();

    bool shouldClipToLayer = true;
    IntRect repaintRect;
    // If this is the root background, we need to check if there is an extended background rect. If
    // there is, then we should not allow painting to clip to the layer size.
    if (isDocumentElementRenderer() || isBody()) {
        shouldClipToLayer = !view.frameView().hasExtendedBackgroundRectForPainting();
        repaintRect = snappedIntRect(view.backgroundRect());
    } else
        repaintRect = snappedIntRect(clippedOverflowRectForRepaint(repaintContainer));

    repaintUsingContainer(repaintContainer, repaintRect, shouldClipToLayer);
}

IntRect RenderObject::pixelSnappedAbsoluteClippedOverflowRect() const
{
    return snappedIntRect(absoluteClippedOverflowRectForRepaint());
}
    
LayoutRect RenderObject::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(clippedOverflowRectForRepaint(repaintContainer));
    r.inflate(outlineWidth);
    return r;
}

LayoutRect RenderObject::clippedOverflowRect(const RenderLayerModelObject*, VisibleRectContext) const
{
    ASSERT_NOT_REACHED();
    return LayoutRect();
}

LayoutRect RenderObject::computeRect(const LayoutRect& rect, const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    return *computeVisibleRectInContainer(rect, repaintContainer, context);
}

FloatRect RenderObject::computeFloatRectForRepaint(const FloatRect& rect, const RenderLayerModelObject* repaintContainer) const
{
    return *computeFloatVisibleRectInContainer(rect, repaintContainer, visibleRectContextForRepaint());
}

std::optional<LayoutRect> RenderObject::computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    if (container == this)
        return rect;

    auto* parent = this->parent();
    if (!parent)
        return rect;

    LayoutRect adjustedRect = rect;
    if (parent->hasNonVisibleOverflow()) {
        bool isEmpty = !downcast<RenderBox>(*parent).applyCachedClipAndScrollPosition(adjustedRect, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRect;
        }
    }
    return parent->computeVisibleRectInContainer(adjustedRect, container, context);
}

std::optional<FloatRect> RenderObject::computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject*, VisibleRectContext) const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

#if ENABLE(TREE_DEBUGGING)

static void outputRenderTreeLegend(TextStream& stream)
{
    stream.nextLine();
    stream << "(B)lock/(I)nline/I(N)line-block, (A)bsolute/Fi(X)ed/(R)elative/Stic(K)y, (F)loating, (O)verflow clip, Anon(Y)mous, (G)enerated, has(L)ayer, hasLayer(S)crollableArea, (C)omposited, (+)Dirty style, (+)Dirty layout";
    stream.nextLine();
}

void RenderObject::showNodeTreeForThis() const
{
    if (!node())
        return;
    node()->showTreeForThis();
}

void RenderObject::showRenderTreeForThis() const
{
    const WebCore::RenderObject* root = this;
    while (root->parent())
        root = root->parent();
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputRenderTreeLegend(stream);
    root->outputRenderSubTreeAndMark(stream, this, 1);
    WTFLogAlways("%s", stream.release().utf8().data());
}

void RenderObject::showLineTreeForThis() const
{
    if (!is<RenderBlockFlow>(*this))
        return;
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputRenderTreeLegend(stream);
    outputRenderObject(stream, false, 1);
    downcast<RenderBlockFlow>(*this).outputLineTreeAndMark(stream, nullptr, 2);
    WTFLogAlways("%s", stream.release().utf8().data());
}

static const RenderFragmentedFlow* enclosingFragmentedFlowFromRenderer(const RenderObject* renderer)
{
    if (!renderer)
        return nullptr;

    if (renderer->fragmentedFlowState() == RenderObject::NotInsideFragmentedFlow)
        return nullptr;

    if (is<RenderBlock>(*renderer))
        return downcast<RenderBlock>(*renderer).cachedEnclosingFragmentedFlow();

    return nullptr;
}

void RenderObject::outputRegionsInformation(TextStream& stream) const
{
    if (is<RenderFragmentedFlow>(*this)) {
        const auto& fragmentedFlow = downcast<RenderFragmentedFlow>(*this);
        auto fragmentContainers = fragmentedFlow.renderFragmentContainerList();

        stream << " [fragment containers ";
        bool first = true;
        for (const auto* fragment : fragmentContainers) {
            if (!first)
                stream << ", ";
            first = false;
            stream << fragment;
        }
        stream << "]";
    }

    const RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlowFromRenderer(this);

    if (!fragmentedFlow) {
        // Only the boxes have region range information.
        // Try to get the flow thread containing block information
        // from the containing block of this box.
        if (is<RenderBox>(*this))
            fragmentedFlow = enclosingFragmentedFlowFromRenderer(containingBlock());
    }

    if (!fragmentedFlow || !is<RenderBox>(*this))
        return;

    RenderFragmentContainer* startContainer = nullptr;
    RenderFragmentContainer* endContainer = nullptr;
    fragmentedFlow->getFragmentRangeForBox(downcast<RenderBox>(this), startContainer, endContainer);
    stream << " [spans fragment containers in flow " << fragmentedFlow << " from " << startContainer << " to " << endContainer << "]";
}

void RenderObject::outputRenderObject(TextStream& stream, bool mark, int depth) const
{
    if (isInlineBlockOrInlineTable())
        stream << "N";
    else if (isInline())
        stream << "I";
    else
        stream << "B";

    if (isPositioned()) {
        if (isRelativelyPositioned())
            stream << "R";
        else if (isStickilyPositioned())
            stream << "K";
        else if (isOutOfFlowPositioned()) {
            if (isAbsolutelyPositioned())
                stream << "A";
            else
                stream << "X";
        }
    } else
        stream << "-";

    if (isFloating())
        stream << "F";
    else
        stream << "-";

    if (hasNonVisibleOverflow())
        stream << "O";
    else
        stream << "-";

    if (isAnonymous())
        stream << "Y";
    else
        stream << "-";

    if (isPseudoElement() || isAnonymous())
        stream << "G";
    else
        stream << "-";

    if (hasLayer()) {
        stream << "L";
        if (downcast<RenderLayerModelObject>(*this).layer()->scrollableArea())
            stream << "S";
        else
            stream << "-";
    } else
        stream << "--";

    if (isComposited())
        stream << "C";
    else
        stream << "-";

    stream << " ";

    if (node() && node()->needsStyleRecalc())
        stream << "+";
    else
        stream << "-";

    if (needsLayout())
        stream << "+";
    else
        stream << "-";

    int printedCharacters = 0;
    if (mark) {
        stream << "*";
        ++printedCharacters;
    }

    while (++printedCharacters <= depth * 2)
        stream << " ";

    if (node())
        stream << node()->nodeName().utf8().data() << " ";

    String name = renderName();
    // FIXME: Renderer's name should not include property value listing.
    int pos = name.find('(');
    if (pos > 0)
        stream << name.left(pos - 1).utf8().data();
    else
        stream << name.utf8().data();

    if (is<RenderBox>(*this)) {
        auto& renderBox = downcast<RenderBox>(*this);
        FloatRect boxRect = renderBox.frameRect();
        if (renderBox.isInFlowPositioned())
            boxRect.move(renderBox.offsetForInFlowPosition());
        stream << " " << boxRect;
    } else if (is<RenderInline>(*this) && isInFlowPositioned()) {
        FloatSize inlineOffset = downcast<RenderInline>(*this).offsetForInFlowPosition();
        stream << "  (" << inlineOffset.width() << ", " << inlineOffset.height() << ")";
    }

    stream << " renderer->(" << this << ")";
    if (node()) {
        stream << " node->(" << node() << ")";
        if (node()->isTextNode()) {
            String value = node()->nodeValue();
            stream << " length->(" << value.length() << ")";

            value.replaceWithLiteral('\\', "\\\\");
            value.replaceWithLiteral('\n', "\\n");
            
            const int maxPrintedLength = 80;
            if (value.length() > maxPrintedLength) {
                String substring = value.substring(0, maxPrintedLength);
                stream << " \"" << substring.utf8().data() << "\"...";
            } else
                stream << " \"" << value.utf8().data() << "\"";
        }
    }
    if (is<RenderBoxModelObject>(*this)) {
        auto& renderer = downcast<RenderBoxModelObject>(*this);
        if (renderer.continuation())
            stream << " continuation->(" << renderer.continuation() << ")";
    }

    if (is<RenderBox>(*this)) {
        const auto& box = downcast<RenderBox>(*this);
        if (box.hasRenderOverflow()) {
            auto layoutOverflow = box.layoutOverflowRect();
            stream << " (layout overflow " << layoutOverflow.x() << "," << layoutOverflow.y() << " " << layoutOverflow.width() << "x" << layoutOverflow.height() << ")";
            
            if (box.hasVisualOverflow()) {
                auto visualOverflow = box.visualOverflowRect();
                stream << " (visual overflow " << visualOverflow.x() << "," << visualOverflow.y() << " " << visualOverflow.width() << "x" << visualOverflow.height() << ")";
            }
        }
    }

    if (is<RenderMultiColumnSet>(*this)) {
        const auto& multicolSet = downcast<RenderMultiColumnSet>(*this);
        stream << " (column count " << multicolSet.computedColumnCount() << ", size " << multicolSet.computedColumnWidth() << "x" << multicolSet.computedColumnHeight() << ", gap " << multicolSet.columnGap() << ")";
    }

    outputRegionsInformation(stream);

    if (needsLayout()) {
        stream << " layout->";
        if (selfNeedsLayout())
            stream << "[self]";
        if (normalChildNeedsLayout())
            stream << "[normal child]";
        if (posChildNeedsLayout())
            stream << "[positioned child]";
        if (needsSimplifiedNormalFlowLayout())
            stream << "[simplified]";
        if (needsPositionedMovementLayout())
            stream << "[positioned movement]";
    }
    stream.nextLine();
}

void RenderObject::outputRenderSubTreeAndMark(TextStream& stream, const RenderObject* markedObject, int depth) const
{
    outputRenderObject(stream, markedObject == this, depth);

    if (is<RenderBlockFlow>(*this))
        downcast<RenderBlockFlow>(*this).outputFloatingObjects(stream, depth + 1);

    if (is<RenderBlockFlow>(*this))
        downcast<RenderBlockFlow>(*this).outputLineTreeAndMark(stream, nullptr, depth + 1);

    for (auto* child = firstChildSlow(); child; child = child->nextSibling())
        child->outputRenderSubTreeAndMark(stream, markedObject, depth + 1);
}

#endif // NDEBUG

FloatPoint RenderObject::localToAbsolute(const FloatPoint& localPoint, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(nullptr, transformState, mode | ApplyContainerFlip, wasFixed);
    transformState.flatten();
    
    return transformState.lastPlanarPoint();
}

FloatPoint RenderObject::absoluteToLocal(const FloatPoint& containerPoint, OptionSet<MapCoordinatesMode> mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, containerPoint);
    mapAbsoluteToLocalPoint(mode, transformState);
    transformState.flatten();
    
    return transformState.lastPlanarPoint();
}

FloatQuad RenderObject::absoluteToLocalQuad(const FloatQuad& quad, OptionSet<MapCoordinatesMode> mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, quad.boundingBox().center(), quad);
    mapAbsoluteToLocalPoint(mode, transformState);
    transformState.flatten();
    return transformState.lastPlanarQuad();
}

void RenderObject::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (ancestorContainer == this)
        return;

    auto* parent = this->parent();
    if (!parent)
        return;

    // FIXME: this should call offsetFromContainer to share code, but I'm not sure it's ever called.
    LayoutPoint centerPoint(transformState.mappedPoint());
    if (mode.contains(ApplyContainerFlip) && is<RenderBox>(*parent)) {
        if (parent->style().isFlippedBlocksWritingMode())
            transformState.move(downcast<RenderBox>(parent)->flipForWritingMode(LayoutPoint(transformState.mappedPoint())) - centerPoint);
        mode.remove(ApplyContainerFlip);
    }

    if (is<RenderBox>(*parent))
        transformState.move(-toLayoutSize(downcast<RenderBox>(*parent).scrollPosition()));

    parent->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

const RenderObject* RenderObject::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT_UNUSED(ancestorToStopAt, ancestorToStopAt != this);

    auto* container = parent();
    if (!container)
        return nullptr;

    // FIXME: this should call offsetFromContainer to share code, but I'm not sure it's ever called.
    LayoutSize offset;
    if (is<RenderBox>(*container))
        offset = -toLayoutSize(downcast<RenderBox>(*container).scrollPosition());

    geometryMap.push(this, offset, false);
    
    return container;
}

void RenderObject::mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode> mode, TransformState& transformState) const
{
    if (auto* parent = this->parent()) {
        parent->mapAbsoluteToLocalPoint(mode, transformState);
        if (is<RenderBox>(*parent))
            transformState.move(toLayoutSize(downcast<RenderBox>(*parent).scrollPosition()));
    }
}

bool RenderObject::shouldUseTransformFromContainer(const RenderObject* containerObject) const
{
#if ENABLE(3D_TRANSFORMS)
    return hasTransform() || (containerObject && containerObject->style().hasPerspective());
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
    if (hasLayer() && (layer = downcast<RenderLayerModelObject>(*this).layer()) && layer->transform())
        transform.multiply(layer->currentTransform());
    
#if ENABLE(3D_TRANSFORMS)
    if (containerObject && containerObject->hasLayer() && containerObject->style().hasPerspective()) {
        // Perpsective on the container affects us, so we have to factor it in here.
        ASSERT(containerObject->hasLayer());
        FloatPoint perspectiveOrigin = downcast<RenderLayerModelObject>(*containerObject).layer()->perspectiveOrigin();

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

FloatQuad RenderObject::localToContainerQuad(const FloatQuad& localQuad, const RenderLayerModelObject* container, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    // Track the point at the center of the quad's bounding box. As mapLocalToContainer() calls offsetFromContainer(),
    // it will use that point as the reference point to decide which column's transform to apply in multiple-column blocks.
    TransformState transformState(TransformState::ApplyTransformDirection, localQuad.boundingBox().center(), localQuad);
    mapLocalToContainer(container, transformState, mode | ApplyContainerFlip, wasFixed);
    transformState.flatten();
    
    return transformState.lastPlanarQuad();
}

FloatPoint RenderObject::localToContainerPoint(const FloatPoint& localPoint, const RenderLayerModelObject* container, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(container, transformState, mode | ApplyContainerFlip, wasFixed);
    transformState.flatten();

    return transformState.lastPlanarPoint();
}

LayoutSize RenderObject::offsetFromContainer(RenderElement& container, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    ASSERT(&container == this->container());

    LayoutSize offset;
    if (is<RenderBox>(container))
        offset -= toLayoutSize(downcast<RenderBox>(container).scrollPosition());

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = is<RenderFragmentedFlow>(container);

    return offset;
}

LayoutSize RenderObject::offsetFromAncestorContainer(RenderElement& container) const
{
    LayoutSize offset;
    LayoutPoint referencePoint;
    const RenderObject* currContainer = this;
    do {
        RenderElement* nextContainer = currContainer->container();
        ASSERT(nextContainer);  // This means we reached the top without finding container.
        if (!nextContainer)
            break;
        ASSERT(!currContainer->hasTransform());
        LayoutSize currentOffset = currContainer->offsetFromContainer(*nextContainer, referencePoint);
        offset += currentOffset;
        referencePoint.move(currentOffset);
        currContainer = nextContainer;
    } while (currContainer != &container);

    return offset;
}

bool RenderObject::isRooted() const
{
    return isDescendantOf(&view());
}

static inline RenderElement* containerForElement(const RenderObject& renderer, const RenderLayerModelObject* repaintContainer, bool* repaintContainerSkipped)
{
    // This method is extremely similar to containingBlock(), but with a few notable
    // exceptions.
    // (1) For normal flow elements, it just returns the parent.
    // (2) For absolute positioned elements, it will return a relative positioned inline, while
    // containingBlock() skips to the non-anonymous containing block.
    // This does mean that computePositionedLogicalWidth and computePositionedLogicalHeight have to use container().
    auto pos = renderer.style().position();
    auto* parent = renderer.parent();
    if (is<RenderText>(renderer) || (pos != PositionType::Fixed && pos != PositionType::Absolute))
        return parent;
    for (; parent && (pos == PositionType::Absolute ? !parent->canContainAbsolutelyPositionedObjects() : !parent->canContainFixedPositionObjects()); parent = parent->parent()) {
        if (repaintContainerSkipped && repaintContainer == parent)
            *repaintContainerSkipped = true;
    }
    return parent;
}

RenderElement* RenderObject::container() const
{
    return containerForElement(*this, nullptr, nullptr);
}

RenderElement* RenderObject::container(const RenderLayerModelObject* repaintContainer, bool& repaintContainerSkipped) const
{
    repaintContainerSkipped = false;
    return containerForElement(*this, repaintContainer, &repaintContainerSkipped);
}

bool RenderObject::isSelectionBorder() const
{
    HighlightState st = selectionState();
    return st == HighlightState::Start
        || st == HighlightState::End
        || st == HighlightState::Both
        || view().selection().start() == this
        || view().selection().end() == this;
}

void RenderObject::willBeDestroyed()
{
    ASSERT(!m_parent);
    ASSERT(renderTreeBeingDestroyed() || !is<RenderElement>(*this) || !view().frameView().hasSlowRepaintObject(downcast<RenderElement>(*this)));

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->remove(this);

    if (auto* node = this->node()) {
        // FIXME: Continuations should be anonymous.
        ASSERT(!node->renderer() || node->renderer() == this || (is<RenderElement>(*this) && downcast<RenderElement>(*this).isContinuation()));
        if (node->renderer() == this)
            node->setRenderer(nullptr);
    }

    removeRareData();
}

void RenderObject::insertedIntoTree(IsInternalMove)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* container = LayoutIntegration::LineLayout::blockContainer(*this))
        container->invalidateLineLayoutPath();
#endif

    // FIXME: We should ASSERT(isRooted()) here but generated content makes some out-of-order insertion.
    if (!isFloating() && parent()->childrenInline())
        parent()->dirtyLinesFromChangedChild(*this);
}

void RenderObject::willBeRemovedFromTree(IsInternalMove)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* container = LayoutIntegration::LineLayout::blockContainer(*this))
        container->invalidateLineLayoutPath();
#endif

    // FIXME: We should ASSERT(isRooted()) but we have some out-of-order removals which would need to be fixed first.
    // Update cached boundaries in SVG renderers, if a child is removed.
    parent()->setNeedsBoundariesUpdate();
}

void RenderObject::destroy()
{
    RELEASE_ASSERT(!m_parent);
    RELEASE_ASSERT(!m_next);
    RELEASE_ASSERT(!m_previous);
    RELEASE_ASSERT(!m_bitfields.beingDestroyed());

    m_bitfields.setBeingDestroyed(true);

#if PLATFORM(IOS_FAMILY)
    if (hasLayer())
        downcast<RenderBoxModelObject>(*this).layer()->willBeDestroyed();
#endif

    willBeDestroyed();

    if (is<RenderWidget>(*this)) {
        downcast<RenderWidget>(*this).deref();
        return;
    }
    delete this;
}

Position RenderObject::positionForPoint(const LayoutPoint& point)
{
    // FIXME: This should just create a Position object instead (webkit.org/b/168566). 
    return positionForPoint(point, nullptr).deepEquivalent();
}

VisiblePosition RenderObject::positionForPoint(const LayoutPoint&, const RenderFragmentContainer*)
{
    return createVisiblePosition(caretMinOffset(), Affinity::Downstream);
}

bool RenderObject::isComposited() const
{
    return hasLayer() && downcast<RenderLayerModelObject>(*this).layer()->isComposited();
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

Node* RenderObject::nodeForHitTest() const
{
    auto* node = this->node();
    // If we hit the anonymous renderers inside generated content we should
    // actually hit the generated content so walk up to the PseudoElement.
    if (!node && parent() && parent()->isBeforeOrAfterContent()) {
        for (auto* renderer = parent(); renderer && !node; renderer = renderer->parent())
            node = renderer->element();
    }
    return node;
}

void RenderObject::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    if (auto* node = nodeForHitTest()) {
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

int RenderObject::caretMinOffset() const
{
    return 0;
}

int RenderObject::caretMaxOffset() const
{
    if (isReplaced())
        return node() ? std::max(1U, node()->countChildNodes()) : 1;
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
    LayoutUnit outlineSize { outlineStyleForRepaint().outlineSize() };
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
    if (isDocumentElementRenderer() || isBody() || isFixedPositioned())
        return nullptr;

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
    auto current = parent();
    while (current && (!current->element() || (!current->isPositioned() && !current->isBody()))) {
        Element* element = current->element();
        if (!skipTables && element && (is<HTMLTableElement>(*element) || is<HTMLTableCellElement>(*element)))
            break;
 
        float newZoom = current->style().effectiveZoom();
        if (currZoom != newZoom)
            break;
        currZoom = newZoom;
        current = current->parent();
    }

    return is<RenderBoxModelObject>(current) ? downcast<RenderBoxModelObject>(current) : nullptr;
}

VisiblePosition RenderObject::createVisiblePosition(int offset, Affinity affinity) const
{
    // If this is a non-anonymous renderer in an editable area, then it's simple.
    if (Node* node = nonPseudoNode()) {
        if (!node->hasEditableStyle()) {
            // If it can be found, we prefer a visually equivalent position that is editable. 
            Position position = makeDeprecatedLegacyPosition(node, offset);
            Position candidate = position.downstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->hasEditableStyle())
                return VisiblePosition(candidate, affinity);
            candidate = position.upstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->hasEditableStyle())
                return VisiblePosition(candidate, affinity);
        }
        // FIXME: Eliminate legacy editing positions
        return VisiblePosition(makeDeprecatedLegacyPosition(node, offset), affinity);
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
                return firstPositionInOrBeforeNode(node);
        }

        // Find non-anonymous content before.
        renderer = child;
        while ((renderer = renderer->previousInPreOrder())) {
            if (renderer == parent)
                break;
            if (Node* node = renderer->nonPseudoNode())
                return lastPositionInOrAfterNode(node);
        }

        // Use the parent itself unless it too is anonymous.
        if (Element* element = parent->nonPseudoElement())
            return firstPositionInOrBeforeNode(element);

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
    return createVisiblePosition(0, Affinity::Downstream);
}

CursorDirective RenderObject::getCursor(const LayoutPoint&, Cursor&) const
{
    return SetCursorBasedOnStyle;
}

bool RenderObject::useDarkAppearance() const
{
    return document().useDarkAppearance(&style());
}

OptionSet<StyleColor::Options> RenderObject::styleColorOptions() const
{
    return document().styleColorOptions(&style());
}

void RenderObject::setSelectionState(HighlightState state)
{
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (state != HighlightState::None) {
        if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
            lineLayout->flow().ensureLineBoxes();
    }
#endif

    m_bitfields.setSelectionState(state);
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
    return downcast<PseudoElement>(*node()).hostElement();
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

RenderFragmentedFlow* RenderObject::locateEnclosingFragmentedFlow() const
{
    RenderBlock* containingBlock = this->containingBlock();
    return containingBlock ? containingBlock->enclosingFragmentedFlow() : nullptr;
}

void RenderObject::calculateBorderStyleColor(const BorderStyle& style, const BoxSide& side, Color& color)
{
    ASSERT(style == BorderStyle::Inset || style == BorderStyle::Outset);

    // This values were derived empirically.
    constexpr float baseDarkColorLuminance { 0.014443844f }; // Luminance of SRGBA<uint8_t> { 32, 32, 32 }
    constexpr float baseLightColorLuminance { 0.83077f }; // Luminance of SRGBA<uint8_t> { 235, 235, 235 }

    enum Operation { Darken, Lighten };

    Operation operation = (side == BoxSide::Top || side == BoxSide::Left) == (style == BorderStyle::Inset) ? Darken : Lighten;

    // Here we will darken the border decoration color when needed. This will yield a similar behavior as in FF.
    if (operation == Darken) {
        if (color.luminance() > baseDarkColorLuminance)
            color = color.darkened();
    } else {
        if (color.luminance() < baseLightColorLuminance)
            color = color.lightened();
    }
}

void RenderObject::setHasReflection(bool hasReflection)
{
    if (hasReflection || hasRareData())
        ensureRareData().setHasReflection(hasReflection);
}

void RenderObject::setIsRenderFragmentedFlow(bool isFragmentedFlow)
{
    if (isFragmentedFlow || hasRareData())
        ensureRareData().setIsRenderFragmentedFlow(isFragmentedFlow);
}

void RenderObject::setHasOutlineAutoAncestor(bool hasOutlineAutoAncestor)
{
    if (hasOutlineAutoAncestor || hasRareData())
        ensureRareData().setHasOutlineAutoAncestor(hasOutlineAutoAncestor);
}

RenderObject::RareDataMap& RenderObject::rareDataMap()
{
    static NeverDestroyed<RareDataMap> map;
    return map;
}

const RenderObject::RenderObjectRareData& RenderObject::rareData() const
{
    ASSERT(hasRareData());
    return *rareDataMap().get(this);
}

RenderObject::RenderObjectRareData& RenderObject::ensureRareData()
{
    setHasRareData(true);
    return *rareDataMap().ensure(this, [] { return makeUnique<RenderObjectRareData>(); }).iterator->value;
}

void RenderObject::removeRareData()
{
    rareDataMap().remove(this);
    setHasRareData(false);
}

bool RenderObject::hasNonEmptyVisibleRectRespectingParentFrames() const
{
    auto enclosingFrameRenderer = [] (const RenderObject& renderer) {
        auto* ownerElement = renderer.document().ownerElement();
        return ownerElement ? ownerElement->renderer() : nullptr;
    };

    auto hasEmptyVisibleRect = [] (const RenderObject& renderer) {
        VisibleRectContext context { false, false, { VisibleRectContextOption::UseEdgeInclusiveIntersection, VisibleRectContextOption::ApplyCompositedClips }};
        auto& box = renderer.enclosingBoxModelObject();
        auto clippedBounds = box.computeVisibleRectInContainer(box.borderBoundingBox(), &box.view(), context);
        return !clippedBounds || clippedBounds->isEmpty();
    };

    for (auto* renderer = this; renderer; renderer = enclosingFrameRenderer(*renderer)) {
        if (hasEmptyVisibleRect(*renderer))
            return true;
    }

    return false;
}

Vector<FloatQuad> RenderObject::absoluteTextQuads(const SimpleRange& range, OptionSet<RenderObject::BoundingRectBehavior> behavior)
{
    Vector<FloatQuad> quads;
    for (auto& node : intersectingNodes(range)) {
        auto renderer = node.renderer();
        if (renderer && renderer->isBR())
            downcast<RenderLineBreak>(*renderer).absoluteQuads(quads);
        else if (is<RenderText>(renderer)) {
            auto offsetRange = characterDataOffsetRange(range, downcast<CharacterData>(node));
            quads.appendVector(downcast<RenderText>(*renderer).absoluteQuadsForRange(offsetRange.start, offsetRange.end, behavior.contains(BoundingRectBehavior::UseSelectionHeight)));
        }
    }
    return quads;
}

static Vector<FloatRect> absoluteRectsForRangeInText(const SimpleRange& range, Text& node, OptionSet<RenderObject::BoundingRectBehavior> behavior)
{
    auto renderer = node.renderer();
    if (!renderer)
        return { };

    auto offsetRange = characterDataOffsetRange(range, node);
    auto textQuads = renderer->absoluteQuadsForRange(offsetRange.start, offsetRange.end, behavior.contains(RenderObject::BoundingRectBehavior::UseSelectionHeight), behavior.contains(RenderObject::BoundingRectBehavior::IgnoreEmptyTextSelections));

    if (behavior.contains(RenderObject::BoundingRectBehavior::RespectClipping)) {
        auto absoluteClippedOverflowRect = renderer->absoluteClippedOverflowRectForRepaint();
        Vector<FloatRect> clippedRects;
        clippedRects.reserveInitialCapacity(textQuads.size());
        for (auto& quad : textQuads) {
            auto clippedRect = intersection(quad.boundingBox(), absoluteClippedOverflowRect);
            if (!clippedRect.isEmpty())
                clippedRects.uncheckedAppend(clippedRect);
        }
        return clippedRects;
    }

    return boundingBoxes(textQuads);
}

// FIXME: This should return Vector<FloatRect> like the other similar functions.
// FIXME: Find a way to share with absoluteTextQuads rather than repeating so much of the logic from that function.
Vector<IntRect> RenderObject::absoluteTextRects(const SimpleRange& range, OptionSet<BoundingRectBehavior> behavior)
{
    ASSERT(!behavior.contains(BoundingRectBehavior::UseVisibleBounds));
    ASSERT(!behavior.contains(BoundingRectBehavior::IgnoreTinyRects));
    Vector<IntRect> rects;
    for (auto& node : intersectingNodes(range)) {
        auto renderer = node.renderer();
        if (renderer && renderer->isBR())
            downcast<RenderLineBreak>(*renderer).absoluteRects(rects, flooredLayoutPoint(renderer->localToAbsolute()));
        else if (is<Text>(node)) {
            for (auto& rect : absoluteRectsForRangeInText(range, downcast<Text>(node), behavior))
                rects.append(enclosingIntRect(rect));
        }
    }
    return rects;
}

static RefPtr<Node> nodeBefore(const BoundaryPoint& point)
{
    if (point.offset) {
        if (auto node = point.container->traverseToChildAt(point.offset - 1))
            return node;
    }
    return point.container.ptr();
}

enum class CoordinateSpace { Client, Absolute };

static Vector<FloatRect> borderAndTextRects(const SimpleRange& range, CoordinateSpace space, OptionSet<RenderObject::BoundingRectBehavior> behavior)
{
    Vector<FloatRect> rects;

    range.start.document().updateLayoutIgnorePendingStylesheets();

    bool useVisibleBounds = behavior.contains(RenderObject::BoundingRectBehavior::UseVisibleBounds);

    HashSet<Element*> selectedElementsSet;
    for (auto& node : intersectingNodesWithDeprecatedZeroOffsetStartQuirk(range)) {
        if (is<Element>(node))
            selectedElementsSet.add(&downcast<Element>(node));
    }

    // Don't include elements at the end of the range that are only partially selected.
    // FIXME: What about the start of the range? The asymmetry here does not make sense. Seems likely this logic is not quite right in other respects, too.
    if (auto lastNode = nodeBefore(range.end)) {
        for (auto& ancestor : ancestorsOfType<Element>(*lastNode))
            selectedElementsSet.remove(&ancestor);
    }

    constexpr OptionSet<RenderObject::VisibleRectContextOption> visibleRectOptions = {
        RenderObject::VisibleRectContextOption::UseEdgeInclusiveIntersection,
        RenderObject::VisibleRectContextOption::ApplyCompositedClips,
        RenderObject::VisibleRectContextOption::ApplyCompositedContainerScrolls
    };

    for (auto& node : intersectingNodesWithDeprecatedZeroOffsetStartQuirk(range)) {
        if (is<Element>(node) && selectedElementsSet.contains(&downcast<Element>(node)) && (useVisibleBounds || !node.parentElement() || !selectedElementsSet.contains(node.parentElement()))) {
            if (auto renderer = downcast<Element>(node).renderBoxModelObject()) {
                if (useVisibleBounds) {
                    auto localBounds = renderer->borderBoundingBox();
                    auto rootClippedBounds = renderer->computeVisibleRectInContainer(localBounds, &renderer->view(), { false, false, visibleRectOptions });
                    if (!rootClippedBounds)
                        continue;
                    auto snappedBounds = snapRectToDevicePixels(*rootClippedBounds, node.document().deviceScaleFactor());
                    if (space == CoordinateSpace::Client)
                        node.document().convertAbsoluteToClientRect(snappedBounds, renderer->style());
                    rects.append(snappedBounds);
                    continue;
                }

                Vector<FloatQuad> elementQuads;
                renderer->absoluteQuads(elementQuads);
                if (space == CoordinateSpace::Client)
                    node.document().convertAbsoluteToClientQuads(elementQuads, renderer->style());
                rects.appendVector(boundingBoxes(elementQuads));
            }
        } else if (is<Text>(node)) {
            if (auto renderer = downcast<Text>(node).renderer()) {
                auto clippedRects = absoluteRectsForRangeInText(range, downcast<Text>(node), behavior);
                if (space == CoordinateSpace::Client)
                    node.document().convertAbsoluteToClientRects(clippedRects, renderer->style());
                rects.appendVector(clippedRects);
            }
        }
    }

    if (behavior.contains(RenderObject::BoundingRectBehavior::IgnoreTinyRects)) {
        rects.removeAllMatching([&] (const FloatRect& rect) -> bool {
            return rect.area() <= 1;
        });
    }

    return rects;
}

Vector<FloatRect> RenderObject::absoluteBorderAndTextRects(const SimpleRange& range, OptionSet<BoundingRectBehavior> behavior)
{
    return borderAndTextRects(range, CoordinateSpace::Absolute, behavior);
}

Vector<FloatRect> RenderObject::clientBorderAndTextRects(const SimpleRange& range)
{
    return borderAndTextRects(range, CoordinateSpace::Client, { });
}

#if PLATFORM(IOS_FAMILY)

static bool intervalsSufficientlyOverlap(int startA, int endA, int startB, int endB)
{
    if (endA <= startA || endB <= startB)
        return false;

    const float sufficientOverlap = .75;

    int lengthA = endA - startA;
    int lengthB = endB - startB;

    int maxStart = std::max(startA, startB);
    int minEnd = std::min(endA, endB);

    if (maxStart > minEnd)
        return false;

    return minEnd - maxStart >= sufficientOverlap * std::min(lengthA, lengthB);
}

static inline void adjustLineHeightOfSelectionGeometries(Vector<SelectionGeometry>& geometries, size_t numberOfGeometries, int lineNumber, int lineTop, int lineHeight)
{
    ASSERT(geometries.size() >= numberOfGeometries);
    for (size_t i = numberOfGeometries; i; ) {
        --i;
        if (geometries[i].lineNumber())
            break;
        if (geometries[i].behavior() == SelectionRenderingBehavior::UseIndividualQuads)
            continue;
        geometries[i].setLineNumber(lineNumber);
        geometries[i].setLogicalTop(lineTop);
        geometries[i].setLogicalHeight(lineHeight);
    }
}

static SelectionGeometry coalesceSelectionGeometries(const SelectionGeometry& original, const SelectionGeometry& previous)
{
    SelectionGeometry result({ unionRect(previous.rect(), original.rect()) }, SelectionRenderingBehavior::CoalesceBoundingRects, original.isHorizontal(), original.pageNumber());
    result.setDirection(original.containsStart() || original.containsEnd() ? original.direction() : previous.direction());
    result.setContainsStart(previous.containsStart() || original.containsStart());
    result.setContainsEnd(previous.containsEnd() || original.containsEnd());
    result.setIsFirstOnLine(previous.isFirstOnLine() || original.isFirstOnLine());
    result.setIsLastOnLine(previous.isLastOnLine() || original.isLastOnLine());
    return result;
}

Vector<SelectionGeometry> RenderObject::collectSelectionGeometriesWithoutUnionInteriorLines(const SimpleRange& range)
{
    return collectSelectionGeometriesInternal(range).geometries;
}

auto RenderObject::collectSelectionGeometriesInternal(const SimpleRange& range) -> SelectionGeometries
{
    Vector<SelectionGeometry> geometries;
    Vector<SelectionGeometry> newGeometries;
    bool hasFlippedWritingMode = range.start.container->renderer() && range.start.container->renderer()->style().isFlippedBlocksWritingMode();
    bool containsDifferentWritingModes = false;
    for (auto& node : intersectingNodesWithDeprecatedZeroOffsetStartQuirk(range)) {
        auto renderer = node.renderer();
        // Only ask leaf render objects for their line box rects.
        if (renderer && !renderer->firstChildSlow() && renderer->style().userSelect() != UserSelect::None) {
            bool isStartNode = renderer->node() == range.start.container.ptr();
            bool isEndNode = renderer->node() == range.end.container.ptr();
            if (hasFlippedWritingMode != renderer->style().isFlippedBlocksWritingMode())
                containsDifferentWritingModes = true;
            // FIXME: Sending 0 for the startOffset is a weird way of telling the renderer that the selection
            // doesn't start inside it, since we'll also send 0 if the selection *does* start in it, at offset 0.
            //
            // FIXME: Selection endpoints aren't always inside leaves, and we only build SelectionGeometries for leaves,
            // so we can't accurately determine which SelectionGeometries contain the selection start and end using
            // only the offsets of the start and end. We need to pass the whole Range.
            int beginSelectionOffset = isStartNode ? range.start.offset : 0;
            int endSelectionOffset = isEndNode ? range.end.offset : std::numeric_limits<int>::max();
            renderer->collectSelectionGeometries(newGeometries, beginSelectionOffset, endSelectionOffset);
            for (auto& selectionGeometry : newGeometries) {
                if (selectionGeometry.containsStart() && !isStartNode)
                    selectionGeometry.setContainsStart(false);
                if (selectionGeometry.containsEnd() && !isEndNode)
                    selectionGeometry.setContainsEnd(false);
                if (selectionGeometry.logicalWidth() || selectionGeometry.logicalHeight())
                    geometries.append(selectionGeometry);
            }
            newGeometries.shrink(0);
        }
    }

    // The range could span nodes with different writing modes.
    // If this is the case, we use the writing mode of the common ancestor.
    if (containsDifferentWritingModes) {
        if (auto ancestor = commonInclusiveAncestor<ComposedTree>(range))
            hasFlippedWritingMode = ancestor->renderer()->style().isFlippedBlocksWritingMode();
    }

    auto numberOfGeometries = geometries.size();

    // If the selection ends in a BR, then add the line break bit to the last rect we have.
    // This will cause its selection rect to extend to the end of the line.
    if (numberOfGeometries) {
        // Only set the line break bit if the end of the range actually
        // extends all the way to include the <br>. VisiblePosition helps to
        // figure this out.
        if (is<HTMLBRElement>(VisiblePosition(makeContainerOffsetPosition(range.end)).deepEquivalent().firstNode()))
            geometries.last().setIsLineBreak(true);
    }

    int lineTop = std::numeric_limits<int>::max();
    int lineBottom = std::numeric_limits<int>::min();
    int lastLineTop = lineTop;
    int lastLineBottom = lineBottom;
    int lineNumber = 0;

    for (size_t i = 0; i < numberOfGeometries; ++i) {
        int currentRectTop = geometries[i].logicalTop();
        int currentRectBottom = currentRectTop + geometries[i].logicalHeight();

        // We don't want to count the ruby text as a separate line.
        if (intervalsSufficientlyOverlap(currentRectTop, currentRectBottom, lineTop, lineBottom) || (i && geometries[i].isRubyText())) {
            // Grow the current line bounds.
            lineTop = std::min(lineTop, currentRectTop);
            lineBottom = std::max(lineBottom, currentRectBottom);
            // Avoid overlap with the previous line.
            if (!hasFlippedWritingMode)
                lineTop = std::max(lastLineBottom, lineTop);
            else
                lineBottom = std::min(lastLineTop, lineBottom);
        } else {
            adjustLineHeightOfSelectionGeometries(geometries, i, lineNumber, lineTop, lineBottom - lineTop);
            if (!hasFlippedWritingMode) {
                lastLineTop = lineTop;
                if (currentRectBottom >= lastLineTop) {
                    lastLineBottom = lineBottom;
                    lineTop = lastLineBottom;
                } else {
                    lineTop = currentRectTop;
                    lastLineBottom = std::numeric_limits<int>::min();
                }
                lineBottom = currentRectBottom;
            } else {
                lastLineBottom = lineBottom;
                if (currentRectTop <= lastLineBottom && i && geometries[i].pageNumber() == geometries[i - 1].pageNumber()) {
                    lastLineTop = lineTop;
                    lineBottom = lastLineTop;
                } else {
                    lastLineTop = std::numeric_limits<int>::max();
                    lineBottom = currentRectBottom;
                }
                lineTop = currentRectTop;
            }
            ++lineNumber;
        }
    }

    // Adjust line height.
    adjustLineHeightOfSelectionGeometries(geometries, numberOfGeometries, lineNumber, lineTop, lineBottom - lineTop);

    // When using SelectionRenderingBehavior::CoalesceBoundingRects, sort the rectangles and make sure there are no gaps.
    // The rectangles could be unsorted when there is ruby text and we could have gaps on the line when adjacent elements
    // on the line have a different orientation.
    //
    // Note that for selection geometries with SelectionRenderingBehavior::UseIndividualQuads, we avoid sorting in order to
    // preserve the fact that the resulting geometries correspond to the order in which the quads are discovered during DOM
    // traversal. This allows us to efficiently coalesce adjacent selection quads.
    size_t firstRectWithCurrentLineNumber = 0;
    for (size_t currentRect = 1; currentRect < numberOfGeometries; ++currentRect) {
        if (geometries[currentRect].lineNumber() != geometries[currentRect - 1].lineNumber()) {
            firstRectWithCurrentLineNumber = currentRect;
            continue;
        }
        if (geometries[currentRect].logicalLeft() >= geometries[currentRect - 1].logicalLeft())
            continue;

        if (geometries[currentRect].behavior() != SelectionRenderingBehavior::CoalesceBoundingRects)
            continue;

        auto selectionRect = geometries[currentRect];
        size_t i;
        for (i = currentRect; i > firstRectWithCurrentLineNumber && selectionRect.logicalLeft() < geometries[i - 1].logicalLeft(); --i)
            geometries[i] = geometries[i - 1];
        geometries[i] = selectionRect;
    }

    for (size_t j = 1; j < numberOfGeometries; ++j) {
        if (geometries[j].lineNumber() != geometries[j - 1].lineNumber())
            continue;
        if (geometries[j].behavior() == SelectionRenderingBehavior::UseIndividualQuads)
            continue;
        auto& previousRect = geometries[j - 1];
        bool previousRectMayNotReachRightEdge = (previousRect.direction() == TextDirection::LTR && previousRect.containsEnd()) || (previousRect.direction() == TextDirection::RTL && previousRect.containsStart());
        if (previousRectMayNotReachRightEdge)
            continue;
        int adjustedWidth = geometries[j].logicalLeft() - previousRect.logicalLeft();
        if (adjustedWidth > previousRect.logicalWidth())
            previousRect.setLogicalWidth(adjustedWidth);
    }

    int maxLineNumber = lineNumber;

    // Extend rects out to edges as needed.
    for (size_t i = 0; i < numberOfGeometries; ++i) {
        auto& selectionGeometry = geometries[i];
        if (!selectionGeometry.isLineBreak() && selectionGeometry.lineNumber() >= maxLineNumber)
            continue;
        if (selectionGeometry.behavior() == SelectionRenderingBehavior::UseIndividualQuads)
            continue;
        if (selectionGeometry.direction() == TextDirection::RTL && selectionGeometry.isFirstOnLine()) {
            selectionGeometry.setLogicalWidth(selectionGeometry.logicalWidth() + selectionGeometry.logicalLeft() - selectionGeometry.minX());
            selectionGeometry.setLogicalLeft(selectionGeometry.minX());
        } else if (selectionGeometry.direction() == TextDirection::LTR && selectionGeometry.isLastOnLine())
            selectionGeometry.setLogicalWidth(selectionGeometry.maxX() - selectionGeometry.logicalLeft());
    }

    return { WTFMove(geometries), maxLineNumber };
}

static bool coalesceSelectionGeometryWithAdjacentQuadsIfPossible(SelectionGeometry& current, const SelectionGeometry& next)
{
    auto nextQuad = next.quad();
    if (nextQuad.isEmpty())
        return true;

    auto areCloseEnoughToCoalesce = [](const FloatPoint& first, const FloatPoint& second) {
        constexpr float maxDistanceBetweenBoundaryPoints = 2;
        return (first - second).diagonalLengthSquared() <= maxDistanceBetweenBoundaryPoints * maxDistanceBetweenBoundaryPoints;
    };

    auto currentQuad = current.quad();
    if (!areCloseEnoughToCoalesce(currentQuad.p2(), nextQuad.p1()) || !areCloseEnoughToCoalesce(currentQuad.p3(), nextQuad.p4()))
        return false;

    if (std::abs(rotatedBoundingRectWithMinimumAngleOfRotation(currentQuad).angleInRadians - rotatedBoundingRectWithMinimumAngleOfRotation(nextQuad).angleInRadians) > radiansPerDegreeFloat)
        return false;

    currentQuad.setP2(nextQuad.p2());
    currentQuad.setP3(nextQuad.p3());
    current.setQuad(currentQuad);
    current.setDirection(current.containsStart() || current.containsEnd() ? current.direction() : next.direction());
    current.setContainsStart(current.containsStart() || next.containsStart());
    current.setContainsEnd(current.containsEnd() || next.containsEnd());
    current.setIsFirstOnLine(current.isFirstOnLine() || next.isFirstOnLine());
    current.setIsLastOnLine(current.isLastOnLine() || next.isLastOnLine());
    return true;
}

Vector<SelectionGeometry> RenderObject::collectSelectionGeometries(const SimpleRange& range)
{
    auto result = RenderObject::collectSelectionGeometriesInternal(range);
    auto numberOfGeometries = result.geometries.size();

    // Union all the rectangles on interior lines (i.e. not first or last).
    // On first and last lines, just avoid having overlaps by merging intersecting rectangles.
    Vector<SelectionGeometry> coalescedGeometries;
    IntRect interiorUnionRect;
    for (size_t i = 0; i < numberOfGeometries; ++i) {
        auto& currentGeometry = result.geometries[i];
        if (currentGeometry.behavior() == SelectionRenderingBehavior::UseIndividualQuads) {
            if (currentGeometry.quad().isEmpty())
                continue;

            if (coalescedGeometries.isEmpty() || !coalesceSelectionGeometryWithAdjacentQuadsIfPossible(coalescedGeometries.last(), currentGeometry))
                coalescedGeometries.append(currentGeometry);
            continue;
        }

        if (currentGeometry.lineNumber() == 1) {
            ASSERT(interiorUnionRect.isEmpty());
            if (!coalescedGeometries.isEmpty()) {
                auto& previousRect = coalescedGeometries.last();
                if (previousRect.rect().intersects(currentGeometry.rect())) {
                    previousRect = coalesceSelectionGeometries(currentGeometry, previousRect);
                    continue;
                }
            }
            // Couldn't merge with previous rect, so just appending.
            coalescedGeometries.append(currentGeometry);
        } else if (currentGeometry.lineNumber() < result.maxLineNumber) {
            if (interiorUnionRect.isEmpty()) {
                // Start collecting interior rects.
                interiorUnionRect = currentGeometry.rect();
            } else if (interiorUnionRect.intersects(currentGeometry.rect())
                || interiorUnionRect.maxX() == currentGeometry.rect().x()
                || interiorUnionRect.maxY() == currentGeometry.rect().y()
                || interiorUnionRect.x() == currentGeometry.rect().maxX()
                || interiorUnionRect.y() == currentGeometry.rect().maxY()) {
                // Only union the lines that are attached.
                // For iBooks, the interior lines may cross multiple horizontal pages.
                interiorUnionRect.unite(currentGeometry.rect());
            } else {
                coalescedGeometries.append(SelectionGeometry({ interiorUnionRect }, SelectionRenderingBehavior::CoalesceBoundingRects, currentGeometry.isHorizontal(), currentGeometry.pageNumber()));
                interiorUnionRect = currentGeometry.rect();
            }
        } else {
            // Processing last line.
            if (!interiorUnionRect.isEmpty()) {
                coalescedGeometries.append(SelectionGeometry({ interiorUnionRect }, SelectionRenderingBehavior::CoalesceBoundingRects, currentGeometry.isHorizontal(), currentGeometry.pageNumber()));
                interiorUnionRect = IntRect();
            }

            ASSERT(!coalescedGeometries.isEmpty());
            auto& previousGeometry = coalescedGeometries.last();
            if (previousGeometry.logicalTop() == currentGeometry.logicalTop() && previousGeometry.rect().intersects(currentGeometry.rect())) {
                // previousRect is also on the last line, and intersects the current one.
                previousGeometry = coalesceSelectionGeometries(currentGeometry, previousGeometry);
                continue;
            }
            // Couldn't merge with previous rect, so just appending.
            coalescedGeometries.append(currentGeometry);
        }
    }

    return coalescedGeometries;
}

#endif

String RenderObject::description() const
{
    StringBuilder builder;

    builder.append(renderName(), ' ');
    if (node())
        builder.append(' ', node()->description());
    
    return builder.toString();
}

String RenderObject::debugDescription() const
{
    StringBuilder builder;

    builder.append(renderName(), " 0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase));
    if (node())
        builder.append(' ', node()->debugDescription());
    
    return builder.toString();
}

TextStream& operator<<(TextStream& ts, const RenderObject& renderer)
{
    ts << renderer.debugDescription();
    return ts;
}

#if ENABLE(TREE_DEBUGGING)

void printRenderTreeForLiveDocuments()
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isMainFrame())
            fprintf(stderr, "----------------------main frame--------------------------\n");
        fprintf(stderr, "%s", document->url().string().utf8().data());
        showRenderTree(document->renderView());
    }
}

void printLayerTreeForLiveDocuments()
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isMainFrame())
            fprintf(stderr, "----------------------main frame--------------------------\n");
        fprintf(stderr, "%s", document->url().string().utf8().data());
        showLayerTree(document->renderView());
    }
}

void printGraphicsLayerTreeForLiveDocuments()
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isMainFrame()) {
            WTFLogAlways("Graphics layer tree for root document %p %s", document, document->url().string().utf8().data());
            showGraphicsLayerTreeForCompositor(document->renderView()->compositor());
        }
    }
}

#endif // ENABLE(TREE_DEBUGGING)

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showNodeTree(const WebCore::RenderObject* object)
{
    if (!object)
        return;
    object->showNodeTreeForThis();
}

void showLineTree(const WebCore::RenderObject* object)
{
    if (!object)
        return;
    object->showLineTreeForThis();
}

void showRenderTree(const WebCore::RenderObject* object)
{
    if (!object)
        return;
    object->showRenderTreeForThis();
}

#endif

bool WebCore::shouldApplyLayoutContainment(const WebCore::RenderObject& renderer)
{
    return renderer.style().containsLayout() && (!renderer.isInline() || renderer.isAtomicInlineLevelBox()) && !renderer.isRubyText() && (!renderer.isTablePart() || renderer.isRenderBlockFlow());
}

bool WebCore::shouldApplySizeContainment(const WebCore::RenderObject& renderer)
{
    return renderer.style().containsSize() && (!renderer.isInline() || renderer.isAtomicInlineLevelBox()) && !renderer.isRubyText() && (!renderer.isTablePart() || renderer.isTableCaption()) && !renderer.isTable();
}
