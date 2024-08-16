/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
#include "FrameSelection.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyRenderSVGContainer.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalFrameViewLayoutContext.h"
#include "RenderButton.h"
#include "RenderCounter.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderEmbeddedObject.h"
#include "RenderGrid.h"
#include "RenderHTMLCanvas.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderMathMLFenced.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderReplaced.h"
#include "RenderSVGInline.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderStyleInlines.h"
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
#include "RenderVideo.h"
#include "RenderView.h"
#include <wtf/SetForScope.h>

namespace WebCore {

RenderTreeBuilder* RenderTreeBuilder::s_current;

enum class IsRemoval : bool { No, Yes };
static void invalidateLineLayout(RenderObject& renderer, IsRemoval isRemoval)
{
    CheckedPtr container = LayoutIntegration::LineLayout::blockContainer(renderer);
    if (!container)
        return;
    auto shouldInvalidateLineLayoutPath = [&](auto& inlinLayout) {
        if (LayoutIntegration::LineLayout::shouldInvalidateLineLayoutPathAfterTreeMutation(*container, renderer, inlinLayout, isRemoval == IsRemoval::Yes))
            return true;
        if (isRemoval == IsRemoval::Yes)
            return !inlinLayout.removedFromTree(*renderer.parent(), renderer);
        return !inlinLayout.insertedIntoTree(*renderer.parent(), renderer);
    };
    if (auto* inlinLayout = container->modernLineLayout(); inlinLayout && shouldInvalidateLineLayoutPath(*inlinLayout))
        container->invalidateLineLayoutPath(RenderBlockFlow::InvalidationReason::InsertionOrRemoval);
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
{
    RELEASE_ASSERT(!s_current || &m_view != &s_current->m_view);
    m_previous = s_current;
    s_current = this;
}

RenderTreeBuilder::~RenderTreeBuilder()
{
    s_current = m_previous;
}

bool RenderTreeBuilder::isRebuildRootForChildren(const RenderElement& renderer)
{
    // If a (non-anonymous) child is added or removed to a rebuild root then we'll rebuild the full
    // subtree instead of trying to maintain the correct anonymous box structure on per-child basis.
    // This can greatly simplify the code needed to maintain the correct structure.

    auto display = renderer.style().display();
    if (display == DisplayType::Ruby || display == DisplayType::RubyBlock)
        return true;

    return false;
}

void RenderTreeBuilder::destroy(RenderObject& renderer, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    RELEASE_ASSERT(RenderTreeMutationDisallowedScope::isMutationAllowed());
    ASSERT(renderer.parent());

    auto notifyDescendantRenderersBeforeSubtreeTearDownIfApplicable = [&] {
        if (renderer.renderTreeBeingDestroyed())
            return;
        auto* rendererToDelete = dynamicDowncast<RenderElement>(renderer);
        if (!rendererToDelete || !rendererToDelete->firstChild())
            return;

        for (auto& descendant : descendantsOfType<RenderObject>(*rendererToDelete))
            descendant.willBeRemovedFromTree();
    };
    notifyDescendantRenderersBeforeSubtreeTearDownIfApplicable();

    auto toDestroy = detach(*renderer.parent(), renderer, WillBeDestroyed::Yes, canCollapseAnonymousBlock);

    if (auto* textFragment = dynamicDowncast<RenderTextFragment>(renderer))
        firstLetterBuilder().cleanupOnDestroy(*textFragment);

    if (auto* renderBox = dynamicDowncast<RenderBoxModelObject>(renderer))
        continuationBuilder().cleanupOnDestroy(*renderBox);

    auto tearDownSubTreeIfApplicable = [&] {
        auto* rendererToDelete = dynamicDowncast<RenderElement>(toDestroy.get());
        if (!rendererToDelete)
            return;

        auto subtreeTearDownType = SetForScope { m_tearDownType, TearDownType::SubtreeWithRootAlreadyDetached };
        while (rendererToDelete->firstChild()) {
            auto& firstChild = *rendererToDelete->firstChild();
            if (auto* node = firstChild.node())
                node->setRenderer(nullptr);
            destroy(firstChild);
        }
    };
    // FIXME: webkit.org/b/182909.
    tearDownSubTreeIfApplicable();
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
            if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(parent); blockFlow && blockFlow->multiColumnFlow()) {
                blockFlowBuilder().attach(*blockFlow, WTFMove(child), beforeChild);
                return;
            }
            attachToRenderElement(parent, WTFMove(child), beforeChild);
            return;
        }
        attachInternal(parentCandidate, WTFMove(child), beforeChild);
    };

    ASSERT(&parent.view() == &m_view);

    if (auto* beforeChildText = dynamicDowncast<RenderText>(beforeChild)) {
        if (auto* wrapperInline = beforeChildText->inlineWrapperForDisplayContents())
            beforeChild = wrapperInline;
    } else if (auto* beforeChildBox = dynamicDowncast<RenderBox>(beforeChild)) {
        // Adjust the beforeChild if it happens to be a spanner and the its actual location is inside the fragmented flow.
        if (auto* enclosingFragmentedFlow = parent.enclosingFragmentedFlow()) {
            auto columnSpannerPlaceholderForBeforeChild = [&]() -> RenderMultiColumnSpannerPlaceholder* {
                auto* multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(enclosingFragmentedFlow);
                return multiColumnFlow ? multiColumnFlow->findColumnSpannerPlaceholder(beforeChildBox) : nullptr;
            };

            if (auto* spannerPlaceholder = columnSpannerPlaceholderForBeforeChild())
                beforeChild = spannerPlaceholder;
        }
    }

    if (auto* text = dynamicDowncast<RenderSVGText>(parent)) {
        svgBuilder().attach(*text, WTFMove(child), beforeChild);
        return;
    }

    if (parent.style().display() == DisplayType::Ruby || parent.style().display() == DisplayType::RubyBlock) {
        auto& parentCandidate = rubyBuilder().findOrCreateParentForStyleBasedRubyChild(parent, *child, beforeChild);
        if (&parentCandidate == &parent) {
            rubyBuilder().attachForStyleBasedRuby(parentCandidate, WTFMove(child), beforeChild);
            return;
        }
        insertRecursiveIfNeeded(parentCandidate);
        return;
    }

    if (auto* parentBlockFlow = dynamicDowncast<RenderBlockFlow>(parent)) {
        blockFlowBuilder().attach(*parentBlockFlow, WTFMove(child), beforeChild);
        return;
    }

    if (auto* row = dynamicDowncast<RenderTableRow>(parent)) {
        auto& parentCandidate = tableBuilder().findOrCreateParentForChild(*row, *child, beforeChild);
        if (&parentCandidate == &parent) {
            tableBuilder().attach(*row, WTFMove(child), beforeChild);
            return;
        }
        insertRecursiveIfNeeded(parentCandidate);
        return;
    }

    if (auto* tableSection = dynamicDowncast<RenderTableSection>(parent)) {
        auto& parentCandidate = tableBuilder().findOrCreateParentForChild(*tableSection, *child, beforeChild);
        if (&parent == &parentCandidate) {
            tableBuilder().attach(*tableSection, WTFMove(child), beforeChild);
            return;
        }
        insertRecursiveIfNeeded(parentCandidate);
        return;
    }

    if (auto* table = dynamicDowncast<RenderTable>(parent)) {
        auto& parentCandidate = tableBuilder().findOrCreateParentForChild(*table, *child, beforeChild);
        if (&parentCandidate == &parent) {
            tableBuilder().attach(*table, WTFMove(child), beforeChild);
            return;
        }
        insertRecursiveIfNeeded(parentCandidate);
        return;
    }

    if (auto* button = dynamicDowncast<RenderButton>(parent)) {
        formControlsBuilder().attach(*button, WTFMove(child), beforeChild);
        return;
    }

    if (auto* menuList = dynamicDowncast<RenderMenuList>(parent)) {
        formControlsBuilder().attach(*menuList, WTFMove(child), beforeChild);
        return;
    }

    if (auto* container = dynamicDowncast<LegacyRenderSVGContainer>(parent)) {
        svgBuilder().attach(*container, WTFMove(child), beforeChild);
        return;
    }

    if (auto* svgInline = dynamicDowncast<RenderSVGInline>(parent)) {
        svgBuilder().attach(*svgInline, WTFMove(child), beforeChild);
        return;
    }

    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(parent)) {
        svgBuilder().attach(*svgRoot, WTFMove(child), beforeChild);
        return;
    }

    if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(parent)) {
        svgBuilder().attach(*svgRoot, WTFMove(child), beforeChild);
        return;
    }

