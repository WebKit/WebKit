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

#include "config.h"
#include "ExclusionPolygon.h"

#include <wtf/MathExtras.h>

namespace WebCore {

enum EdgeIntersectionType {
    Normal,
    VertexMinY,
    VertexMaxY,
    VertexYBoth
};

struct EdgeIntersection {
    const ExclusionPolygonEdge* edge;
    FloatPoint point;
    EdgeIntersectionType type;
};

static inline float determinant(const FloatSize& a, const FloatSize& b)
{
    return a.width() * b.height() - a.height() * b.width();
}

static inline bool areCollinearPoints(const FloatPoint& p0, const FloatPoint& p1, const FloatPoint& p2)
{
    return !determinant(p1 - p0, p2 - p0);
}

static inline bool areCoincidentPoints(const FloatPoint& p0, const FloatPoint& p1)
{
    return p0.x() == p1.x() && p0.y() == p1.y();
}

static inline unsigned nextVertexIndex(unsigned vertexIndex, unsigned nVertices, bool clockwise)
{
    return ((clockwise) ? vertexIndex + 1 : vertexIndex - 1 + nVertices) % nVertices;
}

unsigned ExclusionPolygon::findNextEdgeVertexIndex(unsigned vertexIndex1, bool clockwise) const
{
    unsigned nVertices = numberOfVertices();
    unsigned vertexIndex2 = nextVertexIndex(vertexIndex1, nVertices, clockwise);

    while (vertexIndex2 && areCoincidentPoints(vertexAt(vertexIndex1), vertexAt(vertexIndex2)))
        vertexIndex2 = nextVertexIndex(vertexIndex2, nVertices, clockwise);

    while (vertexIndex2) {
        unsigned vertexIndex3 = nextVertexIndex(vertexIndex2, nVertices, clockwise);
        if (!areCollinearPoints(vertexAt(vertexIndex1), vertexAt(vertexIndex2), vertexAt(vertexIndex3)))
            break;
        vertexIndex2 = vertexIndex3;
    }

    return vertexIndex2;
}

ExclusionPolygon::ExclusionPolygon(PassOwnPtr<Vector<FloatPoint> > vertices, WindRule fillRule)
    : ExclusionShape()
    , m_vertices(vertices)
    , m_fillRule(fillRule)
{
    unsigned nVertices = numberOfVertices();
    m_edges.resize(nVertices);
    m_empty = nVertices < 3;

    if (nVertices)
        m_boundingBox.setLocation(vertexAt(0));

    if (m_empty)
        return;

    unsigned minVertexIndex = 0;
    for (unsigned i = 1; i < nVertices; ++i) {
        const FloatPoint& vertex = vertexAt(i);
        if (vertex.y() < vertexAt(minVertexIndex).y() || (vertex.y() == vertexAt(minVertexIndex).y() && vertex.x() < vertexAt(minVertexIndex).x()))
            minVertexIndex = i;
    }
    FloatPoint nextVertex = vertexAt((minVertexIndex + 1) % nVertices);
    FloatPoint prevVertex = vertexAt((minVertexIndex + nVertices - 1) % nVertices);
    bool clockwise = determinant(vertexAt(minVertexIndex) - prevVertex, nextVertex - prevVertex) > 0;

    unsigned edgeIndex = 0;
    unsigned vertexIndex1 = 0;
    do {
        m_boundingBox.extend(vertexAt(vertexIndex1));
        unsigned vertexIndex2 = findNextEdgeVertexIndex(vertexIndex1, clockwise);
        m_edges[edgeIndex].m_polygon = this;
        m_edges[edgeIndex].m_vertexIndex1 = vertexIndex1;
        m_edges[edgeIndex].m_vertexIndex2 = vertexIndex2;
        m_edges[edgeIndex].m_edgeIndex = edgeIndex;
        edgeIndex++;
        vertexIndex1 = vertexIndex2;
    } while (vertexIndex1);

    if (edgeIndex > 3) {
        const ExclusionPolygonEdge& firstEdge = m_edges[0];
        const ExclusionPolygonEdge& lastEdge = m_edges[edgeIndex - 1];
        if (areCollinearPoints(lastEdge.vertex1(), lastEdge.vertex2(), firstEdge.vertex2())) {
            m_edges[0].m_vertexIndex1 = lastEdge.m_vertexIndex1;
            edgeIndex--;
        }
    }

    m_edges.resize(edgeIndex);
    m_empty = m_edges.size() < 3;

    if (m_empty)
        return;

    for (unsigned i = 0; i < m_edges.size(); i++) {
        ExclusionPolygonEdge* edge = &m_edges[i];
        m_edgeTree.add(EdgeInterval(edge->minY(), edge->maxY(), edge));
    }
}

static bool computeXIntersection(const ExclusionPolygonEdge* edgePointer, float y, EdgeIntersection& result)
{
    const ExclusionPolygonEdge& edge = *edgePointer;

    if (edge.minY() > y || edge.maxY() < y)
        return false;

    const FloatPoint& vertex1 = edge.vertex1();
    const FloatPoint& vertex2 = edge.vertex2();
    float dy = vertex2.y() - vertex1.y();

    float intersectionX;
    EdgeIntersectionType intersectionType;

    if (!dy) {
        intersectionType = VertexYBoth;
        intersectionX = edge.minX();
    } else if (y == edge.minY()) {
        intersectionType = VertexMinY;
        intersectionX = (vertex1.y() < vertex2.y()) ? vertex1.x() : vertex2.x();
    } else if (y == edge.maxY()) {
        intersectionType = VertexMaxY;
        intersectionX = (vertex1.y() > vertex2.y()) ? vertex1.x() : vertex2.x();
    } else {
        intersectionType = Normal;
        intersectionX = ((y - vertex1.y()) * (vertex2.x() - vertex1.x()) / dy) + vertex1.x();
    }

    result.edge = edgePointer;
    result.type = intersectionType;
    result.point.set(intersectionX, y);

    return true;
}

static inline bool getVertexIntersectionVertices(const EdgeIntersection& intersection, FloatPoint& prevVertex, FloatPoint& thisVertex, FloatPoint& nextVertex)
{
    if (intersection.type != VertexMinY && intersection.type != VertexMaxY)
        return false;

    ASSERT(intersection.edge && intersection.edge->polygon());
    const ExclusionPolygon& polygon = *(intersection.edge->polygon());
    const ExclusionPolygonEdge& thisEdge = *(intersection.edge);

    if ((intersection.type == VertexMinY && (thisEdge.vertex1().y() < thisEdge.vertex2().y()))
        || (intersection.type == VertexMaxY && (thisEdge.vertex1().y() > thisEdge.vertex2().y()))) {
        prevVertex = polygon.vertexAt(thisEdge.previousEdge().vertexIndex1());
        thisVertex = polygon.vertexAt(thisEdge.vertexIndex1());
        nextVertex = polygon.vertexAt(thisEdge.vertexIndex2());
    } else {
        prevVertex = polygon.vertexAt(thisEdge.vertexIndex1());
        thisVertex = polygon.vertexAt(thisEdge.vertexIndex2());
        nextVertex = polygon.vertexAt(thisEdge.nextEdge().vertexIndex2());
    }

    return true;
}

static inline bool appendIntervalX(float x, bool inside, Vector<ExclusionInterval>& result)
{
    if (!inside)
        result.append(ExclusionInterval(x));
    else
        result[result.size() - 1].x2 = x;

    return !inside;
}

static bool compareEdgeIntersectionX(const EdgeIntersection& intersection1, const EdgeIntersection& intersection2)
{
    float x1 = intersection1.point.x();
    float x2 = intersection2.point.x();
    return (x1 == x2) ? intersection1.type < intersection2.type : x1 < x2;
}

void ExclusionPolygon::computeXIntersections(float y, bool isMinY, Vector<ExclusionInterval>& result) const
{
    Vector<ExclusionPolygon::EdgeInterval> overlappingEdges;
    m_edgeTree.allOverlaps(ExclusionPolygon::EdgeInterval(y, y, 0), overlappingEdges);

    Vector<EdgeIntersection> intersections;
    EdgeIntersection intersection;
    for (unsigned i = 0; i < overlappingEdges.size(); i++) {
        ExclusionPolygonEdge* edge = static_cast<ExclusionPolygonEdge*>(overlappingEdges[i].data());
        if (computeXIntersection(edge, y, intersection) && intersection.type != VertexYBoth)
            intersections.append(intersection);
    }
    if (intersections.size() < 2)
        return;

    std::sort(intersections.begin(), intersections.end(), WebCore::compareEdgeIntersectionX);

    unsigned index = 0;
    int windCount = 0;
    bool inside = false;

    while (index < intersections.size()) {
        const EdgeIntersection& thisIntersection = intersections[index];
        if (index + 1 < intersections.size()) {
            const EdgeIntersection& nextIntersection = intersections[index + 1];
            if ((thisIntersection.point.x() == nextIntersection.point.x()) && (thisIntersection.type == VertexMinY || thisIntersection.type == VertexMaxY)) {
                if (thisIntersection.type == nextIntersection.type) {
                    // Skip pairs of intersections whose types are VertexMaxY,VertexMaxY and VertexMinY,VertexMinY.
                    index += 2;
                } else {
                    // Replace pairs of intersections whose types are VertexMinY,VertexMaxY or VertexMaxY,VertexMinY with one intersection.
                    index++;
                }
                continue;
            }
        }

        const ExclusionPolygonEdge& thisEdge = *thisIntersection.edge;
        bool evenOddCrossing = !windCount;

        if (fillRule() == RULE_EVENODD) {
            windCount += (thisEdge.vertex2().y() > thisEdge.vertex1().y()) ? 1 : -1;
            evenOddCrossing = evenOddCrossing || !windCount;
        }

        if (evenOddCrossing) {
            bool edgeCrossing = thisIntersection.type == Normal;
            if (!edgeCrossing) {
                FloatPoint prevVertex;
                FloatPoint thisVertex;
                FloatPoint nextVertex;

                if (getVertexIntersectionVertices(thisIntersection, prevVertex, thisVertex, nextVertex)) {
                    if (nextVertex.y() == y)
                        edgeCrossing = (isMinY) ? prevVertex.y() > y : prevVertex.y() < y;
                    else if (prevVertex.y() == y)
                        edgeCrossing = (isMinY) ? nextVertex.y() > y : nextVertex.y() < y;
                    else
                        edgeCrossing = true;
                }
            }
            if (edgeCrossing)
                inside = appendIntervalX(thisIntersection.point.x(), inside, result);
        }

        index++;
    }
}

// Return the X projections of the edges that overlap y1,y2.
void ExclusionPolygon::computeEdgeIntersections(float y1, float y2, Vector<ExclusionInterval>& result) const
{
    Vector<ExclusionPolygon::EdgeInterval> overlappingEdges;
    m_edgeTree.allOverlaps(ExclusionPolygon::EdgeInterval(y1, y2, 0), overlappingEdges);

    EdgeIntersection intersection;
    for (unsigned i = 0; i < overlappingEdges.size(); i++) {
        const ExclusionPolygonEdge *edge = static_cast<ExclusionPolygonEdge*>(overlappingEdges[i].data());
        float x1;
        float x2;

        if (edge->minY() < y1) {
            computeXIntersection(edge, y1, intersection);
            x1 = intersection.point.x();
        } else
            x1 = (edge->vertex1().y() < edge->vertex2().y()) ? edge->vertex1().x() : edge->vertex2().x();

        if (edge->maxY() > y2) {
            computeXIntersection(edge, y2, intersection);
            x2 = intersection.point.x();
        } else
            x2 = (edge->vertex1().y() > edge->vertex2().y()) ? edge->vertex1().x() : edge->vertex2().x();

        if (x1 > x2)
            std::swap(x1, x2);

        if (x2 > x1)
            result.append(ExclusionInterval(x1, x2));
    }

    sortExclusionIntervals(result);
}

void ExclusionPolygon::getExcludedIntervals(float logicalTop, float logicalHeight, SegmentList& result) const
{
    if (isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = y1 + logicalHeight;

    Vector<ExclusionInterval> y1XIntervals, y2XIntervals;
    computeXIntersections(y1, true, y1XIntervals);
    computeXIntersections(y2, false, y2XIntervals);

    Vector<ExclusionInterval> mergedIntervals;
    mergeExclusionIntervals(y1XIntervals, y2XIntervals, mergedIntervals);

    Vector<ExclusionInterval> edgeIntervals;
    computeEdgeIntersections(y1, y2, edgeIntervals);

    Vector<ExclusionInterval> excludedIntervals;
    mergeExclusionIntervals(mergedIntervals, edgeIntervals, excludedIntervals);

    for (unsigned i = 0; i < excludedIntervals.size(); i++) {
        ExclusionInterval interval = excludedIntervals[i];
        result.append(LineSegment(interval.x1, interval.x2));
    }
}

void ExclusionPolygon::getIncludedIntervals(float logicalTop, float logicalHeight, SegmentList& result) const
{
    if (isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = y1 + logicalHeight;

    Vector<ExclusionInterval> y1XIntervals, y2XIntervals;
    computeXIntersections(y1, true, y1XIntervals);
    computeXIntersections(y2, false, y2XIntervals);

    Vector<ExclusionInterval> commonIntervals;
    intersectExclusionIntervals(y1XIntervals, y2XIntervals, commonIntervals);

    Vector<ExclusionInterval> edgeIntervals;
    computeEdgeIntersections(y1, y2, edgeIntervals);

    Vector<ExclusionInterval> includedIntervals;
    subtractExclusionIntervals(commonIntervals, edgeIntervals, includedIntervals);

    for (unsigned i = 0; i < includedIntervals.size(); i++) {
        ExclusionInterval interval = includedIntervals[i];
        result.append(LineSegment(interval.x1, interval.x2));
    }
}

bool ExclusionPolygon::firstIncludedIntervalLogicalTop(float minLogicalIntervalTop, const FloatSize& minLogicalIntervalSize, float& result) const
{
    // FIXME: This is just a temporary stub, https://bugs.webkit.org/show_bug.cgi?id=103429

    if (minLogicalIntervalSize.width() > m_boundingBox.width())
        return false;

    float minY = std::max(m_boundingBox.y(), minLogicalIntervalTop);
    float maxY = minY + minLogicalIntervalSize.height();
    if (minY < m_boundingBox.y() || maxY > m_boundingBox.maxY())
        return false;

    result = minLogicalIntervalTop;
    return true;
}

} // namespace WebCore
