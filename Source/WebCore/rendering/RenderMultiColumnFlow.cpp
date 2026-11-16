/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LocalFrameViewInlines.h"
#include "Node.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderFragmentContainer.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderLayoutState.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "TransformState.h"
#include <wtf/IterationStatus.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderMultiColumnFlow);

RenderMultiColumnFlow::RenderMultiColumnFlow(Document& document, Style::ComputedStyle&& style)
    : RenderBlockFlow(Type::MultiColumnFlow, document, WTF::move(style))
{
    setFragmentedFlowState(FragmentedFlowState::InsideFlow);
    ASSERT(isRenderMultiColumnFlow());
}

RenderMultiColumnFlow::~RenderMultiColumnFlow() = default;

template<typename FragmentList, typename Functor>
static void forEachFragmentInRange(FragmentList& fragmentList, const RenderFragmentContainer& startFragment, const RenderFragmentContainer& endFragment, NOESCAPE const Functor& functor)
{
    ASSERT(fragmentList.contains(startFragment));
    ASSERT(fragmentList.contains(endFragment));

    for (auto it = fragmentList.find(startFragment), end = fragmentList.end(); it != end; ++it) {
        auto& fragment = *it;
        if constexpr (std::same_as<std::invoke_result_t<Functor, decltype(fragment)>, IterationStatus>) {
            if (functor(fragment) == IterationStatus::Done)
                return;
        } else
            functor(fragment);
        if (&fragment == &endFragment)
            return;
    }
}

class RenderMultiColumnFlow::FragmentSearchAdapter {
public:
    explicit FragmentSearchAdapter(LayoutUnit offset)
        : m_offset(offset)
    {
    }

    NODELETE const LayoutUnit& lowValue() const { return m_offset; }
    NODELETE const LayoutUnit& highValue() const { return m_offset; }

    NODELETE void collectIfNeeded(const PODInterval<LayoutUnit, SingleThreadWeakPtr<RenderFragmentContainer>>& interval)
    {
        if (m_result)
            return;
        if (interval.low() <= m_offset && interval.high() > m_offset)
            m_result = interval.data();
    }

    NODELETE RenderFragmentContainer* result() const { return m_result.get(); }

private:
    LayoutUnit m_offset;
    SingleThreadWeakPtr<RenderFragmentContainer> m_result;
};

ASCIILiteral RenderMultiColumnFlow::renderName() const
{
    return "RenderMultiColumnFlowThread"_s;
}

RenderMultiColumnSet* RenderMultiColumnFlow::firstMultiColumnSet() const
{
    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (auto* multiColumnSet = dynamicDowncast<RenderMultiColumnSet>(*sibling))
            return multiColumnSet;
    }
    return nullptr;
}

RenderMultiColumnSet* RenderMultiColumnFlow::lastMultiColumnSet() const
{
    ASSERT(multiColumnBlockFlow());

    for (RenderObject* sibling = multiColumnBlockFlow()->lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (auto* multiColumnSet = dynamicDowncast<RenderMultiColumnSet>(*sibling))
            return multiColumnSet;
    }
    return nullptr;
}

RenderBox* RenderMultiColumnFlow::firstColumnSetOrSpanner() const
{
    if (RenderObject* sibling = nextSibling()) {
        ASSERT(is<RenderBox>(*sibling));
        ASSERT(is<RenderMultiColumnSet>(*sibling) || findColumnSpannerPlaceholder(downcast<RenderBox>(*sibling)));
        return downcast<RenderBox>(sibling);
    }
    return nullptr;
}

RenderBox* RenderMultiColumnFlow::nextColumnSetOrSpannerSiblingOf(const RenderBox* child)
{
    return child ? child->nextSiblingBox() : nullptr;
}

RenderBox* RenderMultiColumnFlow::previousColumnSetOrSpannerSiblingOf(const RenderBox* child)
{
    if (!child)
        return nullptr;
    if (auto* sibling = child->previousSiblingBox()) {
        if (!is<RenderMultiColumnFlow>(*sibling))
            return sibling;
    }
    return nullptr;
}

RenderMultiColumnSpannerPlaceholder* RenderMultiColumnFlow::findColumnSpannerPlaceholder(const RenderBox& spanner) const
{
    return m_spannerMap.get(spanner);
}

void RenderMultiColumnFlow::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    ASSERT(!m_inLayout);
    m_inLayout = true;
    m_lastSetWorkedOn = nullptr;
    if (RenderBox* first = firstColumnSetOrSpanner()) {
        if (CheckedPtr multiColumnSet = dynamicDowncast<RenderMultiColumnSet>(*first)) {
            m_lastSetWorkedOn = multiColumnSet.get();
            multiColumnSet->beginFlow(this);
        }
    }

    m_pageLogicalSizeChanged = m_fragmentsInvalidated && everHadLayout();
    validateFragments();
    RenderBlockFlow::layout();
    m_pageLogicalSizeChanged = false;

    if (RenderMultiColumnSet* lastSet = lastMultiColumnSet()) {
        if (!nextColumnSetOrSpannerSiblingOf(lastSet))
            lastSet->endFlow(this, logicalHeight());
        lastSet->expandToEncompassFragmentedFlowContentsIfNeeded();
    }
    m_inLayout = false;
    m_lastSetWorkedOn = nullptr;
}

