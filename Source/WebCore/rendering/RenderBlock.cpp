/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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
#include "RenderBlock.h"

#include "AXObjectCache.h"
#include "DocumentInlines.h"
#include "Editor.h"
#include "Element.h"
#include "ElementInlines.h"
#include "EventRegion.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestLocation.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "InlineIteratorInlineBox.h"
#include "LayoutRepainter.h"
#include "LegacyInlineElementBox.h"
#include "LegacyInlineTextBox.h"
#include "Logging.h"
#include "LogicalSelectionOffsetCaches.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderButton.h"
#include "RenderChildIterator.h"
#include "RenderCombineText.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentedFlow.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderTableCell.h"
#include "RenderTextControl.h"
#include "RenderTextFragment.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderTreePosition.h"
#include "RenderView.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "ShapeOutsideInfo.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

using namespace HTMLNames;
using namespace WTF::Unicode;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderBlock);

struct SameSizeAsRenderBlock : public RenderBox {
};

static_assert(sizeof(RenderBlock) == sizeof(SameSizeAsRenderBlock), "RenderBlock should stay small");

typedef HashMap<const RenderBlock*, std::unique_ptr<TrackedRendererListHashSet>> TrackedDescendantsMap;
typedef HashMap<const RenderBox*, HashSet<const RenderBlock*>> TrackedContainerMap;

static TrackedDescendantsMap* percentHeightDescendantsMap;
static TrackedContainerMap* percentHeightContainerMap;

static void insertIntoTrackedRendererMaps(const RenderBlock& container, RenderBox& descendant)
{
    if (!percentHeightDescendantsMap) {
        percentHeightDescendantsMap = new TrackedDescendantsMap;
        percentHeightContainerMap = new TrackedContainerMap;
    }
    
    auto& descendantSet = percentHeightDescendantsMap->ensure(&container, [] {
        return makeUnique<TrackedRendererListHashSet>();
    }).iterator->value;

    bool added = descendantSet->add(&descendant).isNewEntry;
    if (!added) {
#if ASSERT_ENABLED
        auto it = percentHeightContainerMap->find(&descendant);
        ASSERT(it != percentHeightContainerMap->end());
        ASSERT(it->value.contains(&container));
#endif
        return;
    }
    
    auto& containerSet = percentHeightContainerMap->add(&descendant, HashSet<const RenderBlock*>()).iterator->value;
    ASSERT(!containerSet.contains(&container));
    containerSet.add(&container);
}

static void removeFromTrackedRendererMaps(RenderBox& descendant)
{
    if (!percentHeightDescendantsMap)
        return;
    
    HashSet<const RenderBlock*> containerSet = percentHeightContainerMap->take(&descendant);
    for (auto* container : containerSet) {
        // FIXME: Disabling this assert temporarily until we fix the layout
        // bugs associated with positioned objects not properly cleared from
        // their ancestor chain before being moved. See webkit bug 93766.
        // ASSERT(descendant->isDescendantOf(container));
        auto descendantsMapIterator = percentHeightDescendantsMap->find(container);
        ASSERT(descendantsMapIterator != percentHeightDescendantsMap->end());
        if (descendantsMapIterator == percentHeightDescendantsMap->end())
            continue;
        auto& descendantSet = descendantsMapIterator->value;
        ASSERT(descendantSet->contains(&descendant));
        descendantSet->remove(&descendant);
        if (descendantSet->isEmpty())
            percentHeightDescendantsMap->remove(descendantsMapIterator);
    }
}

class PositionedDescendantsMap {
public:
    void addDescendant(const RenderBlock& containingBlock, RenderBox& positionedDescendant)
    {
        // Protect against double insert where a descendant would end up with multiple containing blocks.
        auto* previousContainingBlock = m_containerMap.get(&positionedDescendant);
        if (previousContainingBlock && previousContainingBlock != &containingBlock) {
            if (auto* descendants = m_descendantsMap.get(previousContainingBlock))
                descendants->remove(&positionedDescendant);
        }

        auto& descendants = m_descendantsMap.ensure(&containingBlock, [] {
            return makeUnique<TrackedRendererListHashSet>();
        }).iterator->value;

        auto isNewEntry = false;
        if (!is<RenderView>(containingBlock) || descendants->isEmpty())
            isNewEntry = descendants->add(&positionedDescendant).isNewEntry;
        else if (positionedDescendant.isFixedPositioned() || isInTopLayerOrBackdrop(positionedDescendant.style(), positionedDescendant.element()))
            isNewEntry = descendants->appendOrMoveToLast(&positionedDescendant).isNewEntry;
        else {
            auto ensureLayoutDepentBoxPosition = [&] {
                // RenderView is a special containing block as it may hold both absolute and fixed positioned containing blocks.
                // When a fixed positioned box is also a descendant of an absolute positioned box anchored to the RenderView,
                // we have to make sure that the absolute positioned box is inserted before the fixed box to follow
                // block layout dependency.
                for (auto it = descendants->begin(); it != descendants->end(); ++it) {
                    if ((*it)->isFixedPositioned()) {
                        isNewEntry = descendants->insertBefore(it, &positionedDescendant).isNewEntry;
                        return;
                    }
                }
                isNewEntry = descendants->appendOrMoveToLast(&positionedDescendant).isNewEntry;
            };
            ensureLayoutDepentBoxPosition();
        }

        if (!isNewEntry) {
            ASSERT(m_containerMap.contains(&positionedDescendant));
            return;
        }
        m_containerMap.set(&positionedDescendant, &containingBlock);
    }

    void removeDescendant(const RenderBox& positionedDescendant)
    {
        auto* containingBlock = m_containerMap.take(&positionedDescendant);
        if (!containingBlock)
            return;

        auto descendantsIterator = m_descendantsMap.find(containingBlock);
        ASSERT(descendantsIterator != m_descendantsMap.end());
        if (descendantsIterator == m_descendantsMap.end())
            return;

        auto& descendants = descendantsIterator->value;
        ASSERT(descendants->contains(const_cast<RenderBox*>(&positionedDescendant)));

        descendants->remove(const_cast<RenderBox*>(&positionedDescendant));
        if (descendants->isEmpty())
            m_descendantsMap.remove(descendantsIterator);
    }
    
    void removeContainingBlock(const RenderBlock& containingBlock)
    {
        auto descendants = m_descendantsMap.take(&containingBlock);
        if (!descendants)
            return;

        for (auto* renderer : *descendants)
            m_containerMap.remove(renderer);
    }
    
    TrackedRendererListHashSet* positionedRenderers(const RenderBlock& containingBlock) const
    {
        return m_descendantsMap.get(&containingBlock);
    }

private:
    using DescendantsMap = HashMap<const RenderBlock*, std::unique_ptr<TrackedRendererListHashSet>>;
    using ContainerMap = HashMap<const RenderBox*, const RenderBlock*>;
    
    DescendantsMap m_descendantsMap;
    ContainerMap m_containerMap;
};

static PositionedDescendantsMap& positionedDescendantsMap()
{
    static NeverDestroyed<PositionedDescendantsMap> mapForPositionedDescendants;
    return mapForPositionedDescendants;
}

typedef HashMap<RenderBlock*, std::unique_ptr<ListHashSet<RenderInline*>>> ContinuationOutlineTableMap;

struct UpdateScrollInfoAfterLayoutTransaction {
    UpdateScrollInfoAfterLayoutTransaction(const RenderView& view)
        : nestedCount(0)
        , view(&view)
    {
    }

    int nestedCount;
    const RenderView* view;
    HashSet<RenderBlock*> blocks;
};

typedef Vector<UpdateScrollInfoAfterLayoutTransaction> DelayedUpdateScrollInfoStack;
static std::unique_ptr<DelayedUpdateScrollInfoStack>& updateScrollInfoAfterLayoutTransactionStack()
{
    static NeverDestroyed<std::unique_ptr<DelayedUpdateScrollInfoStack>> delayedUpdatedScrollInfoStack;
    return delayedUpdatedScrollInfoStack;
}

// Allocated only when some of these fields have non-default values

struct RenderBlockRareData {
    WTF_MAKE_NONCOPYABLE(RenderBlockRareData); WTF_MAKE_FAST_ALLOCATED;
public:
    RenderBlockRareData()
    {
    }

    LayoutUnit m_paginationStrut;
    LayoutUnit m_pageLogicalOffset;
    LayoutUnit m_intrinsicBorderForFieldset;
    
    std::optional<WeakPtr<RenderFragmentedFlow>> m_enclosingFragmentedFlow;
};

typedef HashMap<const RenderBlock*, std::unique_ptr<RenderBlockRareData>> RenderBlockRareDataMap;
static RenderBlockRareDataMap* gRareDataMap;

// This class helps dispatching the 'overflow' event on layout change. overflow can be set on RenderBoxes, yet the existing code
// only works on RenderBlocks. If this change, this class should be shared with other RenderBoxes.
class OverflowEventDispatcher {
    WTF_MAKE_NONCOPYABLE(OverflowEventDispatcher);
public:
    OverflowEventDispatcher(const RenderBlock* block)
        : m_block(block)
        , m_hadHorizontalLayoutOverflow(false)
        , m_hadVerticalLayoutOverflow(false)
    {
        m_shouldDispatchEvent = !m_block->isAnonymous() && m_block->hasNonVisibleOverflow() && m_block->document().hasListenerType(Document::OVERFLOWCHANGED_LISTENER);
        if (m_shouldDispatchEvent) {
            m_hadHorizontalLayoutOverflow = m_block->hasHorizontalLayoutOverflow();
            m_hadVerticalLayoutOverflow = m_block->hasVerticalLayoutOverflow();
        }
    }

    ~OverflowEventDispatcher()
    {
        if (!m_shouldDispatchEvent)
            return;

        bool hasHorizontalLayoutOverflow = m_block->hasHorizontalLayoutOverflow();
        bool hasVerticalLayoutOverflow = m_block->hasVerticalLayoutOverflow();

        bool horizontalLayoutOverflowChanged = hasHorizontalLayoutOverflow != m_hadHorizontalLayoutOverflow;
        bool verticalLayoutOverflowChanged = hasVerticalLayoutOverflow != m_hadVerticalLayoutOverflow;
        if (!horizontalLayoutOverflowChanged && !verticalLayoutOverflowChanged)
            return;

        Ref<OverflowEvent> overflowEvent = OverflowEvent::create(horizontalLayoutOverflowChanged, hasHorizontalLayoutOverflow, verticalLayoutOverflowChanged, hasVerticalLayoutOverflow);
        overflowEvent->setTarget(RefPtr { m_block->element() });
        m_block->document().enqueueOverflowEvent(WTFMove(overflowEvent));
    }

private:
    const RenderBlock* m_block;
    bool m_shouldDispatchEvent;
    bool m_hadHorizontalLayoutOverflow;
    bool m_hadVerticalLayoutOverflow;
};

RenderBlock::RenderBlock(Element& element, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderBox(element, WTFMove(style), baseTypeFlags | RenderBlockFlag)
{
}

RenderBlock::RenderBlock(Document& document, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderBox(document, WTFMove(style), baseTypeFlags | RenderBlockFlag)
{
}

static void removeBlockFromPercentageDescendantAndContainerMaps(RenderBlock* block)
{
    if (!percentHeightDescendantsMap)
        return;
    std::unique_ptr<TrackedRendererListHashSet> descendantSet = percentHeightDescendantsMap->take(block);
    if (!descendantSet)
        return;
    
    for (auto* descendant : *descendantSet) {
        auto it = percentHeightContainerMap->find(descendant);
        ASSERT(it != percentHeightContainerMap->end());
        if (it == percentHeightContainerMap->end())
            continue;
        auto& containerSet = it->value;
        ASSERT(containerSet.contains(block));
        containerSet.remove(block);
        if (containerSet.isEmpty())
            percentHeightContainerMap->remove(it);
    }
}

RenderBlock::~RenderBlock()
{
    // Blocks can be added to gRareDataMap during willBeDestroyed(), so this code can't move there.
    if (gRareDataMap)
        gRareDataMap->remove(this);

    // Do not add any more code here. Add it to willBeDestroyed() instead.
}

// Note that this is not called for RenderBlockFlows.
void RenderBlock::willBeDestroyed()
{
    if (!renderTreeBeingDestroyed()) {
        if (parent())
            parent()->dirtyLinesFromChangedChild(*this);
    }

    blockWillBeDestroyed();

    RenderBox::willBeDestroyed();
}

void RenderBlock::blockWillBeDestroyed()
{
    removeFromUpdateScrollInfoAfterLayoutTransaction();

    removeBlockFromPercentageDescendantAndContainerMaps(this);
    positionedDescendantsMap().removeContainingBlock(*this);
}

bool RenderBlock::hasRareData() const
{
    return gRareDataMap ? gRareDataMap->contains(this) : false;
}

void RenderBlock::removePositionedObjectsIfNeeded(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    bool hadTransform = oldStyle.hasTransformRelatedProperty();
    bool willHaveTransform = newStyle.hasTransformRelatedProperty();
    bool hadLayoutContainment = oldStyle.containsLayout();
    bool willHaveLayoutContainment = newStyle.containsLayout();
    if (oldStyle.position() == newStyle.position() && hadTransform == willHaveTransform && hadLayoutContainment == willHaveLayoutContainment)
        return;

    // We are no longer the containing block for out-of-flow descendants.
    bool outOfFlowDescendantsHaveNewContainingBlock = (hadTransform && !willHaveTransform) || (newStyle.position() == PositionType::Static && !willHaveTransform);
    if (hadLayoutContainment != willHaveLayoutContainment)
        outOfFlowDescendantsHaveNewContainingBlock = hadLayoutContainment && !willHaveLayoutContainment;
    if (outOfFlowDescendantsHaveNewContainingBlock) {
        // Our out-of-flow descendants will be inserted into a new containing block's positioned objects list during the next layout.
        removePositionedObjects(nullptr, NewContainingBlock);
        return;
    }

    // We are a new containing block.
    if (oldStyle.position() == PositionType::Static && !hadTransform) {
        // Remove our absolutely positioned descendants from their current containing block.
        // They will be inserted into our positioned objects list during layout.
        auto* containingBlock = parent();
        while (containingBlock && !is<RenderView>(*containingBlock)
            && (containingBlock->style().position() == PositionType::Static || (containingBlock->isInline() && !containingBlock->isReplacedOrInlineBlock()))) {
            if (containingBlock->style().position() == PositionType::Relative && containingBlock->isInline() && !containingBlock->isReplacedOrInlineBlock()) {
                containingBlock = containingBlock->containingBlock();
                break;
            }
            containingBlock = containingBlock->parent();
        }
        if (containingBlock && is<RenderBlock>(*containingBlock))
            downcast<RenderBlock>(*containingBlock).removePositionedObjects(this, NewContainingBlock);
    }
}

void RenderBlock::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    // FIXME: Should change the expression below to newStyle.display() == DisplayType::InlineBlock.
    setReplacedOrInlineBlock(newStyle.isDisplayInlineType());
    if (oldStyle) {
        removePositionedObjectsIfNeeded(*oldStyle, newStyle);
        if (isLegend() && !oldStyle->isFloating() && newStyle.isFloating())
            setIsExcludedFromNormalLayout(false);
    }
    RenderBox::styleWillChange(diff, newStyle);
}

static bool borderOrPaddingLogicalWidthChanged(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    if (newStyle.isHorizontalWritingMode()) {
        return oldStyle.borderLeftWidth() != newStyle.borderLeftWidth()
            || oldStyle.borderRightWidth() != newStyle.borderRightWidth()
            || oldStyle.paddingLeft() != newStyle.paddingLeft()
            || oldStyle.paddingRight() != newStyle.paddingRight();
    }

    return oldStyle.borderTopWidth() != newStyle.borderTopWidth()
        || oldStyle.borderBottomWidth() != newStyle.borderBottomWidth()
        || oldStyle.paddingTop() != newStyle.paddingTop()
        || oldStyle.paddingBottom() != newStyle.paddingBottom();
}

void RenderBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);

    if (oldStyle)
        adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded(*oldStyle, style());

    propagateStyleToAnonymousChildren(PropagateToBlockChildrenOnly);

    // It's possible for our border/padding to change, but for the overall logical width of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    setShouldForceRelayoutChildren(oldStyle && diff == StyleDifference::Layout && needsLayout() && borderOrPaddingLogicalWidthChanged(*oldStyle, style()));
}

RenderPtr<RenderBlock> RenderBlock::clone() const
{
    RenderPtr<RenderBlock> cloneBlock;
    if (isAnonymousBlock()) {
        cloneBlock = RenderPtr<RenderBlock>(createAnonymousBlock());
        cloneBlock->setChildrenInline(childrenInline());
    } else {
        RenderTreePosition insertionPosition(*parent());
        cloneBlock = static_pointer_cast<RenderBlock>(element()->createElementRenderer(RenderStyle::clone(style()), insertionPosition));
        cloneBlock->initializeStyle();

        // This takes care of setting the right value of childrenInline in case
        // generated content is added to cloneBlock and 'this' does not have
        // generated content added yet.
        cloneBlock->setChildrenInline(cloneBlock->firstChild() ? cloneBlock->firstChild()->isInline() : childrenInline());
    }
    cloneBlock->setFragmentedFlowState(fragmentedFlowState());
    return cloneBlock;
}

