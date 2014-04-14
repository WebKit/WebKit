/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include "ColumnInfo.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestLocation.h"
#include "HitTestResult.h"
#include "InlineElementBox.h"
#include "InlineIterator.h"
#include "InlineTextBox.h"
#include "LayoutRepainter.h"
#include "LogicalSelectionOffsetCaches.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderBoxRegionInfo.h"
#include "RenderCombineText.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderMarquee.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h"
#include "RenderRegion.h"
#include "RenderTableCell.h"
#include "RenderTextFragment.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SVGTextRunRenderingContext.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TransformState.h"
#include <wtf/StackStats.h>
#include <wtf/TemporaryChange.h>

#if ENABLE(CSS_SHAPES)
#include "ShapeOutsideInfo.h"
#endif

using namespace WTF;
using namespace Unicode;

namespace WebCore {

using namespace HTMLNames;

struct SameSizeAsRenderBlock : public RenderBox {
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(RenderBlock) == sizeof(SameSizeAsRenderBlock), RenderBlock_should_stay_small);

typedef WTF::HashMap<const RenderBox*, std::unique_ptr<ColumnInfo>> ColumnInfoMap;
static ColumnInfoMap* gColumnInfoMap = 0;

static TrackedDescendantsMap* gPositionedDescendantsMap = 0;
static TrackedDescendantsMap* gPercentHeightDescendantsMap = 0;

static TrackedContainerMap* gPositionedContainerMap = 0;
static TrackedContainerMap* gPercentHeightContainerMap = 0;
    
typedef WTF::HashMap<RenderBlock*, std::unique_ptr<ListHashSet<RenderInline*>>> ContinuationOutlineTableMap;

typedef WTF::HashSet<RenderBlock*> DelayedUpdateScrollInfoSet;
static int gDelayUpdateScrollInfo = 0;
static DelayedUpdateScrollInfoSet* gDelayedUpdateScrollInfoSet = 0;

static bool gColumnFlowSplitEnabled = true;

// Allocated only when some of these fields have non-default values

struct RenderBlockRareData {
    WTF_MAKE_NONCOPYABLE(RenderBlockRareData); WTF_MAKE_FAST_ALLOCATED;
public:
    RenderBlockRareData() 
        : m_paginationStrut(0)
        , m_pageLogicalOffset(0)
    { 
    }

    LayoutUnit m_paginationStrut;
    LayoutUnit m_pageLogicalOffset;
};

typedef HashMap<const RenderBlock*, std::unique_ptr<RenderBlockRareData>> RenderBlockRareDataMap;
static RenderBlockRareDataMap* gRareDataMap = 0;

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
        m_shouldDispatchEvent = !m_block->isAnonymous() && m_block->hasOverflowClip() && m_block->document().hasListenerType(Document::OVERFLOWCHANGED_LISTENER);
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

        RefPtr<OverflowEvent> overflowEvent = OverflowEvent::create(horizontalLayoutOverflowChanged, hasHorizontalLayoutOverflow, verticalLayoutOverflowChanged, hasVerticalLayoutOverflow);
        overflowEvent->setTarget(m_block->element());
        m_block->document().enqueueOverflowEvent(overflowEvent.release());
    }

private:
    const RenderBlock* m_block;
    bool m_shouldDispatchEvent;
    bool m_hadHorizontalLayoutOverflow;
    bool m_hadVerticalLayoutOverflow;
};

RenderBlock::RenderBlock(Element& element, PassRef<RenderStyle> style, unsigned baseTypeFlags)
    : RenderBox(element, std::move(style), baseTypeFlags | RenderBlockFlag)
    , m_lineHeight(-1)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_beingDestroyed(false)
    , m_hasMarkupTruncation(false)
    , m_hasBorderOrPaddingLogicalWidthChanged(false)
    , m_lineLayoutPath(UndeterminedPath)
{
}

RenderBlock::RenderBlock(Document& document, PassRef<RenderStyle> style, unsigned baseTypeFlags)
    : RenderBox(document, std::move(style), baseTypeFlags | RenderBlockFlag)
    , m_lineHeight(-1)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_beingDestroyed(false)
    , m_hasMarkupTruncation(false)
    , m_hasBorderOrPaddingLogicalWidthChanged(false)
    , m_lineLayoutPath(UndeterminedPath)
{
}

static void removeBlockFromDescendantAndContainerMaps(RenderBlock* block, TrackedDescendantsMap*& descendantMap, TrackedContainerMap*& containerMap)
{
    if (std::unique_ptr<TrackedRendererListHashSet> descendantSet = descendantMap->take(block)) {
        TrackedRendererListHashSet::iterator end = descendantSet->end();
        for (TrackedRendererListHashSet::iterator descendant = descendantSet->begin(); descendant != end; ++descendant) {
            TrackedContainerMap::iterator it = containerMap->find(*descendant);
            ASSERT(it != containerMap->end());
            if (it == containerMap->end())
                continue;
            HashSet<RenderBlock*>* containerSet = it->value.get();
            ASSERT(containerSet->contains(block));
            containerSet->remove(block);
            if (containerSet->isEmpty())
                containerMap->remove(it);
        }
    }
}

RenderBlock::~RenderBlock()
{
    if (hasColumns())
        gColumnInfoMap->take(this);
    if (gRareDataMap)
        gRareDataMap->remove(this);
    if (gPercentHeightDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
    if (gPositionedDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPositionedDescendantsMap, gPositionedContainerMap);
}

bool RenderBlock::hasRareData() const
{
    return gRareDataMap ? gRareDataMap->contains(this) : false;
}

void RenderBlock::willBeDestroyed()
{
    // Mark as being destroyed to avoid trouble with merges in removeChild().
    m_beingDestroyed = true;

    // Make sure to destroy anonymous children first while they are still connected to the rest of the tree, so that they will
    // properly dirty line boxes that they are removed from. Effects that do :before/:after only on hover could crash otherwise.
    destroyLeftoverChildren();

    // Destroy our continuation before anything other than anonymous children.
    // The reason we don't destroy it before anonymous children is that they may
    // have continuations of their own that are anonymous children of our continuation.
    RenderBoxModelObject* continuation = this->continuation();
    if (continuation) {
        continuation->destroy();
        setContinuation(0);
    }
    
    if (!documentBeingDestroyed()) {
        if (parent())
            parent()->dirtyLinesFromChangedChild(this);
    }

    removeFromDelayedUpdateScrollInfoSet();

    RenderBox::willBeDestroyed();
}

void RenderBlock::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;

    setReplaced(newStyle.isDisplayInlineType());
    
    if (oldStyle && parent() && diff == StyleDifferenceLayout && oldStyle->position() != newStyle.position()) {
        if (newStyle.position() == StaticPosition)
            // Clear our positioned objects list. Our absolutely positioned descendants will be
            // inserted into our containing block's positioned objects list during layout.
            removePositionedObjects(0, NewContainingBlock);
        else if (oldStyle->position() == StaticPosition) {
            // Remove our absolutely positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            auto cb = parent();
            while (cb && (cb->style().position() == StaticPosition || (cb->isInline() && !cb->isReplaced())) && !cb->isRenderView()) {
                if (cb->style().position() == RelativePosition && cb->isInline() && !cb->isReplaced()) {
                    cb = cb->containingBlock();
                    break;
                }
                cb = cb->parent();
            }
            
            if (cb->isRenderBlock())
                toRenderBlock(cb)->removePositionedObjects(this, NewContainingBlock);
        }
    }

    RenderBox::styleWillChange(diff, newStyle);
}

static bool borderOrPaddingLogicalWidthChanged(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (newStyle->isHorizontalWritingMode())
        return oldStyle->borderLeftWidth() != newStyle->borderLeftWidth()
            || oldStyle->borderRightWidth() != newStyle->borderRightWidth()
            || oldStyle->paddingLeft() != newStyle->paddingLeft()
            || oldStyle->paddingRight() != newStyle->paddingRight();

    return oldStyle->borderTopWidth() != newStyle->borderTopWidth()
        || oldStyle->borderBottomWidth() != newStyle->borderBottomWidth()
        || oldStyle->paddingTop() != newStyle->paddingTop()
        || oldStyle->paddingBottom() != newStyle->paddingBottom();
}

void RenderBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    
    RenderStyle& newStyle = style();

    if (!isAnonymousBlock()) {
        // Ensure that all of our continuation blocks pick up the new style.
        for (RenderBlock* currCont = blockElementContinuation(); currCont; currCont = currCont->blockElementContinuation()) {
            RenderBoxModelObject* nextCont = currCont->continuation();
            currCont->setContinuation(0);
            currCont->setStyle(newStyle);
            currCont->setContinuation(nextCont);
        }
    }

    propagateStyleToAnonymousChildren(PropagateToBlockChildrenOnly);
    m_lineHeight = -1;
    
    // It's possible for our border/padding to change, but for the overall logical width of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    m_hasBorderOrPaddingLogicalWidthChanged = oldStyle && diff == StyleDifferenceLayout && needsLayout() && borderOrPaddingLogicalWidthChanged(oldStyle, &newStyle);
}

RenderBlock* RenderBlock::continuationBefore(RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;

    RenderBlock* curr = toRenderBlock(continuation());
    RenderBlock* nextToLast = this;
    RenderBlock* last = this;
    while (curr) {
        if (beforeChild && beforeChild->parent() == curr) {
            if (curr->firstChild() == beforeChild)
                return last;
            return curr;
        }

        nextToLast = last;
        last = curr;
        curr = toRenderBlock(curr->continuation());
    }

    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

void RenderBlock::addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderBlock* flow = continuationBefore(beforeChild);
    ASSERT(!beforeChild || beforeChild->parent()->isAnonymousColumnSpanBlock() || beforeChild->parent()->isRenderBlock());
    RenderBoxModelObject* beforeChildParent = 0;
    if (beforeChild)
        beforeChildParent = toRenderBoxModelObject(beforeChild->parent());
    else {
        RenderBoxModelObject* cont = flow->continuation();
        if (cont)
            beforeChildParent = cont;
        else
            beforeChildParent = flow;
    }

    if (newChild->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }

    // A continuation always consists of two potential candidates: a block or an anonymous
    // column span box holding column span children.
    bool childIsNormal = newChild->isInline() || !newChild->style().columnSpan();
    bool bcpIsNormal = beforeChildParent->isInline() || !beforeChildParent->style().columnSpan();
    bool flowIsNormal = flow->isInline() || !flow->style().columnSpan();

    if (flow == beforeChildParent) {
        flow->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }
    
    // The goal here is to match up if we can, so that we can coalesce and create the
    // minimal # of continuations needed for the inline.
    if (childIsNormal == bcpIsNormal) {
        beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }
    if (flowIsNormal == childIsNormal) {
        flow->addChildIgnoringContinuation(newChild, 0); // Just treat like an append.
        return;
    }
    beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
}


void RenderBlock::addChildToAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild)
{
    ASSERT(!continuation()); // We don't yet support column spans that aren't immediate children of the multi-column block.
        
    // The goal is to locate a suitable box in which to place our child.
    RenderBlock* beforeChildParent = 0;
    if (beforeChild) {
        RenderObject* curr = beforeChild;
        while (curr && curr->parent() != this)
            curr = curr->parent();
        beforeChildParent = toRenderBlock(curr);
        ASSERT(beforeChildParent);
        ASSERT(beforeChildParent->isAnonymousColumnsBlock() || beforeChildParent->isAnonymousColumnSpanBlock());
    } else
        beforeChildParent = toRenderBlock(lastChild());

    // If the new child is floating or positioned it can just go in that block.
    if (newChild->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
        return;
    }

    // See if the child can be placed in the box.
    bool newChildHasColumnSpan = !newChild->isInline() && newChild->style().columnSpan();
    bool beforeChildParentHoldsColumnSpans = beforeChildParent->isAnonymousColumnSpanBlock();

    if (newChildHasColumnSpan == beforeChildParentHoldsColumnSpans) {
        beforeChildParent->addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
        return;
    }

    if (!beforeChild) {
        // Create a new block of the correct type.
        RenderBlock* newBox = newChildHasColumnSpan ? createAnonymousColumnSpanBlock() : createAnonymousColumnsBlock();
        insertChildInternal(newBox, nullptr, NotifyChildren);
        newBox->addChildIgnoringAnonymousColumnBlocks(newChild, 0);
        return;
    }

    RenderObject* immediateChild = beforeChild;
    bool isPreviousBlockViable = true;
    while (immediateChild->parent() != this) {
        if (isPreviousBlockViable)
            isPreviousBlockViable = !immediateChild->previousSibling();
        immediateChild = immediateChild->parent();
    }
    if (isPreviousBlockViable && immediateChild->previousSibling()) {
        toRenderBlock(immediateChild->previousSibling())->addChildIgnoringAnonymousColumnBlocks(newChild, 0); // Treat like an append.
        return;
    }
        
    // Split our anonymous blocks.
    RenderObject* newBeforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    
    // Create a new anonymous box of the appropriate type.
    RenderBlock* newBox = newChildHasColumnSpan ? createAnonymousColumnSpanBlock() : createAnonymousColumnsBlock();
    insertChildInternal(newBox, newBeforeChild, NotifyChildren);
    newBox->addChildIgnoringAnonymousColumnBlocks(newChild, 0);
    return;
}

RenderBlock* RenderBlock::containingColumnsBlock(bool allowAnonymousColumnBlock)
{
    RenderBlock* firstChildIgnoringAnonymousWrappers = 0;
    for (RenderElement* curr = this; curr; curr = curr->parent()) {
        if (!curr->isRenderBlock() || curr->isFloatingOrOutOfFlowPositioned() || curr->isTableCell() || curr->isRoot() || curr->isRenderView() || curr->hasOverflowClip()
            || curr->isInlineBlockOrInlineTable())
            return 0;

        // FIXME: Tables, RenderButtons, and RenderListItems all do special management
        // of their children that breaks when the flow is split through them. Disabling
        // multi-column for them to avoid this problem.
        if (curr->isTable() || curr->isRenderButton() || curr->isListItem())
            return 0;
        
        RenderBlock* currBlock = toRenderBlock(curr);
        if (!currBlock->createsAnonymousWrapper())
            firstChildIgnoringAnonymousWrappers = currBlock;

        if (currBlock->style().specifiesColumns() && (allowAnonymousColumnBlock || !currBlock->isAnonymousColumnsBlock()))
            return firstChildIgnoringAnonymousWrappers;
            
        if (currBlock->isAnonymousColumnSpanBlock())
            return 0;
    }
    return 0;
}

RenderPtr<RenderBlock> RenderBlock::clone() const
{
    RenderPtr<RenderBlock> cloneBlock;
    if (isAnonymousBlock()) {
        cloneBlock = RenderPtr<RenderBlock>(createAnonymousBlock());
        cloneBlock->setChildrenInline(childrenInline());
    } else {
        cloneBlock = static_pointer_cast<RenderBlock>(element()->createElementRenderer(style()));
        cloneBlock->initializeStyle();

        // This takes care of setting the right value of childrenInline in case
        // generated content is added to cloneBlock and 'this' does not have
        // generated content added yet.
        cloneBlock->setChildrenInline(cloneBlock->firstChild() ? cloneBlock->firstChild()->isInline() : childrenInline());
    }
    cloneBlock->setFlowThreadState(flowThreadState());
    return cloneBlock;
}

void RenderBlock::splitBlocks(RenderBlock* fromBlock, RenderBlock* toBlock,
                              RenderBlock* middleBlock,
                              RenderObject* beforeChild, RenderBoxModelObject* oldCont)
{
    // Create a clone of this inline.
    RenderPtr<RenderBlock> cloneBlock = clone();
    if (!isAnonymousBlock())
        cloneBlock->setContinuation(oldCont);

    if (!beforeChild && isAfterContent(lastChild()))
        beforeChild = lastChild();

    // If we are moving inline children from |this| to cloneBlock, then we need
    // to clear our line box tree.
    if (beforeChild && childrenInline())
        deleteLines();

    // Now take all of the children from beforeChild to the end and remove
    // them from |this| and place them in the clone.
    moveChildrenTo(cloneBlock.get(), beforeChild, 0, true);
    
    // Hook |clone| up as the continuation of the middle block.
    if (!cloneBlock->isAnonymousBlock())
        middleBlock->setContinuation(cloneBlock.get());

    // We have been reparented and are now under the fromBlock.  We need
    // to walk up our block parent chain until we hit the containing anonymous columns block.
    // Once we hit the anonymous columns block we're done.
    RenderBoxModelObject* curr = toRenderBoxModelObject(parent());
    RenderBoxModelObject* currChild = this;
    RenderObject* currChildNextSibling = currChild->nextSibling();

    while (curr && curr->isDescendantOf(fromBlock) && curr != fromBlock) {
        RenderBlock* blockCurr = toRenderBlock(curr);
        
        // Create a new clone.
        RenderPtr<RenderBlock> cloneChild = std::move(cloneBlock);
        cloneBlock = blockCurr->clone();

        // Insert our child clone as the first child.
        cloneBlock->addChildIgnoringContinuation(cloneChild.leakPtr(), 0);

        // Hook the clone up as a continuation of |curr|.  Note we do encounter
        // anonymous blocks possibly as we walk up the block chain.  When we split an
        // anonymous block, there's no need to do any continuation hookup, since we haven't
        // actually split a real element.
        if (!blockCurr->isAnonymousBlock()) {
            oldCont = blockCurr->continuation();
            blockCurr->setContinuation(cloneBlock.get());
            cloneBlock->setContinuation(oldCont);
        }

        // Now we need to take all of the children starting from the first child
        // *after* currChild and append them all to the clone.
        blockCurr->moveChildrenTo(cloneBlock.get(), currChildNextSibling, 0, true);

        // Keep walking up the chain.
        currChild = curr;
        currChildNextSibling = currChild->nextSibling();
        curr = toRenderBoxModelObject(curr->parent());
    }

    // Now we are at the columns block level. We need to put the clone into the toBlock.
    toBlock->insertChildInternal(cloneBlock.leakPtr(), nullptr, NotifyChildren);

    // Now take all the children after currChild and remove them from the fromBlock
    // and put them in the toBlock.
    if (currChildNextSibling && currChildNextSibling->parent() == fromBlock)
        fromBlock->moveChildrenTo(toBlock, currChildNextSibling, 0, true);
}

