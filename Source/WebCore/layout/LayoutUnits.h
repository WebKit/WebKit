/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutUnit.h"
#include "LayoutPoint.h"
#include "LayoutRect.h"
#include "MarginTypes.h"
#include <wtf/Optional.h>

namespace WebCore {

#define USE_FLOAT_AS_INLINE_LAYOUT_UNIT 1

#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
using InlineLayoutUnit = float;
using InlineLayoutPoint = FloatPoint;
using InlineLayoutSize = FloatSize;
using InlineLayoutRect = FloatRect;
#else
using InlineLayoutUnit = LayoutUnit;
using InlineLayoutPoint = LayoutPoint;
using InlineLayoutSize = LayoutSize;
using InlineLayoutRect = LayoutRect;
#endif

namespace Layout {

struct Position {
    operator LayoutUnit() const { return value; }
    LayoutUnit value;
};

inline bool operator<(const Position& a, const Position& b)
{
    return a.value < b.value;
}

inline bool operator==(const Position& a, const Position& b)
{
    return a.value == b.value;
}

struct Point {
    // FIXME: Use Position<Horizontal>, Position<Vertical> to avoid top/left vs. x/y confusion.
    LayoutUnit x; // left
    LayoutUnit y; // top

    Point() = default;
    Point(LayoutUnit, LayoutUnit);
    Point(LayoutPoint);
    static Point max() { return { LayoutUnit::max(), LayoutUnit::max() }; }

    void move(LayoutSize);
    void moveBy(LayoutPoint);
    operator LayoutPoint() const { return { x, y }; }
};

// FIXME: Wrap these into structs.
using PointInContextRoot = Point;
using PositionInContextRoot = Position;

inline Point::Point(LayoutPoint point)
    : x(point.x())
    , y(point.y())
{
}

inline Point::Point(LayoutUnit x, LayoutUnit y)
    : x(x)
    , y(y)
{
}

inline void Point::move(LayoutSize offset)
{
    x += offset.width();
    y += offset.height();
}

inline void Point::moveBy(LayoutPoint offset)
{
    x += offset.x();
    y += offset.y();
}

// Margin, border, padding
struct HorizontalEdges {
    LayoutUnit left;
    LayoutUnit right;

    LayoutUnit width() const { return left + right; }
};

struct VerticalEdges {
    LayoutUnit top;
    LayoutUnit bottom;

    LayoutUnit height() const { return top + bottom; }
};

struct Edges {
    HorizontalEdges horizontal;
    VerticalEdges vertical;
};

struct ContentWidthAndMargin {
    LayoutUnit contentWidth;
    UsedHorizontalMargin usedMargin;
    ComputedHorizontalMargin computedMargin;
};

struct ContentHeightAndMargin {
    LayoutUnit contentHeight;
    UsedVerticalMargin::NonCollapsedValues nonCollapsedMargin;
};

struct HorizontalGeometry {
    LayoutUnit left;
    LayoutUnit right;
    ContentWidthAndMargin contentWidthAndMargin;
};

struct VerticalGeometry {
    LayoutUnit top;
    LayoutUnit bottom;
    ContentHeightAndMargin contentHeightAndMargin;
};

struct HorizontalConstraints {
    LayoutUnit logicalLeft;
    LayoutUnit logicalWidth;
};

struct VerticalConstraints {
    LayoutUnit logicalTop;
    Optional<LayoutUnit> logicalHeight;
};

struct OverrideHorizontalValues {
    Optional<LayoutUnit> width;
    Optional<UsedHorizontalMargin> margin;
};

struct OverrideVerticalValues {
    // Consider collapsing it.
    Optional<LayoutUnit> height;
};

inline LayoutUnit toLayoutUnit(InlineLayoutUnit value)
{
    return LayoutUnit { value };
}

inline LayoutPoint toLayoutPoint(const InlineLayoutPoint& point)
{
    return LayoutPoint { point };
}

inline LayoutRect toLayoutRect(const InlineLayoutRect& rect)
{
    return LayoutRect { rect };
}

inline InlineLayoutUnit maxInlineLayoutUnit()
{
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
    return std::numeric_limits<float>::max();
#else
    return LayoutUnit::max();
#endif
}

}
}
#endif