#if ENABLE(MATHML)
    if (auto* mathMLFenced = dynamicDowncast<RenderMathMLFenced>(parent)) {
        mathMLBuilder().attach(*mathMLFenced, WTFMove(child), beforeChild);
        return;
    }
#endif

    if (auto* gridParent = dynamicDowncast<RenderGrid>(parent)) {
        attachToRenderGrid(*gridParent, WTFMove(child), beforeChild);
        return;
    }

    if (auto* parentBlock = dynamicDowncast<RenderBlock>(parent)) {
        blockBuilder().attach(*parentBlock, WTFMove(child), beforeChild);
        return;
    }

    if (auto* inlineParent = dynamicDowncast<RenderInline>(parent)) {
        inlineBuilder().attach(*inlineParent, WTFMove(child), beforeChild);
        return;
    }

    attachToRenderElement(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::attachIgnoringContinuation(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (auto* inlineParent = dynamicDowncast<RenderInline>(parent)) {
        inlineBuilder().attachIgnoringContinuation(*inlineParent, WTFMove(child), beforeChild);
        return;
    }

    if (auto* parentBlock = dynamicDowncast<RenderBlock>(parent)) {
        blockBuilder().attachIgnoringContinuation(*parentBlock, WTFMove(child), beforeChild);
        return;
    }

    attachInternal(parent, WTFMove(child), beforeChild);
}

RenderPtr<RenderObject> RenderTreeBuilder::detach(RenderElement& parent, RenderObject& child, WillBeDestroyed willBeDestroyed, CanCollapseAnonymousBlock canCollapseAnonymousBlock)
{
    if (auto* text = dynamicDowncast<RenderSVGText>(parent))
        return svgBuilder().detach(*text, child, willBeDestroyed);

    if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(parent))
        return blockBuilder().detach(*blockFlow, child, willBeDestroyed, canCollapseAnonymousBlock);

    if (auto* menuList = dynamicDowncast<RenderMenuList>(parent))
        return formControlsBuilder().detach(*menuList, child, willBeDestroyed);

    if (auto* button = dynamicDowncast<RenderButton>(parent))
        return formControlsBuilder().detach(*button, child, willBeDestroyed);

    if (auto* grid = dynamicDowncast<RenderGrid>(parent))
        return detachFromRenderGrid(*grid, child, willBeDestroyed);

    if (auto* svgInline = dynamicDowncast<RenderSVGInline>(parent))
        return svgBuilder().detach(*svgInline, child, willBeDestroyed);

    if (auto* container = dynamicDowncast<LegacyRenderSVGContainer>(parent))
        return svgBuilder().detach(*container, child, willBeDestroyed);

    if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(parent))
        return svgBuilder().detach(*svgRoot, child, willBeDestroyed);

    if (auto* block = dynamicDowncast<RenderBlock>(parent))
        return blockBuilder().detach(*block, child, willBeDestroyed, canCollapseAnonymousBlock);

    return detachFromRenderElement(parent, child, willBeDestroyed);
}

