/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderInline.h"

#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderInline.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderMultiColumn.h"
#include "RenderTreeBuilderTable.h"
#include "Settings.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::Inline);

static bool canUseAsParentForContinuation(const RenderObject* renderer)
{
    if (!renderer)
        return false;
    if (!is<RenderBlock>(renderer) && renderer->isAnonymous())
        return false;
    if (is<RenderTable>(renderer))
        return false;
    return true;
}

static RenderBoxModelObject* nextContinuation(const RenderBoxModelObject* renderer)
{
    if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(*renderer); renderInline && !renderInline->isBlockLevelReplacedOrAtomicInline())
        return renderInline->continuation();
    return renderer->inlineContinuation();
}

RenderBoxModelObject& RenderTreeBuilder::Inline::parentCandidateInContinuation(RenderInline& parent, const RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == &parent)
        return parent;

    CheckedPtr<RenderBoxModelObject> previous = &parent;
    CheckedPtr current = nextContinuation(&parent);
    while (current) {
        if (beforeChild && beforeChild->parent() == current)
            return current->firstChild() == beforeChild ? *previous.unsafeGet() : *current.unsafeGet();
        auto next = nextContinuation(current.get());
        if (!next)
            return !beforeChild && !current->firstChild() ? *previous.unsafeGet() : *current.unsafeGet();
        previous = current;
        current = next;
    }
    ASSERT_NOT_REACHED();
    return *previous.unsafeGet();
}

static RenderPtr<RenderInline> cloneAsContinuation(RenderInline& renderer)
{
    RenderPtr<RenderInline> cloneInline = renderer.isAnonymous() ?
    createRenderer<RenderInline>(RenderObject::Type::Inline, renderer.document(), RenderStyle::clone(renderer.style()))
    : createRenderer<RenderInline>(RenderObject::Type::Inline, *renderer.element(), RenderStyle::clone(renderer.style()));
    cloneInline->initializeStyle();
    cloneInline->setFragmentedFlowState(renderer.fragmentedFlowState());
    cloneInline->setHasOutlineAutoAncestor(renderer.hasOutlineAutoAncestor());
    cloneInline->setIsContinuation();
    return cloneInline;
}

static RenderElement* inFlowPositionedInlineAncestor(RenderElement& renderer)
{
    auto* ancestor = &renderer;
    while (ancestor && ancestor->isRenderInline()) {
        if (ancestor->isInFlowPositioned())
            return ancestor;
        ancestor = ancestor->parent();
    }
    return nullptr;
}

RenderTreeBuilder::Inline::Inline(RenderTreeBuilder& builder)
    : m_builder(builder)
    , m_buildsContinuations(!builder.view().settings().blocksInInlineLayoutEnabled())
{
}

void RenderTreeBuilder::Inline::attach(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto* beforeChildOrPlaceholder = beforeChild;
    if (auto* fragmentedFlow = parent.enclosingFragmentedFlow())
        beforeChildOrPlaceholder = m_builder.multiColumnBuilder().resolveMovedChild(*fragmentedFlow, beforeChild);
    if (parent.continuation()) {
        insertChildToContinuation(parent, WTFMove(child), beforeChildOrPlaceholder);
        return;
    }
    attachIgnoringContinuation(parent, WTFMove(child), beforeChildOrPlaceholder);
}

