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
#include "AxisScrollSnapOffsets.h"

#include "ElementChildIterator.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "Length.h"
#include "Logging.h"
#include "RenderBox.h"
#include "RenderView.h"
#include "ScrollableArea.h"
#include "StyleScrollSnapPoints.h"
#include <wtf/text/StringConcatenateNumbers.h>

#if ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

enum class InsetOrOutset {
    Inset,
    Outset
};

static LayoutRect computeScrollSnapPortOrAreaRect(const LayoutRect& rect, const LengthBox& insetOrOutsetBox, InsetOrOutset insetOrOutset)
{
    LayoutBoxExtent extents(valueForLength(insetOrOutsetBox.top(), rect.height()), valueForLength(insetOrOutsetBox.right(), rect.width()), valueForLength(insetOrOutsetBox.bottom(), rect.height()), valueForLength(insetOrOutsetBox.left(), rect.width()));
    auto snapPortOrArea(rect);
    if (insetOrOutset == InsetOrOutset::Inset)
        snapPortOrArea.contract(extents);
    else
        snapPortOrArea.expand(extents);
    return snapPortOrArea;
}

static LayoutUnit computeScrollSnapAlignOffset(const LayoutUnit& leftOrTop, const LayoutUnit& widthOrHeight, ScrollSnapAxisAlignType alignment)
{
    switch (alignment) {
    case ScrollSnapAxisAlignType::Start:
        return leftOrTop;
    case ScrollSnapAxisAlignType::Center:
        return leftOrTop + widthOrHeight / 2;
    case ScrollSnapAxisAlignType::End:
        return leftOrTop + widthOrHeight;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

#if !LOG_DISABLED

static String snapOffsetsToString(const Vector<LayoutUnit>& snapOffsets)
{
    StringBuilder builder;
    builder.appendLiteral("[ ");
    for (auto& offset : snapOffsets) {
        builder.appendFixedWidthNumber(offset.toFloat(), 1);
        builder.append(' ');
    }
    builder.append(']');
    return builder.toString();
}

static String snapOffsetRangesToString(const Vector<ScrollOffsetRange<LayoutUnit>>& ranges)
{
    StringBuilder builder;
    builder.appendLiteral("[ ");
    for (auto& range : ranges) {
        builder.append('(');
        builder.appendFixedWidthNumber(range.start.toFloat(), 1);
        builder.appendLiteral(", ");
        builder.appendFixedWidthNumber(range.end.toFloat(), 1);
        builder.appendLiteral(") ");
    }
    builder.append(']');
    return builder.toString();
}

static String snapPortOrAreaToString(const LayoutRect& rect)
{
    return makeString("{{",
        FormattedNumber::fixedWidth(rect.x(), 1), ", ",
        FormattedNumber::fixedWidth(rect.y(), 1), "} {",
        FormattedNumber::fixedWidth(rect.width(), 1), ", ",
        FormattedNumber::fixedWidth(rect.height(), 1), "}}");
}

#endif

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
static void indicesOfNearestSnapOffsets(LayoutType offset, const Vector<LayoutType>& snapOffsets, unsigned& lowerIndex, unsigned& upperIndex)
{
    lowerIndex = 0;
    upperIndex = snapOffsets.size() - 1;
    while (lowerIndex < upperIndex - 1) {
        int middleIndex = (lowerIndex + upperIndex) / 2;
        auto middleOffset = snapOffsets[middleIndex];
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

static void adjustAxisSnapOffsetsForScrollExtent(Vector<LayoutUnit>& snapOffsets, float maxScrollExtent)
{
    if (snapOffsets.isEmpty())
        return;

    std::sort(snapOffsets.begin(), snapOffsets.end());
    if (snapOffsets.last() != maxScrollExtent)
        snapOffsets.append(maxScrollExtent);
    if (snapOffsets.first())
        snapOffsets.insert(0, 0);
}

static void computeAxisProximitySnapOffsetRanges(const Vector<LayoutUnit>& snapOffsets, Vector<ScrollOffsetRange<LayoutUnit>>& offsetRanges, LayoutUnit scrollPortAxisLength)
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
        auto startOffset = snapOffsets[index - 1] + proximityDistance;
        auto endOffset = snapOffsets[index] - proximityDistance;
        if (startOffset < endOffset)
            offsetRanges.append({ startOffset, endOffset });
    }
}

void updateSnapOffsetsForScrollableArea(ScrollableArea& scrollableArea, HTMLElement& scrollingElement, const RenderBox& scrollingElementBox, const RenderStyle& scrollingElementStyle)
{
    auto* scrollContainer = scrollingElement.renderer();
    auto scrollSnapType = scrollingElementStyle.scrollSnapType();
    if (!scrollContainer || scrollSnapType.strictness == ScrollSnapStrictness::None || scrollContainer->view().boxesWithScrollSnapPositions().isEmpty()) {
        scrollableArea.clearHorizontalSnapOffsets();
        scrollableArea.clearVerticalSnapOffsets();
        return;
    }

    Vector<LayoutUnit> verticalSnapOffsets;
    Vector<LayoutUnit> horizontalSnapOffsets;
    Vector<ScrollOffsetRange<LayoutUnit>> verticalSnapOffsetRanges;
    Vector<ScrollOffsetRange<LayoutUnit>> horizontalSnapOffsetRanges;
    HashSet<float> seenVerticalSnapOffsets;
    HashSet<float> seenHorizontalSnapOffsets;
    bool hasHorizontalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::XAxis || scrollSnapType.axis == ScrollSnapAxis::Inline;
    bool hasVerticalSnapOffsets = scrollSnapType.axis == ScrollSnapAxis::Both || scrollSnapType.axis == ScrollSnapAxis::YAxis || scrollSnapType.axis == ScrollSnapAxis::Block;
    auto maxScrollLeft = scrollingElementBox.scrollWidth() - scrollingElementBox.contentWidth();
    auto maxScrollTop = scrollingElementBox.scrollHeight() - scrollingElementBox.contentHeight();
    LayoutPoint containerScrollOffset(scrollingElementBox.scrollLeft(), scrollingElementBox.scrollTop());

    // The bounds of the scrolling container's snap port, where the top left of the scrolling container's border box is the origin.
    auto scrollSnapPort = computeScrollSnapPortOrAreaRect(scrollingElementBox.paddingBoxRect(), scrollingElementStyle.scrollPadding(), InsetOrOutset::Inset);
#if !LOG_DISABLED
    LOG(Scrolling, "Computing scroll snap offsets in snap port: %s", snapPortOrAreaToString(scrollSnapPort).utf8().data());
#endif
    for (auto* child : scrollContainer->view().boxesWithScrollSnapPositions()) {
        if (child->enclosingScrollableContainerForSnapping() != scrollContainer)
            continue;

        // The bounds of the child element's snap area, where the top left of the scrolling container's border box is the origin.
        // The snap area is the bounding box of the child element's border box, after applying transformations.
        auto scrollSnapArea = LayoutRect(child->localToContainerQuad(FloatQuad(child->borderBoundingBox()), scrollingElement.renderBox()).boundingBox());
        scrollSnapArea.moveBy(containerScrollOffset);
        scrollSnapArea = computeScrollSnapPortOrAreaRect(scrollSnapArea, child->style().scrollSnapMargin(), InsetOrOutset::Outset);
#if !LOG_DISABLED
        LOG(Scrolling, "    Considering scroll snap area: %s", snapPortOrAreaToString(scrollSnapArea).utf8().data());
#endif
        auto alignment = child->style().scrollSnapAlign();
        if (hasHorizontalSnapOffsets && alignment.x != ScrollSnapAxisAlignType::None) {
            auto absoluteScrollOffset = clampTo<LayoutUnit>(computeScrollSnapAlignOffset(scrollSnapArea.x(), scrollSnapArea.width(), alignment.x) - computeScrollSnapAlignOffset(scrollSnapPort.x(), scrollSnapPort.width(), alignment.x), 0, maxScrollLeft);
            if (!seenHorizontalSnapOffsets.contains(absoluteScrollOffset)) {
                seenHorizontalSnapOffsets.add(absoluteScrollOffset);
                horizontalSnapOffsets.append(absoluteScrollOffset);
            }
        }
        if (hasVerticalSnapOffsets && alignment.y != ScrollSnapAxisAlignType::None) {
            auto absoluteScrollOffset = clampTo<LayoutUnit>(computeScrollSnapAlignOffset(scrollSnapArea.y(), scrollSnapArea.height(), alignment.y) - computeScrollSnapAlignOffset(scrollSnapPort.y(), scrollSnapPort.height(), alignment.y), 0, maxScrollTop);
            if (!seenVerticalSnapOffsets.contains(absoluteScrollOffset)) {
                seenVerticalSnapOffsets.add(absoluteScrollOffset);
                verticalSnapOffsets.append(absoluteScrollOffset);
            }
        }
    }

    if (!horizontalSnapOffsets.isEmpty()) {
        adjustAxisSnapOffsetsForScrollExtent(horizontalSnapOffsets, maxScrollLeft);
#if !LOG_DISABLED
        LOG(Scrolling, " => Computed horizontal scroll snap offsets: %s", snapOffsetsToString(horizontalSnapOffsets).utf8().data());
        LOG(Scrolling, " => Computed horizontal scroll snap offset ranges: %s", snapOffsetRangesToString(horizontalSnapOffsetRanges).utf8().data());
#endif
        if (scrollSnapType.strictness == ScrollSnapStrictness::Proximity)
            computeAxisProximitySnapOffsetRanges(horizontalSnapOffsets, horizontalSnapOffsetRanges, scrollSnapPort.width());

        scrollableArea.setHorizontalSnapOffsets(horizontalSnapOffsets);
        scrollableArea.setHorizontalSnapOffsetRanges(horizontalSnapOffsetRanges);
    } else
        scrollableArea.clearHorizontalSnapOffsets();

    if (!verticalSnapOffsets.isEmpty()) {
        adjustAxisSnapOffsetsForScrollExtent(verticalSnapOffsets, maxScrollTop);
#if !LOG_DISABLED
        LOG(Scrolling, " => Computed vertical scroll snap offsets: %s", snapOffsetsToString(verticalSnapOffsets).utf8().data());
        LOG(Scrolling, " => Computed vertical scroll snap offset ranges: %s", snapOffsetRangesToString(verticalSnapOffsetRanges).utf8().data());
#endif
        if (scrollSnapType.strictness == ScrollSnapStrictness::Proximity)
            computeAxisProximitySnapOffsetRanges(verticalSnapOffsets, verticalSnapOffsetRanges, scrollSnapPort.height());

        scrollableArea.setVerticalSnapOffsets(verticalSnapOffsets);
        scrollableArea.setVerticalSnapOffsetRanges(verticalSnapOffsetRanges);
    } else
        scrollableArea.clearVerticalSnapOffsets();
}

template <typename LayoutType>
LayoutType closestSnapOffset(const Vector<LayoutType>& snapOffsets, const Vector<ScrollOffsetRange<LayoutType>>& snapOffsetRanges, LayoutType scrollDestination, float velocity, unsigned& activeSnapIndex)
{
    ASSERT(snapOffsets.size());
    activeSnapIndex = 0;

    unsigned lowerSnapOffsetRangeIndex;
    unsigned upperSnapOffsetRangeIndex;
    indicesOfNearestSnapOffsetRanges<LayoutType>(scrollDestination, snapOffsetRanges, lowerSnapOffsetRangeIndex, upperSnapOffsetRangeIndex);
    if (lowerSnapOffsetRangeIndex == upperSnapOffsetRangeIndex && upperSnapOffsetRangeIndex != invalidSnapOffsetIndex) {
        activeSnapIndex = invalidSnapOffsetIndex;
        return scrollDestination;
    }

    if (scrollDestination <= snapOffsets.first())
        return snapOffsets.first();

    activeSnapIndex = snapOffsets.size() - 1;
    if (scrollDestination >= snapOffsets.last())
        return snapOffsets.last();

    unsigned lowerIndex;
    unsigned upperIndex;
    indicesOfNearestSnapOffsets<LayoutType>(scrollDestination, snapOffsets, lowerIndex, upperIndex);
    LayoutType lowerSnapPosition = snapOffsets[lowerIndex];
    LayoutType upperSnapPosition = snapOffsets[upperIndex];
    if (!std::abs(velocity)) {
        bool isCloserToLowerSnapPosition = scrollDestination - lowerSnapPosition <= upperSnapPosition - scrollDestination;
        activeSnapIndex = isCloserToLowerSnapPosition ? lowerIndex : upperIndex;
        return isCloserToLowerSnapPosition ? lowerSnapPosition : upperSnapPosition;
    }

    // Non-zero velocity indicates a flick gesture. Even if another snap point is closer, we should choose the one in the direction of the flick gesture
    // as long as a scroll snap offset range does not lie between the scroll destination and the targeted snap offset.
    if (velocity < 0) {
        if (lowerSnapOffsetRangeIndex != invalidSnapOffsetIndex && lowerSnapPosition < snapOffsetRanges[lowerSnapOffsetRangeIndex].end) {
            activeSnapIndex = upperIndex;
            return upperSnapPosition;
        }
        activeSnapIndex = lowerIndex;
        return lowerSnapPosition;
    }

    if (upperSnapOffsetRangeIndex != invalidSnapOffsetIndex && snapOffsetRanges[upperSnapOffsetRangeIndex].start < upperSnapPosition) {
        activeSnapIndex = lowerIndex;
        return lowerSnapPosition;
    }
    activeSnapIndex = upperIndex;
    return upperSnapPosition;
}

LayoutUnit closestSnapOffset(const Vector<LayoutUnit>& snapOffsets, const Vector<ScrollOffsetRange<LayoutUnit>>& snapOffsetRanges, LayoutUnit scrollDestination, float velocity, unsigned& activeSnapIndex)
{
    return closestSnapOffset<LayoutUnit>(snapOffsets, snapOffsetRanges, scrollDestination, velocity, activeSnapIndex);
}

float closestSnapOffset(const Vector<float>& snapOffsets, const Vector<ScrollOffsetRange<float>>& snapOffsetRanges, float scrollDestination, float velocity, unsigned& activeSnapIndex)
{
    return closestSnapOffset<float>(snapOffsets, snapOffsetRanges, scrollDestination, velocity, activeSnapIndex);
}

} // namespace WebCore

#endif // CSS_SCROLL_SNAP
