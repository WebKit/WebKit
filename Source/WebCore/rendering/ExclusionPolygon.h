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
#include "FloatPoint.h"
#include "FloatRect.h"
#include "PODIntervalTree.h"
#include "WindRule.h"
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

struct ExclusionPolygonEdge;

// This class is used by PODIntervalTree for debugging.
#ifndef NDEBUG
template <class> struct ValueToString;
#endif

class ExclusionPolygon : public ExclusionShape {
    WTF_MAKE_NONCOPYABLE(ExclusionPolygon);
public:
    ExclusionPolygon(PassOwnPtr<Vector<FloatPoint> > vertices, WindRule fillRule);

    const FloatPoint& vertexAt(unsigned index) const { return (*m_vertices)[index]; }
    unsigned numberOfVertices() const { return m_vertices->size(); }
    WindRule fillRule() const { return m_fillRule; }

    const ExclusionPolygonEdge& edgeAt(unsigned index) const { return m_edges[index]; }
    unsigned numberOfEdges() const { return m_edges.size(); }

    virtual FloatRect shapeLogicalBoundingBox() const OVERRIDE { return internalToLogicalBoundingBox(m_boundingBox); }
    virtual bool isEmpty() const OVERRIDE { return m_empty; }
    virtual void getExcludedIntervals(float logicalTop, float logicalHeight, SegmentList&) const OVERRIDE;
    virtual void getIncludedIntervals(float logicalTop, float logicalHeight, SegmentList&) const OVERRIDE;
    virtual bool firstIncludedIntervalLogicalTop(float minLogicalIntervalTop, const FloatSize& minLogicalIntervalSize, float&) const OVERRIDE;

private:
    void computeXIntersections(float y, bool isMinY, Vector<ExclusionInterval>&) const;
    void computeEdgeIntersections(float minY, float maxY, Vector<ExclusionInterval>&) const;
    unsigned findNextEdgeVertexIndex(unsigned vertexIndex1, bool clockwise) const;

    typedef PODInterval<float, ExclusionPolygonEdge*> EdgeInterval;
    typedef PODIntervalTree<float, ExclusionPolygonEdge*> EdgeIntervalTree;

    OwnPtr<Vector<FloatPoint> > m_vertices;
    WindRule m_fillRule;
    FloatRect m_boundingBox;
    Vector<ExclusionPolygonEdge> m_edges;
    EdgeIntervalTree m_edgeTree;
    bool m_empty;
};

// EdgeIntervalTree nodes store minY, maxY, and a ("UserData") pointer to an ExclusionPolygonEdge. Edge vertex
// index1 is less than index2, except the last edge, where index2 is 0. When a polygon edge is defined by 3
// or more colinear vertices, index2 can be the the index of the last colinear vertex.
struct ExclusionPolygonEdge {
    const FloatPoint& vertex1() const
    {
        ASSERT(polygon);
        return polygon->vertexAt(vertexIndex1);
    }

    const FloatPoint& vertex2() const
    {
        ASSERT(polygon);
        return polygon->vertexAt(vertexIndex2);
    }

    const ExclusionPolygonEdge& previousEdge() const
    {
        ASSERT(polygon && polygon->numberOfEdges() > 1);
        return polygon->edgeAt((edgeIndex + polygon->numberOfEdges() - 1) % polygon->numberOfEdges());
    }

    const ExclusionPolygonEdge& nextEdge() const
    {
        ASSERT(polygon && polygon->numberOfEdges() > 1);
        return polygon->edgeAt((edgeIndex + 1) % polygon->numberOfEdges());
    }

    float minX() const { return std::min(vertex1().x(), vertex2().x()); }
    float minY() const { return std::min(vertex1().y(), vertex2().y()); }
    float maxX() const { return std::max(vertex1().x(), vertex2().x()); }
    float maxY() const { return std::max(vertex1().y(), vertex2().y()); }

    const ExclusionPolygon* polygon;
    unsigned vertexIndex1;
    unsigned vertexIndex2;
    unsigned edgeIndex;
};

// These structures are used by PODIntervalTree for debugging.1
#ifndef NDEBUG
template <> struct ValueToString<float> {
    static String string(const float value) { return String::number(value); }
};

template<> struct ValueToString<ExclusionPolygonEdge*> {
    static String string(const ExclusionPolygonEdge* edge) { return String::format("%p (%f,%f %f,%f)", edge, edge->vertex1().x(), edge->vertex1().y(), edge->vertex2().x(), edge->vertex2().y()); }
};
#endif

} // namespace WebCore

#endif // ExclusionPolygon_h