void RenderTreeBuilder::attachToRenderElement(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (tableBuilder().childRequiresTable(parent, *child)) {
        RenderTable* table;
        auto* afterChild = dynamicDowncast<RenderTable>(beforeChild ? beforeChild->previousSibling() : parent.lastChild());
        if (afterChild && afterChild->isAnonymous() && !afterChild->isBeforeContent())
            table = afterChild;
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
    ASSERT(!parent.isRenderBlockFlow() || (!child->isRenderTableSection() && !child->isRenderTableRow() && !child->isRenderTableCell()));

    while (beforeChild && beforeChild->parent() && beforeChild->parent() != &parent)
        beforeChild = beforeChild->parent();

    if (beforeChild && !beforeChild->parent()) {
        // Should never let the RenderView be beforeChild (as who is the parent then)
        ASSERT_NOT_REACHED();
        beforeChild = nullptr;
    }

    ASSERT(!beforeChild || beforeChild->parent() == &parent);
    ASSERT(!is<RenderText>(beforeChild) || !downcast<RenderText>(*beforeChild).inlineWrapperForDisplayContents());

    // Take the ownership.
    auto* newChild = parent.attachRendererInternal(WTFMove(child), beforeChild);
    if (parent.renderTreeBeingDestroyed()) {
        ASSERT_NOT_REACHED();
        return;
    }

    newChild->insertedIntoTree();
    invalidateLineLayout(*newChild, IsRemoval::No);

    if (m_internalMovesType == IsInternalMove::No) {
        newChild->initializeFragmentedFlowStateOnInsertion();
        if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderMultiColumnFlow>(newChild->enclosingFragmentedFlow()))
            multiColumnBuilder().multiColumnDescendantInserted(*fragmentedFlow, *newChild);
        if (CheckedPtr listItemRenderer = dynamicDowncast<RenderListItem>(*newChild))
            listItemRenderer->updateListMarkerNumbers();
    }

    newChild->setNeedsLayoutAndPrefWidthsRecalc();
    auto isOutOfFlowBox = newChild->style().hasOutOfFlowPosition();
    if (!isOutOfFlowBox)
        parent.setPreferredLogicalWidthsDirty(true);

    if (!parent.normalChildNeedsLayout()) {
        if (isOutOfFlowBox) {
            // setNeedsLayoutAndPrefWidthsRecalc above already takes care of propagating dirty bits on the ancestor chain, but
            // in order to compute static position for out of flow boxes, the parent has to run normal flow layout as well (as opposed to simplified)
            // FIXME: Introduce a dirty bit to bridge the gap between parent and containing block which would
            // not trigger layout but a simple traversal all the way to the direct parent and also expand it non-direct parent cases.
            // FIXME: RenderVideo's setNeedsLayout pattern does not play well with this optimization: see webkit.org/b/276253
            if (newChild->containingBlock() == &parent && !is<RenderVideo>(*newChild))
                parent.setOutOfFlowChildNeedsStaticPositionLayout();
            else
                parent.setChildNeedsLayout();
        } else
            parent.setChildNeedsLayout();
    }

    if (AXObjectCache* cache = parent.document().axObjectCache())
        cache->childrenChanged(&parent, newChild);

    if (parent.hasOutlineAutoAncestor() || parent.outlineStyleForRepaint().outlineStyleIsAuto() == OutlineIsAuto::On)
        if (!is<RenderMultiColumnSet>(newChild->previousSibling())) 
            newChild->setHasOutlineAutoAncestor();
}

