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
#include "RenderTreeBuilder.h"

#include "AXObjectCache.h"
#include "DocumentInlines.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "LegacyRenderSVGContainer.h"
#include "LegacyRenderSVGRoot.h"
#include "RenderButton.h"
#include "RenderCounter.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderEmbeddedObject.h"
#include "RenderGrid.h"
#include "RenderHTMLCanvas.h"
#include "RenderLineBreak.h"
#include "RenderMathMLFenced.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderReplaced.h"
#include "RenderRuby.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderSVGContainer.h"
#include "RenderSVGInline.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderTableSection.h"
#include "RenderText.h"
#include "RenderTextFragment.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderBlockFlow.h"
#include "RenderTreeBuilderContinuation.h"
#include "RenderTreeBuilderFirstLetter.h"
#include "RenderTreeBuilderFormControls.h"
#include "RenderTreeBuilderInline.h"
#include "RenderTreeBuilderList.h"
#include "RenderTreeBuilderMathML.h"
#include "RenderTreeBuilderMultiColumn.h"
#include "RenderTreeBuilderRuby.h"
#include "RenderTreeBuilderSVG.h"
#include "RenderTreeBuilderTable.h"
#include "RenderTreeMutationDisallowedScope.h"
#include "RenderView.h"
#include <wtf/SetForScope.h>

#include "FrameView.h"
#include "FrameViewLayoutContext.h"

