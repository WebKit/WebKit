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
#include "RenderMultiColumnFlow.h"

#include "HitTestResult.h"
#include "LayoutState.h"
#include "RenderIterator.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderView.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMultiColumnFlow);

bool RenderMultiColumnFlow::gShiftingSpanner = false;

RenderMultiColumnFlow::RenderMultiColumnFlow(Document& document, RenderStyle&& style)
    : RenderFragmentedFlow(document, WTFMove(style))
    , m_spannerMap(std::make_unique<SpannerMap>())
    , m_lastSetWorkedOn(nullptr)
    , m_columnCount(1)
    , m_columnWidth(0)
    , m_columnHeightAvailable(0)
    , m_inLayout(false)
    , m_inBalancingPass(false)
    , m_needsHeightsRecalculation(false)
    , m_progressionIsInline(false)
    , m_progressionIsReversed(false)
{
    setFragmentedFlowState(InsideInFragmentedFlow);
}

RenderMultiColumnFlow::~RenderMultiColumnFlow() = default;

const char* RenderMultiColumnFlow::renderName() const
{    
    return "RenderMultiColumnFlowThread";
}

RenderMultiColumnSet* RenderMultiColumnFlow::firstMultiColumnSet() const
{
    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (is<RenderMultiColumnSet>(*sibling))
            return downcast<RenderMultiColumnSet>(sibling);
    }
    return nullptr;
}

RenderMultiColumnSet* RenderMultiColumnFlow::lastMultiColumnSet() const
{
    for (RenderObject* sibling = multiColumnBlockFlow()->lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (is<RenderMultiColumnSet>(*sibling))
            return downcast<RenderMultiColumnSet>(sibling);
    }
    return nullptr;
}

RenderBox* RenderMultiColumnFlow::firstColumnSetOrSpanner() const
{
    if (RenderObject* sibling = nextSibling()) {
        ASSERT(is<RenderBox>(*sibling));
        ASSERT(is<RenderMultiColumnSet>(*sibling) || findColumnSpannerPlaceholder(downcast<RenderBox>(sibling)));
        return downcast<RenderBox>(sibling);
    }
    return nullptr;
}

RenderBox* RenderMultiColumnFlow::nextColumnSetOrSpannerSiblingOf(const RenderBox* child)
{
    if (!child)
        return nullptr;
    if (RenderObject* sibling = child->nextSibling())
        return downcast<RenderBox>(sibling);
    return nullptr;
}

RenderBox* RenderMultiColumnFlow::previousColumnSetOrSpannerSiblingOf(const RenderBox* child)
{
    if (!child)
        return nullptr;
    if (RenderObject* sibling = child->previousSibling()) {
        if (is<RenderFragmentedFlow>(*sibling))
            return nullptr;
        return downcast<RenderBox>(sibling);
    }
    return nullptr;
}

void RenderMultiColumnFlow::layout()
{
    ASSERT(!m_inLayout);
    m_inLayout = true;
    m_lastSetWorkedOn = nullptr;
    if (RenderBox* first = firstColumnSetOrSpanner()) {
        if (is<RenderMultiColumnSet>(*first)) {
            m_lastSetWorkedOn = downcast<RenderMultiColumnSet>(first);
            m_lastSetWorkedOn->beginFlow(this);
        }
    }
    RenderFragmentedFlow::layout();
    if (RenderMultiColumnSet* lastSet = lastMultiColumnSet()) {
        if (!nextColumnSetOrSpannerSiblingOf(lastSet))
            lastSet->endFlow(this, logicalHeight());
        lastSet->expandToEncompassFragmentedFlowContentsIfNeeded();
    }
    m_inLayout = false;
    m_lastSetWorkedOn = nullptr;
}

static RenderMultiColumnSet* findSetRendering(const RenderMultiColumnFlow& fragmentedFlow, const RenderObject& renderer)
{
    // Find the set inside which the specified renderer would be rendered.
    for (auto* multicolSet = fragmentedFlow.firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        if (multicolSet->containsRendererInFragmentedFlow(renderer))
            return multicolSet;
    }
    return nullptr;
}