void RenderBlock::splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                            RenderObject* newChild, RenderBoxModelObject* oldCont)
{
    RenderBlock* pre = 0;
    RenderBlock* block = containingColumnsBlock();
    
    // Delete our line boxes before we do the inline split into continuations.
    block->deleteLines();
    
    bool madeNewBeforeBlock = false;
    if (block->isAnonymousColumnsBlock()) {
        // We can reuse this block and make it the preBlock of the next continuation.
        pre = block;
        pre->removePositionedObjects(0);
        // FIXME-BLOCKFLOW remove this when splitFlow is moved to RenderBlockFlow.
        if (pre->isRenderBlockFlow())
            toRenderBlockFlow(pre)->removeFloatingObjects();
        block = toRenderBlock(block->parent());
    } else {
        // No anonymous block available for use.  Make one.
        pre = block->createAnonymousColumnsBlock();
        pre->setChildrenInline(false);
        madeNewBeforeBlock = true;
    }

    RenderBlock* post = block->createAnonymousColumnsBlock();
    post->setChildrenInline(false);

    RenderObject* boxFirst = madeNewBeforeBlock ? block->firstChild() : pre->nextSibling();
    if (madeNewBeforeBlock)
        block->insertChildInternal(pre, boxFirst, NotifyChildren);
    block->insertChildInternal(newBlockBox, boxFirst, NotifyChildren);
    block->insertChildInternal(post, boxFirst, NotifyChildren);
    block->setChildrenInline(false);
    
    if (madeNewBeforeBlock)
        block->moveChildrenTo(pre, boxFirst, 0, true);

    splitBlocks(pre, post, newBlockBox, beforeChild, oldCont);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    newBlockBox->setChildrenInline(false);

    // We delayed adding the newChild until now so that the |newBlockBox| would be fully
    // connected, thus allowing newChild access to a renderArena should it need
    // to wrap itself in additional boxes (e.g., table construction).
    newBlockBox->addChild(newChild);

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly.  Because objects moves from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    pre->setNeedsLayoutAndPrefWidthsRecalc();
    block->setNeedsLayoutAndPrefWidthsRecalc();
    post->setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderBlock::makeChildrenAnonymousColumnBlocks(RenderObject* beforeChild, RenderBlock* newBlockBox, RenderObject* newChild)
{
    RenderBlock* pre = 0;
    RenderBlock* post = 0;
    RenderBlock* block = this; // Eventually block will not just be |this|, but will also be a block nested inside |this|.  Assign to a variable
                               // so that we don't have to patch all of the rest of the code later on.
    
    // Delete the block's line boxes before we do the split.
    block->deleteLines();

    if (beforeChild && beforeChild->parent() != this)
        beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    if (beforeChild != firstChild()) {
        pre = block->createAnonymousColumnsBlock();
        pre->setChildrenInline(block->childrenInline());
    }

    if (beforeChild) {
        post = block->createAnonymousColumnsBlock();
        post->setChildrenInline(block->childrenInline());
    }

    RenderObject* boxFirst = block->firstChild();
    if (pre)
        block->insertChildInternal(pre, boxFirst, NotifyChildren);
    block->insertChildInternal(newBlockBox, boxFirst, NotifyChildren);
    if (post)
        block->insertChildInternal(post, boxFirst, NotifyChildren);
    block->setChildrenInline(false);
    
    // The pre/post blocks always have layers, so we know to always do a full insert/remove (so we pass true as the last argument).
    block->moveChildrenTo(pre, boxFirst, beforeChild, true);
    block->moveChildrenTo(post, beforeChild, 0, true);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    newBlockBox->setChildrenInline(false);

    // We delayed adding the newChild until now so that the |newBlockBox| would be fully
    // connected, thus allowing newChild access to a renderArena should it need
    // to wrap itself in additional boxes (e.g., table construction).
    newBlockBox->addChild(newChild);

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly.  Because objects moved from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    if (pre)
        pre->setNeedsLayoutAndPrefWidthsRecalc();
    block->setNeedsLayoutAndPrefWidthsRecalc();
    if (post)
        post->setNeedsLayoutAndPrefWidthsRecalc();
}

RenderBlock* RenderBlock::columnsBlockForSpanningElement(RenderObject* newChild)
{
    // FIXME: This function is the gateway for the addition of column-span support.  It will
    // be added to in three stages:
    // (1) Immediate children of a multi-column block can span.
    // (2) Nested block-level children with only block-level ancestors between them and the multi-column block can span.
    // (3) Nested children with block or inline ancestors between them and the multi-column block can span (this is when we
    // cross the streams and have to cope with both types of continuations mixed together).
    // This function currently supports (1) and (2).
    RenderBlock* columnsBlockAncestor = 0;
    if (!newChild->isText() && newChild->style().columnSpan() && !newChild->isBeforeOrAfterContent()
        && !newChild->isFloatingOrOutOfFlowPositioned() && !newChild->isInline() && !isAnonymousColumnSpanBlock()) {
        columnsBlockAncestor = containingColumnsBlock(false);
        if (columnsBlockAncestor) {
            // Make sure that none of the parent ancestors have a continuation.
            // If yes, we do not want split the block into continuations.
            RenderElement* curr = this;
            while (curr && curr != columnsBlockAncestor) {
                if (curr->isRenderBlock() && toRenderBlock(curr)->continuation()) {
                    columnsBlockAncestor = 0;
                    break;
                }
                curr = curr->parent();
            }
        }
    }
    return columnsBlockAncestor;
}

void RenderBlock::addChildIgnoringAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != this) {
        RenderElement* beforeChildContainer = beforeChild->parent();
        while (beforeChildContainer->parent() != this)
            beforeChildContainer = beforeChildContainer->parent();
        ASSERT(beforeChildContainer);

        if (beforeChildContainer->isAnonymous()) {
            // If the requested beforeChild is not one of our children, then this is because
            // there is an anonymous container within this object that contains the beforeChild.
            RenderElement* beforeChildAnonymousContainer = beforeChildContainer;
            if (beforeChildAnonymousContainer->isAnonymousBlock()
#if ENABLE(FULLSCREEN_API)
                // Full screen renderers and full screen placeholders act as anonymous blocks, not tables:
                || beforeChildAnonymousContainer->isRenderFullScreen()
                || beforeChildAnonymousContainer->isRenderFullScreenPlaceholder()
#endif
                ) {
                // Insert the child into the anonymous block box instead of here.
                if (newChild->isInline() || beforeChild->parent()->firstChild() != beforeChild)
                    beforeChild->parent()->addChild(newChild, beforeChild);
                else
                    addChild(newChild, beforeChild->parent());
                return;
            }

            ASSERT(beforeChildAnonymousContainer->isTable());
            if (newChild->isTablePart()) {
                // Insert into the anonymous table.
                beforeChildAnonymousContainer->addChild(newChild, beforeChild);
                return;
            }

            beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

            ASSERT(beforeChild->parent() == this);
            if (beforeChild->parent() != this) {
                // We should never reach here. If we do, we need to use the
                // safe fallback to use the topmost beforeChild container.
                beforeChild = beforeChildContainer;
            }
        }
    }

    // Check for a spanning element in columns.
    if (gColumnFlowSplitEnabled) {
        RenderBlock* columnsBlockAncestor = columnsBlockForSpanningElement(newChild);
        if (columnsBlockAncestor) {
            TemporaryChange<bool> columnFlowSplitEnabled(gColumnFlowSplitEnabled, false);
            // We are placing a column-span element inside a block.
            RenderBlock* newBox = createAnonymousColumnSpanBlock();
        
            if (columnsBlockAncestor != this && !isRenderFlowThread()) {
                // We are nested inside a multi-column element and are being split by the span. We have to break up
                // our block into continuations.
                RenderBoxModelObject* oldContinuation = continuation();

                // When we split an anonymous block, there's no need to do any continuation hookup,
                // since we haven't actually split a real element.
                if (!isAnonymousBlock())
                    setContinuation(newBox);

                splitFlow(beforeChild, newBox, newChild, oldContinuation);
                return;
            }

            // We have to perform a split of this block's children. This involves creating an anonymous block box to hold
            // the column-spanning |newChild|. We take all of the children from before |newChild| and put them into
            // one anonymous columns block, and all of the children after |newChild| go into another anonymous block.
            makeChildrenAnonymousColumnBlocks(beforeChild, newBox, newChild);
            return;
        }
    }

    bool madeBoxesNonInline = false;

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes.
    if (childrenInline() && !newChild->isInline() && !newChild->isFloatingOrOutOfFlowPositioned()) {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        makeChildrenNonInline(beforeChild);
        madeBoxesNonInline = true;

        if (beforeChild && beforeChild->parent() != this) {
            beforeChild = beforeChild->parent();
            ASSERT(beforeChild->isAnonymousBlock());
            ASSERT(beforeChild->parent() == this);
        }
    } else if (!childrenInline() && (newChild->isFloatingOrOutOfFlowPositioned() || newChild->isInline())) {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            toRenderBlock(afterChild)->addChild(newChild);
            return;
        }

        if (newChild->isInline()) {
            // No suitable existing anonymous box - create a new one.
            RenderBlock* newBox = createAnonymousBlock();
            RenderBox::addChild(newBox, beforeChild);
            newBox->addChild(newChild);
            return;
        }
    }

    invalidateLineLayoutPath();

    RenderBox::addChild(newChild, beforeChild);
 
    if (madeBoxesNonInline && parent() && isAnonymousBlock() && parent()->isRenderBlock())
        toRenderBlock(parent())->removeLeftoverAnonymousBlock(this);
    // this object may be dead here
}

void RenderBlock::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (continuation() && !isAnonymousBlock())
        addChildToContinuation(newChild, beforeChild);
    else
        addChildIgnoringContinuation(newChild, beforeChild);
}

void RenderBlock::addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!isAnonymousBlock() && firstChild() && (firstChild()->isAnonymousColumnsBlock() || firstChild()->isAnonymousColumnSpanBlock()))
        addChildToAnonymousColumnBlocks(newChild, beforeChild);
    else
        addChildIgnoringAnonymousColumnBlocks(newChild, beforeChild);
}

static void getInlineRun(RenderObject* start, RenderObject* boundary,
                         RenderObject*& inlineRunStart,
                         RenderObject*& inlineRunEnd)
{
    // Beginning at |start| we find the largest contiguous run of inlines that
    // we can.  We denote the run with start and end points, |inlineRunStart|
    // and |inlineRunEnd|.  Note that these two values may be the same if
    // we encounter only one inline.
    //
    // We skip any non-inlines we encounter as long as we haven't found any
    // inlines yet.
    //
    // |boundary| indicates a non-inclusive boundary point.  Regardless of whether |boundary|
    // is inline or not, we will not include it in a run with inlines before it.  It's as though we encountered
    // a non-inline.
    
    // Start by skipping as many non-inlines as we can.
    RenderObject * curr = start;
    bool sawInline;
    do {
        while (curr && !(curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()))
            curr = curr->nextSibling();
        
        inlineRunStart = inlineRunEnd = curr;
        
        if (!curr)
            return; // No more inline children to be found.
        
        sawInline = curr->isInline();
        
        curr = curr->nextSibling();
        while (curr && (curr->isInline() || curr->isFloatingOrOutOfFlowPositioned()) && (curr != boundary)) {
            inlineRunEnd = curr;
            if (curr->isInline())
                sawInline = true;
            curr = curr->nextSibling();
        }
    } while (!sawInline);
}

void RenderBlock::deleteLines()
{
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->recomputeIsIgnored(this);
}

void RenderBlock::invalidateLineLayoutPath()
{
    if (m_lineLayoutPath == ForceLineBoxesPath)
        return;
    m_lineLayoutPath = UndeterminedPath;
}

void RenderBlock::makeChildrenNonInline(RenderObject* insertionPoint)
{    
    // makeChildrenNonInline takes a block whose children are *all* inline and it
    // makes sure that inline children are coalesced under anonymous
    // blocks.  If |insertionPoint| is defined, then it represents the insertion point for
    // the new block child that is causing us to have to wrap all the inlines.  This
    // means that we cannot coalesce inlines before |insertionPoint| with inlines following
    // |insertionPoint|, because the new child is going to be inserted in between the inlines,
    // splitting them.
    ASSERT(isInlineBlockOrInlineTable() || !isInline());
    ASSERT(!insertionPoint || insertionPoint->parent() == this);

    setChildrenInline(false);

    RenderObject* child = firstChild();
    if (!child)
        return;

    deleteLines();

    while (child) {
        RenderObject* inlineRunStart;
        RenderObject* inlineRunEnd;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        RenderBlock* block = createAnonymousBlock();
        insertChildInternal(block, inlineRunStart, NotifyChildren);
        moveChildrenTo(block, inlineRunStart, child);
    }

#ifndef NDEBUG
    for (RenderObject* c = firstChild(); c; c = c->nextSibling())
        ASSERT(!c->isInline());
#endif

    repaint();
}

void RenderBlock::removeLeftoverAnonymousBlock(RenderBlock* child)
{
    ASSERT(child->isAnonymousBlock());
    ASSERT(!child->childrenInline());
    
    if (child->continuation() || (child->firstChild() && (child->isAnonymousColumnSpanBlock() || child->isAnonymousColumnsBlock())))
        return;
    
    RenderObject* firstAnChild = child->firstChild();
    RenderObject* lastAnChild = child->lastChild();
    if (firstAnChild) {
        RenderObject* o = firstAnChild;
        while (o) {
            o->setParent(this);
            o = o->nextSibling();
        }
        firstAnChild->setPreviousSibling(child->previousSibling());
        lastAnChild->setNextSibling(child->nextSibling());
        if (child->previousSibling())
            child->previousSibling()->setNextSibling(firstAnChild);
        if (child->nextSibling())
            child->nextSibling()->setPreviousSibling(lastAnChild);
            
        if (child == firstChild())
            setFirstChild(firstAnChild);
        if (child == lastChild())
            setLastChild(lastAnChild);
    } else {
        if (child == firstChild())
            setFirstChild(child->nextSibling());
        if (child == lastChild())
            setLastChild(child->previousSibling());

        if (child->previousSibling())
            child->previousSibling()->setNextSibling(child->nextSibling());
        if (child->nextSibling())
            child->nextSibling()->setPreviousSibling(child->previousSibling());
    }

    child->setFirstChild(0);
    child->m_next = 0;

    // Remove all the information in the flow thread associated with the leftover anonymous block.
    child->removeFromRenderFlowThread();

    child->setParent(0);
    child->setPreviousSibling(0);
    child->setNextSibling(0);

    child->destroy();
}

static bool canMergeAnonymousBlock(RenderBlock* anonymousBlock)
{
    if (anonymousBlock->beingDestroyed() || anonymousBlock->continuation())
        return false;
    if (anonymousBlock->isRubyRun() || anonymousBlock->isRubyBase())
        return false;
    return true;
}

static bool canMergeContiguousAnonymousBlocks(RenderObject& oldChild, RenderObject* previous, RenderObject* next)
{
    if (oldChild.documentBeingDestroyed() || oldChild.isInline() || oldChild.virtualContinuation())
        return false;

    if (previous) {
        if (!previous->isAnonymousBlock())
            return false;
        RenderBlock* previousAnonymousBlock = toRenderBlock(previous);
        if (!canMergeAnonymousBlock(previousAnonymousBlock))
            return false;
    }
    if (next) {
        if (!next->isAnonymousBlock())
            return false;
        RenderBlock* nextAnonymousBlock = toRenderBlock(next);
        if (!canMergeAnonymousBlock(nextAnonymousBlock))
            return false;
    }
    if (!previous || !next)
        return true;

    // Make sure the types of the anonymous blocks match up.
    return previous->isAnonymousColumnsBlock() == next->isAnonymousColumnsBlock()
        && previous->isAnonymousColumnSpanBlock() == next->isAnonymousColumnSpanBlock();
}

void RenderBlock::collapseAnonymousBoxChild(RenderBlock* parent, RenderBlock* child)
{
    parent->setNeedsLayoutAndPrefWidthsRecalc();
    parent->setChildrenInline(child->childrenInline());
    RenderObject* nextSibling = child->nextSibling();

    RenderFlowThread* childFlowThread = child->flowThreadContainingBlock();
    CurrentRenderFlowThreadMaintainer flowThreadMaintainer(childFlowThread);
    if (childFlowThread && childFlowThread->isRenderNamedFlowThread())
        toRenderNamedFlowThread(childFlowThread)->removeFlowChildInfo(child);

    parent->removeChildInternal(*child, child->hasLayer() ? NotifyChildren : DontNotifyChildren);
    child->moveAllChildrenTo(parent, nextSibling, child->hasLayer());
    // Delete the now-empty block's lines and nuke it.
    child->deleteLines();
    child->destroy();
}

void RenderBlock::removeChild(RenderObject& oldChild)
{
    // No need to waste time in merging or removing empty anonymous blocks.
    // We can just bail out if our document is getting destroyed.
    if (documentBeingDestroyed()) {
        RenderBox::removeChild(oldChild);
        return;
    }

    // This protects against column split flows when anonymous blocks are getting merged.
    TemporaryChange<bool> columnFlowSplitEnabled(gColumnFlowSplitEnabled, false);

    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    RenderObject* prev = oldChild.previousSibling();
    RenderObject* next = oldChild.nextSibling();
    bool canMergeAnonymousBlocks = canMergeContiguousAnonymousBlocks(oldChild, prev, next);
    if (canMergeAnonymousBlocks && prev && next) {
        prev->setNeedsLayoutAndPrefWidthsRecalc();
        RenderBlock* nextBlock = toRenderBlock(next);
        RenderBlock* prevBlock = toRenderBlock(prev);
       
        if (prev->childrenInline() != next->childrenInline()) {
            RenderBlock* inlineChildrenBlock = prev->childrenInline() ? prevBlock : nextBlock;
            RenderBlock* blockChildrenBlock = prev->childrenInline() ? nextBlock : prevBlock;
            
            // Place the inline children block inside of the block children block instead of deleting it.
            // In order to reuse it, we have to reset it to just be a generic anonymous block.  Make sure
            // to clear out inherited column properties by just making a new style, and to also clear the
            // column span flag if it is set.
            ASSERT(!inlineChildrenBlock->continuation());
            // Cache this value as it might get changed in setStyle() call.
            bool inlineChildrenBlockHasLayer = inlineChildrenBlock->hasLayer();
            inlineChildrenBlock->setStyle(RenderStyle::createAnonymousStyleWithDisplay(&style(), BLOCK));
            removeChildInternal(*inlineChildrenBlock, inlineChildrenBlockHasLayer ? NotifyChildren : DontNotifyChildren);
            
            // Now just put the inlineChildrenBlock inside the blockChildrenBlock.
            RenderObject* beforeChild = prev == inlineChildrenBlock ? blockChildrenBlock->firstChild() : nullptr;
            blockChildrenBlock->insertChildInternal(inlineChildrenBlock, beforeChild,
                (inlineChildrenBlockHasLayer || blockChildrenBlock->hasLayer()) ? NotifyChildren : DontNotifyChildren);
            next->setNeedsLayoutAndPrefWidthsRecalc();
            
            // inlineChildrenBlock got reparented to blockChildrenBlock, so it is no longer a child
            // of "this". we null out prev or next so that is not used later in the function.
            if (inlineChildrenBlock == prevBlock)
                prev = 0;
            else
                next = 0;
        } else {
            // Take all the children out of the |next| block and put them in
            // the |prev| block.
            nextBlock->moveAllChildrenIncludingFloatsTo(prevBlock, nextBlock->hasLayer() || prevBlock->hasLayer());
            
            // Delete the now-empty block's lines and nuke it.
            nextBlock->deleteLines();
            nextBlock->destroy();
            next = 0;
        }
    }

    invalidateLineLayoutPath();

    RenderBox::removeChild(oldChild);

    RenderObject* child = prev ? prev : next;
    if (canMergeAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling() && canCollapseAnonymousBlockChild()) {
        // The removal has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        collapseAnonymousBoxChild(this, toRenderBlock(child));
    } else if (((prev && prev->isAnonymousBlock()) || (next && next->isAnonymousBlock())) && canCollapseAnonymousBlockChild()) {
        // It's possible that the removal has knocked us down to a single anonymous
        // block with pseudo-style element siblings (e.g. first-letter). If these
        // are floating, then we need to pull the content up also.
        RenderBlock* anonBlock = toRenderBlock((prev && prev->isAnonymousBlock()) ? prev : next);
        if ((anonBlock->previousSibling() || anonBlock->nextSibling())
            && (!anonBlock->previousSibling() || (anonBlock->previousSibling()->style().styleType() != NOPSEUDO && anonBlock->previousSibling()->isFloating() && !anonBlock->previousSibling()->previousSibling()))
            && (!anonBlock->nextSibling() || (anonBlock->nextSibling()->style().styleType() != NOPSEUDO && anonBlock->nextSibling()->isFloating() && !anonBlock->nextSibling()->nextSibling()))) {
            collapseAnonymousBoxChild(this, anonBlock);
        }
    }

    if (!firstChild()) {
        // If this was our last child be sure to clear out our line boxes.
        if (childrenInline())
            deleteLines();

        // If we are an empty anonymous block in the continuation chain,
        // we need to remove ourself and fix the continuation chain.
        if (!beingDestroyed() && isAnonymousBlockContinuation() && !oldChild.isListMarker()) {
            auto containingBlockIgnoringAnonymous = containingBlock();
            while (containingBlockIgnoringAnonymous && containingBlockIgnoringAnonymous->isAnonymousBlock())
                containingBlockIgnoringAnonymous = containingBlockIgnoringAnonymous->containingBlock();
            for (RenderObject* curr = this; curr; curr = curr->previousInPreOrder(containingBlockIgnoringAnonymous)) {
                if (curr->virtualContinuation() != this)
                    continue;

                // Found our previous continuation. We just need to point it to
                // |this|'s next continuation.
                RenderBoxModelObject* nextContinuation = continuation();
                if (curr->isRenderInline())
                    toRenderInline(curr)->setContinuation(nextContinuation);
                else if (curr->isRenderBlock())
                    toRenderBlock(curr)->setContinuation(nextContinuation);
                else
                    ASSERT_NOT_REACHED();

                break;
            }
            setContinuation(0);
            destroy();
        }
    }
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
        || style().logicalMinHeight().isPositive()
        || style().marginBeforeCollapse() == MSEPARATE || style().marginAfterCollapse() == MSEPARATE)
        return false;

    Length logicalHeightLength = style().logicalHeight();
    bool hasAutoHeight = logicalHeightLength.isAuto();
    if (logicalHeightLength.isPercent() && !document().inQuirksMode()) {
        hasAutoHeight = true;
        for (RenderBlock* cb = containingBlock(); !cb->isRenderView(); cb = cb->containingBlock()) {
            if (cb->style().logicalHeight().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (hasAutoHeight || ((logicalHeightLength.isFixed() || logicalHeightLength.isPercent()) && logicalHeightLength.isZero())) {
        // If the block has inline children, see if we generated any line boxes.  If we have any
        // line boxes, then we can't be self-collapsing, since we have content.
        if (childrenInline())
            return !hasLines();
        
        // Whether or not we collapse is dependent on whether all our normal flow children
        // are also self-collapsing.
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isFloatingOrOutOfFlowPositioned())
                continue;
            if (!child->isSelfCollapsingBlock())
                return false;
        }
        return true;
    }
    return false;
}

void RenderBlock::startDelayUpdateScrollInfo()
{
    if (gDelayUpdateScrollInfo == 0) {
        ASSERT(!gDelayedUpdateScrollInfoSet);
        gDelayedUpdateScrollInfoSet = new DelayedUpdateScrollInfoSet;
    }
    ASSERT(gDelayedUpdateScrollInfoSet);
    ++gDelayUpdateScrollInfo;
}

void RenderBlock::finishDelayUpdateScrollInfo()
{
    --gDelayUpdateScrollInfo;
    ASSERT(gDelayUpdateScrollInfo >= 0);
    if (gDelayUpdateScrollInfo == 0) {
        ASSERT(gDelayedUpdateScrollInfoSet);

        std::unique_ptr<DelayedUpdateScrollInfoSet> infoSet(gDelayedUpdateScrollInfoSet);
        gDelayedUpdateScrollInfoSet = 0;

        for (DelayedUpdateScrollInfoSet::iterator it = infoSet->begin(); it != infoSet->end(); ++it) {
            RenderBlock* block = *it;
            if (block->hasOverflowClip()) {
                block->layer()->updateScrollInfoAfterLayout();
                block->clearLayoutOverflow();
            }
        }
    }
}

void RenderBlock::removeFromDelayedUpdateScrollInfoSet()
{
    if (UNLIKELY(gDelayedUpdateScrollInfoSet != 0))
        gDelayedUpdateScrollInfoSet->remove(this);
}

void RenderBlock::updateScrollInfoAfterLayout()
{
    if (hasOverflowClip()) {
        if (style().isFlippedBlocksWritingMode()) {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=97937
            // Workaround for now. We cannot delay the scroll info for overflow
            // for items with opposite writing directions, as the contents needs
            // to overflow in that direction
            layer()->updateScrollInfoAfterLayout();
            return;
        }

        if (gDelayUpdateScrollInfo)
            gDelayedUpdateScrollInfoSet->add(this);
        else
            layer()->updateScrollInfoAfterLayout();
    }
}

void RenderBlock::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    OverflowEventDispatcher dispatcher(this);

    // Update our first letter info now.
    updateFirstLetter();

    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);
    
    // It's safe to check for control clip here, since controls can never be table cells.
    // If we have a lightweight clip, there can never be any overflow from children.
    if (hasControlClip() && m_overflow && !gDelayUpdateScrollInfo)
        clearLayoutOverflow();

    invalidateBackgroundObscurationStatus();
}

static RenderBlockRareData* getRareData(const RenderBlock* block)
{
    return gRareDataMap ? gRareDataMap->get(block) : 0;
}

