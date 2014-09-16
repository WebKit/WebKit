/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "RenderBox.h"
#include "ScrollableArea.h"
#include "StyleScrollSnapPoints.h"

#if ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

static void appendChildSnapOffsets(HTMLElement& parent, bool shouldAddHorizontalChildOffsets, Vector<LayoutUnit>& horizontalSnapOffsetSubsequence, bool shouldAddVerticalChildOffsets, Vector<LayoutUnit>& verticalSnapOffsetSubsequence)
{
    Element* child = parent.children()->collectionBegin();
    // FIXME: Instead of traversing all children, register children with snap coordinates before appending to snapOffsetSubsequence.
    while (child) {
        if (RenderBox* box = child->renderBox()) {
            LayoutUnit viewWidth = box->width();
            LayoutUnit viewHeight = box->height();
#if PLATFORM(IOS)
            // FIXME: Investigate why using localToContainerPoint gives the wrong offsets for iOS mainframe. Also, these offsets won't take transforms into account (make sure to test this!)
            float left = child->offsetLeft();
            float top = child->offsetTop();
#else
            // FIXME: Check that localToContainerPoint works with CSS rotations.
            FloatPoint position = box->localToContainerPoint(FloatPoint(), parent.renderBox());
            float left = position.x();
            float top = position.y();
#endif
            for (auto& coordinate : box->style().scrollSnapCoordinates()) {
                LayoutUnit lastPotentialSnapPositionX = LayoutUnit(left) + valueForLength(coordinate.width(), viewWidth);
                if (shouldAddHorizontalChildOffsets && lastPotentialSnapPositionX > 0)
                    horizontalSnapOffsetSubsequence.append(lastPotentialSnapPositionX);

                LayoutUnit lastPotentialSnapPositionY = LayoutUnit(top) + valueForLength(coordinate.height(), viewHeight);
                if (shouldAddVerticalChildOffsets && lastPotentialSnapPositionY > 0)
                    verticalSnapOffsetSubsequence.append(lastPotentialSnapPositionY);
            }
        }
        child = child->nextElementSibling();
    }
}

static void updateFromStyle(Vector<LayoutUnit>& snapOffsets, const RenderStyle& style, ScrollEventAxis axis, LayoutUnit viewSize, LayoutUnit scrollSize, Vector<LayoutUnit>& snapOffsetSubsequence)
{
    std::sort(snapOffsetSubsequence.begin(), snapOffsetSubsequence.end());
    if (!snapOffsetSubsequence.size())
        snapOffsetSubsequence.append(0);

    bool isHorizontalAxis = axis == ScrollEventAxis::Horizontal;
    auto& points = isHorizontalAxis ? style.scrollSnapPointsX() : style.scrollSnapPointsY();
    auto& destination = style.scrollSnapDestination();
    bool hasRepeat = points.hasRepeat;
    LayoutUnit repeatOffset = valueForLength(points.repeatOffset, viewSize);
    LayoutUnit destinationOffset = valueForLength(isHorizontalAxis ? destination.width() : destination.height(), viewSize);
    LayoutUnit curSnapPositionShift = 0;
    LayoutUnit maxScrollOffset = scrollSize - viewSize;
    LayoutUnit lastSnapPosition = curSnapPositionShift;
    snapOffsets.append(0);
    do {
        for (auto& snapPosition : snapOffsetSubsequence) {
            LayoutUnit potentialSnapPosition = curSnapPositionShift + snapPosition - destinationOffset;
            if (potentialSnapPosition <= 0)
                continue;

            if (potentialSnapPosition >= maxScrollOffset)
                break;

            snapOffsets.append(potentialSnapPosition);
            lastSnapPosition = potentialSnapPosition + destinationOffset;
        }
        curSnapPositionShift = lastSnapPosition + repeatOffset;
    } while (hasRepeat && curSnapPositionShift < maxScrollOffset);
    // Always put a snap point on the maximum scroll offset.
    // Not a part of the spec, but necessary to prevent unreachable content when snapping.
    if (snapOffsets.last() != maxScrollOffset)
        snapOffsets.append(maxScrollOffset);
}

