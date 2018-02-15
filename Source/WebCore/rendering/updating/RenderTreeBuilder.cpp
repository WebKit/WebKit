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
#include "RenderGrid.h"
#include "RenderLineBreak.h"
#include "RenderMathMLFenced.h"
#include "RenderMenuList.h"
#include "RenderRuby.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderSVGContainer.h"
#include "RenderSVGInline.h"
#include "RenderSVGRoot.h"
#include "RenderTable.h"
#include "RenderTableRow.h"
#include "RenderTableSection.h"
#include "RenderText.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderBlockFlow.h"
#include "RenderTreeBuilderFirstLetter.h"
#include "RenderTreeBuilderFormControls.h"
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
    , m_firstLetterBuilder(std::make_unique<FirstLetter>(*this))
    , m_listBuilder(std::make_unique<List>(*this))
    , m_multiColumnBuilder(std::make_unique<MultiColumn>(*this))
    , m_tableBuilder(std::make_unique<Table>(*this))
    , m_rubyBuilder(std::make_unique<Ruby>(*this))
    , m_formControlsBuilder(std::make_unique<FormControls>(*this))
    , m_blockBuilder(std::make_unique<Block>(*this))
    , m_blockFlowBuilder(std::make_unique<BlockFlow>(*this))
    , m_inlineBuilder(std::make_unique<Inline>(*this))
    , m_svgBuilder(std::make_unique<SVG>(*this))
    , m_mathMLBuilder(std::make_unique<MathML>(*this))
{
    RELEASE_ASSERT(!s_current || &m_view != &s_current->m_view);
    m_previous = s_current;
    s_current = this;
}

RenderTreeBuilder::~RenderTreeBuilder()
{
    s_current = m_previous;
}

