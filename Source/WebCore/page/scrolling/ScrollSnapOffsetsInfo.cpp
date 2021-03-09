/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Igalia S.L.
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
#include "ScrollSnapOffsetsInfo.h"

#if ENABLE(CSS_SCROLL_SNAP)

#include "ElementChildIterator.h"
#include "LayoutRect.h"
#include "Length.h"
#include "Logging.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "ScrollableArea.h"
#include "StyleScrollSnapPoints.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

template <typename LayoutType>
static void indicesOfNearestSnapOffsetRanges(LayoutType offset, const Vector<ScrollOffsetRange<LayoutType>>& snapOffsetRanges, unsigned& lowerIndex, unsigned& upperIndex)
{
    if (snapOffsetRanges.isEmpty()) {
        lowerIndex = invalidSnapOffsetIndex;
        upperIndex = invalidSnapOffsetIndex;
        return;
    }

    int lowerIndexAsInt = -1;
    int upperIndexAsInt = snapOffsetRanges.size();
    do {
        int middleIndex = (lowerIndexAsInt + upperIndexAsInt) / 2;
        auto& range = snapOffsetRanges[middleIndex];
        if (range.start < offset && offset < range.end) {
            lowerIndexAsInt = middleIndex;
            upperIndexAsInt = middleIndex;
            break;
        }

        if (offset > range.end)
            lowerIndexAsInt = middleIndex;
        else
            upperIndexAsInt = middleIndex;
    } while (lowerIndexAsInt < upperIndexAsInt - 1);

    if (offset <= snapOffsetRanges.first().start)
        lowerIndex = invalidSnapOffsetIndex;
    else
        lowerIndex = lowerIndexAsInt;

    if (offset >= snapOffsetRanges.last().end)
        upperIndex = invalidSnapOffsetIndex;
    else
        upperIndex = upperIndexAsInt;
}

template <typename LayoutType>
static void indicesOfNearestSnapOffsets(LayoutType offset, const Vector<SnapOffset<LayoutType>>& snapOffsets, unsigned& lowerIndex, unsigned& upperIndex)
{
    lowerIndex = 0;
    upperIndex = snapOffsets.size() - 1;
    while (lowerIndex < upperIndex - 1) {
        int middleIndex = (lowerIndex + upperIndex) / 2;
        auto middleOffset = snapOffsets[middleIndex].offset;
        if (offset == middleOffset) {
            upperIndex = middleIndex;
            lowerIndex = middleIndex;
            break;
        }

        if (offset > middleOffset)
            lowerIndex = middleIndex;
        else
            upperIndex = middleIndex;
    }
}

template <typename LayoutType>
static Optional<unsigned> findFirstSnapStopOffsetBetweenOriginAndDestination(const Vector<SnapOffset<LayoutType>>& snapOffsets, LayoutType scrollOriginOffset, LayoutType scrollDestinationOffset)
{
    LayoutType difference = scrollDestinationOffset - scrollOriginOffset;
    if (!difference)
        return WTF::nullopt;

    unsigned searchStartOffset = 0;
    size_t iteration = 1;
    if (difference < 0) {
        searchStartOffset = snapOffsets.size() - 1;
        iteration = -1;
    }

    auto isPast = [difference](LayoutType mark, LayoutType candidate) {
        return (difference > 0 && candidate > mark) || (difference < 0 && candidate < mark);
    };

    for (int i = searchStartOffset; i >= 0 && static_cast<size_t>(i) < snapOffsets.size(); i += iteration) {
        auto offset = snapOffsets[i].offset;
        if (isPast(scrollDestinationOffset, offset))
            break;
        if (snapOffsets[i].stop != ScrollSnapStop::Always)
            continue;
        if (isPast(scrollOriginOffset, offset))
            return i;
    }

    return WTF::nullopt;
}