void RenderTreeBuilder::Inline::insertChildToContinuation(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    ASSERT(m_buildsContinuations);

    if (!beforeChild) {
        auto& parentCandidate = parentCandidateInContinuation(parent, { });
        auto* lastContinuation = nextContinuation(&parentCandidate);
        if (!lastContinuation) {
            // parentCandidate is the last continuation.
            return m_builder.attachIgnoringContinuation(parentCandidate, WTFMove(child));
        }
        // The inline box inside the "post" part of the continuation is the preferred parent but we may not be able to put this child in there.
        auto& nextToLastContinuation = parentCandidate;
        auto childIsInline = newChildIsInline(parent, *child);
        if (childIsInline == lastContinuation->isInline() || childIsInline != nextToLastContinuation.isInline() || child->isFloatingOrOutOfFlowPositioned())
            return m_builder.attachIgnoringContinuation(*lastContinuation, WTFMove(child));
        return m_builder.attachIgnoringContinuation(nextToLastContinuation, WTFMove(child));
    }

    // It may or may not be the direct parent of the beforeChild.
    RenderBoxModelObject* beforeChildContinuationAncestor = nullptr;
    if (canUseAsParentForContinuation(beforeChild->parent()))
        beforeChildContinuationAncestor = downcast<RenderBoxModelObject>(beforeChild->parent());
    else if (beforeChild->parent()) {
        // In case of anonymous wrappers, the parent of the beforeChild is mostly irrelevant. What we need is the topmost wrapper.
        auto* parent = beforeChild->parent();
        while (parent && parent->parent() && parent->parent()->isAnonymous()) {
            // The ancestor candidate needs to be inside the continuation.
            if (parent->isContinuation())
                break;
            parent = parent->parent();
        }
        ASSERT(parent && parent->parent());
        beforeChildContinuationAncestor = downcast<RenderBoxModelObject>(parent->parent());
    } else
        ASSERT_NOT_REACHED();

    if (child->isFloatingOrOutOfFlowPositioned()) {
        auto& beforeChildParent = *beforeChild->parent();
        auto beforeChildIsFirstChildInContinuation = beforeChild == beforeChildParent.firstChild() && beforeChildParent.isAnonymousBlock() && beforeChildParent.isContinuation();
        if (!beforeChildIsFirstChildInContinuation)
            return m_builder.attachIgnoringContinuation(*beforeChildContinuationAncestor, WTFMove(child), beforeChild);
        return m_builder.attachIgnoringContinuation(parentCandidateInContinuation(parent, beforeChild), WTFMove(child));
    }

    auto& parentCandidate = parentCandidateInContinuation(parent, beforeChild);
    if (&parentCandidate == beforeChildContinuationAncestor)
        return m_builder.attachIgnoringContinuation(parentCandidate, WTFMove(child), beforeChild);
    // A continuation always consists of two potential candidates: an inline or an anonymous block box holding block children.
    bool childInline = newChildIsInline(parent, *child);
    // The goal here is to match up if we can, so that we can coalesce and create the
    // minimal # of continuations needed for the inline.
    if (childInline == beforeChildContinuationAncestor->isInline() || beforeChild->isInline())
        return m_builder.attachIgnoringContinuation(*beforeChildContinuationAncestor, WTFMove(child), beforeChild);
    if (parentCandidate.isInline() == childInline)
        return m_builder.attachIgnoringContinuation(parentCandidate, WTFMove(child)); // Just treat like an append.
    return m_builder.attachIgnoringContinuation(*beforeChildContinuationAncestor, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::Inline::attachIgnoringContinuation(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && RenderElement::isAfterContent(dynamicDowncast<RenderElement>(parent.lastChild())))
        beforeChild = parent.lastChild();

    bool childInline = newChildIsInline(parent, *child);
    // This code is for the old block-inside-inline model that uses continuations.
    if (m_buildsContinuations && !childInline && !child->isFloatingOrOutOfFlowPositioned()) {
        // We are placing a block inside an inline. We have to perform a split of this
        // inline into continuations. This involves creating an anonymous block box to hold
        // |newChild|. We then make that block box a continuation of this inline. We take all of
        // the children after |beforeChild| and put them in a clone of this object.
        auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(parent.containingBlock() ? parent.containingBlock()->style() : parent.style(), DisplayType::Block);

        // If inside an inline affected by in-flow positioning the block needs to be affected by it too.
        // Giving the block a layer like this allows it to collect the x/y offsets from inline parents later.
        if (auto positionedAncestor = inFlowPositionedInlineAncestor(parent))
            newStyle.setPosition(positionedAncestor->style().position());

        auto newBox = createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, parent.document(), WTFMove(newStyle));
        newBox->initializeStyle();
        newBox->setIsContinuation();
        RenderBoxModelObject* oldContinuation = parent.continuation();
        if (oldContinuation)
            oldContinuation->removeFromContinuationChain();
        newBox->insertIntoContinuationChainAfter(parent);

        splitFlow(parent, beforeChild, WTFMove(newBox), WTFMove(child), oldContinuation);
        return;
    }

    if (!m_buildsContinuations) {
        // In blocks-in-inline case we allow blocks that may need splitting.
        if (beforeChild && beforeChild->parent() != &parent)
            beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);
    }

    auto& childToAdd = *child;
    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
    childToAdd.setNeedsLayoutAndPreferredWidthsUpdate();
}

