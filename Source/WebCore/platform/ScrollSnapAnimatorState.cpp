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

float ScrollSnapAnimatorState::adjustedScrollDestination(ScrollEventAxis axis, FloatPoint destinationOffset, float velocity, std::optional<float> originalOffset, const ScrollExtents& scrollExtents, float pageScale, ScrollSnapPointSelectionMethod selectionMethod) const
{
    auto snapOffsets = snapOffsetsForAxis(axis);
    if (!snapOffsets.size())
        return valueForAxis(destinationOffset, axis);

    std::optional<LayoutUnit> originalOffsetInLayoutUnits;
    if (originalOffset)
        originalOffsetInLayoutUnits = LayoutUnit(*originalOffset / pageScale);
    LayoutSize viewportSize(scrollExtents.viewportSize);

    // Clamp the destination to the reachable scroll range before choosing a snap offset.
    auto minScrollOffset = valueForAxis(scrollExtents.minimumScrollOffset(), axis);
    auto maxScrollOffset = valueForAxis(scrollExtents.maximumScrollOffset(), axis);
    destinationOffset = setValueForAxis(destinationOffset, axis, clampTo<float>(valueForAxis(destinationOffset, axis), minScrollOffset, maxScrollOffset));

    LayoutPoint layoutDestinationOffset(destinationOffset.x() / pageScale, destinationOffset.y() / pageScale);
    LayoutUnit offset = snapOffsetInfo().closestSnapOffset(axis, viewportSize, layoutDestinationOffset, velocity, originalOffsetInLayoutUnits, selectionMethod).first;
    return offset * pageScale;
}

// The snap offset nodeID contributes to in this axis, plus nodeID's own area at that offset: a box can
// be any of the areas sharing an offset, not just its representative snapTargetID, so we find both here.
auto ScrollSnapAnimatorState::snapOffsetAndAreaIndicesForNode(ScrollEventAxis axis, NodeIdentifier nodeID) const -> std::optional<SnapOffsetAndAreaIndices>
{
    const auto& snapOffsets = snapOffsetsForAxis(axis);
    const auto& areaIDs = m_snapOffsetsInfo.snapAreasIDs;

    for (unsigned offsetIndex = 0; offsetIndex < snapOffsets.size(); ++offsetIndex) {
        const auto& offset = snapOffsets[offsetIndex];
        for (auto areaIndex : offset.snapAreaIndices) {
            if (areaIndex < areaIDs.size() && areaIDs[areaIndex] == nodeID)
                return SnapOffsetAndAreaIndices { offsetIndex, areaIndex };
        }
        if (offset.snapTargetID == nodeID)
            return SnapOffsetAndAreaIndices { offsetIndex, std::nullopt };
    }
    return std::nullopt;
}

// Returns the index of the snap offset that nodeID contributes to, if any.
std::optional<unsigned> ScrollSnapAnimatorState::snapOffsetIndexForNode(ScrollEventAxis axis, NodeIdentifier nodeID) const
{
    if (auto indices = snapOffsetAndAreaIndicesForNode(axis, nodeID))
        return indices->offsetIndex;
    return std::nullopt;
}

// Returns whether the snap point is changed or not
bool ScrollSnapAnimatorState::preserveCurrentTargetForAxis(ScrollEventAxis axis, NodeIdentifier nodeID)
{
    auto index = snapOffsetIndexForNode(axis, nodeID);
    if (!index) {
        setActiveSnapIndexForAxis(axis, std::nullopt);
        return false;
    }

    setActiveSnapIndexForAxis(axis, *index);
    return true;
}

// The first box among this axis's snap offsets carrying flag as the offset's representative target and
// accepted by isEligible; callers ask for isFocused before isTarget to get the spec's preference order.
Markable<NodeIdentifier> ScrollSnapAnimatorState::flaggedNodeForAxis(ScrollEventAxis axis, bool SnapOffset<LayoutUnit>::*flag, NOESCAPE const SnapOffsetPredicate& isEligible) const
{
    for (const auto& offset : snapOffsetsForAxis(axis)) {
        if (offset.*flag && offset.snapTargetID && (!isEligible || isEligible(offset)))
            return offset.snapTargetID;
    }
    return { };
}

// The focused (then fragment-targeted) box among all of this axis's snap offsets, evaluated fresh so
// a focus/:target change can be detected. Unlike focusedOrTargetedBox() this is not restricted to the
// currently-snapped boxes and does not apply the cross-axis visibility filter: it is only a change
// signal, not a selection.
Markable<NodeIdentifier> ScrollSnapAnimatorState::focusedOrTargetedNodeForAxis(ScrollEventAxis axis) const
{
    if (auto box = flaggedNodeForAxis(axis, &SnapOffset<LayoutUnit>::isFocused))
        return box;
    return flaggedNodeForAxis(axis, &SnapOffset<LayoutUnit>::isTarget);
}

