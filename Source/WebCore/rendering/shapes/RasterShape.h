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

#ifndef RasterShape_h
#define RasterShape_h

#include "FloatRect.h"
#include "Shape.h"
#include "ShapeInterval.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

class RasterShapeIntervals {
public:
    RasterShapeIntervals(unsigned size)
    {
        m_intervalLists.resize(size);
    }

    const IntRect& bounds() const { return m_bounds; }
    bool isEmpty() const { return m_bounds.isEmpty(); }
    void appendInterval(int y, int x1, int x2);

    void getIncludedIntervals(int y1, int y2, IntShapeIntervals& result) const;
    void getExcludedIntervals(int y1, int y2, IntShapeIntervals& result) const;
    bool firstIncludedIntervalY(int minY, const IntSize& minSize, LayoutUnit& result) const;

private:
    int size() const { return m_intervalLists.size(); }
    const IntShapeIntervals& getIntervals(int y) const
    {
        ASSERT(y >= 0 && y < size());
        return m_intervalLists[y];
    }
    bool contains(const IntRect&) const;
    bool getIntervalX1Values(int minY, int maxY, int minIntervalWidth, Vector<int>& result) const;

    IntRect m_bounds;
    Vector<IntShapeIntervals > m_intervalLists;
};

class RasterShape : public Shape {
    WTF_MAKE_NONCOPYABLE(RasterShape);
public:
    RasterShape(PassOwnPtr<RasterShapeIntervals> intervals)
        : Shape()
        , m_intervals(intervals)
    {
    }

    virtual LayoutRect shapeMarginLogicalBoundingBox() const OVERRIDE { return static_cast<LayoutRect>(marginIntervals().bounds()); }
    virtual LayoutRect shapePaddingLogicalBoundingBox() const OVERRIDE { return static_cast<LayoutRect>(paddingIntervals().bounds()); }
    virtual bool isEmpty() const OVERRIDE { return m_intervals->isEmpty(); }
    virtual void getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList&) const OVERRIDE;
    virtual void getIncludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList&) const OVERRIDE;
    virtual bool firstIncludedIntervalLogicalTop(LayoutUnit minLogicalIntervalTop, const LayoutSize& minLogicalIntervalSize, LayoutUnit&) const OVERRIDE;

private:
    const RasterShapeIntervals& marginIntervals() const;
    const RasterShapeIntervals& paddingIntervals() const;

    OwnPtr<RasterShapeIntervals> m_intervals;
};

} // namespace WebCore

#endif // RasterShape_h
