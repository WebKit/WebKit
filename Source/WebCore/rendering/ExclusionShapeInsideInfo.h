/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ExclusionShapeInsideInfo_h
#define ExclusionShapeInsideInfo_h

#if ENABLE(CSS_EXCLUSIONS)

#include "ExclusionShapeInfo.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class InlineIterator;
class RenderBlock;
class RenderObject;

struct LineSegmentIterator {
    RenderObject* root;
    RenderObject* object;
    unsigned offset;
    LineSegmentIterator(RenderObject* root, RenderObject* object, unsigned offset)
        : root(root)
        , object(object)
        , offset(offset)
    {
    }
};

struct LineSegmentRange {
    LineSegmentIterator start;
    LineSegmentIterator end;
    LineSegmentRange(const InlineIterator& start, const InlineIterator& end);
};

typedef Vector<LineSegmentRange> SegmentRangeList;

class ExclusionShapeInsideInfo : public ExclusionShapeInfo<RenderBlock, &RenderStyle::resolvedShapeInside, &ExclusionShape::getIncludedIntervals> {
public:
    static PassOwnPtr<ExclusionShapeInsideInfo> createInfo(const RenderBlock* renderer) { return adoptPtr(new ExclusionShapeInsideInfo(renderer)); }

    static bool isEnabledFor(const RenderBlock* renderer);

    virtual bool computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight) OVERRIDE
    {
        m_segmentRanges.clear();
        return ExclusionShapeInfo<RenderBlock, &RenderStyle::resolvedShapeInside, &ExclusionShape::getIncludedIntervals>::computeSegmentsForLine(lineTop, lineHeight);
    }

    bool hasSegments() const
    {
        return lineOverlapsShapeBounds() && m_segments.size();
    }
    const SegmentList& segments() const
    {
        ASSERT(hasSegments());
        return m_segments;
    }
    SegmentRangeList& segmentRanges() { return m_segmentRanges; }
    const SegmentRangeList& segmentRanges() const { return m_segmentRanges; }
    const LineSegment* currentSegment() const
    {
        if (!hasSegments())
            return 0;
        ASSERT(m_segmentRanges.size() < m_segments.size());
        return &m_segments[m_segmentRanges.size()];
    }
    bool adjustLogicalLineTop(float minSegmentWidth);

    void setNeedsLayout(bool value) { m_needsLayout = value; }
    bool needsLayout() { return m_needsLayout; }

protected:
    virtual FloatRect computedShapeLogicalBoundingBox() const OVERRIDE { return computedShape()->shapePaddingLogicalBoundingBox(); }

private:
    ExclusionShapeInsideInfo(const RenderBlock* renderer)
    : ExclusionShapeInfo<RenderBlock, &RenderStyle::resolvedShapeInside, &ExclusionShape::getIncludedIntervals> (renderer)
    , m_needsLayout(false)
    { }

    SegmentRangeList m_segmentRanges;
    bool m_needsLayout;
};

}
#endif
#endif