void RenderBlock::deleteLines()
{
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferRecomputeIsIgnored(element());
}

bool RenderBlock::childrenPreventSelfCollapsing() const
{
    // Whether or not we collapse is dependent on whether all our normal flow children
    // are also self-collapsing.
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isFloatingOrOutOfFlowPositioned())
            continue;
        if (!child->isSelfCollapsingBlock())
            return true;
    }
    return false;
}

bool RenderBlock::isSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) are a table,
    // (c) have border/padding,
    // (d) have a min-height
    // (e) have specified that one of our margins can't collapse using a CSS extension
    if (logicalHeight() > 0
        || isTable() || borderAndPaddingLogicalHeight()
        || style().logicalMinHeight().isPositive())
        return false;

    Length logicalHeightLength = style().logicalHeight();
    bool hasAutoHeight = logicalHeightLength.isAuto();
    if (logicalHeightLength.isPercentOrCalculated() && !document().inQuirksMode()) {
        hasAutoHeight = true;
        for (RenderBlock* cb = containingBlock(); cb && !is<RenderView>(*cb); cb = cb->containingBlock()) {
            if (cb->style().logicalHeight().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (hasAutoHeight || ((logicalHeightLength.isFixed() || logicalHeightLength.isPercentOrCalculated()) && logicalHeightLength.isZero()))
        return !createsNewFormattingContext() && !childrenPreventSelfCollapsing();


    return false;
}

static inline UpdateScrollInfoAfterLayoutTransaction* currentUpdateScrollInfoAfterLayoutTransaction()
{
    if (!updateScrollInfoAfterLayoutTransactionStack())
        return nullptr;
    return &updateScrollInfoAfterLayoutTransactionStack()->last();
}

void RenderBlock::beginUpdateScrollInfoAfterLayoutTransaction()
{
    if (!updateScrollInfoAfterLayoutTransactionStack())
        updateScrollInfoAfterLayoutTransactionStack() = makeUnique<DelayedUpdateScrollInfoStack>();
    if (updateScrollInfoAfterLayoutTransactionStack()->isEmpty() || currentUpdateScrollInfoAfterLayoutTransaction()->view != &view())
        updateScrollInfoAfterLayoutTransactionStack()->append(UpdateScrollInfoAfterLayoutTransaction(view()));
    ++currentUpdateScrollInfoAfterLayoutTransaction()->nestedCount;
}

void RenderBlock::endAndCommitUpdateScrollInfoAfterLayoutTransaction()
{
    UpdateScrollInfoAfterLayoutTransaction* transaction = currentUpdateScrollInfoAfterLayoutTransaction();
    ASSERT(transaction);
    ASSERT(transaction->view == &view());
    if (--transaction->nestedCount)
        return;

    // Calling RenderLayer::updateScrollInfoAfterLayout() may cause its associated block to layout again and
    // updates its scroll info (i.e. call RenderBlock::updateScrollInfoAfterLayout()). We remove |transaction|
    // from the transaction stack to ensure that all subsequent calls to RenderBlock::updateScrollInfoAfterLayout()
    // are dispatched immediately. That is, to ensure that such subsequent calls aren't added to |transaction|
    // while we are processing it.
    auto blocksToUpdate = copyToVector(transaction->blocks);
    updateScrollInfoAfterLayoutTransactionStack()->removeLast();
    if (updateScrollInfoAfterLayoutTransactionStack()->isEmpty())
        updateScrollInfoAfterLayoutTransactionStack() = nullptr;

    for (auto* block : blocksToUpdate) {
        ASSERT(block->hasNonVisibleOverflow());
        block->layer()->updateScrollInfoAfterLayout();
    }
}

void RenderBlock::removeFromUpdateScrollInfoAfterLayoutTransaction()
{
    if (UNLIKELY(updateScrollInfoAfterLayoutTransactionStack().get() != 0)) {
        UpdateScrollInfoAfterLayoutTransaction* transaction = currentUpdateScrollInfoAfterLayoutTransaction();
        ASSERT(transaction);
        if (transaction->view == &view())
            transaction->blocks.remove(this);
    }
}

void RenderBlock::updateScrollInfoAfterLayout()
{
    if (!hasNonVisibleOverflow())
        return;
    
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=97937
    // Workaround for now. We cannot delay the scroll info for overflow
    // for items with opposite writing directions, as the contents needs
    // to overflow in that direction
    if (!style().isFlippedBlocksWritingMode()) {
        UpdateScrollInfoAfterLayoutTransaction* transaction = currentUpdateScrollInfoAfterLayoutTransaction();
        if (transaction && transaction->view == &view()) {
            transaction->blocks.add(this);
            return;
        }
    }
    if (layer())
        layer()->updateScrollInfoAfterLayout();
}

void RenderBlock::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    OverflowEventDispatcher dispatcher(this);

    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);
    
    // It's safe to check for control clip here, since controls can never be table cells.
    // If we have a lightweight clip, there can never be any overflow from children.
    UpdateScrollInfoAfterLayoutTransaction* transaction = currentUpdateScrollInfoAfterLayoutTransaction();
    bool isDelayingUpdateScrollInfoAfterLayoutInView = transaction && transaction->view == &view();
    if (hasControlClip() && m_overflow && !isDelayingUpdateScrollInfoAfterLayoutInView)
        clearLayoutOverflow();

    invalidateBackgroundObscurationStatus();
}

static RenderBlockRareData* getBlockRareData(const RenderBlock& block)
{
    return gRareDataMap ? gRareDataMap->get(&block) : nullptr;
}

static RenderBlockRareData& ensureBlockRareData(const RenderBlock& block)
{
    if (!gRareDataMap)
        gRareDataMap = new RenderBlockRareDataMap;
    
    auto& rareData = gRareDataMap->add(&block, nullptr).iterator->value;
    if (!rareData)
        rareData = makeUnique<RenderBlockRareData>();
    return *rareData.get();
}

void RenderBlock::preparePaginationBeforeBlockLayout(bool& relayoutChildren)
{
    // Fragments changing widths can force us to relayout our children.
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow)
        fragmentedFlow->logicalWidthChangedInFragmentsForBlock(this, relayoutChildren);
}

bool RenderBlock::recomputeLogicalWidth()
{
    LayoutUnit oldWidth = logicalWidth();

    updateLogicalWidth();
    
    bool hasBorderOrPaddingLogicalWidthChanged = this->hasBorderOrPaddingLogicalWidthChanged();
    setShouldForceRelayoutChildren(false);

    return oldWidth != logicalWidth() || hasBorderOrPaddingLogicalWidthChanged;
}

void RenderBlock::layoutBlock(bool, LayoutUnit)
{
    ASSERT_NOT_REACHED();
    clearNeedsLayout();
}

void RenderBlock::addOverflowFromChildren()
{
    if (childrenInline()) {
        addOverflowFromInlineChildren();
    
        // If this block is flowed inside a flow thread, make sure its overflow is propagated to the containing fragments.
        if (m_overflow) {
            if (auto* flow = enclosingFragmentedFlow())
                flow->addFragmentsVisualOverflow(this, m_overflow->visualOverflowRect());
        }
    } else
        addOverflowFromBlockChildren();
}

// Overflow is always relative to the border-box of the element in question.
// Therefore, if the element has a vertical scrollbar placed on the left, an overflow rect at x=2px would conceptually intersect the scrollbar.
void RenderBlock::computeOverflow(LayoutUnit oldClientAfterEdge, bool)
{
    clearOverflow();
    addOverflowFromChildren();

    addOverflowFromPositionedObjects();

    if (hasNonVisibleOverflow()) {
        auto includePaddingEnd = [&] {
            // As per https://github.com/w3c/csswg-drafts/issues/3653 padding should contribute to the scrollable overflow area.
            if (!paddingEnd())
                return;
            // FIXME: Expand it to non-grid/flex cases when applicable.
            if (!is<RenderGrid>(*this) && !is<RenderFlexibleBox>(*this))
                return;

            auto layoutOverflowRect = this->layoutOverflowRect();
            auto layoutOverflowLogicalWidthIncludingPaddingEnd = [&] {
                if (hasHorizontalLayoutOverflow())
                    return (isHorizontalWritingMode() ? layoutOverflowRect.width() : layoutOverflowRect.height()) + paddingEnd();

                // FIXME: This is not sufficient for BFC layout (missing non-formatting-context root descendants).
                auto contentLogicalRight = LayoutUnit { };
                for (auto& child : childrenOfType<RenderBox>(*this)) {
                    if (child.isOutOfFlowPositioned())
                        continue;
                    auto childLogicalRight = logicalLeftForChild(child) + logicalWidthForChild(child) + std::max(0_lu, marginEndForChild(child));
                    contentLogicalRight = std::max(contentLogicalRight, childLogicalRight);
                }
                auto logicalRightWithPaddingEnd = contentLogicalRight + paddingEnd();
                // Use padding box as the reference box.
                return logicalRightWithPaddingEnd - (isHorizontalWritingMode() ? borderLeft() : borderTop());
            };

            if (isHorizontalWritingMode())
                layoutOverflowRect.setWidth(layoutOverflowLogicalWidthIncludingPaddingEnd());
            else
                layoutOverflowRect.setHeight(layoutOverflowLogicalWidthIncludingPaddingEnd());
            addLayoutOverflow(layoutOverflowRect);
        };
        includePaddingEnd();

        auto includePaddingAfter = [&] {
            // When we have overflow clip, propagate the original spillout since it will include collapsed bottom margins and bottom padding.
            auto clientRect = flippedClientBoxRect();
            auto rectToApply = clientRect;
            // Set the axis we don't care about to be 1, since we want this overflow to always be considered reachable.
            if (isHorizontalWritingMode()) {
                rectToApply.setWidth(1);
                rectToApply.setHeight(std::max(0_lu, oldClientAfterEdge - clientRect.y()));
            } else {
                rectToApply.setWidth(std::max(0_lu, oldClientAfterEdge - clientRect.x()));
                rectToApply.setHeight(1);
            }
            addLayoutOverflow(rectToApply);
        };
        includePaddingAfter();
        if (hasRenderOverflow())
            m_overflow->setLayoutClientAfterEdge(oldClientAfterEdge);
    }
        
    // Add visual overflow from box-shadow, border-image-outset and outline.
    addVisualEffectOverflow();

    // Add visual overflow from theme.
    addVisualOverflowFromTheme();
}

void RenderBlock::clearLayoutOverflow()
{
    if (!m_overflow)
        return;
    
    if (visualOverflowRect() == borderBoxRect()) {
        // FIXME: Implement complete solution for fragments overflow.
        clearOverflow();
        return;
    }
    
    m_overflow->setLayoutOverflow(borderBoxRect());
}

void RenderBlock::addOverflowFromBlockChildren()
{
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!child->isFloatingOrOutOfFlowPositioned())
            addOverflowFromChild(child);
    }
}

void RenderBlock::addOverflowFromPositionedObjects()
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    for (auto it = positionedDescendants->begin(), end = positionedDescendants->end(); it != end; ++it) {
        RenderBox* positionedObject = *it;
        
        // Fixed positioned elements don't contribute to layout overflow, since they don't scroll with the content.
        if (positionedObject->style().position() != PositionType::Fixed)
            addOverflowFromChild(positionedObject, { positionedObject->x(), positionedObject->y() });
    }
}

void RenderBlock::addVisualOverflowFromTheme()
{
    if (!style().hasEffectiveAppearance())
        return;

    FloatRect inflatedRect = borderBoxRect();
    theme().adjustRepaintRect(*this, inflatedRect);
    addVisualOverflow(snappedIntRect(LayoutRect(inflatedRect)));

    if (RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->addFragmentsVisualOverflowFromTheme(this);
}

void RenderBlock::setLogicalLeftForChild(RenderBox& child, LayoutUnit logicalLeft, ApplyLayoutDeltaMode applyDelta)
{
    if (isHorizontalWritingMode()) {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(child.x() - logicalLeft, 0_lu));
        child.setX(logicalLeft);
    } else {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(0_lu, child.y() - logicalLeft));
        child.setY(logicalLeft);
    }
}

void RenderBlock::setLogicalTopForChild(RenderBox& child, LayoutUnit logicalTop, ApplyLayoutDeltaMode applyDelta)
{
    if (isHorizontalWritingMode()) {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(0_lu, child.y() - logicalTop));
        child.setY(logicalTop);
    } else {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(child.x() - logicalTop, 0_lu));
        child.setX(logicalTop);
    }
}

void RenderBlock::updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, RenderBox& child)
{
    if (child.isOutOfFlowPositioned())
        return;

    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    if (relayoutChildren || (child.hasRelativeLogicalHeight() && !isRenderView()))
        child.setChildNeedsLayout(MarkOnlyThis);

    // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
    if (relayoutChildren && child.needsPreferredWidthsRecalculation())
        child.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
}

void RenderBlock::dirtyForLayoutFromPercentageHeightDescendants()
{
    if (!percentHeightDescendantsMap)
        return;

    TrackedRendererListHashSet* descendants = percentHeightDescendantsMap->get(this);
    if (!descendants)
        return;

    for (auto it = descendants->begin(), end = descendants->end(); it != end; ++it) {
        RenderElement* renderer = *it;
        // Let's not dirty the height perecentage descendant when it has an absolutely positioned containing block ancestor. We should be able to dirty such boxes through the regular invalidation logic.
        bool descendantNeedsLayout = true;
        for (auto* ancestor = renderer->containingBlock(); ancestor && ancestor != this; ancestor = ancestor->containingBlock()) {
            if (ancestor->isOutOfFlowPositioned()) {
                descendantNeedsLayout = false;
                break;
            }
        }
        if (!descendantNeedsLayout)
            continue;

        while (renderer != this) {
            if (renderer->normalChildNeedsLayout())
                break;
            renderer->setChildNeedsLayout(MarkOnlyThis);
            
            // If the width of an image is affected by the height of a child (e.g., an image with an aspect ratio),
            // then we have to dirty preferred widths, since even enclosing blocks can become dirty as a result.
            // (A horizontal flexbox that contains an inline image wrapped in an anonymous block for example.)
            if (renderer->hasIntrinsicAspectRatio() || renderer->style().hasAspectRatio())
                renderer->setPreferredLogicalWidthsDirty(true);
            renderer = renderer->container();
            ASSERT(renderer);
            if (!renderer)
                break;
        }
    }
}

void RenderBlock::simplifiedNormalFlowLayout()
{
    ASSERT(!childrenInline());

    for (auto* box = firstChildBox(); box; box = box->nextSiblingBox()) {
        if (!box->isOutOfFlowPositioned())
            box->layoutIfNeeded();
    }
}

bool RenderBlock::canPerformSimplifiedLayout() const
{
    return (posChildNeedsLayout() || needsSimplifiedNormalFlowLayout()) && !normalChildNeedsLayout() && !selfNeedsLayout();
}

bool RenderBlock::simplifiedLayout()
{
    if (!canPerformSimplifiedLayout())
        return false;

    LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());
    if (needsPositionedMovementLayout() && !tryLayoutDoingPositionedMovementOnly())
        return false;

    // Lay out positioned descendants or objects that just need to recompute overflow.
    if (needsSimplifiedNormalFlowLayout())
        simplifiedNormalFlowLayout();

    // Make sure a forced break is applied after the content if we are a flow thread in a simplified layout.
    // This ensures the size information is correctly computed for the last auto-height fragment receiving content.
    if (is<RenderFragmentedFlow>(*this))
        downcast<RenderFragmentedFlow>(*this).applyBreakAfterContent(clientLogicalBottom());

    // Lay out our positioned objects if our positioned child bit is set.
    // Also, if an absolute position element inside a relative positioned container moves, and the absolute element has a fixed position
    // child, neither the fixed element nor its container learn of the movement since posChildNeedsLayout() is only marked as far as the 
    // relative positioned container. So if we can have fixed pos objects in our positioned objects list check if any of them
    // are statically positioned and thus need to move with their absolute ancestors.
    bool canContainFixedPosObjects = canContainFixedPositionObjects();
    if (posChildNeedsLayout() || canContainFixedPosObjects)
        layoutPositionedObjects(false, !posChildNeedsLayout() && canContainFixedPosObjects);

    // Recompute our overflow information.
    // FIXME: We could do better here by computing a temporary overflow object from layoutPositionedObjects and only
    // updating our overflow if we either used to have overflow or if the new temporary object has overflow.
    // For now just always recompute overflow.  This is no worse performance-wise than the old code that called rightmostPosition and
    // lowestPosition on every relayout so it's not a regression.
    // computeOverflow expects the bottom edge before we clamp our height. Since this information isn't available during
    // simplifiedLayout, we cache the value in m_overflow.
    LayoutUnit oldClientAfterEdge = hasRenderOverflow() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
    computeOverflow(oldClientAfterEdge, true);

    updateLayerTransform();

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
    return true;
}

