/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
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
#include "AXUtilities.h"
#include "BoundaryPointInlines.h"
#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "EditingInlines.h"
#include "Editor.h"
#include "ElementAncestorIteratorInlines.h"
#include "FloatQuad.h"
#include "FrameSelection.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLBRElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "LayoutBox.h"
#include "LayoutIntegrationCoverage.h"
#include "LegacyRenderSVGModelObject.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NodeInlines.h"
#include "OutlinePainter.h"
#include "Page.h"
#include "PseudoElement.h"
#include "ReferencedSVGResources.h"
#include "RenderChildIterator.h"
#include "RenderCounter.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLineBreak.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderReplica.h"
#include "RenderSVGBlock.h"
#include "RenderSVGInline.h"
#include "RenderSVGModelObject.h"
#include "RenderScrollbarPart.h"
#include "RenderTableRow.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "RenderViewTransitionCapture.h"
#include "RenderWidget.h"
#include "RenderedPosition.h"
#include "SVGRenderSupport.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "TransformState.h"
#include "ViewTransition.h"
#include <algorithm>
#include <stdio.h>
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "SelectionGeometry.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_PREFERABLY_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL(RenderObject);
WTF_MAKE_PREFERABLY_COMPACT_TZONE_ALLOCATED_IMPL(RenderObject::RenderObjectRareData);
WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderObject::CachedImageListener);

#if ASSERT_ENABLED

RenderObject::SetLayoutNeededForbiddenScope::SetLayoutNeededForbiddenScope(const RenderObject& renderObject, bool isForbidden)
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

struct SameSizeAsRenderObject : public CanMakeSingleThreadWeakPtr<SameSizeAsRenderObject>, public CanMakeCheckedPtr<SameSizeAsRenderObject> {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(SameSizeAsRenderObject);
    WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(SameSizeAsRenderObject);

    virtual ~SameSizeAsRenderObject() = default; // Allocate vtable pointer.
#if ASSERT_ENABLED
    unsigned m_debugBitfields : 2;
#endif
    unsigned m_stateBitfields;
    WeakRef<Node, WeakPtrImplWithEventTargetData> node;
    SingleThreadWeakPtr<RenderObject> pointers;
    SingleThreadPackedWeakPtr<RenderObject> m_previous;
    uint16_t m_typeFlags;
    SingleThreadPackedWeakPtr<RenderObject> m_next;
    uint8_t m_type;
    uint8_t m_typeSpecificFlags;
    CheckedPtr<Layout::Box> layoutBox;
    void* cachedResourceClient;
};

#if CPU(ADDRESS64)
static_assert(sizeof(RenderObject) == sizeof(SameSizeAsRenderObject), "RenderObject should stay small");
#endif

void RenderObjectDeleter::operator() (RenderObject* renderer) const
{
    renderer->destroy();
}

RenderObject::RenderObject(Type type, Node& node, OptionSet<TypeFlag> typeFlags, TypeSpecificFlags typeSpecificFlags)
    : CanMakeSingleThreadWeakPtr<RenderObject>()
#if ASSERT_ENABLED
    , m_setNeedsLayoutForbidden(false)
#endif
    , m_node(node)
    , m_typeFlags(node.isDocumentNode() ? (typeFlags | TypeFlag::IsAnonymous) : typeFlags)
    , m_type(type)
    , m_typeSpecificFlags(typeSpecificFlags)
{
    ASSERT(!typeFlags.contains(TypeFlag::IsAnonymous));
    if (CheckedPtr renderView = node.document().renderView())
        renderView->didCreateRenderer();
}

RenderObject::~RenderObject()
{
    clearLayoutBox();
    ASSERT(!hasRareData());
}

CheckedRef<RenderView> RenderObject::checkedView() const
{
    return view();
}

void RenderObject::setLayoutBox(Layout::Box& box)
{
    m_layoutBox = &box;
    m_layoutBox->setRendererForIntegration(this);
}

void RenderObject::clearLayoutBox()
{
    if (!m_layoutBox)
        return;

    ASSERT(m_layoutBox->rendererForIntegration() == this);

    m_layoutBox->setRendererForIntegration(nullptr);
    m_layoutBox = nullptr;
}

RenderTheme& RenderObject::theme() const
{
    return RenderTheme::singleton();
}

