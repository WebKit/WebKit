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
#include "RasterLayoutShape.h"

#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RasterShapeIntervals);

class MarginIntervalGenerator {
public:
    explicit MarginIntervalGenerator(unsigned radius);
    bool isValid() const { return !m_xIntercepts.isEmpty(); }
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
    if (!m_xIntercepts.tryGrow(radius + 1))
        return;

    double radiusSquared = static_cast<double>(radius) * radius;
    for (unsigned y = 0; y <= radius; y++)
        m_xIntercepts[y] = sqrt(radiusSquared - static_cast<double>(y) * y);
}

void NODELETE MarginIntervalGenerator::set(int y, const IntShapeInterval& interval)
{
    ASSERT(!interval.isEmpty());
    m_y = y;
    m_x1 = interval.x1();
    m_x2 = interval.x2();
}

IntShapeInterval MarginIntervalGenerator::intervalAt(int y) const
{
    uint64_t xInterceptsIndex = std::abs(static_cast<int64_t>(y) - m_y);
    int dx = (xInterceptsIndex >= m_xIntercepts.size()) ? 0 : m_xIntercepts[xInterceptsIndex];
    return IntShapeInterval(clampTo<int>(static_cast<int64_t>(m_x1) - dx), clampTo<int>(static_cast<int64_t>(m_x2) + dx));
}

std::unique_ptr<RasterShapeIntervals> RasterShapeIntervals::computeShapeMarginIntervals(int shapeMargin) const
{
    int64_t marginIntervalsSize = (offset() > shapeMargin) ? size() : static_cast<int64_t>(size()) - static_cast<int64_t>(offset()) * 2 + static_cast<int64_t>(shapeMargin) * 2;
    if (!isInBounds<int>(marginIntervalsSize))
        return makeUnique<RasterShapeIntervals>(0);

    auto result = makeUnique<RasterShapeIntervals>(static_cast<unsigned>(marginIntervalsSize), std::max(shapeMargin, offset()));
    if (!result->allocationSucceeded())
        return result;

    MarginIntervalGenerator marginIntervalGenerator(shapeMargin);
    if (!marginIntervalGenerator.isValid())
        return result;

    for (int y = bounds().y(); y < bounds().maxY(); ++y) {
        const IntShapeInterval& intervalAtY = intervalAt(y);
        if (intervalAtY.isEmpty())
            continue;

        marginIntervalGenerator.set(y, intervalAtY);
        int marginY0 = clampTo<int>(std::max<int64_t>(minY(), static_cast<int64_t>(y) - shapeMargin));
        int marginY1 = clampTo<int>(std::min<int64_t>(maxY(), static_cast<int64_t>(y) + shapeMargin + 1));

        for (int marginY = y - 1; marginY >= marginY0; --marginY) {
            if (marginY > bounds().y() && intervalAt(marginY).contains(intervalAtY))
                break;
            result->intervalAt(marginY).unite(marginIntervalGenerator.intervalAt(marginY));
        }

        result->intervalAt(y).unite(marginIntervalGenerator.intervalAt(y));

        for (int marginY = y + 1; marginY < marginY1; ++marginY) {
            if (marginY < bounds().maxY() && intervalAt(marginY).contains(intervalAtY))
                break;
            result->intervalAt(marginY).unite(marginIntervalGenerator.intervalAt(marginY));
        }
    }

    result->initializeBounds();
    return result;
}

void RasterShapeIntervals::initializeBounds()
{
    m_bounds = IntRect();
    for (int y = minY(); y < maxY(); ++y) {
        const IntShapeInterval& intervalAtY = intervalAt(y);
        if (intervalAtY.isEmpty())
            continue;
        m_bounds.unite(IntRect(intervalAtY.x1(), y, intervalAtY.width(), 1));
    }
}

void RasterShapeIntervals::buildBoundsPath(Path& path) const
{
    for (int y = bounds().y(); y < bounds().maxY(); y++) {
        if (intervalAt(y).isEmpty())
            continue;

        IntShapeInterval extent = intervalAt(y);
        int endY = y + 1;
        for (; endY < bounds().maxY(); endY++) {
            if (intervalAt(endY).isEmpty() || intervalAt(endY) != extent)
                break;
        }
        path.addRect(FloatRect(extent.x1(), y, extent.width(), endY - y));
        y = endY - 1;
    }
}

const RasterShapeIntervals& RasterLayoutShape::marginIntervals() const
{
    ASSERT(shapeMargin() >= 0);
    if (!shapeMargin() || m_intervals->isEmpty())
        return *m_intervals;

    int shapeMarginInt = clampToPositiveInteger(ceil(shapeMargin()));
    int maxShapeMarginInt = clampToPositiveInteger(static_cast<double>(std::max(m_marginRectSize.width(), m_marginRectSize.height())) * sqrt(2));
    if (!m_marginIntervals)
        lazyInitialize(m_marginIntervals, m_intervals->computeShapeMarginIntervals(std::min(shapeMarginInt, maxShapeMarginInt)));

    return *m_marginIntervals;
}

LineSegment RasterLayoutShape::getExcludedInterval(LayoutUnit logicalTop, LayoutUnit logicalHeight) const
{
    const RasterShapeIntervals& intervals = marginIntervals();
    if (intervals.isEmpty())
        return { };

    int y1 = logicalTop;
    int y2 = logicalTop + logicalHeight;
    ASSERT(y2 >= y1);
    if (y2 < intervals.bounds().y() || y1 >= intervals.bounds().maxY())
        return { };

    y1 = std::max(y1, intervals.bounds().y());
    y2 = std::min(y2, intervals.bounds().maxY());
    IntShapeInterval excludedInterval;

    if (y1 == y2)
        excludedInterval = intervals.intervalAt(y1);
    else {
        for (int y = y1; y < y2;  y++)
            excludedInterval.unite(intervals.intervalAt(y));
    }

    if (!shouldFlipStartAndEndPoints(writingMode()))
        return LineSegment(excludedInterval.x1(), excludedInterval.x2());

    auto x1 = m_logicalBoxWidthForFlipping - excludedInterval.x2();
    return LineSegment(x1, x1 + excludedInterval.width());
}

} // namespace WebCore
