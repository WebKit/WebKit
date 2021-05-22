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

#include "FloatRect.h"
#include "LayoutRect.h"
#include "LayoutUnit.h"
#include "ScrollTypes.h"
#include "StyleScrollSnapPoints.h"
#include <utility>
#include <wtf/Vector.h>

namespace WebCore {

class ScrollableArea;
class RenderBox;
class RenderStyle;

template <typename T>
struct SnapOffset {
    T offset;
    ScrollSnapStop stop;
    size_t snapAreaIndex;
};

template <typename UnitType, typename RectType>
struct ScrollSnapOffsetsInfo {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    ScrollSnapStrictness strictness;
    Vector<SnapOffset<UnitType>> horizontalSnapOffsets;
    Vector<SnapOffset<UnitType>> verticalSnapOffsets;
    Vector<RectType> snapAreas;

    bool isEqual(const ScrollSnapOffsetsInfo<UnitType, RectType>& other) const
    {
        return strictness == other.strictness && horizontalSnapOffsets == other.horizontalSnapOffsets && verticalSnapOffsets == other.verticalSnapOffsets && snapAreas == other.snapAreas;
    }

    bool isEmpty() const
    {
        return horizontalSnapOffsets.isEmpty() && verticalSnapOffsets.isEmpty();
    }

    Vector<SnapOffset<UnitType>> offsetsForAxis(ScrollEventAxis axis) const
    {
        return axis == ScrollEventAxis::Vertical ? verticalSnapOffsets : horizontalSnapOffsets;
    }

    template<typename OutputType> OutputType convertUnits(float deviceScaleFactor = 0.0) const;
    template<typename SizeType>
    WEBCORE_EXPORT std::pair<UnitType, unsigned> closestSnapOffset(ScrollEventAxis, const SizeType& viewportSize, UnitType scrollDestinationOffset, float velocity, Optional<UnitType> originalPositionForDirectionalSnapping = WTF::nullopt) const;
};

using LayoutScrollSnapOffsetsInfo = ScrollSnapOffsetsInfo<LayoutUnit, LayoutRect>;
using FloatScrollSnapOffsetsInfo = ScrollSnapOffsetsInfo<float, FloatRect>;

template <> template <>
LayoutScrollSnapOffsetsInfo FloatScrollSnapOffsetsInfo::convertUnits(float /* unusedScaleFactor */) const;
template <> template <>
WEBCORE_EXPORT std::pair<float, unsigned> FloatScrollSnapOffsetsInfo::closestSnapOffset(ScrollEventAxis, const FloatSize& viewportSize, float scrollDestinationOffset, float velocity, Optional<float> originalPositionForDirectionalSnapping) const;


template <> template <>
FloatScrollSnapOffsetsInfo LayoutScrollSnapOffsetsInfo::convertUnits(float deviceScaleFactor) const;
template <> template <>
WEBCORE_EXPORT std::pair<LayoutUnit, unsigned> LayoutScrollSnapOffsetsInfo::closestSnapOffset(ScrollEventAxis, const LayoutSize& viewportSize, LayoutUnit scrollDestinationOffset, float velocity, Optional<LayoutUnit> originalPositionForDirectionalSnapping) const;


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

}; // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)