bool RenderObject::isDescendantOf(const RenderObject* ancestor) const
{
    for (auto* renderer = this; renderer; renderer = renderer->m_parent.get()) {
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

void RenderObject::setFragmentedFlowStateIncludingDescendants(FragmentedFlowState state, SkipDescendentFragmentedFlow skipDescendentFragmentedFlow)
{
    setFragmentedFlowState(state);

    auto* renderElement = dynamicDowncast<RenderElement>(*this);
    if (!renderElement)
        return;

    for (CheckedRef child : childrenOfType<RenderObject>(*renderElement)) {
        // If the child is a fragmentation context it already updated the descendants flag accordingly.
        if (child->isRenderFragmentedFlow() && skipDescendentFragmentedFlow == SkipDescendentFragmentedFlow::Yes)
            continue;
        if (child->isOutOfFlowPositioned()) {
            // Fragmented status propagation stops at out-of-flow boundary.
            auto isInsideMulticolumnFlow = [&] {
                auto* containingBlock = child->containingBlock();
                if (!containingBlock) {
                    ASSERT_NOT_REACHED();
                    return false;
                }
                return containingBlock->fragmentedFlowState() == FragmentedFlowState::InsideFlow;
            };
            if (!isInsideMulticolumnFlow())
                continue;
        }
        ASSERT(skipDescendentFragmentedFlow == SkipDescendentFragmentedFlow::No || state != child->fragmentedFlowState());
        child->setFragmentedFlowStateIncludingDescendants(state, skipDescendentFragmentedFlow);
    }
}

RenderObject::FragmentedFlowState RenderObject::computedFragmentedFlowState(const RenderObject& renderer)
{
    if (!renderer.parent())
        return renderer.fragmentedFlowState();

    if (is<RenderMultiColumnFlow>(renderer)) {
        // Multicolumn flows do not inherit the flow state.
        return FragmentedFlowState::InsideFlow;
    }

    auto inheritedFlowState = RenderObject::FragmentedFlowState::NotInsideFlow;
    if (is<RenderText>(renderer))
        inheritedFlowState = renderer.parent()->fragmentedFlowState();
    else if (is<RenderSVGBlock>(renderer) || is<RenderSVGInline>(renderer) || is<LegacyRenderSVGModelObject>(renderer)) {
        // containingBlock() skips svg boundary (SVG root is a RenderReplaced).
        if (CheckedPtr svgRoot = SVGRenderSupport::findTreeRootObject(downcast<RenderElement>(renderer)))
            inheritedFlowState = svgRoot->fragmentedFlowState();
    } else if (CheckedPtr container = renderer.container())
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

    setFragmentedFlowStateIncludingDescendants(computedState, SkipDescendentFragmentedFlow::No);
}

void RenderObject::resetFragmentedFlowStateOnRemoval()
{
    ASSERT(!renderTreeBeingDestroyed());

    if (fragmentedFlowState() == FragmentedFlowState::NotInsideFlow)
        return;

    if (auto* renderElement = dynamicDowncast<RenderElement>(*this)) {
        renderElement->removeFromRenderFragmentedFlow();
        return;
    }

    // A RenderFragmentedFlow is always considered to be inside itself, so it never has to change its state in response to parent changes.
    if (isRenderFragmentedFlow())
        return;

    setFragmentedFlowStateIncludingDescendants(FragmentedFlowState::NotInsideFlow);
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
    for (CheckedPtr child = firstChildSlow(); child; child = child->nextSibling()) {
        overflowType = inclusionFunction(*child);
        if (overflowType != FixedHeight) {
            currentDepth++;
            if (overflowType == OverflowHeight)
                newFixedDepth = currentDepth;
            ASSERT(!stayWithin || child->isDescendantOf(stayWithin));
            return child.unsafeGet();
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
        for (CheckedPtr sibling = n->nextSibling(); sibling; sibling = sibling->nextSibling()) {
            overflowType = inclusionFunction(*sibling);
            if (overflowType != FixedHeight) {
                if (overflowType == OverflowHeight)
                    newFixedDepth = currentDepth;
                ASSERT(!stayWithin || !n->nextSibling() || n->nextSibling()->isDescendantOf(stayWithin));
                return sibling.unsafeGet();
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

CheckedPtr<RenderLayer> RenderObject::checkedEnclosingLayer() const
{
    return enclosingLayer();
}

RenderBox& RenderObject::enclosingBox() const
{
    return *lineageOfType<RenderBox>(const_cast<RenderObject&>(*this)).first();
}

RenderBoxModelObject& RenderObject::enclosingBoxModelObject() const
{
    return *lineageOfType<RenderBoxModelObject>(const_cast<RenderObject&>(*this)).first();
}

RenderBox* RenderObject::enclosingScrollableContainer() const
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

static inline bool isLayoutBoundary(const RenderElement& renderer)
{
    // FIXME: In future it may be possible to broaden these conditions in order to improve performance.
    if (renderer.isRenderView())
        return true;

    auto& style = renderer.style();
    if (CheckedPtr textControl = dynamicDowncast<RenderTextControl>(renderer)) {
        if (!textControl->isFlexItem() && !textControl->isGridItem() && style.fieldSizing() != FieldSizing::Content) {
            // Flexing type of layout systems may compute different size than what input's preferred width is which won't happen unless they run their layout as well.
            return true;
        }
    }

    if (renderer.shouldApplyLayoutContainment() && renderer.shouldApplySizeContainment())
        return true;

    if (renderer.isRenderOrLegacyRenderSVGRoot())
        return true;

    if (!renderer.hasNonVisibleOverflow()) {
        // While createsNewFormattingContext (a few lines below) covers this case, overflow visible is a super common value so we should be able
        // to bail out here fast.
        return false;
    }

    if (style.width().isIntrinsicOrLegacyIntrinsicOrAuto() || style.height().isIntrinsicOrLegacyIntrinsicOrAuto() || style.height().isPercentOrCalculated())
        return false;

    if (renderer.document().settings().layerBasedSVGEngineEnabled() && renderer.isSVGLayerAwareRenderer())
        return false;

    // Table parts can't be relayout roots since the table is responsible for layouting all the parts.
    if (renderer.isTablePart())
        return false;

    if (CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(renderer); !renderBlock->createsNewFormattingContext())
        return false;

    return true;
}

void RenderObject::clearNeedsLayout(HadSkippedLayout hadSkippedLayout)
{
    // FIXME: Consider not setting the "ever had layout" bit to true when "hadSkippedLayout"
    setEverHadLayout();
    setHadSkippedLayout(hadSkippedLayout == HadSkippedLayout::Yes);

    if (hasLayer())
        downcast<RenderLayerModelObject>(*this).layer()->setSelfAndChildrenNeedPositionUpdate();
    m_stateBitfields.clearFlag(StateFlag::NeedsLayout);
    setOutOfFlowChildNeedsLayoutBit(false);
    setNeedsSimplifiedNormalFlowLayoutBit(false);
    setNormalChildNeedsLayoutBit(false);
    setOutOfFlowChildNeedsStaticPositionLayoutBit(false);
    setNeedsOutOfFlowMovementLayoutBit(false);
#if ASSERT_ENABLED
    auto checkIfOutOfFlowDescendantsNeedLayout = [&](auto& renderBlock) {
        if (auto* outOfFlowDescendants = renderBlock.outOfFlowBoxes()) {
            for (auto& renderer : *outOfFlowDescendants)
                ASSERT(!renderer.needsLayout());
        }
    };
    if (auto* renderBlock = dynamicDowncast<RenderBlock>(*this))
        checkIfOutOfFlowDescendantsNeedLayout(*renderBlock);
#endif // ASSERT_ENABLED
}

void RenderObject::scheduleLayout(RenderElement* layoutRoot)
{
    if (auto* renderView = dynamicDowncast<RenderView>(layoutRoot))
        return renderView->frameView().checkedLayoutContext()->scheduleLayout();

    if (layoutRoot && layoutRoot->isRooted())
        layoutRoot->view().frameView().checkedLayoutContext()->scheduleSubtreeLayout(*layoutRoot);
}

RenderElement* RenderObject::markContainingBlocksForLayout(RenderElement* layoutRoot)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (is<RenderView>(*this))
        return downcast<RenderElement>(this);

    CheckedPtr ancestor = container();

    bool simplifiedNormalFlowLayout = needsSimplifiedNormalFlowLayout() && !selfNeedsLayout() && !normalChildNeedsLayout();
    bool hasOutOfFlowPosition = isOutOfFlowPositioned();

    while (ancestor) {
        // FIXME: Remove this once we remove the special cases for counters, quotes and mathml calling setNeedsLayout during preferred width computation.
        SetLayoutNeededForbiddenScope layoutForbiddenScope(*ancestor, isSetNeedsLayoutForbidden());

        // Don't mark the outermost object of an unrooted subtree. That object will be
        // marked when the subtree is added to the document.
        CheckedPtr container = ancestor->container();
        if (!container && !ancestor->isRenderView()) {
            // Internal render tree shuffle.
            return { };
        }

        if (simplifiedNormalFlowLayout && ancestor->overflowChangesMayAffectLayout())
            simplifiedNormalFlowLayout = false;

        if (hasOutOfFlowPosition) {
            bool willSkipRelativelyPositionedInlines = !ancestor->isRenderBlock() || ancestor->isAnonymousBlock();
            // Skip relatively positioned inlines and anonymous blocks to get to the enclosing RenderBlock.
            while (ancestor && (!ancestor->isRenderBlock() || ancestor->isAnonymousBlock()))
                ancestor = ancestor->container();
            if (!ancestor || ancestor->outOfFlowChildNeedsLayout())
                return { };
            if (willSkipRelativelyPositionedInlines)
                container = ancestor->container();
            ancestor->setOutOfFlowChildNeedsLayoutBit(true);
            simplifiedNormalFlowLayout = true;
        } else if (simplifiedNormalFlowLayout) {
            if (ancestor->needsSimplifiedNormalFlowLayout())
                return { };
            ancestor->setNeedsSimplifiedNormalFlowLayoutBit(true);
        } else {
            if (ancestor->normalChildNeedsLayout())
                return { };
            ancestor->setNormalChildNeedsLayoutBit(true);
        }
        ASSERT(!ancestor->isSetNeedsLayoutForbidden());

        if (layoutRoot) {
            // Having a valid layout root also mean we should not stop at layout boundaries.
            if (ancestor == layoutRoot)
                return layoutRoot;
        } else if (isLayoutBoundary(*ancestor))
            return ancestor.unsafeGet();

        if (auto* renderGrid = dynamicDowncast<RenderGrid>(container.get()); renderGrid && renderGrid->isExtrinsicallySized())
            simplifiedNormalFlowLayout = true;

        hasOutOfFlowPosition = ancestor->isOutOfFlowPositioned();
        ancestor = WTFMove(container);
    }
    return { };
}

void RenderObject::setNeedsPreferredWidthsUpdate(MarkingBehavior markParents)
{
    if (needsPreferredLogicalWidthsUpdate() && (!hasRareData() || !rareData().preferredLogicalWidthsNeedUpdateIsMarkOnlyThis)) {
        // Both this and our ancestor chain are already marked dirty.
        return;
    }

    m_stateBitfields.setFlag(StateFlag::PreferredLogicalWidthsNeedUpdate, true);
    if (isOutOfFlowPositioned()) {
        // A positioned object has no effect on the min/max width of its containing block ever. No need to mark ancestor chain.
        return;
    }

    if (markParents == MarkOnlyThis) {
        ensureRareData().preferredLogicalWidthsNeedUpdateIsMarkOnlyThis = true;
        return;
    }

    invalidateContainerPreferredLogicalWidths();
    if (hasRareData())
        ensureRareData().preferredLogicalWidthsNeedUpdateIsMarkOnlyThis = false;
}

void RenderObject::invalidateContainerPreferredLogicalWidths()
{
    // In order to avoid pathological behavior when inlines are deeply nested, we do include them
    // in the chain that we mark dirty (even though they're kind of irrelevant).
    CheckedPtr ancestor = isRenderTableCell() ? containingBlock() : container();
    while (ancestor) {
        if (ancestor->needsPreferredLogicalWidthsUpdate() && (!ancestor->hasRareData() || !ancestor->rareData().preferredLogicalWidthsNeedUpdateIsMarkOnlyThis))
            break;
        // Don't invalidate the outermost object of an unrooted subtree. That object will be
        // invalidated when the subtree is added to the document.
        CheckedPtr container = ancestor->isRenderTableCell() ? ancestor->containingBlock() : ancestor->container();
        if (!container && !ancestor->isRenderView())
            break;

        ancestor->m_stateBitfields.setFlag(StateFlag::PreferredLogicalWidthsNeedUpdate, true);
        if (ancestor->style().hasOutOfFlowPosition()) {
            // A positioned object has no effect on the min/max width of its containing block ever.
            // We can optimize this case and not go up any further.
            break;
        }
        ancestor = WTFMove(container);
    }
}

void RenderObject::setLayerNeedsFullRepaint()
{
    ASSERT(hasLayer());
    downcast<RenderLayerModelObject>(*this).checkedLayer()->setRepaintStatus(RepaintStatus::NeedsFullRepaint);
}

void RenderObject::setLayerNeedsFullRepaintForOutOfFlowMovementLayout()
{
    ASSERT(hasLayer());
    downcast<RenderLayerModelObject>(*this).checkedLayer()->setRepaintStatus(RepaintStatus::NeedsFullRepaintForOutOfFlowMovementLayout);
}

static inline RenderBlock* nearestNonAnonymousContainingBlockIncludingSelf(RenderElement* renderer)
{
    while (renderer && (!is<RenderBlock>(*renderer) || renderer->isAnonymousBlock()))
        renderer = renderer->containingBlock();
    return downcast<RenderBlock>(renderer);
}

RenderBlock* RenderObject::containingBlockForPositionType(PositionType positionType, const RenderObject& renderer)
{
    if (positionType == PositionType::Static || positionType == PositionType::Relative || positionType == PositionType::Sticky) {
        auto containingBlockForObjectInFlow = [&] {
            auto* ancestor = renderer.parent();
            while (ancestor && ((ancestor->isInline() && !ancestor->isBlockLevelReplacedOrAtomicInline()) || !ancestor->isRenderBlock()))
                ancestor = ancestor->parent();
            return downcast<RenderBlock>(ancestor);
        };
        return containingBlockForObjectInFlow();
    }

    if (positionType == PositionType::Absolute) {
        auto containingBlockForAbsolutePosition = [&] {
            if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(renderer); renderInline && renderInline->style().position() == PositionType::Relative) {
                // A relatively positioned RenderInline forwards its absolute positioned descendants to
                // its nearest non-anonymous containing block (to avoid having positioned objects list in RenderInlines).
                return nearestNonAnonymousContainingBlockIncludingSelf(renderer.parent());
            }
            CheckedPtr ancestor = renderer.parent();
            while (ancestor && !ancestor->canContainAbsolutelyPositionedObjects())
                ancestor = ancestor->parent();
            // Make sure we only return non-anonymous RenderBlock as containing block.
            return nearestNonAnonymousContainingBlockIncludingSelf(ancestor.get());
        };
        return containingBlockForAbsolutePosition();
    }

    if (positionType == PositionType::Fixed) {
        auto containingBlockForFixedPosition = [&] () -> RenderBlock* {
            CheckedPtr ancestor = renderer.parent();
            while (ancestor && !ancestor->canContainFixedPositionObjects()) {
                if (isInTopLayerOrBackdrop(ancestor->style(), ancestor->element()))
                    return &renderer.view();
                ancestor = ancestor->parent();
            }
            return nearestNonAnonymousContainingBlockIncludingSelf(ancestor.get());
        };
        return containingBlockForFixedPosition();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

RenderBlock* RenderObject::containingBlock() const
{
    // FIXME: See https://bugs.webkit.org/show_bug.cgi?id=270977 for RenderLineBreak special treatment.
    if (is<RenderText>(*this) || is<RenderLineBreak>(*this))
        return containingBlockForPositionType(PositionType::Static, *this);

    auto containingBlockForRenderer = [](const auto& renderer) -> RenderBlock* {
        if (isInTopLayerOrBackdrop(renderer.style(), renderer.element()))
            return &renderer.view();
        return containingBlockForPositionType(renderer.style().position(), renderer);
    };

    if (!parent()) {
        if (auto* part = dynamicDowncast<RenderScrollbarPart>(*this)) {
            if (CheckedPtr scrollbarPart = part->rendererOwningScrollbar())
                return containingBlockForRenderer(*scrollbarPart);
            return nullptr;
        }
    }
    return containingBlockForRenderer(downcast<RenderElement>(*this));
}

CheckedPtr<RenderBlock> RenderObject::checkedContainingBlock() const
{
    return containingBlock();
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
        CheckedPtr previous = previousSibling();
        RefPtr node = this->node();
        if (!previous || !previous->isBR() || !node || !node->isContainerNode() || !isInline()) {
            // For inline elements we don't use absoluteQuads, since it takes into account continuations and leads to wrong results.
            absoluteQuadsForSelection(quads);
        }
    } else {
        unsigned offset = start;
        for (CheckedPtr child = childAt(start); child && offset < end; child = child->nextSibling(), ++offset)
            child->absoluteQuads(quads);
    }

    for (auto& quad : quads)
        geometries.append(SelectionGeometry(quad, HTMLElement::selectionRenderingBehavior(protectedNode().get()), isHorizontalWritingMode(), checkedView()->pageNumberForBlockProgressionOffset(quad.enclosingBoundingBox().x())));
}
#endif

IntRect RenderObject::absoluteBoundingBoxRect(bool useTransforms, bool* wasFixed) const
{
    if (useTransforms) {
        Vector<FloatQuad> quads;
        absoluteQuads(quads, wasFixed);
        return enclosingIntRect(unitedBoundingBoxes(quads)).toRectWithExtentsClippedToNumericLimits();
    }

    FloatPoint absPos = localToAbsolute(FloatPoint(), { } /* ignore transforms */, wasFixed);
    Vector<LayoutRect> rects;
    boundingRects(rects, flooredLayoutPoint(absPos));

    size_t n = rects.size();
    if (!n)
        return IntRect();

    LayoutRect result = unionRect(rects);
    return snappedIntRect(result).toRectWithExtentsClippedToNumericLimits();
}

void RenderObject::absoluteFocusRingQuads(Vector<FloatQuad>& quads)
{
    CheckedPtr elementRenderer = dynamicDowncast<RenderElement>(*this);
    if (!elementRenderer)
        return;

    // FIXME: collectFocusRingRects() needs to be passed this transform-unaware
    // localToAbsolute() offset here because OutlinePainter::collectFocusRingRects()
    // implicitly assumes that. This doesn't work correctly with transformed
    // descendants.
    FloatPoint absolutePoint = localToAbsolute();
    auto rects = OutlinePainter::collectFocusRingRects(*elementRenderer, flooredLayoutPoint(absolutePoint), nullptr);
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

    auto* renderElement = dynamicDowncast<RenderElement>(*this);
    if (!renderElement)
        return;

    for (CheckedRef child : childrenOfType<RenderObject>(*renderElement))
        child->addAbsoluteRectForLayer(result);
}

// FIXME: change this to use the subtreePaint terminology
LayoutRect RenderObject::paintingRootRect(LayoutRect& topLevelRect)
{
    LayoutRect result = absoluteBoundingBoxRectIgnoringTransforms();
    topLevelRect = result;
    if (auto* renderElement = dynamicDowncast<RenderElement>(*this)) {
        for (CheckedRef child : childrenOfType<RenderObject>(*renderElement))
            child->addAbsoluteRectForLayer(result);
    }
    return result;
}

static inline bool canRelyOnAncestorLayerFullRepaint(const RenderObject& rendererToRepaint, const RenderLayer& ancestorLayer)
{
    auto* renderElement = dynamicDowncast<RenderElement>(rendererToRepaint);
    if (!renderElement || !renderElement->hasSelfPaintingLayer())
        return true;
    return ancestorLayer.renderer().hasNonVisibleOverflow();
}

RenderObject::RepaintContainerStatus RenderObject::containerForRepaint() const
{
    CheckedPtr<const RenderLayerModelObject> repaintContainer;
    auto fullRepaintAlreadyScheduled = false;

    if (view().usesCompositing()) {
        if (CheckedPtr enclosingLayer = this->enclosingLayer()) {
            auto compLayerStatus = enclosingLayer->enclosingCompositingLayerForRepaint();
            if (compLayerStatus.layer) {
                repaintContainer = &compLayerStatus.layer->renderer();
                fullRepaintAlreadyScheduled = compLayerStatus.fullRepaintAlreadyScheduled && canRelyOnAncestorLayerFullRepaint(*this, *compLayerStatus.layer);
            }
        }
    }
    if (view().hasSoftwareFilters()) {
        if (CheckedPtr parentLayer = enclosingLayer()) {
            if (CheckedPtr enclosingFilterLayer = parentLayer->enclosingFilterLayer()) {
                fullRepaintAlreadyScheduled = parentLayer->needsFullRepaint() && canRelyOnAncestorLayerFullRepaint(*this, *parentLayer);
                return { fullRepaintAlreadyScheduled, &enclosingFilterLayer->renderer() };
            }
        }
    }

    // If we have a flow thread, then we need to do individual repaints within the RenderFragmentContainers instead.
    // Return the flow thread as a repaint container in order to create a chokepoint that allows us to change
    // repainting to do individual region repaints.
    if (CheckedPtr parentRenderFragmentedFlow = enclosingFragmentedFlow()) {
        // If we have already found a repaint container then we will repaint into that container only if it is part of the same
        // flow thread. Otherwise we will need to catch the repaint call and send it to the flow thread.
        CheckedPtr repaintContainerFragmentedFlow = repaintContainer ? repaintContainer->enclosingFragmentedFlow() : nullptr;
        if (!repaintContainerFragmentedFlow || repaintContainerFragmentedFlow != parentRenderFragmentedFlow)
            repaintContainer = WTFMove(parentRenderFragmentedFlow);
    }
    return { fullRepaintAlreadyScheduled, WTFMove(repaintContainer) };
}

void RenderObject::propagateRepaintToParentWithOutlineAutoIfNeeded(const RenderLayerModelObject& repaintContainer, const LayoutRect& repaintRect) const
{
    if (!hasOutlineAutoAncestor())
        return;

    // FIXME: We should really propagate only when the child renderer sticks out.
    bool repaintRectNeedsConverting = false;
    // Issue repaint on the renderer with outline: auto.
    for (CheckedPtr renderer = this; renderer; renderer = renderer->parent()) {
        CheckedPtr originalRenderer = renderer;
        if (CheckedPtr previousMultiColumnSet = dynamicDowncast<RenderMultiColumnSet>(renderer->previousSibling()); previousMultiColumnSet && !renderer->isRenderMultiColumnSet() && !renderer->isLegend()) {
            CheckedPtr enclosingMultiColumnFlow = previousMultiColumnSet->multiColumnFlow();
            CheckedPtr renderMultiColumnPlaceholder = enclosingMultiColumnFlow->findColumnSpannerPlaceholder(downcast<RenderBox>(*renderer));
            ASSERT(renderMultiColumnPlaceholder);
            renderer = WTFMove(renderMultiColumnPlaceholder);
        }

        bool rendererHasOutlineAutoAncestor = renderer->hasOutlineAutoAncestor() || originalRenderer->hasOutlineAutoAncestor();
        ASSERT(rendererHasOutlineAutoAncestor
            || originalRenderer->outlineStyleForRepaint().outlineStyle() == OutlineStyle::Auto
            || (is<RenderBoxModelObject>(*renderer) && downcast<RenderBoxModelObject>(*renderer).isContinuation()));
        if (originalRenderer == &repaintContainer && rendererHasOutlineAutoAncestor)
            repaintRectNeedsConverting = true;
        if (rendererHasOutlineAutoAncestor)
            continue;
        // Issue repaint on the correct repaint container.
        LayoutRect adjustedRepaintRect = repaintRect;
        adjustedRepaintRect.inflate(originalRenderer->outlineStyleForRepaint().outlineSize());
        if (!repaintRectNeedsConverting)
            repaintContainer.repaintRectangle(adjustedRepaintRect);
        else if (CheckedPtr rendererWithOutline = dynamicDowncast<RenderLayerModelObject>(originalRenderer.get())) {
            adjustedRepaintRect = LayoutRect(repaintContainer.localToContainerQuad(FloatRect(adjustedRepaintRect), rendererWithOutline.get()).boundingBox());
            rendererWithOutline->repaintRectangle(adjustedRepaintRect);
        }
        return;
    }
    ASSERT_NOT_REACHED();
}

void RenderObject::repaintUsingContainer(SingleThreadWeakPtr<const RenderLayerModelObject>&& repaintContainer, const LayoutRect& r, bool shouldClipToLayer) const
{
    if (r.isEmpty())
        return;

    if (!repaintContainer)
        repaintContainer = &view();

    if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderFragmentedFlow>(*repaintContainer)) {
        fragmentedFlow->repaintRectangleInFragments(r);
        return;
    }

    if (!repaintContainer)
        return;

    propagateRepaintToParentWithOutlineAutoIfNeeded(*repaintContainer, r);

    if (repaintContainer->hasFilter() && repaintContainer->layer() && repaintContainer->layer()->requiresFullLayerImageForFilters()) {
        repaintContainer->checkedLayer()->setFilterBackendNeedsRepaintingInRect(r);
        return;
    }

    if (repaintContainer->isRenderView()) {
        CheckedRef view = this->view();
        ASSERT(repaintContainer == view.ptr());
        bool viewHasCompositedLayer = view->isComposited();
        if (!viewHasCompositedLayer || view->layer()->backing()->paintsIntoWindow()) {
            LayoutRect rect = r;
            if (viewHasCompositedLayer && view->layer()->transform())
                rect = LayoutRect(view->layer()->transform()->mapRect(snapRectToDevicePixels(rect, document().deviceScaleFactor())));
            view->repaintViewRectangle(rect);
            return;
        }
    }

    if (view().usesCompositing()) {
        ASSERT(repaintContainer->isComposited());
        if (CheckedPtr layer = repaintContainer->layer())
            layer->setBackingNeedsRepaintInRect(r, shouldClipToLayer ? GraphicsLayer::ShouldClipToLayer::Clip : GraphicsLayer::ShouldClipToLayer::DoNotClip);
    }
}

static inline bool fullRepaintIsScheduled(const RenderObject& renderer)
{
    if (!renderer.view().usesCompositing() && !renderer.document().ownerElement())
        return false;
    for (CheckedPtr ancestorLayer = renderer.enclosingLayer(); ancestorLayer; ancestorLayer = ancestorLayer->paintOrderParent()) {
        if (ancestorLayer->needsFullRepaint())
            return canRelyOnAncestorLayerFullRepaint(renderer, *ancestorLayer);
    }
    return false;
}

void RenderObject::issueRepaint(std::optional<LayoutRect> partialRepaintRect, ClipRepaintToLayer clipRepaintToLayer, ForceRepaint forceRepaint, std::optional<LayoutBoxExtent> additionalRepaintOutsets) const
{
    auto repaintContainer = containerForRepaint();
    if (!repaintContainer.renderer)
        repaintContainer = { fullRepaintIsScheduled(*this), &view() };

    if (repaintContainer.fullRepaintIsScheduled && forceRepaint == ForceRepaint::No)
        return;

    LayoutRect repaintRect;

    if (partialRepaintRect) {
        repaintRect = computeRectForRepaint(*partialRepaintRect, repaintContainer.renderer.get());
        if (additionalRepaintOutsets)
            repaintRect.expand(*additionalRepaintOutsets);
    } else
        repaintRect = clippedOverflowRectForRepaint(repaintContainer.renderer.get());

    repaintUsingContainer(repaintContainer.renderer.get(), repaintRect, clipRepaintToLayer == ClipRepaintToLayer::Yes);
}

void RenderObject::repaint(ForceRepaint forceRepaint) const
{
    ASSERT(isDescendantOf(&view()) || is<RenderScrollbarPart>(this) || is<RenderReplica>(this));

    if (view().printing())
        return;
    issueRepaint({ }, ClipRepaintToLayer::No, forceRepaint);
}

void RenderObject::repaintRectangle(const LayoutRect& repaintRect, bool shouldClipToLayer) const
{
    ASSERT(isDescendantOf(&view()) || is<RenderScrollbarPart>(this));
    return repaintRectangle(repaintRect, shouldClipToLayer ? ClipRepaintToLayer::Yes : ClipRepaintToLayer::No, ForceRepaint::No);
}

void RenderObject::repaintRectangle(const LayoutRect& repaintRect, ClipRepaintToLayer shouldClipToLayer, ForceRepaint forceRepaint, std::optional<LayoutBoxExtent> additionalRepaintOutsets) const
{
    ASSERT(isDescendantOf(&view()) || is<RenderScrollbarPart>(this) || is<RenderReplica>(this));

    if (view().printing())
        return;
    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    auto dirtyRect = repaintRect;
    dirtyRect.move(view().frameView().layoutContext().layoutDelta());
    issueRepaint(dirtyRect, shouldClipToLayer, forceRepaint, additionalRepaintOutsets);
}

void RenderObject::repaintSlowRepaintObject() const
{
    ASSERT(isDescendantOf(&view()) || is<RenderScrollbarPart>(this) || is<RenderReplica>(this));

    CheckedRef view = this->view();
    if (view->printing())
        return;

    CheckedPtr repaintContainer = containerForRepaint().renderer;

    bool shouldClipToLayer = true;
    IntRect repaintRect;
    // If this is the root background, we need to check if there is an extended background rect. If
    // there is, then we should not allow painting to clip to the layer size.
    if (isDocumentElementRenderer() || isBody()) {
        shouldClipToLayer = !view->frameView().hasExtendedBackgroundRectForPainting();
        repaintRect = snappedIntRect(view->backgroundRect());
    } else
        repaintRect = snappedIntRect(clippedOverflowRectForRepaint(repaintContainer.get()));

    repaintUsingContainer(repaintContainer.get(), repaintRect, shouldClipToLayer);
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

auto RenderObject::localRectsForRepaint(RepaintOutlineBounds) const -> RepaintRects
{
    ASSERT_NOT_REACHED();
    return { };
}

auto RenderObject::rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds repaintOutlineBounds) const -> RepaintRects
{
    auto localRects = localRectsForRepaint(repaintOutlineBounds);
    if (localRects.clippedOverflowRect.isEmpty())
        return { };

    auto result = computeRects(localRects, repaintContainer, visibleRectContextForRepaint());
    if (result.outlineBoundsRect)
        result.outlineBoundsRect = LayoutRect(snapRectToDevicePixels(*result.outlineBoundsRect, document().deviceScaleFactor()));

    return result;
}

LayoutRect RenderObject::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    auto repaintRects = localRectsForRepaint(RepaintOutlineBounds::No);
    if (repaintRects.clippedOverflowRect.isEmpty())
        return { };

    return computeRects(repaintRects, repaintContainer, context).clippedOverflowRect;
}

auto RenderObject::computeRects(const RepaintRects& rects, const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const -> RepaintRects
{
    auto result = computeVisibleRectsInContainer(rects, repaintContainer, context);
    RELEASE_ASSERT(result);
    return *result;
}

FloatRect RenderObject::computeFloatRectForRepaint(const FloatRect& rect, const RenderLayerModelObject* repaintContainer) const
{
    auto result = computeFloatVisibleRectInContainer(rect, repaintContainer, visibleRectContextForRepaint());
    RELEASE_ASSERT(result);
    return *result;
}

auto RenderObject::computeVisibleRectsInContainer(const RepaintRects& rects, const RenderLayerModelObject* container, VisibleRectContext context) const -> std::optional<RepaintRects>
{
    if (container == this)
        return rects;

    CheckedPtr parent = this->parent();
    if (!parent)
        return rects;

    auto adjustedRects = rects;
    if (parent->hasNonVisibleOverflow()) {
        bool isEmpty = !downcast<RenderLayerModelObject>(*parent).applyCachedClipAndScrollPosition(adjustedRects, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRects;
        }
    }
    return parent->computeVisibleRectsInContainer(adjustedRects, container, context);
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
    stream << "(B)lock/(I)nline Box/(A)tomic inline, (A)bsolute/Fi(X)ed/(R)elative/Stic(K)y, (F)loating, (O)verflow clip, Anon(Y)mous/(P)seudo, has(L)ayer, (C)omposited, Content-visibility:(H)idden/(A)uto, (S)kipped content, (M)odern/(L)egacy/Not(-)applicable layout, (+)Needs style recalc, (+)Needs layout";
    stream.nextLine();
}

void RenderObject::showNodeTreeForThis() const
{
    if (RefPtr node = this->node())
        node->showTreeForThis();
}

void RenderObject::showRenderTreeForThis() const
{
    CheckedPtr root = this;
    while (root->parent())
        root = root->parent();
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputRenderTreeLegend(stream);
    root->outputRenderSubTreeAndMark(stream, this, 1);
    WTFLogAlways("%s", stream.release().utf8().data());
}

void RenderObject::showLineTreeForThis() const
{
    auto* blockFlow = dynamicDowncast<RenderBlockFlow>(*this);
    if (!blockFlow)
        return;
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputRenderTreeLegend(stream);
    outputRenderObject(stream, false, 1);
    blockFlow->outputLineTreeAndMark(stream, nullptr, 2);
    WTFLogAlways("%s", stream.release().utf8().data());
}

static const RenderFragmentedFlow* enclosingFragmentedFlowFromRenderer(const RenderObject* renderer)
{
    if (!renderer)
        return nullptr;

    if (renderer->fragmentedFlowState() == RenderObject::FragmentedFlowState::NotInsideFlow)
        return nullptr;

    if (auto* block = dynamicDowncast<RenderBlock>(*renderer))
        return block->cachedEnclosingFragmentedFlow();

    return nullptr;
}

void RenderObject::outputRegionsInformation(TextStream& stream) const
{
    if (CheckedPtr renderFlagmentedFlow = dynamicDowncast<RenderFragmentedFlow>(*this)) {
        auto fragmentContainers = renderFlagmentedFlow->renderFragmentContainerList();

        stream << " [fragment containers ";
        bool first = true;
        for (const auto& fragment : fragmentContainers) {
            if (!first)
                stream << ", ";
            first = false;
            stream << &fragment;
        }
        stream << "]";
    }

    CheckedPtr fragmentedFlow = enclosingFragmentedFlowFromRenderer(this);

    if (!fragmentedFlow) {
        // Only the boxes have region range information.
        // Try to get the flow thread containing block information
        // from the containing block of this box.
        if (is<RenderBox>(*this))
            fragmentedFlow = enclosingFragmentedFlowFromRenderer(checkedContainingBlock().get());
    }

    if (!fragmentedFlow)
        return;

    auto* box = dynamicDowncast<RenderBox>(*this);
    if (!box)
        return;

    RenderFragmentContainer* startContainer = nullptr;
    RenderFragmentContainer* endContainer = nullptr;
    fragmentedFlow->getFragmentRangeForBox(*box, startContainer, endContainer);
    stream << " [spans fragment containers in flow " << fragmentedFlow.get() << " from " << startContainer << " to " << endContainer << "]";
}

void RenderObject::outputRenderObject(TextStream& stream, bool mark, int depth) const
{
    if (isNonReplacedAtomicInlineLevelBox())
        stream << "A";
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
            if (downcast<RenderElement>(*this).isAbsolutelyPositioned())
                stream << "A";
            else
                stream << "X";
        }
    } else
        stream << "-";

    stream << (isFloating() ? "F" : "-");

    stream << (hasNonVisibleOverflow() ? "O" : "-");

    if (isAnonymous())
        stream << "Y";
    else if (isPseudoElement())
        stream << "P";
    else
        stream << "-";

    stream << (hasLayer() ? "L" : "-");

    stream << (isComposited() ? "C" : "-");

    auto contentVisibility = style().contentVisibility();
    if (contentVisibility == ContentVisibility::Hidden)
        stream << "H";
    else if (contentVisibility == ContentVisibility::Auto)
        stream << "A";
    else
        stream << "-";

    stream << (isSkippedContent() ? "S" : "-");

    if (CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(*this); renderBlock && renderBlock->createsNewFormattingContext()) {
        if (CheckedPtr blockBox = dynamicDowncast<RenderBlockFlow>(*renderBlock))
            stream << (blockBox->childrenInline() && LayoutIntegration::canUseForLineLayout(*blockBox) ? "M" : "L");
        else if (CheckedPtr flexBox = dynamicDowncast<RenderFlexibleBox>(*renderBlock))
            stream << (LayoutIntegration::canUseForFlexLayout(*flexBox) ? "M" : "L");
        else
            stream << "L";
    } else
        stream << "-";

    stream << " ";

    stream << (node() && node()->needsStyleRecalc() ? "+" : "-");

    stream << (needsLayout() ? "+" : "-");

    int printedCharacters = 0;
    if (mark) {
        stream << "*";
        ++printedCharacters;
    }

    while (++printedCharacters <= depth * 2)
        stream << " ";

    if (node())
        stream << node()->nodeName().utf8().data() << " ";

    ASCIILiteral name = renderName();
    StringView nameView { name };
    // FIXME: Renderer's name should not include property value listing.
    int pos = nameView.find('(');
    if (pos > 0)
        stream << nameView.left(pos - 1);
    else
        stream << nameView;

    if (style().pseudoElementType())
        stream << " (::" << *style().pseudoElementType() << ")";

    if (auto* renderBox = dynamicDowncast<RenderBox>(*this)) {
        FloatRect boxRect = renderBox->frameRect();
        if (renderBox->isInFlowPositioned())
            boxRect.move(renderBox->offsetForInFlowPosition());
        stream << " " << boxRect;
    } else if (auto* renderSVGModelObject = dynamicDowncast<RenderSVGModelObject>(*this)) {
        ASSERT(!renderSVGModelObject->isInFlowPositioned());
        stream << " " << renderSVGModelObject->frameRectEquivalent();
    } else if (auto* renderInline = dynamicDowncast<RenderInline>(*this); renderInline && isInFlowPositioned()) {
        FloatSize inlineOffset = renderInline->offsetForInFlowPosition();
        stream << "  (" << inlineOffset.width() << ", " << inlineOffset.height() << ")";
    }

    stream << " renderer (" << this << ")";
    stream << " layout box (" << layoutBox() << ")";

    if (node())
        stream << " node (" << node() << ")";

    if (auto* renderText = dynamicDowncast<RenderText>(*this)) {
        auto value = renderText->text();
        stream << " length->(" << value.length() << ")";

        value = makeStringByReplacingAll(value, '\\', "\\\\"_s);
        value = makeStringByReplacingAll(value, '\n', "\\n"_s);

        const int maxPrintedLength = 80;
        if (value.length() > maxPrintedLength) {
            auto substring = StringView(value).left(maxPrintedLength);
            stream << " \"" << substring.utf8().data() << "\"...";
        } else
            stream << " \"" << value.utf8().data() << "\"";
    }

    if (auto* renderer = dynamicDowncast<RenderBoxModelObject>(*this)) {
        if (renderer->continuation())
            stream << " continuation->(" << renderer->continuation() << ")";
    }

    if (auto* box = dynamicDowncast<RenderBox>(*this)) {
        if (box->hasRenderOverflow()) {
            auto layoutOverflow = box->layoutOverflowRect();
            stream << " (layout overflow " << layoutOverflow.x() << "," << layoutOverflow.y() << " " << layoutOverflow.width() << "x" << layoutOverflow.height() << ")";
            
            if (box->hasVisualOverflow()) {
                auto visualOverflow = box->visualOverflowRect();
                stream << " (visual overflow " << visualOverflow.x() << "," << visualOverflow.y() << " " << visualOverflow.width() << "x" << visualOverflow.height() << ")";
            }
        }
    }

    if (auto* renderSVGModelObject = dynamicDowncast<RenderSVGModelObject>(*this)) {
        if (renderSVGModelObject->hasVisualOverflow()) {
            auto visualOverflow = renderSVGModelObject->visualOverflowRectEquivalent();
            stream << " (visual overflow " << visualOverflow.x() << "," << visualOverflow.y() << " " << visualOverflow.width() << "x" << visualOverflow.height() << ")";
        }
    }

    if (auto* multicolSet = dynamicDowncast<RenderMultiColumnSet>(*this))
        stream << " (column count " << multicolSet->computedColumnCount() << ", size " << multicolSet->computedColumnWidth() << "x" << multicolSet->computedColumnHeight() << ", gap " << multicolSet->columnGap() << ")";

    outputRegionsInformation(stream);

    if (needsLayout()) {
        stream << " layout->";
        if (selfNeedsLayout())
            stream << "[self]";
        if (normalChildNeedsLayout())
            stream << "[normal child]";
        if (outOfFlowChildNeedsLayout())
            stream << "[out-of-flow child]";
        if (needsSimplifiedNormalFlowLayout())
            stream << "[simplified]";
        if (needsOutOfFlowMovementLayout())
            stream << "[out-of-flow movement]";
        if (outOfFlowChildNeedsStaticPositionLayout())
            stream << "[out of flow child needs parent layout]";
    }

    if (RefPtr element = dynamicDowncast<Element>(node()))
        stream << element->attributesForDescription();

    stream.nextLine();
}

void RenderObject::outputRenderSubTreeAndMark(TextStream& stream, const RenderObject* markedObject, int depth) const
{
    outputRenderObject(stream, markedObject == this, depth);

    if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(*this)) {
        blockFlow->outputFloatingObjects(stream, depth + 1);
        blockFlow->outputLineTreeAndMark(stream, nullptr, depth + 1);
    }

    for (CheckedPtr child = firstChildSlow(); child; child = child->nextSibling())
        child->outputRenderSubTreeAndMark(stream, markedObject, depth + 1);
}

#endif // NDEBUG

FloatPoint RenderObject::localToAbsolute(const FloatPoint& localPoint, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(nullptr, transformState, mode | ApplyContainerFlip, wasFixed);
    
    return transformState.mappedPoint();
}

// https://drafts.csswg.org/css-view-transitions-1/#capture-old-state-algorithm
// "a <transform-function> that would map element’s border box from the snapshot containing block origin to its current visual position."
TransformState RenderObject::viewTransitionTransform() const
{
    TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint { });
    OptionSet<MapCoordinatesMode> mode { UseTransforms, ApplyContainerFlip };
    mapLocalToContainer(nullptr, transformState, mode, nullptr);
    return transformState;
}

// FIXME: Should this exist? It is lossy but not obvious when called.
FloatPoint RenderObject::absoluteToLocal(const DoublePoint& containerPoint, OptionSet<MapCoordinatesMode> mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, FloatPoint(containerPoint));
    mapAbsoluteToLocalPoint(mode, transformState);

    return transformState.mappedPoint();
}

