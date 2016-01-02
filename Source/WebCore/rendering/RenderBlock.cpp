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
#include "RenderButton.h"
#include "RenderCombineText.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h"
#include "RenderRegion.h"
#include "RenderTableCell.h"
#include "RenderTextFragment.h"
#include "RenderTheme.h"
#include "RenderTreePosition.h"
#include "RenderView.h"
#include "SVGTextRunRenderingContext.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TextBreakIterator.h"
#include "TransformState.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
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
};

COMPILE_ASSERT(sizeof(RenderBlock) == sizeof(SameSizeAsRenderBlock), RenderBlock_should_stay_small);

static TrackedDescendantsMap* gPositionedDescendantsMap;
static TrackedDescendantsMap* gPercentHeightDescendantsMap;

static TrackedContainerMap* gPositionedContainerMap;
static TrackedContainerMap* gPercentHeightContainerMap;

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
        : m_paginationStrut(0)
        , m_pageLogicalOffset(0)
        , m_flowThreadContainingBlock(Nullopt)
    {
    }

    LayoutUnit m_paginationStrut;
    LayoutUnit m_pageLogicalOffset;

    Optional<RenderFlowThread*> m_flowThreadContainingBlock;
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

        Ref<OverflowEvent> overflowEvent = OverflowEvent::create(horizontalLayoutOverflowChanged, hasHorizontalLayoutOverflow, verticalLayoutOverflowChanged, hasVerticalLayoutOverflow);
        overflowEvent->setTarget(m_block->element());
        m_block->document().enqueueOverflowEvent(WTFMove(overflowEvent));
    }

private:
    const RenderBlock* m_block;
    bool m_shouldDispatchEvent;
    bool m_hadHorizontalLayoutOverflow;
    bool m_hadVerticalLayoutOverflow;
};

RenderBlock::RenderBlock(Element& element, Ref<RenderStyle>&& style, BaseTypeFlags baseTypeFlags)
    : RenderBox(element, WTFMove(style), baseTypeFlags | RenderBlockFlag)
{
}

RenderBlock::RenderBlock(Document& document, Ref<RenderStyle>&& style, BaseTypeFlags baseTypeFlags)
    : RenderBox(document, WTFMove(style), baseTypeFlags | RenderBlockFlag)
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
    removeFromUpdateScrollInfoAfterLayoutTransaction();

    if (gRareDataMap)
        gRareDataMap->remove(this);
    if (gPercentHeightDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPercentHeightDescendantsMap, gPercentHeightContainerMap);
    if (gPositionedDescendantsMap)
        removeBlockFromDescendantAndContainerMaps(this, gPositionedDescendantsMap, gPositionedContainerMap);
}

void RenderBlock::willBeDestroyed()
{
    if (!documentBeingDestroyed()) {
        if (parent())
            parent()->dirtyLinesFromChangedChild(*this);
    }

    RenderBox::willBeDestroyed();
}

bool RenderBlock::hasRareData() const
{
    return gRareDataMap ? gRareDataMap->contains(this) : false;
}

