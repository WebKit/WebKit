/*
 * Copyright (C) 2014-2026 Apple Inc. All rights reserved.
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
#include "ScrollSnapAnimatorState.h"

#include "Logging.h"
#include "ScrollExtents.h"
#include "ScrollingEffectsController.h"
#include <ranges>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollSnapAnimatorState);

ScrollSnapAnimatorState::~ScrollSnapAnimatorState() = default;

bool ScrollSnapAnimatorState::transitionToSnapAnimationState(const ScrollExtents& scrollExtents, float pageScale, const FloatPoint& initialOffset)
{
    return setupAnimationForState(ScrollSnapState::Snapping, scrollExtents, pageScale, initialOffset, { }, { });
}

bool ScrollSnapAnimatorState::transitionToGlideAnimationState(const ScrollExtents& scrollExtents, float pageScale, const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta)
{
    return setupAnimationForState(ScrollSnapState::Gliding, scrollExtents, pageScale, initialOffset, initialVelocity, initialDelta);
}

bool ScrollSnapAnimatorState::setupAnimationForState(ScrollSnapState state, const ScrollExtents& scrollExtents, float pageScale, const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta)
{
    ASSERT(state == ScrollSnapState::Snapping || state == ScrollSnapState::Gliding);
    if (m_currentState == state)
        return false;

    bool animating = m_scrollController.startMomentumScrollWithInitialVelocity(initialOffset, initialVelocity, initialDelta, [&](const FloatPoint& targetOffset) {
        float targetOffsetX, targetOffsetY;
        std::tie(targetOffsetX, m_activeSnapIndexX) = targetOffsetForStartOffset(ScrollEventAxis::Horizontal, scrollExtents, initialOffset.x(), targetOffset, pageScale, initialDelta.width());
        std::tie(targetOffsetY, m_activeSnapIndexY) = targetOffsetForStartOffset(ScrollEventAxis::Vertical, scrollExtents, initialOffset.y(), targetOffset, pageScale, initialDelta.height());
        LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollSnapAnimatorState::setupAnimationForState() - target offset " << targetOffset << " modified to " << FloatPoint(targetOffsetX, targetOffsetY));
        return FloatPoint { targetOffsetX, targetOffsetY };
    });

    if (!animating)
        return false;

    m_currentState = state;
    return animating;
}

std::optional<unsigned> ScrollSnapAnimatorState::closestSnapPointForOffset(ScrollEventAxis axis, ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale) const
{
    LayoutPoint layoutScrollOffset(scrollOffset.x() / pageScale, scrollOffset.y() / pageScale);

    auto snapOffsets = snapOffsetsForAxis(axis);
    LayoutSize viewportSize(scrollExtents.viewportSize);
    std::optional<unsigned> activeIndex;
    if (snapOffsets.size())
        activeIndex = snapOffsetInfo().closestSnapOffset(axis, viewportSize, layoutScrollOffset, 0).second;

    return activeIndex;
}

float ScrollSnapAnimatorState::adjustedScrollDestination(ScrollEventAxis axis, FloatPoint destinationOffset, float velocity, std::optional<float> originalOffset, const ScrollExtents& scrollExtents, float pageScale) const
{
    auto snapOffsets = snapOffsetsForAxis(axis);
    if (!snapOffsets.size())
        return valueForAxis(destinationOffset, axis);

    std::optional<LayoutUnit> originalOffsetInLayoutUnits;
    if (originalOffset)
        originalOffsetInLayoutUnits = LayoutUnit(*originalOffset / pageScale);
    LayoutSize viewportSize(scrollExtents.viewportSize);
    LayoutPoint layoutDestinationOffset(destinationOffset.x() / pageScale, destinationOffset.y() / pageScale);
    LayoutUnit offset = snapOffsetInfo().closestSnapOffset(axis, viewportSize, layoutDestinationOffset, velocity, originalOffsetInLayoutUnits).first;
    return offset * pageScale;
}

// Returns whether the snap point is changed or not
bool ScrollSnapAnimatorState::preserveCurrentTargetForAxis(ScrollEventAxis axis, NodeIdentifier boxID)
{
    auto snapOffsets = snapOffsetsForAxis(axis);

    // A single snap offset can be shared by several snap areas, but only one of them is recorded
    // as the offset's representative snapTargetID. Our box may be one of the other areas at that
    // offset, so check every area contributing to the offset, not just snapTargetID.
    auto offsetContainsBox = [&](const SnapOffset<LayoutUnit>& offset) {
        if (offset.snapTargetID && *offset.snapTargetID == boxID)
            return true;
        for (auto areaIndex : offset.snapAreaIndices) {
            if (areaIndex < m_snapOffsetsInfo.snapAreasIDs.size() && m_snapOffsetsInfo.snapAreasIDs[areaIndex] == boxID)
                return true;
        }
        return false;
    };

    auto found = std::ranges::find_if(snapOffsets, offsetContainsBox);
    if (found == snapOffsets.end()) {
        setActiveSnapIndexForAxis(axis, std::nullopt);
        return false;
    }

    setActiveSnapIndexForAxis(axis, std::distance(snapOffsets.begin(), found));
    return true;
}

Vector<SnapOffset<LayoutUnit>> ScrollSnapAnimatorState::currentlySnappedOffsetsForAxis(ScrollEventAxis axis) const
{
    Vector<SnapOffset<LayoutUnit>> currentlySnappedOffsets;
    auto snapOffsets = snapOffsetsForAxis(axis);
    auto activeIndex = activeSnapIndexForAxis(axis);
    
    if (activeIndex && *activeIndex < snapOffsets.size())
        currentlySnappedOffsets.append(snapOffsets[*activeIndex]);
    return currentlySnappedOffsets;
}

HashSet<NodeIdentifier> ScrollSnapAnimatorState::currentlySnappedBoxes(const Vector<SnapOffset<LayoutUnit>>& horizontalOffsets, const Vector<SnapOffset<LayoutUnit>>& verticalOffsets) const
{
    HashSet<NodeIdentifier> snappedBoxIDs;
        
    for (auto offset : horizontalOffsets) {
        if (!offset.snapTargetID)
            continue;
        snappedBoxIDs.add(*offset.snapTargetID);
        for (auto i : offset.snapAreaIndices)
            snappedBoxIDs.add(m_snapOffsetsInfo.snapAreasIDs[i]);
    }
    
    for (auto offset : verticalOffsets) {
        if (!offset.snapTargetID)
            continue;
        snappedBoxIDs.add(*offset.snapTargetID);
        for (auto i : offset.snapAreaIndices)
            snappedBoxIDs.add(m_snapOffsetsInfo.snapAreasIDs[i]);
    }
    return snappedBoxIDs;
}

void ScrollSnapAnimatorState::setActiveSnapIndexForAxis(ScrollEventAxis axis, std::optional<unsigned> index)
{
    setActiveSnapIndexForAxisInternal(axis, index);
    updateCurrentlySnappedBoxes();
}

// Selects this axis's snap target among the boxes aligned at the active offset, per
// https://drafts.csswg.org/css-scroll-snap/#multiple-aligned-snap-areas: focused, then targeted,
// then innermost (ancestors removed), then the area aligned in both axes (the block/inline set
// intersection), then first in tree order. snapTargetID is the focused/targeted representative; this
// adds the innermost and common-to-both-axes steps.
std::optional<NodeIdentifier> ScrollSnapAnimatorState::selectSnapTargetForAxis(ScrollEventAxis axis) const
{
    auto offsets = currentlySnappedOffsetsForAxis(axis);
    if (offsets.isEmpty())
        return std::nullopt;
    const auto& offset = offsets[0];

    // A focused or fragment-targeted box wins outright.
    if ((offset.isFocused || offset.isTarget) && offset.snapTargetID)
        return *offset.snapTargetID;

    const auto& areaIDs = m_snapOffsetsInfo.snapAreasIDs;

    // Candidate boxes after removing any area that encloses another aligned area (its ancestors), so
    // a nested area supersedes a co-located ancestor, including an ancestor aligned in both axes.
    auto candidateIndices = innermostAlignedAreaIndicesForAxis(axis);
    if (candidateIndices.isEmpty())
        return offset.snapTargetID;

    // If the block and inline candidate sets overlap, both axes snap to the box common to both.
    // snapAreaIndices are in ascending tree order, so iterating this axis's candidates and taking the
    // first also present in the other axis's candidates yields the first-in-tree box of the
    // intersection, which is the same box for both axes.
    auto otherCandidateIndices = innermostAlignedAreaIndicesForAxis(axis == ScrollEventAxis::Horizontal ? ScrollEventAxis::Vertical : ScrollEventAxis::Horizontal);
    if (!otherCandidateIndices.isEmpty()) {
        HashSet<NodeIdentifier> otherAxisIDs;
        for (auto areaIndex : otherCandidateIndices)
            otherAxisIDs.add(areaIDs[areaIndex]);
        for (auto areaIndex : candidateIndices) {
            if (otherAxisIDs.contains(areaIDs[areaIndex]))
                return areaIDs[areaIndex];
        }
    }

    // Otherwise, the first in tree order.
    return areaIDs[candidateIndices.first()];
}

// The snap areas aligned at this axis's active offset, minus any area that encloses another aligned
// area (an ancestor), in tree order. This is the per-axis candidate list from
// https://drafts.csswg.org/css-scroll-snap/#multiple-aligned-snap-areas after ancestor removal.
Vector<size_t, 1> ScrollSnapAnimatorState::innermostAlignedAreaIndicesForAxis(ScrollEventAxis axis) const
{
    Vector<size_t, 1> candidateIndices;
    auto offsets = currentlySnappedOffsetsForAxis(axis);
    if (offsets.isEmpty())
        return candidateIndices;

    const auto& areas = m_snapOffsetsInfo.snapAreas;
    const auto& areaIDs = m_snapOffsetsInfo.snapAreasIDs;
    const auto& indices = offsets[0].snapAreaIndices;

    auto enclosesAnotherArea = [&](size_t areaIndex) {
        return std::ranges::any_of(indices, [&](size_t other) {
            return other != areaIndex && other < areas.size()
                && areas[areaIndex] != areas[other] && areas[areaIndex].contains(areas[other]);
        });
    };

    for (auto areaIndex : indices) {
        if (areaIndex < areas.size() && areaIndex < areaIDs.size() && !enclosesAnotherArea(areaIndex))
            candidateIndices.append(areaIndex);
    }
    return candidateIndices;
}

void ScrollSnapAnimatorState::updateCurrentlySnappedBoxes()
{
    auto horizontalOffsets = currentlySnappedOffsetsForAxis(ScrollEventAxis::Horizontal);
    auto verticalOffsets = currentlySnappedOffsetsForAxis(ScrollEventAxis::Vertical);

    m_currentlySnappedBoxes = currentlySnappedBoxes(horizontalOffsets, verticalOffsets);
    m_currentSnapTargetForHorizontalAxis = selectSnapTargetForAxis(ScrollEventAxis::Horizontal);
    m_currentSnapTargetForVerticalAxis = selectSnapTargetForAxis(ScrollEventAxis::Vertical);
}

// Returns the snapped box this axis is focused/targeted on, evaluated fresh: focus and :target can
// change after the snap, and the re-snap algorithm evaluates them at re-snap time.
static std::optional<NodeIdentifier> focusedOrTargetedBox(const Vector<SnapOffset<LayoutUnit>>& offsets, const HashSet<NodeIdentifier>& snappedBoxes)
{
    auto findFlagged = [&](bool SnapOffset<LayoutUnit>::*flag) -> std::optional<NodeIdentifier> {
        auto found = std::ranges::find_if(offsets, [&](const SnapOffset<LayoutUnit>& offset) {
            return offset.snapTargetID && snappedBoxes.contains(*offset.snapTargetID) && offset.*flag;
        });
        if (found != offsets.end())
            return *found->snapTargetID;
        return std::nullopt;
    };

    if (auto box = findFlagged(&SnapOffset<LayoutUnit>::isFocused))
        return box;
    if (auto box = findFlagged(&SnapOffset<LayoutUnit>::isTarget))
        return box;
    return std::nullopt;
}

bool ScrollSnapAnimatorState::resnapAfterLayout(ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale)
{
    bool snapPointChanged = false;
    auto activeHorizontalIndex = activeSnapIndexForAxis(ScrollEventAxis::Horizontal);
    auto activeVerticalIndex = activeSnapIndexForAxis(ScrollEventAxis::Vertical);
    auto previouslySnappedBoxes = std::exchange(m_currentlySnappedBoxes, { });
    auto previousSnapTargetForHorizontalAxis = std::exchange(m_currentSnapTargetForHorizontalAxis, { });
    auto previousSnapTargetForVerticalAxis = std::exchange(m_currentSnapTargetForVerticalAxis, { });

    // Check if we need to set the current indices
    if (!activeVerticalIndex || *activeVerticalIndex >= snapOffsetsForAxis(ScrollEventAxis::Vertical).size())
        snapPointChanged |= setNearestScrollSnapIndexForAxisAndOffsetInternal(ScrollEventAxis::Vertical, scrollOffset, scrollExtents, pageScale);
    if (!activeHorizontalIndex || *activeHorizontalIndex >= snapOffsetsForAxis(ScrollEventAxis::Horizontal).size())
        snapPointChanged |= setNearestScrollSnapIndexForAxisAndOffsetInternal(ScrollEventAxis::Horizontal, scrollOffset, scrollExtents, pageScale);

    updateCurrentlySnappedBoxes();
    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollSnapAnimatorState::resnapAfterLayout() - previouslySnappedBoxes " << previouslySnappedBoxes << " m_currentlySnappedBoxes " << m_currentlySnappedBoxes);

    // Re-snap each axis independently to its selected box, following it to its post-layout offset.
    // Focused/targeted boxes are re-evaluated fresh; otherwise the box recorded before the layout
    // change (innermost / first-in-tree) is used. Per-axis selection keeps a box tied only in the
    // other axis from dragging this axis off its snap position.
    if (previouslySnappedBoxes.size() > 1) {
        auto targetForAxis = [&](ScrollEventAxis axis, const Markable<NodeIdentifier>& recordedTarget) -> std::optional<NodeIdentifier> {
            if (auto box = focusedOrTargetedBox(snapOffsetsForAxis(axis), previouslySnappedBoxes))
                return box;
            return recordedTarget ? std::optional<NodeIdentifier> { *recordedTarget } : std::nullopt;
        };

        auto targetForHorizontalAxis = targetForAxis(ScrollEventAxis::Horizontal, previousSnapTargetForHorizontalAxis);
        auto targetForVerticalAxis = targetForAxis(ScrollEventAxis::Vertical, previousSnapTargetForVerticalAxis);

        if (targetForHorizontalAxis)
            snapPointChanged |= preserveCurrentTargetForAxis(ScrollEventAxis::Horizontal, *targetForHorizontalAxis);
        if (targetForVerticalAxis)
            snapPointChanged |= preserveCurrentTargetForAxis(ScrollEventAxis::Vertical, *targetForVerticalAxis);

        updateCurrentlySnappedBoxes();

        // Keep the box we actually followed as this axis's recorded target. updateCurrentlySnappedBoxes()
        // re-runs selection, which could pick a different aligned box (first-in-tree, or one that is now
        // common to both axes) and cause the scroller to abandon its tracked target on a later layout
        // change. The followed box is authoritative, so restore it when it is still snapped.
        if (targetForHorizontalAxis && m_currentlySnappedBoxes.contains(*targetForHorizontalAxis))
            m_currentSnapTargetForHorizontalAxis = *targetForHorizontalAxis;
        if (targetForVerticalAxis && m_currentlySnappedBoxes.contains(*targetForVerticalAxis))
            m_currentSnapTargetForVerticalAxis = *targetForVerticalAxis;

        LOG_WITH_STREAM(ScrollSnap, stream << "ScrollSnapAnimatorState::resnapAfterLayout() - multiple boxes snapped; chose H " << targetForHorizontalAxis << " V " << targetForVerticalAxis << " (changed " << snapPointChanged << ") m_currentlySnappedBoxes " << m_currentlySnappedBoxes);
    }

    return snapPointChanged;
}

bool ScrollSnapAnimatorState::setNearestScrollSnapIndexForAxisAndOffsetInternal(ScrollEventAxis axis, ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale)
{
    auto activeIndex = closestSnapPointForOffset(axis, scrollOffset, scrollExtents, pageScale);
    if (activeIndex == activeSnapIndexForAxis(axis))
        return false;

    setActiveSnapIndexForAxisInternal(axis, activeIndex);
    return true;
}

bool ScrollSnapAnimatorState::setNearestScrollSnapIndexForOffset(ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale)
{
    bool snapIndexChanged = false;
    snapIndexChanged |= setNearestScrollSnapIndexForAxisAndOffsetInternal(ScrollEventAxis::Horizontal, scrollOffset, scrollExtents, pageScale);
    snapIndexChanged |= setNearestScrollSnapIndexForAxisAndOffsetInternal(ScrollEventAxis::Vertical, scrollOffset, scrollExtents, pageScale);

    updateCurrentlySnappedBoxes();

    return snapIndexChanged;
}

void ScrollSnapAnimatorState::transitionToUserInteractionState()
{
    teardownAnimationForState(ScrollSnapState::UserInteraction);
}

void ScrollSnapAnimatorState::transitionToDestinationReachedState()
{
    teardownAnimationForState(ScrollSnapState::DestinationReached);
}

void ScrollSnapAnimatorState::teardownAnimationForState(ScrollSnapState state)
{
    ASSERT(state == ScrollSnapState::UserInteraction || state == ScrollSnapState::DestinationReached);
    if (m_currentState == state)
        return;

    m_scrollController.stopAnimatedScroll();
    m_currentState = state;
}

std::pair<float, std::optional<unsigned>> ScrollSnapAnimatorState::targetOffsetForStartOffset(ScrollEventAxis axis, const ScrollExtents& scrollExtents, float startOffset, FloatPoint predictedOffset, float pageScale, float initialDelta) const
{
    auto minScrollOffset = (axis == ScrollEventAxis::Horizontal) ? scrollExtents.minimumScrollOffset().x() : scrollExtents.minimumScrollOffset().y();
    auto maxScrollOffset = (axis == ScrollEventAxis::Horizontal) ? scrollExtents.maximumScrollOffset().x() : scrollExtents.maximumScrollOffset().y();

    const auto& snapOffsets = m_snapOffsetsInfo.offsetsForAxis(axis);
    if (snapOffsets.isEmpty())
        return std::make_pair(clampTo<float>(axis == ScrollEventAxis::Horizontal ? predictedOffset.x() : predictedOffset.y(), minScrollOffset, maxScrollOffset), std::nullopt);

    LayoutPoint predictedLayoutOffset(predictedOffset.x() / pageScale, predictedOffset.y() / pageScale);
    auto [targetOffset, snapIndex] = m_snapOffsetsInfo.closestSnapOffset(axis, LayoutSize { scrollExtents.viewportSize }, predictedLayoutOffset, initialDelta, LayoutUnit(startOffset / pageScale));
    return std::make_pair(pageScale * clampTo<float>(float { targetOffset }, minScrollOffset, maxScrollOffset), snapIndex);
}

TextStream& operator<<(TextStream& ts, const ScrollSnapAnimatorState& state)
{
    ts << "ScrollSnapAnimatorState"_s;
    ts.dumpProperty("snap offsets x"_s, state.snapOffsetsForAxis(ScrollEventAxis::Horizontal));
    ts.dumpProperty("snap offsets y"_s, state.snapOffsetsForAxis(ScrollEventAxis::Vertical));

    ts.dumpProperty("active snap index x"_s, state.activeSnapIndexForAxis(ScrollEventAxis::Horizontal));
    ts.dumpProperty("active snap index y"_s, state.activeSnapIndexForAxis(ScrollEventAxis::Vertical));

    return ts;
}

} // namespace WebCore