void RenderBlock::markFixedPositionObjectForLayoutIfNeeded(RenderBox& positionedChild)
{
    if (positionedChild.style().position() != PositionType::Fixed)
        return;

    bool hasStaticBlockPosition = positionedChild.style().hasStaticBlockPosition(isHorizontalWritingMode());
    bool hasStaticInlinePosition = positionedChild.style().hasStaticInlinePosition(isHorizontalWritingMode());
    if (!hasStaticBlockPosition && !hasStaticInlinePosition)
        return;

    auto* parent = positionedChild.parent();
    while (parent && !is<RenderView>(*parent) && parent->style().position() != PositionType::Absolute)
        parent = parent->parent();
    if (!parent || parent->style().position() != PositionType::Absolute)
        return;

    if (hasStaticInlinePosition) {
        LogicalExtentComputedValues computedValues;
        positionedChild.computeLogicalWidthInFragment(computedValues);
        LayoutUnit newLeft = computedValues.m_position;
        if (newLeft != positionedChild.logicalLeft())
            positionedChild.setChildNeedsLayout(MarkOnlyThis);
    } else if (hasStaticBlockPosition) {
        LayoutUnit oldTop = positionedChild.logicalTop();
        positionedChild.updateLogicalHeight();
        if (positionedChild.logicalTop() != oldTop)
            positionedChild.setChildNeedsLayout(MarkOnlyThis);
    }
}

LayoutUnit RenderBlock::marginIntrinsicLogicalWidthForChild(RenderBox& child) const
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child.style().marginStartUsing(&style());
    Length marginRight = child.style().marginEndUsing(&style());
    LayoutUnit margin;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

void RenderBlock::layoutPositionedObject(RenderBox& r, bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    estimateFragmentRangeForBoxChild(r);

    // A fixed position element with an absolute positioned ancestor has no way of knowing if the latter has changed position. So
    // if this is a fixed position element, mark it for layout if it has an abspos ancestor and needs to move with that ancestor, i.e. 
    // it has static position.
    markFixedPositionObjectForLayoutIfNeeded(r);
    if (fixedPositionObjectsOnly) {
        r.layoutIfNeeded();
        return;
    }

    // When a non-positioned block element moves, it may have positioned children that are implicitly positioned relative to the
    // non-positioned block.  Rather than trying to detect all of these movement cases, we just always lay out positioned
    // objects that are positioned implicitly like this.  Such objects are rare, and so in typical DHTML menu usage (where everything is
    // positioned explicitly) this should not incur a performance penalty.
    if (relayoutChildren || (r.style().hasStaticBlockPosition(isHorizontalWritingMode()) && r.parent() != this))
        r.setChildNeedsLayout(MarkOnlyThis);
        
    // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
    if (relayoutChildren && r.needsPreferredWidthsRecalculation())
        r.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
    
    r.markForPaginationRelayoutIfNeeded();
    
    // We don't have to do a full layout.  We just have to update our position. Try that first. If we have shrink-to-fit width
    // and we hit the available width constraint, the layoutIfNeeded() will catch it and do a full layout.
    if (r.needsPositionedMovementLayoutOnly() && r.tryLayoutDoingPositionedMovementOnly())
        r.clearNeedsLayout();
        
    // If we are paginated or in a line grid, compute a vertical position for our object now.
    // If it's wrong we'll lay out again.
    LayoutUnit oldLogicalTop;
    bool needsBlockDirectionLocationSetBeforeLayout = r.needsLayout() && view().frameView().layoutContext().layoutState()->needsBlockDirectionLocationSetBeforeLayout();
    if (needsBlockDirectionLocationSetBeforeLayout) {
        if (isHorizontalWritingMode() == r.isHorizontalWritingMode())
            r.updateLogicalHeight();
        else
            r.updateLogicalWidth();
        oldLogicalTop = logicalTopForChild(r);
    }

    r.layoutIfNeeded();
    
    auto* parent = r.parent();
    bool layoutChanged = false;
    if (parent->isFlexibleBox() && downcast<RenderFlexibleBox>(parent)->setStaticPositionForPositionedLayout(r)) {
        // The static position of an abspos child of a flexbox depends on its size
        // (for example, they can be centered). So we may have to reposition the
        // item after layout.
        // FIXME: We could probably avoid a layout here and just reposition?
        layoutChanged = true;
    }

    // Lay out again if our estimate was wrong.
    if (layoutChanged || (needsBlockDirectionLocationSetBeforeLayout && logicalTopForChild(r) != oldLogicalTop)) {
        r.setChildNeedsLayout(MarkOnlyThis);
        r.layoutIfNeeded();
    }

    if (updateFragmentRangeForBoxChild(r)) {
        r.setNeedsLayout(MarkOnlyThis);
        r.layoutIfNeeded();
    }
    
    if (view().frameView().layoutContext().layoutState()->isPaginated() && is<RenderBlockFlow>(*this))
        downcast<RenderBlockFlow>(*this).adjustSizeContainmentChildForPagination(r, r.logicalTop());
}

void RenderBlock::layoutPositionedObjects(bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;
    
    // Do not cache positionedDescendants->end() in a local variable, since |positionedDescendants| can be mutated
    // as it is walked. We always need to fetch the new end() value dynamically.
    for (auto it = positionedDescendants->begin(); it != positionedDescendants->end(); ++it)
        layoutPositionedObject(**it, relayoutChildren, fixedPositionObjectsOnly);
}

void RenderBlock::markPositionedObjectsForLayout()
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    for (auto it = positionedDescendants->begin(), end = positionedDescendants->end(); it != end; ++it) {
        RenderBox* r = *it;
        r->setChildNeedsLayout();
    }
}

void RenderBlock::markForPaginationRelayoutIfNeeded()
{
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (needsLayout() || !layoutState->isPaginated())
        return;

    if (layoutState->pageLogicalHeightChanged() || (layoutState->pageLogicalHeight() && layoutState->pageLogicalOffset(this, logicalTop()) != pageLogicalOffset()))
        setChildNeedsLayout(MarkOnlyThis);
}

void RenderBlock::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto adjustedPaintOffset = paintOffset + location();
    PaintPhase phase = paintInfo.phase;

    // FIXME: Could eliminate the isDocumentElementRenderer() check if we fix background painting so that the RenderView paints the root's background.
    auto visualContentIsClippedOut = [&](LayoutRect paintingRect) {
        if (isDocumentElementRenderer())
            return false;

        if (paintInfo.paintBehavior.contains(PaintBehavior::CompositedOverflowScrollContent) && hasLayer() && layer()->usesCompositedScrolling())
            return false;

        auto overflowBox = visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        return !overflowBox.intersects(paintingRect);
    };

    if (visualContentIsClippedOut(paintInfo.rect))
        return;

    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, phase, adjustedPaintOffset);

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index. We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if ((phase == PaintPhase::BlockBackground || phase == PaintPhase::ChildBlockBackground) && hasNonVisibleOverflow() && layer()
        && layer()->scrollableArea() && style().visibility() == Visibility::Visible && paintInfo.shouldPaintWithinRoot(*this) && !paintInfo.paintRootBackgroundOnly())
        layer()->scrollableArea()->paintOverflowControls(paintInfo.context(), roundedIntPoint(adjustedPaintOffset), snappedIntRect(paintInfo.rect));
}

void RenderBlock::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Style is non-final if the element has a pending stylesheet before it. We end up with renderers with such styles if a script
    // forces renderer construction by querying something layout dependent.
    // Avoid FOUC by not painting. Switching to final style triggers repaint.
    if (style().isNotFinal() || shouldSkipContent())
        return;

    if (childrenInline())
        paintInlineChildren(paintInfo, paintOffset);
    else {
        PaintPhase newPhase = (paintInfo.phase == PaintPhase::ChildOutlines) ? PaintPhase::Outline : paintInfo.phase;
        newPhase = (newPhase == PaintPhase::ChildBlockBackgrounds) ? PaintPhase::ChildBlockBackground : newPhase;

        // We don't paint our own background, but we do let the kids paint their backgrounds.
        PaintInfo paintInfoForChild(paintInfo);
        paintInfoForChild.phase = newPhase;
        paintInfoForChild.updateSubtreePaintRootForChildren(this);

        if (paintInfo.eventRegionContext)
            paintInfoForChild.paintBehavior.add(PaintBehavior::EventRegionIncludeBackground);

        // FIXME: Paint-time pagination is obsolete and is now only used by embedded WebViews inside AppKit
        // NSViews. Do not add any more code for this.
        bool usePrintRect = !view().printRect().isEmpty();
        paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
    }
}

void RenderBlock::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect))
            return;
    }
}

bool RenderBlock::paintChild(RenderBox& child, PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect, PaintBlockType paintType)
{
    if (child.isExcludedAndPlacedInBorder())
        return true;

    // Check for page-break-before: always, and if it's set, break and bail.
    bool checkBeforeAlways = !childrenInline() && (usePrintRect && alwaysPageBreak(child.style().breakBefore()));
    LayoutUnit absoluteChildY = paintOffset.y() + child.y();
    if (checkBeforeAlways
        && absoluteChildY > paintInfo.rect.y()
        && absoluteChildY < paintInfo.rect.maxY()) {
        view().setBestTruncatedAt(absoluteChildY, this, true);
        return false;
    }

    if (!child.isFloating() && child.isReplacedOrInlineBlock() && usePrintRect && child.height() <= view().printRect().height()) {
        // Paginate block-level replaced elements.
        if (absoluteChildY + child.height() > view().printRect().maxY()) {
            if (absoluteChildY < view().truncatedAt())
                view().setBestTruncatedAt(absoluteChildY, &child);
            // If we were able to truncate, don't paint.
            if (absoluteChildY >= view().truncatedAt())
                return false;
        }
    }

    LayoutPoint childPoint = flipForWritingModeForChild(child, paintOffset);
    if (!child.hasSelfPaintingLayer() && !child.isFloating()) {
        if (paintType == PaintAsInlineBlock)
            child.paintAsInlineBlock(paintInfoForChild, childPoint);
        else
            child.paint(paintInfoForChild, childPoint);
    }

    // Check for page-break-after: always, and if it's set, break and bail.
    bool checkAfterAlways = !childrenInline() && (usePrintRect && alwaysPageBreak(child.style().breakAfter()));
    if (checkAfterAlways
        && (absoluteChildY + child.height()) > paintInfo.rect.y()
        && (absoluteChildY + child.height()) < paintInfo.rect.maxY()) {
        view().setBestTruncatedAt(absoluteChildY + child.height() + std::max<LayoutUnit>(0, child.collapsedMarginAfter()), this, true);
        return false;
    }

    return true;
}

void RenderBlock::paintCaret(PaintInfo& paintInfo, const LayoutPoint& paintOffset, CaretType type)
{
    // Paint the caret if the FrameSelection says so or if caret browsing is enabled
    RenderBlock* caretPainter;
    bool isContentEditable;
    if (type == CursorCaret) {
        caretPainter = frame().selection().caretRendererWithoutUpdatingLayout();
        isContentEditable = frame().selection().selection().hasEditableStyle();
    } else {
        caretPainter = page().dragCaretController().caretRenderer();
        isContentEditable = page().dragCaretController().isContentEditable();
    }

    if (caretPainter == this && (isContentEditable || settings().caretBrowsingEnabled())) {
        if (type == CursorCaret)
            frame().selection().paintCaret(paintInfo.context(), paintOffset, paintInfo.rect);
        else
            page().dragCaretController().paintDragCaret(&frame(), paintInfo.context(), paintOffset, paintInfo.rect);
    }
}

void RenderBlock::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && style().visibility() == Visibility::Visible) {
        if (hasVisibleBoxDecorations())
            paintBoxDecorations(paintInfo, paintOffset);
    }
    
    // Paint legends just above the border before we scroll or clip.
    if (paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground || paintPhase == PaintPhase::Selection)
        paintExcludedChildrenInBorder(paintInfo, paintOffset);
    
    if (paintPhase == PaintPhase::Mask && style().visibility() == Visibility::Visible) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    if (paintPhase == PaintPhase::ClippingMask && style().visibility() == Visibility::Visible) {
        paintClippingMask(paintInfo, paintOffset);
        return;
    }

    // If just painting the root background, then return.
    if (paintInfo.paintRootBackgroundOnly())
        return;

    if (paintPhase == PaintPhase::EventRegion) {
        auto borderRect = LayoutRect(paintOffset, size());

        if (paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeBackground) && visibleToHitTesting()) {
            auto borderRegion = approximateAsRegion(style().getRoundedBorderFor(borderRect));
            LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << *this << " uniting region " << borderRegion);
            paintInfo.eventRegionContext->unite(borderRegion, *this, style(), isTextControl() && downcast<RenderTextControl>(*this).textFormControlElement().isInnerTextElementEditable());
        }

        if (!paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeForeground))
            return;

        bool needsTraverseDescendants = hasVisualOverflow() || containsFloats() || !paintInfo.eventRegionContext->contains(enclosingIntRect(borderRect)) || view().needsEventRegionUpdateForNonCompositedFrame();
        LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << *this << " needsTraverseDescendants for event region: hasVisualOverflow: " << hasVisualOverflow() << " containsFloats: "
            << containsFloats() <<  " border box is outside current region: " << !paintInfo.eventRegionContext->contains(enclosingIntRect(borderRect)) << " needsEventRegionUpdateForNonCompositedFrame:" << view().needsEventRegionUpdateForNonCompositedFrame());
#if ENABLE(TOUCH_ACTION_REGIONS)
        needsTraverseDescendants |= document().mayHaveElementsWithNonAutoTouchAction();
        LOG_WITH_STREAM(EventRegions, stream << "  may have touch-action elements: " << document().mayHaveElementsWithNonAutoTouchAction());
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
        needsTraverseDescendants |= document().hasWheelEventHandlers();
        LOG_WITH_STREAM(EventRegions, stream << "  has wheel event handlers: " << document().hasWheelEventHandlers());
#endif
#if ENABLE(EDITABLE_REGION)
        // We treat the entire text control as editable to match users' expectation even
        // though it's actually the inner text element of the control that is editable.
        // So, no need to traverse to find the inner text element in this case.
        if (!isTextControl()) {
            needsTraverseDescendants |= document().mayHaveEditableElements() && page().shouldBuildEditableRegion();
            LOG_WITH_STREAM(EventRegions, stream << "  needs editable event region: " << (document().mayHaveEditableElements() && page().shouldBuildEditableRegion()));
        }
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        needsTraverseDescendants |= page().shouldBuildInteractionRegions();
#endif

        if (!needsTraverseDescendants)
            return;
    }

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).
    LayoutPoint scrolledOffset = paintOffset;
    scrolledOffset.moveBy(-scrollPosition());

    // Column rules need to account for scrolling and clipping.
    // FIXME: Clipping of column rules does not work. We will need a separate paint phase for column rules I suspect in order to get
    // clipping correct (since it has to paint as background but is still considered "contents").
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && style().visibility() == Visibility::Visible)
        paintColumnRules(paintInfo, scrolledOffset);

    // Done with backgrounds, borders and column rules.
    if (paintPhase == PaintPhase::BlockBackground)
        return;
    
    // 2. paint contents
    if (paintPhase != PaintPhase::SelfOutline)
        paintContents(paintInfo, scrolledOffset);

    // 3. paint selection
    // FIXME: Make this work with multi column layouts.  For now don't fill gaps.
    bool isPrinting = document().printing();
    if (!isPrinting)
        paintSelection(paintInfo, scrolledOffset); // Fill in gaps in selection on lines and between blocks.

    // 4. paint floats.
    if (paintPhase == PaintPhase::Float || paintPhase == PaintPhase::Selection || paintPhase == PaintPhase::TextClip || paintPhase == PaintPhase::EventRegion)
        paintFloats(paintInfo, scrolledOffset, paintPhase == PaintPhase::Selection || paintPhase == PaintPhase::TextClip || paintPhase == PaintPhase::EventRegion);

    // 5. paint outline.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && hasOutline() && style().visibility() == Visibility::Visible)
        paintOutline(paintInfo, LayoutRect(paintOffset, size()));

    // 6. paint continuation outlines.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::ChildOutlines)) {
        RenderInline* inlineCont = inlineContinuation();
        if (inlineCont && inlineCont->hasOutline() && inlineCont->style().visibility() == Visibility::Visible) {
            RenderInline* inlineRenderer = downcast<RenderInline>(inlineCont->element()->renderer());
            RenderBlock* containingBlock = this->containingBlock();

            bool inlineEnclosedInSelfPaintingLayer = false;
            for (RenderBoxModelObject* box = inlineRenderer; box != containingBlock; box = &box->parent()->enclosingBoxModelObject()) {
                if (box->hasSelfPaintingLayer()) {
                    inlineEnclosedInSelfPaintingLayer = true;
                    break;
                }
            }

            // Do not add continuations for outline painting by our containing block if we are a relative positioned
            // anonymous block (i.e. have our own layer), paint them straightaway instead. This is because a block depends on renderers in its continuation table being
            // in the same layer. 
            if (!inlineEnclosedInSelfPaintingLayer && !hasLayer())
                containingBlock->addContinuationWithOutline(inlineRenderer);
            else if (!InlineIterator::firstInlineBoxFor(*inlineRenderer) || (!inlineEnclosedInSelfPaintingLayer && hasLayer()))
                inlineRenderer->paintOutline(paintInfo, paintOffset - locationOffset() + inlineRenderer->containingBlock()->location());
        }
        paintContinuationOutlines(paintInfo, paintOffset);
    }

    // 7. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintPhase::Foreground,
    // then paint the caret.
    if (paintPhase == PaintPhase::Foreground) {
        paintCaret(paintInfo, paintOffset, CursorCaret);
        paintCaret(paintInfo, paintOffset, DragCaret);
    }
}