static RenderBlockRareData& ensureRareData(const RenderBlock* block)
{
    if (!gRareDataMap)
        gRareDataMap = new RenderBlockRareDataMap;
    
    auto& rareData = gRareDataMap->add(block, nullptr).iterator->value;
    if (!rareData)
        rareData = std::make_unique<RenderBlockRareData>();
    return *rareData.get();
}

#if ENABLE(CSS_SHAPES)
void RenderBlock::imageChanged(WrappedImagePtr image, const IntRect*)
{
    RenderBox::imageChanged(image);

    if (!parent() || !everHadLayout())
        return;
}
#endif

void RenderBlock::preparePaginationBeforeBlockLayout(bool& relayoutChildren)
{
    // Regions changing widths can force us to relayout our children.
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (flowThread)
        flowThread->logicalWidthChangedInRegionsForBlock(this, relayoutChildren);
}

bool RenderBlock::updateLogicalWidthAndColumnWidth()
{
    LayoutUnit oldWidth = logicalWidth();
    LayoutUnit oldColumnWidth = computedColumnWidth();

    updateLogicalWidth();
    computeColumnCountAndWidth();

    bool hasBorderOrPaddingLogicalWidthChanged = m_hasBorderOrPaddingLogicalWidthChanged;
    m_hasBorderOrPaddingLogicalWidthChanged = false;

    return oldWidth != logicalWidth() || oldColumnWidth != computedColumnWidth() || hasBorderOrPaddingLogicalWidthChanged;
}

void RenderBlock::layoutBlock(bool, LayoutUnit)
{
    ASSERT_NOT_REACHED();
    clearNeedsLayout();
}

void RenderBlock::addOverflowFromChildren()
{
    if (!hasColumns()) {
        if (childrenInline())
            addOverflowFromInlineChildren();
        else
            addOverflowFromBlockChildren();
        
        // If this block is flowed inside a flow thread, make sure its overflow is propagated to the containing regions.
        if (m_overflow) {
            if (RenderFlowThread* containingFlowThread = flowThreadContainingBlock())
                containingFlowThread->addRegionsVisualOverflow(this, m_overflow->visualOverflowRect());
        }
    } else {
        ColumnInfo* colInfo = columnInfo();
        if (columnCount(colInfo)) {
            LayoutRect lastRect = columnRectAt(colInfo, columnCount(colInfo) - 1);
            addLayoutOverflow(lastRect);
            if (!hasOverflowClip())
                addVisualOverflow(lastRect);
        }
    }
}

void RenderBlock::computeOverflow(LayoutUnit oldClientAfterEdge, bool)
{
    clearOverflow();

    // Add overflow from children.
    addOverflowFromChildren();

    // Add in the overflow from positioned objects.
    addOverflowFromPositionedObjects();

    if (hasOverflowClip()) {
        // When we have overflow clip, propagate the original spillout since it will include collapsed bottom margins
        // and bottom padding.  Set the axis we don't care about to be 1, since we want this overflow to always
        // be considered reachable.
        LayoutRect clientRect(clientBoxRect());
        LayoutRect rectToApply;
        if (isHorizontalWritingMode())
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), 1, std::max<LayoutUnit>(0, oldClientAfterEdge - clientRect.y()));
        else
            rectToApply = LayoutRect(clientRect.x(), clientRect.y(), std::max<LayoutUnit>(0, oldClientAfterEdge - clientRect.x()), 1);
        addLayoutOverflow(rectToApply);
        if (hasRenderOverflow())
            m_overflow->setLayoutClientAfterEdge(oldClientAfterEdge);
    }
        
    // Add visual overflow from box-shadow and border-image-outset.
    addVisualEffectOverflow();

    // Add visual overflow from theme.
    addVisualOverflowFromTheme();
}

void RenderBlock::clearLayoutOverflow()
{
    if (!m_overflow)
        return;
    
    if (visualOverflowRect() == borderBoxRect()) {
        // FIXME: Implement complete solution for regions overflow.
        clearOverflow();
        return;
    }
    
    m_overflow->setLayoutOverflow(borderBoxRect());
}

void RenderBlock::addOverflowFromBlockChildren()
{
    for (auto child = firstChildBox(); child; child = child->nextSiblingBox()) {
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
        if (positionedObject->style().position() != FixedPosition) {
            LayoutUnit x = positionedObject->x();
            if (style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
                x -= verticalScrollbarWidth();
            addOverflowFromChild(positionedObject, LayoutSize(x, positionedObject->y()));
        }
    }
}

void RenderBlock::addVisualOverflowFromTheme()
{
    if (!style().hasAppearance())
        return;

    IntRect inflatedRect = pixelSnappedBorderBoxRect();
    theme().adjustRepaintRect(this, inflatedRect);
    addVisualOverflow(inflatedRect);

    if (RenderFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->addRegionsVisualOverflowFromTheme(this);
}

bool RenderBlock::isTopLayoutOverflowAllowed() const
{
    bool hasTopOverflow = RenderBox::isTopLayoutOverflowAllowed();
    if (!hasColumns() || style().columnProgression() == NormalColumnProgression)
        return hasTopOverflow;
    
    if (!(isHorizontalWritingMode() ^ !style().hasInlineColumnAxis()))
        hasTopOverflow = !hasTopOverflow;

    return hasTopOverflow;
}

bool RenderBlock::isLeftLayoutOverflowAllowed() const
{
    bool hasLeftOverflow = RenderBox::isLeftLayoutOverflowAllowed();
    if (!hasColumns() || style().columnProgression() == NormalColumnProgression)
        return hasLeftOverflow;
    
    if (isHorizontalWritingMode() ^ !style().hasInlineColumnAxis())
        hasLeftOverflow = !hasLeftOverflow;

    return hasLeftOverflow;
}

bool RenderBlock::expandsToEncloseOverhangingFloats() const
{
    return isInlineBlockOrInlineTable() || isFloatingOrOutOfFlowPositioned() || hasOverflowClip() || (parent() && parent()->isFlexibleBoxIncludingDeprecated())
           || hasColumns() || isTableCell() || isTableCaption() || isFieldset() || isWritingModeRoot() || isRoot();
}


LayoutUnit RenderBlock::computeStartPositionDeltaForChildAvoidingFloats(const RenderBox& child, LayoutUnit childMarginStart, RenderRegion* region)
{
    LayoutUnit startPosition = startOffsetForContent(region);

    // Add in our start margin.
    LayoutUnit oldPosition = startPosition + childMarginStart;
    LayoutUnit newPosition = oldPosition;

    LayoutUnit blockOffset = logicalTopForChild(child);
    if (region)
        blockOffset = std::max(blockOffset, blockOffset + (region->logicalTopForFlowThreadContent() - offsetFromLogicalTopOfFirstPage()));

    LayoutUnit startOff = startOffsetForLineInRegion(blockOffset, false, region, logicalHeightForChild(child));

    if (style().textAlign() != WEBKIT_CENTER && !child.style().marginStartUsing(&style()).isAuto()) {
        if (childMarginStart < 0)
            startOff += childMarginStart;
        newPosition = std::max(newPosition, startOff); // Let the float sit in the child's margin if it can fit.
    } else if (startOff != startPosition)
        newPosition = startOff + childMarginStart;

    return newPosition - oldPosition;
}

void RenderBlock::setLogicalLeftForChild(RenderBox& child, LayoutUnit logicalLeft, ApplyLayoutDeltaMode applyDelta)
{
    if (isHorizontalWritingMode()) {
        if (applyDelta == ApplyLayoutDelta)
            view().addLayoutDelta(LayoutSize(child.x() - logicalLeft, 0));
        child.setX(logicalLeft);
    } else {
        if (applyDelta == ApplyLayoutDelta)
            view().addLayoutDelta(LayoutSize(0, child.y() - logicalLeft));
        child.setY(logicalLeft);
    }
}

void RenderBlock::setLogicalTopForChild(RenderBox& child, LayoutUnit logicalTop, ApplyLayoutDeltaMode applyDelta)
{
    if (isHorizontalWritingMode()) {
        if (applyDelta == ApplyLayoutDelta)
            view().addLayoutDelta(LayoutSize(0, child.y() - logicalTop));
        child.setY(logicalTop);
    } else {
        if (applyDelta == ApplyLayoutDelta)
            view().addLayoutDelta(LayoutSize(child.x() - logicalTop, 0));
        child.setX(logicalTop);
    }
}

void RenderBlock::updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, RenderBox& child)
{
    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    if (relayoutChildren || (child.hasRelativeLogicalHeight() && !isRenderView()) || child.hasViewportPercentageLogicalHeight())
        child.setChildNeedsLayout(MarkOnlyThis);

    // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
    if (relayoutChildren && child.needsPreferredWidthsRecalculation())
        child.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
}

void RenderBlock::dirtyForLayoutFromPercentageHeightDescendants()
{
    if (!gPercentHeightDescendantsMap)
        return;

    TrackedRendererListHashSet* descendants = gPercentHeightDescendantsMap->get(this);
    if (!descendants)
        return;

    for (auto it = descendants->begin(), end = descendants->end(); it != end; ++it) {
        RenderBox* box = *it;
        while (box != this) {
            if (box->normalChildNeedsLayout())
                break;
            box->setChildNeedsLayout(MarkOnlyThis);
            
            // If the width of an image is affected by the height of a child (e.g., an image with an aspect ratio),
            // then we have to dirty preferred widths, since even enclosing blocks can become dirty as a result.
            // (A horizontal flexbox that contains an inline image wrapped in an anonymous block for example.)
            if (box->hasAspectRatio()) 
                box->setPreferredLogicalWidthsDirty(true);
            
            box = box->containingBlock();
            ASSERT(box);
            if (!box)
                break;
        }
    }
}

void RenderBlock::simplifiedNormalFlowLayout()
{
    if (childrenInline()) {
        ListHashSet<RootInlineBox*> lineBoxes;
        for (InlineWalker walker(*this); !walker.atEnd(); walker.advance()) {
            RenderObject* o = walker.current();
            if (!o->isOutOfFlowPositioned() && (o->isReplaced() || o->isFloating())) {
                RenderBox& box = toRenderBox(*o);
                box.layoutIfNeeded();
                if (box.inlineBoxWrapper())
                    lineBoxes.add(&box.inlineBoxWrapper()->root());
            } else if (o->isText() || (o->isRenderInline() && !walker.atEndOfInline()))
                o->clearNeedsLayout();
        }

        // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
        // FIXME: Find a way to invalidate the knownToHaveNoOverflow flag on the InlineBoxes.
        GlyphOverflowAndFallbackFontsMap textBoxDataMap;                  
        for (auto it = lineBoxes.begin(), end = lineBoxes.end(); it != end; ++it) {
            RootInlineBox* box = *it;
            box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
        }
    } else {
        for (auto box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (!box->isOutOfFlowPositioned())
                box->layoutIfNeeded();
        }
    }
}

bool RenderBlock::simplifiedLayout()
{
    if ((!posChildNeedsLayout() && !needsSimplifiedNormalFlowLayout()) || normalChildNeedsLayout() || selfNeedsLayout())
        return false;

    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasColumns() || hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());
    
    if (needsPositionedMovementLayout() && !tryLayoutDoingPositionedMovementOnly())
        return false;

    // Lay out positioned descendants or objects that just need to recompute overflow.
    if (needsSimplifiedNormalFlowLayout())
        simplifiedNormalFlowLayout();

    // Make sure a forced break is applied after the content if we are a flow thread in a simplified layout.
    // This ensures the size information is correctly computed for the last auto-height region receiving content.
    if (isRenderFlowThread())
        toRenderFlowThread(this)->applyBreakAfterContent(clientLogicalBottom());

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

    statePusher.pop();
    
    updateLayerTransform();

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
    return true;
}

void RenderBlock::markFixedPositionObjectForLayoutIfNeeded(RenderObject& child)
{
    if (child.style().position() != FixedPosition)
        return;

    bool hasStaticBlockPosition = child.style().hasStaticBlockPosition(isHorizontalWritingMode());
    bool hasStaticInlinePosition = child.style().hasStaticInlinePosition(isHorizontalWritingMode());
    if (!hasStaticBlockPosition && !hasStaticInlinePosition)
        return;

    auto o = child.parent();
    while (o && !o->isRenderView() && o->style().position() != AbsolutePosition)
        o = o->parent();
    if (o->style().position() != AbsolutePosition)
        return;

    RenderBox& box = toRenderBox(child);
    if (hasStaticInlinePosition) {
        LogicalExtentComputedValues computedValues;
        box.computeLogicalWidthInRegion(computedValues);
        LayoutUnit newLeft = computedValues.m_position;
        if (newLeft != box.logicalLeft())
            box.setChildNeedsLayout(MarkOnlyThis);
    } else if (hasStaticBlockPosition) {
        LayoutUnit oldTop = box.logicalTop();
        box.updateLogicalHeight();
        if (box.logicalTop() != oldTop)
            box.setChildNeedsLayout(MarkOnlyThis);
    }
}

LayoutUnit RenderBlock::marginIntrinsicLogicalWidthForChild(RenderBox& child) const
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child.style().marginStartUsing(&style());
    Length marginRight = child.style().marginEndUsing(&style());
    LayoutUnit margin = 0;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

void RenderBlock::layoutPositionedObjects(bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;
        
    if (hasColumns())
        view().layoutState()->clearPaginationInformation(); // Positioned objects are not part of the column flow, so they don't paginate with the columns.

    for (auto it = positionedDescendants->begin(), end = positionedDescendants->end(); it != end; ++it) {
        RenderBox& r = **it;
        
        estimateRegionRangeForBoxChild(r);

        // A fixed position element with an absolute positioned ancestor has no way of knowing if the latter has changed position. So
        // if this is a fixed position element, mark it for layout if it has an abspos ancestor and needs to move with that ancestor, i.e. 
        // it has static position.
        markFixedPositionObjectForLayoutIfNeeded(r);
        if (fixedPositionObjectsOnly) {
            r.layoutIfNeeded();
            continue;
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
        
        if (!r.needsLayout())
            r.markForPaginationRelayoutIfNeeded();
        
        // We don't have to do a full layout.  We just have to update our position. Try that first. If we have shrink-to-fit width
        // and we hit the available width constraint, the layoutIfNeeded() will catch it and do a full layout.
        if (r.needsPositionedMovementLayoutOnly() && r.tryLayoutDoingPositionedMovementOnly())
            r.clearNeedsLayout();
            
        // If we are paginated or in a line grid, go ahead and compute a vertical position for our object now.
        // If it's wrong we'll lay out again.
        LayoutUnit oldLogicalTop = 0;
        bool needsBlockDirectionLocationSetBeforeLayout = r.needsLayout() && view().layoutState()->needsBlockDirectionLocationSetBeforeLayout();
        if (needsBlockDirectionLocationSetBeforeLayout) {
            if (isHorizontalWritingMode() == r.isHorizontalWritingMode())
                r.updateLogicalHeight();
            else
                r.updateLogicalWidth();
            oldLogicalTop = logicalTopForChild(r);
        }

        r.layoutIfNeeded();

        // Lay out again if our estimate was wrong.
        if (needsBlockDirectionLocationSetBeforeLayout && logicalTopForChild(r) != oldLogicalTop) {
            r.setChildNeedsLayout(MarkOnlyThis);
            r.layoutIfNeeded();
        }

        if (updateRegionRangeForBoxChild(r)) {
            r.setNeedsLayout(MarkOnlyThis);
            r.layoutIfNeeded();
        }
    }
    
    if (hasColumns())
        view().layoutState()->m_columnInfo = columnInfo(); // FIXME: Kind of gross. We just put this back into the layout state so that pop() will work.
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
    ASSERT(!needsLayout());
    if (needsLayout())
        return;

    if (view().layoutState()->pageLogicalHeightChanged() || (view().layoutState()->pageLogicalHeight() && view().layoutState()->pageLogicalOffset(this, logicalTop()) != pageLogicalOffset()))
        setChildNeedsLayout(MarkOnlyThis);
}

void RenderBlock::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + location();
    
    PaintPhase phase = paintInfo.phase;

    // Check our region range to make sure we need to be painting in this region.
    if (paintInfo.renderNamedFlowFragment && !paintInfo.renderNamedFlowFragment->flowThread()->objectShouldPaintInFlowRegion(this, paintInfo.renderNamedFlowFragment))
        return;

    // Check if we need to do anything at all.
    // FIXME: Could eliminate the isRoot() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (!isRoot()) {
        LayoutRect overflowBox = overflowRectForPaintRejection(paintInfo.renderNamedFlowFragment);
        flipForWritingMode(overflowBox);
        overflowBox.inflate(maximalOutlineSize(paintInfo.phase));
        overflowBox.moveBy(adjustedPaintOffset);
        if (!overflowBox.intersects(paintInfo.rect)
#if PLATFORM(IOS)
            // FIXME: This may be applicable to non-iOS ports.
            && (!hasLayer() || !layer()->isComposited())
#endif
        )
            return;
    }

    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, phase, adjustedPaintOffset);

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index.  We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if (hasOverflowClip() && style().visibility() == VISIBLE && (phase == PaintPhaseBlockBackground || phase == PaintPhaseChildBlockBackground) && paintInfo.shouldPaintWithinRoot(*this) && !paintInfo.paintRootBackgroundOnly())
        layer()->paintOverflowControls(paintInfo.context, roundedIntPoint(adjustedPaintOffset), pixelSnappedIntRect(paintInfo.rect));
}

void RenderBlock::paintColumnRules(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.context->paintingDisabled())
        return;

    const Color& ruleColor = style().visitedDependentColor(CSSPropertyWebkitColumnRuleColor);
    bool ruleTransparent = style().columnRuleIsTransparent();
    EBorderStyle ruleStyle = style().columnRuleStyle();
    LayoutUnit ruleThickness = style().columnRuleWidth();
    LayoutUnit colGap = columnGap();
    bool renderRule = ruleStyle > BHIDDEN && !ruleTransparent;
    if (!renderRule)
        return;

    ColumnInfo* colInfo = columnInfo();
    unsigned colCount = columnCount(colInfo);

    bool antialias = shouldAntialiasLines(paintInfo.context);

    if (colInfo->progressionIsInline()) {
        bool leftToRight = style().isLeftToRightDirection() ^ colInfo->progressionIsReversed();
        LayoutUnit currLogicalLeftOffset = leftToRight ? LayoutUnit() : contentLogicalWidth();
        LayoutUnit ruleAdd = logicalLeftOffsetForContent();
        LayoutUnit ruleLogicalLeft = leftToRight ? LayoutUnit() : contentLogicalWidth();
        LayoutUnit inlineDirectionSize = colInfo->desiredColumnWidth();
        BoxSide boxSide = isHorizontalWritingMode()
            ? leftToRight ? BSLeft : BSRight
            : leftToRight ? BSTop : BSBottom;

        for (unsigned i = 0; i < colCount; i++) {
            // Move to the next position.
            if (leftToRight) {
                ruleLogicalLeft += inlineDirectionSize + colGap / 2;
                currLogicalLeftOffset += inlineDirectionSize + colGap;
            } else {
                ruleLogicalLeft -= (inlineDirectionSize + colGap / 2);
                currLogicalLeftOffset -= (inlineDirectionSize + colGap);
            }
           
            // Now paint the column rule.
            if (i < colCount - 1) {
                LayoutUnit ruleLeft = isHorizontalWritingMode() ? paintOffset.x() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd : paintOffset.x() + borderLeft() + paddingLeft();
                LayoutUnit ruleRight = isHorizontalWritingMode() ? ruleLeft + ruleThickness : ruleLeft + contentWidth();
                LayoutUnit ruleTop = isHorizontalWritingMode() ? paintOffset.y() + borderTop() + paddingTop() : paintOffset.y() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd;
                LayoutUnit ruleBottom = isHorizontalWritingMode() ? ruleTop + contentHeight() : ruleTop + ruleThickness;
                IntRect pixelSnappedRuleRect = pixelSnappedIntRectFromEdges(ruleLeft, ruleTop, ruleRight, ruleBottom);
                drawLineForBoxSide(paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(), pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide, ruleColor, ruleStyle, 0, 0, antialias);
            }
            
            ruleLogicalLeft = currLogicalLeftOffset;
        }
    } else {
        bool topToBottom = !style().isFlippedBlocksWritingMode() ^ colInfo->progressionIsReversed();
        LayoutUnit ruleLeft = isHorizontalWritingMode()
            ? borderLeft() + paddingLeft()
            : colGap / 2 - colGap - ruleThickness / 2 + (!colInfo->progressionIsReversed() ? borderAndPaddingBefore() : borderAndPaddingAfter());
        LayoutUnit ruleWidth = isHorizontalWritingMode() ? contentWidth() : ruleThickness;
        LayoutUnit ruleTop = isHorizontalWritingMode()
            ? colGap / 2 - colGap - ruleThickness / 2 + (!colInfo->progressionIsReversed() ? borderAndPaddingBefore() : borderAndPaddingAfter())
            : borderStart() + paddingStart();
        LayoutUnit ruleHeight = isHorizontalWritingMode() ? ruleThickness : contentHeight();
        LayoutRect ruleRect(ruleLeft, ruleTop, ruleWidth, ruleHeight);

        if (!topToBottom) {
            if (isHorizontalWritingMode())
                ruleRect.setY(height() - ruleRect.maxY());
            else
                ruleRect.setX(width() - ruleRect.maxX());
        }

        ruleRect.moveBy(paintOffset);

        BoxSide boxSide = isHorizontalWritingMode()
            ? topToBottom ? BSTop : BSBottom
            : topToBottom ? BSLeft : BSRight;

        LayoutSize step(0, topToBottom ? colInfo->columnHeight() + colGap : -(colInfo->columnHeight() + colGap));
        if (!isHorizontalWritingMode())
            step = step.transposedSize();

        for (unsigned i = 1; i < colCount; i++) {
            ruleRect.move(step);
            IntRect pixelSnappedRuleRect = pixelSnappedIntRect(ruleRect);
            drawLineForBoxSide(paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(), pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide, ruleColor, ruleStyle, 0, 0, antialias);
        }
    }
}

