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

#ifndef ExclusionPolygon_h
#define ExclusionPolygon_h

#include "ExclusionInterval.h"
#include "ExclusionShape.h"
#include "FloatPolygon.h"

namespace WebCore {

class OffsetPolygonEdge : public VertexPair {
public:
    OffsetPolygonEdge(const FloatPolygonEdge& edge, const FloatSize& offset)
        : m_vertex1(edge.vertex1() + offset)
        , m_vertex2(edge.vertex2() + offset)
        , m_edgeIndex(edge.edgeIndex())
    {
    }

    OffsetPolygonEdge(const FloatPoint& reflexVertex, const FloatSize& offset1, const FloatSize& offset2)
        : m_vertex1(reflexVertex + offset1)
        , m_vertex2(reflexVertex + offset2)
        , m_edgeIndex(-1)
    {
    }

    OffsetPolygonEdge(const FloatPolygon& polygon, float minLogicalIntervalTop, const FloatSize& offset)
        : m_vertex1(FloatPoint(polygon.boundingBox().x(), minLogicalIntervalTop) + offset)
        , m_vertex2(FloatPoint(polygon.boundingBox().maxX(), minLogicalIntervalTop) + offset)
        , m_edgeIndex(-1)
    {
    }

    virtual const FloatPoint& vertex1() const OVERRIDE { return m_vertex1; }
    virtual const FloatPoint& vertex2() const OVERRIDE { return m_vertex2; }
    int edgeIndex() const { return m_edgeIndex; }

private:
    FloatPoint m_vertex1;
    FloatPoint m_vertex2;
    int m_edgeIndex;
};

class ExclusionPolygon : public ExclusionShape {
    WTF_MAKE_NONCOPYABLE(ExclusionPolygon);
public:
    ExclusionPolygon(PassOwnPtr<Vector<FloatPoint> > vertices, WindRule fillRule)
        : ExclusionShape()
        , m_polygon(vertices, fillRule)
        , m_marginBounds(nullptr)
        , m_paddingBounds(nullptr)
    {
    }

    virtual FloatRect shapeLogicalBoundingBox() const OVERRIDE { return m_polygon.boundingBox(); }
    virtual bool isEmpty() const OVERRIDE { return m_polygon.isEmpty(); }
    virtual void getExcludedIntervals(float logicalTop, float logicalHeight, SegmentList&) const OVERRIDE;
    virtual void getIncludedIntervals(float logicalTop, float logicalHeight, SegmentList&) const OVERRIDE;
    virtual bool firstIncludedIntervalLogicalTop(float minLogicalIntervalTop, const FloatSize& minLogicalIntervalSize, float&) const OVERRIDE;

    const FloatPolygon& shapeMarginBounds() const;
    const FloatPolygon& shapePaddingBounds() const;

private:
    FloatPolygon m_polygon;
    mutable OwnPtr<FloatPolygon> m_marginBounds;
    mutable OwnPtr<FloatPolygon> m_paddingBounds;
};

} // namespace WebCore

#endif // ExclusionPolygon_h
