/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMultiColumnFlowThread.h"

#include "HitTestResult.h"
#include "LayoutState.h"
#include "RenderIterator.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderView.h"
#include "TransformState.h"

namespace WebCore {

bool RenderMultiColumnFlowThread::gShiftingSpanner = false;

RenderMultiColumnFlowThread::RenderMultiColumnFlowThread(Document& document, PassRef<RenderStyle> style)
    : RenderFlowThread(document, WTF::move(style))
    , m_lastSetWorkedOn(nullptr)
    , m_columnCount(1)
    , m_columnWidth(0)
    , m_columnHeightAvailable(0)
    , m_inLayout(false)
    , m_inBalancingPass(false)
    , m_needsHeightsRecalculation(false)
    , m_progressionIsInline(false)
    , m_progressionIsReversed(false)
    , m_beingEvacuated(false)
{
    setFlowThreadState(InsideInFlowThread);
}

RenderMultiColumnFlowThread::~RenderMultiColumnFlowThread()
{
}

void RenderMultiColumnFlowThread::removeFlowChildInfo(RenderObject* child)
{
    RenderFlowThread::removeFlowChildInfo(child);
    flowThreadRelativeWillBeRemoved(child);
}

const char* RenderMultiColumnFlowThread::renderName() const
{    
    return "RenderMultiColumnFlowThread";
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::firstMultiColumnSet() const
{
    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return nullptr;
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::lastMultiColumnSet() const
{
    for (RenderObject* sibling = multiColumnBlockFlow()->lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return nullptr;
}

RenderBox* RenderMultiColumnFlowThread::firstColumnSetOrSpanner() const
{
    if (RenderObject* sibling = nextSibling()) {
        ASSERT(sibling->isBox());
        ASSERT(sibling->isRenderMultiColumnSet() || findColumnSpannerPlaceholder(toRenderBox(sibling)));
        return toRenderBox(sibling);
    }
    return nullptr;
}

RenderBox* RenderMultiColumnFlowThread::nextColumnSetOrSpannerSiblingOf(const RenderBox* child)
{
    if (!child)
        return nullptr;
    if (RenderObject* sibling = child->nextSibling()) {
        ASSERT(sibling->isBox());
        return toRenderBox(sibling);
    }
    return nullptr;
}

RenderBox* RenderMultiColumnFlowThread::previousColumnSetOrSpannerSiblingOf(const RenderBox* child)
{
    if (!child)
        return nullptr;
    if (RenderObject* sibling = child->previousSibling()) {
        ASSERT(sibling->isBox());
        if (sibling->isRenderFlowThread())
            return nullptr;
        return toRenderBox(sibling);
    }
    return nullptr;
}

void RenderMultiColumnFlowThread::layout()
{
    ASSERT(!m_inLayout);
    m_inLayout = true;
    m_lastSetWorkedOn = nullptr;
    if (RenderBox* first = firstColumnSetOrSpanner()) {
        if (first->isRenderMultiColumnSet()) {
            m_lastSetWorkedOn = toRenderMultiColumnSet(first);
            m_lastSetWorkedOn->beginFlow(this);
        }
    }
    RenderFlowThread::layout();
    if (RenderMultiColumnSet* lastSet = lastMultiColumnSet()) {
        if (!nextColumnSetOrSpannerSiblingOf(lastSet))
            lastSet->endFlow(this, logicalHeight());
        lastSet->expandToEncompassFlowThreadContentsIfNeeded();
    }
    m_inLayout = false;
    m_lastSetWorkedOn = nullptr;
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::findSetRendering(RenderObject* renderer) const
{
    for (RenderMultiColumnSet* multicolSet = firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        if (multicolSet->containsRendererInFlowThread(renderer))
            return multicolSet;
    }
    return nullptr;
}

void RenderMultiColumnFlowThread::populate()
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
    ASSERT(!nextSibling());
    // Reparent children preceding the flow thread into the flow thread. It's multicol content
    // now. At this point there's obviously nothing after the flow thread, but renderers (column
    // sets and spanners) will be inserted there as we insert elements into the flow thread.
    LayoutStateDisabler layoutStateDisabler(&view());
    multicolContainer->moveChildrenTo(this, multicolContainer->firstChild(), this, true);
}

void RenderMultiColumnFlowThread::evacuateAndDestroy()
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
    m_beingEvacuated = true;
    
    // Delete the line box tree.
    deleteLines();
    
    LayoutStateDisabler layoutStateDisabler(&view());

    // First promote all children of the flow thread. Before we move them to the flow thread's
    // container, we need to unregister the flow thread, so that they aren't just re-added again to
    // the flow thread that we're trying to empty.
    multicolContainer->setMultiColumnFlowThread(nullptr);
    moveAllChildrenTo(multicolContainer, true);

    // Move spanners back to their original DOM position in the tree, and destroy the placeholders.
    SpannerMap::iterator it;
    while ((it = m_spannerMap.begin()) != m_spannerMap.end()) {
        RenderBox* spanner = it->key;
        RenderMultiColumnSpannerPlaceholder* placeholder = it->value;
        RenderBlockFlow* originalContainer = toRenderBlockFlow(placeholder->parent());
        multicolContainer->removeChild(*spanner);
        originalContainer->addChild(spanner, placeholder);
        placeholder->destroy();
        m_spannerMap.remove(it);
    }

    // Remove all sets.
    while (RenderMultiColumnSet* columnSet = firstMultiColumnSet())
        columnSet->destroy();
    
    destroy();
}

void RenderMultiColumnFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    RenderMultiColumnSet* columnSet = toRenderMultiColumnSet(renderRegion);
    if (RenderMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        RenderRegionList::iterator it = m_regionList.find(nextSet);
        ASSERT(it != m_regionList.end());
        m_regionList.insertBefore(it, columnSet);
    } else
        m_regionList.add(columnSet);
    renderRegion->setIsValid(true);
}

void RenderMultiColumnFlowThread::willBeRemovedFromTree()
{
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachRegion();
    multiColumnBlockFlow()->setMultiColumnFlowThread(nullptr);
    RenderFlowThread::willBeRemovedFromTree();
}

RenderObject* RenderMultiColumnFlowThread::resolveMovedChild(RenderObject* child) const
{
    if (child->style().columnSpan() != ColumnSpanAll || !child->isBox()) {
        // We only need to resolve for column spanners.
        return child;
    }
    // The renderer for the actual DOM node that establishes a spanner is moved from its original
    // location in the render tree to becoming a sibling of the column sets. In other words, it's
    // moved out from the flow thread (and becomes a sibling of it). When we for instance want to
    // create and insert a renderer for the sibling node immediately preceding the spanner, we need
    // to map that spanner renderer to the spanner's placeholder, which is where the new inserted
    // renderer belongs.
    if (RenderMultiColumnSpannerPlaceholder* placeholder = findColumnSpannerPlaceholder(toRenderBox(child)))
        return placeholder;

    // This is an invalid spanner, or its placeholder hasn't been created yet. This happens when
    // moving an entire subtree into the flow thread, when we are processing the insertion of this
    // spanner's preceding sibling, and we obviously haven't got as far as processing this spanner
    // yet.
    return child;
}

static bool isValidColumnSpanner(RenderMultiColumnFlowThread* flowThread, RenderObject* descendant)
{
    // We assume that we're inside the flow thread. This function is not to be called otherwise.
    ASSERT(descendant->isDescendantOf(flowThread));

    // First make sure that the renderer itself has the right properties for becoming a spanner.
    RenderStyle& style = descendant->style();
    if (style.columnSpan() != ColumnSpanAll || !descendant->isBox() || descendant->isFloatingOrOutOfFlowPositioned())
        return false;

    RenderBlock* container = descendant->containingBlock();
    if (!container->isRenderBlockFlow() || container->childrenInline()) {
        // Needs to be block-level.
        return false;
    }
    
    // We need to have the flow thread as the containing block. A spanner cannot break out of the flow thread.
    RenderFlowThread* enclosingFlowThread = descendant->flowThreadContainingBlock();
    if (enclosingFlowThread != flowThread)
        return false;

    // This looks like a spanner, but if we're inside something unbreakable, it's not to be treated as one.
    for (RenderBox* ancestor = toRenderBox(descendant)->parentBox(); ancestor; ancestor = ancestor->parentBox()) {
        if (ancestor->isRenderFlowThread()) {
            // Don't allow any intervening non-multicol fragmentation contexts. The spec doesn't say
            // anything about disallowing this, but it's just going to be too complicated to
            // implement (not to mention specify behavior).
            return ancestor == flowThread;
        }
        ASSERT(ancestor->style().columnSpan() != ColumnSpanAll || !isValidColumnSpanner(flowThread, ancestor));
        if (ancestor->isUnsplittableForPagination())
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void RenderMultiColumnFlowThread::flowThreadDescendantInserted(RenderObject* descendant)
{
    if (gShiftingSpanner || m_beingEvacuated || descendant->isInFlowRenderFlowThread())
        return;
    RenderObject* subtreeRoot = descendant;
    for (; descendant; descendant = (descendant ? descendant->nextInPreOrder(subtreeRoot) : nullptr)) {
        if (descendant->isRenderMultiColumnSpannerPlaceholder()) {
            // A spanner's placeholder has been inserted. The actual spanner renderer is moved from
            // where it would otherwise occur (if it weren't a spanner) to becoming a sibling of the
            // column sets.
            RenderMultiColumnSpannerPlaceholder* placeholder = toRenderMultiColumnSpannerPlaceholder(descendant);
            if (placeholder->flowThread() != this) {
                // This isn't our spanner! It shifted here from an ancestor multicolumn block. It's going to end up
                // becoming our spanner instead, but for it to do that we first have to nuke the original spanner,
                // and get the spanner content back into this flow thread.
                RenderBox* spanner = placeholder->spanner();
                
                // Get info for the move of the original content back into our flow thread.
                RenderBoxModelObject* placeholderParent = toRenderBoxModelObject(placeholder->parent());
            
                // We have to nuke the placeholder, since the ancestor already lost the mapping to it when
                // we shifted the placeholder down into this flow thread.
                RenderObject* placeholderNextSibling = placeholderParent->removeChild(*placeholder);

                // Get the ancestor multicolumn flow thread to clean up its mess.
                RenderBlockFlow* ancestorBlock = toRenderBlockFlow(spanner->parent());
                ancestorBlock->multiColumnFlowThread()->flowThreadRelativeWillBeRemoved(spanner);
                
                // Now move the original content into our flow thread. It will end up calling flowThreadDescendantInserted
                // on the new content only, and everything will get set up properly.
                ancestorBlock->moveChildTo(placeholderParent, spanner, placeholderNextSibling, true);
                
                // Advance descendant.
                descendant = placeholderNextSibling;
                
                // If the spanner was the subtree root, then we're done, since there is nothing else left to insert.
                if (!descendant)
                    return;

                // Now that we have done this, we can continue past the spanning content, since we advanced
                // descendant already.
                if (descendant)
                    descendant = descendant->previousInPreOrder(subtreeRoot);

                continue;
            }
            
            ASSERT(!m_spannerMap.get(placeholder->spanner()));
            m_spannerMap.add(placeholder->spanner(), placeholder);
            ASSERT(!placeholder->firstChild()); // There should be no children here, but if there are, we ought to skip them.
            continue;
        }
        RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
        RenderObject* nextRendererInFlowThread = descendant->nextInPreOrderAfterChildren(this);
        RenderObject* insertBeforeMulticolChild = nullptr;
        if (isValidColumnSpanner(this, descendant)) {
            // This is a spanner (column-span:all). Such renderers are moved from where they would
            // otherwise occur in the render tree to becoming a direct child of the multicol container,
            // so that they live among the column sets. This simplifies the layout implementation, and
            // basically just relies on regular block layout done by the RenderBlockFlow that
            // establishes the multicol container.
            RenderBlockFlow* container = toRenderBlockFlow(descendant->parent());
            RenderMultiColumnSet* setToSplit = nullptr;
            if (nextRendererInFlowThread) {
                setToSplit = findSetRendering(descendant);
                if (setToSplit) {
                    setToSplit->setNeedsLayout();
                    insertBeforeMulticolChild = setToSplit->nextSibling();
                }
            }
            // Moving a spanner's renderer so that it becomes a sibling of the column sets requires us
            // to insert an anonymous placeholder in the tree where the spanner's renderer otherwise
            // would have been. This is needed for a two reasons: We need a way of separating inline
            // content before and after the spanner, so that it becomes separate line boxes. Secondly,
            // this placeholder serves as a break point for column sets, so that, when encountered, we
            // end flowing one column set and move to the next one.
            RenderMultiColumnSpannerPlaceholder* placeholder = RenderMultiColumnSpannerPlaceholder::createAnonymous(this, toRenderBox(descendant), &container->style());
            container->addChild(placeholder, descendant->nextSibling());
            container->removeChild(*descendant);
            
            // This is a guard to stop an ancestor flow thread from processing the spanner.
            gShiftingSpanner = true;
            multicolContainer->RenderBlock::addChild(descendant, insertBeforeMulticolChild);
            gShiftingSpanner = false;
            
            // The spanner has now been moved out from the flow thread, but we don't want to
            // examine its children anyway. They are all part of the spanner and shouldn't trigger
            // creation of column sets or anything like that. Continue at its original position in
            // the tree, i.e. where the placeholder was just put.
            if (subtreeRoot == descendant)
                subtreeRoot = placeholder;
            descendant = placeholder;
        } else {
            // This is regular multicol content, i.e. not part of a spanner.
            if (nextRendererInFlowThread && nextRendererInFlowThread->isRenderMultiColumnSpannerPlaceholder()) {
                // Inserted right before a spanner. Is there a set for us there?
                RenderMultiColumnSpannerPlaceholder* placeholder = toRenderMultiColumnSpannerPlaceholder(nextRendererInFlowThread);
                if (RenderObject* previous = placeholder->spanner()->previousSibling()) {
                    if (previous->isRenderMultiColumnSet())
                        continue; // There's already a set there. Nothing to do.
                }
                insertBeforeMulticolChild = placeholder->spanner();
            } else if (RenderMultiColumnSet* lastSet = lastMultiColumnSet()) {
                // This child is not an immediate predecessor of a spanner, which means that if this
                // child precedes a spanner at all, there has to be a column set created for us there
                // already. If it doesn't precede any spanner at all, on the other hand, we need a
                // column set at the end of the multicol container. We don't really check here if the
                // child inserted precedes any spanner or not (as that's an expensive operation). Just
                // make sure we have a column set at the end. It's no big deal if it remains unused.
                if (!lastSet->nextSibling())
                    continue;
            }
        }
        // Need to create a new column set when there's no set already created. We also always insert
        // another column set after a spanner. Even if it turns out that there are no renderers
        // following the spanner, there may be bottom margins there, which take up space.
        RenderMultiColumnSet* newSet = new RenderMultiColumnSet(*this, RenderStyle::createAnonymousStyleWithDisplay(&multicolContainer->style(), BLOCK));
        newSet->initializeStyle();
        multicolContainer->RenderBlock::addChild(newSet, insertBeforeMulticolChild);
        invalidateRegions();

        // We cannot handle immediate column set siblings at the moment (and there's no need for
        // it, either). There has to be at least one spanner separating them.
        ASSERT(!previousColumnSetOrSpannerSiblingOf(newSet) || !previousColumnSetOrSpannerSiblingOf(newSet)->isRenderMultiColumnSet());
        ASSERT(!nextColumnSetOrSpannerSiblingOf(newSet) || !nextColumnSetOrSpannerSiblingOf(newSet)->isRenderMultiColumnSet());
    }
}

void RenderMultiColumnFlowThread::flowThreadRelativeWillBeRemoved(RenderObject* relative)
{
    if (m_beingEvacuated)
        return;
    invalidateRegions();
    if (relative->isRenderMultiColumnSpannerPlaceholder()) {
        // Remove the map entry for this spanner, but leave the actual spanner renderer alone. Also
        // keep the reference to the spanner, since the placeholder may be about to be re-inserted
        // in the tree.
        ASSERT(relative->isDescendantOf(this));
        m_spannerMap.remove(toRenderMultiColumnSpannerPlaceholder(relative)->spanner());
        return;
    }
    if (relative->style().columnSpan() == ColumnSpanAll) {
        if (relative->parent() != parent())
            return; // not a valid spanner.

        // The placeholder may already have been removed, but if it hasn't, do so now.
        if (RenderMultiColumnSpannerPlaceholder* placeholder = m_spannerMap.get(toRenderBox(relative))) {
            placeholder->parent()->removeChild(*placeholder);
            m_spannerMap.remove(toRenderBox(relative));
        }

        if (RenderObject* next = relative->nextSibling()) {
            if (RenderObject* previous = relative->previousSibling()) {
                if (previous->isRenderMultiColumnSet() && next->isRenderMultiColumnSet()) {
                    // Merge two sets that no longer will be separated by a spanner.
                    next->destroy();
                    previous->setNeedsLayout();
                }
            }
        }
    }
    // Note that we might end up with empty column sets if all column content is removed. That's no
    // big deal though (and locating them would be expensive), and they will be found and re-used if
    // content is added again later.
}

void RenderMultiColumnFlowThread::flowThreadDescendantBoxLaidOut(RenderBox* descendant)
{
    if (!descendant->isRenderMultiColumnSpannerPlaceholder())
        return;
    RenderMultiColumnSpannerPlaceholder* placeholder = toRenderMultiColumnSpannerPlaceholder(descendant);
    RenderBlock* container = placeholder->containingBlock();

    for (RenderBox* prev = previousColumnSetOrSpannerSiblingOf(placeholder->spanner()); prev; prev = previousColumnSetOrSpannerSiblingOf(prev)) {
        if (prev->isRenderMultiColumnSet()) {
            toRenderMultiColumnSet(prev)->endFlow(container, placeholder->logicalTop());
            break;
        }
    }

    for (RenderBox* next = nextColumnSetOrSpannerSiblingOf(placeholder->spanner()); next; next = nextColumnSetOrSpannerSiblingOf(next)) {
        if (next->isRenderMultiColumnSet()) {
            m_lastSetWorkedOn = toRenderMultiColumnSet(next);
            m_lastSetWorkedOn->beginFlow(container);
            break;
        }
    }
}

void RenderMultiColumnFlowThread::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // We simply remain at our intrinsic height.
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

LayoutUnit RenderMultiColumnFlowThread::initialLogicalWidth() const
{
    return columnWidth();
}

void RenderMultiColumnFlowThread::autoGenerateRegionsToBlockOffset(LayoutUnit offset)
{
    // This function ensures we have the correct column set information at all times.
    // For a simple multi-column layout in continuous media, only one column set child is required.
    // Once a column is nested inside an enclosing pagination context, the number of column sets
    // required becomes 2n-1, where n is the total number of nested pagination contexts. For example:
    //
    // Column layout with no enclosing pagination model = 2 * 1 - 1 = 1 column set.
    // Columns inside pages = 2 * 2 - 1 = 3 column sets (bottom of first page, all the subsequent pages, then the last page).
    // Columns inside columns inside pages = 2 * 3 - 1 = 5 column sets.
    //
    // In addition, column spans will force a column set to "split" into before/after sets around the spanning element.
    //
    // Finally, we will need to deal with columns inside regions. If regions have variable widths, then there will need
    // to be unique column sets created inside any region whose width is different from its surrounding regions. This is
    // actually pretty similar to the spanning case, in that we break up the column sets whenever the width varies.
    //
    // FIXME: Parent fragmentation contexts might require us to insert additional column sets here,
    // but we don't worry about it for now. This matches the old multi-column code. Right now our
    // goal is just feature parity with the old multi-column code so that we can switch over to the
    // new code as soon as possible. The one column set that we need has already been made during
    // render tree creation, so there's nothing to do here, for the time being.

    (void)offset; // hide warning
    ASSERT(toRenderMultiColumnSet(regionAtBlockOffset(0, offset, true, DisallowRegionAutoGeneration)));
}

void RenderMultiColumnFlowThread::setPageBreak(const RenderBlock* block, LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(block, offset)))
        multicolSet->recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnFlowThread::updateMinimumPageHeight(const RenderBlock* block, LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(block, offset)))
        multicolSet->updateMinimumColumnHeight(minHeight);
}

RenderRegion* RenderMultiColumnFlowThread::regionAtBlockOffset(const RenderBox* box, LayoutUnit offset, bool extendLastRegion, RegionAutoGenerationPolicy autoGenerationPolicy)
{
    if (!m_inLayout)
        return RenderFlowThread::regionAtBlockOffset(box, offset, extendLastRegion, autoGenerationPolicy);

    // Layout in progress. We are calculating the set heights as we speak, so the region range
    // information is not up-to-date.

    RenderMultiColumnSet* columnSet = m_lastSetWorkedOn ? m_lastSetWorkedOn : firstMultiColumnSet();
    if (!columnSet) {
        // If there's no set, bail. This multicol is empty or only consists of spanners. There
        // are no regions.
        return nullptr;
    }
    // The last set worked on is a good guess. But if we're not within the bounds, search for the
    // right one.
    if (offset < columnSet->logicalTopInFlowThread()) {
        do {
            if (RenderMultiColumnSet* prev = columnSet->previousSiblingMultiColumnSet())
                columnSet = prev;
            else
                break;
        } while (offset < columnSet->logicalTopInFlowThread());
    } else {
        while (offset >= columnSet->logicalBottomInFlowThread()) {
            RenderMultiColumnSet* next = columnSet->nextSiblingMultiColumnSet();
            if (!next || !next->hasBeenFlowed())
                break;
            columnSet = next;
        }
    }
    return columnSet;
}

void RenderMultiColumnFlowThread::setRegionRangeForBox(const RenderBox* box, RenderRegion* startRegion, RenderRegion* endRegion)
{
    // Some column sets may have zero height, which means that two or more sets may start at the
    // exact same flow thread position, which means that some parts of the code may believe that a
    // given box lives in sets that it doesn't really live in. Make some adjustments here and
    // include such sets if they are adjacent to the start and/or end regions.
    for (RenderMultiColumnSet* columnSet = toRenderMultiColumnSet(startRegion)->previousSiblingMultiColumnSet(); columnSet; columnSet = columnSet->previousSiblingMultiColumnSet()) {
        if (columnSet->logicalHeightInFlowThread())
            break;
        startRegion = columnSet;
    }
    for (RenderMultiColumnSet* columnSet = toRenderMultiColumnSet(startRegion)->nextSiblingMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet()) {
        if (columnSet->logicalHeightInFlowThread())
            break;
        endRegion = columnSet;
    }

    RenderFlowThread::setRegionRangeForBox(box, startRegion, endRegion);
}

bool RenderMultiColumnFlowThread::addForcedRegionBreak(const RenderBlock* block, LayoutUnit offset, RenderBox* /*breakChild*/, bool /*isBefore*/, LayoutUnit* offsetBreakAdjustment)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(block, offset))) {
        multicolSet->addForcedBreak(offset);
        if (offsetBreakAdjustment)
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : LayoutUnit::fromPixel(0);
        return true;
    }
    return false;
}