LayoutUnit RenderBlock::initialBlockOffsetForPainting() const
{
    ColumnInfo* colInfo = columnInfo();
    LayoutUnit result = 0;
    if (!colInfo->progressionIsInline() && colInfo->progressionIsReversed()) {
        LayoutRect colRect = columnRectAt(colInfo, 0);
        result = isHorizontalWritingMode() ? colRect.y() : colRect.x();
        result -= borderAndPaddingBefore();
        if (style().isFlippedBlocksWritingMode())
            result = -result;
    }
    return result;
}
    
LayoutUnit RenderBlock::blockDeltaForPaintingNextColumn() const
{
    ColumnInfo* colInfo = columnInfo();
    LayoutUnit blockDelta = -colInfo->columnHeight();
    LayoutUnit colGap = columnGap();
    if (!colInfo->progressionIsInline()) {
        if (!colInfo->progressionIsReversed())
            blockDelta = colGap;
        else
            blockDelta -= (colInfo->columnHeight() + colGap);
    }
    if (style().isFlippedBlocksWritingMode())
        blockDelta = -blockDelta;
    return blockDelta;
}

void RenderBlock::paintColumnContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset, bool paintingFloats)
{
    // We need to do multiple passes, breaking up our child painting into strips.
    GraphicsContext* context = paintInfo.context;
    ColumnInfo* colInfo = columnInfo();
    unsigned colCount = columnCount(colInfo);
    if (!colCount)
        return;
    LayoutUnit colGap = columnGap();
    LayoutUnit currLogicalTopOffset = initialBlockOffsetForPainting();
    LayoutUnit blockDelta = blockDeltaForPaintingNextColumn();
    for (unsigned i = 0; i < colCount; i++) {
        // For each rect, we clip to the rect, and then we adjust our coords.
        LayoutRect colRect = columnRectAt(colInfo, i);
        flipForWritingMode(colRect);
        
        LayoutUnit logicalLeftOffset = (isHorizontalWritingMode() ? colRect.x() : colRect.y()) - logicalLeftOffsetForContent();
        LayoutSize offset = isHorizontalWritingMode() ? LayoutSize(logicalLeftOffset, currLogicalTopOffset) : LayoutSize(currLogicalTopOffset, logicalLeftOffset);
        colRect.moveBy(paintOffset);
        PaintInfo info(paintInfo);
        info.rect.intersect(pixelSnappedIntRect(colRect));
        
        if (!info.rect.isEmpty()) {
            GraphicsContextStateSaver stateSaver(*context);
            LayoutRect clipRect(colRect);
            
            if (i < colCount - 1) {
                if (isHorizontalWritingMode())
                    clipRect.expand(colGap / 2, 0);
                else
                    clipRect.expand(0, colGap / 2);
            }
            // Each strip pushes a clip, since column boxes are specified as being
            // like overflow:hidden.
            // FIXME: Content and column rules that extend outside column boxes at the edges of the multi-column element
            // are clipped according to the 'overflow' property.
            context->clip(pixelSnappedIntRect(clipRect));

            // Adjust our x and y when painting.
            LayoutPoint adjustedPaintOffset = paintOffset + offset;
            if (paintingFloats)
                paintFloats(info, adjustedPaintOffset, paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip);
            else
                paintContents(info, adjustedPaintOffset);
        }
        currLogicalTopOffset += blockDelta;
    }
}

void RenderBlock::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Avoid painting descendants of the root element when stylesheets haven't loaded.  This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, styleResolverChanged() on the Document
    // will do a full repaint.
    if (document().didLayoutWithPendingStylesheets() && !isRenderView())
        return;

    if (childrenInline())
        paintInlineChildren(paintInfo, paintOffset);
    else {
        PaintPhase newPhase = (paintInfo.phase == PaintPhaseChildOutlines) ? PaintPhaseOutline : paintInfo.phase;
        newPhase = (newPhase == PaintPhaseChildBlockBackgrounds) ? PaintPhaseChildBlockBackground : newPhase;

        // We don't paint our own background, but we do let the kids paint their backgrounds.
        PaintInfo paintInfoForChild(paintInfo);
        paintInfoForChild.phase = newPhase;
        paintInfoForChild.updateSubtreePaintRootForChildren(this);

        // FIXME: Paint-time pagination is obsolete and is now only used by embedded WebViews inside AppKit
        // NSViews. Do not add any more code for this.
        bool usePrintRect = !view().printRect().isEmpty();
        paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
    }
}

void RenderBlock::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (auto child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect))
            return;
    }
}

bool RenderBlock::paintChild(RenderBox& child, PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    // Check for page-break-before: always, and if it's set, break and bail.
    bool checkBeforeAlways = !childrenInline() && (usePrintRect && child.style().pageBreakBefore() == PBALWAYS);
    LayoutUnit absoluteChildY = paintOffset.y() + child.y();
    if (checkBeforeAlways
        && absoluteChildY > paintInfo.rect.y()
        && absoluteChildY < paintInfo.rect.maxY()) {
        view().setBestTruncatedAt(absoluteChildY, this, true);
        return false;
    }

    if (!child.isFloating() && child.isReplaced() && usePrintRect && child.height() <= view().printRect().height()) {
        // Paginate block-level replaced elements.
        if (absoluteChildY + child.height() > view().printRect().maxY()) {
            if (absoluteChildY < view().truncatedAt())
                view().setBestTruncatedAt(absoluteChildY, &child);
            // If we were able to truncate, don't paint.
            if (absoluteChildY >= view().truncatedAt())
                return false;
        }
    }

    LayoutPoint childPoint = flipForWritingModeForChild(&child, paintOffset);
    if (!child.hasSelfPaintingLayer() && !child.isFloating())
        child.paint(paintInfoForChild, childPoint);

    // Check for page-break-after: always, and if it's set, break and bail.
    bool checkAfterAlways = !childrenInline() && (usePrintRect && child.style().pageBreakAfter() == PBALWAYS);
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
    bool caretBrowsing = frame().settings().caretBrowsingEnabled();
    RenderObject* caretPainter;
    bool isContentEditable;
    if (type == CursorCaret) {
        caretPainter = frame().selection().caretRendererWithoutUpdatingLayout();
        isContentEditable = frame().selection().selection().hasEditableStyle();
    } else {
        caretPainter = frame().page()->dragCaretController().caretRenderer();
        isContentEditable = frame().page()->dragCaretController().isContentEditable();
    }

    if (caretPainter == this && (isContentEditable || caretBrowsing)) {
        if (type == CursorCaret)
            frame().selection().paintCaret(paintInfo.context, paintOffset, paintInfo.rect);
        else
            frame().page()->dragCaretController().paintDragCaret(&frame(), paintInfo.context, paintOffset, paintInfo.rect);
    }
}

void RenderBlock::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) && style().visibility() == VISIBLE) {
        if (hasBoxDecorations()) {
            bool didClipToRegion = false;
            
            if (paintInfo.paintContainer && paintInfo.renderNamedFlowFragment && paintInfo.paintContainer->isRenderNamedFlowThread()) {
                // If this box goes beyond the current region, then make sure not to overflow the region.
                // This (overflowing region X altough also fragmented to region X+1) could happen when one of this box's children
                // overflows region X and is an unsplittable element (like an image).
                // The same applies for a box overflowing the top of region X when that box is also fragmented in region X-1.

                paintInfo.context->save();
                didClipToRegion = true;

                paintInfo.context->clip(toRenderNamedFlowThread(paintInfo.paintContainer)->decorationsClipRectForBoxInNamedFlowFragment(*this, *paintInfo.renderNamedFlowFragment));
            }

            paintBoxDecorations(paintInfo, paintOffset);
            
            if (didClipToRegion)
                paintInfo.context->restore();
        }
        if (hasColumns() && !paintInfo.paintRootBackgroundOnly())
            paintColumnRules(paintInfo, paintOffset);
    }

    if (paintPhase == PaintPhaseMask && style().visibility() == VISIBLE) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    // We're done.  We don't bother painting any children.
    if (paintPhase == PaintPhaseBlockBackground || paintInfo.paintRootBackgroundOnly())
        return;

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).
    LayoutPoint scrolledOffset = paintOffset;
    scrolledOffset.move(-scrolledContentOffset());

    // 2. paint contents
    if (paintPhase != PaintPhaseSelfOutline) {
        if (hasColumns())
            paintColumnContents(paintInfo, scrolledOffset);
        else
            paintContents(paintInfo, scrolledOffset);
    }

    // 3. paint selection
    // FIXME: Make this work with multi column layouts.  For now don't fill gaps.
    bool isPrinting = document().printing();
    if (!isPrinting && !hasColumns())
        paintSelection(paintInfo, scrolledOffset); // Fill in gaps in selection on lines and between blocks.

    // 4. paint floats.
    if (paintPhase == PaintPhaseFloat || paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip) {
        if (hasColumns())
            paintColumnContents(paintInfo, scrolledOffset, true);
        else
            paintFloats(paintInfo, scrolledOffset, paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip);
    }

    // 5. paint outline.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && hasOutline() && style().visibility() == VISIBLE)
        paintOutline(paintInfo, LayoutRect(paintOffset, size()));

    // 6. paint continuation outlines.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseChildOutlines)) {
        RenderInline* inlineCont = inlineElementContinuation();
        if (inlineCont && inlineCont->hasOutline() && inlineCont->style().visibility() == VISIBLE) {
            RenderInline* inlineRenderer = toRenderInline(inlineCont->element()->renderer());
            RenderBlock* cb = containingBlock();

            bool inlineEnclosedInSelfPaintingLayer = false;
            for (RenderBoxModelObject* box = inlineRenderer; box != cb; box = box->parent()->enclosingBoxModelObject()) {
                if (box->hasSelfPaintingLayer()) {
                    inlineEnclosedInSelfPaintingLayer = true;
                    break;
                }
            }

            // Do not add continuations for outline painting by our containing block if we are a relative positioned
            // anonymous block (i.e. have our own layer), paint them straightaway instead. This is because a block depends on renderers in its continuation table being
            // in the same layer. 
            if (!inlineEnclosedInSelfPaintingLayer && !hasLayer())
                cb->addContinuationWithOutline(inlineRenderer);
            else if (!inlineRenderer->firstLineBox() || (!inlineEnclosedInSelfPaintingLayer && hasLayer()))
                inlineRenderer->paintOutline(paintInfo, paintOffset - locationOffset() + inlineRenderer->containingBlock()->location());
        }
        paintContinuationOutlines(paintInfo, paintOffset);
    }

    // 7. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintPhaseForeground,
    // then paint the caret.
    if (paintPhase == PaintPhaseForeground) {        
        paintCaret(paintInfo, paintOffset, CursorCaret);
        paintCaret(paintInfo, paintOffset, DragCaret);
    }
}

RenderInline* RenderBlock::inlineElementContinuation() const
{ 
    RenderBoxModelObject* continuation = this->continuation();
    return continuation && continuation->isRenderInline() ? toRenderInline(continuation) : 0;
}

RenderBlock* RenderBlock::blockElementContinuation() const
{
    RenderBoxModelObject* currentContinuation = continuation();
    if (!currentContinuation || currentContinuation->isInline())
        return 0;
    RenderBlock* nextContinuation = toRenderBlock(currentContinuation);
    if (nextContinuation->isAnonymousBlock())
        return nextContinuation->blockElementContinuation();
    return nextContinuation;
}
    
static ContinuationOutlineTableMap* continuationOutlineTable()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(ContinuationOutlineTableMap, table, ());
    return &table;
}

void RenderBlock::addContinuationWithOutline(RenderInline* flow)
{
    // We can't make this work if the inline is in a layer.  We'll just rely on the broken
    // way of painting.
    ASSERT(!flow->layer() && !flow->isInlineElementContinuation());
    
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
    return selectionState() != SelectionNone && style().visibility() == VISIBLE && isSelectionRoot();
}

bool RenderBlock::isSelectionRoot() const
{
    if (isPseudoElement())
        return false;
    ASSERT(element() || isAnonymous());
        
    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    if (isTable())
        return false;
        
    if (isBody() || isRoot() || hasOverflowClip()
        || isPositioned() || isFloating()
        || isTableCell() || isInlineBlockOrInlineTable()
        || hasTransform() || hasReflection() || hasMask() || isWritingModeRoot()
        || isRenderFlowThread())
        return true;
    
    if (view().selectionStart()) {
        Node* startElement = view().selectionStart()->node();
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

    TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint());
    mapLocalToContainer(repaintContainer, transformState, ApplyContainerFlip | UseTransforms);
    LayoutPoint offsetFromRepaintContainer = roundedLayoutPoint(transformState.mappedPoint()) - scrolledContentOffset();

    LogicalSelectionOffsetCaches cache(*this);
    LayoutUnit lastTop = 0;
    LayoutUnit lastLeft = logicalLeftSelectionOffset(*this, lastTop, cache);
    LayoutUnit lastRight = logicalRightSelectionOffset(*this, lastTop, cache);
    
    return selectionGaps(*this, offsetFromRepaintContainer, IntSize(), lastTop, lastLeft, lastRight, cache);
}

void RenderBlock::paintSelection(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
#if ENABLE(TEXT_SELECTION)
    if (shouldPaintSelectionGaps() && paintInfo.phase == PaintPhaseForeground) {
        LogicalSelectionOffsetCaches cache(*this);
        LayoutUnit lastTop = 0;
        LayoutUnit lastLeft = logicalLeftSelectionOffset(*this, lastTop, cache);
        LayoutUnit lastRight = logicalRightSelectionOffset(*this, lastTop, cache);
        GraphicsContextStateSaver stateSaver(*paintInfo.context);

        LayoutRect gapRectsBounds = selectionGaps(*this, paintOffset, LayoutSize(), lastTop, lastLeft, lastRight, cache, &paintInfo);
        if (!gapRectsBounds.isEmpty()) {
            if (RenderLayer* layer = enclosingLayer()) {
                gapRectsBounds.moveBy(-paintOffset);
                if (!hasLayer()) {
                    LayoutRect localBounds(gapRectsBounds);
                    flipForWritingMode(localBounds);
                    gapRectsBounds = localToContainerQuad(FloatRect(localBounds), &layer->renderer()).enclosingBoundingBox();
                    if (layer->renderer().isBox())
                        gapRectsBounds.move(layer->renderBox()->scrolledContentOffset());
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
        paintInfo->context->clipOut(IntRect(offset.x() + r->x(), offset.y() + r->y(), r->width(), r->height()));
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
        if (isBody() || isRoot()) // The <body> must make sure to examine its containingBlock's positioned objects.
            for (RenderBlock* cb = containingBlock(); cb && !cb->isRenderView(); cb = cb->containingBlock())
                clipOutPositionedObjects(paintInfo, LayoutPoint(cb->x(), cb->y()), cb->positionedObjects()); // FIXME: Not right for flipped writing modes.
        clipOutFloatingObjects(rootBlock, paintInfo, rootBlockPhysicalPosition, offsetFromRootBlock);
    }

    // FIXME: overflow: auto/scroll regions need more math here, since painting in the border box is different from painting in the padding box (one is scrolled, the other is
    // fixed).
    GapRects result;
    if (!isRenderBlockFlow()) // FIXME: Make multi-column selection gap filling work someday.
        return result;

    if (hasColumns() || hasTransform() || style().columnSpan()) {
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

    // Go ahead and fill the vertical gap all the way to the bottom of our block if the selection extends past our block.
    if (&rootBlock == this && (selectionState() != SelectionBoth && selectionState() != SelectionEnd)) {
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

    // Go ahead and jump right to the first block child that contains some selected objects.
    RenderBox* curr;
    for (curr = firstChildBox(); curr && curr->selectionState() == SelectionNone; curr = curr->nextSiblingBox()) { }
    
    if (!curr)
        return result;

    LogicalSelectionOffsetCaches childCache(*this, cache);

    for (bool sawSelectionEnd = false; curr && !sawSelectionEnd; curr = curr->nextSiblingBox()) {
        SelectionState childState = curr->selectionState();
        if (childState == SelectionBoth || childState == SelectionEnd)
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
        bool fillBlockGaps = paintsOwnSelection || (curr->canBeSelectionLeaf() && childState != SelectionNone);
        if (fillBlockGaps) {
            // We need to fill the vertical gap above this object.
            if (childState == SelectionEnd || childState == SelectionInside) {
                // Fill the gap above the object.
                result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
                    lastLogicalTop, lastLogicalLeft, lastLogicalRight, curr->logicalTop(), cache, paintInfo));
            }

            // Only fill side gaps for objects that paint their own selection if we know for sure the selection is going to extend all the way *past*
            // our object.  We know this if the selection did not end inside our object.
            if (paintsOwnSelection && (childState == SelectionStart || sawSelectionEnd))
                childState = SelectionNone;

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
        } else if (childState != SelectionNone) {
            // We must be a block that has some selected object inside it.  Go ahead and recur.
            result.unite(toRenderBlock(curr)->selectionGaps(rootBlock, rootBlockPhysicalPosition, LayoutSize(offsetFromRootBlock.width() + curr->x(), offsetFromRootBlock.height() + curr->y()), 
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
        paintInfo->context->fillRect(pixelSnappedIntRect(gapRect), selectionBackgroundColor(), style().colorSpace());
    return gapRect;
}

LayoutRect RenderBlock::logicalLeftSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    RenderObject* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = std::max(logicalLeftSelectionOffset(rootBlock, logicalTop, cache), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight, cache));
    LayoutUnit rootBlockLogicalRight = std::min(inlineDirectionOffset(rootBlock, offsetFromRootBlock) + floorToInt(logicalLeft),
        std::min(logicalRightSelectionOffset(rootBlock, logicalTop, cache), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight, cache)));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context->fillRect(pixelSnappedIntRect(gapRect), selObj->selectionBackgroundColor(), selObj->style().colorSpace());
    return gapRect;
}

LayoutRect RenderBlock::logicalRightSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    RenderObject* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = std::max(inlineDirectionOffset(rootBlock, offsetFromRootBlock) + floorToInt(logicalRight),
        std::max(logicalLeftSelectionOffset(rootBlock, logicalTop, cache), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight, cache)));
    LayoutUnit rootBlockLogicalRight = std::min(logicalRightSelectionOffset(rootBlock, logicalTop, cache), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight, cache));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context->fillRect(pixelSnappedIntRect(gapRect), selObj->selectionBackgroundColor(), selObj->style().colorSpace());
    return gapRect;
}

void RenderBlock::getSelectionGapInfo(SelectionState state, bool& leftGap, bool& rightGap)
{
    bool ltr = style().isLeftToRightDirection();
    leftGap = (state == RenderObject::SelectionInside) ||
              (state == RenderObject::SelectionEnd && ltr) ||
              (state == RenderObject::SelectionStart && !ltr);
    rightGap = (state == RenderObject::SelectionInside) ||
               (state == RenderObject::SelectionStart && ltr) ||
               (state == RenderObject::SelectionEnd && !ltr);
}

LayoutUnit RenderBlock::logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches& cache)
{
    LayoutUnit logicalLeft = logicalLeftOffsetForLine(position, false);
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
        currentCache = info.cache();
    }
    return logicalLeft;
}

LayoutUnit RenderBlock::logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches& cache)
{
    LayoutUnit logicalRight = logicalRightOffsetForLine(position, false);
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
        currentCache = info.cache();
    }
    return logicalRight;
}

RenderBlock* RenderBlock::blockBeforeWithinSelectionRoot(LayoutSize& offset) const
{
    if (isSelectionRoot())
        return 0;

    const RenderElement* object = this;
    RenderObject* sibling;
    do {
        sibling = object->previousSibling();
        while (sibling && (!sibling->isRenderBlock() || toRenderBlock(sibling)->isSelectionRoot()))
            sibling = sibling->previousSibling();

        offset -= LayoutSize(toRenderBlock(object)->logicalLeft(), toRenderBlock(object)->logicalTop());
        object = object->parent();
    } while (!sibling && object && object->isRenderBlock() && !toRenderBlock(object)->isSelectionRoot());

    if (!sibling)
        return 0;

    RenderBlock* beforeBlock = toRenderBlock(sibling);

    offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

    RenderObject* child = beforeBlock->lastChild();
    while (child && child->isRenderBlock()) {
        beforeBlock = toRenderBlock(child);
        offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
        child = beforeBlock->lastChild();
    }
    return beforeBlock;
}

void RenderBlock::insertIntoTrackedRendererMaps(RenderBox& descendant, TrackedDescendantsMap*& descendantsMap, TrackedContainerMap*& containerMap)
{
    if (!descendantsMap) {
        descendantsMap = new TrackedDescendantsMap;
        containerMap = new TrackedContainerMap;
    }
    
    TrackedRendererListHashSet* descendantSet = descendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new TrackedRendererListHashSet;
        descendantsMap->set(this, std::unique_ptr<TrackedRendererListHashSet>(descendantSet));
    }
    bool added = descendantSet->add(&descendant).isNewEntry;
    if (!added) {
        ASSERT(containerMap->get(&descendant));
        ASSERT(containerMap->get(&descendant)->contains(this));
        return;
    }
    
    HashSet<RenderBlock*>* containerSet = containerMap->get(&descendant);
    if (!containerSet) {
        containerSet = new HashSet<RenderBlock*>;
        containerMap->set(&descendant, std::unique_ptr<HashSet<RenderBlock*>>(containerSet));
    }
    ASSERT(!containerSet->contains(this));
    containerSet->add(this);
}