FloatQuad RenderObject::absoluteToLocalQuad(const FloatQuad& quad, OptionSet<MapCoordinatesMode> mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, quad.boundingBox().center(), quad);
    mapAbsoluteToLocalPoint(mode, transformState);
    return transformState.mappedQuad();
}

void RenderObject::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (ancestorContainer == this)
        return;

    CheckedPtr parent = this->parent();
    if (!parent)
        return;

    // FIXME: this should call offsetFromContainer to share code, but I'm not sure it's ever called.
    LayoutPoint centerPoint(transformState.mappedPoint());
    if (auto* parentAsBox = dynamicDowncast<RenderBox>(*parent)) {
        if (mode.contains(ApplyContainerFlip)) {
            if (parentAsBox->writingMode().isBlockFlipped())
                transformState.move(parentAsBox->flipForWritingMode(LayoutPoint(transformState.mappedPoint())) - centerPoint);
            mode.remove(ApplyContainerFlip);
        }
        transformState.move(-toLayoutSize(parentAsBox->scrollPosition()));
    }

    parent->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

void RenderObject::mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode> mode, TransformState& transformState) const
{
    if (CheckedPtr parent = this->parent()) {
        parent->mapAbsoluteToLocalPoint(mode, transformState);
        if (auto* box = dynamicDowncast<RenderBox>(*parent))
            transformState.move(toLayoutSize(box->scrollPosition()));
    }
}