void RenderMultiColumnFlowThread::computeLineGridPaginationOrigin(LayoutState& layoutState) const
{
    if (!progressionIsInline())
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

LayoutSize RenderMultiColumnFlowThread::offsetFromContainer(RenderObject* enclosingContainer, const LayoutPoint& physicalPoint, bool* offsetDependsOnPoint) const
{
    ASSERT(enclosingContainer == container());

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = true;
    
    LayoutPoint translatedPhysicalPoint(physicalPoint);
    RenderRegion* region = physicalTranslationFromFlowToRegion(translatedPhysicalPoint);
    if (region)
        translatedPhysicalPoint.moveBy(region->topLeftLocation());
    
    LayoutSize offset(translatedPhysicalPoint.x(), translatedPhysicalPoint.y());
    if (enclosingContainer->isBox())
        offset -= toRenderBox(enclosingContainer)->scrolledContentOffset();
    return offset;
}
    
void RenderMultiColumnFlowThread::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    // First get the transform state's point into the block flow thread's physical coordinate space.
    parent()->mapAbsoluteToLocalPoint(mode, transformState);
    LayoutPoint transformPoint(transformState.mappedPoint());
    
    // Now walk through each region.
    const RenderMultiColumnSet* candidateColumnSet = nullptr;
    LayoutPoint candidatePoint;
    LayoutSize candidateContainerOffset;
    
    for (const auto& columnSet : childrenOfType<RenderMultiColumnSet>(*parent())) {
        candidateContainerOffset = columnSet.offsetFromContainer(parent(), LayoutPoint());
        
        candidatePoint = transformPoint - candidateContainerOffset;
        candidateColumnSet = &columnSet;
        
        // We really have no clue what to do with overflow. We'll just use the closest region to the point in that case.
        LayoutUnit pointOffset = isHorizontalWritingMode() ? candidatePoint.y() : candidatePoint.x();
        LayoutUnit regionOffset = isHorizontalWritingMode() ? columnSet.topLeftLocation().y() : columnSet.topLeftLocation().x();
        if (pointOffset < regionOffset + columnSet.logicalHeight())
            break;
    }
    
    // Once we have a good guess as to which region we hit tested through (and yes, this was just a heuristic, but it's
    // the best we could do), then we can map from the region into the flow thread.
    LayoutSize translationOffset = physicalTranslationFromRegionToFlow(candidateColumnSet, candidatePoint) + candidateContainerOffset;
    bool preserve3D = mode & UseTransforms && (parent()->style().preserves3D() || style().preserves3D());
    if (mode & UseTransforms && shouldUseTransformFromContainer(parent())) {
        TransformationMatrix t;
        getTransformFromContainer(parent(), translationOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(translationOffset.width(), translationOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
}

LayoutSize RenderMultiColumnFlowThread::physicalTranslationFromRegionToFlow(const RenderMultiColumnSet* columnSet, const LayoutPoint& physicalPoint) const
{
    LayoutPoint logicalPoint = columnSet->flipForWritingMode(physicalPoint);
    LayoutPoint translatedPoint = columnSet->translateRegionPointToFlowThread(logicalPoint);
    LayoutPoint physicalTranslatedPoint = columnSet->flipForWritingMode(translatedPoint);
    return physicalPoint - physicalTranslatedPoint;
}

RenderRegion* RenderMultiColumnFlowThread::mapFromFlowToRegion(TransformState& transformState) const
{
    if (!hasValidRegionInfo())
        return nullptr;

    // Get back into our local flow thread space.
    LayoutRect boxRect = transformState.mappedQuad().enclosingBoundingBox();
    flipForWritingMode(boxRect);

    // FIXME: We need to refactor RenderObject::absoluteQuads to be able to split the quads across regions,
    // for now we just take the center of the mapped enclosing box and map it to a column.
    LayoutPoint centerPoint = boxRect.center();
    LayoutUnit centerLogicalOffset = isHorizontalWritingMode() ? centerPoint.y() : centerPoint.x();
    RenderRegion* renderRegion = const_cast<RenderMultiColumnFlowThread*>(this)->regionAtBlockOffset(this, centerLogicalOffset, true, DisallowRegionAutoGeneration);
    if (!renderRegion)
        return nullptr;
    transformState.move(physicalTranslationOffsetFromFlowToRegion(renderRegion, centerLogicalOffset));
    return renderRegion;
}

LayoutSize RenderMultiColumnFlowThread::physicalTranslationOffsetFromFlowToRegion(const RenderRegion* renderRegion, const LayoutUnit logicalOffset) const
{
    // Now that we know which multicolumn set we hit, we need to get the appropriate translation offset for the column.
    const RenderMultiColumnSet* columnSet = toRenderMultiColumnSet(renderRegion);
    LayoutPoint translationOffset = columnSet->columnTranslationForOffset(logicalOffset);
    
    // Now we know how we want the rect to be translated into the region. At this point we're converting
    // back to physical coordinates.
    if (style().isFlippedBlocksWritingMode()) {
        LayoutRect portionRect(columnSet->flowThreadPortionRect());
        LayoutRect columnRect = columnSet->columnRectAt(0);
        LayoutUnit physicalDeltaFromPortionBottom = logicalHeight() - columnSet->logicalBottomInFlowThread();
        if (isHorizontalWritingMode())
            columnRect.setHeight(portionRect.height());
        else
            columnRect.setWidth(portionRect.width());
        columnSet->flipForWritingMode(columnRect);
        if (isHorizontalWritingMode())
            translationOffset.move(0, columnRect.y() - portionRect.y() - physicalDeltaFromPortionBottom);
        else
            translationOffset.move(columnRect.x() - portionRect.x() - physicalDeltaFromPortionBottom, 0);
    }
    
    return LayoutSize(translationOffset.x(), translationOffset.y());
}

RenderRegion* RenderMultiColumnFlowThread::physicalTranslationFromFlowToRegion(LayoutPoint& physicalPoint) const
{
    if (!hasValidRegionInfo())
        return nullptr;
    
    // Put the physical point into the flow thread's coordinate space.
    LayoutPoint logicalPoint = flipForWritingMode(physicalPoint);
    
    // Now get the region that we are in.
    LayoutUnit logicalOffset = isHorizontalWritingMode() ? logicalPoint.y() : logicalPoint.x();
    RenderRegion* renderRegion = const_cast<RenderMultiColumnFlowThread*>(this)->regionAtBlockOffset(this, logicalOffset, true, DisallowRegionAutoGeneration);
    if (!renderRegion)
        return nullptr;
    
    // Translate to the coordinate space of the region.
    LayoutSize translationOffset = physicalTranslationOffsetFromFlowToRegion(renderRegion, logicalOffset);
    
    // Now shift the physical point into the region's coordinate space.
    physicalPoint += translationOffset;
    
    return renderRegion;
}

bool RenderMultiColumnFlowThread::isPageLogicalHeightKnown() const
{
    if (RenderMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->computedColumnHeight();
    return false;
}

bool RenderMultiColumnFlowThread::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    // You cannot be inside an in-flow RenderFlowThread without a corresponding DOM node. It's better to
    // just let the ancestor figure out where we are instead.
    if (hitTestAction == HitTestBlockBackground)
        return false;
    bool inside = RenderFlowThread::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
    if (inside && !result.innerNode())
        return false;
    return inside;
}

bool RenderMultiColumnFlowThread::shouldCheckColumnBreaks() const
{
    if (!parent()->isRenderView())
        return true;
    return view().frameView().pagination().behavesLikeColumns;
}

}