void RenderBlock::removeFromTrackedRendererMaps(RenderBox& descendant, TrackedDescendantsMap*& descendantsMap, TrackedContainerMap*& containerMap)
{
    if (!descendantsMap)
        return;
    
    std::unique_ptr<HashSet<RenderBlock*>> containerSet = containerMap->take(&descendant);
    if (!containerSet)
        return;
    
    for (auto it = containerSet->begin(), end = containerSet->end(); it != end; ++it) {
        RenderBlock* container = *it;

        // FIXME: Disabling this assert temporarily until we fix the layout
        // bugs associated with positioned objects not properly cleared from
        // their ancestor chain before being moved. See webkit bug 93766.
        // ASSERT(descendant->isDescendantOf(container));

        TrackedDescendantsMap::iterator descendantsMapIterator = descendantsMap->find(container);
        ASSERT(descendantsMapIterator != descendantsMap->end());
        if (descendantsMapIterator == descendantsMap->end())
            continue;
        TrackedRendererListHashSet* descendantSet = descendantsMapIterator->value.get();
        ASSERT(descendantSet->contains(&descendant));
        descendantSet->remove(&descendant);
        if (descendantSet->isEmpty())
            descendantsMap->remove(descendantsMapIterator);
    }
}

TrackedRendererListHashSet* RenderBlock::positionedObjects() const
{
    if (gPositionedDescendantsMap)
        return gPositionedDescendantsMap->get(this);
    return 0;
}

void RenderBlock::insertPositionedObject(RenderBox& o)
{
    ASSERT(!isAnonymousBlock());

    if (o.isRenderFlowThread())
        return;
    
    insertIntoTrackedRendererMaps(o, gPositionedDescendantsMap, gPositionedContainerMap);
}

void RenderBlock::removePositionedObject(RenderBox& o)
{
    removeFromTrackedRendererMaps(o, gPositionedDescendantsMap, gPositionedContainerMap);
}

void RenderBlock::removePositionedObjects(RenderBlock* o, ContainingBlockState containingBlockState)
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;
    
    Vector<RenderBox*, 16> deadObjects;

    for (auto it = positionedDescendants->begin(), end = positionedDescendants->end(); it != end; ++it) {
        RenderBox* r = *it;
        if (!o || r->isDescendantOf(o)) {
            if (containingBlockState == NewContainingBlock)
                r->setChildNeedsLayout(MarkOnlyThis);
            
            // It is parent blocks job to add positioned child to positioned objects list of its containing block
            // Parent layout needs to be invalidated to ensure this happens.
            RenderElement* p = r->parent();
            while (p && !p->isRenderBlock())
                p = p->parent();
            if (p)
                p->setChildNeedsLayout();
            
            deadObjects.append(r);
        }
    }
    
    for (unsigned i = 0; i < deadObjects.size(); i++)
        removePositionedObject(*deadObjects.at(i));
}

void RenderBlock::addPercentHeightDescendant(RenderBox& descendant)
{
    insertIntoTrackedRendererMaps(descendant, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
}

void RenderBlock::removePercentHeightDescendant(RenderBox& descendant)
{
    removeFromTrackedRendererMaps(descendant, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
}

TrackedRendererListHashSet* RenderBlock::percentHeightDescendants() const
{
    return gPercentHeightDescendantsMap ? gPercentHeightDescendantsMap->get(this) : 0;
}

bool RenderBlock::hasPercentHeightContainerMap()
{
    return gPercentHeightContainerMap;
}

bool RenderBlock::hasPercentHeightDescendant(RenderBox& descendant)
{
    // We don't null check gPercentHeightContainerMap since the caller
    // already ensures this and we need to call this function on every
    // descendant in clearPercentHeightDescendantsFrom().
    ASSERT(gPercentHeightContainerMap);
    return gPercentHeightContainerMap->contains(&descendant);
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
    ASSERT(gPercentHeightContainerMap);
    for (RenderObject* curr = parent.firstChild(); curr; curr = curr->nextInPreOrder(&parent)) {
        if (!curr->isBox())
            continue;
 
        RenderBox& box = toRenderBox(*curr);
        if (!hasPercentHeightDescendant(box))
            continue;

        removePercentHeightDescendant(box);
    }
}

LayoutUnit RenderBlock::textIndentOffset() const
{
    LayoutUnit cw = 0;
    if (style().textIndent().isPercent())
        cw = containingBlock()->availableLogicalWidth();
    return minimumValueForLength(style().textIndent(), cw);
}

LayoutUnit RenderBlock::logicalLeftOffsetForContent(RenderRegion* region) const
{
    LayoutUnit logicalLeftOffset = style().isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop();
    if (!region)
        return logicalLeftOffset;
    LayoutRect boxRect = borderBoxRectInRegion(region);
    return logicalLeftOffset + (isHorizontalWritingMode() ? boxRect.x() : boxRect.y());
}

LayoutUnit RenderBlock::logicalRightOffsetForContent(RenderRegion* region) const
{
    LayoutUnit logicalRightOffset = style().isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop();
    logicalRightOffset += availableLogicalWidth();
    if (!region)
        return logicalRightOffset;
    LayoutRect boxRect = borderBoxRectInRegion(region);
    return logicalRightOffset - (logicalWidth() - (isHorizontalWritingMode() ? boxRect.maxX() : boxRect.maxY()));
}

LayoutUnit RenderBlock::adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const
{
    LayoutUnit left = offsetFromFloats;

    if (applyTextIndent && style().isLeftToRightDirection())
        left += textIndentOffset();

    if (style().lineAlign() == LineAlignNone)
        return left;
    
    // Push in our left offset so that it is aligned with the character grid.
    LayoutState* layoutState = view().layoutState();
    if (!layoutState)
        return left;

    RenderBlock* lineGrid = layoutState->lineGrid();
    if (!lineGrid || lineGrid->style().writingMode() != style().writingMode())
        return left;

    // FIXME: Should letter-spacing apply? This is complicated since it doesn't apply at the edge?
    float maxCharWidth = lineGrid->style().font().primaryFont()->maxCharWidth();
    if (!maxCharWidth)
        return left;

    LayoutUnit lineGridOffset = lineGrid->isHorizontalWritingMode() ? layoutState->lineGridOffset().width(): layoutState->lineGridOffset().height();
    LayoutUnit layoutOffset = lineGrid->isHorizontalWritingMode() ? layoutState->layoutOffset().width() : layoutState->layoutOffset().height();
    
    // Push in to the nearest character width (truncated so that we pixel snap left).
    // FIXME: Should be patched when subpixel layout lands, since this calculation doesn't have to pixel snap
    // any more (https://bugs.webkit.org/show_bug.cgi?id=79946).
    // FIXME: This is wrong for RTL (https://bugs.webkit.org/show_bug.cgi?id=79945).
    // FIXME: This doesn't work with columns or regions (https://bugs.webkit.org/show_bug.cgi?id=79942).
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
    
    if (style().lineAlign() == LineAlignNone)
        return right;
    
    // Push in our right offset so that it is aligned with the character grid.
    LayoutState* layoutState = view().layoutState();
    if (!layoutState)
        return right;

    RenderBlock* lineGrid = layoutState->lineGrid();
    if (!lineGrid || lineGrid->style().writingMode() != style().writingMode())
        return right;

    // FIXME: Should letter-spacing apply? This is complicated since it doesn't apply at the edge?
    float maxCharWidth = lineGrid->style().font().primaryFont()->maxCharWidth();
    if (!maxCharWidth)
        return right;

    LayoutUnit lineGridOffset = lineGrid->isHorizontalWritingMode() ? layoutState->lineGridOffset().width(): layoutState->lineGridOffset().height();
    LayoutUnit layoutOffset = lineGrid->isHorizontalWritingMode() ? layoutState->layoutOffset().width() : layoutState->layoutOffset().height();
    
    // Push in to the nearest character width (truncated so that we pixel snap right).
    // FIXME: Should be patched when subpixel layout lands, since this calculation doesn't have to pixel snap
    // any more (https://bugs.webkit.org/show_bug.cgi?id=79946).
    // FIXME: This is wrong for RTL (https://bugs.webkit.org/show_bug.cgi?id=79945).
    // FIXME: This doesn't work with columns or regions (https://bugs.webkit.org/show_bug.cgi?id=79942).
    // FIXME: This doesn't work when the inline position of the object isn't set ahead of time.
    // FIXME: Dynamic changes to the font or to the inline position need to result in a deep relayout.
    // (https://bugs.webkit.org/show_bug.cgi?id=79944)
    float remainder = fmodf(fmodf(right + layoutOffset - lineGridOffset, maxCharWidth), maxCharWidth);
    right -= ceilf(remainder);
    return right;
}

bool RenderBlock::avoidsFloats() const
{
    // Floats can't intrude into our box if we have a non-auto column count or width.
    return RenderBox::avoidsFloats() || !style().hasAutoColumnCount() || !style().hasAutoColumnWidth();
}

bool RenderBlock::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!scrollsOverflow())
        return false;

    return layer()->hitTestOverflowControls(result, roundedIntPoint(locationInContainer - toLayoutSize(accumulatedOffset)));
}

Node* RenderBlock::nodeForHitTest() const
{
    // If we are in the margins of block elements that are part of a
    // continuation we're actually still inside the enclosing element
    // that was split. Use the appropriate inner node.
    if (isRenderView())
        return &document();
    return isAnonymousBlockContinuation() ? continuation()->element() : element();
}

bool RenderBlock::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    LayoutPoint adjustedLocation(accumulatedOffset + location());
    LayoutSize localOffset = toLayoutSize(adjustedLocation);

    if (!isRenderView()) {
        // Check if we need to do anything at all.
        LayoutRect overflowBox = visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedLocation);
        if (!locationInContainer.intersects(overflowBox))
            return false;
    }

    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) && isPointInOverflowControl(result, locationInContainer.point(), adjustedLocation)) {
        updateHitTestResult(result, locationInContainer.point() - localOffset);
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (!result.addNodeToRectBasedTestResult(nodeForHitTest(), request, locationInContainer))
           return true;
    }

    // If we have clipping, then we can't have any spillout.
    bool useOverflowClip = hasOverflowClip() && !hasSelfPaintingLayer();
    bool useClip = (hasControlClip() || useOverflowClip);
    bool checkChildren = !useClip || (hasControlClip() ? locationInContainer.intersects(controlClipRect(adjustedLocation)) : locationInContainer.intersects(overflowClipRect(adjustedLocation, locationInContainer.region(), IncludeOverlayScrollbarSize)));
    if (checkChildren) {
        // Hit test descendants first.
        LayoutSize scrolledOffset(localOffset - scrolledContentOffset());

        // Hit test contents if we don't have columns.
        if (!hasColumns()) {
            if (hitTestContents(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
                updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
                return true;
            }
            if (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, toLayoutPoint(scrolledOffset)))
                return true;
        } else if (hitTestColumns(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            return true;
        }
    }

    // Check if the point is outside radii.
    if (!isRenderView() && style().hasBorderRadius()) {
        LayoutRect borderRect = borderBoxRect();
        borderRect.moveBy(adjustedLocation);
        RoundedRect border = style().getRoundedBorderFor(borderRect, &view());
        if (!locationInContainer.intersects(border))
            return false;
    }

    // Now hit test our background
    if (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (visibleToHitTesting() && locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            if (!result.addNodeToRectBasedTestResult(nodeForHitTest(), request, locationInContainer, boundsRect))
                return true;
        }
    }

    return false;
}

class ColumnRectIterator {
    WTF_MAKE_NONCOPYABLE(ColumnRectIterator);
public:
    ColumnRectIterator(const RenderBlock& block)
        : m_block(block)
        , m_colInfo(block.columnInfo())
        , m_direction(m_block.style().isFlippedBlocksWritingMode() ? 1 : -1)
        , m_isHorizontal(block.isHorizontalWritingMode())
        , m_logicalLeft(block.logicalLeftOffsetForContent())
    {
        int colCount = m_colInfo->columnCount();
        m_colIndex = colCount - 1;
        
        m_currLogicalTopOffset = m_block.initialBlockOffsetForPainting();
        m_currLogicalTopOffset = colCount * m_block.blockDeltaForPaintingNextColumn();
        
        update();
    }

    void advance()
    {
        ASSERT(hasMore());
        m_colIndex--;
        update();
    }

    LayoutRect columnRect() const { return m_colRect; }
    bool hasMore() const { return m_colIndex >= 0; }

    void adjust(LayoutSize& offset) const
    {
        LayoutUnit currLogicalLeftOffset = (m_isHorizontal ? m_colRect.x() : m_colRect.y()) - m_logicalLeft;
        offset += m_isHorizontal ? LayoutSize(currLogicalLeftOffset, m_currLogicalTopOffset) : LayoutSize(m_currLogicalTopOffset, currLogicalLeftOffset);
    }

private:
    void update()
    {
        if (m_colIndex < 0)
            return;
        m_colRect = m_block.columnRectAt(const_cast<ColumnInfo*>(m_colInfo), m_colIndex);
        m_block.flipForWritingMode(m_colRect);
        m_currLogicalTopOffset -= m_block.blockDeltaForPaintingNextColumn();
    }

    const RenderBlock& m_block;
    const ColumnInfo* const m_colInfo;
    const int m_direction;
    const bool m_isHorizontal;
    const LayoutUnit m_logicalLeft;
    int m_colIndex;
    LayoutUnit m_currLogicalTopOffset;
    LayoutRect m_colRect;
};

bool RenderBlock::hitTestColumns(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    // We need to do multiple passes, breaking up our hit testing into strips.
    if (!hasColumns())
        return false;

    for (ColumnRectIterator it(*this); it.hasMore(); it.advance()) {
        LayoutRect hitRect = locationInContainer.boundingBox();
        LayoutRect colRect = it.columnRect();
        colRect.moveBy(accumulatedOffset);
        if (locationInContainer.intersects(colRect)) {
            // The point is inside this column.
            // Adjust accumulatedOffset to change where we hit test.
            LayoutSize offset;
            it.adjust(offset);
            LayoutPoint finalLocation = accumulatedOffset + offset;
            if (!result.isRectBasedTest() || colRect.contains(hitRect))
                return hitTestContents(request, result, locationInContainer, finalLocation, hitTestAction) || (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, finalLocation));

            hitTestContents(request, result, locationInContainer, finalLocation, hitTestAction);
        }
    }

    return false;
}

void RenderBlock::adjustForColumnRect(LayoutSize& offset, const LayoutPoint& locationInContainer) const
{
    for (ColumnRectIterator it(*this); it.hasMore(); it.advance()) {
        LayoutRect colRect = it.columnRect();
        if (colRect.contains(locationInContainer)) {
            it.adjust(offset);
            return;
        }
    }
}