bool RenderObject::shouldUseTransformFromContainer(const RenderElement* containerObject) const
{
    if (isTransformed())
        return true;
    if (hasLayer() && downcast<RenderLayerModelObject>(*this).layer()->anchorScrollAdjustment())
        return true;
    if (containerObject && containerObject->style().hasPerspective())
        return containerObject == parent();
    return false;
}

// FIXME: Now that it's no longer passed a container maybe this should be renamed?
void RenderObject::getTransformFromContainer(const LayoutSize& offsetInContainer, TransformationMatrix& transform) const
{
    transform.makeIdentity();
    transform.translate(offsetInContainer.width(), offsetInContainer.height());
    CheckedPtr<RenderLayer> layer;
    if (hasLayer() && (layer = downcast<RenderLayerModelObject>(*this).layer()) && layer->transform())
        transform.multiply(layer->currentTransform());

    CheckedPtr perspectiveObject = parent();

    if (perspectiveObject && perspectiveObject->hasLayer() && perspectiveObject->style().hasPerspective()) {
        // Perpsective on the container affects us, so we have to factor it in here.
        ASSERT(perspectiveObject->hasLayer());
        FloatPoint perspectiveOrigin = downcast<RenderLayerModelObject>(*perspectiveObject).layer()->perspectiveOrigin();

        TransformationMatrix perspectiveMatrix;
        perspectiveMatrix.applyPerspective(perspectiveObject->style().usedPerspective());
        
        transform.translateRight3d(-perspectiveOrigin.x(), -perspectiveOrigin.y(), 0);
        transform = perspectiveMatrix * transform;
        transform.translateRight3d(perspectiveOrigin.x(), perspectiveOrigin.y(), 0);
    }
}

