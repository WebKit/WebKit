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

#include "RenderButton.h"
#include "RenderElement.h"
#include "RenderRuby.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
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

    parent.addChild(*this, WTFMove(child), beforeChild);
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
        parent.moveChildrenTo(&block, inlineRunStart, child, RenderBoxModelObject::NormalizeAfterInsertion::No);
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
            boxToSplit.moveChildrenTo(&postBox, beforeChild, nullptr, RenderBoxModelObject::NormalizeAfterInsertion::Yes);

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
        auto thisToMove = parent->takeChildInternal(child);
        block.insertChildInternal(WTFMove(thisToMove), nullptr);
    }
}

void RenderTreeBuilder::insertChildToRenderInline(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    inlineBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderInlineIgnoringContinuation(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    inlineBuilder().insertChildIgnoringContinuation(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToSVGContainer(RenderSVGContainer& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    svgBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToSVGInline(RenderSVGInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    svgBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToSVGRoot(RenderSVGRoot& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    svgBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToSVGText(RenderSVGText& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    svgBuilder().insertChild(parent, WTFMove(child), beforeChild);
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

void RenderTreeBuilder::moveRubyChildren(RenderRubyBase& from, RenderRubyBase& to)
{
    rubyBuilder().moveChildren(from, to);
}

void RenderTreeBuilder::insertChildToRenderBlockFlow(RenderBlockFlow& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    blockFlowBuilder().insertChild(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChildToRenderMathMLFenced(RenderMathMLFenced& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    mathMLBuilder().insertChild(parent, WTFMove(child), beforeChild);
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

}