void RenderBlock::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;

    setReplaced(newStyle.isDisplayInlineType());

    if (oldStyle && oldStyle->hasTransformRelatedProperty() && !newStyle.hasTransformRelatedProperty())
        removePositionedObjects(nullptr, NewContainingBlock);

    if (oldStyle && parent() && diff == StyleDifferenceLayout && oldStyle->position() != newStyle.position()) {
        if (newStyle.position() == StaticPosition)
            // Clear our positioned objects list. Our absolutely positioned descendants will be
            // inserted into our containing block's positioned objects list during layout.
            removePositionedObjects(nullptr, NewContainingBlock);
        else if (oldStyle->position() == StaticPosition) {
            // Remove our absolutely positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            auto containingBlock = parent();
            while (containingBlock && (containingBlock->style().position() == StaticPosition || (containingBlock->isInline() && !containingBlock->isReplaced())) && !containingBlock->isRenderView()) {
                if (containingBlock->style().position() == RelativePosition && containingBlock->isInline() && !containingBlock->isReplaced()) {
                    containingBlock = containingBlock->containingBlock();
                    break;
                }
                containingBlock = containingBlock->parent();
            }

            if (is<RenderBlock>(*containingBlock))
                downcast<RenderBlock>(*containingBlock).removePositionedObjects(this, NewContainingBlock);
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
    RenderStyle& newStyle = style();

    bool hadTransform = hasTransform();
    bool flowThreadContainingBlockInvalidated = false;
    if (oldStyle && oldStyle->position() != newStyle.position()) {
        invalidateFlowThreadContainingBlockIncludingDescendants();
        flowThreadContainingBlockInvalidated = true;
    }

    RenderBox::styleDidChange(diff, oldStyle);

    if (hadTransform != hasTransform() && !flowThreadContainingBlockInvalidated)
        invalidateFlowThreadContainingBlockIncludingDescendants();

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

    // It's possible for our border/padding to change, but for the overall logical width of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    setHasBorderOrPaddingLogicalWidthChanged(oldStyle && diff == StyleDifferenceLayout && needsLayout() && borderOrPaddingLogicalWidthChanged(oldStyle, &newStyle));
}

RenderBlock* RenderBlock::continuationBefore(RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;

    RenderBlock* nextToLast = this;
    RenderBlock* last = this;
    for (auto* current = downcast<RenderBlock>(continuation()); current; current = downcast<RenderBlock>(current->continuation())) {
        if (beforeChild && beforeChild->parent() == current) {
            if (current->firstChild() == beforeChild)
                return last;
            return current;
        }

        nextToLast = last;
        last = current;
    }

    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

void RenderBlock::addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderBlock* flow = continuationBefore(beforeChild);
    ASSERT(!beforeChild || is<RenderBlock>(*beforeChild->parent()));
    RenderBoxModelObject* beforeChildParent = nullptr;
    if (beforeChild)
        beforeChildParent = downcast<RenderBoxModelObject>(beforeChild->parent());
    else {
        RenderBoxModelObject* continuation = flow->continuation();
        if (continuation)
            beforeChildParent = continuation;
        else
            beforeChildParent = flow;
    }

    if (newChild->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringContinuation(newChild, beforeChild);
        return;
    }

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

RenderPtr<RenderBlock> RenderBlock::clone() const
{
    RenderPtr<RenderBlock> cloneBlock;
    if (isAnonymousBlock()) {
        cloneBlock = RenderPtr<RenderBlock>(createAnonymousBlock());
        cloneBlock->setChildrenInline(childrenInline());
    } else {
        RenderTreePosition insertionPosition(*parent());
        cloneBlock = static_pointer_cast<RenderBlock>(element()->createElementRenderer(style(), insertionPosition));
        cloneBlock->initializeStyle();

        // This takes care of setting the right value of childrenInline in case
        // generated content is added to cloneBlock and 'this' does not have
        // generated content added yet.
        cloneBlock->setChildrenInline(cloneBlock->firstChild() ? cloneBlock->firstChild()->isInline() : childrenInline());
    }
    cloneBlock->setFlowThreadState(flowThreadState());
    return cloneBlock;
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
            downcast<RenderBlock>(*afterChild).addChild(newChild);
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
 
    if (madeBoxesNonInline && is<RenderBlock>(parent()) && isAnonymousBlock())
        downcast<RenderBlock>(*parent()).removeLeftoverAnonymousBlock(this);
    // this object may be dead here
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
    
    if (child->continuation())
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

static bool canDropAnonymousBlock(const RenderBlock& anonymousBlock)
{
    if (anonymousBlock.beingDestroyed() || anonymousBlock.continuation())
        return false;
    if (anonymousBlock.isRubyRun() || anonymousBlock.isRubyBase())
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
        RenderBlock& previousAnonymousBlock = downcast<RenderBlock>(*previous);
        if (!canDropAnonymousBlock(previousAnonymousBlock))
            return false;
    }
    if (next) {
        if (!next->isAnonymousBlock())
            return false;
        RenderBlock& nextAnonymousBlock = downcast<RenderBlock>(*next);
        if (!canDropAnonymousBlock(nextAnonymousBlock))
            return false;
    }
    return true;
}

void RenderBlock::dropAnonymousBoxChild(RenderBlock& parent, RenderBlock& child)
{
    parent.setNeedsLayoutAndPrefWidthsRecalc();
    parent.setChildrenInline(child.childrenInline());
    if (auto* childFlowThread = child.flowThreadContainingBlock())
        childFlowThread->removeFlowChildInfo(&child);

    RenderObject* nextSibling = child.nextSibling();
    parent.removeChildInternal(child, child.hasLayer() ? NotifyChildren : DontNotifyChildren);
    child.moveAllChildrenTo(&parent, nextSibling, child.hasLayer());
    // Delete the now-empty block's lines and nuke it.
    child.deleteLines();
    child.destroy();
}

void RenderBlock::removeChild(RenderObject& oldChild)
{
    // No need to waste time in merging or removing empty anonymous blocks.
    // We can just bail out if our document is getting destroyed.
    if (documentBeingDestroyed()) {
        RenderBox::removeChild(oldChild);
        return;
    }

    // If this child is a block, and if our previous and next siblings are both anonymous blocks
    // with inline content, then we can fold the inline content back together.
    RenderObject* prev = oldChild.previousSibling();
    RenderObject* next = oldChild.nextSibling();
    bool canMergeAnonymousBlocks = canMergeContiguousAnonymousBlocks(oldChild, prev, next);
    if (canMergeAnonymousBlocks && prev && next) {
        prev->setNeedsLayoutAndPrefWidthsRecalc();
        RenderBlock& nextBlock = downcast<RenderBlock>(*next);
        RenderBlock& prevBlock = downcast<RenderBlock>(*prev);
       
        if (prev->childrenInline() != next->childrenInline()) {
            RenderBlock& inlineChildrenBlock = prev->childrenInline() ? prevBlock : nextBlock;
            RenderBlock& blockChildrenBlock = prev->childrenInline() ? nextBlock : prevBlock;
            
            // Place the inline children block inside of the block children block instead of deleting it.
            // In order to reuse it, we have to reset it to just be a generic anonymous block.  Make sure
            // to clear out inherited column properties by just making a new style, and to also clear the
            // column span flag if it is set.
            ASSERT(!inlineChildrenBlock.continuation());
            // Cache this value as it might get changed in setStyle() call.
            bool inlineChildrenBlockHasLayer = inlineChildrenBlock.hasLayer();
            inlineChildrenBlock.setStyle(RenderStyle::createAnonymousStyleWithDisplay(&style(), BLOCK));
            removeChildInternal(inlineChildrenBlock, inlineChildrenBlockHasLayer ? NotifyChildren : DontNotifyChildren);
            
            // Now just put the inlineChildrenBlock inside the blockChildrenBlock.
            RenderObject* beforeChild = prev == &inlineChildrenBlock ? blockChildrenBlock.firstChild() : nullptr;
            blockChildrenBlock.insertChildInternal(&inlineChildrenBlock, beforeChild,
                (inlineChildrenBlockHasLayer || blockChildrenBlock.hasLayer()) ? NotifyChildren : DontNotifyChildren);
            next->setNeedsLayoutAndPrefWidthsRecalc();
            
            // inlineChildrenBlock got reparented to blockChildrenBlock, so it is no longer a child
            // of "this". we null out prev or next so that is not used later in the function.
            if (&inlineChildrenBlock == &prevBlock)
                prev = nullptr;
            else
                next = nullptr;
        } else {
            // Take all the children out of the |next| block and put them in
            // the |prev| block.
            nextBlock.moveAllChildrenIncludingFloatsTo(prevBlock, nextBlock.hasLayer() || prevBlock.hasLayer());
            
            // Delete the now-empty block's lines and nuke it.
            nextBlock.deleteLines();
            nextBlock.destroy();
            next = nullptr;
        }
    }

    invalidateLineLayoutPath();

    RenderBox::removeChild(oldChild);

    RenderObject* child = prev ? prev : next;
    if (canMergeAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling() && canDropAnonymousBlockChild()) {
        // The removal has knocked us down to containing only a single anonymous
        // box. We can pull the content right back up into our box.
        dropAnonymousBoxChild(*this, downcast<RenderBlock>(*child));
    } else if (((prev && prev->isAnonymousBlock()) || (next && next->isAnonymousBlock())) && canDropAnonymousBlockChild()) {
        // It's possible that the removal has knocked us down to a single anonymous
        // block with floating siblings.
        RenderBlock& anonBlock = downcast<RenderBlock>((prev && prev->isAnonymousBlock()) ? *prev : *next);
        if (canDropAnonymousBlock(anonBlock)) {
            bool dropAnonymousBlock = true;
            for (auto& sibling : childrenOfType<RenderObject>(*this)) {
                if (&sibling == &anonBlock)
                    continue;
                if (!sibling.isFloating()) {
                    dropAnonymousBlock = false;
                    break;
                }
            }
            if (dropAnonymousBlock)
                dropAnonymousBoxChild(*this, anonBlock);
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
            for (RenderObject* current = this; current; current = current->previousInPreOrder(containingBlockIgnoringAnonymous)) {
                if (current->virtualContinuation() != this)
                    continue;

                // Found our previous continuation. We just need to point it to
                // |this|'s next continuation.
                RenderBoxModelObject* nextContinuation = continuation();
                if (is<RenderInline>(*current))
                    downcast<RenderInline>(*current).setContinuation(nextContinuation);
                else if (is<RenderBlock>(*current))
                    downcast<RenderBlock>(*current).setContinuation(nextContinuation);
                else
                    ASSERT_NOT_REACHED();

                break;
            }
            setContinuation(nullptr);
            destroy();
        }
    }
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
        || style().logicalMinHeight().isPositive()
        || style().marginBeforeCollapse() == MSEPARATE || style().marginAfterCollapse() == MSEPARATE)
        return false;

    Length logicalHeightLength = style().logicalHeight();
    bool hasAutoHeight = logicalHeightLength.isAuto();
    if (logicalHeightLength.isPercentOrCalculated() && !document().inQuirksMode()) {
        hasAutoHeight = true;
        for (RenderBlock* cb = containingBlock(); !cb->isRenderView(); cb = cb->containingBlock()) {
            if (cb->style().logicalHeight().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (hasAutoHeight || ((logicalHeightLength.isFixed() || logicalHeightLength.isPercentOrCalculated()) && logicalHeightLength.isZero()))
        return !childrenPreventSelfCollapsing();

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
        updateScrollInfoAfterLayoutTransactionStack() = std::make_unique<DelayedUpdateScrollInfoStack>();
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
    Vector<RenderBlock*> blocksToUpdate;
    copyToVector(transaction->blocks, blocksToUpdate);
    updateScrollInfoAfterLayoutTransactionStack()->removeLast();
    if (updateScrollInfoAfterLayoutTransactionStack()->isEmpty())
        updateScrollInfoAfterLayoutTransactionStack() = nullptr;

    for (auto* block : blocksToUpdate) {
        ASSERT(block->hasOverflowClip());
        block->layer()->updateScrollInfoAfterLayout();
        block->clearLayoutOverflow();
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
    if (!hasOverflowClip())
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

    // Update our first letter info now.
    updateFirstLetter();

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

static RenderBlockRareData* getBlockRareData(const RenderBlock* block)
{
    return gRareDataMap ? gRareDataMap->get(block) : nullptr;
}

static RenderBlockRareData& ensureBlockRareData(const RenderBlock* block)
{
    if (!gRareDataMap)
        gRareDataMap = new RenderBlockRareDataMap;
    
    auto& rareData = gRareDataMap->add(block, nullptr).iterator->value;
    if (!rareData)
        rareData = std::make_unique<RenderBlockRareData>();
    return *rareData.get();
}

void RenderBlock::preparePaginationBeforeBlockLayout(bool& relayoutChildren)
{
    // Regions changing widths can force us to relayout our children.
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (flowThread)
        flowThread->logicalWidthChangedInRegionsForBlock(this, relayoutChildren);
}

bool RenderBlock::recomputeLogicalWidth()
{
    LayoutUnit oldWidth = logicalWidth();
    
    updateLogicalWidth();
    
    bool hasBorderOrPaddingLogicalWidthChanged = this->hasBorderOrPaddingLogicalWidthChanged();
    setHasBorderOrPaddingLogicalWidthChanged(false);

    return oldWidth != logicalWidth() || hasBorderOrPaddingLogicalWidthChanged;
}

void RenderBlock::layoutBlock(bool, LayoutUnit)
{
    ASSERT_NOT_REACHED();
    clearNeedsLayout();
}

void RenderBlock::addOverflowFromChildren()
{
    if (childrenInline())
        addOverflowFromInlineChildren();
    else
        addOverflowFromBlockChildren();
    
    // If this block is flowed inside a flow thread, make sure its overflow is propagated to the containing regions.
    if (m_overflow) {
        if (RenderFlowThread* containingFlowThread = flowThreadContainingBlock())
            containingFlowThread->addRegionsVisualOverflow(this, m_overflow->visualOverflowRect());
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
        LayoutRect clientRect(flippedClientBoxRect());
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

    FloatRect inflatedRect = borderBoxRect();
    theme().adjustRepaintRect(*this, inflatedRect);
    addVisualOverflow(snappedIntRect(LayoutRect(inflatedRect)));

    if (RenderFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->addRegionsVisualOverflowFromTheme(this);
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
    if (relayoutChildren || (child.hasRelativeLogicalHeight() && !isRenderView()))
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
            RenderObject& renderer = *walker.current();
            if (!renderer.isOutOfFlowPositioned() && (renderer.isReplaced() || renderer.isFloating())) {
                RenderBox& box = downcast<RenderBox>(renderer);
                box.layoutIfNeeded();
                if (box.inlineBoxWrapper())
                    lineBoxes.add(&box.inlineBoxWrapper()->root());
            } else if (is<RenderText>(renderer) || (is<RenderInline>(renderer) && !walker.atEndOfInline()))
                renderer.clearNeedsLayout();
        }

        // FIXME: Glyph overflow will get lost in this case, but not really a big deal.
        // FIXME: Find a way to invalidate the knownToHaveNoOverflow flag on the InlineBoxes.
        GlyphOverflowAndFallbackFontsMap textBoxDataMap;                  
        for (auto it = lineBoxes.begin(), end = lineBoxes.end(); it != end; ++it) {
            RootInlineBox* box = *it;
            box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
        }
    } else {
        for (auto* box = firstChildBox(); box; box = box->nextSiblingBox()) {
            if (!box->isOutOfFlowPositioned())
                box->layoutIfNeeded();
        }
    }
}

bool RenderBlock::simplifiedLayout()
{
    if ((!posChildNeedsLayout() && !needsSimplifiedNormalFlowLayout()) || normalChildNeedsLayout() || selfNeedsLayout())
        return false;

    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());
    if (needsPositionedMovementLayout() && !tryLayoutDoingPositionedMovementOnly()) {
        statePusher.pop();
        return false;
    }

    // Lay out positioned descendants or objects that just need to recompute overflow.
    if (needsSimplifiedNormalFlowLayout())
        simplifiedNormalFlowLayout();

    // Make sure a forced break is applied after the content if we are a flow thread in a simplified layout.
    // This ensures the size information is correctly computed for the last auto-height region receiving content.
    if (is<RenderFlowThread>(*this))
        downcast<RenderFlowThread>(*this).applyBreakAfterContent(clientLogicalBottom());

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
    while (o && !is<RenderView>(*o) && o->style().position() != AbsolutePosition)
        o = o->parent();
    if (o->style().position() != AbsolutePosition)
        return;

    auto& box = downcast<RenderBox>(child);
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

void RenderBlock::layoutPositionedObject(RenderBox& r, bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    estimateRegionRangeForBoxChild(r);

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
    if (needsLayout() || !view().layoutState()->isPaginated())
        return;

    if (view().layoutState()->pageLogicalHeightChanged() || (view().layoutState()->pageLogicalHeight() && view().layoutState()->pageLogicalOffset(this, logicalTop()) != pageLogicalOffset()))
        setChildNeedsLayout(MarkOnlyThis);
}

void RenderBlock::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();
    // Check our region range to make sure we need to be painting in this region.
    if (namedFlowFragment && !namedFlowFragment->flowThread()->objectShouldFragmentInFlowRegion(this, namedFlowFragment))
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + location();
    PaintPhase phase = paintInfo.phase;

    // Check if we need to do anything at all.
    // FIXME: Could eliminate the isDocumentElementRenderer() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (!isDocumentElementRenderer()) {
        LayoutRect overflowBox = overflowRectForPaintRejection(namedFlowFragment);
        flipForWritingMode(overflowBox);
        adjustRectWithMaximumOutline(phase, overflowBox);
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
    // z-index. We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if ((phase == PaintPhaseBlockBackground || phase == PaintPhaseChildBlockBackground) && hasOverflowClip() && layer()
        && style().visibility() == VISIBLE && paintInfo.shouldPaintWithinRoot(*this) && !paintInfo.paintRootBackgroundOnly())
        layer()->paintOverflowControls(paintInfo.context(), roundedIntPoint(adjustedPaintOffset), snappedIntRect(paintInfo.rect));
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
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect))
            return;
    }
}

bool RenderBlock::paintChild(RenderBox& child, PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect, PaintBlockType paintType)
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
    if (!child.hasSelfPaintingLayer() && !child.isFloating()) {
        if (paintType == PaintAsInlineBlock)
            child.paintAsInlineBlock(paintInfoForChild, childPoint);
        else
            child.paint(paintInfoForChild, childPoint);
    }

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
    RenderBlock* caretPainter;
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
            frame().selection().paintCaret(paintInfo.context(), paintOffset, paintInfo.rect);
        else
            frame().page()->dragCaretController().paintDragCaret(&frame(), paintInfo.context(), paintOffset, paintInfo.rect);
    }
}