void RenderTreeBuilder::insertChild(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto insertRecursiveIfNeeded = [&](RenderElement& parentCandidate) {
        if (&parent == &parentCandidate) {
            parent.addChild(*this, WTFMove(child), beforeChild);
            return;
        }
        insertChild(parentCandidate, WTFMove(child), beforeChild);
    };

    ASSERT(&parent.view() == &m_view);

    if (is<RenderText>(beforeChild)) {
        if (auto* wrapperInline = downcast<RenderText>(*beforeChild).inlineWrapperForDisplayContents())
            beforeChild = wrapperInline;
    }

    if (is<RenderTableRow>(parent)) {
        insertRecursiveIfNeeded(tableBuilder().findOrCreateParentForChild(downcast<RenderTableRow>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderTableSection>(parent)) {
        insertRecursiveIfNeeded(tableBuilder().findOrCreateParentForChild(downcast<RenderTableSection>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderTable>(parent)) {
        insertRecursiveIfNeeded(tableBuilder().findOrCreateParentForChild(downcast<RenderTable>(parent), *child, beforeChild));
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
        rubyBuilder().insertChild(downcast<RenderRubyRun>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderButton>(parent)) {
        insertRecursiveIfNeeded(formControlsBuilder().createInnerRendererIfNeeded(downcast<RenderButton>(parent)));
        return;
    }

    if (is<RenderMenuList>(parent)) {
        insertRecursiveIfNeeded(formControlsBuilder().createInnerRendererIfNeeded(downcast<RenderMenuList>(parent)));
        return;
    }

    if (is<RenderSVGContainer>(parent)) {
        svgBuilder().insertChild(downcast<RenderSVGContainer>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderSVGInline>(parent)) {
        svgBuilder().insertChild(downcast<RenderSVGInline>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderSVGRoot>(parent)) {
        svgBuilder().insertChild(downcast<RenderSVGRoot>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderSVGText>(parent)) {
        svgBuilder().insertChild(downcast<RenderSVGText>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderMathMLFenced>(parent)) {
        mathMLBuilder().insertChild(downcast<RenderMathMLFenced>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderGrid>(parent)) {
        insertChildToRenderGrid(downcast<RenderGrid>(parent), WTFMove(child), beforeChild);
        return;
    }

    if (is<RenderInline>(parent)) {
        inlineBuilder().insertChild(downcast<RenderInline>(parent), WTFMove(child), beforeChild);
        return;
    }

    parent.addChild(*this, WTFMove(child), beforeChild);
}

RenderPtr<RenderObject> RenderTreeBuilder::takeChild(RenderElement& parent, RenderObject& child)
{
    if (is<RenderRubyAsInline>(parent))
        return rubyBuilder().takeChild(downcast<RenderRubyAsInline>(parent), child);

    if (is<RenderRubyAsBlock>(parent))
        return rubyBuilder().takeChild(downcast<RenderRubyAsBlock>(parent), child);

    if (is<RenderRubyRun>(parent))
        return rubyBuilder().takeChild(downcast<RenderRubyRun>(parent), child);

    if (is<RenderMenuList>(parent))
        return takeChildFromRenderMenuList(downcast<RenderMenuList>(parent), child);

    if (is<RenderButton>(parent))
        return takeChildFromRenderButton(downcast<RenderButton>(parent), child);

    if (is<RenderGrid>(parent))
        return takeChildFromRenderGrid(downcast<RenderGrid>(parent), child);

    if (is<RenderSVGText>(parent))
        return svgBuilder().takeChild(downcast<RenderSVGText>(parent), child);

    if (is<RenderSVGInline>(parent))
        return svgBuilder().takeChild(downcast<RenderSVGInline>(parent), child);

    if (is<RenderSVGContainer>(parent))
        return svgBuilder().takeChild(downcast<RenderSVGContainer>(parent), child);

    if (is<RenderSVGRoot>(parent))
        return svgBuilder().takeChild(downcast<RenderSVGRoot>(parent), child);

    if (is<RenderBlockFlow>(parent))
        return blockBuilder().takeChild(downcast<RenderBlockFlow>(parent), child);

    if (is<RenderBlock>(parent))
        return blockBuilder().takeChild(downcast<RenderBlock>(parent), child);

    return takeChildFromRenderElement(parent, child);
}

void RenderTreeBuilder::insertChild(RenderTreePosition& position, RenderPtr<RenderObject> child)
{
    insertChild(position.parent(), WTFMove(child), position.nextSibling());
}

void RenderTreeBuilder::insertChildToRenderElement(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (tableBuilder().childRequiresTable(parent, *child)) {
        RenderTable* table;
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : parent.lastChild();
        if (afterChild && afterChild->isAnonymous() && is<RenderTable>(*afterChild) && !afterChild->isBeforeContent())
            table = downcast<RenderTable>(afterChild);
        else {
            auto newTable = RenderTable::createAnonymousWithParentRenderer(parent);
            table = newTable.get();
            insertChild(parent, WTFMove(newTable), beforeChild);
        }

        insertChild(*table, WTFMove(child));
        return;
    }
    parent.RenderElement::insertChildInternal(WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderBlock(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    blockBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderBlockIgnoringContinuation(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    blockBuilder().insertChildIgnoringContinuation(parent, WTFMove(child), beforeChild);
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
        parent.insertChildInternal(WTFMove(newBlock), inlineRunStart);
        parent.moveChildrenTo(*this, &block, inlineRunStart, child, RenderBoxModelObject::NormalizeAfterInsertion::No);
    }
#ifndef NDEBUG
    for (RenderObject* c = parent.firstChild(); c; c = c->nextSibling())
        ASSERT(!c->isInline());
#endif
    parent.repaint();
}

RenderObject* RenderTreeBuilder::splitAnonymousBoxesAroundChild(RenderBox& parent, RenderObject* beforeChild)
{
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
            parentBox->insertChildInternal(WTFMove(newPostBox), boxToSplit.nextSibling());
            boxToSplit.moveChildrenTo(*this, &postBox, beforeChild, nullptr, RenderBoxModelObject::NormalizeAfterInsertion::Yes);

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
    } else {
        // An anonymous block must be made to wrap this inline.
        auto newBlock = downcast<RenderBlock>(*parent).createAnonymousBlock();
        auto& block = *newBlock;
        parent->insertChildInternal(WTFMove(newBlock), &child);
        auto thisToMove = takeChildFromRenderElement(*parent, child);
        block.insertChildInternal(WTFMove(thisToMove), nullptr);
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
    auto* current = blockParent.firstChild();
    while (current && ((current->isAnonymousBlock() && !downcast<RenderBlock>(*current).isContinuation()) || current->style().isFloating() || current->style().hasOutOfFlowPosition()))
        current = current->nextSibling();

    if (current)
        return;

    RenderObject* next;
    for (current = blockParent.firstChild(); current; current = next) {
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

void RenderTreeBuilder::multiColumnDescendantInserted(RenderMultiColumnFlow& flow, RenderObject& newDescendant)
{
    multiColumnBuilder().multiColumnDescendantInserted(flow, newDescendant);
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

void RenderTreeBuilder::removeFromParentAndDestroyCleaningUpAnonymousWrappers(RenderObject& child)
{
    // If the tree is destroyed, there is no need for a clean-up phase.
    if (child.renderTreeBeingDestroyed()) {
        child.removeFromParentAndDestroy(*this);
        return;
    }

    // Remove intruding floats from sibling blocks before detaching.
    if (is<RenderBox>(child) && child.isFloatingOrOutOfFlowPositioned())
        downcast<RenderBox>(child).removeFloatingOrPositionedChildFromBlockLists();
    auto& destroyRoot = findDestroyRootIncludingAnonymous(child);
    if (is<RenderTableRow>(destroyRoot))
        tableBuilder().collapseAndDestroyAnonymousSiblingRows(downcast<RenderTableRow>(destroyRoot));

    auto& destroyRootParent = *destroyRoot.parent();
    destroyRootParent.removeAndDestroyChild(*this, destroyRoot);
    removeAnonymousWrappersForInlineChildrenIfNeeded(destroyRootParent);

    // Anonymous parent might have become empty, try to delete it too.
    if (isAnonymousAndSafeToDelete(destroyRootParent) && !destroyRootParent.firstChild())
        removeFromParentAndDestroyCleaningUpAnonymousWrappers(destroyRootParent);
    // WARNING: child is deleted here.
}

void RenderTreeBuilder::insertChildToRenderInlineIgnoringContinuation(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    inlineBuilder().insertChildIgnoringContinuation(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderTable(RenderTable& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    tableBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderTableSection(RenderTableSection& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    tableBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderTableRow(RenderTableRow& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    tableBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderBlockFlow(RenderBlockFlow& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    blockFlowBuilder().insertChild(parent, WTFMove(child), beforeChild);
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

RenderPtr<RenderObject> RenderTreeBuilder::takeChildFromRenderMenuList(RenderMenuList& parent, RenderObject& child)
{
    auto* innerRenderer = parent.innerRenderer();
    if (!innerRenderer || &child == innerRenderer)
        return blockBuilder().takeChild(parent, child);
    return takeChild(*innerRenderer, child);
}

RenderPtr<RenderObject> RenderTreeBuilder::takeChildFromRenderButton(RenderButton& parent, RenderObject& child)
{
    auto* innerRenderer = parent.innerRenderer();
    if (!innerRenderer || &child == innerRenderer || child.parent() == &parent) {
        ASSERT(&child == innerRenderer || !innerRenderer);
        return blockBuilder().takeChild(parent, child);
    }
    return takeChild(*innerRenderer, child);
}

RenderPtr<RenderObject> RenderTreeBuilder::takeChildFromRenderGrid(RenderGrid& parent, RenderObject& child)
{
    auto takenChild = blockBuilder().takeChild(parent, child);
    // Positioned grid items do not take up space or otherwise participate in the layout of the grid,
    // for that reason we don't need to mark the grid as dirty when they are removed.
    if (child.isOutOfFlowPositioned())
        return takenChild;

    // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
    parent.dirtyGrid();
    return takenChild;
}

RenderPtr<RenderObject> RenderTreeBuilder::takeChildFromRenderElement(RenderElement& parent, RenderObject& child)
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

void RenderTreeBuilder::insertChildToRenderGrid(RenderGrid& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& newChild = *child;
    blockBuilder().insertChild(parent, WTFMove(child), beforeChild);

    // Positioned grid items do not take up space or otherwise participate in the layout of the grid,
    // for that reason we don't need to mark the grid as dirty when they are added.
    if (newChild.isOutOfFlowPositioned())
        return;

    // The grid needs to be recomputed as it might contain auto-placed items that
    // will change their position.
    parent.dirtyGrid();
}

}