void RenderMultiColumnFlow::addFragmentToThread(RenderFragmentContainer* RenderFragmentContainer)
{
    auto* columnSet = downcast<RenderMultiColumnSet>(RenderFragmentContainer);
    if (RenderMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        RenderFragmentContainerList::iterator it = m_fragmentList.find(nextSet);
        ASSERT(it != m_fragmentList.end());
        m_fragmentList.insertBefore(it, columnSet);
    } else
        m_fragmentList.add(columnSet);
    RenderFragmentContainer->setIsValid(true);
}

void RenderMultiColumnFlow::willBeRemovedFromTree()
{
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachFragment();
    RenderFragmentedFlow::willBeRemovedFromTree();
}

RenderObject* RenderMultiColumnFlow::resolveMovedChild(RenderObject* child) const
{
    if (!child)
        return nullptr;

    if (child->style().columnSpan() != ColumnSpanAll || !is<RenderBox>(*child)) {
        // We only need to resolve for column spanners.
        return child;
    }
    // The renderer for the actual DOM node that establishes a spanner is moved from its original
    // location in the render tree to becoming a sibling of the column sets. In other words, it's
    // moved out from the flow thread (and becomes a sibling of it). When we for instance want to
    // create and insert a renderer for the sibling node immediately preceding the spanner, we need
    // to map that spanner renderer to the spanner's placeholder, which is where the new inserted
    // renderer belongs.
    if (RenderMultiColumnSpannerPlaceholder* placeholder = findColumnSpannerPlaceholder(downcast<RenderBox>(child)))
        return placeholder;

    // This is an invalid spanner, or its placeholder hasn't been created yet. This happens when
    // moving an entire subtree into the flow thread, when we are processing the insertion of this
    // spanner's preceding sibling, and we obviously haven't got as far as processing this spanner
    // yet.
    return child;
}

