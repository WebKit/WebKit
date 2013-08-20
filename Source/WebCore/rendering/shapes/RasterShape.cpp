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

#include "ShapeInterval.h"
#include <wtf/MathExtras.h>

namespace WebCore {

IntRect RasterShapeIntervals::bounds() const
{
    if (!m_bounds)
        m_bounds = adoptPtr(new IntRect(m_region.bounds()));
    return *m_bounds;
}

void RasterShapeIntervals::addInterval(int y, int x1, int x2)
{
    m_region.unite(Region(IntRect(x1, y, x2 - x1, 1)));
    m_bounds.clear();
}

bool RasterShapeIntervals::firstIncludedIntervalY(int minY, const IntSize& minSize, LayoutUnit& result) const
{
    for (int y = minY; y <= bounds().maxY() - minSize.height(); y++) {
        Region lineRegion(IntRect(bounds().x(), y, bounds().width(), minSize.height()));
        lineRegion.intersect(m_region);
        if (lineRegion.isEmpty())
            continue;

        const Vector<IntRect>& lineRects = lineRegion.rects();
        ASSERT(lineRects.size() > 0);

        for (unsigned i = 0; i < lineRects.size(); i++) {
            IntRect rect = lineRects[i];
            if (rect.width() >= minSize.width() && lineRegion.contains(Region(IntRect(IntPoint(rect.x(), y), minSize)))) {
                result = y;
                return true;
            }
        }
    }
    return false;
}

static inline IntRect alignedRect(IntRect r, int y1, int y2)
{
    return IntRect(r.x(), y1, r.width(), y2 - y1);
}

void RasterShapeIntervals::getIncludedIntervals(int y1, int y2, SegmentList& result) const
{
    ASSERT(y2 >= y1);

    IntRect lineRect(bounds().x(), y1, bounds().width(), y2 - y1);
    Region lineRegion(lineRect);
    lineRegion.intersect(m_region);
    if (lineRegion.isEmpty())
        return;

    const Vector<IntRect>& lineRects = lineRegion.rects();
    ASSERT(lineRects.size() > 0);

    Region segmentsRegion(lineRect);
    Region intervalsRegion;

    // The loop below uses Regions to compute the intersection of the horizontal
    // shape intervals that fall within the line's box.
    int currentLineY = lineRects[0].y();
    int currentLineMaxY = lineRects[0].maxY();
    for (unsigned i = 0; i < lineRects.size(); ++i) {
        int lineY = lineRects[i].y();
        ASSERT(lineY >= currentLineY);
        if (lineY > currentLineMaxY) {
            // We've encountered a vertical gap in lineRects, there are no included intervals.
            return;
        }
        if (lineY > currentLineY) {
            currentLineY = lineY;
            currentLineMaxY = lineRects[i].maxY();
            segmentsRegion.intersect(intervalsRegion);
            intervalsRegion = Region();
        } else
            currentLineMaxY = std::max<int>(currentLineMaxY, lineRects[i].maxY());
        intervalsRegion.unite(Region(alignedRect(lineRects[i], y1, y2)));
    }
    if (!intervalsRegion.isEmpty())
        segmentsRegion.intersect(intervalsRegion);

    const Vector<IntRect>& segmentRects = segmentsRegion.rects();
    for (unsigned i = 0; i < segmentRects.size(); ++i)
        result.append(LineSegment(segmentRects[i].x(), segmentRects[i].maxX()));
}

void RasterShapeIntervals::getExcludedIntervals(int y1, int y2, SegmentList& result) const
{
    ASSERT(y2 >= y1);

    IntRect lineRect(bounds().x(), y1, bounds().width(), y2 - y1);
    Region lineRegion(lineRect);
    lineRegion.intersect(m_region);
    if (lineRegion.isEmpty())
        return;

    const Vector<IntRect>& lineRects = lineRegion.rects();
    ASSERT(lineRects.size() > 0);

    Region segmentsRegion;
    for (unsigned i = 0; i < lineRects.size(); i++)
        segmentsRegion.unite(Region(alignedRect(lineRects[i], y1, y2)));

    const Vector<IntRect>& segmentRects = segmentsRegion.rects();
    for (unsigned i = 0; i < segmentRects.size(); i++)
        result.append(LineSegment(segmentRects[i].x(), segmentRects[i].maxX() + 1));
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

void RasterShape::getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const RasterShapeIntervals& intervals = marginIntervals();
    if (intervals.isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;

    if (y2 < intervals.bounds().y() || y1 >= intervals.bounds().maxY())
        return;

    intervals.getExcludedIntervals(y1, y2, result);
}

void RasterShape::getIncludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const RasterShapeIntervals& intervals = paddingIntervals();
    if (intervals.isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;

    if (y1 < intervals.bounds().y() || y2 > intervals.bounds().maxY())
        return;

    intervals.getIncludedIntervals(y1, y2, result);
}

bool RasterShape::firstIncludedIntervalLogicalTop(LayoutUnit minLogicalIntervalTop, const LayoutSize& minLogicalIntervalSize, LayoutUnit& result) const
{
    float minIntervalTop = minLogicalIntervalTop;
    float minIntervalHeight = minLogicalIntervalSize.height();
    float minIntervalWidth = minLogicalIntervalSize.width();

    const RasterShapeIntervals& intervals = paddingIntervals();
    if (intervals.isEmpty() || minIntervalWidth > intervals.bounds().width())
        return false;

    float minY = std::max<float>(intervals.bounds().y(), minIntervalTop);
    float maxY = minY + minIntervalHeight;
    if (maxY > intervals.bounds().maxY())
        return false;

    return intervals.firstIncludedIntervalY(minY, flooredIntSize(minLogicalIntervalSize), result);
}

} // namespace WebCore