void updateSnapOffsetsForScrollableArea(ScrollableArea& scrollableArea, HTMLElement& scrollingElement, const RenderBox& scrollingElementBox, const RenderStyle& scrollingElementStyle)
{
    if (scrollingElementStyle.scrollSnapType() == ScrollSnapType::None) {
        scrollableArea.clearHorizontalSnapOffsets();
        scrollableArea.clearVerticalSnapOffsets();
        return;
    }

    LayoutUnit viewWidth = scrollingElementBox.width();
    LayoutUnit viewHeight = scrollingElementBox.height();
    LayoutUnit scrollWidth = scrollingElementBox.scrollWidth();
    LayoutUnit scrollHeight = scrollingElementBox.scrollHeight();
    bool canComputeHorizontalOffsets = scrollWidth > 0 && viewWidth > 0 && viewWidth < scrollWidth;
    bool canComputeVerticalOffsets = scrollHeight > 0 && viewHeight > 0 && viewHeight < scrollHeight;

    if (!canComputeHorizontalOffsets)
        scrollableArea.clearHorizontalSnapOffsets();
    if (!canComputeVerticalOffsets)
        scrollableArea.clearVerticalSnapOffsets();

    if (!canComputeHorizontalOffsets && !canComputeVerticalOffsets)
        return;

    Vector<LayoutUnit> horizontalSnapOffsetSubsequence;
    Vector<LayoutUnit> verticalSnapOffsetSubsequence;

    if (scrollingElementStyle.scrollSnapPointsX().usesElements || scrollingElementStyle.scrollSnapPointsY().usesElements) {
        bool shouldAddHorizontalChildOffsets = scrollingElementStyle.scrollSnapPointsX().usesElements && canComputeHorizontalOffsets;
        bool shouldAddVerticalChildOffsets = scrollingElementStyle.scrollSnapPointsY().usesElements && canComputeVerticalOffsets;
        appendChildSnapOffsets(scrollingElement, shouldAddHorizontalChildOffsets, horizontalSnapOffsetSubsequence, shouldAddVerticalChildOffsets, verticalSnapOffsetSubsequence);
    }

    if (!scrollingElementStyle.scrollSnapPointsX().usesElements && canComputeHorizontalOffsets) {
        for (auto& snapLength : scrollingElementStyle.scrollSnapPointsX().offsets)
            horizontalSnapOffsetSubsequence.append(valueForLength(snapLength, viewWidth));
    }

    if (!scrollingElementStyle.scrollSnapPointsY().usesElements && canComputeVerticalOffsets) {
        for (auto& snapLength : scrollingElementStyle.scrollSnapPointsY().offsets)
            verticalSnapOffsetSubsequence.append(valueForLength(snapLength, viewHeight));
    }

    if (canComputeHorizontalOffsets) {
        auto horizontalSnapOffsets = std::make_unique<Vector<LayoutUnit>>();
        updateFromStyle(*horizontalSnapOffsets, scrollingElementStyle, ScrollEventAxis::Horizontal, viewWidth, scrollWidth, horizontalSnapOffsetSubsequence);
        scrollableArea.setHorizontalSnapOffsets(WTF::move(horizontalSnapOffsets));
    }
    if (canComputeVerticalOffsets) {
        auto verticalSnapOffsets = std::make_unique<Vector<LayoutUnit>>();
        updateFromStyle(*verticalSnapOffsets, scrollingElementStyle, ScrollEventAxis::Vertical, viewHeight, scrollHeight, verticalSnapOffsetSubsequence);
        scrollableArea.setVerticalSnapOffsets(WTF::move(verticalSnapOffsets));
    }
}

} // namespace WebCore

#endif // CSS_SCROLL_SNAP