void RenderBlock::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) && style().visibility() == VISIBLE) {
        if (hasBoxDecorations()) {
            bool didClipToRegion = false;
            
            RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();
            if (namedFlowFragment && is<RenderNamedFlowThread>(paintInfo.paintContainer)) {
                // If this box goes beyond the current region, then make sure not to overflow the region.
                // This (overflowing region X altough also fragmented to region X+1) could happen when one of this box's children
                // overflows region X and is an unsplittable element (like an image).
                // The same applies for a box overflowing the top of region X when that box is also fragmented in region X-1.

                paintInfo.context().save();
                didClipToRegion = true;

                paintInfo.context().clip(downcast<RenderNamedFlowThread>(*paintInfo.paintContainer).decorationsClipRectForBoxInNamedFlowFragment(*this, *namedFlowFragment));
            }

            paintBoxDecorations(paintInfo, paintOffset);
            
            if (didClipToRegion)
                paintInfo.context().restore();
        }
    }

    if (paintPhase == PaintPhaseMask && style().visibility() == VISIBLE) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    if (paintPhase == PaintPhaseClippingMask && style().visibility() == VISIBLE) {
        paintClippingMask(paintInfo, paintOffset);
        return;
    }

    // If just painting the root background, then return.
    if (paintInfo.paintRootBackgroundOnly())
        return;

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).
    LayoutPoint scrolledOffset = paintOffset;
    scrolledOffset.move(-scrolledContentOffset());

    // Column rules need to account for scrolling and clipping.
    // FIXME: Clipping of column rules does not work. We will need a separate paint phase for column rules I suspect in order to get
    // clipping correct (since it has to paint as background but is still considered "contents").
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) && style().visibility() == VISIBLE)
        paintColumnRules(paintInfo, scrolledOffset);

    // Done with backgrounds, borders and column rules.
    if (paintPhase == PaintPhaseBlockBackground)
        return;
    
    // 2. paint contents
    if (paintPhase != PaintPhaseSelfOutline)
        paintContents(paintInfo, scrolledOffset);

    // 3. paint selection
    // FIXME: Make this work with multi column layouts.  For now don't fill gaps.
    bool isPrinting = document().printing();
    if (!isPrinting)
        paintSelection(paintInfo, scrolledOffset); // Fill in gaps in selection on lines and between blocks.

    // 4. paint floats.
    if (paintPhase == PaintPhaseFloat || paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip)
        paintFloats(paintInfo, scrolledOffset, paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip);

    // 5. paint outline.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && hasOutline() && style().visibility() == VISIBLE)
        paintOutline(paintInfo, LayoutRect(paintOffset, size()));

    // 6. paint continuation outlines.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseChildOutlines)) {
        RenderInline* inlineCont = inlineElementContinuation();
        if (inlineCont && inlineCont->hasOutline() && inlineCont->style().visibility() == VISIBLE) {
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
    return is<RenderInline>(continuation) ? downcast<RenderInline>(continuation) : nullptr;
}

