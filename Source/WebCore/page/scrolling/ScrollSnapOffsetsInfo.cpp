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

template <typename UnitType>
static bool isNearEnoughToOffsetForProximity(ScrollSnapStrictness strictness, UnitType scrollDestination, UnitType candidateSnapOffset, UnitType viewportLength)
{
    if (strictness != ScrollSnapStrictness::Proximity)
        return true;

    // This is an arbitrary choice for what it means to be "in proximity" of a snap offset. We should play around with
    // this and see what feels best.
    static const float ratioOfScrollPortAxisLengthToBeConsideredForProximity = 0.3;
    return std::abs(float {candidateSnapOffset - scrollDestination}) <= (viewportLength * ratioOfScrollPortAxisLengthToBeConsideredForProximity);
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
        return std::nullopt;

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

    return std::nullopt;
}

template <typename InfoType, typename SizeType, typename LayoutType>
static std::pair<LayoutType, unsigned> closestSnapOffsetWithInfoAndAxis(const InfoType& info, ScrollEventAxis axis, const SizeType& viewportSize, LayoutType scrollDestinationOffset, float velocity, std::optional<LayoutType> originalOffsetForDirectionalSnapping)
{
    const auto& snapOffsets = info.offsetsForAxis(axis);
    if (snapOffsets.isEmpty())
        return std::make_pair(scrollDestinationOffset, invalidSnapOffsetIndex);

    if (originalOffsetForDirectionalSnapping) {
        if (auto firstSnapStopOffsetIndex = findFirstSnapStopOffsetBetweenOriginAndDestination(snapOffsets, *originalOffsetForDirectionalSnapping, scrollDestinationOffset))
            return std::make_pair(snapOffsets[*firstSnapStopOffsetIndex].offset, *firstSnapStopOffsetIndex);
    }

    if (scrollDestinationOffset <= snapOffsets.first().offset)
        return std::make_pair(snapOffsets.first().offset, 0u);

    if (scrollDestinationOffset >= snapOffsets.last().offset)
        return std::make_pair(snapOffsets.last().offset, snapOffsets.size() - 1);

    unsigned lowerIndex;
    unsigned upperIndex;
    indicesOfNearestSnapOffsets<LayoutType>(scrollDestinationOffset, snapOffsets, lowerIndex, upperIndex);
    LayoutType lowerSnapPosition = snapOffsets[lowerIndex].offset;
    LayoutType upperSnapPosition = snapOffsets[upperIndex].offset;

    auto viewportLength = axis == ScrollEventAxis::Horizontal ? viewportSize.width() : viewportSize.height();
    if (!isNearEnoughToOffsetForProximity<LayoutType>(info.strictness, scrollDestinationOffset, lowerSnapPosition, viewportLength)) {
        lowerSnapPosition = scrollDestinationOffset;
        lowerIndex = invalidSnapOffsetIndex;
    }

    if (!isNearEnoughToOffsetForProximity<LayoutType>(info.strictness, scrollDestinationOffset, upperSnapPosition, viewportLength)) {
        upperSnapPosition = scrollDestinationOffset;
        upperIndex = invalidSnapOffsetIndex;
    }
    if (!std::abs(velocity)) {
        bool isCloserToLowerSnapPosition = (upperIndex == invalidSnapOffsetIndex)
            || (lowerIndex != invalidSnapOffsetIndex && scrollDestinationOffset - lowerSnapPosition <= upperSnapPosition - scrollDestinationOffset);
        return isCloserToLowerSnapPosition ? std::make_pair(lowerSnapPosition, lowerIndex) : std::make_pair(upperSnapPosition, upperIndex);
    }

    // Non-zero velocity indicates a flick gesture. Even if another snap point is closer, we should choose the one in the direction of the flick gesture
    // as long as a scroll snap offset is close enough for proximity (or we aren't using proximity). If we are doing directional
    // snapping, we should never snap to a point that was on the other side of the original position in the opposite direction of this scroll.
    // This allows directional scrolling to escape snap points.
    if (velocity < 0)  {
        if (upperIndex != invalidSnapOffsetIndex && (!originalOffsetForDirectionalSnapping || *originalOffsetForDirectionalSnapping > upperSnapPosition))
            return std::make_pair(upperSnapPosition, upperIndex);
        return std::make_pair(lowerSnapPosition, lowerIndex);
    }

    if (lowerIndex != invalidSnapOffsetIndex && (!originalOffsetForDirectionalSnapping || *originalOffsetForDirectionalSnapping < lowerSnapPosition))
        return std::make_pair(lowerSnapPosition, lowerIndex);
    return std::make_pair(upperSnapPosition, upperIndex);
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
    Vector<LayoutRect> snapAreas;
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

        bool snapsHorizontally = hasHorizontalSnapOffsets && alignment.x != ScrollSnapAxisAlignType::None;
        bool snapsVertically = hasVerticalSnapOffsets && alignment.y != ScrollSnapAxisAlignType::None;
        if (!snapsHorizontally && !snapsVertically)
            continue;

        // The scroll snap area is defined via its scroll position, so convert the snap area rectangle to be relative to scroll offsets.
        auto snapAreaOriginRelativeToBorderEdge = scrollSnapArea.location() - scrollSnapPort.location();
        LayoutRect scrollSnapAreaAsOffsets(scrollableArea.scrollOffsetFromPosition(roundedIntPoint(snapAreaOriginRelativeToBorderEdge)), scrollSnapArea.size());
        snapAreas.append(scrollSnapAreaAsOffsets);

        if (snapsHorizontally) {
            auto absoluteScrollXPosition = computeScrollSnapAlignOffset(scrollSnapArea.x(), scrollSnapArea.maxX(), alignment.x, scrollerIsRTL) - computeScrollSnapAlignOffset(scrollSnapPort.x(), scrollSnapPort.maxX(), alignment.x, scrollerIsRTL);
            auto absoluteScrollOffset = clampTo<int>(scrollableArea.scrollOffsetFromPosition({ roundToInt(absoluteScrollXPosition), 0 }).x(), 0, maxScrollOffset.x());
            addOrUpdateStopForSnapOffset(horizontalSnapOffsetsMap, { absoluteScrollOffset, stop, snapAreas.size() - 1 });
        }
        if (snapsVertically) {
            auto absoluteScrollYPosition = computeScrollSnapAlignOffset(scrollSnapArea.y(), scrollSnapArea.maxY(), alignment.y, false) - computeScrollSnapAlignOffset(scrollSnapPort.y(), scrollSnapPort.maxY(), alignment.y, false);
            auto absoluteScrollOffset = clampTo<int>(scrollableArea.scrollOffsetFromPosition({ 0, roundToInt(absoluteScrollYPosition) }).y(), 0, maxScrollOffset.y());
            addOrUpdateStopForSnapOffset(verticalSnapOffsetsMap, { absoluteScrollOffset, stop, snapAreas.size() - 1 });
        }

        if (!snapAreas.isEmpty())
            LOG_WITH_STREAM(ScrollSnap, stream << " => Computed snap areas: " << snapAreas);
    }

    auto compareSnapOffsets = [](const SnapOffset<LayoutUnit>& a, const SnapOffset<LayoutUnit>& b)
    {
        return a.offset < b.offset;
    };

    Vector<SnapOffset<LayoutUnit>> horizontalSnapOffsets = copyToVector(horizontalSnapOffsetsMap.values());
    if (!horizontalSnapOffsets.isEmpty()) {
        std::sort(horizontalSnapOffsets.begin(), horizontalSnapOffsets.end(), compareSnapOffsets);
        LOG_WITH_STREAM(ScrollSnap, stream << " => Computed horizontal scroll snap offsets: " << horizontalSnapOffsets);
    }

    Vector<SnapOffset<LayoutUnit>> verticalSnapOffsets = copyToVector(verticalSnapOffsetsMap.values());
    if (!verticalSnapOffsets.isEmpty()) {
        std::sort(verticalSnapOffsets.begin(), verticalSnapOffsets.end(), compareSnapOffsets);
        LOG_WITH_STREAM(ScrollSnap, stream << " => Computed vertical scroll snap offsets: " << verticalSnapOffsets);
    }

    scrollableArea.setScrollSnapOffsetInfo({
        scrollSnapType.strictness,
        horizontalSnapOffsets,
        verticalSnapOffsets,
        snapAreas
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

template <typename InputType, typename InputRectType, typename OutputType, typename OutputRectType>
static ScrollSnapOffsetsInfo<OutputType, OutputRectType> convertOffsetInfo(const ScrollSnapOffsetsInfo<InputType, InputRectType>& input, float scaleFactor = 0.0)
{
    auto convertOffsets = [scaleFactor](const Vector<SnapOffset<InputType>>& input)
    {
        Vector<SnapOffset<OutputType>> output;
        output.reserveInitialCapacity(input.size());
        for (auto& offset : input)
            output.uncheckedAppend({ convertOffsetUnit(offset.offset, scaleFactor), offset.stop, offset.snapAreaIndex });
        return output;
    };

    auto convertRects = [scaleFactor](const Vector<InputRectType>& input)
    {
        Vector<OutputRectType> output;
        output.reserveInitialCapacity(input.size());
        for (auto& rect : input) {
            OutputRectType outputRect(
                convertOffsetUnit(rect.x(), scaleFactor), convertOffsetUnit(rect.y(), scaleFactor),
                convertOffsetUnit(rect.width(), scaleFactor), convertOffsetUnit(rect.height(), scaleFactor));
            output.uncheckedAppend(outputRect);
        }

        return output;
    };

    return {
        input.strictness,
        convertOffsets(input.horizontalSnapOffsets),
        convertOffsets(input.verticalSnapOffsets),
        convertRects(input.snapAreas),
    };
}

template <> template <>
LayoutScrollSnapOffsetsInfo FloatScrollSnapOffsetsInfo::convertUnits(float /* unusedScaleFactor */) const
{
    return convertOffsetInfo<float, FloatRect, LayoutUnit, LayoutRect>(*this);

}

template <> template <>
FloatScrollSnapOffsetsInfo LayoutScrollSnapOffsetsInfo::convertUnits(float deviceScaleFactor) const
{
    return convertOffsetInfo<LayoutUnit, LayoutRect, float, FloatRect>(*this, deviceScaleFactor);

}

template <> template <>
std::pair<LayoutUnit, unsigned> LayoutScrollSnapOffsetsInfo::closestSnapOffset(ScrollEventAxis axis, const LayoutSize& viewportSize, LayoutUnit scrollDestinationOffset, float velocity, std::optional<LayoutUnit> originalPositionForDirectionalSnapping) const
{
    return closestSnapOffsetWithInfoAndAxis(*this, axis, viewportSize, scrollDestinationOffset, velocity, originalPositionForDirectionalSnapping);
}

template <> template<>
std::pair<float, unsigned> FloatScrollSnapOffsetsInfo::closestSnapOffset(ScrollEventAxis axis, const FloatSize& viewportSize, float scrollDestinationOffset, float velocity, std::optional<float> originalPositionForDirectionalSnapping) const
{
    return closestSnapOffsetWithInfoAndAxis(*this, axis, viewportSize, scrollDestinationOffset, velocity, originalPositionForDirectionalSnapping);
}

}

#endif // ENABLE(CSS_SCROLL_SNAP)