bool RenderBlock::hitTestContents(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (childrenInline() && !isTable())
        return hitTestInlineChildren(request, result, locationInContainer, accumulatedOffset, hitTestAction);

    // Hit test our children.
    HitTestAction childHitTest = hitTestAction;
    if (hitTestAction == HitTestChildBlockBackgrounds)
        childHitTest = HitTestChildBlockBackground;
    for (auto child = lastChildBox(); child; child = child->previousSiblingBox()) {
        LayoutPoint childPoint = flipForWritingModeForChild(child, accumulatedOffset);
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
        return child.positionForPoint(pointInChildCoordinates);

    // Otherwise, first make sure that the editability of the parent and child agree.
    // If they don't agree, then we return a visible position just before or after the child
    RenderElement* ancestor = &parent;
    while (ancestor && !ancestor->nonPseudoElement())
        ancestor = ancestor->parent();

    // If we can't find an ancestor to check editability on, or editability is unchanged, we recur like normal
    if (isEditingBoundary(ancestor, child))
        return child.positionForPoint(pointInChildCoordinates);
    
#if PLATFORM(IOS)
    // On iOS we want to constrain VisiblePositions to the editable region closest to the input position, so
    // we will allow descent from non-edtiable to editable content.
    // FIXME: This constraining must be done at a higher level once we implement contentEditable. For now, if something
    // is editable, the whole document will be.
    if (childElement->isContentEditable() && !ancestor->element()->isContentEditable())
        return child.positionForPoint(pointInChildCoordinates);
#endif

    // Otherwise return before or after the child, depending on if the click was to the logical left or logical right of the child
    LayoutUnit childMiddle = parent.logicalWidthForChild(child) / 2;
    LayoutUnit logicalLeft = parent.isHorizontalWritingMode() ? pointInChildCoordinates.x() : pointInChildCoordinates.y();
    if (logicalLeft < childMiddle)
        return ancestor->createVisiblePosition(childElement->nodeIndex(), DOWNSTREAM);
    return ancestor->createVisiblePosition(childElement->nodeIndex() + 1, UPSTREAM);
}

VisiblePosition RenderBlock::positionForPointWithInlineChildren(const LayoutPoint&)
{
    ASSERT_NOT_REACHED();
    return VisiblePosition();
}

static inline bool isChildHitTestCandidate(const RenderBox& box)
{
    return box.height() && box.style().visibility() == VISIBLE && !box.isFloatingOrOutOfFlowPositioned();
}

// Valid candidates in a FlowThread must be rendered by the region.
static inline bool isChildHitTestCandidate(const RenderBox& box, RenderRegion* region, const LayoutPoint& point)
{
    if (!isChildHitTestCandidate(box))
        return false;
    if (!region)
        return true;
    const RenderBlock& block = box.isRenderBlock() ? toRenderBlock(box) : *box.containingBlock();
    return block.regionAtBlockOffset(point.y()) == region;
}

VisiblePosition RenderBlock::positionForPoint(const LayoutPoint& point)
{
    if (isTable())
        return RenderBox::positionForPoint(point);

    if (isReplaced()) {
        // FIXME: This seems wrong when the object's writing-mode doesn't match the line's writing-mode.
        LayoutUnit pointLogicalLeft = isHorizontalWritingMode() ? point.x() : point.y();
        LayoutUnit pointLogicalTop = isHorizontalWritingMode() ? point.y() : point.x();

        if (pointLogicalTop < 0 || (pointLogicalTop < logicalHeight() && pointLogicalLeft < 0))
            return createVisiblePosition(caretMinOffset(), DOWNSTREAM);
        if (pointLogicalTop >= logicalHeight() || (pointLogicalTop >= 0 && pointLogicalLeft >= logicalWidth()))
            return createVisiblePosition(caretMaxOffset(), DOWNSTREAM);
    } 

    LayoutPoint pointInContents = point;
    offsetForContents(pointInContents);
    LayoutPoint pointInLogicalContents(pointInContents);
    if (!isHorizontalWritingMode())
        pointInLogicalContents = pointInLogicalContents.transposedPoint();

    if (childrenInline())
        return positionForPointWithInlineChildren(pointInLogicalContents);

    RenderRegion* region = regionAtBlockOffset(pointInLogicalContents.y());
    RenderBox* lastCandidateBox = lastChildBox();
    while (lastCandidateBox && !isChildHitTestCandidate(*lastCandidateBox, region, pointInLogicalContents))
        lastCandidateBox = lastCandidateBox->previousSiblingBox();

    bool blocksAreFlipped = style().isFlippedBlocksWritingMode();
    if (lastCandidateBox) {
        if (pointInLogicalContents.y() > logicalTopForChild(*lastCandidateBox)
            || (!blocksAreFlipped && pointInLogicalContents.y() == logicalTopForChild(*lastCandidateBox)))
            return positionForPointRespectingEditingBoundaries(*this, *lastCandidateBox, pointInContents);

        for (auto childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
            if (!isChildHitTestCandidate(*childBox, region, pointInLogicalContents))
                continue;
            LayoutUnit childLogicalBottom = logicalTopForChild(*childBox) + logicalHeightForChild(*childBox);
            // We hit child if our click is above the bottom of its padding box (like IE6/7 and FF3).
            if (isChildHitTestCandidate(*childBox, region, pointInLogicalContents) && (pointInLogicalContents.y() < childLogicalBottom
                || (blocksAreFlipped && pointInLogicalContents.y() == childLogicalBottom)))
                return positionForPointRespectingEditingBoundaries(*this, *childBox, pointInContents);
        }
    }

    // We only get here if there are no hit test candidate children below the click.
    return RenderBox::positionForPoint(point);
}

void RenderBlock::offsetForContents(LayoutPoint& offset) const
{
    offset = flipForWritingMode(offset);
    offset += scrolledContentOffset();

    if (hasColumns())
        adjustPointToColumnContents(offset);

    offset = flipForWritingMode(offset);
}

LayoutUnit RenderBlock::availableLogicalWidth() const
{
    // If we have multiple columns, then the available logical width is reduced to our column width.
    if (hasColumns())
        return computedColumnWidth();
    return RenderBox::availableLogicalWidth();
}

int RenderBlock::columnGap() const
{
    if (style().hasNormalColumnGap())
        return style().fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return static_cast<int>(style().columnGap());
}

void RenderBlock::computeColumnCountAndWidth()
{   
    // Calculate our column width and column count.
    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned desiredColumnCount = 1;
    LayoutUnit desiredColumnWidth = contentLogicalWidth();
    
    // For now, we don't support multi-column layouts when printing, since we have to do a lot of work for proper pagination.
    if (document().paginated() || (style().hasAutoColumnCount() && style().hasAutoColumnWidth()) || !style().hasInlineColumnAxis()) {
        setComputedColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
        return;
    }
        
    LayoutUnit availWidth = desiredColumnWidth;
    LayoutUnit colGap = columnGap();
    LayoutUnit colWidth = std::max<LayoutUnit>(LayoutUnit::fromPixel(1), LayoutUnit(style().columnWidth()));
    int colCount = std::max<int>(1, style().columnCount());

    if (style().hasAutoColumnWidth() && !style().hasAutoColumnCount()) {
        desiredColumnCount = colCount;
        desiredColumnWidth = std::max<LayoutUnit>(0, (availWidth - ((desiredColumnCount - 1) * colGap)) / desiredColumnCount);
    } else if (!style().hasAutoColumnWidth() && style().hasAutoColumnCount()) {
        desiredColumnCount = std::max<LayoutUnit>(1, (availWidth + colGap) / (colWidth + colGap));
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    } else {
        desiredColumnCount = std::max<LayoutUnit>(std::min<LayoutUnit>(colCount, (availWidth + colGap) / (colWidth + colGap)), 1);
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    }
    setComputedColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
}

bool RenderBlock::requiresColumns(int desiredColumnCount) const
{
    // If overflow-y is set to paged-x or paged-y on the body or html element, we'll handle the paginating
    // in the RenderView instead.
    bool isPaginated = (style().overflowY() == OPAGEDX || style().overflowY() == OPAGEDY) && !(isRoot() || isBody());

    return firstChild()
        && (desiredColumnCount != 1 || !style().hasAutoColumnWidth() || !style().hasInlineColumnAxis() || isPaginated)
        && !firstChild()->isAnonymousColumnsBlock()
        && !firstChild()->isAnonymousColumnSpanBlock();
}

void RenderBlock::setComputedColumnCountAndWidth(int count, LayoutUnit width)
{
    bool destroyColumns = !requiresColumns(count);
    if (destroyColumns) {
        if (hasColumns()) {
            gColumnInfoMap->take(this);
            setHasColumns(false);
        }
    } else {
        ColumnInfo* info;
        if (hasColumns())
            info = gColumnInfoMap->get(this);
        else {
            if (!gColumnInfoMap)
                gColumnInfoMap = new ColumnInfoMap;
            info = new ColumnInfo;
            gColumnInfoMap->add(this, std::unique_ptr<ColumnInfo>(info));
            setHasColumns(true);
        }
        info->setDesiredColumnCount(count);
        info->setDesiredColumnWidth(width);
        info->setProgressionIsInline(style().hasInlineColumnAxis());
        info->setProgressionIsReversed(style().columnProgression() == ReverseColumnProgression);
    }
}

void RenderBlock::updateColumnProgressionFromStyle(RenderStyle* style)
{
    if (!hasColumns())
        return;

    ColumnInfo* info = gColumnInfoMap->get(this);

    bool needsLayout = false;
    bool oldProgressionIsInline = info->progressionIsInline();
    bool newProgressionIsInline = style->hasInlineColumnAxis();
    if (oldProgressionIsInline != newProgressionIsInline) {
        info->setProgressionIsInline(newProgressionIsInline);
        needsLayout = true;
    }

    bool oldProgressionIsReversed = info->progressionIsReversed();
    bool newProgressionIsReversed = style->columnProgression() == ReverseColumnProgression;
    if (oldProgressionIsReversed != newProgressionIsReversed) {
        info->setProgressionIsReversed(newProgressionIsReversed);
        needsLayout = true;
    }

    if (needsLayout)
        setNeedsLayoutAndPrefWidthsRecalc();
}

LayoutUnit RenderBlock::computedColumnWidth() const
{
    if (!hasColumns())
        return contentLogicalWidth();
    return gColumnInfoMap->get(this)->desiredColumnWidth();
}

unsigned RenderBlock::computedColumnCount() const
{
    if (!hasColumns())
        return 1;
    return gColumnInfoMap->get(this)->desiredColumnCount();
}

ColumnInfo* RenderBlock::columnInfo() const
{
    if (!hasColumns())
        return 0;
    return gColumnInfoMap->get(this);    
}

unsigned RenderBlock::columnCount(ColumnInfo* colInfo) const
{
    ASSERT(hasColumns());
    ASSERT(gColumnInfoMap->get(this) == colInfo);
    return colInfo->columnCount();
}

LayoutRect RenderBlock::columnRectAt(ColumnInfo* colInfo, unsigned index) const
{
    ASSERT(hasColumns() && gColumnInfoMap->get(this) == colInfo);

    // Compute the appropriate rect based off our information.
    LayoutUnit colLogicalWidth = colInfo->desiredColumnWidth();
    LayoutUnit colLogicalHeight = colInfo->columnHeight();
    LayoutUnit colLogicalTop = borderAndPaddingBefore();
    LayoutUnit colLogicalLeft = logicalLeftOffsetForContent();
    LayoutUnit colGap = columnGap();
    if (colInfo->progressionIsInline()) {
        if (style().isLeftToRightDirection() ^ colInfo->progressionIsReversed())
            colLogicalLeft += index * (colLogicalWidth + colGap);
        else
            colLogicalLeft += contentLogicalWidth() - colLogicalWidth - index * (colLogicalWidth + colGap);
    } else {
        if (!colInfo->progressionIsReversed())
            colLogicalTop += index * (colLogicalHeight + colGap);
        else
            colLogicalTop += contentLogicalHeight() - colLogicalHeight - index * (colLogicalHeight + colGap);
    }

    if (isHorizontalWritingMode())
        return LayoutRect(colLogicalLeft, colLogicalTop, colLogicalWidth, colLogicalHeight);
    return LayoutRect(colLogicalTop, colLogicalLeft, colLogicalHeight, colLogicalWidth);
}

void RenderBlock::adjustPointToColumnContents(LayoutPoint& point) const
{
    // Just bail if we have no columns.
    if (!hasColumns())
        return;
    
    ColumnInfo* colInfo = columnInfo();
    if (!columnCount(colInfo))
        return;

    // Determine which columns we intersect.
    LayoutUnit colGap = columnGap();
    LayoutUnit halfColGap = colGap / 2;
    LayoutPoint columnPoint(columnRectAt(colInfo, 0).location());
    LayoutUnit logicalOffset = 0;
    for (unsigned i = 0; i < colInfo->columnCount(); i++) {
        // Add in half the column gap to the left and right of the rect.
        LayoutRect colRect = columnRectAt(colInfo, i);
        flipForWritingMode(colRect);
        if (isHorizontalWritingMode() == colInfo->progressionIsInline()) {
            LayoutRect gapAndColumnRect(colRect.x() - halfColGap, colRect.y(), colRect.width() + colGap, colRect.height());
            if (point.x() >= gapAndColumnRect.x() && point.x() < gapAndColumnRect.maxX()) {
                if (colInfo->progressionIsInline()) {
                    // FIXME: The clamping that follows is not completely right for right-to-left
                    // content.
                    // Clamp everything above the column to its top left.
                    if (point.y() < gapAndColumnRect.y())
                        point = gapAndColumnRect.location();
                    // Clamp everything below the column to the next column's top left. If there is
                    // no next column, this still maps to just after this column.
                    else if (point.y() >= gapAndColumnRect.maxY()) {
                        point = gapAndColumnRect.location();
                        point.move(0, gapAndColumnRect.height());
                    }
                } else {
                    if (point.x() < colRect.x())
                        point.setX(colRect.x());
                    else if (point.x() >= colRect.maxX())
                        point.setX(colRect.maxX() - 1);
                }

                // We're inside the column.  Translate the x and y into our column coordinate space.
                if (colInfo->progressionIsInline())
                    point.move(columnPoint.x() - colRect.x(), (!style().isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset));
                else
                    point.move((!style().isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset) - colRect.x() + borderLeft() + paddingLeft(), 0);
                return;
            }

            // Move to the next position.
            logicalOffset += colInfo->progressionIsInline() ? colRect.height() : colRect.width();
        } else {
            LayoutRect gapAndColumnRect(colRect.x(), colRect.y() - halfColGap, colRect.width(), colRect.height() + colGap);
            if (point.y() >= gapAndColumnRect.y() && point.y() < gapAndColumnRect.maxY()) {
                if (colInfo->progressionIsInline()) {
                    // FIXME: The clamping that follows is not completely right for right-to-left
                    // content.
                    // Clamp everything above the column to its top left.
                    if (point.x() < gapAndColumnRect.x())
                        point = gapAndColumnRect.location();
                    // Clamp everything below the column to the next column's top left. If there is
                    // no next column, this still maps to just after this column.
                    else if (point.x() >= gapAndColumnRect.maxX()) {
                        point = gapAndColumnRect.location();
                        point.move(gapAndColumnRect.width(), 0);
                    }
                } else {
                    if (point.y() < colRect.y())
                        point.setY(colRect.y());
                    else if (point.y() >= colRect.maxY())
                        point.setY(colRect.maxY() - 1);
                }

                // We're inside the column.  Translate the x and y into our column coordinate space.
                if (colInfo->progressionIsInline())
                    point.move((!style().isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset), columnPoint.y() - colRect.y());
                else
                    point.move(0, (!style().isFlippedBlocksWritingMode() ? logicalOffset : -logicalOffset) - colRect.y() + borderTop() + paddingTop());
                return;
            }

            // Move to the next position.
            logicalOffset += colInfo->progressionIsInline() ? colRect.width() : colRect.height();
        }
    }
}

void RenderBlock::adjustRectForColumns(LayoutRect& r) const
{
    // Just bail if we have no columns.
    if (!hasColumns())
        return;
    
    ColumnInfo* colInfo = columnInfo();
    
    // Determine which columns we intersect.
    unsigned colCount = columnCount(colInfo);
    if (!colCount)
        return;

    // Begin with a result rect that is empty.
    LayoutRect result;

    bool isHorizontal = isHorizontalWritingMode();
    LayoutUnit beforeBorderPadding = borderAndPaddingBefore();
    LayoutUnit colHeight = colInfo->columnHeight();
    if (!colHeight)
        return;

    LayoutUnit startOffset = std::max(isHorizontal ? r.y() : r.x(), beforeBorderPadding);
    LayoutUnit endOffset = std::max(std::min<LayoutUnit>(isHorizontal ? r.maxY() : r.maxX(), beforeBorderPadding + colCount * colHeight), beforeBorderPadding);

    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned startColumn = (startOffset - beforeBorderPadding) / colHeight;
    unsigned endColumn = (endOffset - beforeBorderPadding) / colHeight;

    if (startColumn == endColumn) {
        // The rect is fully contained within one column. Adjust for our offsets
        // and repaint only that portion.
        LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent();
        LayoutRect colRect = columnRectAt(colInfo, startColumn);
        LayoutRect repaintRect = r;

        if (colInfo->progressionIsInline()) {
            if (isHorizontal)
                repaintRect.move(colRect.x() - logicalLeftOffset, - static_cast<int>(startColumn) * colHeight);
            else
                repaintRect.move(- static_cast<int>(startColumn) * colHeight, colRect.y() - logicalLeftOffset);
        } else {
            if (isHorizontal)
                repaintRect.move(0, colRect.y() - startColumn * colHeight - beforeBorderPadding);
            else
                repaintRect.move(colRect.x() - startColumn * colHeight - beforeBorderPadding, 0);
        }
        repaintRect.intersect(colRect);
        result.unite(repaintRect);
    } else {
        // We span multiple columns. We can just unite the start and end column to get the final
        // repaint rect.
        result.unite(columnRectAt(colInfo, startColumn));
        result.unite(columnRectAt(colInfo, endColumn));
    }

    r = result;
}

LayoutPoint RenderBlock::flipForWritingModeIncludingColumns(const LayoutPoint& point) const
{
    ASSERT(hasColumns());
    if (!hasColumns() || !style().isFlippedBlocksWritingMode())
        return point;
    ColumnInfo* colInfo = columnInfo();
    LayoutUnit columnLogicalHeight = colInfo->columnHeight();
    LayoutUnit expandedLogicalHeight = borderAndPaddingBefore() + columnCount(colInfo) * columnLogicalHeight + borderAndPaddingAfter() + scrollbarLogicalHeight();
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), expandedLogicalHeight - point.y());
    return LayoutPoint(expandedLogicalHeight - point.x(), point.y());
}

void RenderBlock::adjustStartEdgeForWritingModeIncludingColumns(LayoutRect& rect) const
{
    ASSERT(hasColumns());
    if (!hasColumns() || !style().isFlippedBlocksWritingMode())
        return;
    
    ColumnInfo* colInfo = columnInfo();
    LayoutUnit columnLogicalHeight = colInfo->columnHeight();
    LayoutUnit expandedLogicalHeight = borderAndPaddingBefore() + columnCount(colInfo) * columnLogicalHeight + borderAndPaddingAfter() + scrollbarLogicalHeight();
    
    if (isHorizontalWritingMode())
        rect.setY(expandedLogicalHeight - rect.maxY());
    else
        rect.setX(expandedLogicalHeight - rect.maxX());
}

void RenderBlock::adjustForColumns(LayoutSize& offset, const LayoutPoint& point) const
{
    if (!hasColumns())
        return;

    ColumnInfo* colInfo = columnInfo();

    LayoutUnit logicalLeft = logicalLeftOffsetForContent();
    unsigned colCount = columnCount(colInfo);
    LayoutUnit colLogicalWidth = colInfo->desiredColumnWidth();
    LayoutUnit colLogicalHeight = colInfo->columnHeight();

    for (unsigned i = 0; i < colCount; ++i) {
        // Compute the edges for a given column in the block progression direction.
        LayoutRect sliceRect = LayoutRect(logicalLeft, borderAndPaddingBefore() + i * colLogicalHeight, colLogicalWidth, colLogicalHeight);
        if (!isHorizontalWritingMode())
            sliceRect = sliceRect.transposedRect();
        
        LayoutUnit logicalOffset = i * colLogicalHeight;

        // Now we're in the same coordinate space as the point.  See if it is inside the rectangle.
        if (isHorizontalWritingMode()) {
            if (point.y() >= sliceRect.y() && point.y() < sliceRect.maxY()) {
                if (colInfo->progressionIsInline())
                    offset.expand(columnRectAt(colInfo, i).x() - logicalLeft, -logicalOffset);
                else
                    offset.expand(0, columnRectAt(colInfo, i).y() - logicalOffset - borderAndPaddingBefore());
                return;
            }
        } else {
            if (point.x() >= sliceRect.x() && point.x() < sliceRect.maxX()) {
                if (colInfo->progressionIsInline())
                    offset.expand(-logicalOffset, columnRectAt(colInfo, i).y() - logicalLeft);
                else
                    offset.expand(columnRectAt(colInfo, i).x() - logicalOffset - borderAndPaddingBefore(), 0);
                return;
            }
        }
    }
}

void RenderBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (childrenInline()) {
        // FIXME: Remove this const_cast.
        const_cast<RenderBlock*>(this)->computeInlinePreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    } else
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    adjustIntrinsicLogicalWidthsForColumns(minLogicalWidth, maxLogicalWidth);

    if (!style().autoWrap() && childrenInline()) {
        // A horizontal marquee with inline children has no minimum width.
        if (layer() && layer()->marquee() && layer()->marquee()->isHorizontal())
            minLogicalWidth = 0;
    }

    if (isTableCell()) {
        Length tableCellWidth = toRenderTableCell(this)->styleOrColLogicalWidth();
        if (tableCellWidth.isFixed() && tableCellWidth.value() > 0)
            maxLogicalWidth = std::max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(tableCellWidth.value()));
    }

    int scrollbarWidth = instrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void RenderBlock::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    updateFirstLetter();

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    const RenderStyle& styleToUse = style();
    if (!isTableCell() && styleToUse.logicalWidth().isFixed() && styleToUse.logicalWidth().value() >= 0
        && !(isDeprecatedFlexItem() && !styleToUse.logicalWidth().intValue()))
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalWidth().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
    
    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
    }
    
    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
    }
    
    // Table layout uses integers, ceil the preferred widths to ensure that they can contain the contents.
    if (isTableCell()) {
        m_minPreferredLogicalWidth = m_minPreferredLogicalWidth.ceil();
        m_maxPreferredLogicalWidth = m_maxPreferredLogicalWidth.ceil();
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    setPreferredLogicalWidthsDirty(false);
}

void RenderBlock::adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // FIXME: Move this code to RenderBlockFlow.

    if (!style().hasAutoColumnCount() || !style().hasAutoColumnWidth()) {
        // The min/max intrinsic widths calculated really tell how much space elements need when
        // laid out inside the columns. In order to eventually end up with the desired column width,
        // we need to convert them to values pertaining to the multicol container.
        int columnCount = style().hasAutoColumnCount() ? 1 : style().columnCount();
        LayoutUnit columnWidth;
        LayoutUnit gapExtra = (columnCount - 1) * columnGap();
        if (style().hasAutoColumnWidth())
            minLogicalWidth = minLogicalWidth * columnCount + gapExtra;
        else {
            columnWidth = style().columnWidth();
            minLogicalWidth = std::min(minLogicalWidth, columnWidth);
        }
        // FIXME: If column-count is auto here, we should resolve it to calculate the maximum
        // intrinsic width, instead of pretending that it's 1. The only way to do that is by
        // performing a layout pass, but this is not an appropriate time or place for layout. The
        // good news is that if height is unconstrained and there are no explicit breaks, the
        // resolved column-count really should be 1.
        maxLogicalWidth = std::max(maxLogicalWidth, columnWidth) * columnCount + gapExtra;
    }
}

struct InlineMinMaxIterator {
/* InlineMinMaxIterator is a class that will iterate over all render objects that contribute to
   inline min/max width calculations.  Note the following about the way it walks:
   (1) Positioned content is skipped (since it does not contribute to min/max width of a block)
   (2) We do not drill into the children of floats or replaced elements, since you can't break
       in the middle of such an element.
   (3) Inline flows (e.g., <a>, <span>, <i>) are walked twice, since each side can have
       distinct borders/margin/padding that contribute to the min/max width.
*/
    RenderObject* parent;
    RenderObject* current;
    bool endOfInline;

    InlineMinMaxIterator(RenderObject* p, bool end = false)
        :parent(p), current(p), endOfInline(end) {}

    RenderObject* next();
};