void RenderTreeBuilder::Inline::splitFlow(RenderInline& parent, RenderObject* beforeChild, RenderPtr<RenderBlock> newBlockBox, RenderPtr<RenderObject> child, RenderBoxModelObject* oldCont)
{
    ASSERT(m_buildsContinuations);
    ASSERT(newBlockBox);
    auto& addedBlockBox = *newBlockBox;
    RenderBlock* pre = nullptr;
    RenderBlock* block = parent.containingBlock();

    // Delete our line boxes before we do the inline split into continuations.
    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(block))
        blockFlow->invalidateLineLayout(RenderBlockFlow::InvalidationReason::InternalMove);

    RenderPtr<RenderBlock> createdPre;
    bool madeNewBeforeBlock = false;
    auto canReuseContainingBlockAsPreBlock = [&] {
        if (!block->isAnonymousBlock())
            return false;
        if (auto* containingBlockParent = block->parent())
            return !containingBlockParent->createsAnonymousWrapper() && !containingBlockParent->isRenderDeprecatedFlexibleBox();
        return false;
    };
    if (canReuseContainingBlockAsPreBlock()) {
        // We can reuse this block and make it the preBlock of the next continuation.
        pre = block;
        pre->removeOutOfFlowBoxes({ });
        // FIXME-BLOCKFLOW: The enclosing method should likely be switched over
        // to only work on RenderBlockFlow, in which case this conversion can be
        // removed.
        if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(*pre))
            blockFlow->removeFloatingObjects();
        block = block->containingBlock();
    } else {
        // No anonymous block available for use. Make one.
        createdPre = Block::createAnonymousBlockWithStyle(block->protectedDocument(), block->style());
        pre = createdPre.get();
        madeNewBeforeBlock = true;
    }

    auto createdPost = createAnonymousBoxWithSameTypeAndWithStyle(*pre, block->style());
    auto& post = downcast<RenderBlock>(*createdPost);

    RenderObject* boxFirst = madeNewBeforeBlock ? block->firstChild() : pre->nextSibling();
    if (createdPre)
        m_builder.attachToRenderElementInternal(*block, WTFMove(createdPre), boxFirst);
    m_builder.attachToRenderElementInternal(*block, WTFMove(newBlockBox), boxFirst);
    m_builder.attachToRenderElementInternal(*block, WTFMove(createdPost), boxFirst);
    block->setChildrenInline(false);

    if (madeNewBeforeBlock) {
        RenderObject* o = boxFirst;
        while (o) {
            RenderObject* no = o;
            auto internalMoveScope = SetForScope { m_builder.m_internalMovesType, IsInternalMove::Yes };
            o = no->nextSibling();
            auto childToMove = m_builder.detachFromRenderElement(*block, *no, WillBeDestroyed::No);
            m_builder.attachToRenderElementInternal(*pre, WTFMove(childToMove));
            no->setNeedsLayoutAndPreferredWidthsUpdate();
        }
    }

    splitInlines(parent, pre, &post, &addedBlockBox, beforeChild, oldCont);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    addedBlockBox.setChildrenInline(false);

    // We delayed adding the newChild until now so that the |newBlockBox| would be fully
    // connected, thus allowing newChild access to a renderArena should it need
    // to wrap itself in additional boxes (e.g., table construction).
    m_builder.attach(addedBlockBox, WTFMove(child));

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly. Because objects moves from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    pre->setNeedsLayoutAndPreferredWidthsUpdate();
    block->setNeedsLayoutAndPreferredWidthsUpdate();
    post.setNeedsLayoutAndPreferredWidthsUpdate();
}