void RenderMultiColumnFlow::addFragmentToThread(RenderFragmentContainer* fragmentContainer)
{
    auto* columnSet = downcast<RenderMultiColumnSet>(fragmentContainer);
    if (RenderMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        auto it = m_fragmentList.find(*nextSet);
        ASSERT(it != m_fragmentList.end());
        m_fragmentList.insertBefore(it, *columnSet);
    } else
        m_fragmentList.add(*columnSet);
}

void RenderMultiColumnFlow::willBeRemovedFromTree()
{
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachFragment();
    RenderBlockFlow::willBeRemovedFromTree();
}

void RenderMultiColumnFlow::fragmentedFlowDescendantBoxLaidOut(RenderBox* descendant)
{
    CheckedPtr placeholder = dynamicDowncast<RenderMultiColumnSpannerPlaceholder>(*descendant);
    if (!placeholder)
        return;

    CheckedPtr container = placeholder->containingBlock();

    for (RenderBox* prev = previousColumnSetOrSpannerSiblingOf(placeholder->spanner()); prev; prev = previousColumnSetOrSpannerSiblingOf(prev)) {
        if (CheckedPtr multiColumnSet = dynamicDowncast<RenderMultiColumnSet>(*prev)) {
            multiColumnSet->endFlow(container.get(), placeholder->logicalTop());
            break;
        }
    }

    for (RenderBox* next = nextColumnSetOrSpannerSiblingOf(placeholder->spanner()); next; next = nextColumnSetOrSpannerSiblingOf(next)) {
        if (CheckedPtr multiColumnSet = dynamicDowncast<RenderMultiColumnSet>(*next)) {
            m_lastSetWorkedOn = multiColumnSet.get();
            multiColumnSet->beginFlow(container.get());
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
    // Only positive values are interesting (and allowed) here. Zero space shortage may be reported
    // when we're at the top of a column and the element has zero height. Ignore this, and also
    // ignore any negative values, which may occur when we set an early break in order to honor
    // widows in the next column.
    if (spaceShortage <= 0)
        return;

    if (auto* multicolSet = downcast<RenderMultiColumnSet>(fragmentAtBlockOffset(block, offset)))
        multicolSet->recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnFlow::updateMinimumPageHeight(const RenderBlock* block, LayoutUnit offset, LayoutUnit minHeight)
{
    if (!hasValidFragmentInfo())
        return;

    if (auto* multicolSet = downcast<RenderMultiColumnSet>(fragmentAtBlockOffset(block, offset)))
        multicolSet->updateMinimumColumnHeight(minHeight);
}

void RenderMultiColumnFlow::updateSpaceShortageForSizeContainment(const RenderBlock* block, LayoutUnit offset, LayoutUnit shortage)
{
    if (auto* multicolSet = downcast<RenderMultiColumnSet>(fragmentAtBlockOffset(block, offset)))
        multicolSet->updateSpaceShortageForSizeContainment(shortage);
}

RenderFragmentContainer* RenderMultiColumnFlow::fragmentAtBlockOffset(const RenderBox* clampBox, LayoutUnit offset, bool extendLastFragment) const
{
    if (m_inLayout) {
        // Layout in progress. We are calculating the set heights as we speak, so the fragment range
        // information is not up-to-date.

        if (m_lastSetWorkedOn && m_lastSetWorkedOn->fragmentedFlow() != this)
            m_lastSetWorkedOn = nullptr;

        RenderMultiColumnSet* columnSet = m_lastSetWorkedOn ? m_lastSetWorkedOn.get() : firstMultiColumnSet();
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

    ASSERT(!m_fragmentsInvalidated);

    if (m_fragmentList.computeIsEmpty())
        return nullptr;

    if (m_fragmentList.computeSize() == 1 && extendLastFragment)
        return const_cast<RenderFragmentContainer*>(&m_fragmentList.first());

    auto clamp = [clampBox](RenderFragmentContainer* fragment)  {
        return clampBox ? clampBox->clampToStartAndEndFragments(fragment) : fragment;
    };

    if (offset <= 0)
        return clamp(const_cast<RenderFragmentContainer*>(&m_fragmentList.first()));

    FragmentSearchAdapter adapter(offset);
    m_fragmentIntervalTree.allOverlapsWithAdapter(adapter);
    if (auto* fragment = adapter.result())
        return clamp(fragment);

    // If no fragment was found, the offset is in the flow thread overflow, which the last column set contains.
    return clamp(const_cast<RenderFragmentContainer*>(&m_fragmentList.last()));
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

    ASSERT(hasFragments());
    ASSERT(startFragment && endFragment && startFragment->fragmentedFlow() == this && endFragment->fragmentedFlow() == this);
    auto result = m_fragmentRangeMap.add(box, RenderFragmentContainerRange(startFragment, endFragment));
    if (result.isNewEntry)
        return;

    // If nothing changed, just bail.
    auto& range = result.iterator->value;
    if (range.startFragment() == startFragment && range.endFragment() == endFragment)
        return;
    clearRenderBoxFragmentInfo(box, startFragment, endFragment, range.startFragment(), range.endFragment());
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

LayoutSize RenderMultiColumnFlow::offsetFromContainer(const RenderElement& enclosingContainer, const LayoutPoint& physicalPoint, bool* offsetDependsOnPoint) const
{
    ASSERT(&enclosingContainer == container());

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = true;

    LayoutPoint translatedPhysicalPoint(physicalPoint);
    if (RenderFragmentContainer* fragment = physicalTranslationFromFlowToFragment(translatedPhysicalPoint))
        translatedPhysicalPoint.moveBy(fragment->topLeftLocation());

    LayoutSize offset(translatedPhysicalPoint.x(), translatedPhysicalPoint.y());
    if (auto* enclosingBox = dynamicDowncast<RenderBox>(enclosingContainer))
        offset -= toLayoutSize(enclosingBox->scrollPosition());
    return offset;
}

void RenderMultiColumnFlow::mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode> mode, TransformState& transformState) const
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
    pushOntoTransformState(transformState, mode, nullptr, parent(), translationOffset, false);
}

LayoutSize RenderMultiColumnFlow::physicalTranslationFromFragmentToFlow(const RenderMultiColumnSet* columnSet, const LayoutPoint& physicalPoint) const
{
    LayoutPoint logicalPoint = columnSet->physicalToBlockStartRelative(physicalPoint);
    LayoutPoint translatedPoint = columnSet->translateFragmentPointToFragmentedFlow(logicalPoint);
    LayoutPoint physicalTranslatedPoint = columnSet->blockStartRelativeToPhysical(translatedPoint);
    return physicalPoint - physicalTranslatedPoint;
}

RenderFragmentContainer* RenderMultiColumnFlow::mapFromFlowToFragment(TransformState& transformState) const
{
    if (!hasValidFragmentInfo())
        return nullptr;

    // Get back into our local flow thread space.
    LayoutRect boxRect = transformState.mappedQuad().enclosingBoundingBox();
    physicalToBlockStartRelative(boxRect);

    // FIXME: We need to refactor RenderObject::absoluteQuads to be able to split the quads across fragments,
    // for now we just take the center of the mapped enclosing box and map it to a column.
    LayoutPoint centerPoint = boxRect.center();
    LayoutUnit centerLogicalOffset = isHorizontalWritingMode() ? centerPoint.y() : centerPoint.x();
    auto* fragmentContainer = fragmentAtBlockOffset(this, centerLogicalOffset, true);
    if (!fragmentContainer)
        return nullptr;
    transformState.move(physicalTranslationOffsetFromFlowToFragment(fragmentContainer, centerLogicalOffset));
    return fragmentContainer;
}

LayoutSize RenderMultiColumnFlow::physicalTranslationOffsetFromFlowToFragment(const RenderFragmentContainer* fragmentContainer, const LayoutUnit logicalOffset) const
{
    // Now that we know which multicolumn set we hit, we need to get the appropriate translation offset for the column.
    const auto* columnSet = downcast<RenderMultiColumnSet>(fragmentContainer);
    LayoutPoint translationOffset = columnSet->columnTranslationForOffset(logicalOffset);

    // Now we know how we want the rect to be translated into the fragment. At this point we're converting
    // back to physical coordinates.
    if (writingMode().isBlockFlipped()) {
        LayoutRect portionRect(columnSet->fragmentedFlowPortionRect());
        LayoutRect columnRect = columnSet->columnRectAt(0);
        LayoutUnit physicalDeltaFromPortionBottom = logicalHeight() - columnSet->logicalBottomInFragmentedFlow();
        if (isHorizontalWritingMode())
            columnRect.setHeight(portionRect.height());
        else
            columnRect.setWidth(portionRect.width());
        columnSet->blockStartRelativeToPhysical(columnRect);
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
    LayoutPoint logicalPoint = physicalToBlockStartRelative(physicalPoint);

    // Now get the fragment that we are in.
    LayoutUnit logicalOffset = isHorizontalWritingMode() ? logicalPoint.y() : logicalPoint.x();
    RenderFragmentContainer* fragmentContainer = fragmentAtBlockOffset(this, logicalOffset, true);
    if (!fragmentContainer)
        return nullptr;

    // Translate to the coordinate space of the fragment.
    LayoutSize translationOffset = physicalTranslationOffsetFromFlowToFragment(fragmentContainer, logicalOffset);

    // Now shift the physical point into the fragment's coordinate space.
    physicalPoint += translationOffset;

    return fragmentContainer;
}

bool RenderMultiColumnFlow::isPageLogicalHeightKnown() const
{
    if (RenderMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->columnHeightComputed();
    return false;
}

bool RenderMultiColumnFlow::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    // You cannot be inside an in-flow RenderMultiColumnFlow without a corresponding DOM node. It's better to
    // just let the ancestor figure out where we are instead.
    if (hitTestAction == HitTestAction::BlockBackground)
        return false;
    bool inside = RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
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

void RenderMultiColumnFlow::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (oldStyle && oldStyle->writingMode().computedWritingMode() != writingMode().computedWritingMode())
        invalidateFragments();
}

void RenderMultiColumnFlow::removeFlowChildInfo(RenderElement& child)
{
    if (CheckedPtr box = dynamicDowncast<RenderBox>(child))
        removeRenderBoxFragmentInfo(*box);
}

void RenderMultiColumnFlow::removeFragmentFromThread(RenderFragmentContainer& renderFragmentContainer)
{
    m_fragmentList.remove(renderFragmentContainer);
}

void RenderMultiColumnFlow::invalidateFragments(MarkingBehavior markingParents)
{
    if (m_fragmentsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    m_fragmentRangeMap.clear();
    setNeedsLayout(markingParents);

    m_fragmentsInvalidated = true;
}

void RenderMultiColumnFlow::validateFragments()
{
    if (m_fragmentsInvalidated) {
        m_fragmentsInvalidated = false;
        m_fragmentsHaveUniformLogicalHeight = true;

        if (hasFragments()) {
            LayoutUnit previousFragmentLogicalHeight;
            bool firstFragmentVisited = false;

            for (auto& fragment : m_fragmentList) {
                fragment.deleteAllRenderBoxFragmentInfo();

                LayoutUnit fragmentLogicalHeight = fragment.pageLogicalHeight();

                if (!firstFragmentVisited)
                    firstFragmentVisited = true;
                else if (m_fragmentsHaveUniformLogicalHeight && previousFragmentLogicalHeight != fragmentLogicalHeight)
                    m_fragmentsHaveUniformLogicalHeight = false;

                previousFragmentLogicalHeight = fragmentLogicalHeight;
            }

            setFragmentRangeForBox(*this, &m_fragmentList.first(), &m_fragmentList.last());
        }
    }

    updateLogicalWidth(); // Called to get the maximum logical width for the fragment.
    updateFragmentsFragmentedFlowPortionRect();
}

void RenderMultiColumnFlow::updateLogicalWidth()
{
    LayoutUnit logicalWidth = initialLogicalWidth();
    for (auto& fragment : m_fragmentList)
        logicalWidth = std::max(fragment.pageLogicalWidth(), logicalWidth);
    setLogicalWidth(logicalWidth);

    // If the fragments have non-uniform logical widths, then insert inset information for the RenderMultiColumnFlow.
    for (auto& fragment : m_fragmentList) {
        LayoutUnit fragmentLogicalWidth = fragment.pageLogicalWidth();
        LayoutUnit logicalLeft = writingMode().isLogicalLeftInlineStart() ? 0_lu
            : logicalWidth - fragmentLogicalWidth;
        fragment.setRenderBoxFragmentInfo(*this, logicalLeft, fragmentLogicalWidth);
    }
}

bool RenderMultiColumnFlow::shouldRepaint(const LayoutRect& r) const
{
    if (view().printing() || r.isEmpty())
        return false;

    return true;
}

void RenderMultiColumnFlow::repaintRectangleInFragments(const LayoutRect& repaintRect) const
{
    if (!shouldRepaint(repaintRect) || !hasValidFragmentInfo())
        return;

    LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext()); // We can't use layout state to repaint, since the fragments are somewhere else.

    for (auto& fragment : m_fragmentList)
        fragment.repaintFragmentedFlowContent(repaintRect);
}

bool RenderMultiColumnFlow::absoluteQuadsForBox(Vector<FloatQuad>& quads, bool* wasFixed, const RenderBox& box) const
{
    if (!hasValidFragmentInfo())
        return false;

    auto boxRect = FloatRect { { }, box.borderBoxSize() };
    auto boxRectInFlowCoordinates = LayoutRect { box.localToContainerQuad(boxRect, this).boundingBox() };

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!computedFragmentRangeForBox(box, startFragment, endFragment))
        return false;

    forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
        auto rectsInFragment = fragment.fragmentRectsForFlowContentRect(boxRectInFlowCoordinates);
        for (auto rect : rectsInFragment) {
            auto absoluteQuad = fragment.localToAbsoluteQuad(FloatRect(rect), MapCoordinatesMode::UseTransforms, wasFixed);
            quads.append(absoluteQuad);
        }
    });

    return true;
}

bool RenderMultiColumnFlow::boxIsFragmented(const RenderBox& box) const
{
    ASSERT(hasValidFragmentInfo());

    auto boxRect = FloatRect { { }, box.borderBoxSize() };
    auto boxRectInFlowCoordinates = LayoutRect { box.localToContainerQuad(boxRect, this).boundingBox() };

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    computedFragmentRangeForBox(box, startFragment, endFragment);
    if (startFragment != endFragment)
        return true;

    if (startFragment->contentRectSpansFragments(boxRectInFlowCoordinates))
        return true;

    return false;
}

LayoutUnit RenderMultiColumnFlow::pageLogicalTopForOffset(LayoutUnit offset) const
{
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(0, offset, false);
    return fragment ? fragment->pageLogicalTopForOffset(offset) : 0_lu;
}

LayoutUnit RenderMultiColumnFlow::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(0, offset, false);
    if (!fragment)
        return 0;

    return fragment->pageLogicalHeight();
}

