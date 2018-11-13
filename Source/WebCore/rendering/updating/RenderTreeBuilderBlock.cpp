/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderTreeBuilderBlock.h"

#include "RenderButton.h"
#include "RenderChildIterator.h"
#include "RenderFullScreen.h"
#include "RenderMultiColumnFlow.h"
#include "RenderRuby.h"
#include "RenderRubyRun.h"
#include "RenderTextControl.h"
#include "RenderTreeBuilderMultiColumn.h"

namespace WebCore {

static void moveAllChildrenToInternal(RenderBoxModelObject& from, RenderElement& newParent)
{
    while (from.firstChild())
        newParent.attachRendererInternal(from.detachRendererInternal(*from.firstChild()), &from);
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
    ASSERT(!oldChild.renderTreeBeingDestroyed());

    if (oldChild.isInline())
        return false;

    if (is<RenderBoxModelObject>(oldChild) && downcast<RenderBoxModelObject>(oldChild).continuation())
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

static RenderBlock* continuationBefore(RenderBlock& parent, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == &parent)
        return &parent;

    RenderBlock* nextToLast = &parent;
    RenderBlock* last = &parent;
    for (auto* current = downcast<RenderBlock>(parent.continuation()); current; current = downcast<RenderBlock>(current->continuation())) {
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

RenderTreeBuilder::Block::Block(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::Block::attach(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (parent.continuation() && !parent.isAnonymousBlock())
        insertChildToContinuation(parent, WTFMove(child), beforeChild);
    else
        attachIgnoringContinuation(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::Block::insertChildToContinuation(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    RenderBlock* flow = continuationBefore(parent, beforeChild);
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

    if (child->isFloatingOrOutOfFlowPositioned()) {
        m_builder.attachIgnoringContinuation(*beforeChildParent, WTFMove(child), beforeChild);
        return;
    }

    bool childIsNormal = child->isInline() || child->style().columnSpan() == ColumnSpan::None;
    bool bcpIsNormal = beforeChildParent->isInline() || beforeChildParent->style().columnSpan() == ColumnSpan::None;
    bool flowIsNormal = flow->isInline() || flow->style().columnSpan() == ColumnSpan::None;

    if (flow == beforeChildParent) {
        m_builder.attachIgnoringContinuation(*flow, WTFMove(child), beforeChild);
        return;
    }

    // The goal here is to match up if we can, so that we can coalesce and create the
    // minimal # of continuations needed for the inline.
    if (childIsNormal == bcpIsNormal) {
        m_builder.attachIgnoringContinuation(*beforeChildParent, WTFMove(child), beforeChild);
        return;
    }
    if (flowIsNormal == childIsNormal) {
        m_builder.attachIgnoringContinuation(*flow, WTFMove(child)); // Just treat like an append.
        return;
    }
    m_builder.attachIgnoringContinuation(*beforeChildParent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::Block::attachIgnoringContinuation(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != &parent) {
        RenderElement* beforeChildContainer = beforeChild->parent();
        while (beforeChildContainer->parent() != &parent)
            beforeChildContainer = beforeChildContainer->parent();
        ASSERT(beforeChildContainer);

        if (beforeChildContainer->isAnonymous()) {
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!beforeChildContainer->isInline());

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
                if (child->isInline() || beforeChild->parent()->firstChild() != beforeChild)
                    m_builder.attach(*beforeChild->parent(), WTFMove(child), beforeChild);
                else
                    m_builder.attach(parent, WTFMove(child), beforeChild->parent());
                return;
            }

            ASSERT(beforeChildAnonymousContainer->isTable());

            if (child->isTablePart()) {
                // Insert into the anonymous table.
                m_builder.attach(*beforeChildAnonymousContainer, WTFMove(child), beforeChild);
                return;
            }

            beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);

            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(beforeChild->parent() == &parent);
        }
    }

    bool madeBoxesNonInline = false;

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes.
    if (parent.childrenInline() && !child->isInline() && !child->isFloatingOrOutOfFlowPositioned()) {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        m_builder.makeChildrenNonInline(parent, beforeChild);
        madeBoxesNonInline = true;

        if (beforeChild && beforeChild->parent() != &parent) {
            beforeChild = beforeChild->parent();
            ASSERT(beforeChild->isAnonymousBlock());
            ASSERT(beforeChild->parent() == &parent);
        }
    } else if (!parent.childrenInline() && (child->isFloatingOrOutOfFlowPositioned() || child->isInline())) {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : parent.lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            m_builder.attach(downcast<RenderBlock>(*afterChild), WTFMove(child));
            return;
        }

        if (child->isInline()) {
            // No suitable existing anonymous box - create a new one.
            auto newBox = parent.createAnonymousBlock();
            auto& box = *newBox;
            m_builder.attachToRenderElement(parent, WTFMove(newBox), beforeChild);
            m_builder.attach(box, WTFMove(child));
            return;
        }
    }

    parent.invalidateLineLayoutPath();

    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);

    if (madeBoxesNonInline && is<RenderBlock>(parent.parent()) && parent.isAnonymousBlock())
        removeLeftoverAnonymousBlock(parent);
    // parent object may be dead here
}

void RenderTreeBuilder::Block::childBecameNonInline(RenderBlock& parent, RenderElement&)
{
    m_builder.makeChildrenNonInline(parent);
    if (parent.isAnonymousBlock() && is<RenderBlock>(parent.parent()))
        removeLeftoverAnonymousBlock(parent);
    // parent may be dead here
}

void RenderTreeBuilder::Block::removeLeftoverAnonymousBlock(RenderBlock& anonymousBlock)
{
    ASSERT(anonymousBlock.isAnonymousBlock());
    ASSERT(!anonymousBlock.childrenInline());
    ASSERT(anonymousBlock.parent());

    if (anonymousBlock.continuation())
        return;

    auto* parent = anonymousBlock.parent();
    if (is<RenderButton>(*parent) || is<RenderTextControl>(*parent) || is<RenderRubyAsBlock>(*parent) || is<RenderRubyRun>(*parent))
        return;

    // FIXME: This should really just be a moveAllChilrenTo (see webkit.org/b/182495)
    moveAllChildrenToInternal(anonymousBlock, *parent);
    auto toBeDestroyed = m_builder.detachFromRenderElement(*parent, anonymousBlock);
    // anonymousBlock is dead here.
}

RenderPtr<RenderObject> RenderTreeBuilder::Block::detach(RenderBlock& parent, RenderObject& oldChild, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    // No need to waste time in merging or removing empty anonymous blocks.
    // We can just bail out if our document is getting destroyed.
    if (parent.renderTreeBeingDestroyed())
        return m_builder.detachFromRenderElement(parent, oldChild);

    // If this child is a block, and if our previous and next siblings are both anonymous blocks
    // with inline content, then we can fold the inline content back together.
    auto prev = makeWeakPtr(oldChild.previousSibling());
    auto next = makeWeakPtr(oldChild.nextSibling());
    bool canMergeAnonymousBlocks = canMergeContiguousAnonymousBlocks(oldChild, prev.get(), next.get());

    parent.invalidateLineLayoutPath();

    auto takenChild = m_builder.detachFromRenderElement(parent, oldChild);

    if (canMergeAnonymousBlocks && prev && next) {
        prev->setNeedsLayoutAndPrefWidthsRecalc();
        RenderBlock& nextBlock = downcast<RenderBlock>(*next);
        RenderBlock& prevBlock = downcast<RenderBlock>(*prev);

        if (prev->childrenInline() != next->childrenInline()) {
            RenderBlock& inlineChildrenBlock = prev->childrenInline() ? prevBlock : nextBlock;
            RenderBlock& blockChildrenBlock = prev->childrenInline() ? nextBlock : prevBlock;

            // Place the inline children block inside of the block children block instead of deleting it.
            // In order to reuse it, we have to reset it to just be a generic anonymous block. Make sure
            // to clear out inherited column properties by just making a new style, and to also clear the
            // column span flag if it is set.
            ASSERT(!inlineChildrenBlock.continuation());
            // Cache this value as it might get changed in setStyle() call.
            inlineChildrenBlock.setStyle(RenderStyle::createAnonymousStyleWithDisplay(parent.style(), DisplayType::Block));
            auto blockToMove = m_builder.detachFromRenderElement(parent, inlineChildrenBlock);

            // Now just put the inlineChildrenBlock inside the blockChildrenBlock.
            RenderObject* beforeChild = prev == &inlineChildrenBlock ? blockChildrenBlock.firstChild() : nullptr;
            m_builder.attachToRenderElementInternal(blockChildrenBlock, WTFMove(blockToMove), beforeChild);
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
            m_builder.moveAllChildrenIncludingFloats(nextBlock, prevBlock, RenderTreeBuilder::NormalizeAfterInsertion::No);

            // Delete the now-empty block's lines and nuke it.
            nextBlock.deleteLines();
            m_builder.destroy(nextBlock);
        }
    }

    if (canCollapseAnonymousBlock == CanCollapseAnonymousBlock::Yes && parent.canDropAnonymousBlockChild()) {
        RenderObject* child = prev ? prev.get() : next.get();
        if (canMergeAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling()) {
            // The removal has knocked us down to containing only a single anonymous box. We can pull the content right back up into our box.
            dropAnonymousBoxChild(parent, downcast<RenderBlock>(*child));
        } else if ((prev && prev->isAnonymousBlock()) || (next && next->isAnonymousBlock())) {
            // It's possible that the removal has knocked us down to a single anonymous block with floating siblings.
            RenderBlock& anonBlock = downcast<RenderBlock>((prev && prev->isAnonymousBlock()) ? *prev : *next);
            if (canDropAnonymousBlock(anonBlock)) {
                bool dropAnonymousBlock = true;
                for (auto& sibling : childrenOfType<RenderObject>(parent)) {
                    if (&sibling == &anonBlock)
                        continue;
                    if (!sibling.isFloating()) {
                        dropAnonymousBlock = false;
                        break;
                    }
                }
                if (dropAnonymousBlock)
                    dropAnonymousBoxChild(parent, anonBlock);
            }
        }
    }

    if (!parent.firstChild()) {
        // If this was our last child be sure to clear out our line boxes.
        if (parent.childrenInline())
            parent.deleteLines();
    }
    return takenChild;
}

void RenderTreeBuilder::Block::dropAnonymousBoxChild(RenderBlock& parent, RenderBlock& child)
{
    parent.setNeedsLayoutAndPrefWidthsRecalc();
    parent.setChildrenInline(child.childrenInline());
    auto* nextSibling = child.nextSibling();

    auto toBeDeleted = m_builder.detachFromRenderElement(parent, child);
    m_builder.moveAllChildren(child, parent, nextSibling, RenderTreeBuilder::NormalizeAfterInsertion::No);
    // Delete the now-empty block's lines and nuke it.
    child.deleteLines();
}

RenderPtr<RenderObject> RenderTreeBuilder::Block::detach(RenderBlockFlow& parent, RenderObject& child, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    if (!parent.renderTreeBeingDestroyed()) {
        auto* fragmentedFlow = parent.multiColumnFlow();
        if (fragmentedFlow && fragmentedFlow != &child)
            m_builder.multiColumnBuilder().multiColumnRelativeWillBeRemoved(*fragmentedFlow, child);
    }
    return detach(static_cast<RenderBlock&>(parent), child, canCollapseAnonymousBlock);
}

}
