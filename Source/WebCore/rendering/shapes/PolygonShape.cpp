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
#include "PolygonShape.h"

#include "ShapeInterval.h"
#include <wtf/MathExtras.h>

namespace WebCore {

enum EdgeIntersectionType {
    Normal,
    VertexMinY,
    VertexMaxY,
    VertexYBoth
};

struct EdgeIntersection {
    const FloatPolygonEdge* edge;
    FloatPoint point;
    EdgeIntersectionType type;
};

static bool computeXIntersection(const FloatPolygonEdge* edgePointer, float y, EdgeIntersection& result)
{
    const FloatPolygonEdge& edge = *edgePointer;

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

static inline FloatSize inwardEdgeNormal(const FloatPolygonEdge& edge)
{
    FloatSize edgeDelta = edge.vertex2() - edge.vertex1();
    if (!edgeDelta.width())
        return FloatSize((edgeDelta.height() > 0 ? -1 : 1), 0);
    if (!edgeDelta.height())
        return FloatSize(0, (edgeDelta.width() > 0 ? 1 : -1));
    float edgeLength = edgeDelta.diagonalLength();
    return FloatSize(-edgeDelta.height() / edgeLength, edgeDelta.width() / edgeLength);
}

static inline FloatSize outwardEdgeNormal(const FloatPolygonEdge& edge)
{
    return -inwardEdgeNormal(edge);
}

static inline void appendArc(Vector<FloatPoint>& vertices, const FloatPoint& arcCenter, float arcRadius, const FloatPoint& startArcVertex, const FloatPoint& endArcVertex, bool padding)
{
    float startAngle = atan2(startArcVertex.y() - arcCenter.y(), startArcVertex.x() - arcCenter.x());
    float endAngle = atan2(endArcVertex.y() - arcCenter.y(), endArcVertex.x() - arcCenter.x());
    const float twoPI = piFloat * 2;
    if (startAngle < 0)
        startAngle += twoPI;
    if (endAngle < 0)
        endAngle += twoPI;
    float angle = (startAngle > endAngle) ? (startAngle - endAngle) : (startAngle + twoPI - endAngle);
    const float arcSegmentCount = 6; // An even number so that one arc vertex will be eactly arcRadius from arcCenter.
    float arcSegmentAngle =  ((padding) ? -angle : twoPI - angle) / arcSegmentCount;

    vertices.append(startArcVertex);
    for (unsigned i = 1; i < arcSegmentCount; ++i) {
        float angle = startAngle + arcSegmentAngle * i;
        vertices.append(arcCenter + FloatPoint(cos(angle) * arcRadius, sin(angle) * arcRadius));
    }
    vertices.append(endArcVertex);
}

static inline void snapVerticesToLayoutUnitGrid(Vector<FloatPoint>& vertices)
{
    for (unsigned i = 0; i < vertices.size(); ++i)
        vertices[i].set(LayoutUnit(vertices[i].x()).toFloat(), LayoutUnit(vertices[i].y()).toFloat());
}

static inline PassOwnPtr<FloatPolygon> computeShapeMarginBounds(const FloatPolygon& polygon, float margin, WindRule fillRule)
{
    OwnPtr<Vector<FloatPoint>> marginVertices = adoptPtr(new Vector<FloatPoint>());
    FloatPoint intersection;

    for (unsigned i = 0; i < polygon.numberOfEdges(); ++i) {
        const FloatPolygonEdge& thisEdge = polygon.edgeAt(i);
        const FloatPolygonEdge& prevEdge = thisEdge.previousEdge();
        OffsetPolygonEdge thisOffsetEdge(thisEdge, outwardEdgeNormal(thisEdge) * margin);
        OffsetPolygonEdge prevOffsetEdge(prevEdge, outwardEdgeNormal(prevEdge) * margin);

        if (prevOffsetEdge.intersection(thisOffsetEdge, intersection))
            marginVertices->append(intersection);
        else
            appendArc(*marginVertices, thisEdge.vertex1(), margin, prevOffsetEdge.vertex2(), thisOffsetEdge.vertex1(), false);
    }

    snapVerticesToLayoutUnitGrid(*marginVertices);
    return adoptPtr(new FloatPolygon(marginVertices.release(), fillRule));
}


const FloatPolygon& PolygonShape::shapeMarginBounds() const
{
    ASSERT(shapeMargin() >= 0);
    if (!shapeMargin() || m_polygon.isEmpty())
        return m_polygon;

    if (!m_marginBounds)
        m_marginBounds = computeShapeMarginBounds(m_polygon, shapeMargin(), m_polygon.fillRule());

    return *m_marginBounds;
}

static inline bool getVertexIntersectionVertices(const EdgeIntersection& intersection, FloatPoint& prevVertex, FloatPoint& thisVertex, FloatPoint& nextVertex)
{
    if (intersection.type != VertexMinY && intersection.type != VertexMaxY)
        return false;

    ASSERT(intersection.edge && intersection.edge->polygon());
    const FloatPolygon& polygon = *(intersection.edge->polygon());
    const FloatPolygonEdge& thisEdge = *(intersection.edge);

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

static inline bool appendIntervalX(float x, bool inside, FloatShapeIntervals& result)
{
    if (!inside)
        result.append(FloatShapeInterval(x, x));
    else
        result.last().setX2(x);

    return !inside;
}

static bool compareEdgeIntersectionX(const EdgeIntersection& intersection1, const EdgeIntersection& intersection2)
{
    float x1 = intersection1.point.x();
    float x2 = intersection2.point.x();
    return (x1 == x2) ? intersection1.type < intersection2.type : x1 < x2;
}

static void computeXIntersections(const FloatPolygon& polygon, float y, bool isMinY, FloatShapeIntervals& result)
{
    Vector<const FloatPolygonEdge*> edges;
    if (!polygon.overlappingEdges(y, y, edges))
        return;

    Vector<EdgeIntersection> intersections;
    EdgeIntersection intersection;
    for (unsigned i = 0; i < edges.size(); ++i) {
        if (computeXIntersection(edges[i], y, intersection) && intersection.type != VertexYBoth)
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
                    ++index;
                }
                continue;
            }
        }

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

        if (edgeCrossing && polygon.fillRule() == RULE_NONZERO) {
            const FloatPolygonEdge& thisEdge = *thisIntersection.edge;
            windCount += (thisEdge.vertex2().y() > thisEdge.vertex1().y()) ? 1 : -1;
        }

        if (edgeCrossing && (!inside || !windCount))
            inside = appendIntervalX(thisIntersection.point.x(), inside, result);

        ++index;
    }
}