LayoutUnit RenderMultiColumnFlow::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    RenderFragmentContainer* fragment = fragmentAtBlockOffset(0, offset, false);
    if (!fragment)
        return 0;

    LayoutUnit pageLogicalTop = fragment->pageLogicalTopForOffset(offset);
    LayoutUnit pageLogicalHeight = fragment->pageLogicalHeight();
    LayoutUnit pageLogicalBottom = pageLogicalTop + pageLogicalHeight;
    LayoutUnit remainingHeight = pageLogicalBottom - offset;
    if (pageBoundaryRule == IncludePageBoundary) {
        // If IncludePageBoundary is set, the line exactly on the top edge of a
        // fragment will act as being part of the previous fragment.
        remainingHeight = intMod(remainingHeight, pageLogicalHeight);
    } else if (!remainingHeight) {
        // When pageBoundaryRule is IncludePageBoundary, we shouldn't just return 0 if there's no
        // space left, because in that case we're at a column boundary, in which case we should
        // return the amount of space remaining in the *next* column. Note that the page height
        // itself may be 0, though.
        remainingHeight = pageLogicalHeight;
    }
    return remainingHeight;
}

void RenderMultiColumnFlow::removeRenderBoxFragmentInfo(const RenderBox& box)
{
    if (!hasFragments())
        return;

    // If the fragment chain was invalidated the next layout will clear the box information from all the fragments.
    if (m_fragmentsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (getFragmentRangeForBox(box, startFragment, endFragment)) {
        forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
            fragment.removeRenderBoxFragmentInfo(box);
        });
    }

