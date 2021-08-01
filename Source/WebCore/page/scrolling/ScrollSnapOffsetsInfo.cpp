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
struct PotentialSnapPointSearchResult {
    std::optional<std::pair<UnitType, unsigned>> previous;
    std::optional<std::pair<UnitType, unsigned>> next;
    std::optional<std::pair<UnitType, unsigned>> snapStop;
    bool landedInsideSnapAreaThatConsumesViewport;
};

template <typename InfoType, typename UnitType>
static PotentialSnapPointSearchResult<UnitType> searchForPotentialSnapPoints(const InfoType& info, ScrollEventAxis axis, UnitType viewportLength, UnitType destinationOffset, std::optional<UnitType> originalOffset)
{
    const auto& snapOffsets = info.offsetsForAxis(axis);
    std::optional<std::pair<UnitType, unsigned>> previous, next, exact, snapStop;
    bool landedInsideSnapAreaThatConsumesViewport = false;

    // A particular snap stop is better if it's between the original offset and destination offset and closer original
    // offset than the previously selected snap stop. We always want to stop at the snap stop closest to the original offset.
    auto isBetterSnapStop = [&](UnitType candidate) {
        if (!originalOffset)
            return false;
        auto original = *originalOffset;
        if (candidate <= std::min(destinationOffset, original) || candidate >= std::max(destinationOffset, original))
            return false;
        return !snapStop || std::abs(float { candidate - original }) < std::abs(float { (*snapStop).first - original });
    };

    for (unsigned i = 0; i < snapOffsets.size(); i++) {
        UnitType potentialSnapOffset = snapOffsets[i].offset;

        const auto& snapArea = info.snapAreas[snapOffsets[i].snapAreaIndex];
        auto snapAreaMin = axis == ScrollEventAxis::Horizontal ? snapArea.x() : snapArea.y();
        auto snapAreaMax = axis == ScrollEventAxis::Horizontal ? snapArea.maxX() : snapArea.maxY();
        landedInsideSnapAreaThatConsumesViewport |= snapAreaMin <= destinationOffset && snapAreaMax >= (destinationOffset + viewportLength);

        if (potentialSnapOffset == destinationOffset)
            exact = std::make_pair(potentialSnapOffset, i);
        else if (potentialSnapOffset < destinationOffset)
            previous = std::make_pair(potentialSnapOffset, i);
        else if (!next && potentialSnapOffset > destinationOffset)
            next = std::make_pair(potentialSnapOffset, i);

        if (snapOffsets[i].stop == ScrollSnapStop::Always && isBetterSnapStop(potentialSnapOffset))
            snapStop = std::make_pair(potentialSnapOffset, i);
    }

    if (exact)
        return { exact, exact, snapStop, landedInsideSnapAreaThatConsumesViewport };
    return { previous, next, snapStop, landedInsideSnapAreaThatConsumesViewport };
}