namespace WebCore {

RenderTreeBuilder* RenderTreeBuilder::s_current;

static void getInlineRun(RenderObject* start, RenderObject* boundary, RenderObject*& inlineRunStart, RenderObject*& inlineRunEnd)
{
    // Beginning at |start| we find the largest contiguous run of inlines that
    // we can. We denote the run with start and end points, |inlineRunStart|
    // and |inlineRunEnd|. Note that these two values may be the same if
    // we encounter only one inline.
    //
    // We skip any non-inlines we encounter as long as we haven't found any
    // inlines yet.
    //
    // |boundary| indicates a non-inclusive boundary point. Regardless of whether |boundary|
    // is inline or not, we will not include it in a run with inlines before it. It's as though we encountered
    // a non-inline.

    // Start by skipping as many non-inlines as we can.
    auto* curr = start;
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

RenderTreeBuilder::RenderTreeBuilder(RenderView& view)
    : m_view(view)
    , m_firstLetterBuilder(makeUnique<FirstLetter>(*this))
    , m_listBuilder(makeUnique<List>(*this))
    , m_multiColumnBuilder(makeUnique<MultiColumn>(*this))
    , m_tableBuilder(makeUnique<Table>(*this))
    , m_rubyBuilder(makeUnique<Ruby>(*this))
    , m_formControlsBuilder(makeUnique<FormControls>(*this))
    , m_blockBuilder(makeUnique<Block>(*this))
    , m_blockFlowBuilder(makeUnique<BlockFlow>(*this))
    , m_inlineBuilder(makeUnique<Inline>(*this))
    , m_svgBuilder(makeUnique<SVG>(*this))
#if ENABLE(MATHML)
    , m_mathMLBuilder(makeUnique<MathML>(*this))
#endif
    , m_continuationBuilder(makeUnique<Continuation>(*this))
{
    RELEASE_ASSERT(!s_current || &m_view != &s_current->m_view);
    m_previous = s_current;
    s_current = this;
}

RenderTreeBuilder::~RenderTreeBuilder()
{
    s_current = m_previous;
}

void RenderTreeBuilder::destroy(RenderObject& renderer, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    RELEASE_ASSERT(RenderTreeMutationDisallowedScope::isMutationAllowed());
    ASSERT(renderer.parent());
    auto toDestroy = detach(*renderer.parent(), renderer, canCollapseAnonymousBlock);

    if (is<RenderTextFragment>(renderer))
        firstLetterBuilder().cleanupOnDestroy(downcast<RenderTextFragment>(renderer));

    if (is<RenderBoxModelObject>(renderer))
        continuationBuilder().cleanupOnDestroy(downcast<RenderBoxModelObject>(renderer));

    // We need to detach the subtree first so that the descendants don't have
    // access to previous/next sublings at detach().
    // FIXME: webkit.org/b/182909.
    if (!is<RenderElement>(toDestroy))
        return;

    auto& childToDestroy = downcast<RenderElement>(*toDestroy.get());
    while (childToDestroy.firstChild()) {
        auto& firstChild = *childToDestroy.firstChild();
        if (auto* node = firstChild.node())
            node->setRenderer(nullptr);
        destroy(firstChild);
    }
}

void RenderTreeBuilder::attach(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    reportVisuallyNonEmptyContent(parent, *child);
    attachInternal(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::attachInternal(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto insertRecursiveIfNeeded = [&](RenderElement& parentCandidate) {
        if (&parent == &parentCandidate) {
            // Parents inside multicols can't call internal attach directly.
            if (is<RenderBlockFlow>(parent) && downcast<RenderBlockFlow>(parent).multiColumnFlow()) {
                blockFlowBuilder().attach(downcast<RenderBlockFlow>(parent), WTFMove(child), beforeChild);
                return;
            }
            attachToRenderElement(parent, WTFMove(child), beforeChild);
            return;
        }
        attachInternal(parentCandidate, WTFMove(child), beforeChild);
    };

    ASSERT(&parent.view() == &m_view);

    if (is<RenderText>(beforeChild)) {
        if (auto* wrapperInline = downcast<RenderText>(*beforeChild).inlineWrapperForDisplayContents())
            beforeChild = wrapperInline;
    } else if (is<RenderBox>(beforeChild)) {
        // Adjust the beforeChild if it happens to be a spanner and the its actual location is inside the fragmented flow.
        auto& beforeChildBox = downcast<RenderBox>(*beforeChild);
        if (auto* enclosingFragmentedFlow = parent.enclosingFragmentedFlow()) {
            auto columnSpannerPlaceholderForBeforeChild = [&]() -> RenderMultiColumnSpannerPlaceholder* {
                if (!is<RenderMultiColumnFlow>(enclosingFragmentedFlow))
                    return nullptr;
                auto& multiColumnFlow = downcast<RenderMultiColumnFlow>(*enclosingFragmentedFlow);
                return multiColumnFlow.findColumnSpannerPlaceholder(&beforeChildBox);
            };

            if (auto* spannerPlaceholder = columnSpannerPlaceholderForBeforeChild())
                beforeChild = spannerPlaceholder;
        }
    }

    if (is<RenderTableRow>(parent)) {
        auto& parentCandidate = tableBuilder().findOrCreateParentForChild(downcast<RenderTableRow>(parent), *child, beforeChild);
        if (&parentCandidate == &parent) {
            tableBuilder().attach(downcast<RenderTableRow>(parentCandidate), WTFMove(child), beforeChild);
            return;
        }
        insertRecursiveIfNeeded(parentCandidate);
        return;
    }

    if (is<RenderTableSection>(parent)) {
        auto& parentCandidate = tableBuilder().findOrCreateParentForChild(downcast<RenderTableSection>(parent), *child, beforeChild);
        if (&parent == &parentCandidate) {
            tableBuilder().attach(downcast<RenderTableSection>(parent), WTFMove(child), beforeChild);
            return;
        }
        insertRecursiveIfNeeded(parentCandidate);
        return;
    }

    if (is<RenderTable>(parent)) {
        auto& parentCandidate = tableBuilder().findOrCreateParentForChild(downcast<RenderTable>(parent), *child, beforeChild);
        if (&parentCandidate == &parent) {
            tableBuilder().attach(downcast<RenderTable>(parentCandidate), WTFMove(child), beforeChild);
            return;
        }
        insertRecursiveIfNeeded(parentCandidate);
        return;
    }

    if (is<RenderRubyAsBlock>(parent)) {
        insertRecursiveIfNeeded(rubyBuilder().findOrCreateParentForChild(downcast<RenderRubyAsBlock>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderRubyAsInline>(parent)) {
        insertRecursiveIfNeeded(rubyBuilder().findOrCreateParentForChild(downcast<RenderRubyAsInline>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderRubyRun>(parent)) {
        rubyBuilder().attach(downcast<RenderRubyRun>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderButton>(parent)) {
        formControlsBuilder().attach(downcast<RenderButton>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderMenuList>(parent)) {
        formControlsBuilder().attach(downcast<RenderMenuList>(parent), WTFMove(child), beforeChild);
        return;
    }

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGContainer>(parent)) {
        svgBuilder().attach(downcast<RenderSVGContainer>(parent), WTFMove(child), beforeChild);
        return;
    }
#endif

    if (is<LegacyRenderSVGContainer>(parent)) {
        svgBuilder().attach(downcast<LegacyRenderSVGContainer>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderSVGInline>(parent)) {
        svgBuilder().attach(downcast<RenderSVGInline>(parent), WTFMove(child), beforeChild);
        return;
    }

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGRoot>(parent)) {
        svgBuilder().attach(downcast<RenderSVGRoot>(parent), WTFMove(child), beforeChild);
        return;
    }
#endif

    if (is<LegacyRenderSVGRoot>(parent)) {
        svgBuilder().attach(downcast<LegacyRenderSVGRoot>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderSVGText>(parent)) {
        svgBuilder().attach(downcast<RenderSVGText>(parent), WTFMove(child), beforeChild);
        return;
    }

#if ENABLE(MATHML)
    if (is<RenderMathMLFenced>(parent)) {
        mathMLBuilder().attach(downcast<RenderMathMLFenced>(parent), WTFMove(child), beforeChild);
        return;
    }
#endif

    if (is<RenderGrid>(parent)) {
        attachToRenderGrid(downcast<RenderGrid>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderBlockFlow>(parent)) {
        blockFlowBuilder().attach(downcast<RenderBlockFlow>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderBlock>(parent)) {
        blockBuilder().attach(downcast<RenderBlock>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderInline>(parent)) {
        inlineBuilder().attach(downcast<RenderInline>(parent), WTFMove(child), beforeChild);
        return;
    }

    attachToRenderElement(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::attachIgnoringContinuation(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (is<RenderInline>(parent)) {
        inlineBuilder().attachIgnoringContinuation(downcast<RenderInline>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderBlock>(parent)) {
        blockBuilder().attachIgnoringContinuation(downcast<RenderBlock>(parent), WTFMove(child), beforeChild);
        return;
    }

    attachInternal(parent, WTFMove(child), beforeChild);
}

RenderPtr<RenderObject> RenderTreeBuilder::detach(RenderElement& parent, RenderObject& child, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    if (is<RenderRubyAsInline>(parent))
        return rubyBuilder().detach(downcast<RenderRubyAsInline>(parent), child);

    if (is<RenderRubyAsBlock>(parent))
        return rubyBuilder().detach(downcast<RenderRubyAsBlock>(parent), child);

    if (is<RenderRubyRun>(parent))
        return rubyBuilder().detach(downcast<RenderRubyRun>(parent), child);

    if (is<RenderMenuList>(parent))
        return formControlsBuilder().detach(downcast<RenderMenuList>(parent), child);

    if (is<RenderButton>(parent))
        return formControlsBuilder().detach(downcast<RenderButton>(parent), child);

    if (is<RenderGrid>(parent))
        return detachFromRenderGrid(downcast<RenderGrid>(parent), child);

    if (is<RenderSVGText>(parent))
        return svgBuilder().detach(downcast<RenderSVGText>(parent), child);

    if (is<RenderSVGInline>(parent))
        return svgBuilder().detach(downcast<RenderSVGInline>(parent), child);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGContainer>(parent))
        return svgBuilder().detach(downcast<RenderSVGContainer>(parent), child);
#endif

    if (is<LegacyRenderSVGContainer>(parent))
        return svgBuilder().detach(downcast<LegacyRenderSVGContainer>(parent), child);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGRoot>(parent))
        return svgBuilder().detach(downcast<RenderSVGRoot>(parent), child);
#endif

    if (is<LegacyRenderSVGRoot>(parent))
        return svgBuilder().detach(downcast<LegacyRenderSVGRoot>(parent), child);

    if (is<RenderBlockFlow>(parent))
        return blockBuilder().detach(downcast<RenderBlockFlow>(parent), child, canCollapseAnonymousBlock);

    if (is<RenderBlock>(parent))
        return blockBuilder().detach(downcast<RenderBlock>(parent), child, canCollapseAnonymousBlock);

    return detachFromRenderElement(parent, child);
}

void RenderTreeBuilder::attachToRenderElement(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (tableBuilder().childRequiresTable(parent, *child)) {
        RenderTable* table;
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : parent.lastChild();
        if (afterChild && afterChild->isAnonymous() && is<RenderTable>(*afterChild) && !afterChild->isBeforeContent())
            table = downcast<RenderTable>(afterChild);
        else {
            auto newTable = RenderTable::createAnonymousWithParentRenderer(parent);
            table = newTable.get();
            attach(parent, WTFMove(newTable), beforeChild);
        }

        attach(*table, WTFMove(child));
        return;
    }
    auto& newChild = *child.get();
    attachToRenderElementInternal(parent, WTFMove(child), beforeChild);
    parent.didAttachChild(newChild, beforeChild);
}

void RenderTreeBuilder::attachToRenderElementInternal(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild, RenderObject::IsInternalMove isInternalMove)
{
    RELEASE_ASSERT_WITH_MESSAGE(!parent.view().frameView().layoutContext().layoutState(), "Layout must not mutate render tree");
    ASSERT(parent.canHaveChildren() || parent.canHaveGeneratedChildren());
    ASSERT(!child->parent());
    ASSERT(!parent.isRenderBlockFlow() || (!child->isTableSection() && !child->isTableRow() && !child->isTableCell()));

    while (beforeChild && beforeChild->parent() && beforeChild->parent() != &parent)
        beforeChild = beforeChild->parent();

    ASSERT(!beforeChild || beforeChild->parent() == &parent);
    ASSERT(!is<RenderText>(beforeChild) || !downcast<RenderText>(*beforeChild).inlineWrapperForDisplayContents());

    // Take the ownership.
    auto* newChild = parent.attachRendererInternal(WTFMove(child), beforeChild);

    if (m_internalMovesType == RenderObject::IsInternalMove::No)
        newChild->initializeFragmentedFlowStateOnInsertion();
    if (!parent.renderTreeBeingDestroyed()) {
        newChild->insertedIntoTree(isInternalMove);

        if (m_internalMovesType == RenderObject::IsInternalMove::No) {
            if (auto* fragmentedFlow = newChild->enclosingFragmentedFlow(); is<RenderMultiColumnFlow>(fragmentedFlow))
                multiColumnBuilder().multiColumnDescendantInserted(downcast<RenderMultiColumnFlow>(*fragmentedFlow), *newChild);

            if (is<RenderElement>(*newChild))
                RenderCounter::rendererSubtreeAttached(downcast<RenderElement>(*newChild));
        }
    }

    newChild->setNeedsLayoutAndPrefWidthsRecalc();
    parent.setPreferredLogicalWidthsDirty(true);
    if (!parent.normalChildNeedsLayout())
        parent.setChildNeedsLayout(); // We may supply the static position for an absolute positioned child.

    if (AXObjectCache* cache = parent.document().axObjectCache())
        cache->childrenChanged(&parent, newChild);

    if (parent.hasOutlineAutoAncestor() || parent.outlineStyleForRepaint().outlineStyleIsAuto() == OutlineIsAuto::On)
        newChild->setHasOutlineAutoAncestor();
}

void RenderTreeBuilder::move(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject& child, RenderObject* beforeChild, NormalizeAfterInsertion normalizeAfterInsertion)
{
    // We assume that callers have cleared their positioned objects list for child moves so the
    // positioned renderer maps don't become stale. It would be too slow to do the map lookup on each call.
    ASSERT(normalizeAfterInsertion == NormalizeAfterInsertion::No || !is<RenderBlock>(from) || !downcast<RenderBlock>(from).hasPositionedObjects());

    ASSERT(&from == child.parent());
    ASSERT(!beforeChild || &to == beforeChild->parent());
    if (normalizeAfterInsertion == NormalizeAfterInsertion::Yes && (to.isRenderBlock() || to.isRenderInline())) {
        // Takes care of adding the new child correctly if toBlock and fromBlock
        // have different kind of children (block vs inline).
        auto childToMove = detachFromRenderElement(from, child);
        attach(to, WTFMove(childToMove), beforeChild);
    } else {
        auto internalMoveScope = SetForScope { m_internalMovesType, RenderObject::IsInternalMove::Yes };
        auto childToMove = detachFromRenderElement(from, child, WillBeDestroyed::No);
        attachToRenderElementInternal(to, WTFMove(childToMove), beforeChild);
    }

    auto findBFCRootAndDestroyInlineTree = [&] {
        auto* containingBlock = &from;
        while (containingBlock) {
            containingBlock->setNeedsLayout();
            if (is<RenderBlockFlow>(*containingBlock)) {
                downcast<RenderBlockFlow>(*containingBlock).deleteLines();
                break;
            }
            containingBlock = containingBlock->containingBlock();
        }
    };
    // When moving a subtree out of a BFC we need to make sure that the line boxes generated for the inline tree are not accessible anymore from the renderers.
    // Let's find the BFC root and nuke the inline tree (At some point we are going to destroy the subtree instead of moving these renderers around.)
    if (is<RenderInline>(child))
        findBFCRootAndDestroyInlineTree();
}

void RenderTreeBuilder::move(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject& child, NormalizeAfterInsertion normalizeAfterInsertion)
{
    move(from, to, child, nullptr, normalizeAfterInsertion);
}

void RenderTreeBuilder::moveAllChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, NormalizeAfterInsertion normalizeAfterInsertion)
{
    moveAllChildren(from, to, nullptr, normalizeAfterInsertion);
}

void RenderTreeBuilder::moveAllChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject* beforeChild, NormalizeAfterInsertion normalizeAfterInsertion)
{
    moveChildren(from, to, from.firstChild(), nullptr, beforeChild, normalizeAfterInsertion);
}

void RenderTreeBuilder::moveChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject* startChild, RenderObject* endChild, NormalizeAfterInsertion normalizeAfterInsertion)
{
    moveChildren(from, to, startChild, endChild, nullptr, normalizeAfterInsertion);
}

void RenderTreeBuilder::moveChildren(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject* startChild, RenderObject* endChild, RenderObject* beforeChild, NormalizeAfterInsertion normalizeAfterInsertion)
{
    // This condition is rarely hit since this function is usually called on
    // anonymous blocks which can no longer carry positioned objects (see r120761)
    // or when fullRemoveInsert is false.
    if (normalizeAfterInsertion == NormalizeAfterInsertion::Yes && is<RenderBlock>(from)) {
        auto& blockFlow = downcast<RenderBlock>(from);
        blockFlow.removePositionedObjects(nullptr);
        removeFloatingObjects(blockFlow);
    }

    ASSERT(!beforeChild || &to == beforeChild->parent());
    for (RenderObject* child = startChild; child && child != endChild; ) {
        // Save our next sibling as moveChildTo will clear it.
        RenderObject* nextSibling = child->nextSibling();

        // FIXME: This logic here fails to detect the first letter in certain cases
        // and skips a valid sibling renderer (see webkit.org/b/163737).
        // Check to make sure we're not saving the firstLetter as the nextSibling.
        // When the |child| object will be moved, its firstLetter will be recreated,
        // so saving it now in nextSibling would leave us with a stale object.
        if (is<RenderTextFragment>(*child) && is<RenderText>(nextSibling)) {
            RenderObject* firstLetterObj = nullptr;
            if (RenderBlock* block = downcast<RenderTextFragment>(*child).blockForAccompanyingFirstLetter()) {
                RenderElement* firstLetterContainer = nullptr;
                block->getFirstLetter(firstLetterObj, firstLetterContainer, child);
            }

            // This is the first letter, skip it.
            if (firstLetterObj == nextSibling)
                nextSibling = nextSibling->nextSibling();
        }

        move(from, to, *child, beforeChild, normalizeAfterInsertion);
        child = nextSibling;
    }
}

void RenderTreeBuilder::moveAllChildrenIncludingFloats(RenderBlock& from, RenderBlock& to, RenderTreeBuilder::NormalizeAfterInsertion normalizeAfterInsertion)
{
    if (is<RenderBlockFlow>(from)) {
        blockFlowBuilder().moveAllChildrenIncludingFloats(downcast<RenderBlockFlow>(from), to, normalizeAfterInsertion);
        return;
    }
    moveAllChildren(from, to, normalizeAfterInsertion);
}

void RenderTreeBuilder::normalizeTreeAfterStyleChange(RenderElement& renderer, RenderStyle& oldStyle)
{
    if (!renderer.parent())
        return;

    bool wasFloating = oldStyle.isFloating();
    bool wasOutOfFlowPositioned = oldStyle.hasOutOfFlowPosition();
    bool isFloating = renderer.style().isFloating();
    bool isOutOfFlowPositioned = renderer.style().hasOutOfFlowPosition();
    bool startsAffectingParent = false;
    bool noLongerAffectsParent = false;
    auto handleFragmentedFlowStateChange = [&] {
        if (!renderer.parent())
            return;
        // Out of flow children of RenderMultiColumnFlow are not really part of the multicolumn flow. We need to ensure that changes in positioning like this
        // trigger insertions into the multicolumn flow.
        if (auto* enclosingFragmentedFlow = renderer.parent()->enclosingFragmentedFlow(); is<RenderMultiColumnFlow>(enclosingFragmentedFlow)) {
            auto movingIntoMulticolumn = [&] {
                if (wasOutOfFlowPositioned && !isOutOfFlowPositioned)
                    return true;
                if (auto* containingBlock = renderer.containingBlock(); containingBlock && isOutOfFlowPositioned) {
                    // Sometimes the flow state could change even when the renderer stays out-of-flow (e.g when going from fixed to absolute and
                    // the containing block is inside a multi-column flow).
                    return containingBlock->fragmentedFlowState() == RenderObject::InsideInFragmentedFlow
                        && renderer.fragmentedFlowState() == RenderObject::NotInsideFragmentedFlow;
                }
                return false;
            }();
            if (movingIntoMulticolumn) {
                renderer.initializeFragmentedFlowStateOnInsertion();
                multiColumnBuilder().multiColumnDescendantInserted(downcast<RenderMultiColumnFlow>(*enclosingFragmentedFlow), renderer);
                return;
            }
            auto movingOutOfMulticolumn = !wasOutOfFlowPositioned && isOutOfFlowPositioned;
            if (movingOutOfMulticolumn) {
                multiColumnBuilder().restoreColumnSpannersForContainer(renderer, downcast<RenderMultiColumnFlow>(*enclosingFragmentedFlow));
                return;
            }

            // Style change may have moved some subtree out of the fragmented flow. Their flow states have already been updated (see adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded)
            // and here is where we take care of the remaining, spanner tree mutation.
            WeakHashSet<RenderElement> spannerContainingBlockSet;
            for (auto& descendant : descendantsOfType<RenderMultiColumnSpannerPlaceholder>(renderer)) {
                if (auto* containingBlock = descendant.containingBlock(); containingBlock && containingBlock->enclosingFragmentedFlow() != enclosingFragmentedFlow)
                    spannerContainingBlockSet.add(*containingBlock);
            }
            auto oldEnclosingFragmentedFlow = WeakPtr { *enclosingFragmentedFlow };
            for (auto& containingBlock : spannerContainingBlockSet) {
                if (!oldEnclosingFragmentedFlow)
                    break;
                multiColumnBuilder().restoreColumnSpannersForContainer(containingBlock, downcast<RenderMultiColumnFlow>(*oldEnclosingFragmentedFlow));
            }
        }
    };

    auto& parent = *renderer.parent();
    if (is<RenderBlock>(parent))
        noLongerAffectsParent = (!wasFloating && isFloating) || (!wasOutOfFlowPositioned && isOutOfFlowPositioned);

    if (is<RenderBlockFlow>(parent) || is<RenderInline>(parent)) {
        startsAffectingParent = (wasFloating || wasOutOfFlowPositioned) && !isFloating && !isOutOfFlowPositioned;
        ASSERT(!startsAffectingParent || !noLongerAffectsParent);
    }

    if (startsAffectingParent) {
        // We have gone from not affecting the inline status of the parent flow to suddenly
        // having an impact. See if there is a mismatch between the parent flow's
        // childrenInline() state and our state.
        if (renderer.isInline() != renderer.parent()->childrenInline())
            childFlowStateChangesAndAffectsParentBlock(renderer);
        // WARNING: original parent might be deleted at this point.
        handleFragmentedFlowStateChange();
        return;
    }

    if (noLongerAffectsParent) {
        childFlowStateChangesAndNoLongerAffectsParentBlock(renderer);

        if (isFloating && is<RenderBlockFlow>(renderer)) {
            auto clearDescendantFloats = [&] {
                // These descendent floats can not intrude other, sibling block containers anymore.
                for (auto& descendant : descendantsOfType<RenderBox>(renderer)) {
                    if (descendant.isFloatingOrOutOfFlowPositioned())
                        descendant.removeFloatingOrPositionedChildFromBlockLists();
                }
            };
            clearDescendantFloats();
            downcast<RenderBlockFlow>(renderer).removeFloatingObjects();
            // Fresh floats need to be reparented if they actually belong to the previous anonymous block.
            // It copies the logic of RenderBlock::addChildIgnoringContinuation
            if (renderer.previousSibling() && renderer.previousSibling()->isAnonymousBlock())
                move(downcast<RenderBoxModelObject>(parent), downcast<RenderBoxModelObject>(*renderer.previousSibling()), renderer, RenderTreeBuilder::NormalizeAfterInsertion::No);
        }
    }

    handleFragmentedFlowStateChange();
}

void RenderTreeBuilder::makeChildrenNonInline(RenderBlock& parent, RenderObject* insertionPoint)
{
    // makeChildrenNonInline takes a block whose children are *all* inline and it
    // makes sure that inline children are coalesced under anonymous
    // blocks. If |insertionPoint| is defined, then it represents the insertion point for
    // the new block child that is causing us to have to wrap all the inlines. This
    // means that we cannot coalesce inlines before |insertionPoint| with inlines following
    // |insertionPoint|, because the new child is going to be inserted in between the inlines,
    // splitting them.
    ASSERT(parent.isInlineBlockOrInlineTable() || !parent.isInline());
    ASSERT(!insertionPoint || insertionPoint->parent() == &parent);

    parent.setChildrenInline(false);

    auto* child = parent.firstChild();
    if (!child)
        return;

    parent.deleteLines();

    while (child) {
        RenderObject* inlineRunStart = nullptr;
        RenderObject* inlineRunEnd = nullptr;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        auto newBlock = parent.createAnonymousBlock();
        auto& block = *newBlock;
        attachToRenderElementInternal(parent, WTFMove(newBlock), inlineRunStart);
        moveChildren(parent, block, inlineRunStart, child, RenderTreeBuilder::NormalizeAfterInsertion::No);
    }
#ifndef NDEBUG
    for (RenderObject* c = parent.firstChild(); c; c = c->nextSibling())
        ASSERT(!c->isInline());
#endif
    parent.repaint();
}

RenderObject* RenderTreeBuilder::splitAnonymousBoxesAroundChild(RenderBox& parent, RenderObject& originalBeforeChild)
{
    // Adjust beforeChild if it is a column spanner and has been moved out of its original position.
    auto* beforeChild = RenderTreeBuilder::MultiColumn::adjustBeforeChildForMultiColumnSpannerIfNeeded(originalBeforeChild);
    bool didSplitParentAnonymousBoxes = false;

    while (beforeChild->parent() != &parent) {
        auto& boxToSplit = downcast<RenderBox>(*beforeChild->parent());
        if (boxToSplit.firstChild() != beforeChild && boxToSplit.isAnonymous()) {
            didSplitParentAnonymousBoxes = true;

            // We have to split the parent box into two boxes and move children
            // from |beforeChild| to end into the new post box.
            auto newPostBox = boxToSplit.createAnonymousBoxWithSameTypeAs(parent);
            auto& postBox = *newPostBox;
            postBox.setChildrenInline(boxToSplit.childrenInline());
            RenderBox* parentBox = downcast<RenderBox>(boxToSplit.parent());
            // We need to invalidate the |parentBox| before inserting the new node
            // so that the table repainting logic knows the structure is dirty.
            // See for example RenderTableCell:clippedOverflowRectForRepaint.
            markBoxForRelayoutAfterSplit(*parentBox);
            attachToRenderElementInternal(*parentBox, WTFMove(newPostBox), boxToSplit.nextSibling());
            moveChildren(boxToSplit, postBox, beforeChild, nullptr, RenderTreeBuilder::NormalizeAfterInsertion::Yes);

            markBoxForRelayoutAfterSplit(boxToSplit);
            markBoxForRelayoutAfterSplit(postBox);

            beforeChild = &postBox;
        } else
            beforeChild = &boxToSplit;
    }

    if (didSplitParentAnonymousBoxes)
        markBoxForRelayoutAfterSplit(parent);

    ASSERT(beforeChild->parent() == &parent);
    return beforeChild;
}

void RenderTreeBuilder::childFlowStateChangesAndAffectsParentBlock(RenderElement& child)
{
    if (!child.isInline()) {
        WeakPtr parent = child.parent();
        if (is<RenderBlock>(*parent))
            blockBuilder().childBecameNonInline(downcast<RenderBlock>(*parent), child);
        else if (is<RenderInline>(*parent))
            inlineBuilder().childBecameNonInline(downcast<RenderInline>(*parent), child);
        // WARNING: original parent might be deleted at this point.
        if (auto* newParent = child.parent(); newParent != parent && is<RenderGrid>(newParent)) {
            // We need to re-run the grid items placement if it had gained a new item.
            downcast<RenderGrid>(*newParent).dirtyGrid();
        }
        return;
    }
    // An anonymous block must be made to wrap this inline.
    auto* parent = child.parent();
    auto newBlock = downcast<RenderBlock>(*parent).createAnonymousBlock();
    auto& block = *newBlock;
    attachToRenderElementInternal(*parent, WTFMove(newBlock), &child);
    auto thisToMove = detachFromRenderElement(*parent, child);
    attachToRenderElementInternal(block, WTFMove(thisToMove));
}

void RenderTreeBuilder::removeAnonymousWrappersForInlineChildrenIfNeeded(RenderElement& parent)
{
    if (!is<RenderBlock>(parent))
        return;
    auto& blockParent = downcast<RenderBlock>(parent);
    if (!blockParent.canDropAnonymousBlockChild())
        return;

    // We have changed to floated or out-of-flow positioning so maybe all our parent's
    // children can be inline now. Bail if there are any block children left on the line,
    // otherwise we can proceed to stripping solitary anonymous wrappers from the inlines.
    // FIXME: We should also handle split inlines here - we exclude them at the moment by returning
    // if we find a continuation.
    std::optional<bool> shouldAllChildrenBeInline;
    for (auto* current = blockParent.firstChild(); current; current = current->nextSibling()) {
        if (current->style().isFloating() || current->style().hasOutOfFlowPosition())
            continue;
        if (!current->isAnonymousBlock() || downcast<RenderBlock>(*current).isContinuation())
            return;
        // Anonymous block not in continuation. Check if it holds a set of inline or block children and try not to mix them.
        auto* firstChild = current->firstChildSlow();
        if (!firstChild)
            continue;
        auto isInlineLevelBox = firstChild->isInline();
        if (!shouldAllChildrenBeInline.has_value()) {
            shouldAllChildrenBeInline = isInlineLevelBox;
            continue;
        }
        // Mixing inline and block level boxes?
        if (*shouldAllChildrenBeInline != isInlineLevelBox)
            return;
    }

    RenderObject* next = nullptr;
    for (auto* current = blockParent.firstChild(); current; current = next) {
        next = current->nextSibling();
        if (current->isAnonymousBlock())
            blockBuilder().dropAnonymousBoxChild(blockParent, downcast<RenderBlock>(*current));
    }
}

void RenderTreeBuilder::childFlowStateChangesAndNoLongerAffectsParentBlock(RenderElement& child)
{
    ASSERT(child.parent());
    removeAnonymousWrappersForInlineChildrenIfNeeded(*child.parent());
}

void RenderTreeBuilder::destroyAndCleanUpAnonymousWrappers(RenderObject& rendererToDestroy)
{
    // If the tree is destroyed, there is no need for a clean-up phase.
    if (rendererToDestroy.renderTreeBeingDestroyed()) {
        destroy(rendererToDestroy);
        return;
    }

    // Also destroy ::backdrop along with element if there is one
    if (is<RenderElement>(rendererToDestroy)) {
        if (auto backdropRenderer = downcast<RenderElement>(rendererToDestroy).backdropRenderer())
            destroy(*backdropRenderer);
    }

    auto isAnonymousAndSafeToDelete = [] (const auto& renderer) {
        return renderer.isAnonymous() && !is<RenderRubyBase>(renderer) && !renderer.isRenderView() && !renderer.isRenderFragmentedFlow() && !renderer.isSVGViewportContainer();
    };

    auto destroyRootIncludingAnonymous = [&] () -> RenderObject& {
        auto* destroyRoot = &rendererToDestroy;
        while (!is<RenderView>(*destroyRoot)) {
            auto& destroyRootParent = *destroyRoot->parent();
            if (!isAnonymousAndSafeToDelete(destroyRootParent))
                break;
            bool destroyingOnlyChild = destroyRootParent.firstChild() == destroyRoot && destroyRootParent.lastChild() == destroyRoot;
            if (!destroyingOnlyChild)
                break;
            destroyRoot = &destroyRootParent;
        }
        return *destroyRoot;
    };

    auto& destroyRoot = destroyRootIncludingAnonymous();

    auto clearFloatsAndOutOfFlowPositionedObjects = [&] {
        // Remove floats and out-of-flow positioned objects from their containing block before detaching
        // the renderer from the tree. It includes all the anonymous block descendants that we are about
        // to destroy as well as part of the cleanup process below.
        if (!is<RenderElement>(destroyRoot))
            return;
        for (auto& descendant : descendantsOfType<RenderBox>(downcast<RenderElement>(destroyRoot))) {
            if (descendant.isFloatingOrOutOfFlowPositioned())
                descendant.removeFloatingOrPositionedChildFromBlockLists();
        }
        if (is<RenderBox>(destroyRoot) && destroyRoot.isFloatingOrOutOfFlowPositioned())
            downcast<RenderBox>(destroyRoot).removeFloatingOrPositionedChildFromBlockLists();
    };
    clearFloatsAndOutOfFlowPositionedObjects();

    auto collapseAndDestroyAnonymousSiblings = [&] {
        // FIXME: Probably need to handle other table parts here as well.
        if (is<RenderTableCell>(destroyRoot)) {
            tableBuilder().collapseAndDestroyAnonymousSiblingCells(downcast<RenderTableCell>(destroyRoot));
            return;
        }

        if (is<RenderTableRow>(destroyRoot)) {
            tableBuilder().collapseAndDestroyAnonymousSiblingRows(downcast<RenderTableRow>(destroyRoot));
            return;
        }
    };
    collapseAndDestroyAnonymousSiblings();

    // FIXME: Do not try to collapse/cleanup the anonymous wrappers inside destroy (see webkit.org/b/186746).
    WeakPtr destroyRootParent = *destroyRoot.parent();
    if (&rendererToDestroy != &destroyRoot) {
        // Destroy the child renderer first, before we start tearing down the anonymous wrapper ancestor chain.
        destroy(rendererToDestroy);
    }
    destroy(destroyRoot);
    if (!destroyRootParent)
        return;
    removeAnonymousWrappersForInlineChildrenIfNeeded(*destroyRootParent);

    // Anonymous parent might have become empty, try to delete it too.
    if (isAnonymousAndSafeToDelete(*destroyRootParent) && !destroyRootParent->firstChild())
        destroyAndCleanUpAnonymousWrappers(*destroyRootParent);
    // WARNING: rendererToDestroy is deleted here.
}

void RenderTreeBuilder::updateAfterDescendants(RenderElement& renderer)
{
    if (is<RenderBlock>(renderer))
        firstLetterBuilder().updateAfterDescendants(downcast<RenderBlock>(renderer));
    if (is<RenderListItem>(renderer))
        listBuilder().updateItemMarker(downcast<RenderListItem>(renderer));
    if (is<RenderBlockFlow>(renderer))
        multiColumnBuilder().updateAfterDescendants(downcast<RenderBlockFlow>(renderer));
}

RenderPtr<RenderObject> RenderTreeBuilder::detachFromRenderGrid(RenderGrid& parent, RenderObject& child)
{
    auto takenChild = blockBuilder().detach(parent, child);
    // Positioned grid items do not take up space or otherwise participate in the layout of the grid,
    // for that reason we don't need to mark the grid as dirty when they are removed.
    if (child.isOutOfFlowPositioned())
        return takenChild;

    // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
    parent.dirtyGrid();
    return takenChild;
}

RenderPtr<RenderObject> RenderTreeBuilder::detachFromRenderElement(RenderElement& parent, RenderObject& child, WillBeDestroyed willBeDestroyed)
{
    RELEASE_ASSERT_WITH_MESSAGE(!parent.view().frameView().layoutContext().layoutState(), "Layout must not mutate render tree");

    ASSERT(parent.canHaveChildren() || parent.canHaveGeneratedChildren());
    ASSERT(child.parent() == &parent);

    if (child.isFloatingOrOutOfFlowPositioned())
        downcast<RenderBox>(child).removeFloatingOrPositionedChildFromBlockLists();

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked). We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!parent.renderTreeBeingDestroyed() && child.everHadLayout()) {
        if (child.isBody())
            parent.view().repaintRootContents();
        else
            child.repaint();
        child.setNeedsLayoutAndPrefWidthsRecalc();
    }

    // If we have a line box wrapper, delete it.
    if (is<RenderBox>(child))
        downcast<RenderBox>(child).deleteLineBoxWrapper();
    else if (is<RenderLineBreak>(child))
        downcast<RenderLineBreak>(child).deleteInlineBoxWrapper();
    else if (is<RenderText>(child))
        downcast<RenderText>(child).removeAndDestroyTextBoxes();

    if (!parent.renderTreeBeingDestroyed() && is<RenderFlexibleBox>(parent) && !child.isFloatingOrOutOfFlowPositioned() && child.isBox())
        downcast<RenderFlexibleBox>(parent).clearCachedChildIntrinsicContentLogicalHeight(downcast<RenderBox>(child));

    // If child is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    if (!parent.renderTreeBeingDestroyed() && willBeDestroyed == WillBeDestroyed::Yes && child.isSelectionBorder())
        parent.frame().selection().setNeedsSelectionUpdate();

    if (!parent.renderTreeBeingDestroyed() && m_internalMovesType == RenderObject::IsInternalMove::No)
        child.resetFragmentedFlowStateOnRemoval();

    if (!parent.renderTreeBeingDestroyed())
        child.willBeRemovedFromTree(m_internalMovesType);

    // WARNING: There should be no code running between willBeRemovedFromTree() and the actual removal below.
    // This is needed to avoid race conditions where willBeRemovedFromTree() would dirty the tree's structure
    // and the code running here would force an untimely rebuilding, leaving |child| dangling.
    auto childToTake = parent.detachRendererInternal(child);

    // rendererRemovedFromTree() walks the whole subtree. We can improve performance
    // by skipping this step when destroying the entire tree.
    if (!parent.renderTreeBeingDestroyed() && is<RenderElement>(*childToTake))
        RenderCounter::rendererRemovedFromTree(downcast<RenderElement>(*childToTake));

    if (!parent.renderTreeBeingDestroyed()) {
        if (AXObjectCache* cache = parent.document().existingAXObjectCache())
            cache->childrenChanged(&parent);
    }
    return childToTake;
}

void RenderTreeBuilder::attachToRenderGrid(RenderGrid& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& newChild = *child;
    blockBuilder().attach(parent, WTFMove(child), beforeChild);

    // Positioned grid items do not take up space or otherwise participate in the layout of the grid,
    // for that reason we don't need to mark the grid as dirty when they are added.
    if (newChild.isOutOfFlowPositioned())
        return;

    // The grid needs to be recomputed as it might contain auto-placed items that
    // will change their position.
    parent.dirtyGrid();
}

void RenderTreeBuilder::reportVisuallyNonEmptyContent(const RenderElement& parent, const RenderObject& child)
{
    if (is<RenderText>(child)) {
        auto& style = parent.style();
        // FIXME: Find out how to increment the visually non empty character count when the font becomes available.
        if (style.visibility() == Visibility::Visible && !style.fontCascade().isLoadingCustomFonts()) {
            auto& textRenderer = downcast<RenderText>(child);
            m_view.frameView().incrementVisuallyNonEmptyCharacterCount(textRenderer.text());
        }
        return;
    }
    if (is<RenderHTMLCanvas>(child) || is<RenderEmbeddedObject>(child)) {
        // Actual size is not known yet, report the default intrinsic size for replaced elements.
        auto& replacedRenderer = downcast<RenderReplaced>(child);
        m_view.frameView().incrementVisuallyNonEmptyPixelCount(roundedIntSize(replacedRenderer.intrinsicSize()));
        return;
    }
    if (child.isSVGRootOrLegacySVGRoot()) {
        auto fixedSize = [] (const auto& renderer) -> std::optional<IntSize> {
            auto& style = renderer.style();
            if (!style.width().isFixed() || !style.height().isFixed())
                return { };
            return std::make_optional(IntSize { style.width().intValue(), style.height().intValue() });
        };
        // SVG content tends to have a fixed size construct. However this is known to be inaccurate in certain cases (box-sizing: border-box) or especially when the parent box is oversized.
        auto candidateSize = IntSize { };
        if (auto size = fixedSize(child))
            candidateSize = *size;
        else if (auto size = fixedSize(parent))
            candidateSize = *size;

        if (!candidateSize.isEmpty())
            m_view.frameView().incrementVisuallyNonEmptyPixelCount(candidateSize);
        return;
    }
}

void RenderTreeBuilder::markBoxForRelayoutAfterSplit(RenderBox& box)
{
    // FIXME: The table code should handle that automatically. If not,
    // we should fix it and remove the table part checks.
    if (is<RenderTable>(box)) {
        // Because we may have added some sections with already computed column structures, we need to
        // sync the table structure with them now. This avoids crashes when adding new cells to the table.
        downcast<RenderTable>(box).forceSectionsRecalc();
    } else if (is<RenderTableSection>(box))
        downcast<RenderTableSection>(box).setNeedsCellRecalc();

    box.setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTreeBuilder::removeFloatingObjects(RenderBlock& renderer)
{
    if (renderer.renderTreeBeingDestroyed())
        return;
    if (!is<RenderBlockFlow>(renderer))
        return;
    auto& blockFlow = downcast<RenderBlockFlow>(renderer);
    auto* floatingObjects = blockFlow.floatingObjectSet();
    if (!floatingObjects)
        return;
    // Here we remove the floating objects from the descendants as well.
    auto copyOfFloatingObjects = WTF::map(*floatingObjects, [](auto& floatingObject) {
        return floatingObject.get();
    });
    for (auto* floatingObject : copyOfFloatingObjects)
        floatingObject->renderer().removeFloatingOrPositionedChildFromBlockLists();
}

}