RenderBlock* RenderBlock::blockElementContinuation() const
{
    RenderBoxModelObject* currentContinuation = continuation();
    if (!currentContinuation || currentContinuation->isInline())
        return nullptr;
    RenderBlock& nextContinuation = downcast<RenderBlock>(*currentContinuation);
    if (nextContinuation.isAnonymousBlock())
        return nextContinuation.blockElementContinuation();
    return &nextContinuation;
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
        
    if (isBody() || isDocumentElementRenderer() || hasOverflowClip()
        || isPositioned() || isFloating()
        || isTableCell() || isInlineBlockOrInlineTable()
        || hasTransform() || hasReflection() || hasMask() || isWritingModeRoot()
        || isRenderFlowThread() || style().columnSpan() == ColumnSpanAll)
        return true;
    
    if (view().selectionUnsplitStart()) {
        Node* startElement = view().selectionUnsplitStart()->node();
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
    LayoutPoint offsetFromRepaintContainer(containerPoint - scrolledContentOffset());

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
            for (RenderBlock* cb = containingBlock(); cb && !cb->isRenderView(); cb = cb->containingBlock())
                clipOutPositionedObjects(paintInfo, LayoutPoint(cb->x(), cb->y()), cb->positionedObjects()); // FIXME: Not right for flipped writing modes.
        }
        clipOutFloatingObjects(rootBlock, paintInfo, rootBlockPhysicalPosition, offsetFromRootBlock);
    }

    // FIXME: overflow: auto/scroll regions need more math here, since painting in the border box is different from painting in the padding box (one is scrolled, the other is
    // fixed).
    GapRects result;
    if (!isRenderBlockFlow()) // FIXME: Make multi-column selection gap filling work someday.
        return result;

    if (hasTransform() || style().columnSpan() == ColumnSpanAll || isInFlowRenderFlowThread()) {
        // FIXME: We should learn how to gap fill multiple columns and transforms eventually.
        lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalHeight();
        lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, logicalHeight(), cache);
        lastLogicalRight = logicalRightSelectionOffset(rootBlock, logicalHeight(), cache);
        return result;
    }
    
    RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();
    if (paintInfo && namedFlowFragment && is<RenderFlowThread>(*paintInfo->paintContainer)) {
        // Make sure the current object is actually flowed into the region being painted.
        if (!downcast<RenderFlowThread>(*paintInfo->paintContainer).objectShouldFragmentInFlowRegion(this, namedFlowFragment))
            return result;
    }

    if (childrenInline())
        result = inlineSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, cache, paintInfo);
    else
        result = blockSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, cache, paintInfo);

    // Fill the vertical gap all the way to the bottom of our block if the selection extends past our block.
    if (&rootBlock == this && (selectionState() != SelectionBoth && selectionState() != SelectionEnd) && !isRubyBase() && !isRubyText()) {
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
        bool fillBlockGaps = (paintsOwnSelection || (curr->canBeSelectionLeaf() && childState != SelectionNone)) && !isRubyBase() && !isRubyText();
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
        } else if (childState != SelectionNone && is<RenderBlock>(*curr)) {
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

LayoutRect RenderBlock::logicalLeftSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    RenderBoxModelObject* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
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

LayoutRect RenderBlock::logicalRightSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    RenderBoxModelObject* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
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
        return nullptr;

    const RenderElement* object = this;
    RenderObject* sibling;
    do {
        sibling = object->previousSibling();
        while (sibling && (!is<RenderBlock>(*sibling) || downcast<RenderBlock>(*sibling).isSelectionRoot()))
            sibling = sibling->previousSibling();

        offset -= LayoutSize(downcast<RenderBlock>(*object).logicalLeft(), downcast<RenderBlock>(*object).logicalTop());
        object = object->parent();
    } while (!sibling && is<RenderBlock>(object) && !downcast<RenderBlock>(*object).isSelectionRoot());

    if (!sibling)
        return nullptr;

    RenderBlock* beforeBlock = downcast<RenderBlock>(sibling);

    offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

    RenderObject* child = beforeBlock->lastChild();
    while (is<RenderBlock>(child)) {
        beforeBlock = downcast<RenderBlock>(child);
        offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
        child = beforeBlock->lastChild();
    }
    return beforeBlock;
}

void RenderBlock::insertIntoTrackedRendererMaps(RenderBox& descendant, TrackedDescendantsMap*& descendantsMap, TrackedContainerMap*& containerMap, bool forceNewEntry)
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
    
    if (forceNewEntry) {
        descendantSet->remove(&descendant);
        containerMap->remove(&descendant);
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
    return nullptr;
}

void RenderBlock::insertPositionedObject(RenderBox& o)
{
    ASSERT(!isAnonymousBlock());

    if (o.isRenderFlowThread())
        return;
    
    insertIntoTrackedRendererMaps(o, gPositionedDescendantsMap, gPositionedContainerMap, isRenderView());
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
    for (RenderObject* child = parent.firstChild(); child; child = child->nextInPreOrder(&parent)) {
        if (!is<RenderBox>(*child))
            continue;
 
        auto& box = downcast<RenderBox>(*child);
        if (!hasPercentHeightDescendant(box))
            continue;

        removePercentHeightDescendant(box);
    }
}