static bool isValidColumnSpanner(const RenderMultiColumnFlow& fragmentedFlow, const RenderObject& descendant)
{
    // We assume that we're inside the flow thread. This function is not to be called otherwise.
    ASSERT(descendant.isDescendantOf(&fragmentedFlow));
    // First make sure that the renderer itself has the right properties for becoming a spanner.
    if (!is<RenderBox>(descendant))
        return false;

    auto& descendantBox = downcast<RenderBox>(descendant);
    if (descendantBox.isFloatingOrOutOfFlowPositioned())
        return false;

    if (descendantBox.style().columnSpan() != ColumnSpanAll)
        return false;

    auto* parent = descendantBox.parent();
    if (!is<RenderBlockFlow>(*parent) || parent->childrenInline()) {
        // Needs to be block-level.
        return false;
    }
    
    // We need to have the flow thread as the containing block. A spanner cannot break out of the flow thread.
    auto* enclosingFragmentedFlow = descendantBox.enclosingFragmentedFlow();
    if (enclosingFragmentedFlow != &fragmentedFlow)
        return false;

    // This looks like a spanner, but if we're inside something unbreakable, it's not to be treated as one.
    for (auto* ancestor = descendantBox.containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (is<RenderView>(*ancestor))
            return false;
        if (is<RenderFragmentedFlow>(*ancestor)) {
            // Don't allow any intervening non-multicol fragmentation contexts. The spec doesn't say
            // anything about disallowing this, but it's just going to be too complicated to
            // implement (not to mention specify behavior).
            return ancestor == &fragmentedFlow;
        }
        // This ancestor (descendent of the fragmentedFlow) will create columns later. The spanner belongs to it.
        if (is<RenderBlockFlow>(*ancestor) && downcast<RenderBlockFlow>(*ancestor).willCreateColumns())
            return false;
        ASSERT(ancestor->style().columnSpan() != ColumnSpanAll || !isValidColumnSpanner(fragmentedFlow, *ancestor));
        if (ancestor->isUnsplittableForPagination())
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static RenderObject* spannerPlacehoderCandidate(const RenderObject& renderer, const RenderMultiColumnFlow& stayWithin)
{
    // Spanner candidate is a next sibling/ancestor's next child within the flow thread and
    // it is in the same inflow/out-of-flow layout context.
    if (renderer.isOutOfFlowPositioned())
        return nullptr;

    ASSERT(renderer.isDescendantOf(&stayWithin));
    auto* current = &renderer;
    while (true) {
        // Skip to the first in-flow sibling.
        auto* nextSibling = current->nextSibling();
        while (nextSibling && nextSibling->isOutOfFlowPositioned())
            nextSibling = nextSibling->nextSibling();
        if (nextSibling)
            return nextSibling;
        // No sibling candidate, jump to the parent and check its siblings.
        current = current->parent();
        if (!current || current == &stayWithin || current->isOutOfFlowPositioned())
            return nullptr;
    }
    return nullptr;
}

RenderObject* RenderMultiColumnFlow::processPossibleSpannerDescendant(RenderObject*& subtreeRoot, RenderObject& descendant)
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
    RenderObject* nextRendererInFragmentedFlow = spannerPlacehoderCandidate(descendant, *this);
    RenderObject* insertBeforeMulticolChild = nullptr;
    RenderObject* nextDescendant = &descendant;

    if (isValidColumnSpanner(*this, descendant)) {
        // This is a spanner (column-span:all). Such renderers are moved from where they would
        // otherwise occur in the render tree to becoming a direct child of the multicol container,
        // so that they live among the column sets. This simplifies the layout implementation, and
        // basically just relies on regular block layout done by the RenderBlockFlow that
        // establishes the multicol container.
        RenderBlockFlow* container = downcast<RenderBlockFlow>(descendant.parent());
        RenderMultiColumnSet* setToSplit = nullptr;
        if (nextRendererInFragmentedFlow) {
            setToSplit = findSetRendering(*this, descendant);
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
        auto newPlaceholder = RenderMultiColumnSpannerPlaceholder::createAnonymous(*this, downcast<RenderBox>(descendant), container->style());
        auto& placeholder = *newPlaceholder;
        container->addChild(WTFMove(newPlaceholder), descendant.nextSibling());
        auto takenDescendant = container->takeChild(descendant);
        
        // This is a guard to stop an ancestor flow thread from processing the spanner.
        gShiftingSpanner = true;
        multicolContainer->RenderBlock::addChild(WTFMove(takenDescendant), insertBeforeMulticolChild);
        gShiftingSpanner = false;
        
        // The spanner has now been moved out from the flow thread, but we don't want to
        // examine its children anyway. They are all part of the spanner and shouldn't trigger
        // creation of column sets or anything like that. Continue at its original position in
        // the tree, i.e. where the placeholder was just put.
        if (subtreeRoot == &descendant)
            subtreeRoot = &placeholder;
        nextDescendant = &placeholder;
    } else {
        // This is regular multicol content, i.e. not part of a spanner.
        if (is<RenderMultiColumnSpannerPlaceholder>(nextRendererInFragmentedFlow)) {
            // Inserted right before a spanner. Is there a set for us there?
            RenderMultiColumnSpannerPlaceholder& placeholder = downcast<RenderMultiColumnSpannerPlaceholder>(*nextRendererInFragmentedFlow);
            if (RenderObject* previous = placeholder.spanner()->previousSibling()) {
                if (is<RenderMultiColumnSet>(*previous))
                    return nextDescendant; // There's already a set there. Nothing to do.
            }
            insertBeforeMulticolChild = placeholder.spanner();
        } else if (RenderMultiColumnSet* lastSet = lastMultiColumnSet()) {
            // This child is not an immediate predecessor of a spanner, which means that if this
            // child precedes a spanner at all, there has to be a column set created for us there
            // already. If it doesn't precede any spanner at all, on the other hand, we need a
            // column set at the end of the multicol container. We don't really check here if the
            // child inserted precedes any spanner or not (as that's an expensive operation). Just
            // make sure we have a column set at the end. It's no big deal if it remains unused.
            if (!lastSet->nextSibling())
                return nextDescendant;
        }
    }
    // Need to create a new column set when there's no set already created. We also always insert
    // another column set after a spanner. Even if it turns out that there are no renderers
    // following the spanner, there may be bottom margins there, which take up space.
    auto newSet = createRenderer<RenderMultiColumnSet>(*this, RenderStyle::createAnonymousStyleWithDisplay(multicolContainer->style(), BLOCK));
    newSet->initializeStyle();
    auto& set = *newSet;
    multicolContainer->RenderBlock::addChild(WTFMove(newSet), insertBeforeMulticolChild);
    invalidateFragments();

    // We cannot handle immediate column set siblings at the moment (and there's no need for
    // it, either). There has to be at least one spanner separating them.
    ASSERT_UNUSED(set, !previousColumnSetOrSpannerSiblingOf(&set) || !previousColumnSetOrSpannerSiblingOf(&set)->isRenderMultiColumnSet());
    ASSERT(!nextColumnSetOrSpannerSiblingOf(&set) || !nextColumnSetOrSpannerSiblingOf(&set)->isRenderMultiColumnSet());
    
    return nextDescendant;
}

void RenderMultiColumnFlow::fragmentedFlowDescendantInserted(RenderObject& newDescendant)
{
    if (gShiftingSpanner || newDescendant.isInFlowRenderFragmentedFlow())
        return;

    auto* subtreeRoot = &newDescendant;
    auto* descendant = subtreeRoot;
    while (descendant) {
        // Skip nested multicolumn flows.
        if (is<RenderMultiColumnFlow>(*descendant)) {
            descendant = descendant->nextSibling();
            continue;
        }
        if (is<RenderMultiColumnSpannerPlaceholder>(*descendant)) {
            // A spanner's placeholder has been inserted. The actual spanner renderer is moved from
            // where it would otherwise occur (if it weren't a spanner) to becoming a sibling of the
            // column sets.
            RenderMultiColumnSpannerPlaceholder& placeholder = downcast<RenderMultiColumnSpannerPlaceholder>(*descendant);
            ASSERT(!spannerMap().get(placeholder.spanner()));
            spannerMap().add(placeholder.spanner(), makeWeakPtr(downcast<RenderMultiColumnSpannerPlaceholder>(descendant)));
            ASSERT(!placeholder.firstChild()); // There should be no children here, but if there are, we ought to skip them.
        } else
            descendant = processPossibleSpannerDescendant(subtreeRoot, *descendant);
        if (descendant)
            descendant = descendant->nextInPreOrder(subtreeRoot);
    }
}

void RenderMultiColumnFlow::handleSpannerRemoval(RenderObject& spanner)
{
    // The placeholder may already have been removed, but if it hasn't, do so now.
    if (auto placeholder = spannerMap().take(&downcast<RenderBox>(spanner)))
        placeholder->removeFromParentAndDestroy();

    if (RenderObject* next = spanner.nextSibling()) {
        if (RenderObject* previous = spanner.previousSibling()) {
            if (previous->isRenderMultiColumnSet() && next->isRenderMultiColumnSet()) {
                // Merge two sets that no longer will be separated by a spanner.
                next->removeFromParentAndDestroy();
                previous->setNeedsLayout();
            }
        }
    }
}

void RenderMultiColumnFlow::fragmentedFlowRelativeWillBeRemoved(RenderObject& relative)
{
    invalidateFragments();
    if (is<RenderMultiColumnSpannerPlaceholder>(relative)) {
        // Remove the map entry for this spanner, but leave the actual spanner renderer alone. Also
        // keep the reference to the spanner, since the placeholder may be about to be re-inserted
        // in the tree.
        ASSERT(relative.isDescendantOf(this));
        spannerMap().remove(downcast<RenderMultiColumnSpannerPlaceholder>(relative).spanner());
        return;
    }
    if (relative.style().columnSpan() == ColumnSpanAll) {
        if (relative.parent() != parent())
            return; // not a valid spanner.
        
        handleSpannerRemoval(relative);
    }
    // Note that we might end up with empty column sets if all column content is removed. That's no
    // big deal though (and locating them would be expensive), and they will be found and re-used if
    // content is added again later.
}

void RenderMultiColumnFlow::fragmentedFlowDescendantBoxLaidOut(RenderBox* descendant)
{
    if (!is<RenderMultiColumnSpannerPlaceholder>(*descendant))
        return;
    auto& placeholder = downcast<RenderMultiColumnSpannerPlaceholder>(*descendant);
    RenderBlock* container = placeholder.containingBlock();

    for (RenderBox* prev = previousColumnSetOrSpannerSiblingOf(placeholder.spanner()); prev; prev = previousColumnSetOrSpannerSiblingOf(prev)) {
        if (is<RenderMultiColumnSet>(*prev)) {
            downcast<RenderMultiColumnSet>(*prev).endFlow(container, placeholder.logicalTop());
            break;
        }
    }

    for (RenderBox* next = nextColumnSetOrSpannerSiblingOf(placeholder.spanner()); next; next = nextColumnSetOrSpannerSiblingOf(next)) {
        if (is<RenderMultiColumnSet>(*next)) {
            m_lastSetWorkedOn = downcast<RenderMultiColumnSet>(next);
            m_lastSetWorkedOn->beginFlow(container);
            break;
        }
    }
}

RenderBox::LogicalExtentComputedValues RenderMultiColumnFlow::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    // We simply remain at our intrinsic height.
    return { logicalHeight, logicalTop, ComputedMarginValues() };
}

LayoutUnit RenderMultiColumnFlow::initialLogicalWidth() const
{
    return columnWidth();
}

void RenderMultiColumnFlow::setPageBreak(const RenderBlock* block, LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (auto* multicolSet = downcast<RenderMultiColumnSet>(fragmentAtBlockOffset(block, offset)))
        multicolSet->recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnFlow::updateMinimumPageHeight(const RenderBlock* block, LayoutUnit offset, LayoutUnit minHeight)
{
    if (auto* multicolSet = downcast<RenderMultiColumnSet>(fragmentAtBlockOffset(block, offset)))
        multicolSet->updateMinimumColumnHeight(minHeight);
}

RenderFragmentContainer* RenderMultiColumnFlow::fragmentAtBlockOffset(const RenderBox* box, LayoutUnit offset, bool extendLastFragment) const
{
    if (!m_inLayout)
        return RenderFragmentedFlow::fragmentAtBlockOffset(box, offset, extendLastFragment);

    // Layout in progress. We are calculating the set heights as we speak, so the fragment range
    // information is not up-to-date.

    RenderMultiColumnSet* columnSet = m_lastSetWorkedOn ? m_lastSetWorkedOn : firstMultiColumnSet();
    if (!columnSet) {
        // If there's no set, bail. This multicol is empty or only consists of spanners. There
        // are no fragments.
        return nullptr;
    }
    // The last set worked on is a good guess. But if we're not within the bounds, search for the
    // right one.
    if (offset < columnSet->logicalTopInFragmentedFlow()) {
        do {
            if (RenderMultiColumnSet* prev = columnSet->previousSiblingMultiColumnSet())
                columnSet = prev;
            else
                break;
        } while (offset < columnSet->logicalTopInFragmentedFlow());
    } else {
        while (offset >= columnSet->logicalBottomInFragmentedFlow()) {
            RenderMultiColumnSet* next = columnSet->nextSiblingMultiColumnSet();
            if (!next || !next->hasBeenFlowed())
                break;
            columnSet = next;
        }
    }
    return columnSet;
}

void RenderMultiColumnFlow::setFragmentRangeForBox(const RenderBox& box, RenderFragmentContainer* startFragment, RenderFragmentContainer* endFragment)
{
    // Some column sets may have zero height, which means that two or more sets may start at the
    // exact same flow thread position, which means that some parts of the code may believe that a
    // given box lives in sets that it doesn't really live in. Make some adjustments here and
    // include such sets if they are adjacent to the start and/or end fragments.
    for (RenderMultiColumnSet* columnSet = downcast<RenderMultiColumnSet>(*startFragment).previousSiblingMultiColumnSet(); columnSet; columnSet = columnSet->previousSiblingMultiColumnSet()) {
        if (columnSet->logicalHeightInFragmentedFlow())
            break;
        startFragment = columnSet;
    }
    for (RenderMultiColumnSet* columnSet = downcast<RenderMultiColumnSet>(*startFragment).nextSiblingMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet()) {
        if (columnSet->logicalHeightInFragmentedFlow())
            break;
        endFragment = columnSet;
    }

    RenderFragmentedFlow::setFragmentRangeForBox(box, startFragment, endFragment);
}

bool RenderMultiColumnFlow::addForcedFragmentBreak(const RenderBlock* block, LayoutUnit offset, RenderBox* /*breakChild*/, bool /*isBefore*/, LayoutUnit* offsetBreakAdjustment)
{
    if (auto* multicolSet = downcast<RenderMultiColumnSet>(fragmentAtBlockOffset(block, offset))) {
        multicolSet->addForcedBreak(offset);
        if (offsetBreakAdjustment)
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : LayoutUnit::fromPixel(0);
        return true;
    }
    return false;
}

LayoutSize RenderMultiColumnFlow::offsetFromContainer(RenderElement& enclosingContainer, const LayoutPoint& physicalPoint, bool* offsetDependsOnPoint) const
{
    ASSERT(&enclosingContainer == container());

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = true;
    
    LayoutPoint translatedPhysicalPoint(physicalPoint);
    if (RenderFragmentContainer* fragment = physicalTranslationFromFlowToFragment(translatedPhysicalPoint))
        translatedPhysicalPoint.moveBy(fragment->topLeftLocation());
    
    LayoutSize offset(translatedPhysicalPoint.x(), translatedPhysicalPoint.y());
    if (is<RenderBox>(enclosingContainer))
        offset -= toLayoutSize(downcast<RenderBox>(enclosingContainer).scrollPosition());
    return offset;
}
    
void RenderMultiColumnFlow::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    // First get the transform state's point into the block flow thread's physical coordinate space.
    parent()->mapAbsoluteToLocalPoint(mode, transformState);
    LayoutPoint transformPoint(transformState.mappedPoint());
    
    // Now walk through each fragment.
    const RenderMultiColumnSet* candidateColumnSet = nullptr;
    LayoutPoint candidatePoint;
    LayoutSize candidateContainerOffset;
    
    for (const auto& columnSet : childrenOfType<RenderMultiColumnSet>(*parent())) {
        candidateContainerOffset = columnSet.offsetFromContainer(*parent(), LayoutPoint());
        
        candidatePoint = transformPoint - candidateContainerOffset;
        candidateColumnSet = &columnSet;
        
        // We really have no clue what to do with overflow. We'll just use the closest fragment to the point in that case.
        LayoutUnit pointOffset = isHorizontalWritingMode() ? candidatePoint.y() : candidatePoint.x();
        LayoutUnit fragmentOffset = isHorizontalWritingMode() ? columnSet.topLeftLocation().y() : columnSet.topLeftLocation().x();
        if (pointOffset < fragmentOffset + columnSet.logicalHeight())
            break;
    }
    
    // Once we have a good guess as to which fragment we hit tested through (and yes, this was just a heuristic, but it's
    // the best we could do), then we can map from the fragment into the flow thread.
    LayoutSize translationOffset = physicalTranslationFromFragmentToFlow(candidateColumnSet, candidatePoint) + candidateContainerOffset;
    bool preserve3D = mode & UseTransforms && (parent()->style().preserves3D() || style().preserves3D());
    if (mode & UseTransforms && shouldUseTransformFromContainer(parent())) {
        TransformationMatrix t;
        getTransformFromContainer(parent(), translationOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(translationOffset.width(), translationOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
}

LayoutSize RenderMultiColumnFlow::physicalTranslationFromFragmentToFlow(const RenderMultiColumnSet* columnSet, const LayoutPoint& physicalPoint) const
{
    LayoutPoint logicalPoint = columnSet->flipForWritingMode(physicalPoint);
    LayoutPoint translatedPoint = columnSet->translateFragmentPointToFragmentedFlow(logicalPoint);
    LayoutPoint physicalTranslatedPoint = columnSet->flipForWritingMode(translatedPoint);
    return physicalPoint - physicalTranslatedPoint;
}

RenderFragmentContainer* RenderMultiColumnFlow::mapFromFlowToFragment(TransformState& transformState) const
{
    if (!hasValidFragmentInfo())
        return nullptr;

    // Get back into our local flow thread space.
    LayoutRect boxRect = transformState.mappedQuad().enclosingBoundingBox();
    flipForWritingMode(boxRect);

    // FIXME: We need to refactor RenderObject::absoluteQuads to be able to split the quads across fragments,
    // for now we just take the center of the mapped enclosing box and map it to a column.
    LayoutPoint centerPoint = boxRect.center();
    LayoutUnit centerLogicalOffset = isHorizontalWritingMode() ? centerPoint.y() : centerPoint.x();
    RenderFragmentContainer* RenderFragmentContainer = fragmentAtBlockOffset(this, centerLogicalOffset, true);
    if (!RenderFragmentContainer)
        return nullptr;
    transformState.move(physicalTranslationOffsetFromFlowToFragment(RenderFragmentContainer, centerLogicalOffset));
    return RenderFragmentContainer;
}

LayoutSize RenderMultiColumnFlow::physicalTranslationOffsetFromFlowToFragment(const RenderFragmentContainer* RenderFragmentContainer, const LayoutUnit logicalOffset) const
{
    // Now that we know which multicolumn set we hit, we need to get the appropriate translation offset for the column.
    const auto* columnSet = downcast<RenderMultiColumnSet>(RenderFragmentContainer);
    LayoutPoint translationOffset = columnSet->columnTranslationForOffset(logicalOffset);
    
    // Now we know how we want the rect to be translated into the fragment. At this point we're converting
    // back to physical coordinates.
    if (style().isFlippedBlocksWritingMode()) {
        LayoutRect portionRect(columnSet->fragmentedFlowPortionRect());
        LayoutRect columnRect = columnSet->columnRectAt(0);
        LayoutUnit physicalDeltaFromPortionBottom = logicalHeight() - columnSet->logicalBottomInFragmentedFlow();
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

RenderFragmentContainer* RenderMultiColumnFlow::physicalTranslationFromFlowToFragment(LayoutPoint& physicalPoint) const
{
    if (!hasValidFragmentInfo())
        return nullptr;
    
    // Put the physical point into the flow thread's coordinate space.
    LayoutPoint logicalPoint = flipForWritingMode(physicalPoint);
    
    // Now get the fragment that we are in.
    LayoutUnit logicalOffset = isHorizontalWritingMode() ? logicalPoint.y() : logicalPoint.x();
    RenderFragmentContainer* RenderFragmentContainer = fragmentAtBlockOffset(this, logicalOffset, true);
    if (!RenderFragmentContainer)
        return nullptr;
    
    // Translate to the coordinate space of the fragment.
    LayoutSize translationOffset = physicalTranslationOffsetFromFlowToFragment(RenderFragmentContainer, logicalOffset);
    
    // Now shift the physical point into the fragment's coordinate space.
    physicalPoint += translationOffset;
    
    return RenderFragmentContainer;
}

bool RenderMultiColumnFlow::isPageLogicalHeightKnown() const
{
    if (RenderMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->columnHeightComputed();
    return false;
}

bool RenderMultiColumnFlow::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    // You cannot be inside an in-flow RenderFragmentedFlow without a corresponding DOM node. It's better to
    // just let the ancestor figure out where we are instead.
    if (hitTestAction == HitTestBlockBackground)
        return false;
    bool inside = RenderFragmentedFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
    if (inside && !result.innerNode())
        return false;
    return inside;
}

bool RenderMultiColumnFlow::shouldCheckColumnBreaks() const
{
    if (!parent()->isRenderView())
        return true;
    return view().frameView().pagination().behavesLikeColumns;
}

}