RenderObject* InlineMinMaxIterator::next()
{
    RenderObject* result = 0;
    bool oldEndOfInline = endOfInline;
    endOfInline = false;
    while (current || current == parent) {
        if (!oldEndOfInline &&
            (current == parent ||
             (!current->isFloating() && !current->isReplaced() && !current->isOutOfFlowPositioned())))
            result = current->firstChildSlow();
        if (!result) {
            // We hit the end of our inline. (It was empty, e.g., <span></span>.)
            if (!oldEndOfInline && current->isRenderInline()) {
                result = current;
                endOfInline = true;
                break;
            }

            while (current && current != parent) {
                result = current->nextSibling();
                if (result) break;
                current = current->parent();
                if (current && current != parent && current->isRenderInline()) {
                    result = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!result)
            break;

        if (!result->isOutOfFlowPositioned() && (result->isTextOrLineBreak() || result->isFloating() || result->isReplaced() || result->isRenderInline()))
             break;
        
        current = result;
        result = 0;
    }

    // Update our position.
    current = result;
    return current;
}

static LayoutUnit getBPMWidth(LayoutUnit childValue, Length cssUnit)
{
    if (cssUnit.type() != Auto)
        return (cssUnit.isFixed() ? LayoutUnit(cssUnit.value()) : childValue);
    return 0;
}

static LayoutUnit getBorderPaddingMargin(const RenderBoxModelObject* child, bool endOfInline)
{
    const RenderStyle& childStyle = child->style();
    if (endOfInline)
        return getBPMWidth(child->marginEnd(), childStyle.marginEnd()) +
               getBPMWidth(child->paddingEnd(), childStyle.paddingEnd()) +
               child->borderEnd();
    return getBPMWidth(child->marginStart(), childStyle.marginStart()) +
               getBPMWidth(child->paddingStart(), childStyle.paddingStart()) +
               child->borderStart();
}

static inline void stripTrailingSpace(float& inlineMax, float& inlineMin,
                                      RenderObject* trailingSpaceChild)
{
    if (trailingSpaceChild && trailingSpaceChild->isText()) {
        // Collapse away the trailing space at the end of a block.
        RenderText* t = toRenderText(trailingSpaceChild);
        const UChar space = ' ';
        const Font& font = t->style().font(); // FIXME: This ignores first-line.
        float spaceWidth = font.width(RenderBlock::constructTextRun(t, font, &space, 1, t->style()));
        inlineMax -= spaceWidth + font.wordSpacing();
        if (inlineMin > inlineMax)
            inlineMin = inlineMax;
    }
}

static inline void updatePreferredWidth(LayoutUnit& preferredWidth, float& result)
{
    LayoutUnit snappedResult = ceiledLayoutUnit(result);
    preferredWidth = std::max(snappedResult, preferredWidth);
}

// With sub-pixel enabled: When converting between floating point and LayoutUnits
// we risk losing precision with each conversion. When this occurs while
// accumulating our preferred widths, we can wind up with a line width that's
// larger than our maxPreferredWidth due to pure float accumulation.
//
// With sub-pixel disabled: values from Lengths or the render tree aren't subject
// to the same loss of precision, as they're always truncated and stored as
// integers. We mirror that behavior here to prevent over-allocating our preferred
// width.
static inline LayoutUnit adjustFloatForSubPixelLayout(float value)
{
#if ENABLE(SUBPIXEL_LAYOUT)
    return ceiledLayoutUnit(value);
#else
    return static_cast<int>(value);
#endif
}


void RenderBlock::computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth)
{
    float inlineMax = 0;
    float inlineMin = 0;

    const RenderStyle& styleToUse = style();
    RenderBlock* containingBlock = this->containingBlock();
    LayoutUnit cw = containingBlock ? containingBlock->contentLogicalWidth() : LayoutUnit();

    // If we are at the start of a line, we want to ignore all white-space.
    // Also strip spaces if we previously had text that ended in a trailing space.
    bool stripFrontSpaces = true;
    RenderObject* trailingSpaceChild = 0;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific cirucumstances (in order to match common WinIE renderings). 
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.) 
    bool allowImagesToBreak = !document().inQuirksMode() || !isTableCell() || !styleToUse.logicalWidth().isIntrinsicOrAuto();

    bool autoWrap, oldAutoWrap;
    autoWrap = oldAutoWrap = styleToUse.autoWrap();

    InlineMinMaxIterator childIterator(this);

    // Only gets added to the max preffered width once.
    bool addedTextIndent = false;
    // Signals the text indent was more negative than the min preferred width
    bool hasRemainingNegativeTextIndent = false;

    LayoutUnit textIndent = minimumValueForLength(styleToUse.textIndent(), cw);
    RenderObject* prevFloat = 0;
    bool isPrevChildInlineFlow = false;
    bool shouldBreakLineAfterText = false;
    while (RenderObject* child = childIterator.next()) {
        autoWrap = child->isReplaced() ? child->parent()->style().autoWrap() : 
            child->style().autoWrap();

        if (!child->isBR()) {
            // Step One: determine whether or not we need to go ahead and
            // terminate our current line.  Each discrete chunk can become
            // the new min-width, if it is the widest chunk seen so far, and
            // it can also become the max-width.

            // Children fall into three categories:
            // (1) An inline flow object.  These objects always have a min/max of 0,
            // and are included in the iteration solely so that their margins can
            // be added in.
            //
            // (2) An inline non-text non-flow object, e.g., an inline replaced element.
            // These objects can always be on a line by themselves, so in this situation
            // we need to go ahead and break the current line, and then add in our own
            // margins and min/max width on its own line, and then terminate the line.
            //
            // (3) A text object.  Text runs can have breakable characters at the start,
            // the middle or the end.  They may also lose whitespace off the front if
            // we're already ignoring whitespace.  In order to compute accurate min-width
            // information, we need three pieces of information.
            // (a) the min-width of the first non-breakable run.  Should be 0 if the text string
            // starts with whitespace.
            // (b) the min-width of the last non-breakable run. Should be 0 if the text string
            // ends with whitespace.
            // (c) the min/max width of the string (trimmed for whitespace).
            //
            // If the text string starts with whitespace, then we need to go ahead and
            // terminate our current line (unless we're already in a whitespace stripping
            // mode.
            //
            // If the text string has a breakable character in the middle, but didn't start
            // with whitespace, then we add the width of the first non-breakable run and
            // then end the current line.  We then need to use the intermediate min/max width
            // values (if any of them are larger than our current min/max).  We then look at
            // the width of the last non-breakable run and use that to start a new line
            // (unless we end in whitespace).
            const RenderStyle& childStyle = child->style();
            float childMin = 0;
            float childMax = 0;

            if (!child->isText()) {
                if (child->isLineBreakOpportunity()) {
                    updatePreferredWidth(minLogicalWidth, inlineMin);
                    inlineMin = 0;
                    continue;
                }
                // Case (1) and (2).  Inline replaced and inline flow elements.
                if (child->isRenderInline()) {
                    // Add in padding/border/margin from the appropriate side of
                    // the element.
                    float bpm = getBorderPaddingMargin(toRenderInline(child), childIterator.endOfInline);
                    childMin += bpm;
                    childMax += bpm;

                    inlineMin += childMin;
                    inlineMax += childMax;
                    
                    child->setPreferredLogicalWidthsDirty(false);
                } else {
                    // Inline replaced elts add in their margins to their min/max values.
                    LayoutUnit margins = 0;
                    Length startMargin = childStyle.marginStart();
                    Length endMargin = childStyle.marginEnd();
                    if (startMargin.isFixed())
                        margins += adjustFloatForSubPixelLayout(startMargin.value());
                    if (endMargin.isFixed())
                        margins += adjustFloatForSubPixelLayout(endMargin.value());
                    childMin += margins.ceilToFloat();
                    childMax += margins.ceilToFloat();
                }
            }

            if (!child->isRenderInline() && !child->isText()) {
                // Case (2). Inline replaced elements and floats.
                // Go ahead and terminate the current line as far as
                // minwidth is concerned.
                childMin += child->minPreferredLogicalWidth().ceilToFloat();
                childMax += child->maxPreferredLogicalWidth().ceilToFloat();

                bool clearPreviousFloat;
                if (child->isFloating()) {
                    clearPreviousFloat = (prevFloat
                        && ((prevFloat->style().floating() == LeftFloat && (childStyle.clear() & CLEFT))
                            || (prevFloat->style().floating() == RightFloat && (childStyle.clear() & CRIGHT))));
                    prevFloat = child;
                } else
                    clearPreviousFloat = false;

                bool canBreakReplacedElement = !child->isImage() || allowImagesToBreak;
                if ((canBreakReplacedElement && (autoWrap || oldAutoWrap) && (!isPrevChildInlineFlow || shouldBreakLineAfterText)) || clearPreviousFloat) {
                    updatePreferredWidth(minLogicalWidth, inlineMin);
                    inlineMin = 0;
                }

                // If we're supposed to clear the previous float, then terminate maxwidth as well.
                if (clearPreviousFloat) {
                    updatePreferredWidth(maxLogicalWidth, inlineMax);
                    inlineMax = 0;
                }

                // Add in text-indent.  This is added in only once.
                if (!addedTextIndent && !child->isFloating()) {
                    LayoutUnit ceiledIndent = textIndent.ceilToFloat();
                    childMin += ceiledIndent;
                    childMax += ceiledIndent;

                    if (childMin < 0)
                        textIndent = adjustFloatForSubPixelLayout(childMin);
                    else
                        addedTextIndent = true;
                }

                // Add our width to the max.
                inlineMax += std::max<float>(0, childMax);

                if (!autoWrap || !canBreakReplacedElement || (isPrevChildInlineFlow && !shouldBreakLineAfterText)) {
                    if (child->isFloating())
                        updatePreferredWidth(minLogicalWidth, childMin);
                    else
                        inlineMin += childMin;
                } else {
                    // Now check our line.
                    updatePreferredWidth(minLogicalWidth, childMin);

                    // Now start a new line.
                    inlineMin = 0;
                }

                if (autoWrap && canBreakReplacedElement && isPrevChildInlineFlow) {
                    updatePreferredWidth(minLogicalWidth, inlineMin);
                    inlineMin = 0;
                }

                // We are no longer stripping whitespace at the start of
                // a line.
                if (!child->isFloating()) {
                    stripFrontSpaces = false;
                    trailingSpaceChild = 0;
                }
            } else if (child->isText()) {
                // Case (3). Text.
                RenderText* t = toRenderText(child);

                if (t->style().hasTextCombine() && t->isCombineText())
                    toRenderCombineText(*t).combineText();

                // Determine if we have a breakable character.  Pass in
                // whether or not we should ignore any spaces at the front
                // of the string.  If those are going to be stripped out,
                // then they shouldn't be considered in the breakable char
                // check.
                bool hasBreakableChar, hasBreak;
                float beginMin, endMin;
                bool beginWS, endWS;
                float beginMax, endMax;
                t->trimmedPrefWidths(inlineMax, beginMin, beginWS, endMin, endWS,
                                     hasBreakableChar, hasBreak, beginMax, endMax,
                                     childMin, childMax, stripFrontSpaces);

                // This text object will not be rendered, but it may still provide a breaking opportunity.
                if (!hasBreak && childMax == 0) {
                    if (autoWrap && (beginWS || endWS)) {
                        updatePreferredWidth(minLogicalWidth, inlineMin);
                        inlineMin = 0;
                    }
                    continue;
                }
                
                if (stripFrontSpaces)
                    trailingSpaceChild = child;
                else
                    trailingSpaceChild = 0;

                // Add in text-indent.  This is added in only once.
                float ti = 0;
                if (!addedTextIndent || hasRemainingNegativeTextIndent) {
                    ti = textIndent.ceilToFloat();
                    childMin += ti;
                    beginMin += ti;
                    
                    // It the text indent negative and larger than the child minimum, we re-use the remainder
                    // in future minimum calculations, but using the negative value again on the maximum
                    // will lead to under-counting the max pref width.
                    if (!addedTextIndent) {
                        childMax += ti;
                        beginMax += ti;
                        addedTextIndent = true;
                    }
                    
                    if (childMin < 0) {
                        textIndent = childMin;
                        hasRemainingNegativeTextIndent = true;
                    }
                }
                
                // If we have no breakable characters at all,
                // then this is the easy case. We add ourselves to the current
                // min and max and continue.
                if (!hasBreakableChar) {
                    inlineMin += childMin;
                } else {
                    // We have a breakable character.  Now we need to know if
                    // we start and end with whitespace.
                    if (beginWS)
                        // Go ahead and end the current line.
                        updatePreferredWidth(minLogicalWidth, inlineMin);
                    else {
                        inlineMin += beginMin;
                        updatePreferredWidth(minLogicalWidth, inlineMin);
                        childMin -= ti;
                    }

                    inlineMin = childMin;

                    if (endWS) {
                        // We end in whitespace, which means we can go ahead
                        // and end our current line.
                        updatePreferredWidth(minLogicalWidth, inlineMin);
                        inlineMin = 0;
                        shouldBreakLineAfterText = false;
                    } else {
                        updatePreferredWidth(minLogicalWidth, inlineMin);
                        inlineMin = endMin;
                        shouldBreakLineAfterText = true;
                    }
                }

                if (hasBreak) {
                    inlineMax += beginMax;
                    updatePreferredWidth(maxLogicalWidth, inlineMax);
                    updatePreferredWidth(maxLogicalWidth, childMax);
                    inlineMax = endMax;
                    addedTextIndent = true;
                } else
                    inlineMax += std::max<float>(0, childMax);
            }

            // Ignore spaces after a list marker.
            if (child->isListMarker())
                stripFrontSpaces = true;
        } else {
            updatePreferredWidth(minLogicalWidth, inlineMin);
            updatePreferredWidth(maxLogicalWidth, inlineMax);
            inlineMin = inlineMax = 0;
            stripFrontSpaces = true;
            trailingSpaceChild = 0;
            addedTextIndent = true;
        }

        if (!child->isText() && child->isRenderInline())
            isPrevChildInlineFlow = true;
        else
            isPrevChildInlineFlow = false;

        oldAutoWrap = autoWrap;
    }

    if (styleToUse.collapseWhiteSpace())
        stripTrailingSpace(inlineMax, inlineMin, trailingSpaceChild);

    updatePreferredWidth(minLogicalWidth, inlineMin);
    updatePreferredWidth(maxLogicalWidth, inlineMax);
}

void RenderBlock::computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const RenderStyle& styleToUse = style();
    bool nowrap = styleToUse.whiteSpace() == NOWRAP;

    RenderObject* child = firstChild();
    RenderBlock* containingBlock = this->containingBlock();
    LayoutUnit floatLeftWidth = 0, floatRightWidth = 0;
    while (child) {
        // Positioned children don't affect the min/max width
        if (child->isOutOfFlowPositioned()) {
            child = child->nextSibling();
            continue;
        }

        const RenderStyle& childStyle = child->style();
        if (child->isFloating() || (child->isBox() && toRenderBox(child)->avoidsFloats())) {
            LayoutUnit floatTotalWidth = floatLeftWidth + floatRightWidth;
            if (childStyle.clear() & CLEFT) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatLeftWidth = 0;
            }
            if (childStyle.clear() & CRIGHT) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatRightWidth = 0;
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        Length startMarginLength = childStyle.marginStartUsing(&styleToUse);
        Length endMarginLength = childStyle.marginEndUsing(&styleToUse);
        LayoutUnit margin = 0;
        LayoutUnit marginStart = 0;
        LayoutUnit marginEnd = 0;
        if (startMarginLength.isFixed())
            marginStart += startMarginLength.value();
        if (endMarginLength.isFixed())
            marginEnd += endMarginLength.value();
        margin = marginStart + marginEnd;

        LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
        if (child->isBox() && child->isHorizontalWritingMode() != isHorizontalWritingMode()) {
            RenderBox* childBox = toRenderBox(child);
            LogicalExtentComputedValues computedValues;
            childBox->computeLogicalHeight(childBox->borderAndPaddingLogicalHeight(), 0, computedValues);
            childMinPreferredLogicalWidth = childMaxPreferredLogicalWidth = computedValues.m_extent;
        } else {
            childMinPreferredLogicalWidth = child->minPreferredLogicalWidth();
            childMaxPreferredLogicalWidth = child->maxPreferredLogicalWidth();
        }

        LayoutUnit w = childMinPreferredLogicalWidth + margin;
        minLogicalWidth = std::max(w, minLogicalWidth);
        
        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !child->isTable())
            maxLogicalWidth = std::max(w, maxLogicalWidth);

        w = childMaxPreferredLogicalWidth + margin;

        if (!child->isFloating()) {
            if (child->isBox() && toRenderBox(child)->avoidsFloats()) {
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
            if (childStyle.floating() == LeftFloat)
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
    if (isReplaced() && linePositionMode == PositionOnContainingLine)
        return RenderBox::lineHeight(firstLine, direction, linePositionMode);

    if (firstLine && document().styleSheetCollection().usesFirstLineRules()) {
        RenderStyle& s = firstLine ? firstLineStyle() : style();
        if (&s != &style())
            return s.computedLineHeight(&view());
    }
    
    if (m_lineHeight == -1)
        m_lineHeight = style().computedLineHeight(&view());

    return m_lineHeight;
}

int RenderBlock::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && linePositionMode == PositionOnContainingLine) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        // FIXME: Need to patch form controls to deal with vertical lines.
        if (style().hasAppearance() && !theme().isControlContainer(style().appearance()))
            return theme().baselinePosition(this);
            
        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow.  We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves).  This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved) or if the baseline is outside
        // of our content box.
        bool ignoreBaseline = (layer() && (layer()->marquee() || (direction == HorizontalLine ? (layer()->verticalScrollbar() || layer()->scrollYOffset() != 0)
            : (layer()->horizontalScrollbar() || layer()->scrollXOffset() != 0)))) || (isWritingModeRoot() && !isRubyRun());
        
        int baselinePos = ignoreBaseline ? -1 : inlineBlockBaseline(direction);
        
        LayoutUnit bottomOfContent = direction == HorizontalLine ? borderTop() + paddingTop() + contentHeight() : borderRight() + paddingRight() + contentWidth();
        if (baselinePos != -1 && baselinePos <= bottomOfContent)
            return direction == HorizontalLine ? marginTop() + baselinePos : marginRight() + baselinePos;
            
        return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
    }

    const RenderStyle& style = firstLine ? firstLineStyle() : this->style();
    const FontMetrics& fontMetrics = style.fontMetrics();
    return fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2;
}

LayoutUnit RenderBlock::minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const
{
    if (!document().inNoQuirksMode() && replacedHeight)
        return replacedHeight;

    const RenderStyle& style = isFirstLine ? firstLineStyle() : this->style();
    if (!(style.lineBoxContain() & LineBoxContainBlock))
        return 0;

    return std::max<LayoutUnit>(replacedHeight, lineHeight(isFirstLine, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

int RenderBlock::firstLineBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    for (RenderBox* curr = firstChildBox(); curr; curr = curr->nextSiblingBox()) {
        if (!curr->isFloatingOrOutOfFlowPositioned()) {
            int result = curr->firstLineBaseline();
            if (result != -1)
                return curr->logicalTop() + result; // Translate to our coordinate space.
        }
    }

    return -1;
}

int RenderBlock::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    bool haveNormalFlowChild = false;
    for (auto box = lastChildBox(); box; box = box->previousSiblingBox()) {
        if (box->isFloatingOrOutOfFlowPositioned())
            continue;
        haveNormalFlowChild = true;
        int result = box->inlineBlockBaseline(lineDirection);
        if (result != -1)
            return box->logicalTop() + result; // Translate to our coordinate space.
    }

    if (!haveNormalFlowChild && hasLineIfEmpty()) {
        auto& fontMetrics = firstLineStyle().fontMetrics();
        return fontMetrics.ascent()
             + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
             + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
    }

    return -1;
}

RenderBlock* RenderBlock::firstLineBlock() const
{
    RenderBlock* firstLineBlock = const_cast<RenderBlock*>(this);
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style().hasPseudoStyle(FIRST_LINE);
        if (hasPseudo)
            break;
        RenderElement* parentBlock = firstLineBlock->parent();
        // We include isRenderButton in this check because buttons are
        // implemented using flex box but should still support first-line. The
        // flex box spec requires that flex box does not support first-line,
        // though.
        // FIXME: Remove when buttons are implemented with align-items instead
        // of flexbox.
        if (firstLineBlock->isReplaced() || firstLineBlock->isFloating()
            || !parentBlock || parentBlock->firstChild() != firstLineBlock || (!parentBlock->isRenderBlockFlow() && !parentBlock->isRenderButton()))
            break;
        firstLineBlock = toRenderBlock(parentBlock);
    } 
    
    if (!hasPseudo)
        return 0;
    
    return firstLineBlock;
}

static RenderStyle* styleForFirstLetter(RenderObject* firstLetterBlock, RenderObject* firstLetterContainer)
{
    RenderStyle* pseudoStyle = firstLetterBlock->getCachedPseudoStyle(FIRST_LETTER, &firstLetterContainer->firstLineStyle());
    // Force inline display (except for floating first-letters).
    pseudoStyle->setDisplay(pseudoStyle->isFloating() ? BLOCK : INLINE);
    // CSS2 says first-letter can't be positioned.
    pseudoStyle->setPosition(StaticPosition);
    return pseudoStyle;
}

// CSS 2.1 http://www.w3.org/TR/CSS21/selector.html#first-letter
// "Punctuation (i.e, characters defined in Unicode [UNICODE] in the "open" (Ps), "close" (Pe),
// "initial" (Pi). "final" (Pf) and "other" (Po) punctuation classes), that precedes or follows the first letter should be included"
static inline bool isPunctuationForFirstLetter(UChar c)
{
    return U_GET_GC_MASK(c) & (U_GC_PS_MASK | U_GC_PE_MASK | U_GC_PI_MASK | U_GC_PF_MASK | U_GC_PO_MASK);
}

static inline bool shouldSkipForFirstLetter(UChar c)
{
    return isSpaceOrNewline(c) || c == noBreakSpace || isPunctuationForFirstLetter(c);
}

static inline RenderBlock* findFirstLetterBlock(RenderBlock* start)
{
    RenderBlock* firstLetterBlock = start;
    while (true) {
        // We include isRenderButton in these two checks because buttons are
        // implemented using flex box but should still support first-letter.
        // The flex box spec requires that flex box does not support
        // first-letter, though.
        // FIXME: Remove when buttons are implemented with align-items instead
        // of flexbox.
        bool canHaveFirstLetterRenderer = firstLetterBlock->style().hasPseudoStyle(FIRST_LETTER)
            && firstLetterBlock->canHaveGeneratedChildren()
            && (!firstLetterBlock->isFlexibleBox() || firstLetterBlock->isRenderButton());
        if (canHaveFirstLetterRenderer)
            return firstLetterBlock;

        RenderElement* parentBlock = firstLetterBlock->parent();
        if (firstLetterBlock->isReplaced() || !parentBlock || parentBlock->firstChild() != firstLetterBlock
            || (!parentBlock->isRenderBlockFlow() && !parentBlock->isRenderButton()))
            return 0;
        firstLetterBlock = toRenderBlock(parentBlock);
    } 

    return 0;
}

void RenderBlock::updateFirstLetterStyle(RenderObject* firstLetterBlock, RenderObject* currentChild)
{
    RenderElement* firstLetter = currentChild->parent();
    RenderElement* firstLetterContainer = firstLetter->parent();
    RenderStyle* pseudoStyle = styleForFirstLetter(firstLetterBlock, firstLetterContainer);
    ASSERT(firstLetter->isFloating() || firstLetter->isInline());

    if (Style::determineChange(&firstLetter->style(), pseudoStyle) == Style::Detach) {
        // The first-letter renderer needs to be replaced. Create a new renderer of the right type.
        RenderBoxModelObject* newFirstLetter;
        if (pseudoStyle->display() == INLINE)
            newFirstLetter = new RenderInline(document(), *pseudoStyle);
        else
            newFirstLetter = new RenderBlockFlow(document(), *pseudoStyle);
        newFirstLetter->initializeStyle();

        // Move the first letter into the new renderer.
        LayoutStateDisabler layoutStateDisabler(&view());
        while (RenderObject* child = firstLetter->firstChild()) {
            if (child->isText())
                toRenderText(child)->removeAndDestroyTextBoxes();
            firstLetter->removeChild(*child);
            newFirstLetter->addChild(child, 0);
        }

        RenderObject* nextSibling = firstLetter->nextSibling();
        if (RenderTextFragment* remainingText = toRenderBoxModelObject(firstLetter)->firstLetterRemainingText()) {
            ASSERT(remainingText->isAnonymous() || remainingText->textNode()->renderer() == remainingText);
            // Replace the old renderer with the new one.
            remainingText->setFirstLetter(*newFirstLetter);
            newFirstLetter->setFirstLetterRemainingText(remainingText);
        }
        // To prevent removal of single anonymous block in RenderBlock::removeChild and causing
        // |nextSibling| to go stale, we remove the old first letter using removeChildNode first.
        firstLetterContainer->removeChildInternal(*firstLetter, NotifyChildren);
        firstLetter->destroy();
        firstLetter = newFirstLetter;
        firstLetterContainer->addChild(firstLetter, nextSibling);
    } else
        firstLetter->setStyle(*pseudoStyle);
}

void RenderBlock::createFirstLetterRenderer(RenderObject* firstLetterBlock, RenderText* currentTextChild)
{
    RenderElement* firstLetterContainer = currentTextChild->parent();
    RenderStyle* pseudoStyle = styleForFirstLetter(firstLetterBlock, firstLetterContainer);
    RenderBoxModelObject* firstLetter = 0;
    if (pseudoStyle->display() == INLINE)
        firstLetter = new RenderInline(document(), *pseudoStyle);
    else
        firstLetter = new RenderBlockFlow(document(), *pseudoStyle);
    firstLetter->initializeStyle();
    firstLetterContainer->addChild(firstLetter, currentTextChild);

    // The original string is going to be either a generated content string or a DOM node's
    // string.  We want the original string before it got transformed in case first-letter has
    // no text-transform or a different text-transform applied to it.
    String oldText = currentTextChild->originalText();
    ASSERT(!oldText.isNull());

    if (!oldText.isEmpty()) {
        unsigned length = 0;

        // Account for leading spaces and punctuation.
        while (length < oldText.length() && shouldSkipForFirstLetter(oldText[length]))
            length++;

        // Account for first letter.
        length++;
        
        // Keep looking for whitespace and allowed punctuation, but avoid
        // accumulating just whitespace into the :first-letter.
        for (unsigned scanLength = length; scanLength < oldText.length(); ++scanLength) {
            UChar c = oldText[scanLength];
            
            if (!shouldSkipForFirstLetter(c))
                break;

            if (isPunctuationForFirstLetter(c))
                length = scanLength + 1;
         }
         
        // Construct a text fragment for the text after the first letter.
        // This text fragment might be empty.
        RenderTextFragment* remainingText;
        if (currentTextChild->textNode())
            remainingText = new RenderTextFragment(*currentTextChild->textNode(), oldText, length, oldText.length() - length);
        else
            remainingText = new RenderTextFragment(document(), oldText, length, oldText.length() - length);

        if (remainingText->textNode())
            remainingText->textNode()->setRenderer(remainingText);

        firstLetterContainer->addChild(remainingText, currentTextChild);
        firstLetterContainer->removeChild(*currentTextChild);
        remainingText->setFirstLetter(*firstLetter);
        firstLetter->setFirstLetterRemainingText(remainingText);
        
        // construct text fragment for the first letter
        RenderTextFragment* letter;
        if (remainingText->textNode())
            letter = new RenderTextFragment(*remainingText->textNode(), oldText, 0, length);
        else
            letter = new RenderTextFragment(document(), oldText, 0, length);

        firstLetter->addChild(letter);

        currentTextChild->destroy();
    }
}

