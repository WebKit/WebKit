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
#include "RenderIterator.h"
#include "RenderLayoutState.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMultiColumnFlow);

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
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : 0_lu;
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
            translationOffset.move(0_lu, columnRect.y() - portionRect.y() - physicalDeltaFromPortionBottom);
        else
            translationOffset.move(columnRect.x() - portionRect.x() - physicalDeltaFromPortionBottom, 0_lu);
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