static ContinuationOutlineTableMap* continuationOutlineTable()
{
    static NeverDestroyed<ContinuationOutlineTableMap> table;
    return &table.get();
}

void RenderBlock::addContinuationWithOutline(RenderInline* flow)
{
    // We can't make this work if the inline is in a layer.  We'll just rely on the broken
    // way of painting.
    ASSERT(!flow->layer() && !flow->isContinuation());
    
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    ListHashSet<RenderInline*>* continuations = table->get(this);
    if (!continuations) {
        continuations = new ListHashSet<RenderInline*>;
        table->set(this, std::unique_ptr<ListHashSet<RenderInline*>>(continuations));
    }
    
    continuations->add(flow);
}

bool RenderBlock::paintsContinuationOutline(RenderInline* flow)
{
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    if (table->isEmpty())
        return false;
        
    ListHashSet<RenderInline*>* continuations = table->get(this);
    if (!continuations)
        return false;

    return continuations->contains(flow);
}

void RenderBlock::paintContinuationOutlines(PaintInfo& info, const LayoutPoint& paintOffset)
{
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    if (table->isEmpty())
        return;
        
    std::unique_ptr<ListHashSet<RenderInline*>> continuations = table->take(this);
    if (!continuations)
        return;

    LayoutPoint accumulatedPaintOffset = paintOffset;
    // Paint each continuation outline.
    ListHashSet<RenderInline*>::iterator end = continuations->end();
    for (ListHashSet<RenderInline*>::iterator it = continuations->begin(); it != end; ++it) {
        // Need to add in the coordinates of the intervening blocks.
        RenderInline* flow = *it;
        RenderBlock* block = flow->containingBlock();
        for ( ; block && block != this; block = block->containingBlock())
            accumulatedPaintOffset.moveBy(block->location());
        ASSERT(block);   
        flow->paintOutline(info, accumulatedPaintOffset);
    }
}

bool RenderBlock::shouldPaintSelectionGaps() const
{
    return selectionState() != HighlightState::None && style().visibility() == Visibility::Visible && isSelectionRoot();
}

bool RenderBlock::isSelectionRoot() const
{
    if (isPseudoElement())
        return false;
    ASSERT(element() || isAnonymous());
        
    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    if (isTable())
        return false;
        
    if (isBody() || isDocumentElementRenderer() || hasNonVisibleOverflow()
        || isPositioned() || isFloating()
        || isTableCell() || isInlineBlockOrInlineTable()
        || hasTransform() || hasReflection() || hasMask() || isWritingModeRoot()
        || isRenderFragmentedFlow() || style().columnSpan() == ColumnSpan::All
        || isFlexItemIncludingDeprecated() || isGridItem())
        return true;
    
    if (view().selection().start()) {
        Node* startElement = view().selection().start()->node();
        if (startElement && startElement->rootEditableElement() == element())
            return true;
    }
    
    return false;
}

GapRects RenderBlock::selectionGapRectsForRepaint(const RenderLayerModelObject* repaintContainer)
{
    ASSERT(!needsLayout());

    if (!shouldPaintSelectionGaps())
        return GapRects();

    FloatPoint containerPoint = localToContainerPoint(FloatPoint(), repaintContainer, UseTransforms);
    LayoutPoint offsetFromRepaintContainer(containerPoint - toFloatSize(scrollPosition()));

    LogicalSelectionOffsetCaches cache(*this);
    LayoutUnit lastTop;
    LayoutUnit lastLeft = logicalLeftSelectionOffset(*this, lastTop, cache);
    LayoutUnit lastRight = logicalRightSelectionOffset(*this, lastTop, cache);
    
    return selectionGaps(*this, offsetFromRepaintContainer, IntSize(), lastTop, lastLeft, lastRight, cache);
}

void RenderBlock::paintSelection(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
#if ENABLE(TEXT_SELECTION)
    if (shouldPaintSelectionGaps() && paintInfo.phase == PaintPhase::Foreground) {
        LogicalSelectionOffsetCaches cache(*this);
        LayoutUnit lastTop;
        LayoutUnit lastLeft = logicalLeftSelectionOffset(*this, lastTop, cache);
        LayoutUnit lastRight = logicalRightSelectionOffset(*this, lastTop, cache);
        GraphicsContextStateSaver stateSaver(paintInfo.context());

        LayoutRect gapRectsBounds = selectionGaps(*this, paintOffset, LayoutSize(), lastTop, lastLeft, lastRight, cache, &paintInfo);
        if (!gapRectsBounds.isEmpty()) {
            if (RenderLayer* layer = enclosingLayer()) {
                gapRectsBounds.moveBy(-paintOffset);
                if (!hasLayer()) {
                    LayoutRect localBounds(gapRectsBounds);
                    flipForWritingMode(localBounds);
                    gapRectsBounds = localToContainerQuad(FloatRect(localBounds), &layer->renderer()).enclosingBoundingBox();
                    if (layer->renderer().isBox())
                        gapRectsBounds.moveBy(layer->renderBox()->scrollPosition());
                }
                layer->addBlockSelectionGapsBounds(gapRectsBounds);
            }
        }
    }
#else
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(paintOffset);
#endif
}

static void clipOutPositionedObjects(const PaintInfo* paintInfo, const LayoutPoint& offset, TrackedRendererListHashSet* positionedObjects)
{
    if (!positionedObjects)
        return;
    
    TrackedRendererListHashSet::const_iterator end = positionedObjects->end();
    for (TrackedRendererListHashSet::const_iterator it = positionedObjects->begin(); it != end; ++it) {
        RenderBox* r = *it;
        paintInfo->context().clipOut(IntRect(offset.x() + r->x(), offset.y() + r->y(), r->width(), r->height()));
    }
}

LayoutUnit blockDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock)
{
    return rootBlock.isHorizontalWritingMode() ? offsetFromRootBlock.height() : offsetFromRootBlock.width();
}

LayoutUnit inlineDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock)
{
    return rootBlock.isHorizontalWritingMode() ? offsetFromRootBlock.width() : offsetFromRootBlock.height();
}

LayoutRect RenderBlock::logicalRectToPhysicalRect(const LayoutPoint& rootBlockPhysicalPosition, const LayoutRect& logicalRect)
{
    LayoutRect result;
    if (isHorizontalWritingMode())
        result = logicalRect;
    else
        result = LayoutRect(logicalRect.y(), logicalRect.x(), logicalRect.height(), logicalRect.width());
    flipForWritingMode(result);
    result.moveBy(rootBlockPhysicalPosition);
    return result;
}

GapRects RenderBlock::selectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    // IMPORTANT: Callers of this method that intend for painting to happen need to do a save/restore.
    // Clip out floating and positioned objects when painting selection gaps.
    if (paintInfo) {
        // Note that we don't clip out overflow for positioned objects.  We just stick to the border box.
        LayoutRect flippedBlockRect(offsetFromRootBlock.width(), offsetFromRootBlock.height(), width(), height());
        rootBlock.flipForWritingMode(flippedBlockRect);
        flippedBlockRect.moveBy(rootBlockPhysicalPosition);
        clipOutPositionedObjects(paintInfo, flippedBlockRect.location(), positionedObjects());
        if (isBody() || isDocumentElementRenderer()) { // The <body> must make sure to examine its containingBlock's positioned objects.
            for (RenderBlock* cb = containingBlock(); cb && !is<RenderView>(*cb); cb = cb->containingBlock())
                clipOutPositionedObjects(paintInfo, LayoutPoint(cb->x(), cb->y()), cb->positionedObjects()); // FIXME: Not right for flipped writing modes.
        }
        clipOutFloatingObjects(rootBlock, paintInfo, rootBlockPhysicalPosition, offsetFromRootBlock);
    }

    // FIXME: overflow: auto/scroll fragments need more math here, since painting in the border box is different from painting in the padding box (one is scrolled, the other is
    // fixed).
    GapRects result;
    if (!isRenderBlockFlow()) // FIXME: Make multi-column selection gap filling work someday.
        return result;

    if (hasTransform() || style().columnSpan() == ColumnSpan::All || isRenderFragmentedFlow()) {
        // FIXME: We should learn how to gap fill multiple columns and transforms eventually.
        lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalHeight();
        lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, logicalHeight(), cache);
        lastLogicalRight = logicalRightSelectionOffset(rootBlock, logicalHeight(), cache);
        return result;
    }

    if (childrenInline())
        result = inlineSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, cache, paintInfo);
    else
        result = blockSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, cache, paintInfo);

    // Fill the vertical gap all the way to the bottom of our block if the selection extends past our block.
    if (&rootBlock == this && (selectionState() != HighlightState::Both && selectionState() != HighlightState::End) && !isRubyBase() && !isRubyText()) {
        result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
            lastLogicalTop, lastLogicalLeft, lastLogicalRight, logicalHeight(), cache, paintInfo));
    }

    return result;
}

GapRects RenderBlock::inlineSelectionGaps(RenderBlock&, const LayoutPoint&, const LayoutSize&, LayoutUnit&, LayoutUnit&, LayoutUnit&, const LogicalSelectionOffsetCaches&, const PaintInfo*)
{
    ASSERT_NOT_REACHED();
    return GapRects();
}

GapRects RenderBlock::blockSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    GapRects result;

    // Jump right to the first block child that contains some selected objects.
    RenderBox* curr;
    for (curr = firstChildBox(); curr && curr->selectionState() == HighlightState::None; curr = curr->nextSiblingBox()) { }
    
    if (!curr)
        return result;

    LogicalSelectionOffsetCaches childCache(*this, cache);

    for (bool sawSelectionEnd = false; curr && !sawSelectionEnd; curr = curr->nextSiblingBox()) {
        HighlightState childState = curr->selectionState();
        if (childState == HighlightState::Both || childState == HighlightState::End)
            sawSelectionEnd = true;

        if (curr->isFloatingOrOutOfFlowPositioned())
            continue; // We must be a normal flow object in order to even be considered.

        if (curr->isInFlowPositioned() && curr->hasLayer()) {
            // If the relposition offset is anything other than 0, then treat this just like an absolute positioned element.
            // Just disregard it completely.
            LayoutSize relOffset = curr->layer()->offsetForInFlowPosition();
            if (relOffset.width() || relOffset.height())
                continue;
        }

        bool paintsOwnSelection = curr->shouldPaintSelectionGaps() || curr->isTable(); // FIXME: Eventually we won't special-case table like this.
        bool fillBlockGaps = (paintsOwnSelection || (curr->canBeSelectionLeaf() && childState != HighlightState::None)) && !isRubyBase() && !isRubyText();
        if (fillBlockGaps) {
            // We need to fill the vertical gap above this object.
            if (childState == HighlightState::End || childState == HighlightState::Inside) {
                // Fill the gap above the object.
                result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
                    lastLogicalTop, lastLogicalLeft, lastLogicalRight, curr->logicalTop(), cache, paintInfo));
            }

            // Only fill side gaps for objects that paint their own selection if we know for sure the selection is going to extend all the way *past*
            // our object.  We know this if the selection did not end inside our object.
            if (paintsOwnSelection && (childState == HighlightState::Start || sawSelectionEnd))
                childState = HighlightState::None;

            // Fill side gaps on this object based off its state.
            bool leftGap, rightGap;
            getSelectionGapInfo(childState, leftGap, rightGap);

            if (leftGap)
                result.uniteLeft(logicalLeftSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, this, curr->logicalLeft(), curr->logicalTop(), curr->logicalHeight(), cache, paintInfo));
            if (rightGap)
                result.uniteRight(logicalRightSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, this, curr->logicalRight(), curr->logicalTop(), curr->logicalHeight(), cache, paintInfo));

            // Update lastLogicalTop to be just underneath the object.  lastLogicalLeft and lastLogicalRight extend as far as
            // they can without bumping into floating or positioned objects.  Ideally they will go right up
            // to the border of the root selection block.
            lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + curr->logicalBottom();
            lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, curr->logicalBottom(), cache);
            lastLogicalRight = logicalRightSelectionOffset(rootBlock, curr->logicalBottom(), cache);
        } else if (childState != HighlightState::None && is<RenderBlock>(*curr)) {
            // We must be a block that has some selected object inside it, so recur.
            result.unite(downcast<RenderBlock>(*curr).selectionGaps(rootBlock, rootBlockPhysicalPosition, LayoutSize(offsetFromRootBlock.width() + curr->x(), offsetFromRootBlock.height() + curr->y()),
                lastLogicalTop, lastLogicalLeft, lastLogicalRight, childCache, paintInfo));
        }
    }
    return result;
}

LayoutRect RenderBlock::blockSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit lastLogicalTop, LayoutUnit lastLogicalLeft, LayoutUnit lastLogicalRight, LayoutUnit logicalBottom, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit logicalTop = lastLogicalTop;
    LayoutUnit logicalHeight = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalBottom - logicalTop;
    if (logicalHeight <= 0)
        return LayoutRect();

    // Get the selection offsets for the bottom of the gap
    LayoutUnit logicalLeft = std::max(lastLogicalLeft, logicalLeftSelectionOffset(rootBlock, logicalBottom, cache));
    LayoutUnit logicalRight = std::min(lastLogicalRight, logicalRightSelectionOffset(rootBlock, logicalBottom, cache));
    LayoutUnit logicalWidth = logicalRight - logicalLeft;
    if (logicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(logicalLeft, logicalTop, logicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context().fillRect(snapRectToDevicePixels(gapRect, document().deviceScaleFactor()), selectionBackgroundColor());
    return gapRect;
}

LayoutRect RenderBlock::logicalLeftSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock, RenderElement* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = std::max(logicalLeftSelectionOffset(rootBlock, logicalTop, cache), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight, cache));
    LayoutUnit rootBlockLogicalRight = std::min(inlineDirectionOffset(rootBlock, offsetFromRootBlock) + logicalLeft,
        std::min(logicalRightSelectionOffset(rootBlock, logicalTop, cache), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight, cache)));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context().fillRect(snapRectToDevicePixels(gapRect, document().deviceScaleFactor()), selObj->selectionBackgroundColor());
    return gapRect;
}

LayoutRect RenderBlock::logicalRightSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock, RenderElement* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = std::max(inlineDirectionOffset(rootBlock, offsetFromRootBlock) + logicalRight,
        std::max(logicalLeftSelectionOffset(rootBlock, logicalTop, cache), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight, cache)));
    LayoutUnit rootBlockLogicalRight = std::min(logicalRightSelectionOffset(rootBlock, logicalTop, cache), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight, cache));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context().fillRect(snapRectToDevicePixels(gapRect, document().deviceScaleFactor()), selObj->selectionBackgroundColor());
    return gapRect;
}

void RenderBlock::getSelectionGapInfo(HighlightState state, bool& leftGap, bool& rightGap)
{
    bool ltr = style().isLeftToRightDirection();
    leftGap = (state == RenderObject::HighlightState::Inside) || (state == RenderObject::HighlightState::End && ltr) || (state == RenderObject::HighlightState::Start && !ltr);
    rightGap = (state == RenderObject::HighlightState::Inside) || (state == RenderObject::HighlightState::Start && ltr) || (state == RenderObject::HighlightState::End && !ltr);
}

LayoutUnit RenderBlock::logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches& cache)
{
    LayoutUnit logicalLeft = logicalLeftOffsetForLine(position, DoNotIndentText);
    if (logicalLeft == logicalLeftOffsetForContent()) {
        if (&rootBlock != this) // The border can potentially be further extended by our containingBlock().
            return cache.containingBlockInfo(*this).logicalLeftSelectionOffset(rootBlock, position + logicalTop());
        return logicalLeft;
    }

    RenderBlock* cb = this;
    const LogicalSelectionOffsetCaches* currentCache = &cache;
    while (cb != &rootBlock) {
        logicalLeft += cb->logicalLeft();

        ASSERT(currentCache);
        auto info = currentCache->containingBlockInfo(*cb);
        cb = info.block();
        if (!cb)
            break;
        currentCache = info.cache();
    }
    return logicalLeft;
}