void RenderTreeBuilder::move(RenderBoxModelObject& from, RenderBoxModelObject& to, RenderObject& child, RenderObject* beforeChild, NormalizeAfterInsertion normalizeAfterInsertion)
{
    // We assume that callers have cleared their positioned objects list for child moves so the
    // positioned renderer maps don't become stale. It would be too slow to do the map lookup on each call.
    ASSERT(normalizeAfterInsertion == NormalizeAfterInsertion::No || !is<RenderBlock>(from) || !downcast<RenderBlock>(from).hasPositionedObjects());

    ASSERT(&from == child.parent());
    ASSERT(!beforeChild || &to == beforeChild->parent());
    if (normalizeAfterInsertion == NormalizeAfterInsertion::Yes && is<RenderBlock>(from) && child.isRenderBox())
        RenderBlock::removePercentHeightDescendantIfNeeded(downcast<RenderBox>(child));
    if (normalizeAfterInsertion == NormalizeAfterInsertion::Yes && (to.isRenderBlock() || to.isRenderInline())) {
        // Takes care of adding the new child correctly if toBlock and fromBlock
        // have different kind of children (block vs inline).
        auto childToMove = detachFromRenderElement(from, child, WillBeDestroyed::No);
        attach(to, WTFMove(childToMove), beforeChild);
    } else {
        auto internalMoveScope = SetForScope { m_internalMovesType, IsInternalMove::Yes };
        auto childToMove = detachFromRenderElement(from, child, WillBeDestroyed::No);
        attachToRenderElementInternal(to, WTFMove(childToMove), beforeChild);
    }

    auto findBFCRootAndDestroyInlineTree = [&] {
        auto* containingBlock = &from;
        while (containingBlock) {
            containingBlock->setNeedsLayout();
            if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(*containingBlock)) {
                blockFlow->deleteLines();
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
    if (normalizeAfterInsertion == NormalizeAfterInsertion::Yes) {
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlock>(from)) {
            blockFlow->removePositionedObjects(nullptr);
            RenderBlock::removePercentHeightDescendantIfNeeded(*blockFlow);
            removeFloatingObjects(*blockFlow);
        }
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
        if (auto* enclosingFragmentedFlow = dynamicDowncast<RenderMultiColumnFlow>(renderer.parent()->enclosingFragmentedFlow())) {
            auto movingIntoMulticolumn = [&] {
                if (wasOutOfFlowPositioned && !isOutOfFlowPositioned)
                    return true;
                if (auto* containingBlock = renderer.containingBlock(); containingBlock && isOutOfFlowPositioned) {
                    // Sometimes the flow state could change even when the renderer stays out-of-flow (e.g when going from fixed to absolute and
                    // the containing block is inside a multi-column flow).
                    return containingBlock->fragmentedFlowState() == RenderObject::FragmentedFlowState::InsideFlow
                        && renderer.fragmentedFlowState() == RenderObject::FragmentedFlowState::NotInsideFlow;
                }
                return false;
            }();
            if (movingIntoMulticolumn) {
                renderer.initializeFragmentedFlowStateOnInsertion();
                multiColumnBuilder().multiColumnDescendantInserted(*enclosingFragmentedFlow, renderer);
                return;
            }
            auto movingOutOfMulticolumn = !wasOutOfFlowPositioned && isOutOfFlowPositioned;
            if (movingOutOfMulticolumn) {
                multiColumnBuilder().restoreColumnSpannersForContainer(renderer, *enclosingFragmentedFlow);
                return;
            }

            // Style change may have moved some subtree out of the fragmented flow. Their flow states have already been updated (see adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded)
            // and here is where we take care of the remaining, spanner tree mutation.
            SingleThreadWeakHashSet<RenderElement> spannerContainingBlockSet;
            for (auto& descendant : descendantsOfType<RenderMultiColumnSpannerPlaceholder>(renderer)) {
                if (auto* containingBlock = descendant.containingBlock(); containingBlock && containingBlock->enclosingFragmentedFlow() != enclosingFragmentedFlow)
                    spannerContainingBlockSet.add(*containingBlock);
            }
            WeakPtr oldEnclosingFragmentedFlow = *enclosingFragmentedFlow;
            for (auto& containingBlock : spannerContainingBlockSet) {
                if (!oldEnclosingFragmentedFlow)
                    break;
                multiColumnBuilder().restoreColumnSpannersForContainer(containingBlock, *oldEnclosingFragmentedFlow);
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

        if (isFloating) {
            if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(renderer)) {
                auto clearDescendantFloats = [&] {
                    // These descendent floats can not intrude other, sibling block containers anymore.
                    for (auto& descendant : descendantsOfType<RenderBox>(*blockFlow)) {
                        if (descendant.isFloating())
                            descendant.removeFloatingAndInvalidateForLayout();
                    }
                };
                clearDescendantFloats();
                removeFloatingObjects(*blockFlow);
                // Fresh floats need to be reparented if they actually belong to the previous anonymous block.
                // It copies the logic of RenderBlock::addChildIgnoringContinuation
                if (blockFlow->previousSibling() && blockFlow->previousSibling()->isAnonymousBlock())
                    move(downcast<RenderBoxModelObject>(parent), downcast<RenderBoxModelObject>(*blockFlow->previousSibling()), renderer, RenderTreeBuilder::NormalizeAfterInsertion::No);
            }
        }
    }

    handleFragmentedFlowStateChange();
}

void RenderTreeBuilder::createAnonymousWrappersForInlineContent(RenderBlock& parent, RenderObject* insertionPoint)
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
        if (auto* parentBlockRenderer = dynamicDowncast<RenderBlock>(*parent))
            blockBuilder().childBecameNonInline(*parentBlockRenderer, child);
        else if (auto* parentInlineRenderer = dynamicDowncast<RenderInline>(*parent))
            inlineBuilder().childBecameNonInline(*parentInlineRenderer, child);
        // WARNING: original parent might be deleted at this point.
        if (auto* newParent = child.parent(); newParent != parent) {
            if (CheckedPtr gridRenderer = dynamicDowncast<RenderGrid>(newParent)) {
                // We need to re-run the grid items placement if it had gained a new item.
                gridRenderer->dirtyGrid();
            }
        }
        return;
    }
    // An anonymous block must be made to wrap this inline.
    auto* parent = child.parent();
    auto newBlock = downcast<RenderBlock>(*parent).createAnonymousBlock();
    auto& block = *newBlock;
    attachToRenderElementInternal(*parent, WTFMove(newBlock), &child);
    auto thisToMove = detachFromRenderElement(*parent, child, WillBeDestroyed::No);
    attachToRenderElementInternal(block, WTFMove(thisToMove));
}