LayoutUnit RenderBlock::textIndentOffset() const
{
    LayoutUnit cw = 0;
    if (style().textIndent().isPercentOrCalculated())
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
    float maxCharWidth = lineGrid->style().fontCascade().primaryFont().maxCharWidth();
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
    float maxCharWidth = lineGrid->style().fontCascade().primaryFont().maxCharWidth();
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
    return RenderBox::avoidsFloats() || style().hasFlowFrom();
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

    RenderFlowThread* flowThread = flowThreadContainingBlock();
    RenderNamedFlowFragment* namedFlowFragment = flowThread ? downcast<RenderNamedFlowFragment>(flowThread->currentRegion()) : nullptr;
    // If we are now searching inside a region, make sure this element
    // is being fragmented into this region.
    if (namedFlowFragment && !flowThread->objectShouldFragmentInFlowRegion(this, namedFlowFragment))
        return false;

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

    if (style().clipPath()) {
        switch (style().clipPath()->type()) {
        case ClipPathOperation::Shape: {
            auto& clipPath = downcast<ShapeClipPathOperation>(*style().clipPath());

            LayoutRect referenceBoxRect;
            switch (clipPath.referenceBox()) {
            case CSSBoxType::MarginBox:
                referenceBoxRect = marginBoxRect();
                break;
            case CSSBoxType::BorderBox:
                referenceBoxRect = borderBoxRect();
                break;
            case CSSBoxType::PaddingBox:
                referenceBoxRect = paddingBoxRect();
                break;
            case CSSBoxType::ContentBox:
                referenceBoxRect = contentBoxRect();
                break;
            case CSSBoxType::BoxMissing:
            case CSSBoxType::Fill:
            case CSSBoxType::Stroke:
            case CSSBoxType::ViewBox:
                referenceBoxRect = borderBoxRect();
            }
            if (!clipPath.pathForReferenceRect(referenceBoxRect).contains(locationInContainer.point() - localOffset, clipPath.windRule()))
                return false;
            break;
        }
        // FIXME: handle Reference/Box
        case ClipPathOperation::Reference:
        case ClipPathOperation::Box:
            break;
        }
    }

    // If we have clipping, then we can't have any spillout.
    bool useOverflowClip = hasOverflowClip() && !hasSelfPaintingLayer();
    bool useClip = (hasControlClip() || useOverflowClip);
    bool checkChildren = !useClip || (hasControlClip() ? locationInContainer.intersects(controlClipRect(adjustedLocation)) : locationInContainer.intersects(overflowClipRect(adjustedLocation, namedFlowFragment, IncludeOverlayScrollbarSize)));
    if (checkChildren) {
        // Hit test descendants first.
        LayoutSize scrolledOffset(localOffset - scrolledContentOffset());

        if (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, toLayoutPoint(scrolledOffset)))
            return true;
        if (hitTestContents(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            return true;
        }
    }

    // Check if the point is outside radii.
    if (!isRenderView() && style().hasBorderRadius()) {
        LayoutRect borderRect = borderBoxRect();
        borderRect.moveBy(adjustedLocation);
        RoundedRect border = style().getRoundedBorderFor(borderRect);
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

bool RenderBlock::hitTestContents(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (childrenInline() && !isTable())
        return hitTestInlineChildren(request, result, locationInContainer, accumulatedOffset, hitTestAction);

    // Hit test our children.
    HitTestAction childHitTest = hitTestAction;
    if (hitTestAction == HitTestChildBlockBackgrounds)
        childHitTest = HitTestChildBlockBackground;
    for (auto* child = lastChildBox(); child; child = child->previousSiblingBox()) {
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
        return ancestor->createVisiblePosition(childElement->computeNodeIndex(), DOWNSTREAM);
    return ancestor->createVisiblePosition(childElement->computeNodeIndex() + 1, UPSTREAM);
}

VisiblePosition RenderBlock::positionForPointWithInlineChildren(const LayoutPoint&, const RenderRegion*)
{
    ASSERT_NOT_REACHED();
    return VisiblePosition();
}

static inline bool isChildHitTestCandidate(const RenderBox& box)
{
    return box.height() && box.style().visibility() == VISIBLE && !box.isFloatingOrOutOfFlowPositioned() && !box.isInFlowRenderFlowThread();
}

// Valid candidates in a FlowThread must be rendered by the region.
static inline bool isChildHitTestCandidate(const RenderBox& box, const RenderRegion* region, const LayoutPoint& point)
{
    if (!isChildHitTestCandidate(box))
        return false;
    if (!region)
        return true;
    const RenderBlock& block = is<RenderBlock>(box) ? downcast<RenderBlock>(box) : *box.containingBlock();
    return block.regionAtBlockOffset(point.y()) == region;
}

VisiblePosition RenderBlock::positionForPoint(const LayoutPoint& point, const RenderRegion* region)
{
    if (isTable())
        return RenderBox::positionForPoint(point, region);

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
        return positionForPointWithInlineChildren(pointInLogicalContents, region);

    RenderBox* lastCandidateBox = lastChildBox();

    if (!region)
        region = regionAtBlockOffset(pointInLogicalContents.y());

    while (lastCandidateBox && !isChildHitTestCandidate(*lastCandidateBox, region, pointInLogicalContents))
        lastCandidateBox = lastCandidateBox->previousSiblingBox();

    bool blocksAreFlipped = style().isFlippedBlocksWritingMode();
    if (lastCandidateBox) {
        if (pointInLogicalContents.y() > logicalTopForChild(*lastCandidateBox)
            || (!blocksAreFlipped && pointInLogicalContents.y() == logicalTopForChild(*lastCandidateBox)))
            return positionForPointRespectingEditingBoundaries(*this, *lastCandidateBox, pointInContents);

        for (auto* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
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
    return RenderBox::positionForPoint(point, region);
}

void RenderBlock::offsetForContents(LayoutPoint& offset) const
{
    offset = flipForWritingMode(offset);
    offset += scrolledContentOffset();
    offset = flipForWritingMode(offset);
}

void RenderBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    ASSERT(!childrenInline());
    
    computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    int scrollbarWidth = intrinsicScrollbarLogicalWidth();
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
    
    // Table layout uses float, fudge the preferred widths to ensure that they can contain the contents.
    if (isTableCell()) {
        m_minPreferredLogicalWidth = m_minPreferredLogicalWidth + LayoutUnit::epsilon();
        m_maxPreferredLogicalWidth = m_maxPreferredLogicalWidth + LayoutUnit::epsilon();
    }
    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    setPreferredLogicalWidthsDirty(false);
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
        if (child->isFloating() || (is<RenderBox>(*child) && downcast<RenderBox>(*child).avoidsFloats())) {
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
        if (is<RenderBox>(*child) && child->isHorizontalWritingMode() != isHorizontalWritingMode()) {
            auto& childBox = downcast<RenderBox>(*child);
            LogicalExtentComputedValues computedValues;
            childBox.computeLogicalHeight(childBox.borderAndPaddingLogicalHeight(), 0, computedValues);
            childMinPreferredLogicalWidth = childMaxPreferredLogicalWidth = computedValues.m_extent;
        } else {
            if (is<RenderBlock>(*child) && !is<RenderTable>(*child)) {
                const Length& computedInlineSize = child->style().logicalWidth();
                if (computedInlineSize.isFitContent() || computedInlineSize.isFillAvailable() || computedInlineSize.isAuto()
                    || computedInlineSize.isPercentOrCalculated()) {
                    // FIXME: we could do a lot better for percents (we're considering them always indefinite)
                    // but we need https://bugs.webkit.org/show_bug.cgi?id=152262 to be fixed first
                    childMinPreferredLogicalWidth = child->minPreferredLogicalWidth();
                    childMaxPreferredLogicalWidth = child->maxPreferredLogicalWidth();
                } else {
                    ASSERT(computedInlineSize.isMinContent() || computedInlineSize.isMaxContent() || computedInlineSize.isFixed());
                    LogicalExtentComputedValues computedValues;
                    downcast<RenderBlock>(*child).computeLogicalWidthInRegion(computedValues);
                    childMinPreferredLogicalWidth = childMaxPreferredLogicalWidth = computedValues.m_extent;
                    child->setPreferredLogicalWidthsDirty(false);
                }
            } else {
                // FIXME: we leave the original implementation as default fallback. Still need to specialcase
                // some other situations: https://drafts.csswg.org/css-sizing/#intrinsic
                childMinPreferredLogicalWidth = child->minPreferredLogicalWidth();
                childMaxPreferredLogicalWidth = child->maxPreferredLogicalWidth();
            }
        }

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
    // Anonymous inline blocks don't include margins or any real line height.
    if (isAnonymousInlineBlock() && linePositionMode == PositionOnContainingLine)
        return direction == HorizontalLine ? height() : width();
    
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && linePositionMode == PositionOnContainingLine)
        return RenderBox::lineHeight(firstLine, direction, linePositionMode);

    if (firstLine && view().usesFirstLineRules()) {
        RenderStyle& s = firstLine ? firstLineStyle() : style();
        if (&s != &style())
            return s.computedLineHeight();
    }
    
    return style().computedLineHeight();
}

int RenderBlock::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && linePositionMode == PositionOnContainingLine) {
        if (isAnonymousInlineBlock())
            return direction == HorizontalLine ? height() : width();
        
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        // FIXME: Need to patch form controls to deal with vertical lines.
        if (style().hasAppearance() && !theme().isControlContainer(style().appearance()))
            return theme().baselinePosition(*this);
            
        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow.  We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves).  This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved).
        bool ignoreBaseline = (layer() && (layer()->marquee() || (direction == HorizontalLine ? (layer()->verticalScrollbar() || layer()->scrollOffset().y() != 0)
            : (layer()->horizontalScrollbar() || layer()->scrollOffset().x() != 0)))) || (isWritingModeRoot() && !isRubyRun());
        
        Optional<int> baselinePos = ignoreBaseline ? Optional<int>() : inlineBlockBaseline(direction);
        
        if (isDeprecatedFlexibleBox()) {
            // Historically, we did this check for all baselines. But we can't
            // remove this code from deprecated flexbox, because it effectively
            // breaks -webkit-line-clamp, which is used in the wild -- we would
            // calculate the baseline as if -webkit-line-clamp wasn't used.
            // For simplicity, we use this for all uses of deprecated flexbox.
            LayoutUnit bottomOfContent = direction == HorizontalLine ? borderTop() + paddingTop() + contentHeight() : borderRight() + paddingRight() + contentWidth();
            if (baselinePos && baselinePos.value() > bottomOfContent)
                baselinePos = Optional<int>();
        }
        if (baselinePos)
            return direction == HorizontalLine ? marginTop() + baselinePos.value() : marginRight() + baselinePos.value();

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

Optional<int> RenderBlock::firstLineBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun())
        return Optional<int>();

    for (RenderBox* curr = firstChildBox(); curr; curr = curr->nextSiblingBox()) {
        if (!curr->isFloatingOrOutOfFlowPositioned()) {
            if (Optional<int> result = curr->firstLineBaseline())
                return Optional<int>(curr->logicalTop() + result.value()); // Translate to our coordinate space.
        }
    }

    return Optional<int>();
}