Vector<SnapOffset<LayoutUnit>> ScrollSnapAnimatorState::currentlySnappedOffsetsForAxis(ScrollEventAxis axis) const
{
    Vector<SnapOffset<LayoutUnit>> currentlySnappedOffsets;
    const auto& snapOffsets = snapOffsetsForAxis(axis);
    auto activeIndex = activeSnapIndexForAxis(axis);
    
    if (activeIndex && *activeIndex < snapOffsets.size())
        currentlySnappedOffsets.append(snapOffsets[*activeIndex]);
    return currentlySnappedOffsets;
}

HashSet<NodeIdentifier> ScrollSnapAnimatorState::currentlySnappedBoxes(const Vector<SnapOffset<LayoutUnit>>& horizontalOffsets, const Vector<SnapOffset<LayoutUnit>>& verticalOffsets) const
{
    HashSet<NodeIdentifier> snappedNodeIDs;

    for (auto offset : horizontalOffsets) {
        if (!offset.snapTargetID)
            continue;
        snappedNodeIDs.add(*offset.snapTargetID);
        for (auto i : offset.snapAreaIndices)
            snappedNodeIDs.add(m_snapOffsetsInfo.snapAreasIDs[i]);
    }
    
    for (auto offset : verticalOffsets) {
        if (!offset.snapTargetID)
            continue;
        snappedNodeIDs.add(*offset.snapTargetID);
        for (auto i : offset.snapAreaIndices)
            snappedNodeIDs.add(m_snapOffsetsInfo.snapAreasIDs[i]);
    }
    return snappedNodeIDs;
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

    const auto& areaIDs = m_snapOffsetsInfo.snapAreasIDs;

    // A focused or fragment-targeted box wins outright, unless its area is scrolled out of the
    // snapport in a non-snapping cross axis (not a valid snap position).
    if ((offset.isFocused || offset.isTarget) && offset.snapTargetID) {
        bool representativeAreaIsVisible = true;
        for (auto areaIndex : offset.snapAreaIndices) {
            if (areaIndex < areaIDs.size() && areaIDs[areaIndex] == *offset.snapTargetID) {
                representativeAreaIsVisible = isSnapAreaVisibleInCrossAxis(areaIndex, axis);
                break;
            }
        }
        if (representativeAreaIsVisible)
            return *offset.snapTargetID;
    }

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
        if (areaIndex < areas.size() && areaIndex < areaIDs.size() && isSnapAreaVisibleInCrossAxis(areaIndex, axis) && !enclosesAnotherArea(areaIndex))
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

// True if the snap area is at least partly within the snapport in the (non-snapping) cross axis at
// the last known scroll position. When the cross axis also snaps (2D case, handled by
// closestSnapOffset) or no viewport is known yet, we don't filter. Mirrors findCompatibleSnapArea().
bool ScrollSnapAnimatorState::isSnapAreaVisibleInCrossAxis(size_t areaIndex, ScrollEventAxis axis) const
{
    auto crossAxis = axis == ScrollEventAxis::Horizontal ? ScrollEventAxis::Vertical : ScrollEventAxis::Horizontal;
    if (!snapOffsetsForAxis(crossAxis).isEmpty() || m_lastViewportSize.isEmpty())
        return true;
    if (areaIndex >= m_snapOffsetsInfo.snapAreas.size())
        return true;

    const auto& area = m_snapOffsetsInfo.snapAreas[areaIndex];
    auto crossMin = crossAxis == ScrollEventAxis::Horizontal ? area.x() : area.y();
    auto crossMax = crossAxis == ScrollEventAxis::Horizontal ? area.maxX() : area.maxY();
    auto crossScroll = crossAxis == ScrollEventAxis::Horizontal ? m_lastLayoutScrollOffset.x() : m_lastLayoutScrollOffset.y();
    auto crossViewport = crossAxis == ScrollEventAxis::Horizontal ? m_lastViewportSize.width() : m_lastViewportSize.height();
    return (crossScroll + crossViewport) >= crossMin && crossScroll <= crossMax;
}

// This axis's focused/targeted snapped box, evaluated fresh (focus and :target can change after the
// snap). A focused/targeted area scrolled out of the snapport in a non-snapping cross axis is skipped:
// it isn't a valid snap position, so it must not win the preference.
std::optional<NodeIdentifier> ScrollSnapAnimatorState::focusedOrTargetedBox(ScrollEventAxis axis, const HashSet<NodeIdentifier>& snappedBoxes) const
{
    const auto& areaIDs = m_snapOffsetsInfo.snapAreasIDs;

    SnapOffsetPredicate isEligible = [&](const SnapOffset<LayoutUnit>& offset) {
        if (!snappedBoxes.contains(*offset.snapTargetID))
            return false;
        for (auto areaIndex : offset.snapAreaIndices) {
            if (areaIndex < areaIDs.size() && areaIDs[areaIndex] == *offset.snapTargetID)
                return isSnapAreaVisibleInCrossAxis(areaIndex, axis);
        }
        return true;
    };

    if (auto box = flaggedNodeForAxis(axis, &SnapOffset<LayoutUnit>::isFocused, isEligible))
        return *box;
    if (auto box = flaggedNodeForAxis(axis, &SnapOffset<LayoutUnit>::isTarget, isEligible))
        return *box;
    return std::nullopt;
}

// The snap area rect (scroll-offset space) and this axis's snap offset for the area contributed by
// nodeID, if it establishes an offset in this axis. The rect lets us test whether both axes' boxes coexist.
std::optional<std::pair<LayoutRect, LayoutUnit>> ScrollSnapAnimatorState::snapAreaAndOffsetForNode(ScrollEventAxis axis, NodeIdentifier nodeID) const
{
    auto indices = snapOffsetAndAreaIndicesForNode(axis, nodeID);
    if (!indices || !indices->areaIndex || *indices->areaIndex >= m_snapOffsetsInfo.snapAreas.size())
        return std::nullopt;

    return std::make_pair(m_snapOffsetsInfo.snapAreas[*indices->areaIndex], snapOffsetsForAxis(axis)[indices->offsetIndex].offset);
}

// Per https://drafts.csswg.org/css-scroll-snap/#re-snap, when the two axes pick different boxes that
// can't both be snapped (snapping one pushes the other offscreen), collapse to one: focused, then
// targeted, then the block-axis box. Returns nullopt when both boxes can be snapped simultaneously.
std::optional<NodeIdentifier> ScrollSnapAnimatorState::resolvePreferredBoxForAxisConflict(NodeIdentifier horizontalBox, NodeIdentifier verticalBox) const
{
    if (horizontalBox == verticalBox)
        return std::nullopt;

    auto horizontal = snapAreaAndOffsetForNode(ScrollEventAxis::Horizontal, horizontalBox);
    auto vertical = snapAreaAndOffsetForNode(ScrollEventAxis::Vertical, verticalBox);
    if (!horizontal || !vertical)
        return std::nullopt;

    if (m_lastViewportSize.isEmpty())
        return std::nullopt;

    // Both boxes can be snapped only if both areas still intersect the snapport at the combined
    // position (the x that aligns the H box, the y that aligns the V box); else it's a conflict.
    LayoutPoint combinedOffset(horizontal->second, vertical->second);
    auto areaIntersectsSnapport = [&](const LayoutRect& area) {
        LayoutRect snapport(combinedOffset, m_lastViewportSize);
        return area.intersects(snapport);
    };
    if (areaIntersectsSnapport(horizontal->first) && areaIntersectsSnapport(vertical->first))
        return std::nullopt;

    // Conflict: prefer focused, then targeted, then the block-axis box.
    SnapOffsetPredicate isConflictingBox = [&](const SnapOffset<LayoutUnit>& offset) {
        return *offset.snapTargetID == horizontalBox || *offset.snapTargetID == verticalBox;
    };
    for (auto flag : { &SnapOffset<LayoutUnit>::isFocused, &SnapOffset<LayoutUnit>::isTarget }) {
        for (auto axis : { ScrollEventAxis::Horizontal, ScrollEventAxis::Vertical }) {
            if (auto box = flaggedNodeForAxis(axis, flag, isConflictingBox))
                return *box;
        }
    }

    return m_snapOffsetsInfo.blockAxis() == ScrollEventAxis::Horizontal ? horizontalBox : verticalBox;
}

bool ScrollSnapAnimatorState::resnapAfterLayout(ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale)
{
    m_lastLayoutScrollOffset = LayoutPoint(scrollOffset.x() / pageScale, scrollOffset.y() / pageScale);
    m_lastViewportSize = LayoutSize(scrollExtents.viewportSize);

    auto activeHorizontalIndex = activeSnapIndexForAxis(ScrollEventAxis::Horizontal);
    auto activeVerticalIndex = activeSnapIndexForAxis(ScrollEventAxis::Vertical);

    auto previouslySnappedBoxes = std::exchange(m_currentlySnappedBoxes, { });
    auto previousSnapTargetForHorizontalAxis = std::exchange(m_currentSnapTargetForHorizontalAxis, { });
    auto previousSnapTargetForVerticalAxis = std::exchange(m_currentSnapTargetForVerticalAxis, { });

    auto currentFocusedOrTargetedNodeX = focusedOrTargetedNodeForAxis(ScrollEventAxis::Horizontal);
    auto currentFocusedOrTargetedNodeY = focusedOrTargetedNodeForAxis(ScrollEventAxis::Vertical);
    bool focusOrTargetChangedX = currentFocusedOrTargetedNodeX != std::exchange(m_lastFocusedOrTargetedNodeX, currentFocusedOrTargetedNodeX);
    bool focusOrTargetChangedY = currentFocusedOrTargetedNodeY != std::exchange(m_lastFocusedOrTargetedNodeY, currentFocusedOrTargetedNodeY);

    bool snapPointChanged = false;
    // Check if we need to set the current indices
    if (!activeVerticalIndex || *activeVerticalIndex >= snapOffsetsForAxis(ScrollEventAxis::Vertical).size())
        snapPointChanged |= setNearestScrollSnapIndexForAxisAndOffsetInternal(ScrollEventAxis::Vertical, scrollOffset, scrollExtents, pageScale);
    if (!activeHorizontalIndex || *activeHorizontalIndex >= snapOffsetsForAxis(ScrollEventAxis::Horizontal).size())
        snapPointChanged |= setNearestScrollSnapIndexForAxisAndOffsetInternal(ScrollEventAxis::Horizontal, scrollOffset, scrollExtents, pageScale);

    updateCurrentlySnappedBoxes();
    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollSnapAnimatorState::resnapAfterLayout() - previouslySnappedBoxes " << previouslySnappedBoxes << " m_currentlySnappedBoxes " << m_currentlySnappedBoxes);

    // Re-snap each axis independently to its selected box, following it to its post-layout offset.
    // When the focus/:target changed we honor that new preference among the previously-snapped boxes;
    // otherwise we keep the box recorded before the layout change (focused/targeted / innermost /
    // first-in-tree). Per-axis selection keeps a box tied only in the other axis from dragging this
    // axis off its snap position.
    auto targetForAxis = [&](ScrollEventAxis axis, const Markable<NodeIdentifier>& recordedTarget, bool focusOrTargetChanged) -> std::optional<NodeIdentifier> {
        if (focusOrTargetChanged) {
            if (auto box = focusedOrTargetedBox(axis, previouslySnappedBoxes))
                return box;
        }
        // Keep the recorded target only if it is still a snap target for this axis; otherwise fall
        // through so the nearest-offset selection made above stands.
        if (recordedTarget && snapOffsetIndexForNode(axis, *recordedTarget))
            return recordedTarget;
        return std::nullopt;
    };

    auto targetForHorizontalAxis = targetForAxis(ScrollEventAxis::Horizontal, previousSnapTargetForHorizontalAxis, focusOrTargetChangedX);
    auto targetForVerticalAxis = targetForAxis(ScrollEventAxis::Vertical, previousSnapTargetForVerticalAxis, focusOrTargetChangedY);

    // §4.1.3 "prefer block axis": if the two axes' boxes conflict, collapse both onto the one box.
    if (targetForHorizontalAxis && targetForVerticalAxis) {
        if (auto preferredBox = resolvePreferredBoxForAxisConflict(*targetForHorizontalAxis, *targetForVerticalAxis)) {
            targetForHorizontalAxis = *preferredBox;
            targetForVerticalAxis = *preferredBox;
        }
    }

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

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollSnapAnimatorState::resnapAfterLayout() - chose H " << targetForHorizontalAxis << " V " << targetForVerticalAxis << " (changed " << snapPointChanged << ") m_currentlySnappedBoxes " << m_currentlySnappedBoxes);

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
    m_lastLayoutScrollOffset = LayoutPoint(scrollOffset.x() / pageScale, scrollOffset.y() / pageScale);
    m_lastViewportSize = LayoutSize(scrollExtents.viewportSize);
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

    // Clamp the predicted destination to the reachable scroll range before choosing a snap offset.
    auto clampedAxisOffset = clampTo<float>(axis == ScrollEventAxis::Horizontal ? predictedOffset.x() : predictedOffset.y(), minScrollOffset, maxScrollOffset) / pageScale;
    if (axis == ScrollEventAxis::Horizontal)
        predictedLayoutOffset.setX(LayoutUnit(clampedAxisOffset));
    else
        predictedLayoutOffset.setY(LayoutUnit(clampedAxisOffset));

    auto [targetOffset, snapIndex] = m_snapOffsetsInfo.closestSnapOffset(axis, LayoutSize { scrollExtents.viewportSize }, predictedLayoutOffset, initialDelta, LayoutUnit(startOffset / pageScale));
    targetOffset = clampTo<float>(float { targetOffset }, minScrollOffset, maxScrollOffset) * pageScale;
    return std::make_pair(targetOffset, snapIndex);
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
