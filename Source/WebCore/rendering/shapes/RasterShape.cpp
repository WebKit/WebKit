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

class MarginIntervalGenerator {
public:
    MarginIntervalGenerator(unsigned radius);
    void set(int y, const IntShapeInterval&);
    IntShapeInterval intervalAt(int y) const;

private:
    Vector<int> m_xIntercepts;
    int m_y;
    int m_x1;
    int m_x2;
};

MarginIntervalGenerator::MarginIntervalGenerator(unsigned radius)
    : m_y(0)
    , m_x1(0)
    , m_x2(0)
{
    m_xIntercepts.resize(radius + 1);
    unsigned radiusSquared = radius * radius;
    for (unsigned y = 0; y <= radius; y++)
        m_xIntercepts[y] = sqrt(static_cast<double>(radiusSquared - y * y));
}

void MarginIntervalGenerator::set(int y, const IntShapeInterval& interval)
{
    ASSERT(y >= 0 && interval.x1() >= 0);
    m_y = y;
    m_x1 = interval.x1();
    m_x2 = interval.x2();
}

IntShapeInterval MarginIntervalGenerator::intervalAt(int y) const
{
    unsigned xInterceptsIndex = abs(y - m_y);
    int dx = (xInterceptsIndex >= m_xIntercepts.size()) ? 0 : m_xIntercepts[xInterceptsIndex];
    return IntShapeInterval(m_x1 - dx, m_x2 + dx);
}

void RasterShapeIntervals::appendInterval(int y, int x1, int x2)
{
    ASSERT(x2 > x1 && (intervalsAt(y).isEmpty() || x1 > intervalsAt(y).last().x2()));
    m_bounds.unite(IntRect(x1, y, x2 - x1, 1));
    intervalsAt(y).append(IntShapeInterval(x1, x2));
}

void RasterShapeIntervals::uniteMarginInterval(int y, const IntShapeInterval& interval)
{
    ASSERT(intervalsAt(y).size() <= 1); // Each m_intervalLists entry has 0 or one interval.

    if (intervalsAt(y).isEmpty())
        intervalsAt(y).append(interval);
    else {
        IntShapeInterval& resultInterval = intervalsAt(y)[0];
        resultInterval.set(std::min(resultInterval.x1(), interval.x1()), std::max(resultInterval.x2(), interval.x2()));
    }

    m_bounds.unite(IntRect(interval.x1(), y, interval.width(), 1));
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
        if (!shapeIntervalsContain(intervalsAt(y), rectInterval))
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
        if (intervalsAt(y).isEmpty())
            return false;
    }

    appendX1Values(intervalsAt(y1), minIntervalWidth, result);
    for (int y = y1 + 1; y < y2;  y++) {
        if (intervalsAt(y) != intervalsAt(y - 1))
            appendX1Values(intervalsAt(y), minIntervalWidth, result);
    }

    return true;
}

void RasterShapeIntervals::getExcludedIntervals(int y1, int y2, IntShapeIntervals& result) const
{
    ASSERT(y2 >= y1);

    if (y2 < bounds().y() || y1 >= bounds().maxY())
        return;

    y1 = std::max(y1, bounds().y());
    y2 = std::min(y2, bounds().maxY());

    result = intervalsAt(y1);
    for (int y = y1 + 1; y < y2;  y++) {
        IntShapeIntervals intervals;
        IntShapeInterval::uniteShapeIntervals(result, intervalsAt(y), intervals);
        result.swap(intervals);
    }
}

PassOwnPtr<RasterShapeIntervals> RasterShapeIntervals::computeShapeMarginIntervals(int shapeMargin) const
{
    int marginIntervalsSize = (offset() > shapeMargin) ? size() : size() - offset() * 2 + shapeMargin * 2;
    OwnPtr<RasterShapeIntervals> result = adoptPtr(new RasterShapeIntervals(marginIntervalsSize, std::max(shapeMargin, offset())));
    MarginIntervalGenerator marginIntervalGenerator(shapeMargin);

    for (int y = bounds().y(); y < bounds().maxY(); ++y) {
        const IntShapeInterval& intervalAtY = limitIntervalAt(y);
        if (intervalAtY.isEmpty())
            continue;

        marginIntervalGenerator.set(y, intervalAtY);
        int marginY0 = std::max(minY(), y - shapeMargin);
        int marginY1 = std::min(maxY(), y + shapeMargin);

        for (int marginY = y - 1; marginY >= marginY0; --marginY) {
            if (marginY > bounds().y() && limitIntervalAt(marginY).contains(intervalAtY))
                break;
            result->uniteMarginInterval(marginY, marginIntervalGenerator.intervalAt(marginY));
        }

        result->uniteMarginInterval(y, marginIntervalGenerator.intervalAt(y));

        for (int marginY = y + 1; marginY <= marginY1; ++marginY) {
            if (marginY < bounds().maxY() && limitIntervalAt(marginY).contains(intervalAtY))
                break;
            result->uniteMarginInterval(marginY, marginIntervalGenerator.intervalAt(marginY));
        }
    }

    return result.release();
}

void RasterShapeIntervals::buildBoundsPath(Path& path) const
{
    for (int y = bounds().y(); y < bounds().maxY(); y++) {
        if (intervalsAt(y).isEmpty())
            continue;

        IntShapeInterval extent = limitIntervalAt(y);
        int endY = y + 1;
        for (; endY < bounds().maxY(); endY++) {
            if (intervalsAt(endY).isEmpty() || limitIntervalAt(endY) != extent)
                break;
        }
        path.addRect(FloatRect(extent.x1(), y, extent.width(), endY - y));
        y = endY - 1;
    }
}

const RasterShapeIntervals& RasterShape::marginIntervals() const
{
    ASSERT(shapeMargin() >= 0);
    if (!shapeMargin())
        return *m_intervals;

    int shapeMarginInt = clampToPositiveInteger(ceil(shapeMargin()));
    int maxShapeMarginInt = std::max(m_marginRectSize.width(), m_marginRectSize.height()) * sqrt(2);
    if (!m_marginIntervals)
        m_marginIntervals = m_intervals->computeShapeMarginIntervals(std::min(shapeMarginInt, maxShapeMarginInt));

    return *m_marginIntervals;
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

} // namespace WebCore