static bool compareX1(const FloatShapeInterval a, const FloatShapeInterval& b) { return a.x1() < b.x1(); }

static void sortAndMergeShapeIntervals(FloatShapeIntervals& intervals)
{
    std::sort(intervals.begin(), intervals.end(), compareX1);

    for (unsigned i = 1; i < intervals.size(); ) {
        const FloatShapeInterval& thisInterval = intervals[i];
        FloatShapeInterval& previousInterval = intervals[i - 1];
        if (thisInterval.overlaps(previousInterval)) {
            previousInterval.setX2(std::max<float>(previousInterval.x2(), thisInterval.x2()));
            intervals.remove(i);
        } else
            ++i;
    }
}

static void computeOverlappingEdgeXProjections(const FloatPolygon& polygon, float y1, float y2, FloatShapeIntervals& result)
{
    Vector<const FloatPolygonEdge*> edges;
    if (!polygon.overlappingEdges(y1, y2, edges))
        return;

    EdgeIntersection intersection;
    for (unsigned i = 0; i < edges.size(); ++i) {
        const FloatPolygonEdge *edge = edges[i];
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
            result.append(FloatShapeInterval(x1, x2));
    }

    sortAndMergeShapeIntervals(result);
}

void PolygonShape::getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const FloatPolygon& polygon = shapeMarginBounds();
    if (polygon.isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;

    FloatShapeIntervals y1XIntervals, y2XIntervals;
    computeXIntersections(polygon, y1, true, y1XIntervals);
    computeXIntersections(polygon, y2, false, y2XIntervals);

    FloatShapeIntervals mergedIntervals;
    FloatShapeInterval::uniteShapeIntervals(y1XIntervals, y2XIntervals, mergedIntervals);

    FloatShapeIntervals edgeIntervals;
    computeOverlappingEdgeXProjections(polygon, y1, y2, edgeIntervals);

    FloatShapeIntervals excludedIntervals;
    FloatShapeInterval::uniteShapeIntervals(mergedIntervals, edgeIntervals, excludedIntervals);

    for (unsigned i = 0; i < excludedIntervals.size(); ++i) {
        FloatShapeInterval interval = excludedIntervals[i];
        result.append(LineSegment(interval.x1(), interval.x2()));
    }
}

static void addPolygon(Path& path, const FloatPolygon& polygon)
{
    if (!polygon.numberOfVertices())
        return;

    path.moveTo(polygon.vertexAt(0));

    for (size_t i = 1; i < polygon.numberOfVertices(); i++)
        path.addLineTo(polygon.vertexAt(i));

    path.closeSubpath();
}

void PolygonShape::buildDisplayPaths(DisplayPaths& paths) const
{
    addPolygon(paths.shape, m_polygon);
    if (shapeMargin())
        addPolygon(paths.marginShape, shapeMarginBounds());
}

} // namespace WebCore
