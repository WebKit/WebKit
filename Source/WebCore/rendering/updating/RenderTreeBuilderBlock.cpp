/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
#include "RenderMultiColumnFlow.h"
#include "RenderStyleInlines.h"
#include "RenderTextControl.h"
#include "RenderTreeBuilderMultiColumn.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(RenderTreeBuilder, Block);

static void moveAllChildrenToInternal(RenderBoxModelObject& from, RenderElement& newParent)
{
    while (from.firstChild())
        newParent.attachRendererInternal(from.detachRendererInternal(*from.firstChild()), &from);
}

static bool canDropAnonymousBlock(const RenderBlock& anonymousBlock)
{
    if (anonymousBlock.beingDestroyed() || anonymousBlock.continuation())
        return false;
    return true;
}

static bool canMergeContiguousAnonymousBlocks(const RenderObject& rendererToBeRemoved, const RenderObject* previous, const RenderObject* next, const RenderObject* anonymousDestroyRoot)
{
    ASSERT(!rendererToBeRemoved.renderTreeBeingDestroyed());

    if (rendererToBeRemoved.isInline())
        return false;

    if (previous && (!previous->isAnonymousBlock() || !canDropAnonymousBlock(downcast<RenderBlock>(*previous))))
        return false;

    if (next && (!next->isAnonymousBlock() || !canDropAnonymousBlock(downcast<RenderBlock>(*next))))
        return false;

    auto* boxToBeRemoved = dynamicDowncast<RenderBoxModelObject>(rendererToBeRemoved);
    if (!boxToBeRemoved || !boxToBeRemoved->continuation())
        return true;

    // Let's merge pre and post anonymous block containers when the continuation triggering box (rendererToBeRemoved) is going away.
    return previous && next && previous != anonymousDestroyRoot && next != anonymousDestroyRoot;
}

RenderBlock* RenderTreeBuilder::Block::continuationBefore(RenderBlock& parent, RenderObject* beforeChild)
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

struct ParentAndBeforeChild {
    RenderElement* parent { nullptr };
    RenderObject* beforeChild { nullptr };
};
static std::optional<ParentAndBeforeChild> findParentAndBeforeChildForNonSibling(RenderBlock& parent, const RenderObject& child, RenderObject& beforeChild)
{
    auto* beforeChildContainer = beforeChild.parent();
    while (beforeChildContainer->parent() != &parent)
        beforeChildContainer = beforeChildContainer->parent();

    ASSERT(beforeChildContainer);
    if (!beforeChildContainer || !beforeChildContainer->isAnonymous())
        return { };

    if (beforeChildContainer->isInline() && child.isInline()) {
        // The before child happens to be a block level box wrapped in an anonymous inline-block in an inline context (e.g. ruby).
        // Let's attach this new child before the anonymous inline-block wrapper.
        ASSERT(beforeChildContainer->isInlineBlockOrInlineTable());
        return ParentAndBeforeChild { &parent, beforeChildContainer };
    }
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!beforeChildContainer->isInline() || beforeChildContainer->isRenderTable());

    // If the requested beforeChild is not one of our children, then this is because
    // there is an anonymous container within this object that contains the beforeChild.
    auto& beforeChildAnonymousContainer = *beforeChildContainer;
    if (beforeChildAnonymousContainer.isAnonymousBlock()) {
        auto mayUseBeforeChildContainerAsParent = [&] {
            auto isFlexOrGridItemContainer = [&] {
                if (auto* renderBox = dynamicDowncast<RenderBox>(beforeChildAnonymousContainer))
                    return renderBox->isFlexItemIncludingDeprecated() || renderBox->isGridItem();
                return false;
            };
            if (child.isOutOfFlowPositioned() && isFlexOrGridItemContainer()) {
                // Do not try to move an out-of-flow block box under an anonymous flex/grid item. It should stay a direct child of the flex/grid container.
                // https://www.w3.org/TR/css-flexbox-1/#abspos-items
                // As it is out-of-flow, an absolutely-positioned child of a flex container does not participate in flex layout.
                // The static position of an absolutely-positioned child of a flex container is determined such that the
                // child is positioned as if it were the sole flex item in the flex container,
                return false;
            }
            return child.isInline() || beforeChildAnonymousContainer.firstChild() != &beforeChild;
        };
        if (mayUseBeforeChildContainerAsParent())
            return ParentAndBeforeChild { &beforeChildAnonymousContainer, &beforeChild };
        return ParentAndBeforeChild { &parent, beforeChild.parent() };
    }

    ASSERT(beforeChildAnonymousContainer.isRenderTable());
    if (child.isTablePart()) {
        // Insert into the anonymous table.
        return ParentAndBeforeChild { &beforeChildAnonymousContainer, &beforeChild };
    }

    // parent needs splitting.
    return ParentAndBeforeChild { };
}