void RenderTreeBuilder::removeAnonymousWrappersForInlineChildrenIfNeeded(RenderElement& parent)
{
    CheckedPtr blockParent = dynamicDowncast<RenderBlock>(parent);
    if (!blockParent || !blockParent->canDropAnonymousBlockChild())
        return;

    // We have changed to floated or out-of-flow positioning so maybe all our parent's
    // children can be inline now. Bail if there are any block children left on the line,
    // otherwise we can proceed to stripping solitary anonymous wrappers from the inlines.
    // FIXME: We should also handle split inlines here - we exclude them at the moment by returning
    // if we find a continuation.
    std::optional<bool> shouldAllChildrenBeInline;
    for (auto* current = blockParent->firstChild(); current; current = current->nextSibling()) {
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
    for (auto* current = blockParent->firstChild(); current; current = next) {
        next = current->nextSibling();
        if (current->isAnonymousBlock())
            blockBuilder().dropAnonymousBoxChild(*blockParent, downcast<RenderBlock>(*current));
    }
}

void RenderTreeBuilder::childFlowStateChangesAndNoLongerAffectsParentBlock(RenderElement& child)
{
    ASSERT(child.parent());
    removeAnonymousWrappersForInlineChildrenIfNeeded(*child.parent());
}

void RenderTreeBuilder::destroyAndCleanUpAnonymousWrappers(RenderObject& rendererToDestroy, const RenderElement* subtreeDestroyRoot)
{
    auto tearDownType = SetForScope { m_tearDownType, !subtreeDestroyRoot || &rendererToDestroy == subtreeDestroyRoot ? TearDownType::Root : TearDownType::SubtreeWithRootStillAttached };
    auto tearDownDestroyRoot = SetForScope { m_subtreeDestroyRoot, m_tearDownType == TearDownType::SubtreeWithRootStillAttached ? subtreeDestroyRoot : nullptr };

    // If the tree is destroyed, there is no need for a clean-up phase.
    if (rendererToDestroy.renderTreeBeingDestroyed()) {
        destroy(rendererToDestroy);
        return;
    }

    auto isAnonymousAndSafeToDelete = [] (const auto& renderer) {
        return renderer.isAnonymous() && !renderer.isRenderView() && !renderer.isRenderFragmentedFlow() && !renderer.isRenderSVGViewportContainer();
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

    WeakPtr destroyRoot = destroyRootIncludingAnonymous();

    auto clearFloatsAndOutOfFlowPositionedObjects = [&] {
        // Remove floats and out-of-flow positioned objects from their containing block before detaching
        // the renderer from the tree. It includes all the anonymous block descendants that we are about
        // to destroy as well as part of the cleanup process below.
        WeakPtr destroyRootElement = dynamicDowncast<RenderElement>(destroyRoot.get());
        if (!destroyRootElement)
            return;
        for (auto& descendant : descendantsOfType<RenderBox>(*destroyRootElement)) {
            if (descendant.isFloatingOrOutOfFlowPositioned())
                descendant.removeFloatingOrPositionedChildFromBlockLists();
        }
        if (CheckedPtr box = dynamicDowncast<RenderBox>(destroyRoot.get()); box && box->isFloatingOrOutOfFlowPositioned())
            box->removeFloatingOrPositionedChildFromBlockLists();
    };
    clearFloatsAndOutOfFlowPositionedObjects();

    auto collapseAndDestroyAnonymousSiblings = [&] {
        // FIXME: Probably need to handle other table parts here as well.
        if (CheckedPtr cell = dynamicDowncast<RenderTableCell>(destroyRoot.get())) {
            tableBuilder().collapseAndDestroyAnonymousSiblingCells(*cell);
            return;
        }

        if (CheckedPtr row = dynamicDowncast<RenderTableRow>(destroyRoot.get())) {
            tableBuilder().collapseAndDestroyAnonymousSiblingRows(*row);
            return;
        }
    };
    collapseAndDestroyAnonymousSiblings();

    // FIXME: Do not try to collapse/cleanup the anonymous wrappers inside destroy (see webkit.org/b/186746).
    WeakPtr destroyRootParent = destroyRoot->parent();
    if (&rendererToDestroy != destroyRoot.get()) {
        // Destroy the child renderer first, before we start tearing down the anonymous wrapper ancestor chain.
        auto anonymousDestroyRoot = SetForScope { m_anonymousDestroyRoot, destroyRoot };
        destroy(rendererToDestroy);
    }

    if (destroyRoot) {
        auto anonymousDestroyRoot = SetForScope { m_anonymousDestroyRoot, destroyRoot.get() != &rendererToDestroy ? destroyRoot : nullptr };
        destroy(*destroyRoot);
    }

    if (!destroyRootParent)
        return;

    auto anonymousDestroyRoot = SetForScope { m_anonymousDestroyRoot, destroyRootParent };
    removeAnonymousWrappersForInlineChildrenIfNeeded(*destroyRootParent);

    // Anonymous parent might have become empty, try to delete it too.
    if (isAnonymousAndSafeToDelete(*destroyRootParent) && !destroyRootParent->firstChild())
        destroyAndCleanUpAnonymousWrappers(*destroyRootParent, destroyRootParent.get());
    // WARNING: rendererToDestroy is deleted here.
}

void RenderTreeBuilder::updateAfterDescendants(RenderElement& renderer)
{
    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(renderer)) {
        svgBuilder().updateAfterDescendants(*svgRoot);
        return; // A RenderSVGRoot cannot be a RenderBlock, RenderListItem or RenderBlockFlow: early return.
    }

    // Do not early return here in any case. For example, RenderListItem derives
    // from RenderBlockFlow and indirectly from RenderBlock thus fulfilling all
    // update conditions below.
    if (auto* block = dynamicDowncast<RenderBlock>(renderer))
        firstLetterBuilder().updateAfterDescendants(*block);
    if (auto* listItem = dynamicDowncast<RenderListItem>(renderer))
        listBuilder().updateItemMarker(*listItem);
    if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(renderer))
        multiColumnBuilder().updateAfterDescendants(*blockFlow);
}