void RenderObject::pushOntoTransformState(TransformState& transformState, OptionSet<MapCoordinatesMode> mode, const RenderLayerModelObject* repaintContainer, const RenderElement* container, const LayoutSize& offsetInContainer, bool containerSkipped) const
{
    bool preserve3D = mode.contains(UseTransforms) && participatesInPreserve3D();
    if (mode.contains(UseTransforms) && shouldUseTransformFromContainer(container)) {
        TransformationMatrix matrix;
        getTransformFromContainer(offsetInContainer, matrix);
        transformState.applyTransform(matrix, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(offsetInContainer.width(), offsetInContainer.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);

    if (containerSkipped) {
        // There can't be a transform between repaintContainer and container, because transforms create containers, so it should be safe
        // to just subtract the delta between the repaintContainer and container.
        LayoutSize containerOffset = repaintContainer->offsetFromAncestorContainer(*container);
        transformState.move(-containerOffset.width(), -containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    }
}

FloatQuad RenderObject::localToContainerQuad(const FloatQuad& localQuad, const RenderLayerModelObject* container, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    // Track the point at the center of the quad's bounding box. As mapLocalToContainer() calls offsetFromContainer(),
    // it will use that point as the reference point to decide which column's transform to apply in multiple-column blocks.
    TransformState transformState(TransformState::ApplyTransformDirection, localQuad.boundingBox().center(), localQuad);
    mapLocalToContainer(container, transformState, mode | ApplyContainerFlip, wasFixed);
    
    return transformState.mappedQuad();
}

FloatPoint RenderObject::localToContainerPoint(const FloatPoint& localPoint, const RenderLayerModelObject* container, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, localPoint);
    mapLocalToContainer(container, transformState, mode | ApplyContainerFlip, wasFixed);

    return transformState.mappedPoint();
}

LayoutSize RenderObject::offsetFromContainer(const RenderElement& container, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    ASSERT(&container == this->container());

    LayoutSize offset;
    if (auto* box = dynamicDowncast<RenderBox>(container))
        offset -= toLayoutSize(box->scrollPosition());

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = is<RenderFragmentedFlow>(container);

    return offset;
}

LayoutSize RenderObject::offsetFromAncestorContainer(const RenderElement& container) const
{
    LayoutSize offset;
    LayoutPoint referencePoint;
    CheckedPtr currentContainer = this;
    do {
        CheckedPtr nextContainer = currentContainer->container();
        ASSERT(nextContainer);  // This means we reached the top without finding container.
        if (!nextContainer)
            break;
        ASSERT(!currentContainer->isTransformed());
        LayoutSize currentOffset = currentContainer->offsetFromContainer(*nextContainer, referencePoint);
        offset += currentOffset;
        referencePoint.move(currentOffset);
        currentContainer = WTFMove(nextContainer);
    } while (currentContainer != &container);

    return offset;
}

bool RenderObject::participatesInPreserve3D() const
{
    return hasLayer() && downcast<RenderLayerModelObject>(*this).layer()->participatesInPreserve3D();
}

HostWindow* RenderObject::hostWindow() const
{
    return view().frameView().root() ? view().frameView().root()->hostWindow() : nullptr;
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
    // This does mean that computeOutOfFlowPositionedLogicalWidth and computeOutOfFlowPositionedLogicalHeight have to use container().
    // FIXME: See https://bugs.webkit.org/show_bug.cgi?id=270977 for RenderLineBreak special treatment.
    if (!is<RenderElement>(renderer) || is<RenderText>(renderer) || is<RenderLineBreak>(renderer))
        return renderer.parent();

    auto* renderElement = dynamicDowncast<RenderElement>(renderer);
    if (!renderElement) {
        ASSERT_NOT_REACHED();
        return renderer.parent();
    }
    auto updateRepaintContainerSkippedFlagIfApplicable = [&] {
        if (!repaintContainerSkipped)
            return;
        *repaintContainerSkipped = false;
        if (repaintContainer == &renderElement->view())
            return;
        for (auto& ancestor : ancestorsOfType<RenderElement>(*renderElement)) {
            if (repaintContainer == &ancestor) {
                *repaintContainerSkipped = true;
                break;
            }
        }
    };
    if (isInTopLayerOrBackdrop(renderElement->style(), renderElement->element())) {
        updateRepaintContainerSkippedFlagIfApplicable();
        return &renderElement->view();
    }
    auto position = renderElement->style().position();
    if (position == PositionType::Static || position == PositionType::Relative || position == PositionType::Sticky)
        return renderElement->parent();
    CheckedPtr parent = renderElement->parent();
    if (position == PositionType::Absolute) {
        for (; parent && !parent->canContainAbsolutelyPositionedObjects(); parent = parent->parent()) {
            if (repaintContainerSkipped && repaintContainer == parent)
                *repaintContainerSkipped = true;
        }
        return parent.unsafeGet();
    }
    for (; parent && !parent->canContainFixedPositionObjects(); parent = parent->parent()) {
        if (isInTopLayerOrBackdrop(parent->style(), parent->element())) {
            updateRepaintContainerSkippedFlagIfApplicable();
            return &renderElement->view();
        }
        if (repaintContainerSkipped && repaintContainer == parent)
            *repaintContainerSkipped = true;
    }
    return parent.unsafeGet();
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

bool RenderObject::setCapturedInViewTransition(bool captured)
{
    if (capturedInViewTransition() == captured)
        return false;

    m_stateBitfields.setFlag(StateFlag::CapturedInViewTransition, captured);

    CheckedPtr<RenderLayer> layerToInvalidate;
    if (isDocumentElementRenderer()) {
        layerToInvalidate = view().layer();
        view().compositor().setRootElementCapturedInViewTransition(captured);
    } else if (hasLayer())
        layerToInvalidate = downcast<RenderLayerModelObject>(*this).layer();

    if (layerToInvalidate) {
        layerToInvalidate->setNeedsPostLayoutCompositingUpdate();
        layerToInvalidate->setNeedsCompositingConfigurationUpdate();

        // Invalidate transform applied by `RenderLayerBacking::updateTransform`.
        layerToInvalidate->setNeedsCompositingGeometryUpdate();
    }

    if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(*this))
        renderBox->invalidateAncestorBackgroundObscurationStatus();

    if (CheckedPtr layerModelRenderer = dynamicDowncast<RenderLayerModelObject>(this)) {
        if (RefPtr activeViewTransition = document().activeViewTransition()) {
            if (CheckedPtr viewTransitionCapture = activeViewTransition->viewTransitionNewPseudoForCapturedElement(*layerModelRenderer)) {
                if (viewTransitionCapture->hasLayer())
                    viewTransitionCapture->layer()->setNeedsCompositingLayerConnection();
            }
        }
    }

    return true;
}

void RenderObject::willBeDestroyed()
{
    ASSERT(!m_parent);
    ASSERT(renderTreeBeingDestroyed() || !is<RenderElement>(*this) || !view().frameView().hasSlowRepaintObject(downcast<RenderElement>(*this)));

    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->remove(*this);

    setCapturedInViewTransition(false);

    if (RefPtr node = this->node()) {
        // FIXME: Continuations should be anonymous.
        ASSERT(!node->renderer() || node->renderer() == this || (is<RenderElement>(*this) && downcast<RenderElement>(*this).isContinuation()));
        if (node->renderer() == this)
            node->setRenderer({ });
    }

    checkedView()->willDestroyRenderer();

    removeRareData();
}

void RenderObject::insertedIntoTree()
{
    // FIXME: We should ASSERT(isRooted()) here but generated content makes some out-of-order insertion.
    if (!isFloating() && parent()->isSVGRenderer() && parent()->childrenInline())
        checkedParent()->dirtyLineFromChangedChild();
}

void RenderObject::willBeRemovedFromTree()
{
    // FIXME: We should ASSERT(isRooted()) but we have some out-of-order removals which would need to be fixed first.
    // Update cached boundaries in SVG renderers, if a child is removed.
    checkedParent()->invalidateCachedBoundaries();
}

void RenderObject::destroy()
{
    RELEASE_ASSERT(!m_parent);
    RELEASE_ASSERT(!m_next);
    RELEASE_ASSERT(!m_previous);
    RELEASE_ASSERT(!m_stateBitfields.hasFlag(StateFlag::BeingDestroyed));

    setIsBeingDestroyed();

    willBeDestroyed();

    if (auto* widgetRenderer = dynamicDowncast<RenderWidget>(*this)) {
        widgetRenderer->deref();
        return;
    }
    delete this;
}

Position RenderObject::positionForPoint(const LayoutPoint& point, HitTestSource source)
{
    return positionForPoint(point, source, nullptr).position();
}

PositionWithAffinity RenderObject::positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*)
{
    return createPositionWithAffinity(caretMinOffset(), Affinity::Downstream);
}

VisiblePosition RenderObject::visiblePositionForPoint(const LayoutPoint& point, HitTestSource source)
{
    auto positionWithAffinity = positionForPoint(point, source, nullptr);
    return VisiblePosition(positionWithAffinity.position(), positionWithAffinity.affinity());
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

RefPtr<Node> RenderObject::protectedNodeForHitTest() const
{
    return nodeForHitTest();
}

void RenderObject::updateHitTestResult(HitTestResult& result, const LayoutPoint& point) const
{
    if (result.innerNode())
        return;

    if (RefPtr node = nodeForHitTest()) {
        result.setInnerNode(node.get());
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node.get());
        result.setLocalPoint(point);
        result.setPseudoElementIdentifier(style().pseudoElementIdentifier());
    }
}

bool RenderObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& /*locationInContainer*/, const LayoutPoint& /*accumulatedOffset*/, HitTestAction)
{
    return false;
}

int RenderObject::caretMinOffset() const
{
    return 0;
}