template <typename LayoutType>
static std::pair<LayoutType, unsigned> closestSnapOffsetWithOffsetsAndRanges(const Vector<SnapOffset<LayoutType>>& snapOffsets, const Vector<ScrollOffsetRange<LayoutType>>& snapOffsetRanges, LayoutType scrollDestinationOffset, float velocity, Optional<LayoutType> originalOffsetForDirectionalSnapping)
{
    if (snapOffsets.isEmpty())
        return std::make_pair(scrollDestinationOffset, invalidSnapOffsetIndex);

    if (originalOffsetForDirectionalSnapping.hasValue()) {
        auto firstSnapStopOffsetIndex = findFirstSnapStopOffsetBetweenOriginAndDestination(snapOffsets, *originalOffsetForDirectionalSnapping, scrollDestinationOffset);
        if (firstSnapStopOffsetIndex.hasValue())
            return std::make_pair(snapOffsets[*firstSnapStopOffsetIndex].offset, *firstSnapStopOffsetIndex);
    }

    unsigned lowerSnapOffsetRangeIndex;
    unsigned upperSnapOffsetRangeIndex;
    indicesOfNearestSnapOffsetRanges<LayoutType>(scrollDestinationOffset, snapOffsetRanges, lowerSnapOffsetRangeIndex, upperSnapOffsetRangeIndex);
    if (lowerSnapOffsetRangeIndex == upperSnapOffsetRangeIndex && upperSnapOffsetRangeIndex != invalidSnapOffsetIndex)
        return std::make_pair(scrollDestinationOffset, invalidSnapOffsetIndex);

    if (scrollDestinationOffset <= snapOffsets.first().offset)
        return std::make_pair(snapOffsets.first().offset, 0u);

    if (scrollDestinationOffset >= snapOffsets.last().offset)
        return std::make_pair(snapOffsets.last().offset, snapOffsets.size() - 1);

    unsigned lowerIndex;
    unsigned upperIndex;
    indicesOfNearestSnapOffsets<LayoutType>(scrollDestinationOffset, snapOffsets, lowerIndex, upperIndex);
    LayoutType lowerSnapPosition = snapOffsets[lowerIndex].offset;
    LayoutType upperSnapPosition = snapOffsets[upperIndex].offset;
    if (!std::abs(velocity)) {
        bool isCloserToLowerSnapPosition = scrollDestinationOffset - lowerSnapPosition <= upperSnapPosition - scrollDestinationOffset;
        return isCloserToLowerSnapPosition ? std::make_pair(lowerSnapPosition, lowerIndex) : std::make_pair(upperSnapPosition, upperIndex);
    }

    // Non-zero velocity indicates a flick gesture. Even if another snap point is closer, we should choose the one in the direction of the flick gesture
    // as long as a scroll snap offset range does not lie between the scroll destination and the targeted snap offset. If we are doing directional
    // snapping, we should never snap to a point that was on the other side of the original position in the opposite direction of this scroll.
    // This allows directional scrolling to escape snap points.
    if (velocity < 0) {
        if (lowerSnapOffsetRangeIndex == invalidSnapOffsetIndex || lowerSnapPosition >= snapOffsetRanges[lowerSnapOffsetRangeIndex].end)
            return std::make_pair(lowerSnapPosition, lowerIndex);
        if (!originalOffsetForDirectionalSnapping.hasValue() || *originalOffsetForDirectionalSnapping > upperSnapPosition)
            return std::make_pair(upperSnapPosition, upperIndex);
    } else {
        if (upperSnapOffsetRangeIndex == invalidSnapOffsetIndex || snapOffsetRanges[upperSnapOffsetRangeIndex].start >= upperSnapPosition)
            return std::make_pair(upperSnapPosition, upperIndex);
        if (!originalOffsetForDirectionalSnapping.hasValue() || *originalOffsetForDirectionalSnapping < lowerSnapPosition)
            return std::make_pair(lowerSnapPosition, lowerIndex);
    }

    return std::make_pair(scrollDestinationOffset, invalidSnapOffsetIndex);
}

enum class InsetOrOutset {
    Inset,
    Outset
};

static LayoutRect computeScrollSnapPortOrAreaRect(const LayoutRect& rect, const LengthBox& insetOrOutsetBox, InsetOrOutset insetOrOutset)
{
    // We are using minimumValueForLength here for insetOrOutset box, because if this value is defined by scroll-padding then the
    // Length of any side may be "auto." In that case, we want to use 0, because that is how WebKit currently interprets an "auto"
    // value for scroll-padding. See: https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-padding
    LayoutBoxExtent extents(
        minimumValueForLength(insetOrOutsetBox.top(), rect.height()), minimumValueForLength(insetOrOutsetBox.right(), rect.width()),
        minimumValueForLength(insetOrOutsetBox.bottom(), rect.height()), minimumValueForLength(insetOrOutsetBox.left(), rect.width()));
    auto snapPortOrArea(rect);
    if (insetOrOutset == InsetOrOutset::Inset)
        snapPortOrArea.contract(extents);
    else
        snapPortOrArea.expand(extents);
    return snapPortOrArea;
}