void RenderTreeBuilder::Block::attachIgnoringContinuation(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto parentAndBeforeChildMayNeedAdjustment = beforeChild && beforeChild->parent() != &parent;
    if (parentAndBeforeChildMayNeedAdjustment) {
        if (auto parentAndBeforeChild = findParentAndBeforeChildForNonSibling(parent, *child, *beforeChild)) {
            if (parentAndBeforeChild->parent) {
                m_builder.attach(*parentAndBeforeChild->parent, WTFMove(child), parentAndBeforeChild->beforeChild);
                return;
            }
            beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(beforeChild->parent() == &parent);
        }
    }

    if (child->isFloatingOrOutOfFlowPositioned()) {
        if (parent.childrenInline() || is<RenderFlexibleBox>(parent) || parent.isRenderGrid()) {
            m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
            return;
        }
        // In case of sibling block box(es), let's check if we can add this out of flow/floating box under a previous sibling anonymous block.
        auto* previousSibling = beforeChild ? beforeChild->previousSibling() : parent.lastChild();
        if (!previousSibling || !previousSibling->isAnonymousBlock()) {
            m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
            return;
        }
        m_builder.attach(downcast<RenderBlock>(*previousSibling), WTFMove(child));
        return;
    }

    // Parent and inflow child match.
    if ((parent.childrenInline() && child->isInline()) || (!parent.childrenInline() && !child->isInline()))
        return m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);

    // Inline parent with block child.
    if (parent.childrenInline()) {
        ASSERT(!child->isInline() && !child->isFloatingOrOutOfFlowPositioned());
        // A block has to either have all of its children inline, or all of its children as blocks.
        // So, if our children are currently inline and a block child has to be inserted, we move all our
        // inline children into anonymous block boxes.
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        m_builder.createAnonymousWrappersForInlineContent(parent, beforeChild);
        if (beforeChild && beforeChild->parent() != &parent) {
            beforeChild = beforeChild->parent();
            ASSERT(beforeChild->isAnonymousBlock());
            ASSERT(beforeChild->parent() == &parent);
        }
        m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);

        if (is<RenderBlock>(parent.parent()) && parent.isAnonymousBlock())
            removeLeftoverAnonymousBlock(parent);
        return;
    }

    // Block parent with inline child.
    // If we're inserting an inline child but all of our children are blocks, then we have to make sure
    // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
    // a new one is created and inserted into our list of children in the appropriate position.
    auto* previousSibling = beforeChild ? beforeChild->previousSibling() : parent.lastChild();
    if (previousSibling && previousSibling->isAnonymousBlock()) {
        m_builder.attach(downcast<RenderBlock>(*previousSibling), WTFMove(child));
        return;
    }

    // No suitable existing anonymous box - create a new one.
    auto newBox = parent.createAnonymousBlock();
    auto& box = *newBox;
    m_builder.attachToRenderElement(parent, WTFMove(newBox), beforeChild);
    m_builder.attach(box, WTFMove(child));
}

void RenderTreeBuilder::Block::childBecameNonInline(RenderBlock& parent, RenderElement&)
{
    m_builder.createAnonymousWrappersForInlineContent(parent);
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
    if (is<RenderButton>(*parent) || is<RenderTextControl>(*parent))
        return;

    m_builder.removeFloatingObjects(anonymousBlock);
    // FIXME: This should really just be a moveAllChilrenTo (see webkit.org/b/182495)
    moveAllChildrenToInternal(anonymousBlock, *parent);
    auto toBeDestroyed = m_builder.detachFromRenderElement(*parent, anonymousBlock, WillBeDestroyed::Yes);
    // anonymousBlock is dead here.
}

RenderPtr<RenderObject> RenderTreeBuilder::Block::detach(RenderBlock& parent, RenderObject& oldChild, RenderTreeBuilder::WillBeDestroyed willBeDestroyed, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    // No need to waste time in merging or removing empty anonymous blocks.
    // We can just bail out if our document is getting destroyed.
    if (parent.renderTreeBeingDestroyed())
        return m_builder.detachFromRenderElement(parent, oldChild, willBeDestroyed);

    // If this child is a block, and if our previous and next siblings are both anonymous blocks
    // with inline content, then we can fold the inline content back together.
    WeakPtr prev = oldChild.previousSibling();
    WeakPtr next = oldChild.nextSibling();
    bool canMergeAnonymousBlocks = canCollapseAnonymousBlock == CanCollapseAnonymousBlock::Yes && canMergeContiguousAnonymousBlocks(oldChild, prev.get(), next.get(), m_builder.m_anonymousDestroyRoot.get());

    auto takenChild = m_builder.detachFromRenderElement(parent, oldChild, willBeDestroyed);

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
            auto blockToMove = m_builder.detachFromRenderElement(parent, inlineChildrenBlock, WillBeDestroyed::No);

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

    // FIXME: This should really just be a moveAllChilrenTo (see webkit.org/b/182495)
    moveAllChildrenToInternal(child, parent);
    auto toBeDeleted = m_builder.detachFromRenderElement(parent, child, WillBeDestroyed::Yes);

    // Delete the now-empty block's lines and nuke it.
    child.deleteLines();
}

RenderPtr<RenderObject> RenderTreeBuilder::Block::detach(RenderBlockFlow& parent, RenderObject& child, RenderTreeBuilder::WillBeDestroyed willBeDestroyed, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    if (!parent.renderTreeBeingDestroyed()) {
        auto* fragmentedFlow = parent.multiColumnFlow();
        if (fragmentedFlow && fragmentedFlow != &child)
            m_builder.multiColumnBuilder().multiColumnRelativeWillBeRemoved(*fragmentedFlow, child, canCollapseAnonymousBlock);
    }
    return detach(static_cast<RenderBlock&>(parent), child, willBeDestroyed, canCollapseAnonymousBlock);
}

}
