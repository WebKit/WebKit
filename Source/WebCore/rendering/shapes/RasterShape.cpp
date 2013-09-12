/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RasterShape.h"

#include <wtf/MathExtras.h>

namespace WebCore {

void RasterShapeIntervals::appendInterval(int y, int x1, int x2)
{
    ASSERT(y >= 0 && y < size() && x1 >= 0 && x2 > x1 && (m_intervalLists[y].isEmpty() || x1 > m_intervalLists[y].last().x2()));

    m_bounds.unite(IntRect(x1, y, x2 - x1, 1));
    m_intervalLists[y].append(IntShapeInterval(x1, x2));
}

static inline bool shapeIntervalsContain(const IntShapeIntervals& intervals, const IntShapeInterval& interval)
{
    for (unsigned i = 0; i < intervals.size(); i++) {
        if (intervals[i].x1() > interval.x2())
            return false;
        if (intervals[i].contains(interval))
            return true;
    }

    return false;
}

bool RasterShapeIntervals::contains(const IntRect& rect) const
{
    if (!bounds().contains(rect))
        return false;

    const IntShapeInterval& rectInterval = IntShapeInterval(rect.x(), rect.maxX());
    for (int y = rect.y(); y < rect.maxY(); y++) {
        if (!shapeIntervalsContain(getIntervals(y), rectInterval))
            return false;
    }

    return true;
}

static inline void appendX1Values(const IntShapeIntervals& intervals, int minIntervalWidth, Vector<int>& result)
{
    for (unsigned i = 0; i < intervals.size(); i++)
        if (intervals[i].width() >= minIntervalWidth)
            result.append(intervals[i].x1());
}

bool RasterShapeIntervals::getIntervalX1Values(int y1, int y2, int minIntervalWidth, Vector<int>& result) const
{
    ASSERT(y1 >= 0 && y2 > y1);

    for (int y = y1; y < y2;  y++) {
        if (getIntervals(y).isEmpty())
            return false;
    }

    appendX1Values(getIntervals(y1), minIntervalWidth, result);
    for (int y = y1 + 1; y < y2;  y++) {
        if (getIntervals(y) != getIntervals(y - 1))
            appendX1Values(getIntervals(y), minIntervalWidth, result);
    }

    return true;
}

bool RasterShapeIntervals::firstIncludedIntervalY(int minY, const IntSize& minSize, LayoutUnit& result) const
{
    minY = std::max<int>(bounds().y(), minY);

    ASSERT(minY >= 0 && minY + minSize.height() < size());

    if (minSize.isEmpty() || minSize.width() > bounds().width())
        return false;

    for (int lineY = minY; lineY <= bounds().maxY() - minSize.height(); lineY++) {
        Vector<int> intervalX1Values;
        if (!getIntervalX1Values(lineY, lineY + minSize.height(), minSize.width(), intervalX1Values))
            continue;

        std::sort(intervalX1Values.begin(), intervalX1Values.end());

        IntRect firstFitRect(IntPoint(0, 0), minSize);
        for (unsigned i = 0; i < intervalX1Values.size(); i++) {
            int lineX = intervalX1Values[i];
            if (i > 0 && lineX == intervalX1Values[i - 1])
                continue;
            firstFitRect.setLocation(IntPoint(lineX, lineY));
            if (contains(firstFitRect)) {
                result = lineY;
                return true;
            }
        }
    }

    return false;
}

void RasterShapeIntervals::getIncludedIntervals(int y1, int y2, IntShapeIntervals& result) const
{
    ASSERT(y2 >= y1);

    if (y1 < bounds().y() || y2 > bounds().maxY())
        return;

    for (int y = y1; y < y2;  y++) {
        if (getIntervals(y).isEmpty())
            return;
    }

    result = getIntervals(y1);
    for (int y = y1 + 1; y < y2 && !result.isEmpty();  y++) {
        IntShapeIntervals intervals;
        IntShapeInterval::intersectShapeIntervals(result, getIntervals(y), intervals);
        result.swap(intervals);
    }
}

void RasterShapeIntervals::getExcludedIntervals(int y1, int y2, IntShapeIntervals& result) const
{
    ASSERT(y2 >= y1);

    if (y2 < bounds().y() || y1 >= bounds().maxY())
        return;

    for (int y = y1; y < y2;  y++) {
        if (getIntervals(y).isEmpty())
            return;
    }

    result = getIntervals(y1);
    for (int y = y1 + 1; y < y2;  y++) {
        IntShapeIntervals intervals;
        IntShapeInterval::uniteShapeIntervals(result, getIntervals(y), intervals);
        result.swap(intervals);
    }
}

const RasterShapeIntervals& RasterShape::marginIntervals() const
{
    ASSERT(shapeMargin() >= 0);
    if (!shapeMargin())
        return *m_intervals;

    // FIXME: Add support for non-zero margin, see https://bugs.webkit.org/show_bug.cgi?id=116348.
    return *m_intervals;
}

const RasterShapeIntervals& RasterShape::paddingIntervals() const
{
    ASSERT(shapePadding() >= 0);
    if (!shapePadding())
        return *m_intervals;

    // FIXME: Add support for non-zero padding, see https://bugs.webkit.org/show_bug.cgi?id=116348.
    return *m_intervals;
}

static inline void appendLineSegments(const IntShapeIntervals& intervals, SegmentList& result)
{
    for (unsigned i = 0; i < intervals.size(); i++)
        result.append(LineSegment(intervals[i].x1(), intervals[i].x2() + 1));
}

void RasterShape::getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const RasterShapeIntervals& intervals = marginIntervals();
    if (intervals.isEmpty())
        return;

    IntShapeIntervals excludedIntervals;
    intervals.getExcludedIntervals(logicalTop, logicalTop + logicalHeight, excludedIntervals);
    appendLineSegments(excludedIntervals, result);
}

void RasterShape::getIncludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const RasterShapeIntervals& intervals = paddingIntervals();
    if (intervals.isEmpty())
        return;

    IntShapeIntervals includedIntervals;
    intervals.getIncludedIntervals(logicalTop, logicalTop + logicalHeight, includedIntervals);
    appendLineSegments(includedIntervals, result);
}

bool RasterShape::firstIncludedIntervalLogicalTop(LayoutUnit minLogicalIntervalTop, const LayoutSize& minLogicalIntervalSize, LayoutUnit& result) const
{
    const RasterShapeIntervals& intervals = paddingIntervals();
    if (intervals.isEmpty())
        return false;

    return intervals.firstIncludedIntervalY(minLogicalIntervalTop.floor(), flooredIntSize(minLogicalIntervalSize), result);
}

} // namespace WebCore