void RenderTreeBuilder::Inline::splitInlines(RenderInline& parent, RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock, RenderObject* beforeChild, RenderBoxModelObject* oldCont)
{
    ASSERT(m_buildsContinuations);
    auto internalMoveScope = SetForScope { m_builder.m_internalMovesType, IsInternalMove::Yes };
    // Create a clone of this inline.
    RenderPtr<RenderInline> cloneInline = cloneAsContinuation(parent);

    // Now take all of the children from beforeChild to the end and remove
    // them from |this| and place them in the clone.
    for (RenderObject* rendererToMove = beforeChild; rendererToMove;) {
        RenderObject* nextSibling = rendererToMove->nextSibling();
        // When anonymous wrapper is present, we might need to move the whole subtree instead.
        if (rendererToMove->parent() != &parent) {
            auto* anonymousParent = rendererToMove->parent();
            while (anonymousParent && anonymousParent->parent() != &parent) {
                ASSERT(anonymousParent->isAnonymous());
                anonymousParent = anonymousParent->parent();
            }
            if (!anonymousParent) {
                ASSERT_NOT_REACHED();
                break;
            }
            // If beforeChild is the first child in the subtree, we could just move the whole subtree.
            if (!rendererToMove->previousSibling()) {
                // Reparent the whole anonymous wrapper tree.
                rendererToMove = anonymousParent;
                // Skip to the next sibling that is not in this subtree.
                nextSibling = anonymousParent->nextSibling();
            } else if (!rendererToMove->nextSibling()) {
                // This is the last renderer in the subtree. We need to jump out of the wrapper subtree, so that
                // the siblings are getting reparented too.
                nextSibling = anonymousParent->nextSibling();
            }
            // Otherwise just move the renderer to the inline clone. Should the renderer need an anon
            // wrapper, the addChild() will generate one for it.
            // FIXME: When the anonymous wrapper has multiple children, we end up traversing up to the topmost wrapper
            // every time, which is a bit wasteful.
        }
        auto childToMove = m_builder.detachFromRenderElement(*rendererToMove->parent(), *rendererToMove, WillBeDestroyed::No);
        m_builder.attachIgnoringContinuation(*cloneInline, WTFMove(childToMove));
        auto* newParent = rendererToMove->parent();
        if (CheckedPtr newParentBox = dynamicDowncast<RenderBox>(newParent))
            markBoxForRelayoutAfterSplit(*newParentBox);
        rendererToMove->setNeedsLayoutAndPreferredWidthsUpdate();
        rendererToMove = nextSibling;
    }
    // Hook |clone| up as the continuation of the middle block.
    cloneInline->insertIntoContinuationChainAfter(*middleBlock);
    if (oldCont)
        oldCont->insertIntoContinuationChainAfter(*cloneInline);

    // We have been reparented and are now under the fromBlock. We need
    // to walk up our inline parent chain until we hit the containing block.
    // Once we hit the containing block we're done.
    RenderBoxModelObject* current = downcast<RenderBoxModelObject>(parent.parent());
    RenderBoxModelObject* currentChild = &parent;

    // FIXME: Because splitting is O(n^2) as tags nest pathologically, we cap the depth at which we're willing to clone.
    // There will eventually be a better approach to this problem that will let us nest to a much
    // greater depth (see bugzilla bug 13430) but for now we have a limit. This *will* result in
    // incorrect rendering, but the alternative is to hang forever.
    unsigned splitDepth = 1;
    const unsigned cMaxSplitDepth = 200;
    while (current && current != fromBlock) {
        if (splitDepth < cMaxSplitDepth && !current->isAnonymous()) {
            // Create a new clone.
            RenderPtr<RenderInline> cloneChild = WTFMove(cloneInline);
            cloneInline = cloneAsContinuation(downcast<RenderInline>(*current));

            // Insert our child clone as the first child.
            m_builder.attachIgnoringContinuation(*cloneInline, WTFMove(cloneChild));

            // Hook the clone up as a continuation of |curr|.
            cloneInline->insertIntoContinuationChainAfter(*current);

            // Now we need to take all of the children starting from the first child
            // *after* currentChild and append them all to the clone.
            for (auto* sibling = currentChild->nextSibling(); sibling;) {
                auto* next = sibling->nextSibling();
                auto childToMove = m_builder.detachFromRenderElement(*current, *sibling, WillBeDestroyed::No);
                m_builder.attachIgnoringContinuation(*cloneInline, WTFMove(childToMove));
                sibling->setNeedsLayoutAndPreferredWidthsUpdate();
                sibling = next;
            }
        } else
            m_builder.setHasBrokenContinuation();

        // Keep walking up the chain.
        currentChild = current;
        current = downcast<RenderBoxModelObject>(current->parent());
        ++splitDepth;
    }

    // Clear the flow thread containing blocks cached during the detached state insertions.
    for (auto& cloneBlockChild : childrenOfType<RenderBlock>(*cloneInline))
        cloneBlockChild.resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants();

    // Now we are at the block level. We need to put the clone into the toBlock.
    m_builder.attachToRenderElementInternal(*toBlock, WTFMove(cloneInline));

    // Now take all the children after currentChild and remove them from the fromBlock
    // and put them in the toBlock.
    for (auto* current = currentChild->nextSibling(); current;) {
        auto* next = current->nextSibling();
        auto childToMove = m_builder.detachFromRenderElement(*fromBlock, *current, WillBeDestroyed::No);
        m_builder.attachToRenderElementInternal(*toBlock, WTFMove(childToMove));
        current = next;
    }
}

bool RenderTreeBuilder::Inline::newChildIsInline(const RenderInline& parent, const RenderObject& child)
{
    // inline parent generates inline-table.
    return child.isInline() || (m_builder.tableBuilder().childRequiresTable(parent, child) && parent.style().display() == DisplayType::Inline);
}