int RenderObject::caretMaxOffset() const
{
    if (isBlockLevelReplacedOrAtomicInline())
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

PositionWithAffinity RenderObject::createPositionWithAffinity(int offset, Affinity affinity) const
{
    // If this is a non-anonymous renderer in an editable area, then it's simple.
    if (RefPtr node = nonPseudoNode()) {
        if (!node->hasEditableStyle()) {
            // If it can be found, we prefer a visually equivalent position that is editable. 
            Position position = makeDeprecatedLegacyPosition(node.get(), offset);
            Position candidate = position.downstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->hasEditableStyle())
                return PositionWithAffinity(candidate, affinity);
            candidate = position.upstream(CanCrossEditingBoundary);
            if (candidate.deprecatedNode()->hasEditableStyle())
                return PositionWithAffinity(candidate, affinity);
        }
        // FIXME: Eliminate legacy editing positions
        return PositionWithAffinity(makeDeprecatedLegacyPosition(node.get(), offset), affinity);
    }

    // We don't want to cross the boundary between editable and non-editable
    // regions of the document, but that is either impossible or at least
    // extremely unlikely in any normal case because we stop as soon as we
    // find a single non-anonymous renderer.

    // Find a nearby non-anonymous renderer.
    CheckedPtr child = this;
    while (CheckedPtr parent = child->parent()) {
        // Find non-anonymous content after.
        CheckedPtr renderer = child;
        while ((renderer = renderer->nextInPreOrder(parent.get()))) {
            if (RefPtr node = renderer->nonPseudoNode())
                return PositionWithAffinity(firstPositionInOrBeforeNode(node.get()));
        }

        // Find non-anonymous content before.
        renderer = child;
        while ((renderer = renderer->previousInPreOrder())) {
            if (renderer == parent)
                break;
            if (RefPtr node = renderer->nonPseudoNode())
                return PositionWithAffinity(lastPositionInOrAfterNode(node.get()));
        }

        // Use the parent itself unless it too is anonymous.
        if (RefPtr element = parent->nonPseudoElement())
            return PositionWithAffinity(firstPositionInOrBeforeNode(element.get()));

        // Repeat at the next level up.
        child = WTFMove(parent);
    }

    // Everything was anonymous. Give up.
    return PositionWithAffinity();
}

PositionWithAffinity RenderObject::createPositionWithAffinity(const Position& position) const
{
    if (position.isNotNull())
        return PositionWithAffinity(position);

    ASSERT(!node());
    return createPositionWithAffinity(0, Affinity::Downstream);
}

const RenderStyle& RenderObject::outlineStyleForRepaint() const
{
    return style();
}

CursorDirective RenderObject::getCursor(const LayoutPoint&, Cursor&) const
{
    return SetCursorBasedOnStyle;
}

bool RenderObject::useDarkAppearance() const
{
    return document().useDarkAppearance(&style());
}

OptionSet<StyleColorOptions> RenderObject::styleColorOptions() const
{
    return document().styleColorOptions(&style());
}

void RenderObject::setSelectionState(HighlightState state)
{
    m_stateBitfields.setSelectionState(state);
}