LayoutUnit RenderBlock::logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches& cache)
{
    LayoutUnit logicalRight = logicalRightOffsetForLine(position, DoNotIndentText);
    if (logicalRight == logicalRightOffsetForContent()) {
        if (&rootBlock != this) // The border can potentially be further extended by our containingBlock().
            return cache.containingBlockInfo(*this).logicalRightSelectionOffset(rootBlock, position + logicalTop());
        return logicalRight;
    }

    RenderBlock* cb = this;
    const LogicalSelectionOffsetCaches* currentCache = &cache;
    while (cb != &rootBlock) {
        logicalRight += cb->logicalLeft();

        ASSERT(currentCache);
        auto info = currentCache->containingBlockInfo(*cb);
        cb = info.block();
        if (!cb)
            break;
        currentCache = info.cache();
    }
    return logicalRight;
}

TrackedRendererListHashSet* RenderBlock::positionedObjects() const
{
    return positionedDescendantsMap().positionedRenderers(*this);
}

void RenderBlock::insertPositionedObject(RenderBox& positioned)
{
    ASSERT(!isAnonymousBlock());

    positioned.clearOverridingContainingBlockContentSize();

    if (positioned.isRenderFragmentedFlow())
        return;
    // FIXME: Find out if we can do this as part of positioned.setChildNeedsLayout(MarkOnlyThis)
    if (positioned.needsLayout()) {
        // We should turn this bit on only while in layout.
        ASSERT(posChildNeedsLayout() || view().frameView().layoutContext().isInLayout());
        setPosChildNeedsLayoutBit(true);
    }
    positionedDescendantsMap().addDescendant(*this, positioned);
}

void RenderBlock::removePositionedObject(const RenderBox& rendererToRemove)
{
    positionedDescendantsMap().removeDescendant(rendererToRemove);
}

void RenderBlock::removePositionedObjects(const RenderBlock* newContainingBlockCandidate, ContainingBlockState containingBlockState)
{
    auto* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;
    
    Vector<RenderBox*, 16> renderersToRemove;
    for (auto* renderer : *positionedDescendants) {
        if (newContainingBlockCandidate && !renderer->isDescendantOf(newContainingBlockCandidate))
            continue;
        renderersToRemove.append(renderer);
        if (containingBlockState == NewContainingBlock) {
            renderer->setChildNeedsLayout(MarkOnlyThis);
            if (renderer->needsPreferredWidthsRecalculation())
                renderer->setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
        }
        // It is the parent block's job to add positioned children to positioned objects list of its containing block.
        // Dirty the parent to ensure this happens. We also need to make sure the new containing block is dirty as well so
        // that it gets to these new positioned objects.
        auto* parent = renderer->parent();
        while (parent && !is<RenderBlock>(parent))
            parent = parent->parent();
        if (parent)
            parent->setChildNeedsLayout();

        if (renderer->isFixedPositioned())
            view().setNeedsLayout();
        else if (auto* newContainingBlock = containingBlock()) {
            // During style change, at this point the renderer's containing block is still "this" renderer, and "this" renderer is still positioned.
            // FIXME: During subtree moving, this is mostly invalid but either the subtree is detached (we don't even get here) or renderers
            // are already marked dirty.
            for (; newContainingBlock && !newContainingBlock->canContainAbsolutelyPositionedObjects(); newContainingBlock = newContainingBlock->containingBlock()) { }
            if (newContainingBlock)
                newContainingBlock->setNeedsLayout();
        }
    }
    for (auto* renderer : renderersToRemove)
        removePositionedObject(*renderer);
}

void RenderBlock::addPercentHeightDescendant(RenderBox& descendant)
{
    insertIntoTrackedRendererMaps(*this, descendant);
}

void RenderBlock::removePercentHeightDescendant(RenderBox& descendant)
{
    removeFromTrackedRendererMaps(descendant);
}

TrackedRendererListHashSet* RenderBlock::percentHeightDescendants() const
{
    return percentHeightDescendantsMap ? percentHeightDescendantsMap->get(this) : nullptr;
}

bool RenderBlock::hasPercentHeightContainerMap()
{
    return percentHeightContainerMap;
}

bool RenderBlock::hasPercentHeightDescendant(RenderBox& descendant)
{
    // We don't null check percentHeightContainerMap since the caller
    // already ensures this and we need to call this function on every
    // descendant in clearPercentHeightDescendantsFrom().
    ASSERT(percentHeightContainerMap);
    return percentHeightContainerMap->contains(&descendant);
}

void RenderBlock::removePercentHeightDescendantIfNeeded(RenderBox& descendant)
{
    // We query the map directly, rather than looking at style's
    // logicalHeight()/logicalMinHeight()/logicalMaxHeight() since those
    // can change with writing mode/directional changes.
    if (!hasPercentHeightContainerMap())
        return;

    if (!hasPercentHeightDescendant(descendant))
        return;

    removePercentHeightDescendant(descendant);
}

void RenderBlock::clearPercentHeightDescendantsFrom(RenderBox& parent)
{
    ASSERT(percentHeightContainerMap);
    for (RenderObject* child = parent.firstChild(); child; child = child->nextInPreOrder(&parent)) {
        if (!is<RenderBox>(*child))
            continue;
 
        auto& box = downcast<RenderBox>(*child);
        if (!hasPercentHeightDescendant(box))
            continue;

        removePercentHeightDescendant(box);
    }
}

bool RenderBlock::isContainingBlockAncestorFor(RenderObject& renderer) const
{
    for (const auto* ancestor = renderer.containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor == this)
            return true;
    }
    return false;
}

LayoutUnit RenderBlock::textIndentOffset() const
{
    LayoutUnit cw;
    if (style().textIndent().isPercentOrCalculated())
        cw = availableLogicalWidth();
    return minimumValueForLength(style().textIndent(), cw);
}

LayoutUnit RenderBlock::logicalLeftOffsetForContent(RenderFragmentContainer* fragment) const
{
    LayoutUnit logicalLeftOffset = style().isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop();
    if (shouldPlaceVerticalScrollbarOnLeft() && isHorizontalWritingMode())
        logicalLeftOffset += verticalScrollbarWidth();
    if (!fragment)
        return logicalLeftOffset;
    LayoutRect boxRect = borderBoxRectInFragment(fragment);
    return logicalLeftOffset + (isHorizontalWritingMode() ? boxRect.x() : boxRect.y());
}

LayoutUnit RenderBlock::logicalRightOffsetForContent(RenderFragmentContainer* fragment) const
{
    LayoutUnit logicalRightOffset = style().isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop();
    if (shouldPlaceVerticalScrollbarOnLeft() && isHorizontalWritingMode())
        logicalRightOffset += verticalScrollbarWidth();
    logicalRightOffset += availableLogicalWidth();
    if (!fragment)
        return logicalRightOffset;
    LayoutRect boxRect = borderBoxRectInFragment(fragment);
    return logicalRightOffset - (logicalWidth() - (isHorizontalWritingMode() ? boxRect.maxX() : boxRect.maxY()));
}

LayoutUnit RenderBlock::adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const
{
    LayoutUnit left = offsetFromFloats;

    if (applyTextIndent && style().isLeftToRightDirection())
        left += textIndentOffset();

    if (style().lineAlign() == LineAlign::None)
        return left;
    
    // Push in our left offset so that it is aligned with the character grid.
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return left;

    RenderBlock* lineGrid = layoutState->lineGrid();
    if (!lineGrid || lineGrid->style().writingMode() != style().writingMode())
        return left;

    // FIXME: Should letter-spacing apply? This is complicated since it doesn't apply at the edge?
    float maxCharWidth = lineGrid->style().fontCascade().primaryFont().maxCharWidth();
    if (!maxCharWidth)
        return left;

    LayoutUnit lineGridOffset = lineGrid->isHorizontalWritingMode() ? layoutState->lineGridOffset().width(): layoutState->lineGridOffset().height();
    LayoutUnit layoutOffset = lineGrid->isHorizontalWritingMode() ? layoutState->layoutOffset().width() : layoutState->layoutOffset().height();
    
    // Push in to the nearest character width (truncated so that we pixel snap left).
    // FIXME: Should be patched when subpixel layout lands, since this calculation doesn't have to pixel snap
    // any more (https://bugs.webkit.org/show_bug.cgi?id=79946).
    // FIXME: This is wrong for RTL (https://bugs.webkit.org/show_bug.cgi?id=79945).
    // FIXME: This doesn't work with columns or fragments (https://bugs.webkit.org/show_bug.cgi?id=79942).
    // FIXME: This doesn't work when the inline position of the object isn't set ahead of time.
    // FIXME: Dynamic changes to the font or to the inline position need to result in a deep relayout.
    // (https://bugs.webkit.org/show_bug.cgi?id=79944)
    float remainder = fmodf(maxCharWidth - fmodf(left + layoutOffset - lineGridOffset, maxCharWidth), maxCharWidth);
    left += remainder;
    return left;
}

LayoutUnit RenderBlock::adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const
{
    LayoutUnit right = offsetFromFloats;
    
    if (applyTextIndent && !style().isLeftToRightDirection())
        right -= textIndentOffset();
    
    if (style().lineAlign() == LineAlign::None)
        return right;
    
    // Push in our right offset so that it is aligned with the character grid.
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return right;

    RenderBlock* lineGrid = layoutState->lineGrid();
    if (!lineGrid || lineGrid->style().writingMode() != style().writingMode())
        return right;

    // FIXME: Should letter-spacing apply? This is complicated since it doesn't apply at the edge?
    float maxCharWidth = lineGrid->style().fontCascade().primaryFont().maxCharWidth();
    if (!maxCharWidth)
        return right;

    LayoutUnit lineGridOffset = lineGrid->isHorizontalWritingMode() ? layoutState->lineGridOffset().width(): layoutState->lineGridOffset().height();
    LayoutUnit layoutOffset = lineGrid->isHorizontalWritingMode() ? layoutState->layoutOffset().width() : layoutState->layoutOffset().height();
    
    // Push in to the nearest character width (truncated so that we pixel snap right).
    // FIXME: Should be patched when subpixel layout lands, since this calculation doesn't have to pixel snap
    // any more (https://bugs.webkit.org/show_bug.cgi?id=79946).
    // FIXME: This is wrong for RTL (https://bugs.webkit.org/show_bug.cgi?id=79945).
    // FIXME: This doesn't work with columns or fragments (https://bugs.webkit.org/show_bug.cgi?id=79942).
    // FIXME: This doesn't work when the inline position of the object isn't set ahead of time.
    // FIXME: Dynamic changes to the font or to the inline position need to result in a deep relayout.
    // (https://bugs.webkit.org/show_bug.cgi?id=79944)
    float remainder = fmodf(fmodf(right + layoutOffset - lineGridOffset, maxCharWidth), maxCharWidth);
    right -= ceilf(remainder);
    return right;
}

bool RenderBlock::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!scrollsOverflow())
        return false;
    if (auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr)
        return scrollableArea->hitTestOverflowControls(result, roundedIntPoint(locationInContainer - toLayoutSize(accumulatedOffset)));
    return false;
}

Node* RenderBlock::nodeForHitTest() const
{
    // If we are in the margins of block elements that are part of a
    // continuation we're actually still inside the enclosing element
    // that was split. Use the appropriate inner node.
    return continuation() ? continuation()->element() : element();
}

bool RenderBlock::hitTestChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction hitTestAction)
{
    // Hit test descendants first.
    const LayoutSize localOffset = toLayoutSize(adjustedLocation);
    const LayoutSize scrolledOffset(localOffset - toLayoutSize(scrollPosition()));

    if (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, toLayoutPoint(scrolledOffset)))
        return true;
    if (hitTestContents(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
        updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
        return true;
    }
    return false;
}

bool RenderBlock::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    const LayoutPoint adjustedLocation(accumulatedOffset + location());
    const LayoutSize localOffset = toLayoutSize(adjustedLocation);

    // Check if we need to do anything at all.
    if (!hitTestVisualOverflow(locationInContainer, accumulatedOffset))
        return false;

    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground)
        && visibleToHitTesting(request) && isPointInOverflowControl(result, locationInContainer.point(), adjustedLocation)) {
        updateHitTestResult(result, locationInContainer.point() - localOffset);
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer) == HitTestProgress::Stop)
           return true;
    }

    if (!hitTestClipPath(locationInContainer, accumulatedOffset))
        return false;

    // If we have clipping, then we can't have any spillout.
    bool useClip = (hasControlClip() || hasNonVisibleOverflow());
    bool checkChildren = !useClip || (hasControlClip() ? locationInContainer.intersects(controlClipRect(adjustedLocation)) : locationInContainer.intersects(overflowClipRect(adjustedLocation, nullptr, IncludeOverlayScrollbarSize)));
    if (checkChildren && hitTestChildren(request, result, locationInContainer, adjustedLocation, hitTestAction))
        return true;

    if (!checkChildren && hitTestExcludedChildrenInBorder(request, result, locationInContainer, adjustedLocation, hitTestAction))
        return true;

    if (!hitTestBorderRadius(locationInContainer, accumulatedOffset))
        return false;

    // Now hit test our background
    if (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (visibleToHitTesting(request) && locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
                return true;
        }
    }

    return false;
}

bool RenderBlock::hitTestContents(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (childrenInline() && !isTable())
        return hitTestInlineChildren(request, result, locationInContainer, accumulatedOffset, hitTestAction);

    // Hit test our children.
    HitTestAction childHitTest = hitTestAction;
    if (hitTestAction == HitTestChildBlockBackgrounds)
        childHitTest = HitTestChildBlockBackground;
    for (auto* child = lastChildBox(); child; child = child->previousSiblingBox()) {
        LayoutPoint childPoint = flipForWritingModeForChild(*child, accumulatedOffset);
        if (!child->hasSelfPaintingLayer() && !child->isFloating() && child->nodeAtPoint(request, result, locationInContainer, childPoint, childHitTest))
            return true;
    }

    return false;
}

static inline bool isEditingBoundary(RenderElement* ancestor, RenderObject& child)
{
    ASSERT(!ancestor || ancestor->nonPseudoElement());
    ASSERT(child.nonPseudoNode());
    return !ancestor || !ancestor->parent() || (ancestor->hasLayer() && ancestor->parent()->isRenderView())
        || ancestor->nonPseudoElement()->hasEditableStyle() == child.nonPseudoNode()->hasEditableStyle();
}

// FIXME: This function should go on RenderObject as an instance method. Then
// all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
VisiblePosition positionForPointRespectingEditingBoundaries(RenderBlock& parent, RenderBox& child, const LayoutPoint& pointInParentCoordinates)
{
    LayoutPoint childLocation = child.location();
    if (child.isInFlowPositioned())
        childLocation += child.offsetForInFlowPosition();

    // FIXME: This is wrong if the child's writing-mode is different from the parent's.
    LayoutPoint pointInChildCoordinates(toLayoutPoint(pointInParentCoordinates - childLocation));

    // If this is an anonymous renderer, we just recur normally
    Element* childElement= child.nonPseudoElement();
    if (!childElement)
        return child.positionForPoint(pointInChildCoordinates, nullptr);

    // Otherwise, first make sure that the editability of the parent and child agree.
    // If they don't agree, then we return a visible position just before or after the child
    RenderElement* ancestor = &parent;
    while (ancestor && !ancestor->nonPseudoElement())
        ancestor = ancestor->parent();

    // If we can't find an ancestor to check editability on, or editability is unchanged, we recur like normal
    if (isEditingBoundary(ancestor, child))
        return child.positionForPoint(pointInChildCoordinates, nullptr);

    // Otherwise return before or after the child, depending on if the click was to the logical left or logical right of the child
    LayoutUnit childMiddle = parent.logicalWidthForChild(child) / 2;
    LayoutUnit logicalLeft = parent.isHorizontalWritingMode() ? pointInChildCoordinates.x() : pointInChildCoordinates.y();
    if (logicalLeft < childMiddle)
        return ancestor->createVisiblePosition(childElement->computeNodeIndex(), Affinity::Downstream);
    return ancestor->createVisiblePosition(childElement->computeNodeIndex() + 1, Affinity::Upstream);
}

VisiblePosition RenderBlock::positionForPointWithInlineChildren(const LayoutPoint&, const RenderFragmentContainer*)
{
    ASSERT_NOT_REACHED();
    return VisiblePosition();
}

static inline bool isChildHitTestCandidate(const RenderBox& box)
{
    return box.height() && box.style().visibility() == Visibility::Visible && !box.isOutOfFlowPositioned() && !box.isRenderFragmentedFlow();
}

// Valid candidates in a FragmentedFlow must be rendered by the fragment.
static inline bool isChildHitTestCandidate(const RenderBox& box, const RenderFragmentContainer* fragment, const LayoutPoint& point)
{
    if (!isChildHitTestCandidate(box))
        return false;
    if (!fragment)
        return true;
    const RenderBlock& block = is<RenderBlock>(box) ? downcast<RenderBlock>(box) : *box.containingBlock();
    return block.fragmentAtBlockOffset(point.y()) == fragment;
}

