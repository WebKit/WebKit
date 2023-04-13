/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

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
bool ScrollSnapAnimatorState::preserveCurrentTargetForAxis(ScrollEventAxis axis, ElementIdentifier boxID)
{
    auto snapOffsets = snapOffsetsForAxis(axis);
    
    auto found = std::find_if(snapOffsets.begin(), snapOffsets.end(), [boxID](SnapOffset<LayoutUnit> p) -> bool {
        return p.snapTargetID == boxID;
    });
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

HashSet<ElementIdentifier> ScrollSnapAnimatorState::currentlySnappedBoxes(const Vector<SnapOffset<LayoutUnit>>& horizontalOffsets, const Vector<SnapOffset<LayoutUnit>>& verticalOffsets) const
{
    HashSet<ElementIdentifier> snappedBoxIDs;
        
    for (auto offset : horizontalOffsets) {
        snappedBoxIDs.add(offset.snapTargetID);
        for (auto i : offset.snapAreaIndices)
            snappedBoxIDs.add(m_snapOffsetsInfo.snapAreasIDs[i]);
    }
    
    for (auto offset : verticalOffsets) {
        snappedBoxIDs.add(offset.snapTargetID);
        for (auto i : offset.snapAreaIndices)
            snappedBoxIDs.add(m_snapOffsetsInfo.snapAreasIDs[i]);
    }
    return snappedBoxIDs;
}

ElementIdentifier ScrollSnapAnimatorState::chooseBoxToResnapTo(const Vector<SnapOffset<LayoutUnit>>& horizontalOffsets, const Vector<SnapOffset<LayoutUnit>>& verticalOffsets) const
{
    auto found = std::find_if(horizontalOffsets.begin(), horizontalOffsets.end(), [this](SnapOffset<LayoutUnit> p) -> bool {
        return m_currentlySnappedBoxes.contains(p.snapTargetID) && p.isFocused;
    });
    if (found != horizontalOffsets.end())
        return found->snapTargetID;
    
    found = std::find_if(verticalOffsets.begin(), verticalOffsets.end(), [this](SnapOffset<LayoutUnit> p) -> bool {
        return m_currentlySnappedBoxes.contains(p.snapTargetID) && p.isFocused;
    });
    if (found != verticalOffsets.end())
        return found->snapTargetID;
    
    return *m_currentlySnappedBoxes.begin();
}

bool ScrollSnapAnimatorState::resnapAfterLayout(ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale)
{
    bool snapPointChanged = false;
    auto activeHorizontalIndex = activeSnapIndexForAxis(ScrollEventAxis::Horizontal);
    auto activeVerticalIndex = activeSnapIndexForAxis(ScrollEventAxis::Vertical);
    auto snapOffsetsVertical = snapOffsetsForAxis(ScrollEventAxis::Vertical);
    auto snapOffsetsHorizontal = snapOffsetsForAxis(ScrollEventAxis::Horizontal);
    
    // Check if we need to set the current indices
    if (!activeVerticalIndex || *activeVerticalIndex >= snapOffsetsForAxis(ScrollEventAxis::Vertical).size()) {
        snapPointChanged |= setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Vertical, scrollOffset, scrollExtents, pageScale);
        activeVerticalIndex = activeSnapIndexForAxis(ScrollEventAxis::Vertical);
    }
    if (!activeHorizontalIndex || *activeHorizontalIndex >= snapOffsetsForAxis(ScrollEventAxis::Horizontal).size()) {
        snapPointChanged |= setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, scrollOffset, scrollExtents, pageScale);
        activeHorizontalIndex = activeSnapIndexForAxis(ScrollEventAxis::Horizontal);
    }
    
    auto horizontalOffsets = currentlySnappedOffsetsForAxis(ScrollEventAxis::Horizontal);
    auto verticalOffsets = currentlySnappedOffsetsForAxis(ScrollEventAxis::Vertical);

    auto currentlySnapped = currentlySnappedBoxes(horizontalOffsets, verticalOffsets);
    
    auto wasSnappedToMultipleBoxes = m_currentlySnappedBoxes.size() > 1;
    auto currentlySnappedToMultipleBoxes = currentlySnapped.size() > 1;
    
    if (wasSnappedToMultipleBoxes && !currentlySnappedToMultipleBoxes) {
        // Pick offset to snap to
        auto box = chooseBoxToResnapTo(snapOffsetsHorizontal, snapOffsetsVertical);
        snapPointChanged |= preserveCurrentTargetForAxis(ScrollEventAxis::Horizontal, box);
        snapPointChanged |= preserveCurrentTargetForAxis(ScrollEventAxis::Vertical, box);
        horizontalOffsets = currentlySnappedOffsetsForAxis(ScrollEventAxis::Horizontal);
        verticalOffsets = currentlySnappedOffsetsForAxis(ScrollEventAxis::Vertical);
        currentlySnapped = currentlySnappedBoxes(horizontalOffsets, verticalOffsets);
        currentlySnappedToMultipleBoxes = currentlySnapped.size() > 1;
        LOG_WITH_STREAM(ScrollSnap, stream << "ScrollSnapAnimatorState::resnapAfterLayout() current target: " << box << " snapPointChanged: " << snapPointChanged);
    }
    if (wasSnappedToMultipleBoxes != currentlySnappedToMultipleBoxes)
        m_currentlySnappedBoxes = currentlySnapped;

    return snapPointChanged;
}

bool ScrollSnapAnimatorState::setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis axis, ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale)
{
    auto activeIndex = closestSnapPointForOffset(axis, scrollOffset, scrollExtents, pageScale);
    if (activeIndex == activeSnapIndexForAxis(axis))
        return false;

    setActiveSnapIndexForAxis(axis, activeIndex);
    return true;
}

bool ScrollSnapAnimatorState::setNearestScrollSnapIndexForOffset(ScrollOffset scrollOffset, const ScrollExtents& scrollExtents, float pageScale)
{
    bool snapIndexChanged = false;
    snapIndexChanged |= setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, scrollOffset, scrollExtents, pageScale);
    snapIndexChanged |= setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Vertical, scrollOffset, scrollExtents, pageScale);
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
    ts << "ScrollSnapAnimatorState";
    ts.dumpProperty("snap offsets x", state.snapOffsetsForAxis(ScrollEventAxis::Horizontal));
    ts.dumpProperty("snap offsets y", state.snapOffsetsForAxis(ScrollEventAxis::Vertical));

    ts.dumpProperty("active snap index x", state.activeSnapIndexForAxis(ScrollEventAxis::Horizontal));
    ts.dumpProperty("active snap index y", state.activeSnapIndexForAxis(ScrollEventAxis::Vertical));

    return ts;
}

} // namespace WebCore