#ifndef NDEBUG
    // We have to make sure we did not leave any RenderBoxFragmentInfo attached.
    for (auto& fragment : m_fragmentList)
        ASSERT_UNUSED(fragment, !fragment.renderBoxFragmentInfo(box));
#endif

    m_fragmentRangeMap.remove(box);
}

void RenderMultiColumnFlow::logicalWidthChangedInFragmentsForBlock(const RenderBlock& block, RelayoutChildren& relayoutChildren)
{
    if (!hasValidFragmentInfo())
        return;

    auto it = m_fragmentRangeMap.find(block);
    if (it == m_fragmentRangeMap.end())
        return;

    RenderFragmentContainerRange& range = it->value;
    bool rangeInvalidated = range.rangeInvalidated();
    range.clearRangeInvalidated();

    // If there will be a relayout anyway skip the next steps because they only verify
    // the state of the ranges.
    if (relayoutChildren == RelayoutChildren::Yes)
        return;

    // Not necessary for the flow thread, since we already computed the correct info for it.
    // If the fragments have changed invalidate the children.
    if (&block == this) {
        relayoutChildren = m_pageLogicalSizeChanged ? RelayoutChildren::Yes : RelayoutChildren::No;
        return;
    }

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(block, startFragment, endFragment))
        return;

    forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
        // We have no information computed for this fragment so we need to do it.
        std::unique_ptr<RenderBoxFragmentInfo> oldInfo = fragment.takeRenderBoxFragmentInfo(block);
        if (!oldInfo) {
            relayoutChildren = rangeInvalidated ? RelayoutChildren::Yes : RelayoutChildren::No;
            return IterationStatus::Done;
        }

        LayoutUnit oldLogicalWidth = oldInfo->logicalWidth();
        auto* newInfo = block.renderBoxFragmentInfo(&fragment);
        if (!newInfo || newInfo->logicalWidth() != oldLogicalWidth) {
            relayoutChildren = RelayoutChildren::Yes;
            return IterationStatus::Done;
        }

        return IterationStatus::Continue;
    });
}

