/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CSS_SCROLL_SNAP)

#include "LayoutUnit.h"
#include "ScrollTypes.h"
#include "StyleScrollSnapPoints.h"
#include <utility>
#include <wtf/Vector.h>

namespace WebCore {

class LayoutRect;
class ScrollableArea;
class RenderBox;
class RenderStyle;

template <typename T>
struct SnapOffset {
    T offset;
    ScrollSnapStop stop;
};

template <typename T>
struct ScrollOffsetRange {
    T start;
    T end;
};

template <typename T>
struct ScrollSnapOffsetsInfo {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Vector<SnapOffset<T>> horizontalSnapOffsets;
    Vector<SnapOffset<T>> verticalSnapOffsets;

    // Snap offset ranges represent non-empty ranges of scroll offsets in which scrolling may rest after scroll snapping.
    // These are used in two cases: (1) for proximity scroll snapping, where portions of areas between adjacent snap offsets
    // may emit snap offset ranges, and (2) in the case where the snap area is larger than the snap port, in which case areas
    // where the snap port fits within the snap area are considered to be valid snap positions.
    Vector<ScrollOffsetRange<T>> horizontalSnapOffsetRanges;
    Vector<ScrollOffsetRange<T>> verticalSnapOffsetRanges;

    bool isEqual(const ScrollSnapOffsetsInfo<T>& other) const
    {
        return horizontalSnapOffsets == other.horizontalSnapOffsets && verticalSnapOffsets == other.verticalSnapOffsets && horizontalSnapOffsetRanges == other.horizontalSnapOffsetRanges && verticalSnapOffsetRanges == other.verticalSnapOffsetRanges;
    }

    bool isEmpty() const
    {
        return horizontalSnapOffsets.isEmpty() && verticalSnapOffsets.isEmpty();
    }

    Vector<SnapOffset<T>> offsetsForAxis(ScrollEventAxis axis) const
    {
        return axis == ScrollEventAxis::Vertical ? verticalSnapOffsets : horizontalSnapOffsets;
    }

    Vector<ScrollOffsetRange<T>> offsetRangesForAxis(ScrollEventAxis axis) const
    {
        return axis == ScrollEventAxis::Vertical ? verticalSnapOffsetRanges : horizontalSnapOffsetRanges;
    }

    template<typename OutputType> ScrollSnapOffsetsInfo<OutputType> convertUnits(float deviceScaleFactor = 0.0) const;
    WEBCORE_EXPORT std::pair<T, unsigned> closestSnapOffset(ScrollEventAxis, T scrollDestinationOffset, float velocity, Optional<T> originalPositionForDirectionalSnapping = WTF::nullopt) const;
};

template <> template <>
ScrollSnapOffsetsInfo<LayoutUnit> ScrollSnapOffsetsInfo<float>::convertUnits(float /* unusedScaleFactor */) const;
template <>
WEBCORE_EXPORT std::pair<float, unsigned> ScrollSnapOffsetsInfo<float>::closestSnapOffset(ScrollEventAxis, float scrollDestinationOffset, float velocity, Optional<float> originalPositionForDirectionalSnapping) const;

template <> template <>
ScrollSnapOffsetsInfo<float> ScrollSnapOffsetsInfo<LayoutUnit>::convertUnits(float deviceScaleFactor) const;
template <>
WEBCORE_EXPORT std::pair<LayoutUnit, unsigned> ScrollSnapOffsetsInfo<LayoutUnit>::closestSnapOffset(ScrollEventAxis, LayoutUnit scrollDestinationOffset, float velocity, Optional<LayoutUnit> originalPositionForDirectionalSnapping) const;

const unsigned invalidSnapOffsetIndex = UINT_MAX;

// Update the snap offsets for this scrollable area, given the RenderBox of the scroll container, the RenderStyle
// which defines the scroll-snap properties, and the viewport rectangle with the origin at the top left of
// the scrolling container's border box.
void updateSnapOffsetsForScrollableArea(ScrollableArea&, const RenderBox& scrollingElementBox, const RenderStyle& scrollingElementStyle, LayoutRect viewportRectInBorderBoxCoordinates);

template <typename T> WTF::TextStream& operator<<(WTF::TextStream& ts, SnapOffset<T> offset)
{
    ts << offset.offset;
    if (offset.stop == ScrollSnapStop::Always)
        ts << " (always)";
    return ts;
}

template<typename T>
TextStream& operator<<(TextStream& ts, const ScrollOffsetRange<T>& range)
{
    ts << "start: " << range.start << " end: " << range.end;
    return ts;
}

}; // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)