void RenderBlock::updateFirstLetter()
{
    if (!document().styleSheetCollection().usesFirstLetterRules())
        return;
    // Don't recur
    if (style().styleType() == FIRST_LETTER)
        return;

    // FIXME: We need to destroy the first-letter object if it is no longer the first child. Need to find
    // an efficient way to check for that situation though before implementing anything.
    RenderElement* firstLetterBlock = findFirstLetterBlock(this);
    if (!firstLetterBlock)
        return;

    // Drill into inlines looking for our first text descendant.
    RenderObject* descendant = firstLetterBlock->firstChild();
    while (descendant) {
        if (descendant->isText())
            break;
        RenderElement& current = toRenderElement(*descendant);
        if (current.isListMarker())
            descendant = current.nextSibling();
        else if (current.isFloatingOrOutOfFlowPositioned()) {
            if (current.style().styleType() == FIRST_LETTER) {
                descendant = current.firstChild();
                break;
            }
            descendant = current.nextSibling();
        } else if (current.isReplaced() || current.isRenderButton() || current.isMenuList())
            break;
        else if (current.style().hasPseudoStyle(FIRST_LETTER) && current.canHaveGeneratedChildren())  {
            // We found a lower-level node with first-letter, which supersedes the higher-level style
            firstLetterBlock = &current;
            descendant = current.firstChild();
        } else
            descendant = current.firstChild();
    }

    if (!descendant)
        return;

    // If the child already has style, then it has already been created, so we just want
    // to update it.
    if (descendant->parent()->style().styleType() == FIRST_LETTER) {
        updateFirstLetterStyle(firstLetterBlock, descendant);
        return;
    }

    if (!descendant->isText())
        return;

    // Our layout state is not valid for the repaints we are going to trigger by
    // adding and removing children of firstLetterContainer.
    LayoutStateDisabler layoutStateDisabler(&view());

    createFirstLetterRenderer(firstLetterBlock, toRenderText(descendant));
}

LayoutUnit RenderBlock::paginationStrut() const
{
    RenderBlockRareData* rareData = getRareData(this);
    return rareData ? rareData->m_paginationStrut : LayoutUnit();
}

LayoutUnit RenderBlock::pageLogicalOffset() const
{
    RenderBlockRareData* rareData = getRareData(this);
    return rareData ? rareData->m_pageLogicalOffset : LayoutUnit();
}

void RenderBlock::setPaginationStrut(LayoutUnit strut)
{
    RenderBlockRareData* rareData = getRareData(this);
    if (!rareData) {
        if (!strut)
            return;
        rareData = &ensureRareData(this);
    }
    rareData->m_paginationStrut = strut;
}

void RenderBlock::setPageLogicalOffset(LayoutUnit logicalOffset)
{
    RenderBlockRareData* rareData = getRareData(this);
    if (!rareData) {
        if (!logicalOffset)
            return;
        rareData = &ensureRareData(this);
    }
    rareData->m_pageLogicalOffset = logicalOffset;
}

void RenderBlock::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        rects.append(pixelSnappedIntRect(accumulatedOffset.x(), accumulatedOffset.y() - collapsedMarginBefore(),
                                width(), height() + collapsedMarginBefore() + collapsedMarginAfter()));
        continuation()->absoluteRects(rects, accumulatedOffset - toLayoutSize(location() +
                inlineElementContinuation()->containingBlock()->location()));
    } else
        rects.append(pixelSnappedIntRect(accumulatedOffset, size()));
}

void RenderBlock::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        FloatRect localRect(0, -collapsedMarginBefore(),
                            width(), height() + collapsedMarginBefore() + collapsedMarginAfter());
        quads.append(localToAbsoluteQuad(localRect, 0 /* mode */, wasFixed));
        continuation()->absoluteQuads(quads, wasFixed);
    } else
        quads.append(RenderBox::localToAbsoluteQuad(FloatRect(0, 0, width(), height()), 0 /* mode */, wasFixed));
}

LayoutRect RenderBlock::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBox::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    if (isAnonymousBlockContinuation())
        r.inflateY(collapsedMarginBefore()); // FIXME: This is wrong for block-flows that are horizontal.
    return r;
}

RenderElement* RenderBlock::hoverAncestor() const
{
    return isAnonymousBlockContinuation() ? continuation() : RenderBox::hoverAncestor();
}

void RenderBlock::updateDragState(bool dragOn)
{
    RenderBox::updateDragState(dragOn);
    if (RenderBoxModelObject* continuation = this->continuation())
        continuation->updateDragState(dragOn);
}

const RenderStyle& RenderBlock::outlineStyleForRepaint() const
{
    return isAnonymousBlockContinuation() ? continuation()->style() : style();
}

void RenderBlock::childBecameNonInline(RenderObject*)
{
    makeChildrenNonInline();
    if (isAnonymousBlock() && parent() && parent()->isRenderBlock())
        toRenderBlock(parent())->removeLeftoverAnonymousBlock(this);
    // |this| may be dead here
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

LayoutRect RenderBlock::localCaretRect(InlineBox* inlineBox, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    // Do the normal calculation in most cases.
    if (firstChild())
        return RenderBox::localCaretRect(inlineBox, caretOffset, extraWidthToEndOfLine);

    LayoutRect caretRect = localCaretRectForEmptyElement(width(), textIndentOffset());

    // FIXME: Does this need to adjust for vertical orientation?
    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = width() - caretRect.maxX();

    return caretRect;
}

void RenderBlock::addFocusRingRectsForInlineChildren(Vector<IntRect>&, const LayoutPoint&, const RenderLayerModelObject*)
{
    ASSERT_NOT_REACHED();
}

void RenderBlock::addFocusRingRects(Vector<IntRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (inlineElementContinuation()) {
        // FIXME: This check really isn't accurate. 
        bool nextInlineHasLineBox = inlineElementContinuation()->firstLineBox();
        // FIXME: This is wrong. The principal renderer may not be the continuation preceding this block.
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = toRenderInline(inlineElementContinuation()->element()->renderer())->firstLineBox();
        float topMargin = prevInlineHasLineBox ? collapsedMarginBefore() : LayoutUnit();
        float bottomMargin = nextInlineHasLineBox ? collapsedMarginAfter() : LayoutUnit();
        LayoutRect rect(additionalOffset.x(), additionalOffset.y() - topMargin, width(), height() + topMargin + bottomMargin);
        if (!rect.isEmpty())
            rects.append(pixelSnappedIntRect(rect));
    } else if (width() && height())
        rects.append(pixelSnappedIntRect(additionalOffset, size()));

    if (!hasOverflowClip() && !hasControlClip()) {
        if (childrenInline())
            addFocusRingRectsForInlineChildren(rects, additionalOffset, paintContainer);
    
        for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
            if (!curr->isText() && !curr->isListMarker() && curr->isBox()) {
                RenderBox* box = toRenderBox(curr);
                FloatPoint pos;
                // FIXME: This doesn't work correctly with transforms.
                if (box->layer()) 
                    pos = curr->localToContainerPoint(FloatPoint(), paintContainer);
                else
                    pos = FloatPoint(additionalOffset.x() + box->x(), additionalOffset.y() + box->y());
                box->addFocusRingRects(rects, flooredLayoutPoint(pos), paintContainer);
            }
        }
    }

    if (inlineElementContinuation())
        inlineElementContinuation()->addFocusRingRects(rects, flooredLayoutPoint(LayoutPoint(additionalOffset + inlineElementContinuation()->containingBlock()->location() - location())), paintContainer);
}

RenderBox* RenderBlock::createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const
{
    if (isAnonymousColumnsBlock())
        return createAnonymousColumnsWithParentRenderer(parent);
    if (isAnonymousColumnSpanBlock())
        return createAnonymousColumnSpanWithParentRenderer(parent);
    return createAnonymousWithParentRendererAndDisplay(parent, style().display());
}

ColumnInfo::PaginationUnit RenderBlock::paginationUnit() const
{
    return ColumnInfo::Column;
}

LayoutUnit RenderBlock::offsetFromLogicalTopOfFirstPage() const
{
    LayoutState* layoutState = view().layoutState();
    if (layoutState && !layoutState->isPaginated())
        return 0;

    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (flowThread)
        return flowThread->offsetFromLogicalTopOfFirstRegion(this);

    if (layoutState) {
        ASSERT(layoutState->m_renderer == this);

        LayoutSize offsetDelta = layoutState->m_layoutOffset - layoutState->m_pageOffset;
        return isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }
    
    ASSERT_NOT_REACHED();
    return 0;
}

RenderRegion* RenderBlock::regionAtBlockOffset(LayoutUnit blockOffset) const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread || !flowThread->hasValidRegionInfo())
        return 0;

    return flowThread->regionAtBlockOffset(this, offsetFromLogicalTopOfFirstPage() + blockOffset, true);
}

static bool canComputeRegionRangeForBox(const RenderBlock* parentBlock, const RenderBox& childBox, const RenderFlowThread* flowThreadContainingBlock)
{
    ASSERT(parentBlock);
    ASSERT(!childBox.isRenderFlowThread());

    if (!flowThreadContainingBlock)
        return false;

    if (!flowThreadContainingBlock->hasRegions())
        return false;

    if (!childBox.canHaveOutsideRegionRange())
        return false;

    return flowThreadContainingBlock->hasRegionRangeForBox(parentBlock);
}

bool RenderBlock::childBoxIsUnsplittableForFragmentation(const RenderBox& child) const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool isInsideMulticolFlowThread = flowThread && !flowThread->isRenderNamedFlowThread();
    bool checkColumnBreaks = isInsideMulticolFlowThread || view().layoutState()->isPaginatingColumns();
    bool checkPageBreaks = !checkColumnBreaks && view().layoutState()->m_pageLogicalHeight;
    bool checkRegionBreaks = flowThread && flowThread->isRenderNamedFlowThread();
    return child.isUnsplittableForPagination() || (checkColumnBreaks && child.style().columnBreakInside() == PBAVOID)
        || (checkPageBreaks && child.style().pageBreakInside() == PBAVOID)
        || (checkRegionBreaks && child.style().regionBreakInside() == PBAVOID);
}

void RenderBlock::computeRegionRangeForBoxChild(const RenderBox& box) const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    ASSERT(canComputeRegionRangeForBox(this, box, flowThread));

    RenderRegion* startRegion;
    RenderRegion* endRegion;
    LayoutUnit offsetFromLogicalTopOfFirstRegion = box.offsetFromLogicalTopOfFirstPage();
    if (childBoxIsUnsplittableForFragmentation(box))
        startRegion = endRegion = flowThread->regionAtBlockOffset(this, offsetFromLogicalTopOfFirstRegion, true);
    else {
        startRegion = flowThread->regionAtBlockOffset(this, offsetFromLogicalTopOfFirstRegion, true);
        endRegion = flowThread->regionAtBlockOffset(this, offsetFromLogicalTopOfFirstRegion + logicalHeightForChild(box), true);
    }

    flowThread->setRegionRangeForBox(&box, startRegion, endRegion);
}

void RenderBlock::estimateRegionRangeForBoxChild(const RenderBox& box) const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!canComputeRegionRangeForBox(this, box, flowThread))
        return;

    if (childBoxIsUnsplittableForFragmentation(box)) {
        computeRegionRangeForBoxChild(box);
        return;
    }

    LogicalExtentComputedValues estimatedValues;
    box.computeLogicalHeight(RenderFlowThread::maxLogicalHeight(), logicalTopForChild(box), estimatedValues);

    LayoutUnit offsetFromLogicalTopOfFirstRegion = box.offsetFromLogicalTopOfFirstPage();
    RenderRegion* startRegion = flowThread->regionAtBlockOffset(this, offsetFromLogicalTopOfFirstRegion, true);
    RenderRegion* endRegion = flowThread->regionAtBlockOffset(this, offsetFromLogicalTopOfFirstRegion + estimatedValues.m_extent, true);

    flowThread->setRegionRangeForBox(&box, startRegion, endRegion);
}

bool RenderBlock::updateRegionRangeForBoxChild(const RenderBox& box) const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!canComputeRegionRangeForBox(this, box, flowThread))
        return false;

    RenderRegion* startRegion = nullptr;
    RenderRegion* endRegion = nullptr;
    flowThread->getRegionRangeForBox(&box, startRegion, endRegion);

    computeRegionRangeForBoxChild(box);

    RenderRegion* newStartRegion = nullptr;
    RenderRegion* newEndRegion = nullptr;
    flowThread->getRegionRangeForBox(&box, newStartRegion, newEndRegion);

    // The region range of the box has changed. Some boxes (e.g floats) may have been positioned assuming
    // a different range.
    // FIXME: Be smarter about this. We don't need to relayout all the time.
    if (newStartRegion != startRegion || newEndRegion != endRegion)
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
        return child.isRenderBlock() ? toRenderBlock(child).hasMarginBeforeQuirk() : child.style().hasMarginBeforeQuirk();
    
    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.isRenderBlock() ? toRenderBlock(child).hasMarginAfterQuirk() : child.style().hasMarginAfterQuirk();
    
    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

bool RenderBlock::hasMarginAfterQuirk(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child.isWritingModeRoot())
        return child.isRenderBlock() ? toRenderBlock(child).hasMarginAfterQuirk() : child.style().hasMarginAfterQuirk();
    
    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.isRenderBlock() ? toRenderBlock(child).hasMarginBeforeQuirk() : child.style().hasMarginBeforeQuirk();
    
    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

const char* RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"; // FIXME: Temporary hack until we know that the regression tests pass.

    if (isFloating())
        return "RenderBlock (floating)";
    if (isOutOfFlowPositioned())
        return "RenderBlock (positioned)";
    if (isAnonymousColumnsBlock())
        return "RenderBlock (anonymous multi-column)";
    if (isAnonymousColumnSpanBlock())
        return "RenderBlock (anonymous multi-column span)";
    if (isAnonymousBlock())
        return "RenderBlock (anonymous)";
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderBlock (generated)";
    if (isAnonymous())
        return "RenderBlock (generated)";
    if (isRelPositioned())
        return "RenderBlock (relative positioned)";
    if (isStickyPositioned())
        return "RenderBlock (sticky positioned)";
    return "RenderBlock";
}

template <typename CharacterType>
static inline TextRun constructTextRunInternal(RenderObject* context, const Font& font, const CharacterType* characters, int length, const RenderStyle& style, TextRun::ExpansionBehavior expansion)
{
    TextDirection textDirection = LTR;
    bool directionalOverride = style.rtlOrdering() == VisualOrder;

    TextRun run(characters, length, 0, 0, expansion, textDirection, directionalOverride);
    if (font.isSVGFont()) {
        ASSERT(context); // FIXME: Thread a RenderObject& to this point so we don't have to dereference anything.
        run.setRenderingContext(SVGTextRunRenderingContext::create(*context));
    }

    return run;
}

template <typename CharacterType>
static inline TextRun constructTextRunInternal(RenderObject* context, const Font& font, const CharacterType* characters, int length, const RenderStyle& style, TextRun::ExpansionBehavior expansion, TextRunFlags flags)
{
    TextDirection textDirection = LTR;
    bool directionalOverride = style.rtlOrdering() == VisualOrder;
    if (flags != DefaultTextRunFlags) {
        if (flags & RespectDirection)
            textDirection = style.direction();
        if (flags & RespectDirectionOverride)
            directionalOverride |= isOverride(style.unicodeBidi());
    }
    TextRun run(characters, length, 0, 0, expansion, textDirection, directionalOverride);
    if (font.isSVGFont()) {
        ASSERT(context); // FIXME: Thread a RenderObject& to this point so we don't have to dereference anything.
        run.setRenderingContext(SVGTextRunRenderingContext::create(*context));
    }

    return run;
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const Font& font, const LChar* characters, int length, const RenderStyle& style, TextRun::ExpansionBehavior expansion)
{
    return constructTextRunInternal(context, font, characters, length, style, expansion);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const Font& font, const UChar* characters, int length, const RenderStyle& style, TextRun::ExpansionBehavior expansion)
{
    return constructTextRunInternal(context, font, characters, length, style, expansion);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const Font& font, const RenderText* text, const RenderStyle& style, TextRun::ExpansionBehavior expansion)
{
    if (text->is8Bit())
        return constructTextRunInternal(context, font, text->characters8(), text->textLength(), style, expansion);
    return constructTextRunInternal(context, font, text->characters16(), text->textLength(), style, expansion);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const Font& font, const RenderText* text, unsigned offset, unsigned length, const RenderStyle& style, TextRun::ExpansionBehavior expansion)
{
    ASSERT(offset + length <= text->textLength());
    if (text->is8Bit())
        return constructTextRunInternal(context, font, text->characters8() + offset, length, style, expansion);
    return constructTextRunInternal(context, font, text->characters16() + offset, length, style, expansion);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const Font& font, const String& string, const RenderStyle& style, TextRun::ExpansionBehavior expansion, TextRunFlags flags)
{
    unsigned length = string.length();

    if (!length || string.is8Bit())
        return constructTextRunInternal(context, font, string.characters8(), length, style, expansion, flags);
    return constructTextRunInternal(context, font, string.characters16(), length, style, expansion, flags);
}

RenderBlock* RenderBlock::createAnonymousWithParentRendererAndDisplay(const RenderObject* parent, EDisplay display)
{
    // FIXME: Do we need to convert all our inline displays to block-type in the anonymous logic ?
    RenderBlock* newBox;
    if (display == FLEX || display == INLINE_FLEX)
        newBox = new RenderFlexibleBox(parent->document(), RenderStyle::createAnonymousStyleWithDisplay(&parent->style(), FLEX));
    else
        newBox = new RenderBlockFlow(parent->document(), RenderStyle::createAnonymousStyleWithDisplay(&parent->style(), BLOCK));

    newBox->initializeStyle();
    return newBox;
}

RenderBlock* RenderBlock::createAnonymousColumnsWithParentRenderer(const RenderObject* parent)
{
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(&parent->style(), BLOCK);
    newStyle.get().inheritColumnPropertiesFrom(&parent->style());

    RenderBlock* newBox = new RenderBlockFlow(parent->document(), std::move(newStyle));
    newBox->initializeStyle();
    return newBox;
}

RenderBlock* RenderBlock::createAnonymousColumnSpanWithParentRenderer(const RenderObject* parent)
{
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(&parent->style(), BLOCK);
    newStyle.get().setColumnSpan(ColumnSpanAll);

    RenderBlock* newBox = new RenderBlockFlow(parent->document(), std::move(newStyle));
    newBox->initializeStyle();
    return newBox;
}

void RenderBlock::computeLineGridPaginationOrigin(LayoutState& layoutState) const
{
    if (!hasColumns() || !style().hasInlineColumnAxis())
        return;
    
    // We need to cache a line grid pagination origin so that we understand how to reset the line grid
    // at the top of each column.
    // Get the current line grid and offset.
    const auto lineGrid = layoutState.lineGrid();
    if (!lineGrid)
        return;

    // Get the hypothetical line box used to establish the grid.
    auto lineGridBox = lineGrid->lineGridBox();
    if (!lineGridBox)
        return;
    
    bool isHorizontalWritingMode = lineGrid->isHorizontalWritingMode();

    LayoutUnit lineGridBlockOffset = isHorizontalWritingMode ? layoutState.lineGridOffset().height() : layoutState.lineGridOffset().width();

    // Now determine our position on the grid. Our baseline needs to be adjusted to the nearest baseline multiple
    // as established by the line box.
    // FIXME: Need to handle crazy line-box-contain values that cause the root line box to not be considered. I assume
    // the grid should honor line-box-contain.
    LayoutUnit gridLineHeight = lineGridBox->lineBottomWithLeading() - lineGridBox->lineTopWithLeading();
    if (!gridLineHeight)
        return;

    LayoutUnit firstLineTopWithLeading = lineGridBlockOffset + lineGridBox->lineTopWithLeading();
    
    if (layoutState.isPaginated() && layoutState.pageLogicalHeight()) {
        LayoutUnit pageLogicalTop = isHorizontalWritingMode ? layoutState.pageOffset().height() : layoutState.pageOffset().width();
        if (pageLogicalTop > firstLineTopWithLeading) {
            // Shift to the next highest line grid multiple past the page logical top. Cache the delta
            // between this new value and the page logical top as the pagination origin.
            LayoutUnit remainder = roundToInt(pageLogicalTop - firstLineTopWithLeading) % roundToInt(gridLineHeight);
            LayoutUnit paginationDelta = gridLineHeight - remainder;
            if (isHorizontalWritingMode)
                layoutState.setLineGridPaginationOrigin(LayoutSize(layoutState.lineGridPaginationOrigin().width(), paginationDelta));
            else
                layoutState.setLineGridPaginationOrigin(LayoutSize(paginationDelta, layoutState.lineGridPaginationOrigin().height()));
        }
    }
}

#ifndef NDEBUG
void RenderBlock::checkPositionedObjectsNeedLayout()
{
    if (!gPositionedDescendantsMap)
        return;

    TrackedRendererListHashSet* positionedDescendantSet = positionedObjects();
    if (!positionedDescendantSet)
        return;

    for (auto it = positionedDescendantSet->begin(), end = positionedDescendantSet->end(); it != end; ++it) {
        RenderBox* currBox = *it;
        ASSERT(!currBox->needsLayout());
    }
}

void RenderBlock::showLineTreeAndMark(const InlineBox*, const char*, const InlineBox*, const char*, const RenderObject*) const
{
    showRenderObject();
}

#endif

} // namespace WebCore
