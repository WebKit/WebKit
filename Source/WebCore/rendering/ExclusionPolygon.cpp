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

static bool compareEdgeMinY(const ExclusionPolygonEdge* e1, const ExclusionPolygonEdge* e2)
{
    return e1->minY() < e2->minY();
}

ExclusionPolygon::ExclusionPolygon(PassOwnPtr<Vector<FloatPoint> > vertices, WindRule fillRule)
    : ExclusionShape()
    , m_vertices(vertices)
    , m_fillRule(fillRule)
{
    unsigned nVertices = numberOfVertices();
    m_edges.resize(nVertices);
    m_empty = nVertices < 3;
    Vector<ExclusionPolygonEdge*> sortedEdgesMinY(nVertices);

    if (nVertices)
        m_boundingBox.setLocation(vertexAt(0));

    for (unsigned i = 0; i < nVertices; i++) {
        const FloatPoint& vertex = vertexAt(i);
        m_boundingBox.extend(vertex);
        m_edges[i].polygon = this;
        m_edges[i].index1 = i;
        m_edges[i].index2 = (i + 1) % nVertices;

        sortedEdgesMinY[i] = &m_edges[i];
    }

    std::sort(sortedEdgesMinY.begin(), sortedEdgesMinY.end(), WebCore::compareEdgeMinY);

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

float ExclusionPolygon::rightVertexY(unsigned index) const
{
    unsigned nVertices = numberOfVertices();
    const FloatPoint& vertex1 = vertexAt((index + 1) % nVertices);
    const FloatPoint& vertex2 = vertexAt((index - 1) % nVertices);

    if (vertex1.x() == vertex2.x())
        return vertex1.y() > vertex2.y() ? vertex1.y() : vertex2.y();
    return vertex1.x() > vertex2.x() ? vertex1.y() : vertex2.y();
}

static bool appendIntervalX(float x, bool inside, Vector<ExclusionInterval>& result)
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

void ExclusionPolygon::computeXIntersections(float y, Vector<ExclusionInterval>& result) const
{
    Vector<ExclusionPolygon::EdgeInterval> overlappingEdges;
    m_edgeTree.allOverlaps(ExclusionPolygon::EdgeInterval(y, y, 0), overlappingEdges);

    Vector<EdgeIntersection> intersections;
    EdgeIntersection intersection;
    for (unsigned i = 0; i < overlappingEdges.size(); i++) {
        ExclusionPolygonEdge* edge = static_cast<ExclusionPolygonEdge*>(overlappingEdges[i].data());
        if (computeXIntersection(edge, y, intersection))
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
                    // Replace pairs of intersections whose types are VertexMinY,VertexMaxY or VertexMaxY,VertexMinY with one VertexMinY intersection.
                    if (nextIntersection.type == VertexMaxY)
                        intersections[index + 1] = thisIntersection;
                    index++;
                }
                continue;
            }
        }

        const ExclusionPolygonEdge& thisEdge = *thisIntersection.edge;
        bool crossing = !windCount;

        if (fillRule() == RULE_EVENODD) {
            windCount += (thisEdge.vertex2().y() > thisEdge.vertex1().y()) ? 1 : -1;
            crossing = crossing || !windCount;
        }

        if ((thisIntersection.type == Normal) || (thisIntersection.type == VertexMinY)) {
            if (crossing)
                inside = appendIntervalX(thisIntersection.point.x(), inside, result);
        } else if (thisIntersection.type == VertexMaxY) {
            int vertexIndex = (thisEdge.vertex2().y() > thisEdge.vertex1().y()) ? thisEdge.index2 : thisEdge.index1;
            if (crossing && rightVertexY(vertexIndex) > y)
                inside = appendIntervalX(thisEdge.maxX(), inside, result);
        } else if (thisIntersection.type == VertexYBoth)
            result.append(ExclusionInterval(thisEdge.minX(), thisEdge.maxX()));

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

        result.append(ExclusionInterval(x1, x2));
    }

    sortExclusionIntervals(result);
}

void ExclusionPolygon::getExcludedIntervals(float logicalTop, float logicalHeight, SegmentList& result) const
{
    if (isEmpty())
        return;

    float y1 = minYForLogicalLine(logicalTop, logicalHeight);
    float y2 = maxYForLogicalLine(logicalTop, logicalHeight);

    Vector<ExclusionInterval> y1XIntervals, y2XIntervals;
    computeXIntersections(y1, y1XIntervals);
    computeXIntersections(y2, y2XIntervals);

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

    float y1 = minYForLogicalLine(logicalTop, logicalHeight);
    float y2 = maxYForLogicalLine(logicalTop, logicalHeight);

    Vector<ExclusionInterval> y1XIntervals, y2XIntervals;
    computeXIntersections(y1, y1XIntervals);
    computeXIntersections(y2, y2XIntervals);

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

} // namespace WebCore