bool RenderObject::canUpdateSelectionOnRootLineBoxes()
{
    if (needsLayout())
        return false;

    CheckedPtr containingBlock = this->containingBlock();
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

void RenderObject::setNeedsBoundariesUpdate()
{
}

void RenderObject::invalidateCachedBoundaries()
{
    for (CheckedPtr renderer = this; renderer && renderer->isSVGRenderer(); renderer = renderer->parent()) {
        if (renderer->usesBoundaryCaching()) {
            renderer->setNeedsBoundariesUpdate();
            break;
        }
    }
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
FloatRect RenderObject::repaintRectInLocalCoordinates(RepaintRectCalculation) const
{
    ASSERT_NOT_REACHED();
    return FloatRect();
}

AffineTransform RenderObject::localTransform() const
{
    return AffineTransform();
}

const AffineTransform& RenderObject::localToParentTransform() const
{
    return identity;
}

bool RenderObject::nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint&, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

RenderFragmentedFlow* RenderObject::locateEnclosingFragmentedFlow() const
{
    CheckedPtr containingBlock = this->containingBlock();
    return containingBlock ? containingBlock->enclosingFragmentedFlow() : nullptr;
}

void RenderObject::setHasReflection(bool hasReflection)
{
    if (hasReflection || hasRareData())
        ensureRareData().hasReflection = hasReflection;
}

void RenderObject::setHasOutlineAutoAncestor(bool hasOutlineAutoAncestor)
{
    if (hasOutlineAutoAncestor || hasRareData())
        ensureRareData().hasOutlineAutoAncestor = hasOutlineAutoAncestor;
}

void RenderObject::markIsYouTubeReplacement()
{
    ensureRareData().isYouTubeReplacement = true;
}

RenderObject::RareDataMap& RenderObject::rareDataMap()
{
    static NeverDestroyed<RareDataMap> map;
    return map;
}

const RenderObject::RenderObjectRareData& RenderObject::rareData() const
{
    ASSERT(hasRareData());
    return *rareDataMap().get(*this);
}

RenderObject::RenderObjectRareData& RenderObject::ensureRareData()
{
    m_stateBitfields.setFlag(StateFlag::HasRareData);
    return *rareDataMap().ensure(*this, [] { return makeUnique<RenderObjectRareData>(); }).iterator->value;
}

void RenderObject::removeRareData()
{
    if (!hasRareData())
        return;
    rareDataMap().remove(*this);
    m_stateBitfields.clearFlag(StateFlag::HasRareData);
}

RenderObject::RenderObjectRareData::RenderObjectRareData() = default;
RenderObject::RenderObjectRareData::~RenderObjectRareData() = default;

bool RenderObject::hasEmptyVisibleRectRespectingParentFrames() const
{
    auto enclosingFrameRenderer = [] (const RenderObject& renderer) {
        auto* ownerElement = renderer.document().ownerElement();
        return ownerElement ? ownerElement->renderer() : nullptr;
    };

    auto hasEmptyVisibleRect = [] (const RenderObject& renderer) {
        VisibleRectContext context {
            .hasPositionFixedDescendant = false,
            .dirtyRectIsFlipped = false,
            .descendantNeedsEnclosingIntRect = false,
            .options = { VisibleRectContext::Option::UseEdgeInclusiveIntersection, VisibleRectContext::Option::ApplyCompositedClips },
            .scrollMargin = { }
        };
        CheckedRef box = renderer.enclosingBoxModelObject();
        auto clippedBounds = box->computeVisibleRectsInContainer({ box->borderBoundingBox() }, &box->view(), context);
        return !clippedBounds || clippedBounds->clippedOverflowRect.isEmpty();
    };

    for (CheckedPtr renderer = this; renderer; renderer = enclosingFrameRenderer(*renderer)) {
        if (hasEmptyVisibleRect(*renderer))
            return true;
    }

    return false;
}

Vector<FloatQuad> RenderObject::absoluteTextQuads(const SimpleRange& range, OptionSet<RenderObject::BoundingRectBehavior> behavior)
{
    Vector<FloatQuad> quads;
    for (Ref node : intersectingNodes(range)) {
        CheckedPtr renderer = node->renderer();
        if (!renderer)
            continue;
        if (auto* lineBreakRenderer = dynamicDowncast<RenderLineBreak>(*renderer); lineBreakRenderer && lineBreakRenderer->isBR())
            lineBreakRenderer->absoluteQuads(quads);
        else if (auto* renderText = dynamicDowncast<RenderText>(*renderer)) {
            auto offsetRange = characterDataOffsetRange(range, downcast<CharacterData>(node.get()));
            quads.appendVector(renderText->absoluteQuadsForRange(offsetRange.start, offsetRange.end, behavior));
        }
    }
    return quads;
}

static Vector<FloatRect> absoluteRectsForRangeInText(const SimpleRange& range, Text& node, OptionSet<RenderObject::BoundingRectBehavior> behavior)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return { };

    auto offsetRange = characterDataOffsetRange(range, node);
    // Move to surrogate pair start for Range start and past surrogate pair end for Range end in case the trailing surrogate is indexed.
    if (offsetRange.start < node.data().length() && offsetRange.start && U16_IS_TRAIL(node.data()[offsetRange.start]) && U16_IS_LEAD(node.data()[offsetRange.start - 1]))
        offsetRange.start--;
    if (offsetRange.end < node.data().length() && offsetRange.end && U16_IS_TRAIL(node.data()[offsetRange.end]) && U16_IS_LEAD(node.data()[offsetRange.end - 1]))
        offsetRange.end++;
    auto textQuads = renderer->absoluteQuadsForRange(offsetRange.start, offsetRange.end, behavior);

    if (behavior.contains(RenderObject::BoundingRectBehavior::RespectClipping)) {
        auto absoluteClippedOverflowRect = renderer->absoluteClippedOverflowRectForRepaint();
        return WTF::compactMap(textQuads, [&](auto& quad) -> std::optional<FloatRect> {
            auto clippedRect = intersection(quad.boundingBox(), absoluteClippedOverflowRect);
            if (!clippedRect.isEmpty())
                return clippedRect;
            return std::nullopt;
        });
    }

    return boundingBoxes(textQuads);
}

// FIXME: This should return Vector<FloatRect> like the other similar functions.
// FIXME: Find a way to share with absoluteTextQuads rather than repeating so much of the logic from that function.
Vector<IntRect> RenderObject::absoluteTextRects(const SimpleRange& range, OptionSet<BoundingRectBehavior> behavior)
{
    ASSERT(!behavior.contains(BoundingRectBehavior::UseVisibleBounds));
    ASSERT(!behavior.contains(BoundingRectBehavior::IgnoreTinyRects));
    Vector<LayoutRect> rects;
    for (Ref node : intersectingNodes(range)) {
        CheckedPtr renderer = node->renderer();
        if (auto* lineBreakRenderer = dynamicDowncast<RenderLineBreak>(renderer.get()); lineBreakRenderer && lineBreakRenderer->isBR())
            lineBreakRenderer->boundingRects(rects, flooredLayoutPoint(renderer->localToAbsolute()));
        else if (auto* textNode = dynamicDowncast<Text>(node.get())) {
            for (auto& rect : absoluteRectsForRangeInText(range, *textNode, behavior))
                rects.append(LayoutRect { rect });
        }
    }

    return WTF::map(rects, [](auto& layoutRect) {
        return enclosingIntRect(layoutRect);
    });
}

static RefPtr<Node> nodeAfter(const BoundaryPoint& point)
{
    if (RefPtr node = point.container->traverseToChildAt(point.offset + 1))
        return node;
    return point.container.ptr();
}

enum class CoordinateSpace { Client, Absolute };

static Vector<FloatRect> borderAndTextRects(const SimpleRange& range, CoordinateSpace space, OptionSet<RenderObject::BoundingRectBehavior> behavior)
{
    Vector<FloatRect> rects;

    range.start.protectedDocument()->updateLayoutIgnorePendingStylesheets();

    bool useVisibleBounds = behavior.contains(RenderObject::BoundingRectBehavior::UseVisibleBounds);

    HashSet<RefPtr<Element>> selectedElementsSet;
    for (Ref node : intersectingNodesWithDeprecatedZeroOffsetStartQuirk(range)) {
        if (RefPtr element = dynamicDowncast<Element>(WTFMove(node)))
            selectedElementsSet.add(element.releaseNonNull());
    }

    // Don't include elements at the end of the range that are only partially selected.
    // FIXME: What about the start of the range? The asymmetry here does not make sense. Seems likely this logic is not quite right in other respects, too.
    if (RefPtr lastNode = nodeAfter(range.end)) {
        for (auto& ancestor : lineageOfType<Element>(*lastNode))
            selectedElementsSet.remove(&ancestor);
    }

    for (Ref node : intersectingNodesWithDeprecatedZeroOffsetStartQuirk(range)) {
        auto* element = dynamicDowncast<Element>(node.get());
        if (element && selectedElementsSet.contains(element) && (useVisibleBounds || !node->parentElement() || !selectedElementsSet.contains(node->parentElement()))) {
            if (CheckedPtr renderer = element->renderBoxModelObject()) {
                if (useVisibleBounds) {
                    auto localBounds = renderer->borderBoundingBox();
                    auto rootClippedBounds = renderer->computeVisibleRectsInContainer(
                        { localBounds },
                        renderer->checkedView().ptr(),
                        {
                            .hasPositionFixedDescendant = false,
                            .dirtyRectIsFlipped = false,
                            .descendantNeedsEnclosingIntRect = false,
                            .options = {
                                VisibleRectContext::Option::UseEdgeInclusiveIntersection,
                                VisibleRectContext::Option::ApplyCompositedClips,
                                VisibleRectContext::Option::ApplyCompositedContainerScrolls
                            },
                            .scrollMargin = { }
                        }
                    );
                    if (!rootClippedBounds)
                        continue;
                    auto snappedBounds = snapRectToDevicePixels(rootClippedBounds->clippedOverflowRect, node->document().deviceScaleFactor());
                    if (space == CoordinateSpace::Client)
                        node->protectedDocument()->convertAbsoluteToClientRect(snappedBounds, renderer->style());
                    rects.append(snappedBounds);
                    continue;
                }

                Vector<FloatQuad> elementQuads;
                renderer->absoluteQuads(elementQuads);
                if (space == CoordinateSpace::Client)
                    node->protectedDocument()->convertAbsoluteToClientQuads(elementQuads, renderer->style());
                rects.appendVector(boundingBoxes(elementQuads));
            }
        } else if (auto* textNode = dynamicDowncast<Text>(node.get())) {
            if (CheckedPtr renderer = textNode->renderer()) {
                auto clippedRects = absoluteRectsForRangeInText(range, *textNode, behavior);
                if (space == CoordinateSpace::Client)
                    node->protectedDocument()->convertAbsoluteToClientRects(clippedRects, renderer->style());
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

ScrollAnchoringController* RenderObject::searchParentChainForScrollAnchoringController(const RenderObject& renderer)
{
    if (renderer.hasLayer()) {
        if (auto* scrollableArea = downcast<RenderLayerModelObject>(renderer).layer()->scrollableArea()) {
            auto controller = scrollableArea->scrollAnchoringController();
            if (controller && controller->anchorElement())
                return controller;
        }
    }
    for (auto* enclosingLayer = renderer.enclosingLayer(); enclosingLayer; enclosingLayer = enclosingLayer->parent()) {
        if (RenderLayerScrollableArea* scrollableArea = enclosingLayer->scrollableArea()) {
            auto controller = scrollableArea->scrollAnchoringController();
            if (controller && controller->anchorElement())
                return controller;
        }
    }
    return renderer.view().frameView().scrollAnchoringController();
}

void RenderObject::RepaintRects::transform(const TransformationMatrix& matrix)
{
    clippedOverflowRect = matrix.mapRect(clippedOverflowRect);
    if (outlineBoundsRect)
        *outlineBoundsRect = matrix.mapRect(*outlineBoundsRect);
}

void RenderObject::RepaintRects::transform(const TransformationMatrix& matrix, float deviceScaleFactor)
{
    bool identicalRects = outlineBoundsRect && *outlineBoundsRect == clippedOverflowRect;
    clippedOverflowRect = LayoutRect(encloseRectToDevicePixels(matrix.mapRect(clippedOverflowRect), deviceScaleFactor));
    if (identicalRects)
        *outlineBoundsRect = clippedOverflowRect;
    else if (outlineBoundsRect)
        *outlineBoundsRect = LayoutRect(encloseRectToDevicePixels(matrix.mapRect(*outlineBoundsRect), deviceScaleFactor));
}

bool RenderObject::effectiveCapturedInViewTransition() const
{
    if (isDocumentElementRenderer())
        return false;
    if (isRenderView())
        return document().activeViewTransitionCapturedDocumentElement();
    return capturedInViewTransition();
}

PointerEvents RenderObject::usedPointerEvents() const
{
    if (document().renderingIsSuppressedForViewTransition() && !isDocumentElementRenderer())
        return PointerEvents::None;
    return style().usedPointerEvents();
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

static bool areOnSameLine(const SelectionGeometry& a, const SelectionGeometry& b)
{
    if (a.lineNumber() && a.lineNumber() == b.lineNumber())
        return true;

    auto quadA = a.quad();
    auto quadB = b.quad();
    return FloatQuad { quadA.p1(), quadA.p2(), quadB.p2(), quadB.p1() }.isEmpty()
        && FloatQuad { quadA.p4(), quadA.p3(), quadB.p3(), quadB.p4() }.isEmpty();
}

static bool usesVisuallyContiguousBidiTextSelection(const SimpleRange& range)
{
    return range.protectedStartContainer()->protectedDocument()->settings().visuallyContiguousBidiTextSelectionEnabled();
}

struct SelectionEndpointDirections {
    TextDirection firstLine { TextDirection::LTR };
    TextDirection lastLine { TextDirection::LTR };
    bool isSingleLine { false };
};

static SelectionEndpointDirections computeSelectionEndpointDirections(const SimpleRange& range)
{
    auto [start, end] = positionsForRange(range);
    if (inSameLine(start, end)) {
        auto direction = primaryDirectionForSingleLineRange(start, end);
        return { direction, direction, true };
    }
    return { start.primaryDirection(), end.primaryDirection(), false };
}

static void makeBidiSelectionVisuallyContiguousIfNeeded(const SelectionEndpointDirections directions, const SimpleRange& range, Vector<SelectionGeometry>& geometries)
{
    if (!range.startContainer().document().editor().shouldDrawVisuallyContiguousBidiSelection())
        return;

    FloatPoint selectionStartTop;
    FloatPoint selectionStartBottom;
    FloatPoint selectionEndTop;
    FloatPoint selectionEndBottom;

    auto [start, end] = positionsForRange(range);
    bool flipEndpointsAtStart = false;
    bool flipEndpointsAtEnd = false;

    auto anyGeometryHasSameDirectionAsLine = [&](TextDirection direction) {
        return geometries.containsIf([direction](auto& geometry) {
            return geometry.direction() == direction;
        });
    };

    auto atVisualBoundaryOfBidiRun = [](const Position& position) {
        RenderedPosition renderedPosition { position };
        return renderedPosition.atLeftBoundaryOfBidiRun() || renderedPosition.atRightBoundaryOfBidiRun();
    };

    if (geometries.size() > 1 && directions.isSingleLine && !anyGeometryHasSameDirectionAsLine(directions.firstLine)) {
        flipEndpointsAtStart = atVisualBoundaryOfBidiRun(start);
        flipEndpointsAtEnd = atVisualBoundaryOfBidiRun(end);
    }

    std::optional<SelectionGeometry> startGeometry;
    std::optional<SelectionGeometry> endGeometry;
    for (auto& geometry : geometries) {
        if (!geometry.isHorizontal())
            return;

        bool isRightToLeft = geometry.direction() == TextDirection::RTL;
        if (geometry.containsStart()) {
            selectionStartTop = flipEndpointsAtStart == isRightToLeft ? geometry.quad().p1() : geometry.quad().p2();
            selectionStartBottom = flipEndpointsAtStart == isRightToLeft ? geometry.quad().p4() : geometry.quad().p3();
            startGeometry = { geometry };
        }

        if (geometry.containsEnd()) {
            selectionEndTop = flipEndpointsAtEnd == isRightToLeft ? geometry.quad().p2() : geometry.quad().p1();
            selectionEndBottom = flipEndpointsAtEnd == isRightToLeft ? geometry.quad().p3() : geometry.quad().p4();
            endGeometry = { geometry };
        }
    }

    if (!startGeometry || !endGeometry)
        return;

    unsigned geometryCountOnFirstLine = 0;
    unsigned geometryCountOnLastLine = 0;
    IntRect selectionBoundsOnFirstLine;
    IntRect selectionBoundsOnLastLine;
    geometries.removeAllMatching([&](auto& geometry) {
        if (geometry.containsStart() || areOnSameLine(*startGeometry, geometry)) {
            selectionBoundsOnFirstLine.uniteIfNonZero(geometry.rect());
            geometryCountOnFirstLine++;
            return true;
        }

        if (geometry.containsEnd() || areOnSameLine(*endGeometry, geometry)) {
            selectionBoundsOnLastLine.uniteIfNonZero(geometry.rect());
            geometryCountOnLastLine++;
            return true;
        }

        // Keep selection geometries that lie in the interior of the selection.
        return false;
    });

    if (areOnSameLine(*startGeometry, *endGeometry)) {
        // For a single line selection, simply merge the end into the start and remove other selection geometries on the same line.
        startGeometry->setQuad({ selectionStartTop, selectionEndTop, selectionEndBottom, selectionStartBottom });
        startGeometry->setContainsEnd(true);
        geometries.append(WTFMove(*startGeometry));
        return;
    }

    auto makeSelectionQuad = [](const Position& position, const IntRect& selectionBounds, bool caretIsOnVisualLeftEdge) -> FloatQuad {
        VisiblePosition visiblePosition { position };
        RenderedPosition renderedPosition { position };
        auto boundingRect = selectionBounds;
        boundingRect.uniteIfNonZero(caretIsOnVisualLeftEdge
            ? renderedPosition.rightBoundaryOfBidiRun(0).absoluteRect(CaretRectMode::ExpandToEndOfLine)
            : renderedPosition.leftBoundaryOfBidiRun(0).absoluteRect(CaretRectMode::ExpandToEndOfLine));
        auto caretRect = visiblePosition.absoluteCaretBounds();
        auto rectOnLeftEdge = caretIsOnVisualLeftEdge ? caretRect : boundingRect;
        auto rectOnRightEdge = caretIsOnVisualLeftEdge ? boundingRect : caretRect;
        return {
            rectOnLeftEdge.minXMinYCorner(),
            rectOnRightEdge.maxXMinYCorner(),
            rectOnRightEdge.maxXMaxYCorner(),
            rectOnLeftEdge.minXMaxYCorner(),
        };
    };

    startGeometry->setDirection(directions.firstLine);
    startGeometry->setQuad(makeSelectionQuad(start, selectionBoundsOnFirstLine, directions.firstLine == TextDirection::LTR));
    endGeometry->setDirection(directions.lastLine);
    endGeometry->setQuad(makeSelectionQuad(end, selectionBoundsOnLastLine, directions.lastLine == TextDirection::RTL));
    geometries.appendList({ WTFMove(*startGeometry), WTFMove(*endGeometry) });
}

static void adjustTextDirectionForCoalescedGeometries(const SelectionEndpointDirections& directions, const SimpleRange& range, Vector<SelectionGeometry>& geometries)
{
    if (!usesVisuallyContiguousBidiTextSelection(range))
        return;

    for (auto& geometry : geometries) {
        if (geometry.containsStart())
            geometry.setDirection(directions.firstLine);
        if (geometry.containsEnd())
            geometry.setDirection(directions.lastLine);
    }
}

static bool shouldRenderSelectionOnSeparateLine(const RenderObject* currentRenderer)
{
    if (!currentRenderer)
        return false;

    if (currentRenderer->isOutOfFlowPositioned())
        return true;

    if (CheckedPtr blockFlow = dynamicDowncast<const RenderBlockFlow>(*currentRenderer))
        return blockFlow->multiColumnFlow();

    return false;
}

static bool hasAncestorWithSelectionOnSeparateLine(RenderObject* descendant, const RenderObject* stayWithin)
{
    for (CheckedPtr current = descendant; current; current = current->parent()) {
        if (current->isOutOfFlowPositioned())
            return true;
        if (current->isRenderMultiColumnFlow())
            return true;
        if (current == stayWithin)
            break;
    }
    return false;
}

static bool shouldRenderPreviousSelectionOnSeparateLine(RenderObject* previousRenderer, const RenderObject* stayWithin)
{
    if (!previousRenderer || !stayWithin)
        return false;
    return hasAncestorWithSelectionOnSeparateLine(previousRenderer, stayWithin);
}

static std::optional<PlatformLayerIdentifier> primaryLayerID(const RenderObject& renderer)
{
    CheckedPtr layerRenderer = dynamicDowncast<RenderLayerModelObject>(renderer);
    if (!layerRenderer)
        return { };

    CheckedPtr layer = layerRenderer->layer();
    if (!layer)
        return { };

    auto* layerBacking = layer->backing();
    if (!layerBacking)
        return { };

    RefPtr graphicsLayer = layerBacking->graphicsLayer();
    if (!graphicsLayer)
        return { };

    return graphicsLayer->primaryLayerID();
}

auto RenderObject::collectSelectionGeometriesInternal(const SimpleRange& range) -> SelectionGeometriesInternal
{
    Vector<PlatformLayerIdentifier> intersectingLayerIDs;
    Vector<SelectionGeometry> geometries;
    Vector<SelectionGeometry> newGeometries;
    bool hasFlippedWritingMode = range.start.container->renderer() && range.start.container->renderer()->writingMode().isBlockFlipped();
    bool containsDifferentWritingModes = false;
    bool hasLeftToRightText = false;
    bool hasRightToLeftText = false;
    bool separateFromPreviousLine = false;
    SingleThreadWeakPtr<RenderObject> previousRenderer;
    for (Ref node : intersectingNodesWithDeprecatedZeroOffsetStartQuirk(range)) {
        CheckedPtr renderer = node->renderer();
        if (!renderer)
            continue;

        if (auto layerID = primaryLayerID(*renderer))
            intersectingLayerIDs.append(WTFMove(*layerID));

        if (!separateFromPreviousLine)
            separateFromPreviousLine = shouldRenderSelectionOnSeparateLine(renderer.get()) || shouldRenderPreviousSelectionOnSeparateLine(previousRenderer.get(), renderer->previousSibling());
        previousRenderer = renderer.get();

        // Only ask leaf render objects for their line box rects.
        if (!renderer->firstChildSlow() && renderer->style().usedUserSelect() != UserSelect::None) {
            bool isStartNode = renderer->node() == range.start.container.ptr();
            bool isEndNode = renderer->node() == range.end.container.ptr();
            if (hasFlippedWritingMode != renderer->writingMode().isBlockFlipped())
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
                if (separateFromPreviousLine) {
                    selectionGeometry.setSeparateFromPreviousLine(true);
                    separateFromPreviousLine = false;
                }
                if (selectionGeometry.containsStart() && !isStartNode)
                    selectionGeometry.setContainsStart(false);
                if (selectionGeometry.containsEnd() && !isEndNode)
                    selectionGeometry.setContainsEnd(false);
                if (selectionGeometry.logicalWidth() || selectionGeometry.logicalHeight())
                    geometries.append(selectionGeometry);
                if (selectionGeometry.direction() == TextDirection::RTL)
                    hasRightToLeftText = true;
                else
                    hasLeftToRightText = true;
            }
            newGeometries.shrink(0);
        }
    }

    // The range could span nodes with different writing modes.
    // If this is the case, we use the writing mode of the common ancestor.
    if (containsDifferentWritingModes) {
        if (RefPtr ancestor = commonInclusiveAncestor<ComposedTree>(range)) {
            if (CheckedPtr renderer = ancestor->renderer())
                hasFlippedWritingMode = renderer->writingMode().isBlockFlipped();
        }
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

        if (intervalsSufficientlyOverlap(currentRectTop, currentRectBottom, lineTop, lineBottom)) {
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
            if (geometries[i].separateFromPreviousLine()) {
                lastLineTop = std::numeric_limits<int>::max();
                lastLineBottom = std::numeric_limits<int>::min();
                lineTop = currentRectTop;
                lineBottom = currentRectBottom;
            } else if (!hasFlippedWritingMode) {
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

    adjustLineHeightOfSelectionGeometries(geometries, numberOfGeometries, lineNumber, lineTop, lineBottom - lineTop);

    // When using SelectionRenderingBehavior::CoalesceBoundingRects, sort the rectangles and make sure there are no gaps.
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

    bool visuallyContiguousBidiTextSelection = usesVisuallyContiguousBidiTextSelection(range);
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
        if (adjustedWidth > previousRect.logicalWidth() && (!visuallyContiguousBidiTextSelection || previousRect.direction() == geometries[j].direction()))
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

    return { WTFMove(geometries), maxLineNumber, hasRightToLeftText && hasLeftToRightText, WTFMove(intersectingLayerIDs) };
}

static bool coalesceSelectionGeometryWithAdjacentQuadsIfPossible(SelectionGeometry& current, const SelectionGeometry& next)
{
    auto nextQuad = next.quad();
    if (nextQuad.isEmpty())
        return true;

    auto areCloseEnoughToCoalesce = [](const FloatPoint& first, const FloatPoint& second) {
        constexpr float maxDistanceBetweenBoundaryPoints = 8;
        return (first - second).diagonalLengthSquared() <= maxDistanceBetweenBoundaryPoints * maxDistanceBetweenBoundaryPoints;
    };

    auto currentQuad = current.quad();

    if (std::abs(rotatedBoundingRectWithMinimumAngleOfRotation(currentQuad).angleInRadians - rotatedBoundingRectWithMinimumAngleOfRotation(nextQuad).angleInRadians) > radiansPerDegreeFloat)
        return false;

    if (!areCloseEnoughToCoalesce(currentQuad.p2(), nextQuad.p1()) || !areCloseEnoughToCoalesce(currentQuad.p3(), nextQuad.p4()))
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

static bool canCoalesceGeometries(const SimpleRange& range, const SelectionGeometry& first, const SelectionGeometry& second)
{
    auto firstRect = first.rect();
    auto secondRect = second.rect();
    if (firstRect.intersects(secondRect))
        return true;

    if (first.logicalTop() == second.logicalTop() && first.isHorizontal() == second.isHorizontal() && usesVisuallyContiguousBidiTextSelection(range)) {
        if (first.logicalLeftExtent() == second.logicalLeft())
            return true;

        if (second.logicalLeftExtent() == first.logicalLeft())
            return true;
    }

    return false;
}

auto RenderObject::collectSelectionGeometries(const SimpleRange& range) -> SelectionGeometries
{
    auto [geometries, maxLineNumber, hasBidirectionalText, intersectingLayerIDs] = RenderObject::collectSelectionGeometriesInternal(range);
    auto numberOfGeometries = geometries.size();

    // Union all the rectangles on interior lines (i.e. not first or last).
    // On first and last lines, just avoid having overlaps by merging intersecting rectangles.
    Vector<SelectionGeometry> coalescedGeometries;
    IntRect interiorUnionRect;
    for (size_t i = 0; i < numberOfGeometries; ++i) {
        auto& currentGeometry = geometries[i];
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
                auto& previousGeometry = coalescedGeometries.last();
                if (canCoalesceGeometries(range, previousGeometry, currentGeometry)) {
                    previousGeometry = coalesceSelectionGeometries(currentGeometry, previousGeometry);
                    continue;
                }
            }
            // Couldn't merge with previous rect, so just appending.
            coalescedGeometries.append(currentGeometry);
        } else if (currentGeometry.lineNumber() < maxLineNumber) {
            if (interiorUnionRect.isEmpty()) {
                // Start collecting interior rects.
                interiorUnionRect = currentGeometry.rect();
            } else if (!currentGeometry.separateFromPreviousLine()
                && (interiorUnionRect.intersects(currentGeometry.rect())
                    || interiorUnionRect.maxX() == currentGeometry.rect().x()
                    || interiorUnionRect.maxY() == currentGeometry.rect().y()
                    || interiorUnionRect.x() == currentGeometry.rect().maxX()
                    || interiorUnionRect.y() == currentGeometry.rect().maxY())) {
                // Only union the lines that are attached.
                // For Apple Books, the interior lines may cross multiple horizontal pages.
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
            if (previousGeometry.logicalTop() == currentGeometry.logicalTop() && canCoalesceGeometries(range, previousGeometry, currentGeometry)) {
                // previousRect is also on the last line, and intersects the current one.
                previousGeometry = coalesceSelectionGeometries(currentGeometry, previousGeometry);
                continue;
            }
            // Couldn't merge with previous rect, so just appending.
            coalescedGeometries.append(currentGeometry);
        }
    }

    if (hasBidirectionalText) {
        auto directions = computeSelectionEndpointDirections(range);
        makeBidiSelectionVisuallyContiguousIfNeeded(directions, range, coalescedGeometries);
        adjustTextDirectionForCoalescedGeometries(directions, range, coalescedGeometries);
    }

    return { WTFMove(coalescedGeometries), WTFMove(intersectingLayerIDs) };
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

bool RenderObject::isSkippedContent() const
{
    if (is<RenderText>(*this))
        return style().isSkippedRootOrSkippedContent();

    if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(*this); renderBox && renderBox->isColumnSpanner()) {
        // Checking if parent is root or part of a skipped tree does not work in cases when the renderer is
        // moved out of its original position (e.g. column spanners).
        return renderBox->style().isSkippedRootOrSkippedContent() && !isSkippedContentRoot(*renderBox);
    }
    return parent() && parent()->style().isSkippedRootOrSkippedContent();
}

TextStream& operator<<(TextStream& ts, const RenderObject& renderer)
{
    ts << renderer.debugDescription();
    return ts;
}

TextStream& operator<<(TextStream& ts, const RenderObject::RepaintRects& repaintRects)
{
    ts << " (clipped overflow "_s << repaintRects.clippedOverflowRect << ')';
    if (repaintRects.outlineBoundsRect && repaintRects.outlineBoundsRect != repaintRects.clippedOverflowRect)
        ts << " (outline bounds "_s << repaintRects.outlineBoundsRect << ')';
    return ts;
}

auto RenderObject::CachedImageListener::create(RenderObject& renderer) -> Ref<CachedImageListener>
{
    return adoptRef(*new CachedImageListener(renderer));
}

RenderObject::CachedImageListener::CachedImageListener(RenderObject& renderer)
    : m_renderer(renderer)
{
}

void RenderObject::CachedImageListener::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& metrics, LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    if (CheckedPtr renderer = m_renderer.get())
        renderer->notifyFinished(resource, metrics, loadWillContinueInAnotherProcess);
}

void RenderObject::CachedImageListener::imageChanged(CachedImage* image, const IntRect* rect)
{
    if (CheckedPtr renderer = m_renderer.get())
        renderer->imageChanged(static_cast<WrappedImagePtr>(image), rect);
}

bool RenderObject::CachedImageListener::allowsAnimation() const
{
    if (CheckedPtr renderer = m_renderer.get())
        return renderer->allowsAnimation();
    return true;
}

bool RenderObject::CachedImageListener::canDestroyDecodedData() const
{
    if (CheckedPtr renderer = m_renderer.get())
        return renderer->canDestroyDecodedData();
    return true;
}

VisibleInViewportState RenderObject::CachedImageListener::imageFrameAvailable(CachedImage& image, ImageAnimatingState state, const IntRect* rect)
{
    if (CheckedPtr renderer = m_renderer.get())
        return renderer->imageFrameAvailable(image, state, rect);
    return VisibleInViewportState::No;
}

VisibleInViewportState RenderObject::CachedImageListener::imageVisibleInViewport(const Document& document) const
{
    if (CheckedPtr renderer = m_renderer.get())
        return renderer->imageVisibleInViewport(document);
    return VisibleInViewportState::No;
}

void RenderObject::CachedImageListener::didRemoveCachedImageClient(CachedImage& image)
{
    if (CheckedPtr renderer = m_renderer.get())
        renderer->didRemoveCachedImageClient(image);
}

void RenderObject::CachedImageListener::imageContentChanged(CachedImage& image)
{
    if (CheckedPtr renderer = m_renderer.get())
        renderer->imageContentChanged(image);
}

void RenderObject::CachedImageListener::scheduleRenderingUpdateForImage(CachedImage& image)
{
    if (CheckedPtr renderer = m_renderer.get())
        renderer->scheduleRenderingUpdateForImage(image);
}

VisibleInViewportState RenderObject::imageFrameAvailable(CachedImage& image, ImageAnimatingState, const IntRect* changeRect)
{
    imageChanged(static_cast<WrappedImagePtr>(&image), changeRect);
    return VisibleInViewportState::No;
}

#if ENABLE(TREE_DEBUGGING)

void printPaintOrderTreeForLiveDocuments()
{
    for (auto& document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isRootFrame())
            WTFLogAlways("----------------------root frame--------------------------\n");
        WTFLogAlways("%s", document->url().string().utf8().data());
        showPaintOrderTree(document->renderView());
    }
}

void printRenderTreeForLiveDocuments()
{
    for (auto& document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isRootFrame())
            WTFLogAlways("----------------------root frame--------------------------\n");
        WTFLogAlways("%s", document->url().string().utf8().data());
        showRenderTree(document->renderView());
    }
}

void printLayerTreeForLiveDocuments()
{
    for (auto& document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isRootFrame())
            WTFLogAlways("----------------------root frame--------------------------\n");
        WTFLogAlways("%s", document->url().string().utf8().data());
        showLayerTree(document->renderView());
    }
}

void printAccessibilityTreeForLiveDocuments()
{
    for (auto& document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame()) {
            if (document->frame()->isRootFrame())
                WTFLogAlways("Accessibility tree for root document %p %s", document.ptr(), document->url().string().utf8().data());
            else
                WTFLogAlways("Accessibility tree for non-root document %p %s", document.ptr(), document->url().string().utf8().data());
            dumpAccessibilityTreeToStderr(document.get());
        }
    }
}

void printGraphicsLayerTreeForLiveDocuments()
{
    for (auto& document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isRootFrame()) {
            WTFLogAlways("Graphics layer tree for root document %p %s", document.ptr(), document->url().string().utf8().data());
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