static LayoutUnit computeScrollSnapAlignOffset(LayoutUnit minLocation, LayoutUnit maxLocation, ScrollSnapAxisAlignType alignment, bool axisIsFlipped)
{
    switch (alignment) {
    case ScrollSnapAxisAlignType::Start:
        return axisIsFlipped ? maxLocation : minLocation;
    case ScrollSnapAxisAlignType::Center:
        return (minLocation + maxLocation) / 2;
    case ScrollSnapAxisAlignType::End:
        return axisIsFlipped ? minLocation : maxLocation;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

static void computeAxisProximitySnapOffsetRanges(const Vector<SnapOffset<LayoutUnit>>& snapOffsets, Vector<ScrollOffsetRange<LayoutUnit>>& offsetRanges, LayoutUnit scrollPortAxisLength)
{
    // This is an arbitrary choice for what it means to be "in proximity" of a snap offset. We should play around with
    // this and see what feels best.
    static const float ratioOfScrollPortAxisLengthToBeConsideredForProximity = 0.3;
    if (snapOffsets.size() < 2)
        return;

    // The extra rule accounting for scroll offset ranges in between the scroll destination and a potential snap offset
    // handles the corner case where the user scrolls with momentum very lightly away from a snap offset, such that the
    // predicted scroll destination is still within proximity of the snap offset. In this case, the regular (mandatory
    // scroll snapping) behavior would be to snap to the next offset in the direction of momentum scrolling, but
    // instead, it is more intuitive to either return to the original snap position (which we arbitrarily choose here)
    // or scroll just outside of the snap offset range. This is another minor behavior tweak that we should play around
    // with to see what feels best.
    LayoutUnit proximityDistance { ratioOfScrollPortAxisLengthToBeConsideredForProximity * scrollPortAxisLength };
    for (size_t index = 1; index < snapOffsets.size(); ++index) {
        auto startOffset = snapOffsets[index - 1].offset + proximityDistance;
        auto endOffset = snapOffsets[index].offset - proximityDistance;
        if (startOffset < endOffset)
            offsetRanges.append({ startOffset, endOffset });
    }
}

void updateSnapOffsetsForScrollableArea(ScrollableArea& scrollableArea, const RenderBox& scrollingElementBox, const RenderStyle& scrollingElementStyle, LayoutRect viewportRectInBorderBoxCoordinates)
{
    auto scrollSnapType = scrollingElementStyle.scrollSnapType();
    const auto& boxesWithScrollSnapPositions = scrollingElementBox.view().boxesWithScrollSnapPositions();
    if (scrollSnapType.strictness == ScrollSnapStrictness::None || boxesWithScrollSnapPositions.isEmpty()) {
        scrollableArea.clearSnapOffsets();
        return;
    }

    auto addOrUpdateStopForSnapOffset = [](HashMap<float, SnapOffset<LayoutUnit>>& offsets, SnapOffset<LayoutUnit> newOffset)
    {
        // If the offset already exists, we ensure that it has ScrollSnapStop::Always, when appropriate.
        auto addResult = offsets.add(newOffset.offset, newOffset);
        if (newOffset.stop == ScrollSnapStop::Always)
            addResult.iterator->value.stop = ScrollSnapStop::Always;
    };

    HashMap<float, SnapOffset<LayoutUnit>> verticalSnapOffsetsMap;
    HashMap<float, SnapOffset<LayoutUnit>> horizontalSnapOffsetsMap;
    bool hasHorizontalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::XAxis || scrollSnapType.axis == ScrollSnapAxis::Inline;
    bool hasVerticalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::YAxis || scrollSnapType.axis == ScrollSnapAxis::Block;

    auto maxScrollOffset = scrollableArea.maximumScrollOffset();
    auto scrollPosition = LayoutPoint { scrollableArea.scrollPosition() };
    bool scrollerIsRTL = !scrollingElementBox.style().isLeftToRightDirection();

    // The bounds of the scrolling container's snap port, where the top left of the scrolling container's border box is the origin.
    auto scrollSnapPort = computeScrollSnapPortOrAreaRect(viewportRectInBorderBoxCoordinates, scrollingElementStyle.scrollPadding(), InsetOrOutset::Inset);
    LOG_WITH_STREAM(ScrollSnap, stream << "Computing scroll snap offsets for " << scrollableArea << " in snap port " << scrollSnapPort);
    for (auto* child : boxesWithScrollSnapPositions) {
        if (child->enclosingScrollableContainerForSnapping() != &scrollingElementBox)
            continue;

        // The bounds of the child element's snap area, where the top left of the scrolling container's border box is the origin.
        // The snap area is the bounding box of the child element's border box, after applying transformations.
        // FIXME: For now, just consider whether the scroller is RTL. The behavior of LTR boxes inside a RTL scroller is poorly defined: https://github.com/w3c/csswg-drafts/issues/5361.
        auto scrollSnapArea = LayoutRect(child->localToContainerQuad(FloatQuad(child->borderBoundingBox()), &scrollingElementBox).boundingBox());

        // localToContainerQuad will transform the scroll snap area by the scroll position, except in the case that this position is
        // coming from a ScrollView. We want the transformed area, but without scroll position taken into account.
        if (!scrollableArea.isScrollView())
            scrollSnapArea.moveBy(scrollPosition);

        scrollSnapArea = computeScrollSnapPortOrAreaRect(scrollSnapArea, child->style().scrollMargin(), InsetOrOutset::Outset);
        LOG_WITH_STREAM(ScrollSnap, stream << "    Considering scroll snap target area " << scrollSnapArea);
        auto alignment = child->style().scrollSnapAlign();
        auto stop = child->style().scrollSnapStop();
        if (hasHorizontalSnapOffsets && alignment.x != ScrollSnapAxisAlignType::None) {
            auto absoluteScrollXPosition = computeScrollSnapAlignOffset(scrollSnapArea.x(), scrollSnapArea.maxX(), alignment.x, scrollerIsRTL) - computeScrollSnapAlignOffset(scrollSnapPort.x(), scrollSnapPort.maxX(), alignment.x, scrollerIsRTL);
            auto absoluteScrollOffset = clampTo<int>(scrollableArea.scrollOffsetFromPosition({ roundToInt(absoluteScrollXPosition), 0 }).x(), 0, maxScrollOffset.x());
            addOrUpdateStopForSnapOffset(horizontalSnapOffsetsMap, { absoluteScrollOffset, stop });
        }
        if (hasVerticalSnapOffsets && alignment.y != ScrollSnapAxisAlignType::None) {
            auto absoluteScrollYPosition = computeScrollSnapAlignOffset(scrollSnapArea.y(), scrollSnapArea.maxY(), alignment.y, false) - computeScrollSnapAlignOffset(scrollSnapPort.y(), scrollSnapPort.maxY(), alignment.y, false);
            auto absoluteScrollOffset = clampTo<int>(scrollableArea.scrollOffsetFromPosition({ 0, roundToInt(absoluteScrollYPosition) }).y(), 0, maxScrollOffset.y());
            addOrUpdateStopForSnapOffset(verticalSnapOffsetsMap, { absoluteScrollOffset, stop });
        }
    }

    auto compareSnapOffsets = [](const SnapOffset<LayoutUnit>& a, const SnapOffset<LayoutUnit>& b)
    {
        return a.offset < b.offset;
    };

    Vector<SnapOffset<LayoutUnit>> horizontalSnapOffsets = copyToVector(horizontalSnapOffsetsMap.values());
    Vector<ScrollOffsetRange<LayoutUnit>> horizontalSnapOffsetRanges;
    if (!horizontalSnapOffsets.isEmpty()) {
        std::sort(horizontalSnapOffsets.begin(), horizontalSnapOffsets.end(), compareSnapOffsets);
        if (scrollSnapType.strictness == ScrollSnapStrictness::Proximity)
            computeAxisProximitySnapOffsetRanges(horizontalSnapOffsets, horizontalSnapOffsetRanges, scrollSnapPort.width());

        LOG_WITH_STREAM(ScrollSnap, stream << " => Computed horizontal scroll snap offsets: " << horizontalSnapOffsets);
        LOG_WITH_STREAM(ScrollSnap, stream << " => Computed horizontal scroll snap offset ranges: " << horizontalSnapOffsetRanges);
    }

    Vector<SnapOffset<LayoutUnit>> verticalSnapOffsets = copyToVector(verticalSnapOffsetsMap.values());
    Vector<ScrollOffsetRange<LayoutUnit>> verticalSnapOffsetRanges;
    if (!verticalSnapOffsets.isEmpty()) {
        std::sort(verticalSnapOffsets.begin(), verticalSnapOffsets.end(), compareSnapOffsets);
        if (scrollSnapType.strictness == ScrollSnapStrictness::Proximity)
            computeAxisProximitySnapOffsetRanges(verticalSnapOffsets, verticalSnapOffsetRanges, scrollSnapPort.height());

        LOG_WITH_STREAM(ScrollSnap, stream << " => Computed vertical scroll snap offsets: " << verticalSnapOffsets);
        LOG_WITH_STREAM(ScrollSnap, stream << " => Computed vertical scroll snap offset ranges: " << verticalSnapOffsetRanges);
    }

    scrollableArea.setScrollSnapOffsetInfo({
        horizontalSnapOffsets,
        verticalSnapOffsets,
        horizontalSnapOffsetRanges,
        verticalSnapOffsetRanges
    });
}

static float convertOffsetUnit(LayoutUnit input, float deviceScaleFactor)
{
    return roundToDevicePixel(input, deviceScaleFactor, false);
}

static LayoutUnit convertOffsetUnit(float input, float /* scaleFactor */)
{
    return LayoutUnit(input);
}

template <typename InputType, typename OutputType>
static ScrollSnapOffsetsInfo<OutputType> convertOffsetInfo(const ScrollSnapOffsetsInfo<InputType>& input, float scaleFactor = 0.0)
{
    auto convertOffsets = [scaleFactor](const Vector<SnapOffset<InputType>>& input)
    {
        Vector<SnapOffset<OutputType>> output;
        output.reserveInitialCapacity(input.size());
        for (auto& offset : input)
            output.uncheckedAppend({ convertOffsetUnit(offset.offset, scaleFactor), offset.stop });
        return output;
    };

    auto convertOffsetRanges = [scaleFactor](const Vector<ScrollOffsetRange<InputType>>& input)
    {
        Vector<ScrollOffsetRange<OutputType>> output;
        output.reserveInitialCapacity(input.size());
        for (auto& range : input)
            output.uncheckedAppend({ convertOffsetUnit(range.start, scaleFactor), convertOffsetUnit(range.end, scaleFactor) });
        return output;
    };

    return {
        convertOffsets(input.horizontalSnapOffsets),
        convertOffsets(input.verticalSnapOffsets),
        convertOffsetRanges(input.horizontalSnapOffsetRanges),
        convertOffsetRanges(input.verticalSnapOffsetRanges)
    };
}

template <> template <>
ScrollSnapOffsetsInfo<LayoutUnit> ScrollSnapOffsetsInfo<float>::convertUnits(float /* unusedScaleFactor */) const
{
    return convertOffsetInfo<float, LayoutUnit>(*this);
}

template <> template <>
ScrollSnapOffsetsInfo<float> ScrollSnapOffsetsInfo<LayoutUnit>::convertUnits(float deviceScaleFactor) const
{
    return convertOffsetInfo<LayoutUnit, float>(*this, deviceScaleFactor);
}

template <>
std::pair<LayoutUnit, unsigned> ScrollSnapOffsetsInfo<LayoutUnit>::closestSnapOffset(ScrollEventAxis axis, LayoutUnit scrollDestinationOffset, float velocity, Optional<LayoutUnit> originalPositionForDirectionalSnapping) const
{
    return closestSnapOffsetWithOffsetsAndRanges(offsetsForAxis(axis), offsetRangesForAxis(axis), scrollDestinationOffset, velocity, originalPositionForDirectionalSnapping);
}

template <>
std::pair<float, unsigned> ScrollSnapOffsetsInfo<float>::closestSnapOffset(ScrollEventAxis axis, float scrollDestinationOffset, float velocity, Optional<float> originalPositionForDirectionalSnapping) const
{
    return closestSnapOffsetWithOffsetsAndRanges(offsetsForAxis(axis), offsetRangesForAxis(axis), scrollDestinationOffset, velocity, originalPositionForDirectionalSnapping);
}

}

#endif // ENABLE(CSS_SCROLL_SNAP)
