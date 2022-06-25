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

#pragma once

#include "FloatPoint.h"
#include "FloatRect.h"
#include "PODIntervalTree.h"
#include "WindRule.h"
#include <memory>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPolygonEdge;

class FloatPolygon {
public:
    FloatPolygon(Vector<FloatPoint>&& vertices, WindRule fillRule);

    const FloatPoint& vertexAt(unsigned index) const { return m_vertices[index]; }
    unsigned numberOfVertices() const { return m_vertices.size(); }

    WindRule fillRule() const { return m_fillRule; }

    const FloatPolygonEdge& edgeAt(unsigned index) const { return m_edges[index]; }
    unsigned numberOfEdges() const { return m_edges.size(); }

    FloatRect boundingBox() const { return m_boundingBox; }
    Vector<std::reference_wrapper<const FloatPolygonEdge>> overlappingEdges(float minY, float maxY) const;
    bool contains(const FloatPoint&) const;
    bool isEmpty() const { return m_empty; }

private:
    using EdgeIntervalTree = PODIntervalTree<float, FloatPolygonEdge*>;

    bool containsNonZero(const FloatPoint&) const;
    bool containsEvenOdd(const FloatPoint&) const;

    Vector<FloatPoint> m_vertices;
    WindRule m_fillRule;
    FloatRect m_boundingBox;
    bool m_empty;
    Vector<FloatPolygonEdge> m_edges;
    EdgeIntervalTree m_edgeTree; // Each EdgeIntervalTree node stores minY, maxY, and a ("UserData") pointer to a FloatPolygonEdge.

};

class VertexPair {
public:
    virtual ~VertexPair() = default;

    virtual const FloatPoint& vertex1() const = 0;
    virtual const FloatPoint& vertex2() const = 0;

    float minX() const { return std::min(vertex1().x(), vertex2().x()); }
    float minY() const { return std::min(vertex1().y(), vertex2().y()); }
    float maxX() const { return std::max(vertex1().x(), vertex2().x()); }
    float maxY() const { return std::max(vertex1().y(), vertex2().y()); }

    bool overlapsRect(const FloatRect&) const;
    bool intersection(const VertexPair&, FloatPoint&) const;
};

class FloatPolygonEdge : public VertexPair {
    friend class FloatPolygon;
public:
    const FloatPoint& vertex1() const override
    {
        ASSERT(m_polygon);
        return m_polygon->vertexAt(m_vertexIndex1);
    }

    const FloatPoint& vertex2() const override
    {
        ASSERT(m_polygon);
        return m_polygon->vertexAt(m_vertexIndex2);
    }

    const FloatPolygonEdge& previousEdge() const
    {
        ASSERT(m_polygon && m_polygon->numberOfEdges() > 1);
        return m_polygon->edgeAt((m_edgeIndex + m_polygon->numberOfEdges() - 1) % m_polygon->numberOfEdges());
    }

    const FloatPolygonEdge& nextEdge() const
    {
        ASSERT(m_polygon && m_polygon->numberOfEdges() > 1);
        return m_polygon->edgeAt((m_edgeIndex + 1) % m_polygon->numberOfEdges());
    }

    const FloatPolygon* polygon() const { return m_polygon; }
    unsigned vertexIndex1() const { return m_vertexIndex1; }
    unsigned vertexIndex2() const { return m_vertexIndex2; }
    unsigned edgeIndex() const { return m_edgeIndex; }

private:
    // Edge vertex index1 is less than index2, except the last edge, where index2 is 0. When a polygon edge
    // is defined by 3 or more colinear vertices, index2 can be the index of the last colinear vertex.
    unsigned m_vertexIndex1;
    unsigned m_vertexIndex2;
    unsigned m_edgeIndex;
    const FloatPolygon* m_polygon;
};

#ifndef NDEBUG
TextStream& operator<<(TextStream&, const FloatPolygonEdge&);
#endif

} // namespace WebCore