Optional<int> RenderBlock::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    if (isWritingModeRoot() && !isRubyRun())
        return Optional<int>();

    bool haveNormalFlowChild = false;
    for (auto* box = lastChildBox(); box; box = box->previousSiblingBox()) {
        if (box->isFloatingOrOutOfFlowPositioned())
            continue;
        haveNormalFlowChild = true;
        if (Optional<int> result = box->inlineBlockBaseline(lineDirection))
            return Optional<int>(box->logicalTop() + result.value()); // Translate to our coordinate space.
    }

    if (!haveNormalFlowChild && hasLineIfEmpty()) {
        auto& fontMetrics = firstLineStyle().fontMetrics();
        return Optional<int>(fontMetrics.ascent()
            + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
            + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight()));
    }

    return Optional<int>();
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

RenderBlock* RenderBlock::firstLineBlock() const
{
    RenderBlock* firstLineBlock = const_cast<RenderBlock*>(this);
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style().hasPseudoStyle(FIRST_LINE);
        if (hasPseudo)
            break;
        RenderElement* parentBlock = firstLineBlock->parent();
        if (firstLineBlock->isReplaced() || firstLineBlock->isFloating()
            || !parentBlock || parentBlock->firstChild() != firstLineBlock || !isRenderBlockFlowOrRenderButton(*parentBlock))
            break;
        firstLineBlock = downcast<RenderBlock>(parentBlock);
    } 
    
    if (!hasPseudo)
        return nullptr;
    
    return firstLineBlock;
}

static RenderStyle& styleForFirstLetter(RenderElement* firstLetterBlock, RenderObject* firstLetterContainer)
{
    RenderStyle* pseudoStyle = firstLetterBlock->getCachedPseudoStyle(FIRST_LETTER, &firstLetterContainer->firstLineStyle());
    
    // If we have an initial letter drop that is >= 1, then we need to force floating to be on.
    if (pseudoStyle->initialLetterDrop() >= 1 && !pseudoStyle->isFloating())
        pseudoStyle->setFloating(pseudoStyle->isLeftToRightDirection() ? LeftFloat : RightFloat);

    // We have to compute the correct font-size for the first-letter if it has an initial letter height set.
    RenderObject* paragraph = firstLetterContainer->isRenderBlockFlow() ? firstLetterContainer : firstLetterContainer->containingBlock();
    if (pseudoStyle->initialLetterHeight() >= 1 && pseudoStyle->fontMetrics().hasCapHeight() && paragraph->style().fontMetrics().hasCapHeight()) {
        // FIXME: For ideographic baselines, we want to go from line edge to line edge. This is equivalent to (N-1)*line-height + the font height.
        // We don't yet support ideographic baselines.
        // For an N-line first-letter and for alphabetic baselines, the cap-height of the first letter needs to equal (N-1)*line-height of paragraph lines + cap-height of the paragraph
        // Mathematically we can't rely on font-size, since font().height() doesn't necessarily match. For reliability, the best approach is simply to
        // compare the final measured cap-heights of the two fonts in order to get to the closest possible value.
        pseudoStyle->setLineBoxContain(LineBoxContainInitialLetter);
        int lineHeight = paragraph->style().computedLineHeight();
        
        // Set the font to be one line too big and then ratchet back to get to a precise fit. We can't just set the desired font size based off font height metrics
        // because many fonts bake ascent into the font metrics. Therefore we have to look at actual measured cap height values in order to know when we have a good fit.
        auto newFontDescription = pseudoStyle->fontDescription();
        float capRatio = pseudoStyle->fontMetrics().floatCapHeight() / pseudoStyle->fontSize();
        float startingFontSize = ((pseudoStyle->initialLetterHeight() - 1) * lineHeight + paragraph->style().fontMetrics().capHeight()) / capRatio;
        newFontDescription.setSpecifiedSize(startingFontSize);
        newFontDescription.setComputedSize(startingFontSize);
        pseudoStyle->setFontDescription(newFontDescription);
        pseudoStyle->fontCascade().update(pseudoStyle->fontCascade().fontSelector());
        
        int desiredCapHeight = (pseudoStyle->initialLetterHeight() - 1) * lineHeight + paragraph->style().fontMetrics().capHeight();
        int actualCapHeight = pseudoStyle->fontMetrics().capHeight();
        while (actualCapHeight > desiredCapHeight) {
            auto newFontDescription = pseudoStyle->fontDescription();
            newFontDescription.setSpecifiedSize(newFontDescription.specifiedSize() - 1);
            newFontDescription.setComputedSize(newFontDescription.computedSize() -1);
            pseudoStyle->setFontDescription(newFontDescription);
            pseudoStyle->fontCascade().update(pseudoStyle->fontCascade().fontSelector());
            actualCapHeight = pseudoStyle->fontMetrics().capHeight();
        }
    }
    
    // Force inline display (except for floating first-letters).
    pseudoStyle->setDisplay(pseudoStyle->isFloating() ? BLOCK : INLINE);
    // CSS2 says first-letter can't be positioned.
    pseudoStyle->setPosition(StaticPosition);
    return *pseudoStyle;
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
        bool canHaveFirstLetterRenderer = firstLetterBlock->style().hasPseudoStyle(FIRST_LETTER)
            && firstLetterBlock->canHaveGeneratedChildren()
            && isRenderBlockFlowOrRenderButton(*firstLetterBlock);
        if (canHaveFirstLetterRenderer)
            return firstLetterBlock;

        RenderElement* parentBlock = firstLetterBlock->parent();
        if (firstLetterBlock->isReplaced() || !parentBlock || parentBlock->firstChild() != firstLetterBlock
            || !isRenderBlockFlowOrRenderButton(*parentBlock))
            return nullptr;
        firstLetterBlock = downcast<RenderBlock>(parentBlock);
    } 

    return nullptr;
}