template <typename InfoType, typename SizeType, typename LayoutType, typename PointType>
static std::pair<LayoutType, std::optional<unsigned>> closestSnapOffsetWithInfoAndAxis(const InfoType& info, ScrollEventAxis axis, const SizeType& viewportSize, PointType scrollDestinationOffsetPoint, float velocity, std::optional<LayoutType> originalOffsetForDirectionalSnapping)
{
    auto scrollDestinationOffset = axis == ScrollEventAxis::Horizontal ? scrollDestinationOffsetPoint.x() : scrollDestinationOffsetPoint.y();
    const auto& snapOffsets = info.offsetsForAxis(axis);
    auto pairForNoSnapping = std::make_pair(scrollDestinationOffset, std::nullopt);
    if (snapOffsets.isEmpty())
        return pairForNoSnapping;

    auto viewportLength = axis == ScrollEventAxis::Horizontal ? viewportSize.width() : viewportSize.height();
    auto [previous, next, snapStop, landedInsideSnapAreaThatConsumesViewport] = searchForPotentialSnapPoints(info, axis, viewportLength, scrollDestinationOffset, originalOffsetForDirectionalSnapping);
    if (snapStop)
        return *snapStop;

    // From https://www.w3.org/TR/css-scroll-snap-1/#snap-overflow
    // "If the snap area is larger than the snapport in a particular axis, then any scroll position
    // in which the snap area covers the snapport, and the distance between the geometrically
    // previous and subsequent snap positions in that axis is larger than size of the snapport in
    // that axis, is a valid snap position in that axis. The UA may use the specified alignment as a
    // more precise target for certain scroll operations (e.g. explicit paging)."
    if (landedInsideSnapAreaThatConsumesViewport && (!previous || !next || ((*next).first - (*previous).first) >= viewportLength))
        return pairForNoSnapping;

    auto isNearEnoughToOffsetForProximity = [&](LayoutType candidateSnapOffset) {
        if (info.strictness != ScrollSnapStrictness::Proximity)
            return true;

        // This is an arbitrary choice for what it means to be "in proximity" of a snap offset. We should play around with
        // this and see what feels best.
        static const float ratioOfScrollPortAxisLengthToBeConsideredForProximity = 0.3;
        return std::abs(float {candidateSnapOffset - scrollDestinationOffset}) <= (viewportLength * ratioOfScrollPortAxisLengthToBeConsideredForProximity);
    };

    if (scrollDestinationOffset <= snapOffsets.first().offset)
        return isNearEnoughToOffsetForProximity(snapOffsets.first().offset) ? std::make_pair(snapOffsets.first().offset, std::make_optional(0u)) : pairForNoSnapping;

    if (scrollDestinationOffset >= snapOffsets.last().offset) {
        unsigned lastIndex = static_cast<unsigned>(snapOffsets.size() - 1);
        return isNearEnoughToOffsetForProximity(snapOffsets.last().offset) ? std::make_pair(snapOffsets.last().offset, std::make_optional(lastIndex)) : pairForNoSnapping;
    }

    if (previous && !isNearEnoughToOffsetForProximity((*previous).first))
        previous.reset();
    if (next && !isNearEnoughToOffsetForProximity((*next).first))
        next.reset();

    if (originalOffsetForDirectionalSnapping) {
        // From https://www.w3.org/TR/css-scroll-snap-1/#choosing
        // "User agents must ensure that a user can “escape” a snap position, regardless of the scroll
        // method. For example, if the snap type is mandatory and the next snap position is more than
        // two screen-widths away, a naïve “always snap to nearest” selection algorithm might “trap” the
        //
        // For a directional scroll, we never snap back to the original scroll position or before it,
        // always preferring the snap offset in the scroll direction.
        auto& originalOffset = *originalOffsetForDirectionalSnapping;
        if (originalOffset < scrollDestinationOffset && previous && (*previous).first <= originalOffset)
            previous.reset();
        if (originalOffset > scrollDestinationOffset && next && (*next).first >= originalOffset)
            next.reset();
    }

    if (!previous && !next)
        return pairForNoSnapping;
    if (!previous)
        return *next;
    if (!next)
        return *previous;

    // If this scroll isn't directional, then choose whatever snap point is closer, otherwise pick the offset in the scroll direction.
    if (!std::abs(velocity))
        return (scrollDestinationOffset - (*previous).first) <= ((*next).first - scrollDestinationOffset) ? *previous : *next;
    return velocity < 0 ? *previous : *next;
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

static std::pair<bool, bool> axesFlippedForWritingModeAndDirection(WritingMode writingMode, TextDirection textDirection)
{
    // text-direction flips the inline axis and writing-mode can flip the block axis. Whether or
    // not the writing-mode is vertical determines the physical orientation of the block and inline axes.
    bool hasVerticalWritingMode = isVerticalWritingMode(writingMode);
    bool blockAxisFlipped = isFlippedWritingMode(writingMode);
    bool inlineAxisFlipped = textDirection == TextDirection::RTL;
    return std::make_pair(hasVerticalWritingMode ? blockAxisFlipped : inlineAxisFlipped, hasVerticalWritingMode ? inlineAxisFlipped : blockAxisFlipped);
}

void updateSnapOffsetsForScrollableArea(ScrollableArea& scrollableArea, const RenderBox& scrollingElementBox, const RenderStyle& scrollingElementStyle, LayoutRect viewportRectInBorderBoxCoordinates, WritingMode writingMode, TextDirection textDirection)
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

    auto maxScrollOffset = scrollableArea.maximumScrollOffset();
    auto scrollPosition = LayoutPoint { scrollableArea.scrollPosition() };

    auto [scrollerXAxisFlipped, scrollerYAxisFlipped] = axesFlippedForWritingModeAndDirection(writingMode, textDirection);
    bool scrollerHasVerticalWritingMode = isVerticalWritingMode(writingMode);
    bool hasHorizontalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::XAxis;
    bool hasVerticalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::YAxis;
    if (scrollSnapType.axis == ScrollSnapAxis::Block) {
        hasHorizontalSnapOffsets = scrollerHasVerticalWritingMode;
        hasVerticalSnapOffsets = !scrollerHasVerticalWritingMode;
    }
    if (scrollSnapType.axis == ScrollSnapAxis::Inline) {
        hasHorizontalSnapOffsets = !scrollerHasVerticalWritingMode;
        hasVerticalSnapOffsets = scrollerHasVerticalWritingMode;
    }

    // The bounds of the scrolling container's snap port, where the top left of the scrolling container's border box is the origin.
    auto scrollSnapPort = computeScrollSnapPortOrAreaRect(viewportRectInBorderBoxCoordinates, scrollingElementStyle.scrollPadding(), InsetOrOutset::Inset);
    LOG_WITH_STREAM(ScrollSnap, stream << "Computing scroll snap offsets for " << scrollableArea << " in snap port " << scrollSnapPort);
    for (auto* child : boxesWithScrollSnapPositions) {
        if (child->enclosingScrollableContainerForSnapping() != &scrollingElementBox)
            continue;

        // The bounds of the child element's snap area, where the top left of the scrolling container's border box is the origin.
        // The snap area is the bounding box of the child element's border box, after applying transformations.
        auto scrollSnapArea = LayoutRect(child->localToContainerQuad(FloatQuad(child->borderBoundingBox()), &scrollingElementBox).boundingBox());

        // localToContainerQuad will transform the scroll snap area by the scroll position, except in the case that this position is
        // coming from a ScrollView. We want the transformed area, but without scroll position taken into account.
        if (!scrollableArea.isScrollView())
            scrollSnapArea.moveBy(scrollPosition);

        scrollSnapArea = computeScrollSnapPortOrAreaRect(scrollSnapArea, child->style().scrollMargin(), InsetOrOutset::Outset);
        LOG_WITH_STREAM(ScrollSnap, stream << "    Considering scroll snap target area " << scrollSnapArea);
        auto alignment = child->style().scrollSnapAlign();
        auto stop = child->style().scrollSnapStop();

        // From https://drafts.csswg.org/css-scroll-snap-1/#scroll-snap-align:
        // "Start and end alignments are resolved with respect to the writing mode of the snap container unless the
        // scroll snap area is larger than the snapport, in which case they are resolved with respect to the writing
        // mode of the box itself."
        bool areaXAxisFlipped = scrollerXAxisFlipped;
        bool areaYAxisFlipped = scrollerYAxisFlipped;
        bool areaHasVerticalWritingMode = isVerticalWritingMode(child->style().writingMode());
        if ((areaHasVerticalWritingMode && scrollSnapArea.height() > scrollSnapPort.height()) || (!areaHasVerticalWritingMode && scrollSnapArea.width() > scrollSnapPort.width()))
            std::tie(areaXAxisFlipped, areaYAxisFlipped) = axesFlippedForWritingModeAndDirection(child->style().writingMode(), child->style().direction());

        ScrollSnapAxisAlignType xAlign = scrollerHasVerticalWritingMode ? alignment.blockAlign : alignment.inlineAlign;
        ScrollSnapAxisAlignType yAlign = scrollerHasVerticalWritingMode ? alignment.inlineAlign : alignment.blockAlign;
        bool snapsHorizontally = hasHorizontalSnapOffsets && xAlign != ScrollSnapAxisAlignType::None;
        bool snapsVertically = hasVerticalSnapOffsets && yAlign != ScrollSnapAxisAlignType::None;

        if (!snapsHorizontally && !snapsVertically)
            continue;
        // The scroll snap area is defined via its scroll position, so convert the snap area rectangle to be relative to scroll offsets.
        auto snapAreaOriginRelativeToBorderEdge = scrollSnapArea.location() - scrollSnapPort.location();
        LayoutRect scrollSnapAreaAsOffsets(scrollableArea.scrollOffsetFromPosition(roundedIntPoint(snapAreaOriginRelativeToBorderEdge)), scrollSnapArea.size());
        snapAreas.append(scrollSnapAreaAsOffsets);

        if (snapsHorizontally) {
            auto absoluteScrollXPosition = computeScrollSnapAlignOffset(scrollSnapArea.x(), scrollSnapArea.maxX(), xAlign, areaXAxisFlipped) - computeScrollSnapAlignOffset(scrollSnapPort.x(), scrollSnapPort.maxX(), xAlign, areaXAxisFlipped);
            auto absoluteScrollOffset = clampTo<int>(scrollableArea.scrollOffsetFromPosition({ roundToInt(absoluteScrollXPosition), 0 }).x(), 0, maxScrollOffset.x());
            addOrUpdateStopForSnapOffset(horizontalSnapOffsetsMap, { absoluteScrollOffset, stop, snapAreas.size() - 1 });
        }
        if (snapsVertically) {
            auto absoluteScrollYPosition = computeScrollSnapAlignOffset(scrollSnapArea.y(), scrollSnapArea.maxY(), yAlign, areaYAxisFlipped) - computeScrollSnapAlignOffset(scrollSnapPort.y(), scrollSnapPort.maxY(), yAlign, areaYAxisFlipped);
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
std::pair<LayoutUnit, std::optional<unsigned>> LayoutScrollSnapOffsetsInfo::closestSnapOffset(ScrollEventAxis axis, const LayoutSize& viewportSize, LayoutPoint scrollDestinationOffset, float velocity, std::optional<LayoutUnit> originalPositionForDirectionalSnapping) const
{
    return closestSnapOffsetWithInfoAndAxis(*this, axis, viewportSize, scrollDestinationOffset, velocity, originalPositionForDirectionalSnapping);
}

template <> template<>
std::pair<float, std::optional<unsigned>> FloatScrollSnapOffsetsInfo::closestSnapOffset(ScrollEventAxis axis, const FloatSize& viewportSize, FloatPoint scrollDestinationOffset, float velocity, std::optional<float> originalPositionForDirectionalSnapping) const
{
    return closestSnapOffsetWithInfoAndAxis(*this, axis, viewportSize, scrollDestinationOffset, velocity, originalPositionForDirectionalSnapping);
}

}