void RenderTreeBuilder::Inline::childBecameNonInline(RenderInline& parent, RenderElement& child)
{
    if (!m_buildsContinuations)
        return;

    // We have to split the parent flow.
    auto newBox = Block::createAnonymousBlockWithStyle(parent.containingBlock()->protectedDocument(), parent.containingBlock()->style());
    newBox->setIsContinuation();
    auto* oldContinuation = parent.continuation();
    if (oldContinuation)
        oldContinuation->removeFromContinuationChain();
    newBox->insertIntoContinuationChainAfter(parent);
    auto* beforeChild = child.nextSibling();
    auto removedChild = m_builder.detachFromRenderElement(parent, child, WillBeDestroyed::No);
    splitFlow(parent, beforeChild, WTFMove(newBox), WTFMove(removedChild), oldContinuation);
}

void RenderTreeBuilder::Inline::updateAfterDescendants(RenderInline& parent)
{
    if (m_buildsContinuations)
        return;
    wrapRunsOfBlocksInAnonymousBlock(parent);
}

void RenderTreeBuilder::Inline::wrapRunsOfBlocksInAnonymousBlock(RenderInline& parent)
{
    // Wrap runs of block boxes with an anonymous block so their margins collapse correctly for blocks-in-inline.
    ASSERT(!m_buildsContinuations);

    auto dropNestedAnonymousBlocks = [&](CheckedRef<RenderBlockFlow> anonymousBlock) {
        ASSERT(anonymousBlock->isAnonymousBlock());
        SingleThreadWeakPtr<RenderObject> nextChild;
        for (SingleThreadWeakPtr<RenderObject> movedChild = anonymousBlock->firstChild(); movedChild; movedChild = nextChild) {
            nextChild = movedChild->nextSibling();
            auto blockChild = dynamicDowncast<RenderBlockFlow>(movedChild.get());
            if (blockChild && blockChild->isAnonymousBlock() && !blockChild->childrenInline())
                m_builder.blockBuilder().dropAnonymousBoxChild(anonymousBlock, *blockChild);
        }
    };

    SingleThreadWeakPtr<RenderBox> firstInRun;
    SingleThreadWeakPtr<RenderBox> lastInRun;
    auto isSingleAnonymousBlockRun = false;

    auto wrapRunInAnonymousBlockIfNeeded = [&] {
        // FIXME: Removing wrapping requires changes how RenderBlockFlow handles block formatting state.
        if (!firstInRun)
            return;

        auto newBlock = Block::createAnonymousBlockWithStyle(parent.protectedDocument(), parent.containingBlock()->style());
        newBlock->setChildrenInline(false);
        CheckedRef block = *newBlock;
        m_builder.attachToRenderElementInternal(parent, WTFMove(newBlock), firstInRun.get());
        m_builder.moveChildren(parent, block, firstInRun.get(), lastInRun->nextSibling(), RenderTreeBuilder::NormalizeAfterInsertion::No);

        // We might have wrapped existing anonymous blocks and they are now nested. Get rid of them.
        dropNestedAnonymousBlocks(block);
    };

    auto dropSingleAnonymousBlockIfNotNeeded = [&] {
        ASSERT(firstInRun == lastInRun);
        ASSERT(firstInRun->isAnonymousBlock());

        auto hasInFlowChild = [&] {
            for (auto& child : childrenOfType<RenderObject>(*firstInRun)) {
                if (child.isInFlow())
                    return true;
            }
            return false;
        };
        if (hasInFlowChild())
            return;
        m_builder.moveAllChildren(*firstInRun, parent, RenderTreeBuilder::NormalizeAfterInsertion::No);
        auto emptyAnonymousBlock = m_builder.detachFromRenderElement(parent, *firstInRun, WillBeDestroyed::Yes);
    };

    for (CheckedPtr child = parent.firstChild() ; child; child = child->nextSibling()) {
        if (child->isInline()) {
            !isSingleAnonymousBlockRun ? wrapRunInAnonymousBlockIfNeeded() : dropSingleAnonymousBlockIfNotNeeded();
            firstInRun = nullptr;
            lastInRun = nullptr;
            isSingleAnonymousBlockRun = false;
            continue;
        }
        if (auto* blockChild = dynamicDowncast<RenderBox>(*child); blockChild && blockChild->isInFlow()) {
            // Floats and out-of-flow boxes are wrapped if they are in the middle of a block run.
            isSingleAnonymousBlockRun = !firstInRun && blockChild->isAnonymousBlock();
            if (!firstInRun)
                firstInRun = blockChild;
            lastInRun = blockChild;
        }
    }
    !isSingleAnonymousBlockRun ? wrapRunInAnonymousBlockIfNeeded() : dropSingleAnonymousBlockIfNotNeeded();
}

}