void RenderBlock::updateFirstLetterStyle(RenderElement* firstLetterBlock, RenderObject* currentChild)
{
    RenderElement* firstLetter = currentChild->parent();
    RenderElement* firstLetterContainer = firstLetter->parent();
    RenderStyle& pseudoStyle = styleForFirstLetter(firstLetterBlock, firstLetterContainer);
    ASSERT(firstLetter->isFloating() || firstLetter->isInline());

    if (Style::determineChange(firstLetter->style(), pseudoStyle) == Style::Detach) {
        // The first-letter renderer needs to be replaced. Create a new renderer of the right type.
        RenderBoxModelObject* newFirstLetter;
        if (pseudoStyle.display() == INLINE)
            newFirstLetter = new RenderInline(document(), pseudoStyle);
        else
            newFirstLetter = new RenderBlockFlow(document(), pseudoStyle);
        newFirstLetter->initializeStyle();

        // Move the first letter into the new renderer.
        LayoutStateDisabler layoutStateDisabler(view());
        while (RenderObject* child = firstLetter->firstChild()) {
            if (is<RenderText>(*child))
                downcast<RenderText>(*child).removeAndDestroyTextBoxes();
            firstLetter->removeChild(*child);
            newFirstLetter->addChild(child, nullptr);
        }

        RenderObject* nextSibling = firstLetter->nextSibling();
        if (RenderTextFragment* remainingText = downcast<RenderBoxModelObject>(*firstLetter).firstLetterRemainingText()) {
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
        firstLetter->setStyle(pseudoStyle);
}

void RenderBlock::createFirstLetterRenderer(RenderElement* firstLetterBlock, RenderText* currentTextChild)
{
    RenderElement* firstLetterContainer = currentTextChild->parent();
    RenderStyle& pseudoStyle = styleForFirstLetter(firstLetterBlock, firstLetterContainer);
    RenderBoxModelObject* firstLetter = nullptr;
    if (pseudoStyle.display() == INLINE)
        firstLetter = new RenderInline(document(), pseudoStyle);
    else
        firstLetter = new RenderBlockFlow(document(), pseudoStyle);
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

        // Account for first grapheme cluster.
        length += numCharactersInGraphemeClusters(StringView(oldText).substring(length), 1);
        
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
    
void RenderBlock::getFirstLetter(RenderObject*& firstLetter, RenderElement*& firstLetterContainer, RenderObject* skipObject)
{
    firstLetter = nullptr;
    firstLetterContainer = nullptr;

    if (!view().usesFirstLetterRules())
        return;

    // Don't recur
    if (style().styleType() == FIRST_LETTER)
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
            if (current.style().styleType() == FIRST_LETTER) {
                firstLetter = current.firstChild();
                break;
            }
            firstLetter = current.nextSibling();
        } else if (current.isReplaced() || is<RenderButton>(current) || is<RenderMenuList>(current))
            break;
        else if (current.isFlexibleBoxIncludingDeprecated()
#if ENABLE(CSS_GRID_LAYOUT)
            || current.isRenderGrid()
#endif
            )
            firstLetter = current.nextSibling();
        else if (current.style().hasPseudoStyle(FIRST_LETTER) && current.canHaveGeneratedChildren())  {
            // We found a lower-level node with first-letter, which supersedes the higher-level style
            firstLetterContainer = &current;
            firstLetter = current.firstChild();
        } else
            firstLetter = current.firstChild();
    }
    
    if (!firstLetter)
        firstLetterContainer = nullptr;
}

void RenderBlock::updateFirstLetter()
{
    RenderObject* firstLetterObj;
    RenderElement* firstLetterContainer;
    // FIXME: The first letter might be composed of a variety of code units, and therefore might
    // be contained within multiple RenderElements.
    getFirstLetter(firstLetterObj, firstLetterContainer);

    if (!firstLetterObj || !firstLetterContainer)
        return;

    // If the child already has style, then it has already been created, so we just want
    // to update it.
    if (firstLetterObj->parent()->style().styleType() == FIRST_LETTER) {
        updateFirstLetterStyle(firstLetterContainer, firstLetterObj);
        return;
    }

    if (!is<RenderText>(*firstLetterObj))
        return;

    // Our layout state is not valid for the repaints we are going to trigger by
    // adding and removing children of firstLetterContainer.
    LayoutStateDisabler layoutStateDisabler(view());

    createFirstLetterRenderer(firstLetterContainer, downcast<RenderText>(firstLetterObj));
}

RenderFlowThread* RenderBlock::cachedFlowThreadContainingBlock() const
{
    RenderBlockRareData* rareData = getBlockRareData(this);

    if (!rareData || !rareData->m_flowThreadContainingBlock)
        return nullptr;

    return rareData->m_flowThreadContainingBlock.value();
}

bool RenderBlock::cachedFlowThreadContainingBlockNeedsUpdate() const
{
    RenderBlockRareData* rareData = getBlockRareData(this);

    if (!rareData || !rareData->m_flowThreadContainingBlock)
        return true;

    return false;
}

void RenderBlock::setCachedFlowThreadContainingBlockNeedsUpdate()
{
    RenderBlockRareData& rareData = ensureBlockRareData(this);
    rareData.m_flowThreadContainingBlock = Nullopt;
}

RenderFlowThread* RenderBlock::updateCachedFlowThreadContainingBlock(RenderFlowThread* flowThread) const
{
    RenderBlockRareData& rareData = ensureBlockRareData(this);
    rareData.m_flowThreadContainingBlock = flowThread;

    return flowThread;
}

RenderFlowThread* RenderBlock::locateFlowThreadContainingBlock() const
{
    RenderBlockRareData* rareData = getBlockRareData(this);
    if (!rareData || !rareData->m_flowThreadContainingBlock)
        return updateCachedFlowThreadContainingBlock(RenderBox::locateFlowThreadContainingBlock());

    ASSERT(rareData->m_flowThreadContainingBlock.value() == RenderBox::locateFlowThreadContainingBlock());
    return rareData->m_flowThreadContainingBlock.value();
}

LayoutUnit RenderBlock::paginationStrut() const
{
    RenderBlockRareData* rareData = getBlockRareData(this);
    return rareData ? rareData->m_paginationStrut : LayoutUnit();
}

LayoutUnit RenderBlock::pageLogicalOffset() const
{
    RenderBlockRareData* rareData = getBlockRareData(this);
    return rareData ? rareData->m_pageLogicalOffset : LayoutUnit();
}

void RenderBlock::setPaginationStrut(LayoutUnit strut)
{
    RenderBlockRareData* rareData = getBlockRareData(this);
    if (!rareData) {
        if (!strut)
            return;
        rareData = &ensureBlockRareData(this);
    }
    rareData->m_paginationStrut = strut;
}

void RenderBlock::setPageLogicalOffset(LayoutUnit logicalOffset)
{
    RenderBlockRareData* rareData = getBlockRareData(this);
    if (!rareData) {
        if (!logicalOffset)
            return;
        rareData = &ensureBlockRareData(this);
    }
    rareData->m_pageLogicalOffset = logicalOffset;
}

void RenderBlock::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    if (isAnonymousBlockContinuation()) {
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        rects.append(snappedIntRect(accumulatedOffset.x(), accumulatedOffset.y() - collapsedMarginBefore(),
                                width(), height() + collapsedMarginBefore() + collapsedMarginAfter()));
        continuation()->absoluteRects(rects, accumulatedOffset - toLayoutSize(location() +
                inlineElementContinuation()->containingBlock()->location()));
    } else
        rects.append(snappedIntRect(accumulatedOffset, size()));
}