LayoutUnit RenderMultiColumnFlow::contentLogicalWidthOfFirstFragment() const
{
    RenderFragmentContainer* firstValidFragmentInFlow = firstFragment();
    if (!firstValidFragmentInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidFragmentInFlow->contentBoxWidth() : firstValidFragmentInFlow->contentBoxHeight();
}

LayoutUnit RenderMultiColumnFlow::contentLogicalHeightOfFirstFragment() const
{
    RenderFragmentContainer* firstValidFragmentInFlow = firstFragment();
    if (!firstValidFragmentInFlow)
        return 0;
    return isHorizontalWritingMode() ? firstValidFragmentInFlow->contentBoxHeight() : firstValidFragmentInFlow->contentBoxWidth();
}

RenderFragmentContainer* RenderMultiColumnFlow::firstFragment() const
{
    if (!hasFragments())
        return nullptr;
    return const_cast<RenderFragmentContainer*>(&m_fragmentList.first());
}

RenderFragmentContainer* RenderMultiColumnFlow::lastFragment() const
{
    if (!hasFragments())
        return nullptr;
    return const_cast<RenderFragmentContainer*>(&m_fragmentList.last());
}

void RenderMultiColumnFlow::clearRenderBoxFragmentInfo(const RenderBox& box,
    const RenderFragmentContainer* newStartFragment, const RenderFragmentContainer* newEndFragment,
    const RenderFragmentContainer* oldStartFragment, const RenderFragmentContainer* oldEndFragment)
{
    ASSERT(newStartFragment && newEndFragment && oldStartFragment && oldEndFragment);

    bool insideOldFragmentRange = false;
    bool insideNewFragmentRange = false;
    for (auto& fragment : m_fragmentList) {
        if (oldStartFragment == &fragment)
            insideOldFragmentRange = true;
        if (newStartFragment == &fragment)
            insideNewFragmentRange = true;

        if (!(insideOldFragmentRange && insideNewFragmentRange)) {
            if (fragment.renderBoxFragmentInfo(box))
                fragment.removeRenderBoxFragmentInfo(box);
        }

        if (oldEndFragment == &fragment)
            insideOldFragmentRange = false;
        if (newEndFragment == &fragment)
            insideNewFragmentRange = false;
    }
}

bool RenderMultiColumnFlow::hasCachedFragmentRangeForBox(const RenderBox& box) const
{
    return m_fragmentRangeMap.contains(box);
}

bool RenderMultiColumnFlow::getFragmentRangeForBoxFromCachedInfo(const RenderBox& box, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const
{
    ASSERT(hasValidFragmentInfo());
    ASSERT((startFragment == nullptr) && (endFragment == nullptr));

    auto it = m_fragmentRangeMap.find(box);
    if (it != m_fragmentRangeMap.end()) {
        const RenderFragmentContainerRange& range = it->value;
        startFragment = range.startFragment();
        endFragment = range.endFragment();
        ASSERT(m_fragmentList.contains(*startFragment) && m_fragmentList.contains(*endFragment));
        return true;
    }

    return false;
}

bool RenderMultiColumnFlow::getFragmentRangeForBox(const RenderBox& box, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const
{
    startFragment = endFragment = nullptr;
    if (!hasValidFragmentInfo()) // We clear the ranges when we invalidate the fragments.
        return false;

    if (m_fragmentList.computeSize() == 1) {
        startFragment = endFragment = const_cast<RenderFragmentContainer*>(&m_fragmentList.first());
        return true;
    }

    if (getFragmentRangeForBoxFromCachedInfo(box, startFragment, endFragment))
        return true;

    return false;
}

bool RenderMultiColumnFlow::computedFragmentRangeForBox(const RenderBox& box, RenderFragmentContainer*& startFragment, RenderFragmentContainer*& endFragment) const
{
    startFragment = endFragment = nullptr;
    if (!hasValidFragmentInfo()) // We clear the ranges when we invalidate the fragments.
        return false;

    if (getFragmentRangeForBox(box, startFragment, endFragment))
        return true;

    // Search the fragment range using the information provided by the containing block chain.
    auto* containingBlock = const_cast<RenderBox*>(&box);
    while (!containingBlock->isRenderMultiColumnFlow()) {
        // FIXME: Use the containingBlock() value once we patch all the layout systems to be fragment range aware
        // (e.g. if we use containingBlock() the shadow controls of a video element won't get the range from the
        // video box because it's not a block; they need to be patched separately).
        ASSERT(containingBlock->parent());
        containingBlock = &containingBlock->parent()->enclosingBox();
        ASSERT(containingBlock);

        // If a box doesn't have a cached fragment range it usually means the box belongs to a line so startFragment should be equal with endFragment.
        // FIXME: Find the cases when this startFragment should not be equal with endFragment and make sure these boxes have cached fragment ranges.
        if (containingBlock && hasCachedFragmentRangeForBox(*containingBlock)) {
            startFragment = endFragment = fragmentAtBlockOffset(containingBlock, containingBlock->offsetFromLogicalTopOfFirstPage(), true);
            return true;
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool RenderMultiColumnFlow::fragmentInRange(const RenderFragmentContainer* targetFragment, const RenderFragmentContainer* startFragment, const RenderFragmentContainer* endFragment) const
{
    ASSERT(targetFragment);

    for (auto it = m_fragmentList.find(*startFragment), end = m_fragmentList.end(); it != end; ++it) {
        const RenderFragmentContainer& currFragment = *it;
        if (targetFragment == &currFragment)
            return true;
        if (&currFragment == endFragment)
            break;
    }

    return false;
}

bool RenderMultiColumnFlow::objectShouldFragmentInFlowFragment(const RenderObject* object, const RenderFragmentContainer* fragment) const
{
    ASSERT(object);
    ASSERT(fragment || isSkippedContent());

    RenderMultiColumnFlow* fragmentedFlow = object->enclosingFragmentedFlow();
    if (fragmentedFlow != this)
        return false;

    if (!fragment || !m_fragmentList.contains(*fragment))
        return false;

    RenderFragmentContainer* enclosingBoxStartFragment = nullptr;
    RenderFragmentContainer* enclosingBoxEndFragment = nullptr;
    // If the box has no range, do not check fragmentInRange. Boxes inside inlines do not get ranges.
    // Instead, the containing RootInlineBox will abort when trying to paint inside the wrong fragment.
    if (computedFragmentRangeForBox(object->enclosingBox(), enclosingBoxStartFragment, enclosingBoxEndFragment)
        && !fragmentInRange(fragment, enclosingBoxStartFragment, enclosingBoxEndFragment))
        return false;

    return object->isRenderBox() || object->isInlineBox();
}

void RenderMultiColumnFlow::updateFragmentsFragmentedFlowPortionRect()
{
    LayoutUnit logicalHeight;
    // FIXME: Optimize not to clear the interval tree all the time. This would involve manually managing the tree nodes' lifecycle.
    m_fragmentIntervalTree.clear();
    for (auto& fragment : m_fragmentList) {
        LayoutUnit fragmentLogicalWidth = fragment.pageLogicalWidth();
        LayoutUnit fragmentLogicalHeight = std::min<LayoutUnit>(RenderMultiColumnFlow::maxLogicalHeight() - logicalHeight, fragment.logicalHeightOfAllFragmentedFlowContent());

        LayoutRect fragmentRect(writingMode().isLogicalLeftInlineStart() ? 0_lu
            : logicalWidth() - fragmentLogicalWidth, logicalHeight, fragmentLogicalWidth, fragmentLogicalHeight);

        fragment.setFragmentedFlowPortionRect(isHorizontalWritingMode() ? fragmentRect : fragmentRect.transposedRect());

        m_fragmentIntervalTree.add({ logicalHeight, logicalHeight + fragmentLogicalHeight, &fragment });

        logicalHeight += fragmentLogicalHeight;
    }
}

void RenderMultiColumnFlow::collectLayerFragments(LayerFragments& layerFragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect) const
{
    ASSERT(!m_fragmentsInvalidated || isSkippedContent());

    for (auto& fragment : m_fragmentList)
        fragment.collectLayerFragments(layerFragments, layerBoundingBox, dirtyRect);
}

LayoutRect RenderMultiColumnFlow::fragmentsBoundingBox(const LayoutRect& layerBoundingBox) const
{
    ASSERT(!m_fragmentsInvalidated);

    LayoutRect result;
    for (auto& fragment : m_fragmentList) {
        LayerFragments fragments;
        fragment.collectLayerFragments(fragments, layerBoundingBox, LayoutRect::infiniteRect());
        for (const auto& fragment : fragments) {
            LayoutRect fragmentRect(layerBoundingBox);
            fragmentRect.intersect(fragment.paginationClip);
            fragmentRect.move(fragment.paginationOffset);
            result.unite(fragmentRect);
        }
    }

    return result;
}

LayoutUnit RenderMultiColumnFlow::offsetFromLogicalTopOfFirstFragment(const RenderBlock* currentBlock) const
{
    // As a last resort, take the slow path.
    // Use container() rather than containingBlock() since containingBlock()
    // skips non-RenderBlock ancestors like RenderTableSection.
    CheckedPtr<const RenderBox> currentBox = currentBlock;
    LayoutRect blockRect(0_lu, 0_lu, currentBox->borderBoxWidth(), currentBox->borderBoxHeight());
    auto nextBoxContainer = [](const RenderElement& renderer) -> const RenderElement* {
        auto* container = renderer.container();
        // Skip non-box ancestors like inline boxes that don't contribute offsets.
        while (container && !is<RenderBox>(*container))
            container = container->container();
        return container;
    };
    while (currentBox && !is<RenderView>(*currentBox) && !currentBox->isRenderMultiColumnFlow()) {
        CheckedPtr containerBox = dynamicDowncast<RenderBox>(nextBoxContainer(*currentBox));
        if (!containerBox)
            return 0;
        auto currentBoxLocation = currentBox->location();
        if (CheckedPtr inlineBox = dynamicDowncast<RenderBoxModelObject>(currentBox->container()); inlineBox && inlineBox->isInlineBox() && currentBox->isOutOfFlowPositioned())
            currentBoxLocation += toLayoutSize(inlineBox->blockStartRelativeFirstFragmentBorderBoxRect().location());

        if (containerBox->writingMode().blockDirection() != currentBox->writingMode().blockDirection()) {
            // We have to put the block rect in container coordinates
            // and we have to take into account both the container and current block flipping modes
            if (containerBox->writingMode().isBlockFlipped()) {
                if (containerBox->isHorizontalWritingMode())
                    blockRect.setY(currentBox->borderBoxHeight() - blockRect.maxY());
                else
                    blockRect.setX(currentBox->borderBoxWidth() - blockRect.maxX());
            }
            currentBox->blockStartRelativeToPhysical(blockRect);
        }
        blockRect.moveBy(currentBoxLocation);
        currentBox = containerBox;
    }

    return currentBox->isHorizontalWritingMode() ? blockRect.y() : blockRect.x();
}

void RenderMultiColumnFlow::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (this == ancestorContainer)
        return;

    if (auto* fragment = mapFromFlowToFragment(transformState)) {
        // FIXME: The cast below is probably not the best solution, we may need to find a better way.
        const RenderObject* fragmentObject = static_cast<const RenderObject*>(fragment);

        // If the repaint container is nullptr, we have to climb up to the RenderView, otherwise swap
        // it with the fragment's repaint container.
        ancestorContainer = ancestorContainer ? fragment->containerForRepaint().renderer.unsafeGet() : nullptr;

        fragmentObject->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
    }
}

// FIXME: Make this function faster. Walking the render tree is slow, better use a caching mechanism (e.g. |cachedOffsetFromLogicalTopOfFirstFragment|).
LayoutRect RenderMultiColumnFlow::mapFromLocalToFragmentedFlow(const RenderBox* box, const LayoutRect& localRect) const
{
    LayoutRect boxRect = localRect;

    while (box && box != this) {
        RenderBlock* containerBlock = box->containingBlock();
        ASSERT(containerBlock);
        if (!containerBlock)
            return LayoutRect();
        LayoutPoint currentBoxLocation = box->location();
        if (CheckedPtr inlineBox = dynamicDowncast<RenderBoxModelObject>(box->container()); inlineBox && inlineBox->isInlineBox() && box->isOutOfFlowPositioned()) {
            // The box is positioned relative to the inline box, which is positioned in its own containing block.
            currentBoxLocation += toLayoutSize(inlineBox->blockStartRelativeFirstFragmentBorderBoxRect().location());
            containerBlock = inlineBox->containingBlock();
        }

        if (containerBlock->writingMode().blockDirection() != box->writingMode().blockDirection())
            box->blockStartRelativeToPhysical(boxRect);

        boxRect.moveBy(currentBoxLocation);
        box = containerBlock;
    }

    return boxRect;
}

// FIXME: Make this function faster. Walking the render tree is slow, better use a caching mechanism (e.g. |cachedOffsetFromLogicalTopOfFirstFragment|).
LayoutRect RenderMultiColumnFlow::mapFromFragmentedFlowToLocal(const RenderBox* box, const LayoutRect& rect) const
{
    LayoutRect localRect = rect;
    if (box == this)
        return localRect;

    RenderBlock* containerBlock = box->containingBlock();
    ASSERT(containerBlock);
    if (!containerBlock)
        return LayoutRect();
    LayoutPoint currentBoxLocation = box->location();
    if (CheckedPtr inlineBox = dynamicDowncast<RenderBoxModelObject>(box->container()); inlineBox && inlineBox->isInlineBox() && box->isOutOfFlowPositioned()) {
        // The box is positioned relative to the inline box, which is positioned in its own containing block.
        currentBoxLocation += toLayoutSize(inlineBox->blockStartRelativeFirstFragmentBorderBoxRect().location());
        containerBlock = inlineBox->containingBlock();
    }
    localRect = mapFromFragmentedFlowToLocal(containerBlock, localRect);

    localRect.moveBy(-currentBoxLocation);

    if (containerBlock->writingMode().blockDirection() != box->writingMode().blockDirection())
        box->physicalToBlockStartRelative(localRect);

    return localRect;
}

void RenderMultiColumnFlow::physicalToBlockStartRelativeIgnoringSize(LayoutRect& rect) const
{
    if (!writingMode().isBlockFlipped())
        return;

    if (isHorizontalWritingMode())
        rect.setY(0 - rect.maxY());
    else
        rect.setX(0 - rect.maxX());
}

void RenderMultiColumnFlow::addFragmentsVisualEffectOverflow(const RenderBox& box)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(box, startFragment, endFragment))
        return;

    forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
        LayoutRect borderBox = box.borderBoxRect();
        borderBox = box.applyVisualEffectOverflow(borderBox);
        borderBox = fragment.rectFlowPortionForBox(box, borderBox);

        fragment.addVisualOverflowForBox(box, borderBox);
    });
}

void RenderMultiColumnFlow::addFragmentsVisualOverflowFromTheme(const RenderBlock& block)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(block, startFragment, endFragment))
        return;

    forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
        LayoutRect borderBox = block.borderBoxRect();
        borderBox = fragment.rectFlowPortionForBox(block, borderBox);

        FloatRect inflatedRect = borderBox;
        block.theme().adjustRepaintRect(block, inflatedRect);

        fragment.addVisualOverflowForBox(block, snappedIntRect(LayoutRect(inflatedRect)));
    });
}