VisiblePosition RenderBlock::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer* fragment)
{
    if (isTable())
        return RenderBox::positionForPoint(point, fragment);

    if (isReplacedOrInlineBlock()) {
        // FIXME: This seems wrong when the object's writing-mode doesn't match the line's writing-mode.
        LayoutUnit pointLogicalLeft = isHorizontalWritingMode() ? point.x() : point.y();
        LayoutUnit pointLogicalTop = isHorizontalWritingMode() ? point.y() : point.x();

        if (pointLogicalTop < 0)
            return createVisiblePosition(caretMinOffset(), Affinity::Downstream);
        if (pointLogicalLeft >= logicalWidth())
            return createVisiblePosition(caretMaxOffset(), Affinity::Downstream);
        if (pointLogicalTop < 0)
            return createVisiblePosition(caretMinOffset(), Affinity::Downstream);
        if (pointLogicalTop >= logicalHeight())
            return createVisiblePosition(caretMaxOffset(), Affinity::Downstream);
    }
    if (isFlexibleBoxIncludingDeprecated() || isRenderGrid())
        return RenderBox::positionForPoint(point, fragment);

    LayoutPoint pointInContents = point;
    offsetForContents(pointInContents);
    LayoutPoint pointInLogicalContents(pointInContents);
    if (!isHorizontalWritingMode())
        pointInLogicalContents = pointInLogicalContents.transposedPoint();

    if (childrenInline())
        return positionForPointWithInlineChildren(pointInLogicalContents, fragment);

    RenderBox* lastCandidateBox = lastChildBox();

    if (!fragment)
        fragment = fragmentAtBlockOffset(pointInLogicalContents.y());

    while (lastCandidateBox && !isChildHitTestCandidate(*lastCandidateBox, fragment, pointInLogicalContents))
        lastCandidateBox = lastCandidateBox->previousSiblingBox();

    bool blocksAreFlipped = style().isFlippedBlocksWritingMode();
    if (lastCandidateBox) {
        if (pointInLogicalContents.y() > logicalTopForChild(*lastCandidateBox)
            || (!blocksAreFlipped && pointInLogicalContents.y() == logicalTopForChild(*lastCandidateBox)))
            return positionForPointRespectingEditingBoundaries(*this, *lastCandidateBox, pointInContents);

        for (auto* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
            if (!isChildHitTestCandidate(*childBox, fragment, pointInLogicalContents))
                continue;
            auto childLogicalBottom = logicalTopForChild(*childBox) + logicalHeightForChild(*childBox);
            if (is<RenderBlockFlow>(childBox))
                childLogicalBottom = std::max(childLogicalBottom, downcast<RenderBlockFlow>(*childBox).lowestFloatLogicalBottom());
            // We hit child if our click is above the bottom of its padding box (like IE6/7 and FF3).
            if (pointInLogicalContents.y() < childLogicalBottom || (blocksAreFlipped && pointInLogicalContents.y() == childLogicalBottom))
                return positionForPointRespectingEditingBoundaries(*this, *childBox, pointInContents);
        }
    }

    // We only get here if there are no hit test candidate children below the click.
    return RenderBox::positionForPoint(point, fragment);
}

void RenderBlock::offsetForContents(LayoutPoint& offset) const
{
    offset = flipForWritingMode(offset);
    offset += toLayoutSize(scrollPosition());
    offset = flipForWritingMode(offset);
}

void RenderBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    ASSERT(!childrenInline());
    if (shouldApplySizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = width.value();
            maxLogicalWidth = width.value();
        }
    } else if (!shouldApplyInlineSizeContainment())
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    int scrollbarWidth = intrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void RenderBlock::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    const RenderStyle& styleToUse = style();
    const auto& lengthToUse = hasOverridingLogicalWidthLength() ? overridingLogicalWidthLength() : styleToUse.logicalWidth();
    if (!isTableCell() && lengthToUse.isFixed() && lengthToUse.value() >= 0
        && !(isDeprecatedFlexItem() && !lengthToUse.intValue()))
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(lengthToUse);
    else if (shouldComputeLogicalWidthFromAspectRatio()) {
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = (computeLogicalWidthFromAspectRatio() - borderAndPaddingLogicalWidth());
        m_minPreferredLogicalWidth = std::max(0_lu, m_minPreferredLogicalWidth);
        m_maxPreferredLogicalWidth = std::max(0_lu, m_maxPreferredLogicalWidth);
    } else 
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(styleToUse.logicalMinWidth(), styleToUse.logicalMaxWidth(), borderAndPaddingLogicalWidth());

    setPreferredLogicalWidthsDirty(false);
}

void RenderBlock::computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    ASSERT(!shouldApplyInlineSizeContainment());

    const RenderStyle& styleToUse = style();
    bool nowrap = styleToUse.whiteSpace() == WhiteSpace::NoWrap;

    RenderObject* child = firstChild();
    RenderBlock* containingBlock = this->containingBlock();
    LayoutUnit floatLeftWidth, floatRightWidth;

    LayoutUnit childMinWidth;
    LayoutUnit childMaxWidth;
    bool hadExcludedChildren = computePreferredWidthsForExcludedChildren(childMinWidth, childMaxWidth);
    if (hadExcludedChildren) {
        minLogicalWidth = std::max(childMinWidth, minLogicalWidth);
        maxLogicalWidth = std::max(childMaxWidth, maxLogicalWidth);
    }

    while (child) {
        // Positioned children don't affect the min/max width. Legends in fieldsets are skipped here
        // since they compute outside of any one layout system. Other children excluded from
        // normal layout are only used with block flows, so it's ok to calculate them here.
        if (child->isOutOfFlowPositioned() || child->isExcludedAndPlacedInBorder()) {
            child = child->nextSibling();
            continue;
        }

        const RenderStyle& childStyle = child->style();
        if (child->isFloating() || (is<RenderBox>(*child) && downcast<RenderBox>(*child).avoidsFloats())) {
            LayoutUnit floatTotalWidth = floatLeftWidth + floatRightWidth;
            auto childUsedClear = RenderStyle::usedClear(*child);
            if (childUsedClear == UsedClear::Left || childUsedClear == UsedClear::Both) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatLeftWidth = 0;
            }
            if (childUsedClear == UsedClear::Right || childUsedClear == UsedClear::Both) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatRightWidth = 0;
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        Length startMarginLength = childStyle.marginStartUsing(&styleToUse);
        Length endMarginLength = childStyle.marginEndUsing(&styleToUse);
        LayoutUnit margin;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
        if (startMarginLength.isFixed())
            marginStart += startMarginLength.value();
        if (endMarginLength.isFixed())
            marginEnd += endMarginLength.value();
        margin = marginStart + marginEnd;

        LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
        computeChildPreferredLogicalWidths(*child, childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);

        LayoutUnit w = childMinPreferredLogicalWidth + margin;
        minLogicalWidth = std::max(w, minLogicalWidth);

        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !child->isTable())
            maxLogicalWidth = std::max(w, maxLogicalWidth);

        w = childMaxPreferredLogicalWidth + margin;

        if (!child->isFloating()) {
            if (is<RenderBox>(*child) && downcast<RenderBox>(*child).avoidsFloats()) {
                // Determine a left and right max value based off whether or not the floats can fit in the
                // margins of the object.  For negative margins, we will attempt to overlap the float if the negative margin
                // is smaller than the float width.
                bool ltr = containingBlock ? containingBlock->style().isLeftToRightDirection() : styleToUse.isLeftToRightDirection();
                LayoutUnit marginLogicalLeft = ltr ? marginStart : marginEnd;
                LayoutUnit marginLogicalRight = ltr ? marginEnd : marginStart;
                LayoutUnit maxLeft = marginLogicalLeft > 0 ? std::max(floatLeftWidth, marginLogicalLeft) : floatLeftWidth + marginLogicalLeft;
                LayoutUnit maxRight = marginLogicalRight > 0 ? std::max(floatRightWidth, marginLogicalRight) : floatRightWidth + marginLogicalRight;
                w = childMaxPreferredLogicalWidth + maxLeft + maxRight;
                w = std::max(w, floatLeftWidth + floatRightWidth);
            }
            else
                maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
            floatLeftWidth = floatRightWidth = 0;
        }

        if (child->isFloating()) {
            if (RenderStyle::usedFloat(*child) == UsedFloat::Left)
                floatLeftWidth += w;
            else
                floatRightWidth += w;
        } else
            maxLogicalWidth = std::max(w, maxLogicalWidth);

        child = child->nextSibling();
    }

    // Always make sure these values are non-negative.
    minLogicalWidth = std::max<LayoutUnit>(0, minLogicalWidth);
    maxLogicalWidth = std::max<LayoutUnit>(0, maxLogicalWidth);

    maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
}

void RenderBlock::computeChildIntrinsicLogicalWidths(RenderObject& child, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const
{
    minPreferredLogicalWidth = child.minPreferredLogicalWidth();
    maxPreferredLogicalWidth = child.maxPreferredLogicalWidth();
}

void RenderBlock::computeChildPreferredLogicalWidths(RenderObject& child, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const
{
    if (child.isBox() && child.isHorizontalWritingMode() != isHorizontalWritingMode()) {
        // If the child is an orthogonal flow, child's height determines the width,
        // but the height is not available until layout.
        // http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-shrink-to-fit
        if (!child.needsLayout()) {
            minPreferredLogicalWidth = maxPreferredLogicalWidth = downcast<RenderBox>(child).logicalHeight();
            return;
        }
        auto& box = downcast<RenderBox>(child);
        if (box.shouldComputeLogicalHeightFromAspectRatio() && box.style().logicalWidth().isFixed()) {
            LayoutUnit logicalWidth = LayoutUnit(box.style().logicalWidth().value());
            minPreferredLogicalWidth = maxPreferredLogicalWidth = blockSizeFromAspectRatio(box.horizontalBorderAndPaddingExtent(), box.verticalBorderAndPaddingExtent(), LayoutUnit(box.style().logicalAspectRatio()), box.style().boxSizingForAspectRatio(), logicalWidth);
            return;
        }
        minPreferredLogicalWidth = maxPreferredLogicalWidth = box.computeLogicalHeightWithoutLayout();
        return;
    }
    
    computeChildIntrinsicLogicalWidths(child, minPreferredLogicalWidth, maxPreferredLogicalWidth);

    // For non-replaced blocks if the inline size is min|max-content or a definite
    // size the min|max-content contribution is that size plus border, padding and
    // margin https://drafts.csswg.org/css-sizing/#block-intrinsic
    if (child.isRenderBlock()) {
        const Length& computedInlineSize = child.style().logicalWidth();
        if (computedInlineSize.isMaxContent())
            minPreferredLogicalWidth = maxPreferredLogicalWidth;
        else if (computedInlineSize.isMinContent())
            maxPreferredLogicalWidth = minPreferredLogicalWidth;
    }
}

bool RenderBlock::hasLineIfEmpty() const
{
    if (!element())
        return false;
    
    if (element()->isRootEditableElement())
        return true;
    
    return false;
}

LayoutUnit RenderBlock::lineHeight(bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplacedOrInlineBlock() && linePositionMode == PositionOnContainingLine)
        return RenderBox::lineHeight(firstLine, direction, linePositionMode);

    auto& lineStyle = firstLine ? firstLineStyle() : style();
    return lineStyle.computedLineHeight();
}

LayoutUnit RenderBlock::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplacedOrInlineBlock() && linePositionMode == PositionOnContainingLine) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        // FIXME: Need to patch form controls to deal with vertical lines.
        if (style().hasEffectiveAppearance() && !theme().isControlContainer(style().effectiveAppearance()))
            return theme().baselinePosition(*this);
            
        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow.  We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves).  This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved).
        auto ignoreBaseline = [this, direction]() -> bool {
            if (isWritingModeRoot() && !isRubyRun())
                return true;

            auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
            if (!scrollableArea)
                return false;

            if (scrollableArea->marquee())
                return true;

            if (direction == HorizontalLine)
                return scrollableArea->verticalScrollbar() || scrollableArea->scrollOffset().y();
            return scrollableArea->horizontalScrollbar() || scrollableArea->scrollOffset().x();
        };

        auto baselinePos = ignoreBaseline() ? std::optional<LayoutUnit>() : inlineBlockBaseline(direction);
        
        if (isDeprecatedFlexibleBox()) {
            // Historically, we did this check for all baselines. But we can't
            // remove this code from deprecated flexbox, because it effectively
            // breaks -webkit-line-clamp, which is used in the wild -- we would
            // calculate the baseline as if -webkit-line-clamp wasn't used.
            // For simplicity, we use this for all uses of deprecated flexbox.
            LayoutUnit bottomOfContent = direction == HorizontalLine ? borderTop() + paddingTop() + contentHeight() : borderRight() + paddingRight() + contentWidth();
            if (baselinePos && baselinePos.value() > bottomOfContent)
                baselinePos = std::optional<LayoutUnit>();
        }
        if (baselinePos)
            return direction == HorizontalLine ? marginTop() + baselinePos.value() : marginRight() + baselinePos.value();

        return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
    }

    const RenderStyle& style = firstLine ? firstLineStyle() : this->style();
    const FontMetrics& fontMetrics = style.metricsOfPrimaryFont();
    return LayoutUnit { fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2 }.toInt();
}