RenderPtr<RenderObject> RenderTreeBuilder::detachFromRenderGrid(RenderGrid& parent, RenderObject& child, WillBeDestroyed willBeDestroyed)
{
    auto takenChild = blockBuilder().detach(parent, child, willBeDestroyed);
    // Positioned grid items do not take up space or otherwise participate in the layout of the grid,
    // for that reason we don't need to mark the grid as dirty when they are removed.
    if (child.isOutOfFlowPositioned())
        return takenChild;

    // The grid needs to be recomputed as it might contain auto-placed items that will change their position.
    parent.dirtyGrid();
    return takenChild;
}

static void resetRendererStateOnDetach(RenderElement& parent, RenderObject& child, RenderTreeBuilder::WillBeDestroyed willBeDestroyed, RenderTreeBuilder::IsInternalMove isInternalMove)
{
    if (child.isFloatingOrOutOfFlowPositioned())
        downcast<RenderBox>(child).removeFloatingOrPositionedChildFromBlockLists();
    else if (CheckedPtr parentFlexibleBox = dynamicDowncast<RenderFlexibleBox>(parent)) {
        if (CheckedPtr childBox = dynamicDowncast<RenderBox>(child)) {
            parentFlexibleBox->clearCachedFlexItemIntrinsicContentLogicalHeight(*childBox);
            parentFlexibleBox->clearCachedMainSizeForFlexItem(*childBox);
        }
    }

    if (willBeDestroyed == RenderTreeBuilder::WillBeDestroyed::No)
        child.setNeedsLayoutAndPrefWidthsRecalc();

    // If we have a line box wrapper, delete it.
    if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(child))
        textRenderer->removeAndDestroyTextBoxes();

    if (CheckedPtr listItemRenderer = dynamicDowncast<RenderListItem>(child); listItemRenderer && isInternalMove == RenderTreeBuilder::IsInternalMove::No)
        listItemRenderer->updateListMarkerNumbers();

    // If child is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    if (willBeDestroyed == RenderTreeBuilder::WillBeDestroyed::Yes && child.isSelectionBorder())
        parent.frame().selection().setNeedsSelectionUpdate();
}

