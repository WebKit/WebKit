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
#include "Frame.h"
#include "FrameSelection.h"
#include "RenderButton.h"
#include "RenderCounter.h"
#include "RenderElement.h"
#include "RenderFullScreen.h"
#include "RenderGrid.h"
#include "RenderLineBreak.h"
#include "RenderMathMLFenced.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderRuby.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderSVGContainer.h"
#include "RenderSVGInline.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderTable.h"
#include "RenderTableRow.h"
#include "RenderTableSection.h"
#include "RenderText.h"
#include "RenderTextFragment.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderBlockFlow.h"
#include "RenderTreeBuilderContinuation.h"
#include "RenderTreeBuilderFirstLetter.h"
#include "RenderTreeBuilderFormControls.h"
#include "RenderTreeBuilderFullScreen.h"
#include "RenderTreeBuilderInline.h"
#include "RenderTreeBuilderList.h"
#include "RenderTreeBuilderMathML.h"
#include "RenderTreeBuilderMultiColumn.h"
#include "RenderTreeBuilderRuby.h"
#include "RenderTreeBuilderSVG.h"
#include "RenderTreeBuilderTable.h"

namespace WebCore {

RenderTreeBuilder* RenderTreeBuilder::s_current;

static void markBoxForRelayoutAfterSplit(RenderBox& box)
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
#if ENABLE(FULLSCREEN_API)
    , m_fullScreenBuilder(makeUnique<FullScreen>(*this))
#endif
{
    RELEASE_ASSERT(!s_current || &m_view != &s_current->m_view);
    m_previous = s_current;
    s_current = this;
}

RenderTreeBuilder::~RenderTreeBuilder()
{
    s_current = m_previous;
}

void RenderTreeBuilder::destroy(RenderObject& renderer)
{
    ASSERT(renderer.parent());
    auto toDestroy = detach(*renderer.parent(), renderer);

#if ENABLE(FULLSCREEN_API)
    if (is<RenderFullScreen>(renderer))
        fullScreenBuilder().cleanupOnDestroy(downcast<RenderFullScreen>(renderer));
#endif

    if (is<RenderTextFragment>(renderer))
        firstLetterBuilder().cleanupOnDestroy(downcast<RenderTextFragment>(renderer));

    if (is<RenderBoxModelObject>(renderer))
        continuationBuilder().cleanupOnDestroy(downcast<RenderBoxModelObject>(renderer));

    // We need to detach the subtree first so that the descendants don't have
    // access to previous/next sublings at detach().
    // FIXME: webkit.org/b/182909.
    if (!is<RenderElement>(toDestroy.get()))
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
    auto insertRecursiveIfNeeded = [&](RenderElement& parentCandidate) {
        if (&parent == &parentCandidate) {
            attachToRenderElement(parent, WTFMove(child), beforeChild);
            return;
        }
        attach(parentCandidate, WTFMove(child), beforeChild);
    };

    ASSERT(&parent.view() == &m_view);

    if (is<RenderText>(beforeChild)) {
        if (auto* wrapperInline = downcast<RenderText>(*beforeChild).inlineWrapperForDisplayContents())
            beforeChild = wrapperInline;
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

    if (is<RenderSVGContainer>(parent)) {
        svgBuilder().attach(downcast<RenderSVGContainer>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderSVGInline>(parent)) {
        svgBuilder().attach(downcast<RenderSVGInline>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderSVGRoot>(parent)) {
        svgBuilder().attach(downcast<RenderSVGRoot>(parent), WTFMove(child), beforeChild);
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

    attach(parent, WTFMove(child), beforeChild);
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

    if (is<RenderSVGContainer>(parent))
        return svgBuilder().detach(downcast<RenderSVGContainer>(parent), child);

    if (is<RenderSVGRoot>(parent))
        return svgBuilder().detach(downcast<RenderSVGRoot>(parent), child);

    if (is<RenderBlockFlow>(parent))
        return blockBuilder().detach(downcast<RenderBlockFlow>(parent), child, canCollapseAnonymousBlock);

    if (is<RenderBlock>(parent))
        return blockBuilder().detach(downcast<RenderBlock>(parent), child, canCollapseAnonymousBlock);

    return detachFromRenderElement(parent, child);
}

void RenderTreeBuilder::attach(RenderTreePosition& position, RenderPtr<RenderObject> child)
{
    attach(position.parent(), WTFMove(child), position.nextSibling());
}

#if ENABLE(FULLSCREEN_API)
void RenderTreeBuilder::createPlaceholderForFullScreen(RenderFullScreen& renderer, std::unique_ptr<RenderStyle> style, const LayoutRect& frameRect)
{
    fullScreenBuilder().createPlaceholder(renderer, WTFMove(style), frameRect);
}
#endif

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

void RenderTreeBuilder::attachToRenderElementInternal(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
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

    newChild->initializeFragmentedFlowStateOnInsertion();
    if (!parent.renderTreeBeingDestroyed()) {
        newChild->insertedIntoTree();

        auto* fragmentedFlow = newChild->enclosingFragmentedFlow();
        if (is<RenderMultiColumnFlow>(fragmentedFlow))
            multiColumnBuilder().multiColumnDescendantInserted(downcast<RenderMultiColumnFlow>(*fragmentedFlow), *newChild);

        if (is<RenderElement>(*newChild))
            RenderCounter::rendererSubtreeAttached(downcast<RenderElement>(*newChild));
    }

    newChild->setNeedsLayoutAndPrefWidthsRecalc();
    parent.setPreferredLogicalWidthsDirty(true);
    if (!parent.normalChildNeedsLayout())
        parent.setChildNeedsLayout(); // We may supply the static position for an absolute positioned child.

    if (AXObjectCache* cache = parent.document().axObjectCache())
        cache->childrenChanged(&parent, newChild);
    if (is<RenderBlockFlow>(parent))
        downcast<RenderBlockFlow>(parent).invalidateLineLayoutPath();
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
        auto childToMove = detachFromRenderElement(from, child);
        attachToRenderElementInternal(to, WTFMove(childToMove), beforeChild);
    }
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
        downcast<RenderBlock>(from).removePositionedObjects(nullptr);
        if (is<RenderBlockFlow>(from))
            downcast<RenderBlockFlow>(from).removeFloatingObjects();
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

    auto& parent = *renderer.parent();

    bool wasFloating = oldStyle.isFloating();
    bool wasOufOfFlowPositioned = oldStyle.hasOutOfFlowPosition();
    bool isFloating = renderer.style().isFloating();
    bool isOutOfFlowPositioned = renderer.style().hasOutOfFlowPosition();
    bool startsAffectingParent = false;
    bool noLongerAffectsParent = false;

    if (is<RenderBlock>(parent))
        noLongerAffectsParent = (!wasFloating && isFloating) || (!wasOufOfFlowPositioned && isOutOfFlowPositioned);

    if (is<RenderBlockFlow>(parent) || is<RenderInline>(parent)) {
        startsAffectingParent = (wasFloating || wasOufOfFlowPositioned) && !isFloating && !isOutOfFlowPositioned;
        ASSERT(!startsAffectingParent || !noLongerAffectsParent);
    }

    if (startsAffectingParent) {
        // We have gone from not affecting the inline status of the parent flow to suddenly
        // having an impact. See if there is a mismatch between the parent flow's
        // childrenInline() state and our state.
        // FIXME(186894): startsAffectingParent has clearly nothing to do with resetting the inline state.
        if (!is<RenderSVGInline>(renderer))
            renderer.setInline(renderer.style().isDisplayInlineType());
        if (renderer.isInline() != renderer.parent()->childrenInline())
            childFlowStateChangesAndAffectsParentBlock(renderer);
        return;
    }

    if (noLongerAffectsParent) {
        childFlowStateChangesAndNoLongerAffectsParentBlock(renderer);

        if (is<RenderBlockFlow>(renderer)) {
            // Fresh floats need to be reparented if they actually belong to the previous anonymous block.
            // It copies the logic of RenderBlock::addChildIgnoringContinuation
            if (isFloating && renderer.previousSibling() && renderer.previousSibling()->isAnonymousBlock())
                move(downcast<RenderBoxModelObject>(parent), downcast<RenderBoxModelObject>(*renderer.previousSibling()), renderer, RenderTreeBuilder::NormalizeAfterInsertion::No);
        }
    }
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
    auto* parent = child.parent();
    if (!child.isInline()) {
        if (is<RenderBlock>(parent))
            blockBuilder().childBecameNonInline(downcast<RenderBlock>(*parent), child);
        else if (is<RenderInline>(*parent))
            inlineBuilder().childBecameNonInline(downcast<RenderInline>(*parent), child);

        // childBecameNonInline might have re-parented us.
        if (auto* newParent = child.parent()) {
            // We need to re-run the grid items placement if it had gained a new item.
            if (newParent != parent && is<RenderGrid>(*newParent))
                downcast<RenderGrid>(*newParent).dirtyGrid();
        }
    } else {
        // An anonymous block must be made to wrap this inline.
        auto newBlock = downcast<RenderBlock>(*parent).createAnonymousBlock();
        auto& block = *newBlock;
        attachToRenderElementInternal(*parent, WTFMove(newBlock), &child);
        auto thisToMove = detachFromRenderElement(*parent, child);
        attachToRenderElementInternal(block, WTFMove(thisToMove));
    }
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
    Optional<bool> shouldAllChildrenBeInline;
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
        if (!shouldAllChildrenBeInline.hasValue()) {
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

static bool isAnonymousAndSafeToDelete(RenderElement& element)
{
    if (!element.isAnonymous())
        return false;
    if (element.isRenderView() || element.isRenderFragmentedFlow())
        return false;
    return true;
}

static RenderObject& findDestroyRootIncludingAnonymous(RenderObject& renderer)
{
    auto* destroyRoot = &renderer;
    while (true) {
        auto& destroyRootParent = *destroyRoot->parent();
        if (!isAnonymousAndSafeToDelete(destroyRootParent))
            break;
        bool destroyingOnlyChild = destroyRootParent.firstChild() == destroyRoot && destroyRootParent.lastChild() == destroyRoot;
        if (!destroyingOnlyChild)
            break;
        destroyRoot = &destroyRootParent;
    }
    return *destroyRoot;
}

void RenderTreeBuilder::destroyAndCleanUpAnonymousWrappers(RenderObject& child)
{
    // If the tree is destroyed, there is no need for a clean-up phase.
    if (child.renderTreeBeingDestroyed()) {
        destroy(child);
        return;
    }

    // Remove intruding floats from sibling blocks before detaching.
    if (is<RenderBox>(child) && child.isFloatingOrOutOfFlowPositioned())
        downcast<RenderBox>(child).removeFloatingOrPositionedChildFromBlockLists();
    auto& destroyRoot = findDestroyRootIncludingAnonymous(child);
    if (is<RenderTableRow>(destroyRoot))
        tableBuilder().collapseAndDestroyAnonymousSiblingRows(downcast<RenderTableRow>(destroyRoot));

    // FIXME: Do not try to collapse/cleanup the anonymous wrappers inside destroy (see webkit.org/b/186746).
    auto destroyRootParent = makeWeakPtr(*destroyRoot.parent());
    destroy(destroyRoot);
    if (!destroyRootParent)
        return;
    removeAnonymousWrappersForInlineChildrenIfNeeded(*destroyRootParent);

    // Anonymous parent might have become empty, try to delete it too.
    if (isAnonymousAndSafeToDelete(*destroyRootParent) && !destroyRootParent->firstChild())
        destroyAndCleanUpAnonymousWrappers(*destroyRootParent);
    // WARNING: child is deleted here.
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

RenderPtr<RenderObject> RenderTreeBuilder::detachFromRenderElement(RenderElement& parent, RenderObject& child)
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
        child.setNeedsLayoutAndPrefWidthsRecalc();
        // We only repaint |child| if we have a RenderLayer as its visual overflow may not be tracked by its parent.
        if (child.isBody())
            parent.view().repaintRootContents();
        else
            child.repaint();
    }

    // If we have a line box wrapper, delete it.
    if (is<RenderBox>(child))
        downcast<RenderBox>(child).deleteLineBoxWrapper();
    else if (is<RenderLineBreak>(child))
        downcast<RenderLineBreak>(child).deleteInlineBoxWrapper();

    if (!parent.renderTreeBeingDestroyed() && is<RenderFlexibleBox>(parent) && !child.isFloatingOrOutOfFlowPositioned() && child.isBox())
        downcast<RenderFlexibleBox>(parent).clearCachedChildIntrinsicContentLogicalHeight(downcast<RenderBox>(child));

    // If child is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    if (!parent.renderTreeBeingDestroyed() && child.isSelectionBorder())
        parent.frame().selection().setNeedsSelectionUpdate();

    if (!parent.renderTreeBeingDestroyed())
        child.willBeRemovedFromTree();

    child.resetFragmentedFlowStateOnRemoval();

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

}