void RenderMultiColumnFlow::addFragmentsOverflowFromChild(const RenderBox& box, const RenderBox& child, const LayoutSize& delta)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(child, startFragment, endFragment))
        return;

    RenderFragmentContainer* containerStartFragment = nullptr;
    RenderFragmentContainer* containerEndFragment = nullptr;
    if (!getFragmentRangeForBox(box, containerStartFragment, containerEndFragment))
        return;

    forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
        if (!fragmentInRange(&fragment, containerStartFragment, containerEndFragment))
            return;

        LayoutRect childLayoutOverflowRect = fragment.layoutOverflowRectForBoxForPropagation(child);
        childLayoutOverflowRect.move(delta);

        fragment.addLayoutOverflowForBox(box, childLayoutOverflowRect);

        if (child.hasSelfPaintingLayer() || box.hasNonVisibleOverflow())
            return;
        LayoutRect childVisualOverflowRect = fragment.visualOverflowRectForBoxForPropagation(child);
        childVisualOverflowRect.move(delta);
        fragment.addVisualOverflowForBox(box, childVisualOverflowRect);
    });
}

void RenderMultiColumnFlow::addFragmentsVisualOverflow(const RenderBox& box, const LayoutRect& visualOverflow)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(box, startFragment, endFragment))
        return;

    forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
        LayoutRect visualOverflowInFragment = fragment.rectFlowPortionForBox(box, visualOverflow);

        fragment.addVisualOverflowForBox(box, visualOverflowInFragment);
    });
}

void RenderMultiColumnFlow::clearFragmentsOverflow(const RenderBox& box)
{
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!getFragmentRangeForBox(box, startFragment, endFragment))
        return;

    forEachFragmentInRange(m_fragmentList, *startFragment, *endFragment, [&](auto& fragment) {
        RenderBoxFragmentInfo* boxInfo = fragment.renderBoxFragmentInfo(box);
        if (boxInfo && boxInfo->overflow())
            boxInfo->clearOverflow();
    });
}

} // namespace WebCore