RenderPtr<RenderObject> RenderTreeBuilder::detachFromRenderElement(RenderElement& parent, RenderObject& child, WillBeDestroyed willBeDestroyed)
{
    RELEASE_ASSERT_WITH_MESSAGE(!parent.view().frameView().layoutContext().layoutState(), "Layout must not mutate render tree");
    ASSERT(parent.canHaveChildren() || parent.canHaveGeneratedChildren());
    ASSERT(child.parent() == &parent);

    if (parent.renderTreeBeingDestroyed() || m_tearDownType == TearDownType::SubtreeWithRootAlreadyDetached)
        return parent.detachRendererInternal(child);

    if (child.everHadLayout())
        resetRendererStateOnDetach(parent, child, willBeDestroyed, m_internalMovesType);

    if (m_tearDownType == RenderTreeBuilder::TearDownType::Root || is<RenderInline>(m_subtreeDestroyRoot)) {
        // In case of partial damage on the inline content (the block root is not going away), we need to initiate inline layout invalidation on leaf renderers too.
        invalidateLineLayout(child, IsRemoval::Yes);
    }

    // FIXME: Fragment state should not be such a special case.
    if (m_internalMovesType == IsInternalMove::No)
        child.resetFragmentedFlowStateOnRemoval();

    child.willBeRemovedFromTree();
    // WARNING: There should be no code running between willBeRemovedFromTree() and the actual removal below.
    // This is needed to avoid race conditions where willBeRemovedFromTree() would dirty the tree's structure
    // and the code running here would force an untimely rebuilding, leaving |child| dangling.
    auto childToTake = parent.detachRendererInternal(child);

    if (AXObjectCache* cache = parent.document().existingAXObjectCache())
        cache->childrenChanged(&parent);

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
    if (m_view.frameView().hasEnoughContentForVisualMilestones())
        return;

    if (auto* textRenderer = dynamicDowncast<RenderText>(child)) {
        auto& style = parent.style();
        // FIXME: Find out how to increment the visually non empty character count when the font becomes available.
        if (style.usedVisibility() == Visibility::Visible && !style.fontCascade().isLoadingCustomFonts())
            m_view.frameView().incrementVisuallyNonEmptyCharacterCount(textRenderer->text());
        return;
    }
    if (is<RenderHTMLCanvas>(child) || is<RenderEmbeddedObject>(child)) {
        // Actual size is not known yet, report the default intrinsic size for replaced elements.
        auto& replacedRenderer = downcast<RenderReplaced>(child);
        m_view.frameView().incrementVisuallyNonEmptyPixelCount(roundedIntSize(replacedRenderer.intrinsicSize()));
        return;
    }
    if (child.isRenderOrLegacyRenderSVGRoot()) {
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
    if (CheckedPtr table = dynamicDowncast<RenderTable>(box)) {
        // Because we may have added some sections with already computed column structures, we need to
        // sync the table structure with them now. This avoids crashes when adding new cells to the table.
        table->forceSectionsRecalc();
    } else if (CheckedPtr tableSection = dynamicDowncast<RenderTableSection>(box))
        tableSection->setNeedsCellRecalc();

    box.setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTreeBuilder::removeFloatingObjects(RenderBlock& renderer)
{
    if (renderer.renderTreeBeingDestroyed())
        return;

    auto* blockFlow = dynamicDowncast<RenderBlockFlow>(renderer);
    if (!blockFlow)
        return;

    auto* floatingObjects = blockFlow->floatingObjectSet();
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