void RenderBlock::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    FloatRect localRect = isAnonymousBlockContinuation() 
        ? FloatRect(0, -collapsedMarginBefore(), width(), height() + collapsedMarginBefore() + collapsedMarginAfter())
        : FloatRect(0, 0, width(), height());
    
    // FIXME: This is wrong for block-flows that are horizontal.
    // https://bugs.webkit.org/show_bug.cgi?id=46781
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread || !flowThread->absoluteQuadsForBox(quads, wasFixed, this, localRect.y(), localRect.maxY()))
        quads.append(localToAbsoluteQuad(localRect, UseTransforms, wasFixed));

    if (isAnonymousBlockContinuation())
        continuation()->absoluteQuads(quads, wasFixed);
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

void RenderBlock::childBecameNonInline(RenderElement&)
{
    makeChildrenNonInline();
    if (isAnonymousBlock() && is<RenderBlock>(parent()))
        downcast<RenderBlock>(*parent()).removeLeftoverAnonymousBlock(this);
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
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    if (inlineElementContinuation()) {
        // FIXME: This check really isn't accurate. 
        bool nextInlineHasLineBox = inlineElementContinuation()->firstLineBox();
        // FIXME: This is wrong. The principal renderer may not be the continuation preceding this block.
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = downcast<RenderInline>(*inlineElementContinuation()->element()->renderer()).firstLineBox();
        float topMargin = prevInlineHasLineBox ? collapsedMarginBefore() : LayoutUnit();
        float bottomMargin = nextInlineHasLineBox ? collapsedMarginAfter() : LayoutUnit();
        LayoutRect rect(additionalOffset.x(), additionalOffset.y() - topMargin, width(), height() + topMargin + bottomMargin);
        if (!rect.isEmpty())
            rects.append(snappedIntRect(rect));
    } else if (width() && height())
        rects.append(snappedIntRect(additionalOffset, size()));

    if (!hasOverflowClip() && !hasControlClip()) {
        if (childrenInline())
            addFocusRingRectsForInlineChildren(rects, additionalOffset, paintContainer);
    
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if (!is<RenderText>(*child) && !is<RenderListMarker>(*child) && is<RenderBox>(*child)) {
                auto& box = downcast<RenderBox>(*child);
                FloatPoint pos;
                // FIXME: This doesn't work correctly with transforms.
                if (box.layer())
                    pos = child->localToContainerPoint(FloatPoint(), paintContainer);
                else
                    pos = FloatPoint(additionalOffset.x() + box.x(), additionalOffset.y() + box.y());
                box.addFocusRingRects(rects, flooredLayoutPoint(pos), paintContainer);
            }
        }
    }

    if (inlineElementContinuation())
        inlineElementContinuation()->addFocusRingRects(rects, flooredLayoutPoint(LayoutPoint(additionalOffset + inlineElementContinuation()->containingBlock()->location() - location())), paintContainer);
}

RenderBox* RenderBlock::createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const
{
    return createAnonymousWithParentRendererAndDisplay(parent, style().display());
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
    ASSERT(!childBox.isRenderNamedFlowThread());

    if (!flowThreadContainingBlock)
        return false;

    if (!flowThreadContainingBlock->hasRegions())
        return false;

    if (!childBox.canHaveOutsideRegionRange())
        return false;

    return flowThreadContainingBlock->hasCachedRegionRangeForBox(parentBlock);
}

bool RenderBlock::childBoxIsUnsplittableForFragmentation(const RenderBox& child) const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool checkColumnBreaks = flowThread && flowThread->shouldCheckColumnBreaks();
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


    // Changing the start region means we shift everything and a relayout is needed.
    if (newStartRegion != startRegion)
        return true;

    // The region range of the box has changed. Some boxes (e.g floats) may have been positioned assuming
    // a different range.
    if (box.needsLayoutAfterRegionRangeChange() && newEndRegion != endRegion)
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

const char* RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"; // FIXME: Temporary hack until we know that the regression tests pass.

    if (isFloating())
        return "RenderBlock (floating)";
    if (isOutOfFlowPositioned())
        return "RenderBlock (positioned)";
    if (isAnonymousBlock())
        return "RenderBlock (anonymous)";
    if (isAnonymousInlineBlock())
        return "RenderBlock (anonymous inline-block)";
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

TextRun RenderBlock::constructTextRun(RenderObject* context, const FontCascade& font, StringView stringView, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    TextDirection textDirection = LTR;
    bool directionalOverride = style.rtlOrdering() == VisualOrder;
    if (flags != DefaultTextRunFlags) {
        if (flags & RespectDirection)
            textDirection = style.direction();
        if (flags & RespectDirectionOverride)
            directionalOverride |= isOverride(style.unicodeBidi());
    }
    TextRun run(stringView, 0, 0, expansion, textDirection, directionalOverride);
    if (font.primaryFont().isSVGFont()) {
        ASSERT(context); // FIXME: Thread a RenderObject& to this point so we don't have to dereference anything.
        run.setRenderingContext(SVGTextRunRenderingContext::create(*context));
    }

    return run;
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const FontCascade& font, const String& string, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    return constructTextRun(context, font, StringView(string), style, expansion, flags);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const FontCascade& font, const RenderText* text, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(context, font, text->stringView(), style, expansion);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const FontCascade& font, const RenderText* text, unsigned offset, unsigned length, const RenderStyle& style, ExpansionBehavior expansion)
{
    unsigned stop = offset + length;
    ASSERT(stop <= text->textLength());
    return constructTextRun(context, font, text->stringView(offset, stop), style, expansion);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const FontCascade& font, const LChar* characters, int length, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(context, font, StringView(characters, length), style, expansion);
}

TextRun RenderBlock::constructTextRun(RenderObject* context, const FontCascade& font, const UChar* characters, int length, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(context, font, StringView(characters, length), style, expansion);
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

#endif

} // namespace WebCore