LayoutUnit RenderBlock::minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const
{
    if (!document().inNoQuirksMode() && replacedHeight)
        return replacedHeight;

    const RenderStyle& style = isFirstLine ? firstLineStyle() : this->style();
    if (!(style.lineBoxContain().contains(LineBoxContain::Block)))
        return 0;

    return std::max<LayoutUnit>(replacedHeight, lineHeight(isFirstLine, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

std::optional<LayoutUnit> RenderBlock::firstLineBaseline() const
{
    if (shouldApplyLayoutContainment())
        return std::nullopt;

    if (isWritingModeRoot() && !isRubyRun())
        return std::optional<LayoutUnit>();

    for (RenderBox* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        if (auto baseline = child->firstLineBaseline())
            return LayoutUnit { child->logicalTop() + baseline.value() };
    }
    return std::optional<LayoutUnit>();
}

std::optional<LayoutUnit> RenderBlock::lastLineBaseline() const
{
    if (shouldApplyLayoutContainment())
        return std::nullopt;

    if (isWritingModeRoot() && !isRubyRun())
        return std::optional<LayoutUnit>();

    for (RenderBox* child = lastInFlowChildBox(); child; child = child->previousInFlowSiblingBox()) {
        if (auto baseline = child->lastLineBaseline())
            return LayoutUnit { baseline.value() + child->logicalTop() };
    } 
    return std::optional<LayoutUnit>();
}

std::optional<LayoutUnit> RenderBlock::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    if (shouldApplyLayoutContainment())
        return synthesizedBaselineFromBorderBox(*this, lineDirection) + (lineDirection == HorizontalLine ? marginBottom() : marginLeft());

    if (isWritingModeRoot() && !isRubyRun())
        return std::optional<LayoutUnit>();

    bool haveNormalFlowChild = false;
    for (auto* box = lastChildBox(); box; box = box->previousSiblingBox()) {
        if (box->isFloatingOrOutOfFlowPositioned())
            continue;
        haveNormalFlowChild = true;
        if (auto result = box->inlineBlockBaseline(lineDirection))
            return LayoutUnit { (box->logicalTop() + result.value()).toInt() }; // Translate to our coordinate space.
    }

    if (!haveNormalFlowChild && hasLineIfEmpty()) {
        auto& fontMetrics = firstLineStyle().metricsOfPrimaryFont();
        return LayoutUnit { LayoutUnit(fontMetrics.ascent()
            + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
            + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight())).toInt() };
    }

    return std::optional<LayoutUnit>();
}

static inline bool isRenderBlockFlowOrRenderButton(RenderElement& renderElement)
{
    // We include isRenderButton in this check because buttons are implemented
    // using flex box but should still support first-line|first-letter.
    // The flex box and specs require that flex box and grid do not support
    // first-line|first-letter, though.
    // FIXME: Remove when buttons are implemented with align-items instead of
    // flex box.
    return renderElement.isRenderBlockFlow() || renderElement.isRenderButton();
}

static inline RenderBlock* findFirstLetterBlock(RenderBlock* start)
{
    RenderBlock* firstLetterBlock = start;
    while (true) {
        bool canHaveFirstLetterRenderer = firstLetterBlock->style().hasPseudoStyle(PseudoId::FirstLetter)
            && firstLetterBlock->canHaveGeneratedChildren()
            && isRenderBlockFlowOrRenderButton(*firstLetterBlock);
        if (canHaveFirstLetterRenderer)
            return firstLetterBlock;

        RenderElement* parentBlock = firstLetterBlock->parent();
        if (firstLetterBlock->isReplacedOrInlineBlock() || !parentBlock || parentBlock->firstChild() != firstLetterBlock
            || !isRenderBlockFlowOrRenderButton(*parentBlock))
            return nullptr;
        firstLetterBlock = downcast<RenderBlock>(parentBlock);
    } 

    return nullptr;
}

void RenderBlock::getFirstLetter(RenderObject*& firstLetter, RenderElement*& firstLetterContainer, RenderObject* skipObject)
{
    firstLetter = nullptr;
    firstLetterContainer = nullptr;

    // Don't recur
    if (style().styleType() == PseudoId::FirstLetter)
        return;
    
    // FIXME: We need to destroy the first-letter object if it is no longer the first child. Need to find
    // an efficient way to check for that situation though before implementing anything.
    firstLetterContainer = findFirstLetterBlock(this);
    if (!firstLetterContainer)
        return;
    
    // Drill into inlines looking for our first text descendant.
    firstLetter = firstLetterContainer->firstChild();
    while (firstLetter) {
        if (is<RenderText>(*firstLetter)) {
            if (firstLetter == skipObject) {
                firstLetter = firstLetter->nextSibling();
                continue;
            }
            
            break;
        }

        RenderElement& current = downcast<RenderElement>(*firstLetter);
        if (is<RenderListMarker>(current))
            firstLetter = current.nextSibling();
        else if (current.isFloatingOrOutOfFlowPositioned()) {
            if (current.style().styleType() == PseudoId::FirstLetter) {
                firstLetter = current.firstChild();
                break;
            }
            firstLetter = current.nextSibling();
        } else if (current.isReplacedOrInlineBlock() || is<RenderButton>(current) || is<RenderMenuList>(current))
            break;
        else if (current.isFlexibleBoxIncludingDeprecated() || current.isRenderGrid())
            firstLetter = current.nextSibling();
        else if (current.style().hasPseudoStyle(PseudoId::FirstLetter) && current.canHaveGeneratedChildren())  {
            // We found a lower-level node with first-letter, which supersedes the higher-level style
            firstLetterContainer = &current;
            firstLetter = current.firstChild();
        } else
            firstLetter = current.firstChild();
    }
    
    if (!firstLetter)
        firstLetterContainer = nullptr;
}

RenderFragmentedFlow* RenderBlock::cachedEnclosingFragmentedFlow() const
{
    RenderBlockRareData* rareData = getBlockRareData(*this);

    if (!rareData || !rareData->m_enclosingFragmentedFlow)
        return nullptr;

    return rareData->m_enclosingFragmentedFlow.value().get();
}

bool RenderBlock::cachedEnclosingFragmentedFlowNeedsUpdate() const
{
    RenderBlockRareData* rareData = getBlockRareData(*this);

    if (!rareData || !rareData->m_enclosingFragmentedFlow)
        return true;

    return false;
}

void RenderBlock::setCachedEnclosingFragmentedFlowNeedsUpdate()
{
    RenderBlockRareData& rareData = ensureBlockRareData(*this);
    rareData.m_enclosingFragmentedFlow = std::nullopt;
}

RenderFragmentedFlow* RenderBlock::updateCachedEnclosingFragmentedFlow(RenderFragmentedFlow* fragmentedFlow) const
{
    RenderBlockRareData& rareData = ensureBlockRareData(*this);
    rareData.m_enclosingFragmentedFlow = fragmentedFlow;

    return fragmentedFlow;
}

RenderFragmentedFlow* RenderBlock::locateEnclosingFragmentedFlow() const
{
    RenderBlockRareData* rareData = getBlockRareData(*this);
    if (!rareData || !rareData->m_enclosingFragmentedFlow)
        return updateCachedEnclosingFragmentedFlow(RenderBox::locateEnclosingFragmentedFlow());

    ASSERT(rareData->m_enclosingFragmentedFlow.value() == RenderBox::locateEnclosingFragmentedFlow());
    return rareData->m_enclosingFragmentedFlow.value().get();
}

void RenderBlock::resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(RenderFragmentedFlow* fragmentedFlow)
{
    if (fragmentedFlowState() == NotInsideFragmentedFlow)
        return;

    if (auto* cachedFragmentedFlow = cachedEnclosingFragmentedFlow())
        fragmentedFlow = cachedFragmentedFlow;
    setCachedEnclosingFragmentedFlowNeedsUpdate();
    RenderElement::resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(fragmentedFlow);
}

LayoutUnit RenderBlock::paginationStrut() const
{
    RenderBlockRareData* rareData = getBlockRareData(*this);
    return rareData ? rareData->m_paginationStrut : 0_lu;
}

LayoutUnit RenderBlock::pageLogicalOffset() const
{
    RenderBlockRareData* rareData = getBlockRareData(*this);
    return rareData ? rareData->m_pageLogicalOffset : 0_lu;
}

void RenderBlock::setPaginationStrut(LayoutUnit strut)
{
    RenderBlockRareData* rareData = getBlockRareData(*this);
    if (!rareData) {
        if (!strut)
            return;
        rareData = &ensureBlockRareData(*this);
    }
    rareData->m_paginationStrut = strut;
}

void RenderBlock::setPageLogicalOffset(LayoutUnit logicalOffset)
{
    RenderBlockRareData* rareData = getBlockRareData(*this);
    if (!rareData) {
        if (!logicalOffset)
            return;
        rareData = &ensureBlockRareData(*this);
    }
    rareData->m_pageLogicalOffset = logicalOffset;
}

void RenderBlock::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    if (auto* continuation = this->continuation()) {
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        rects.append(snappedIntRect(accumulatedOffset.x(), accumulatedOffset.y() - collapsedMarginBefore(), width(), height() + collapsedMarginBefore() + collapsedMarginAfter()));
        continuation->absoluteRects(rects, accumulatedOffset - toLayoutSize(location() + inlineContinuation()->containingBlock()->location()));
    } else
        rects.append(snappedIntRect(accumulatedOffset, size()));
}

void RenderBlock::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (!continuation()) {
        absoluteQuadsIgnoringContinuation({ { }, size() }, quads, wasFixed);
        return;
    }
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    auto logicalRect = FloatRect { 0, -collapsedMarginBefore(), width(), height() + collapsedMarginBefore() + collapsedMarginAfter() };
    absoluteQuadsIgnoringContinuation(logicalRect, quads, wasFixed);
    collectAbsoluteQuadsForContinuation(quads, wasFixed);
}

void RenderBlock::absoluteQuadsIgnoringContinuation(const FloatRect& logicalRect, Vector<FloatQuad>& quads, bool* wasFixed) const
{
    // FIXME: This is wrong for block-flows that are horizontal.
    // https://bugs.webkit.org/show_bug.cgi?id=46781
    auto* fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow || !fragmentedFlow->absoluteQuadsForBox(quads, wasFixed, this))
        quads.append(localToAbsoluteQuad(logicalRect, UseTransforms, wasFixed));
}

LayoutRect RenderBlock::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBox::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    if (isContinuation())
        r.inflateY(collapsedMarginBefore()); // FIXME: This is wrong for block-flows that are horizontal.
    return r;
}

const RenderStyle& RenderBlock::outlineStyleForRepaint() const
{
    if (auto* continuation = this->continuation())
        return continuation->style();
    return RenderElement::outlineStyleForRepaint();
}

void RenderBlock::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    if (Node* n = nodeForHitTest()) {
        result.setInnerNode(n);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(n);
        result.setLocalPoint(point);
    }
}

void RenderBlock::addFocusRingRectsForInlineChildren(Vector<LayoutRect>&, const LayoutPoint&, const RenderLayerModelObject*) const
{
    ASSERT_NOT_REACHED();
}

void RenderBlock::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer) const
{
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    auto* inlineContinuation = this->inlineContinuation();
    if (inlineContinuation) {
        // FIXME: This check really isn't accurate. 
        bool nextInlineHasLineBox = inlineContinuation->firstLineBox();
        // FIXME: This is wrong. The principal renderer may not be the continuation preceding this block.
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = downcast<RenderInline>(*inlineContinuation->element()->renderer()).firstLineBox();
        auto topMargin = prevInlineHasLineBox ? collapsedMarginBefore() : 0_lu;
        auto bottomMargin = nextInlineHasLineBox ? collapsedMarginAfter() : 0_lu;
        LayoutRect rect(additionalOffset.x(), additionalOffset.y() - topMargin, width(), height() + topMargin + bottomMargin);
        if (!rect.isEmpty())
            rects.append(rect);
    } else if (width() && height())
        rects.append(LayoutRect(additionalOffset, size()));

    if (!hasNonVisibleOverflow() && !hasControlClip()) {
        if (childrenInline())
            addFocusRingRectsForInlineChildren(rects, additionalOffset, paintContainer);
    
        for (auto& box : childrenOfType<RenderBox>(*this)) {
            if (is<RenderListMarker>(box))
                continue;

            FloatPoint pos;
            // FIXME: This doesn't work correctly with transforms.
            if (box.layer())
                pos = box.localToContainerPoint(FloatPoint(), paintContainer);
            else
                pos = FloatPoint(additionalOffset.x() + box.x(), additionalOffset.y() + box.y());
            box.addFocusRingRects(rects, flooredLayoutPoint(pos), paintContainer);
        }
    }

    if (inlineContinuation)
        inlineContinuation->addFocusRingRects(rects, flooredLayoutPoint(LayoutPoint(additionalOffset + inlineContinuation->containingBlock()->location() - location())), paintContainer);
}

RenderPtr<RenderBlock> RenderBlock::createAnonymousBlockWithStyleAndDisplay(Document& document, const RenderStyle& style, DisplayType display)
{
    // FIXME: Do we need to convert all our inline displays to block-type in the anonymous logic ?
    RenderPtr<RenderBlock> newBox;
    if (display == DisplayType::Flex || display == DisplayType::InlineFlex)
        newBox = createRenderer<RenderFlexibleBox>(document, RenderStyle::createAnonymousStyleWithDisplay(style, DisplayType::Flex));
    else
        newBox = createRenderer<RenderBlockFlow>(document, RenderStyle::createAnonymousStyleWithDisplay(style, DisplayType::Block));
    
    newBox->initializeStyle();
    return newBox;
}

LayoutUnit RenderBlock::offsetFromLogicalTopOfFirstPage() const
{
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (layoutState && !layoutState->isPaginated())
        return 0;

    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow)
        return fragmentedFlow->offsetFromLogicalTopOfFirstFragment(this);

    if (layoutState) {
        ASSERT(layoutState->renderer() == this);

        LayoutSize offsetDelta = layoutState->layoutOffset() - layoutState->pageOffset();
        return isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }
    
    ASSERT_NOT_REACHED();
    return 0;
}

RenderFragmentContainer* RenderBlock::fragmentAtBlockOffset(LayoutUnit blockOffset) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow || !fragmentedFlow->hasValidFragmentInfo())
        return 0;

    return fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstPage() + blockOffset, true);
}

static bool canComputeFragmentRangeForBox(const RenderBlock& parentBlock, const RenderBox& childBox, const RenderFragmentedFlow* enclosingFragmentedFlow)
{
    if (!enclosingFragmentedFlow)
        return false;

    if (!enclosingFragmentedFlow->hasFragments())
        return false;

    if (!childBox.canHaveOutsideFragmentRange())
        return false;

    return enclosingFragmentedFlow->hasCachedFragmentRangeForBox(parentBlock);
}

bool RenderBlock::childBoxIsUnsplittableForFragmentation(const RenderBox& child) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    bool checkColumnBreaks = fragmentedFlow && fragmentedFlow->shouldCheckColumnBreaks();
    bool checkPageBreaks = !checkColumnBreaks && view().frameView().layoutContext().layoutState()->pageLogicalHeight();
    return child.isUnsplittableForPagination() || child.style().breakInside() == BreakInside::Avoid
        || (checkColumnBreaks && child.style().breakInside() == BreakInside::AvoidColumn)
        || (checkPageBreaks && child.style().breakInside() == BreakInside::AvoidPage);
}

void RenderBlock::computeFragmentRangeForBoxChild(const RenderBox& box) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    ASSERT(canComputeFragmentRangeForBox(*this, box, fragmentedFlow));

    RenderFragmentContainer* startFragment;
    RenderFragmentContainer* endFragment;
    LayoutUnit offsetFromLogicalTopOfFirstFragment = box.offsetFromLogicalTopOfFirstPage();
    if (childBoxIsUnsplittableForFragmentation(box))
        startFragment = endFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment, true);
    else {
        startFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment, true);
        endFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment + logicalHeightForChild(box), true);
    }

    fragmentedFlow->setFragmentRangeForBox(box, startFragment, endFragment);
}

void RenderBlock::estimateFragmentRangeForBoxChild(const RenderBox& box) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!canComputeFragmentRangeForBox(*this, box, fragmentedFlow))
        return;

    if (childBoxIsUnsplittableForFragmentation(box)) {
        computeFragmentRangeForBoxChild(box);
        return;
    }

    auto estimatedValues = box.computeLogicalHeight(RenderFragmentedFlow::maxLogicalHeight(), logicalTopForChild(box));
    LayoutUnit offsetFromLogicalTopOfFirstFragment = box.offsetFromLogicalTopOfFirstPage();
    RenderFragmentContainer* startFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment, true);
    RenderFragmentContainer* endFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment + estimatedValues.m_extent, true);

    fragmentedFlow->setFragmentRangeForBox(box, startFragment, endFragment);
}

bool RenderBlock::updateFragmentRangeForBoxChild(const RenderBox& box) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!canComputeFragmentRangeForBox(*this, box, fragmentedFlow))
        return false;

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    fragmentedFlow->getFragmentRangeForBox(&box, startFragment, endFragment);

    computeFragmentRangeForBoxChild(box);

    RenderFragmentContainer* newStartFragment = nullptr;
    RenderFragmentContainer* newEndFragment = nullptr;
    fragmentedFlow->getFragmentRangeForBox(&box, newStartFragment, newEndFragment);


    // Changing the start fragment means we shift everything and a relayout is needed.
    if (newStartFragment != startFragment)
        return true;

    // The fragment range of the box has changed. Some boxes (e.g floats) may have been positioned assuming
    // a different range.
    if (box.needsLayoutAfterFragmentRangeChange() && newEndFragment != endFragment)
        return true;

    return false;
}

LayoutUnit RenderBlock::collapsedMarginBeforeForChild(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginBefore();
    
    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginAfter();
    
    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" sides of the child box.  We can just return the raw margin in this case.  
    return marginBeforeForChild(child);
}

LayoutUnit RenderBlock::collapsedMarginAfterForChild(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginAfter();
    
    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginBefore();
    
    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" side of the child box.  We can just return the raw margin in this case.  
    return marginAfterForChild(child);
}

bool RenderBlock::hasMarginBeforeQuirk(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child.isWritingModeRoot())
        return is<RenderBlock>(child) ? downcast<RenderBlock>(child).hasMarginBeforeQuirk() : child.style().hasMarginBeforeQuirk();
    
    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return is<RenderBlock>(child) ? downcast<RenderBlock>(child).hasMarginAfterQuirk() : child.style().hasMarginAfterQuirk();
    
    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

bool RenderBlock::hasMarginAfterQuirk(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child.isWritingModeRoot())
        return is<RenderBlock>(child) ? downcast<RenderBlock>(child).hasMarginAfterQuirk() : child.style().hasMarginAfterQuirk();
    
    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return is<RenderBlock>(child) ? downcast<RenderBlock>(child).hasMarginBeforeQuirk() : child.style().hasMarginBeforeQuirk();
    
    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

ASCIILiteral RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"_s; // FIXME: Temporary hack until we know that the regression tests pass.
    if (isFieldset())
        return "RenderFieldSet"_s; // FIXME: Remove eventually, but done to keep tests from breaking.
    if (isFloating())
        return "RenderBlock (floating)"_s;
    if (isOutOfFlowPositioned())
        return "RenderBlock (positioned)"_s;
    if (isAnonymousBlock())
        return "RenderBlock (anonymous)"_s;
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderBlock (generated)"_s;
    if (isAnonymous())
        return "RenderBlock (generated)"_s;
    if (isRelativelyPositioned())
        return "RenderBlock (relative positioned)"_s;
    if (isStickilyPositioned())
        return "RenderBlock (sticky positioned)"_s;
    return "RenderBlock"_s;
}

TextRun RenderBlock::constructTextRun(StringView stringView, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    auto textDirection = TextDirection::LTR;
    bool directionalOverride = style.rtlOrdering() == Order::Visual;
    if (flags != DefaultTextRunFlags) {
        if (flags & RespectDirection)
            textDirection = style.direction();
        if (flags & RespectDirectionOverride)
            directionalOverride |= isOverride(style.unicodeBidi());
    }

    // This works because:
    // 1. TextRun owns its text string. Its member is a String, not a StringView
    // 2. This replacement doesn't affect string indices. We're replacing a single Unicode code unit with another Unicode code unit.
    // How convenient.
    auto updatedString = RenderBlock::updateSecurityDiscCharacters(style, stringView.toStringWithoutCopying());
    return TextRun(WTFMove(updatedString), 0, 0, expansion, textDirection, directionalOverride);
}

TextRun RenderBlock::constructTextRun(const String& string, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    return constructTextRun(StringView(string), style, expansion, flags);
}

TextRun RenderBlock::constructTextRun(const AtomString& atomString, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    return constructTextRun(StringView(atomString), style, expansion, flags);
}

TextRun RenderBlock::constructTextRun(const RenderText& text, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(text.stringView(), style, expansion);
}

TextRun RenderBlock::constructTextRun(const RenderText& text, unsigned offset, unsigned length, const RenderStyle& style, ExpansionBehavior expansion)
{
    unsigned stop = offset + length;
    ASSERT(stop <= text.text().length());
    return constructTextRun(text.stringView(offset, stop), style, expansion);
}

TextRun RenderBlock::constructTextRun(const LChar* characters, unsigned length, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(StringView(characters, length), style, expansion);
}

TextRun RenderBlock::constructTextRun(const UChar* characters, unsigned length, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(StringView(characters, length), style, expansion);
}

#if ASSERT_ENABLED
void RenderBlock::checkPositionedObjectsNeedLayout()
{
    auto* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;

    for (auto* renderer : *positionedDescendants)
        ASSERT(!renderer->needsLayout());
}
#endif // ASSERT_ENABLED

bool RenderBlock::hasDefiniteLogicalHeight() const
{
    return (bool)availableLogicalHeightForPercentageComputation();
}

std::optional<LayoutUnit> RenderBlock::availableLogicalHeightForPercentageComputation() const
{
    std::optional<LayoutUnit> availableHeight;
    
    // For anonymous blocks that are skipped during percentage height calculation,
    // we consider them to have an indefinite height.
    if (skipContainingBlockForPercentHeightCalculation(*this, false))
        return availableHeight;
    
    const auto& styleToUse = style();
    
    // A positioned element that specified both top/bottom or that specifies
    // height should be treated as though it has a height explicitly specified
    // that can be used for any percentage computations.
    bool isOutOfFlowPositionedWithSpecifiedHeight = isOutOfFlowPositioned() && (!styleToUse.logicalHeight().isAuto() || (!styleToUse.logicalTop().isAuto() && !styleToUse.logicalBottom().isAuto()));
    
    if (isFlexItem() && downcast<RenderFlexibleBox>(parent())->useChildOverridingLogicalHeightForPercentageResolution(*this))
        availableHeight = overridingContentLogicalHeight();
    else if (isGridItem() && hasOverridingLogicalHeight())
        availableHeight = overridingContentLogicalHeight();
    else if (styleToUse.logicalHeight().isFixed()) {
        LayoutUnit contentBoxHeight = adjustContentBoxLogicalHeightForBoxSizing((LayoutUnit)styleToUse.logicalHeight().value());
        availableHeight = std::max(0_lu, constrainContentBoxLogicalHeightByMinMax(contentBoxHeight - scrollbarLogicalHeight(), std::nullopt));
    } else if (shouldComputeLogicalHeightFromAspectRatio()) {
        // Only grid is expected to be in a state where it is calculating pref width and having unknown logical width.
        if (isRenderGrid() && preferredLogicalWidthsDirty() && !style().logicalWidth().isSpecified())
            return availableHeight;
        availableHeight = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), LayoutUnit(style().logicalAspectRatio()), style().boxSizingForAspectRatio(), logicalWidth());
    } else if (isOutOfFlowPositionedWithSpecifiedHeight) {
        // Don't allow this to affect the block' size() member variable, since this
        // can get called while the block is still laying out its kids.
        LogicalExtentComputedValues computedValues = computeLogicalHeight(logicalHeight(), 0_lu);
        availableHeight = computedValues.m_extent - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight();
    } else if (styleToUse.logicalHeight().isPercentOrCalculated()) {
        std::optional<LayoutUnit> heightWithScrollbar = computePercentageLogicalHeight(styleToUse.logicalHeight());
        if (heightWithScrollbar) {
            LayoutUnit contentBoxHeightWithScrollbar = adjustContentBoxLogicalHeightForBoxSizing(heightWithScrollbar.value());
            // We need to adjust for min/max height because this method does not
            // handle the min/max of the current block, its caller does. So the
            // return value from the recursive call will not have been adjusted
            // yet.
            LayoutUnit contentBoxHeight = constrainContentBoxLogicalHeightByMinMax(contentBoxHeightWithScrollbar - scrollbarLogicalHeight(), std::nullopt);
            availableHeight = std::max(0_lu, contentBoxHeight);
        }
    } else if (isRenderView())
        availableHeight = view().pageOrViewLogicalHeight();
    
    return availableHeight;
}
    
void RenderBlock::layoutExcludedChildren(bool relayoutChildren)
{
    if (!isFieldset())
        return;

    setIntrinsicBorderForFieldset(0);

    RenderBox* box = findFieldsetLegend();
    if (!box)
        return;

    box->setIsExcludedFromNormalLayout(true);
    for (auto& child : childrenOfType<RenderBox>(*this)) {
        if (&child == box || !child.isLegend())
            continue;
        child.setIsExcludedFromNormalLayout(false);
    }

    RenderBox& legend = *box;
    if (relayoutChildren)
        legend.setChildNeedsLayout(MarkOnlyThis);
    legend.layoutIfNeeded();
    
    LayoutUnit logicalLeft;
    if (style().isLeftToRightDirection()) {
        switch (legend.style().textAlign()) {
        case TextAlignMode::Center:
            logicalLeft = (logicalWidth() - logicalWidthForChild(legend)) / 2;
            break;
        case TextAlignMode::Right:
            logicalLeft = logicalWidth() - borderEnd() - paddingEnd() - logicalWidthForChild(legend);
            break;
        default:
            logicalLeft = borderStart() + paddingStart() + marginStartForChild(legend);
            break;
        }
    } else {
        switch (legend.style().textAlign()) {
        case TextAlignMode::Left:
            logicalLeft = borderStart() + paddingStart();
            break;
        case TextAlignMode::Center: {
            // Make sure that the extra pixel goes to the end side in RTL (since it went to the end side
            // in LTR).
            LayoutUnit centeredWidth = logicalWidth() - logicalWidthForChild(legend);
            logicalLeft = centeredWidth - centeredWidth / 2;
            break;
        }
        default:
            logicalLeft = logicalWidth() - borderStart() - paddingStart() - marginStartForChild(legend) - logicalWidthForChild(legend);
            break;
        }
    }
    
    setLogicalLeftForChild(legend, logicalLeft);
    
    LayoutUnit fieldsetBorderBefore = borderBefore();
    LayoutUnit legendLogicalHeight = logicalHeightForChild(legend);
    LayoutUnit legendAfterMargin = marginAfterForChild(legend);
    LayoutUnit topPositionForLegend = std::max(0_lu, (fieldsetBorderBefore - legendLogicalHeight) / 2);
    LayoutUnit bottomPositionForLegend = topPositionForLegend + legendLogicalHeight + legendAfterMargin;

    // Place the legend now.
    setLogicalTopForChild(legend, topPositionForLegend);

    // If the bottom of the legend (including its after margin) is below the fieldset border,
    // then we need to add in sufficient intrinsic border to account for this gap.
    // FIXME: Should we support the before margin of the legend? Not entirely clear.
    // FIXME: Consider dropping support for the after margin of the legend. Not sure other
    // browsers support that anyway.
    if (bottomPositionForLegend > fieldsetBorderBefore)
        setIntrinsicBorderForFieldset(bottomPositionForLegend - fieldsetBorderBefore);
    
    // Now that the legend is included in the border extent, we can set our logical height
    // to the borderBefore (which includes the legend and its after margin if they were bigger
    // than the actual fieldset border) and then add in our padding before.
    setLogicalHeight(borderBefore() + paddingBefore());
}

RenderBox* RenderBlock::findFieldsetLegend(FieldsetFindLegendOption option) const
{
    for (auto& legend : childrenOfType<RenderBox>(*this)) {
        if (option == FieldsetIgnoreFloatingOrOutOfFlow && legend.isFloatingOrOutOfFlowPositioned())
            continue;
        if (legend.isLegend())
            return const_cast<RenderBox*>(&legend);
    }
    return nullptr;
}

void RenderBlock::adjustBorderBoxRectForPainting(LayoutRect& paintRect)
{
    if (!isFieldset() || !intrinsicBorderForFieldset())
        return;
    
    auto* legend = findFieldsetLegend();
    if (!legend)
        return;

    if (style().isHorizontalWritingMode()) {
        LayoutUnit yOff = std::max(0_lu, (legend->height() - RenderBox::borderBefore()) / 2);
        paintRect.setHeight(paintRect.height() - yOff);
        if (style().writingMode() == WritingMode::TopToBottom)
            paintRect.setY(paintRect.y() + yOff);
    } else {
        LayoutUnit xOff = std::max(0_lu, (legend->width() - RenderBox::borderBefore()) / 2);
        paintRect.setWidth(paintRect.width() - xOff);
        if (style().writingMode() == WritingMode::LeftToRight)
            paintRect.setX(paintRect.x() + xOff);
    }
}

LayoutRect RenderBlock::paintRectToClipOutFromBorder(const LayoutRect& paintRect)
{
    LayoutRect clipRect;
    if (!isFieldset())
        return clipRect;
    auto* legend = findFieldsetLegend();
    if (!legend)
        return clipRect;

    LayoutUnit borderExtent = RenderBox::borderBefore();
    if (style().isHorizontalWritingMode()) {
        clipRect.setX(paintRect.x() + legend->x());
        clipRect.setY(style().writingMode() == WritingMode::TopToBottom ? paintRect.y() : paintRect.y() + paintRect.height() - borderExtent);
        clipRect.setWidth(legend->width());
        clipRect.setHeight(borderExtent);
    } else {
        clipRect.setX(style().writingMode() == WritingMode::LeftToRight ? paintRect.x() : paintRect.x() + paintRect.width() - borderExtent);
        clipRect.setY(paintRect.y() + legend->y());
        clipRect.setWidth(borderExtent);
        clipRect.setHeight(legend->height());
    }
    return clipRect;
}

LayoutUnit RenderBlock::intrinsicBorderForFieldset() const
{
    auto* rareData = getBlockRareData(*this);
    return rareData ? rareData->m_intrinsicBorderForFieldset : 0_lu;
}

void RenderBlock::setIntrinsicBorderForFieldset(LayoutUnit padding)
{
    auto* rareData = getBlockRareData(*this);
    if (!rareData) {
        if (!padding)
            return;
        rareData = &ensureBlockRareData(*this);
    }
    rareData->m_intrinsicBorderForFieldset = padding;
}

LayoutUnit RenderBlock::borderTop() const
{
    if (style().writingMode() != WritingMode::TopToBottom || !intrinsicBorderForFieldset())
        return RenderBox::borderTop();
    return RenderBox::borderTop() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderLeft() const
{
    if (style().writingMode() != WritingMode::LeftToRight || !intrinsicBorderForFieldset())
        return RenderBox::borderLeft();
    return RenderBox::borderLeft() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderBottom() const
{
    if (style().writingMode() != WritingMode::BottomToTop || !intrinsicBorderForFieldset())
        return RenderBox::borderBottom();
    return RenderBox::borderBottom() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderRight() const
{
    if (style().writingMode() != WritingMode::RightToLeft || !intrinsicBorderForFieldset())
        return RenderBox::borderRight();
    return RenderBox::borderRight() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderBefore() const
{
    return RenderBox::borderBefore() + intrinsicBorderForFieldset();
}

bool RenderBlock::computePreferredWidthsForExcludedChildren(LayoutUnit& minWidth, LayoutUnit& maxWidth) const
{
    if (!isFieldset())
        return false;
    
    auto* legend = findFieldsetLegend();
    if (!legend)
        return false;
    
    legend->setIsExcludedFromNormalLayout(true);

    computeChildPreferredLogicalWidths(*legend, minWidth, maxWidth);
    
    // These are going to be added in later, so we subtract them out to reflect the
    // fact that the legend is outside the scrollable area.
    auto scrollbarWidth = intrinsicScrollbarLogicalWidth();
    minWidth -= scrollbarWidth;
    maxWidth -= scrollbarWidth;
    
    const auto& childStyle = legend->style();
    auto startMarginLength = childStyle.marginStartUsing(&style());
    auto endMarginLength = childStyle.marginEndUsing(&style());
    LayoutUnit margin;
    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (startMarginLength.isFixed())
        marginStart += startMarginLength.value();
    if (endMarginLength.isFixed())
        marginEnd += endMarginLength.value();
    margin = marginStart + marginEnd;
    
    minWidth += margin;
    maxWidth += margin;

    return true;
}

LayoutUnit RenderBlock::adjustBorderBoxLogicalHeightForBoxSizing(LayoutUnit height) const
{
    // FIXME: We're doing this to match other browsers even though it's questionable.
    // Shouldn't height:100px mean the fieldset content gets 100px of height even if the
    // resulting fieldset becomes much taller because of the legend?
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    if (style().boxSizing() == BoxSizing::ContentBox)
        return height + bordersPlusPadding - intrinsicBorderForFieldset();
    return std::max(height, bordersPlusPadding);
}

LayoutUnit RenderBlock::adjustContentBoxLogicalHeightForBoxSizing(std::optional<LayoutUnit> height) const
{
    // FIXME: We're doing this to match other browsers even though it's questionable.
    // Shouldn't height:100px mean the fieldset content gets 100px of height even if the
    // resulting fieldset becomes much taller because of the legend?
    if (!height)
        return 0;
    LayoutUnit result = height.value();
    if (style().boxSizing() == BoxSizing::BorderBox)
        result -= borderAndPaddingLogicalHeight();
    else
        result -= intrinsicBorderForFieldset();
    return std::max(0_lu, result);
}

LayoutUnit RenderBlock::adjustIntrinsicLogicalHeightForBoxSizing(LayoutUnit height) const
{
    if (style().boxSizing() == BoxSizing::BorderBox)
        return height + borderAndPaddingLogicalHeight();
    return height + intrinsicBorderForFieldset();
}

void RenderBlock::paintExcludedChildrenInBorder(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!isFieldset())
        return;
    
    RenderBox* box = findFieldsetLegend();
    if (!box || !box->isExcludedFromNormalLayout() || box->hasSelfPaintingLayer())
        return;
    
    LayoutPoint childPoint = flipForWritingModeForChild(*box, paintOffset);
    box->paintAsInlineBlock(paintInfo, childPoint);
}

bool RenderBlock::hitTestExcludedChildrenInBorder(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!isFieldset())
        return false;

    auto* legend = findFieldsetLegend();
    if (!legend || !legend->isExcludedFromNormalLayout() || legend->hasSelfPaintingLayer())
        return false;

    HitTestAction childHitTest = hitTestAction;
    if (hitTestAction == HitTestChildBlockBackgrounds)
        childHitTest = HitTestChildBlockBackground;
    LayoutPoint childPoint = flipForWritingModeForChild(*legend, accumulatedOffset);
    return legend->nodeAtPoint(request, result, locationInContainer, childPoint, childHitTest);
}

String RenderBlock::updateSecurityDiscCharacters(const RenderStyle& style, String&& string)
{
#if !PLATFORM(COCOA)
    UNUSED_PARAM(style);
    return WTFMove(string);
#else
    if (style.textSecurity() == TextSecurity::None)
        return WTFMove(string);
    // This PUA character in the system font is used to render password field dots on Cocoa platforms.
    constexpr UChar textSecurityDiscPUACodePoint = 0xF79A;
    auto& font = style.fontCascade().primaryFont();
    if (!(font.platformData().isSystemFont() && font.glyphForCharacter(textSecurityDiscPUACodePoint)))
        return WTFMove(string);

    // See RenderText::setRenderedText()
#if PLATFORM(IOS_FAMILY)
    constexpr UChar discCharacterToReplace = blackCircle;
#else
    constexpr UChar discCharacterToReplace = bullet;
#endif

    return makeStringByReplacingAll(string, discCharacterToReplace, textSecurityDiscPUACodePoint);
#endif
}

LayoutUnit RenderBlock::layoutOverflowLogicalBottom(const RenderBlock& renderer)
{
    ASSERT(is<RenderGrid>(renderer) || is<RenderFlexibleBox>(renderer));
    auto maxChildLogicalBottom = LayoutUnit { };
    for (auto& child : childrenOfType<RenderBox>(renderer)) {
        if (child.isOutOfFlowPositioned())
            continue;
        auto childLogicalBottom = renderer.logicalTopForChild(child) + renderer.logicalHeightForChild(child) + renderer.marginAfterForChild(child);
        maxChildLogicalBottom = std::max(maxChildLogicalBottom, childLogicalBottom);
    }
    return std::max(renderer.clientLogicalBottom(), maxChildLogicalBottom + renderer.paddingAfter());
}

} // namespace WebCore
