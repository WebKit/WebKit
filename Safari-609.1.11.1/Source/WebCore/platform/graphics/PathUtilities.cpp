/*
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */


#include "config.h"
#include "PathUtilities.h"

#include "AffineTransform.h"
#include "BorderData.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

class FloatPointGraph {
    WTF_MAKE_NONCOPYABLE(FloatPointGraph);
public:
    FloatPointGraph() { }

    class Node : public FloatPoint {
        WTF_MAKE_NONCOPYABLE(Node);
    public:
        Node(FloatPoint point)
            : FloatPoint(point)
        { }

        const Vector<Node*>& nextPoints() const { return m_nextPoints; }
        void addNextPoint(Node* node)
        {
            if (!m_nextPoints.contains(node))
                m_nextPoints.append(node);
        }

        bool isVisited() const { return m_visited; }
        void visit() { m_visited = true; }

        void reset() { m_visited = false; m_nextPoints.clear(); }

    private:
        Vector<Node*> m_nextPoints;
        bool m_visited { false };
    };

    typedef std::pair<Node*, Node*> Edge;
    typedef Vector<Edge> Polygon;

    Node* findOrCreateNode(FloatPoint);

    void reset()
    {
        for (auto& node : m_allNodes)
            node->reset();
    }

private:
    Vector<std::unique_ptr<Node>> m_allNodes;
};

FloatPointGraph::Node* FloatPointGraph::findOrCreateNode(FloatPoint point)
{
    for (auto& testNode : m_allNodes) {
        if (areEssentiallyEqual(*testNode, point))
            return testNode.get();
    }

    m_allNodes.append(makeUnique<FloatPointGraph::Node>(point));
    return m_allNodes.last().get();
}

static bool findLineSegmentIntersection(const FloatPointGraph::Edge& edgeA, const FloatPointGraph::Edge& edgeB, FloatPoint& intersectionPoint)
{
    if (!findIntersection(*edgeA.first, *edgeA.second, *edgeB.first, *edgeB.second, intersectionPoint))
        return false;

    FloatPoint edgeAVec(*edgeA.second - *edgeA.first);
    FloatPoint edgeBVec(*edgeB.second - *edgeB.first);

    float dotA = edgeAVec.dot(toFloatPoint(intersectionPoint - *edgeA.first));
    if (dotA < 0 || dotA > edgeAVec.lengthSquared())
        return false;

    float dotB = edgeBVec.dot(toFloatPoint(intersectionPoint - *edgeB.first));
    if (dotB < 0 || dotB > edgeBVec.lengthSquared())
        return false;

    return true;
}

static bool addIntersectionPoints(Vector<FloatPointGraph::Polygon>& polys, FloatPointGraph& graph)
{
    bool foundAnyIntersections = false;

    Vector<FloatPointGraph::Edge> allEdges;
    for (auto& poly : polys)
        allEdges.appendVector(poly);

    for (const FloatPointGraph::Edge& edgeA : allEdges) {
        Vector<FloatPointGraph::Node*> intersectionPoints({edgeA.first, edgeA.second});

        for (const FloatPointGraph::Edge& edgeB : allEdges) {
            if (&edgeA == &edgeB)
                continue;

            FloatPoint intersectionPoint;
            if (!findLineSegmentIntersection(edgeA, edgeB, intersectionPoint))
                continue;
            foundAnyIntersections = true;
            intersectionPoints.append(graph.findOrCreateNode(intersectionPoint));
        }

        std::sort(intersectionPoints.begin(), intersectionPoints.end(), [edgeA](auto* a, auto* b) {
            return FloatPoint(*edgeA.first - *b).lengthSquared() > FloatPoint(*edgeA.first - *a).lengthSquared();
        });

        for (unsigned pointIndex = 1; pointIndex < intersectionPoints.size(); pointIndex++)
            intersectionPoints[pointIndex - 1]->addNextPoint(intersectionPoints[pointIndex]);
    }

    return foundAnyIntersections;
}

static FloatPointGraph::Polygon walkGraphAndExtractPolygon(FloatPointGraph::Node* startNode)
{
    FloatPointGraph::Polygon outPoly;

    FloatPointGraph::Node* currentNode = startNode;
    FloatPointGraph::Node* previousNode = startNode;

    do {
        currentNode->visit();

        FloatPoint currentVec(*previousNode - *currentNode);
        currentVec.normalize();

        // Walk the graph, at each node choosing the next non-visited
        // point with the greatest internal angle.
        FloatPointGraph::Node* nextNode = nullptr;
        float nextNodeAngle = 0;
        for (auto* potentialNextNode : currentNode->nextPoints()) {
            if (potentialNextNode == currentNode)
                continue;

            // If we can get back to the start, we should, ignoring the fact that we already visited it.
            // Otherwise we'll head inside the shape.
            if (potentialNextNode == startNode) {
                nextNode = startNode;
                break;
            }

            if (potentialNextNode->isVisited())
                continue;

            FloatPoint nextVec(*potentialNextNode - *currentNode);
            nextVec.normalize();

            float angle = acos(nextVec.dot(currentVec));
            float crossZ = nextVec.x() * currentVec.y() - nextVec.y() * currentVec.x();

            if (crossZ < 0)
                angle = (2 * piFloat) - angle;

            if (!nextNode || angle > nextNodeAngle) {
                nextNode = potentialNextNode;
                nextNodeAngle = angle;
            }
        }

        // If we don't end up at a node adjacent to the starting node,
        // something went wrong (there's probably a hole in the shape),
        // so bail out. We'll use a bounding box instead.
        if (!nextNode)
            return FloatPointGraph::Polygon();

        outPoly.append(std::make_pair(currentNode, nextNode));

        previousNode = currentNode;
        currentNode = nextNode;
    } while (currentNode != startNode);

    return outPoly;
}

static FloatPointGraph::Node* findUnvisitedPolygonStartPoint(Vector<FloatPointGraph::Polygon>& polys)
{
    for (auto& poly : polys) {
        for (auto& edge : poly) {
            if (edge.first->isVisited() || edge.second->isVisited())
                goto nextPolygon;
        }

        // FIXME: We should make sure we find an outside edge to start with.
        return poly[0].first;
    nextPolygon:
        continue;
    }
    return nullptr;
}

static Vector<FloatPointGraph::Polygon> unitePolygons(Vector<FloatPointGraph::Polygon>& polys, FloatPointGraph& graph)
{
    graph.reset();

    // There are no intersections, so the polygons are disjoint (we already removed wholly-contained rects in an earlier step).
    if (!addIntersectionPoints(polys, graph))
        return polys;

    Vector<FloatPointGraph::Polygon> unitedPolygons;

    while (FloatPointGraph::Node* startNode = findUnvisitedPolygonStartPoint(polys)) {
        FloatPointGraph::Polygon unitedPolygon = walkGraphAndExtractPolygon(startNode);
        if (unitedPolygon.isEmpty())
            return Vector<FloatPointGraph::Polygon>();
        unitedPolygons.append(unitedPolygon);
    }

    return unitedPolygons;
}

static FloatPointGraph::Polygon edgesForRect(FloatRect rect, FloatPointGraph& graph)
{
    auto minMin = graph.findOrCreateNode(rect.minXMinYCorner());
    auto minMax = graph.findOrCreateNode(rect.minXMaxYCorner());
    auto maxMax = graph.findOrCreateNode(rect.maxXMaxYCorner());
    auto maxMin = graph.findOrCreateNode(rect.maxXMinYCorner());

    return FloatPointGraph::Polygon({
        std::make_pair(minMin, maxMin),
        std::make_pair(maxMin, maxMax),
        std::make_pair(maxMax, minMax),
        std::make_pair(minMax, minMin)
    });
}

static Vector<FloatPointGraph::Polygon> polygonsForRect(const Vector<FloatRect>& rects, FloatPointGraph& graph)
{
    Vector<FloatRect> sortedRects = rects;
    std::sort(sortedRects.begin(), sortedRects.end(), [](FloatRect a, FloatRect b) { return b.y() > a.y(); });

    Vector<FloatPointGraph::Polygon> rectPolygons;
    rectPolygons.reserveInitialCapacity(sortedRects.size());

    for (auto& rect : sortedRects) {
        bool isContained = false;
        for (auto& otherRect : sortedRects) {
            if (&rect == &otherRect)
                continue;
            if (otherRect.contains(rect)) {
                isContained = true;
                break;
            }
        }

        if (!isContained)
            rectPolygons.uncheckedAppend(edgesForRect(rect, graph));
    }
    return unitePolygons(rectPolygons, graph);
}

Vector<Path> PathUtilities::pathsWithShrinkWrappedRects(const Vector<FloatRect>& rects, float radius)
{
    Vector<Path> paths;

    if (rects.isEmpty())
        return paths;

    if (rects.size() > 20) {
        Path path;
        for (const auto& rect : rects)
            path.addRoundedRect(rect, FloatSize(radius, radius));
        paths.append(path);
        return paths;
    }

    FloatPointGraph graph;
    Vector<FloatPointGraph::Polygon> polys = polygonsForRect(rects, graph);
    if (polys.isEmpty()) {
        Path path;
        for (const auto& rect : rects)
            path.addRoundedRect(rect, FloatSize(radius, radius));
        paths.append(path);
        return paths;
    }

    for (auto& poly : polys) {
        Path path;
        for (unsigned i = 0; i < poly.size(); ++i) {
            FloatPointGraph::Edge& toEdge = poly[i];
            // Connect the first edge to the last.
            FloatPointGraph::Edge& fromEdge = (i > 0) ? poly[i - 1] : poly[poly.size() - 1];

            FloatPoint fromEdgeVec = toFloatPoint(*fromEdge.second - *fromEdge.first);
            FloatPoint toEdgeVec = toFloatPoint(*toEdge.second - *toEdge.first);

            // Clamp the radius to no more than half the length of either adjacent edge,
            // because we want a smooth curve and don't want unequal radii.
            float clampedRadius = std::min(radius, fabsf(fromEdgeVec.x() ? fromEdgeVec.x() : fromEdgeVec.y()) / 2);
            clampedRadius = std::min(clampedRadius, fabsf(toEdgeVec.x() ? toEdgeVec.x() : toEdgeVec.y()) / 2);

            FloatPoint fromEdgeNorm = fromEdgeVec;
            fromEdgeNorm.normalize();
            FloatPoint toEdgeNorm = toEdgeVec;
            toEdgeNorm.normalize();

            // Project the radius along the incoming and outgoing edge.
            FloatSize fromOffset = clampedRadius * toFloatSize(fromEdgeNorm);
            FloatSize toOffset = clampedRadius * toFloatSize(toEdgeNorm);

            if (!i)
                path.moveTo(*fromEdge.second - fromOffset);
            else
                path.addLineTo(*fromEdge.second - fromOffset);
            path.addArcTo(*fromEdge.second, *toEdge.first + toOffset, clampedRadius);
        }
        path.closeSubpath();
        paths.append(path);
    }
    return paths;
}

Path PathUtilities::pathWithShrinkWrappedRects(const Vector<FloatRect>& rects, float radius)
{
    Vector<Path> paths = pathsWithShrinkWrappedRects(rects, radius);

    Path unionPath;
    for (const auto& path : paths)
        unionPath.addPath(path, AffineTransform());

    return unionPath;
}

static std::pair<FloatPoint, FloatPoint> startAndEndPointsForCorner(const FloatPointGraph::Edge& fromEdge, const FloatPointGraph::Edge& toEdge, const FloatSize& radius)
{
    FloatPoint startPoint;
    FloatPoint endPoint;
    
    FloatSize fromEdgeVector = *fromEdge.second - *fromEdge.first;
    FloatSize toEdgeVector = *toEdge.second - *toEdge.first;

    FloatPoint fromEdgeNorm = toFloatPoint(fromEdgeVector);
    fromEdgeNorm.normalize();
    FloatSize fromOffset = FloatSize(radius.width() * fromEdgeNorm.x(), radius.height() * fromEdgeNorm.y());
    startPoint = *fromEdge.second - fromOffset;

    FloatPoint toEdgeNorm = toFloatPoint(toEdgeVector);
    toEdgeNorm.normalize();
    FloatSize toOffset = FloatSize(radius.width() * toEdgeNorm.x(), radius.height() * toEdgeNorm.y());
    endPoint = *toEdge.first + toOffset;
    return std::make_pair(startPoint, endPoint);
}

enum class CornerType { TopLeft, TopRight, BottomRight, BottomLeft, Other };
static CornerType cornerType(const FloatPointGraph::Edge& fromEdge, const FloatPointGraph::Edge& toEdge)
{
    auto fromEdgeVector = *fromEdge.second - *fromEdge.first;
    auto toEdgeVector = *toEdge.second - *toEdge.first;

    if (fromEdgeVector.height() < 0 && toEdgeVector.width() > 0)
        return CornerType::TopLeft;
    if (fromEdgeVector.width() > 0 && toEdgeVector.height() > 0)
        return CornerType::TopRight;
    if (fromEdgeVector.height() > 0 && toEdgeVector.width() < 0)
        return CornerType::BottomRight;
    if (fromEdgeVector.width() < 0 && toEdgeVector.height() < 0)
        return CornerType::BottomLeft;
    return CornerType::Other;
}

static CornerType cornerTypeForMultiline(const FloatPointGraph::Edge& fromEdge, const FloatPointGraph::Edge& toEdge, const Vector<FloatPoint>& corners)
{
    auto corner = cornerType(fromEdge, toEdge);
    if (corner == CornerType::TopLeft && corners.at(0) == *fromEdge.second)
        return corner;
    if (corner == CornerType::TopRight && corners.at(1) == *fromEdge.second)
        return corner;
    if (corner == CornerType::BottomRight && corners.at(2) == *fromEdge.second)
        return corner;
    if (corner == CornerType::BottomLeft && corners.at(3) == *fromEdge.second)
        return corner;
    return CornerType::Other;
}

static std::pair<FloatPoint, FloatPoint> controlPointsForBezierCurve(CornerType cornerType, const FloatPointGraph::Edge& fromEdge,
    const FloatPointGraph::Edge& toEdge, const FloatSize& radius)
{
    FloatPoint cp1;
    FloatPoint cp2;
    switch (cornerType) {
    case CornerType::TopLeft: {
        cp1 = FloatPoint(fromEdge.second->x(), fromEdge.second->y() + radius.height() * Path::circleControlPoint());
        cp2 = FloatPoint(toEdge.first->x() + radius.width() * Path::circleControlPoint(), toEdge.first->y());
        break;
    }
    case CornerType::TopRight: {
        cp1 = FloatPoint(fromEdge.second->x() - radius.width() * Path::circleControlPoint(), fromEdge.second->y());
        cp2 = FloatPoint(toEdge.first->x(), toEdge.first->y() + radius.height() * Path::circleControlPoint());
        break;
    }
    case CornerType::BottomRight: {
        cp1 = FloatPoint(fromEdge.second->x(), fromEdge.second->y() - radius.height() * Path::circleControlPoint());
        cp2 = FloatPoint(toEdge.first->x() - radius.width() * Path::circleControlPoint(), toEdge.first->y());
        break;
    }
    case CornerType::BottomLeft: {
        cp1 = FloatPoint(fromEdge.second->x() + radius.width() * Path::circleControlPoint(), fromEdge.second->y());
        cp2 = FloatPoint(toEdge.first->x(), toEdge.first->y() - radius.height() * Path::circleControlPoint());
        break;
    }
    case CornerType::Other: {
        ASSERT_NOT_REACHED();
        break;
    }
    }
    return std::make_pair(cp1, cp2);
}

static FloatRoundedRect::Radii adjustedtRadiiForHuggingCurve(const FloatSize& topLeftRadius, const FloatSize& topRightRadius,
    const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius, float outlineOffset)
{
    FloatRoundedRect::Radii radii;
    // This adjusts the radius so that it follows the border curve even when offset is present.
    auto adjustedRadius = [outlineOffset](const FloatSize& radius)
    {
        FloatSize expandSize;
        if (radius.width() > outlineOffset)
            expandSize.setWidth(std::min(outlineOffset, radius.width() - outlineOffset));
        if (radius.height() > outlineOffset)
            expandSize.setHeight(std::min(outlineOffset, radius.height() - outlineOffset));
        FloatSize adjustedRadius = radius;
        adjustedRadius.expand(expandSize.width(), expandSize.height());
        // Do not go to negative radius.
        return adjustedRadius.expandedTo(FloatSize(0, 0));
    };

    radii.setTopLeft(adjustedRadius(topLeftRadius));
    radii.setTopRight(adjustedRadius(topRightRadius));
    radii.setBottomRight(adjustedRadius(bottomRightRadius));
    radii.setBottomLeft(adjustedRadius(bottomLeftRadius));
    return radii;
}
    
static Optional<FloatRect> rectFromPolygon(const FloatPointGraph::Polygon& poly)
{
    if (poly.size() != 4)
        return Optional<FloatRect>();

    Optional<FloatPoint> topLeft;
    Optional<FloatPoint> bottomRight;
    for (unsigned i = 0; i < poly.size(); ++i) {
        const auto& toEdge = poly[i];
        const auto& fromEdge = (i > 0) ? poly[i - 1] : poly[poly.size() - 1];
        auto corner = cornerType(fromEdge, toEdge);
        if (corner == CornerType::TopLeft) {
            ASSERT(!topLeft);
            topLeft = *fromEdge.second;
        } else if (corner == CornerType::BottomRight) {
            ASSERT(!bottomRight);
            bottomRight = *fromEdge.second;
        }
    }
    if (!topLeft || !bottomRight)
        return Optional<FloatRect>();
    return FloatRect(topLeft.value(), bottomRight.value());
}

Path PathUtilities::pathWithShrinkWrappedRectsForOutline(const Vector<FloatRect>& rects, const BorderData& borderData,
    float outlineOffset, TextDirection direction, WritingMode writingMode, float deviceScaleFactor)
{
    ASSERT(borderData.hasBorderRadius());

    FloatSize topLeftRadius { borderData.topLeftRadius().width.value(), borderData.topLeftRadius().height.value() };
    FloatSize topRightRadius { borderData.topRightRadius().width.value(), borderData.topRightRadius().height.value() };
    FloatSize bottomRightRadius { borderData.bottomRightRadius().width.value(), borderData.bottomRightRadius().height.value() };
    FloatSize bottomLeftRadius { borderData.bottomLeftRadius().width.value(), borderData.bottomLeftRadius().height.value() };

    auto roundedRect = [topLeftRadius, topRightRadius, bottomRightRadius, bottomLeftRadius, outlineOffset, deviceScaleFactor] (const FloatRect& rect)
    {
        auto radii = adjustedtRadiiForHuggingCurve(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius, outlineOffset);
        radii.scale(calcBorderRadiiConstraintScaleFor(rect, radii));
        RoundedRect roundedRect(LayoutRect(rect),
            RoundedRect::Radii(LayoutSize(radii.topLeft()), LayoutSize(radii.topRight()), LayoutSize(radii.bottomLeft()), LayoutSize(radii.bottomRight())));
        Path path;
        path.addRoundedRect(roundedRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
        return path;
    };

    if (rects.size() == 1)
        return roundedRect(rects.at(0));

    FloatPointGraph graph;
    const auto polys = polygonsForRect(rects, graph);
    // Fall back to corner painting with no radius for empty and disjoint rectangles.
    if (!polys.size() || polys.size() > 1)
        return Path();
    const auto& poly = polys.at(0);
    // Fast path when poly has one rect only.
    Optional<FloatRect> rect = rectFromPolygon(poly);
    if (rect)
        return roundedRect(rect.value());

    Path path;
    // Multiline outline needs to match multiline border painting. Only first and last lines are getting rounded borders.
    auto isLeftToRight = isLeftToRightDirection(direction);
    auto firstLineRect = isLeftToRight ? rects.at(0) : rects.at(rects.size() - 1);
    auto lastLineRect = isLeftToRight ? rects.at(rects.size() - 1) : rects.at(0);
    // Adjust radius so that it matches the box border.
    auto firstLineRadii = FloatRoundedRect::Radii(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
    auto lastLineRadii = FloatRoundedRect::Radii(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
    firstLineRadii.scale(calcBorderRadiiConstraintScaleFor(firstLineRect, firstLineRadii));
    lastLineRadii.scale(calcBorderRadiiConstraintScaleFor(lastLineRect, lastLineRadii));
    topLeftRadius = firstLineRadii.topLeft();
    bottomLeftRadius = firstLineRadii.bottomLeft();
    topRightRadius = lastLineRadii.topRight();
    bottomRightRadius = lastLineRadii.bottomRight();
    Vector<FloatPoint> corners;
    // physical topLeft/topRight/bottomRight/bottomLeft
    auto isHorizontal = isHorizontalWritingMode(writingMode);
    corners.append(firstLineRect.minXMinYCorner());
    corners.append(isHorizontal ? lastLineRect.maxXMinYCorner() : firstLineRect.maxXMinYCorner());
    corners.append(lastLineRect.maxXMaxYCorner());
    corners.append(isHorizontal ? firstLineRect.minXMaxYCorner() : lastLineRect.minXMaxYCorner());

    for (unsigned i = 0; i < poly.size(); ++i) {
        auto moveOrAddLineTo = [i, &path] (const FloatPoint& startPoint)
        {
            if (!i)
                path.moveTo(startPoint);
            else
                path.addLineTo(startPoint);
        };
        const auto& toEdge = poly[i];
        const auto& fromEdge = (i > 0) ? poly[i - 1] : poly[poly.size() - 1];
        FloatSize radius;
        auto corner = cornerTypeForMultiline(fromEdge, toEdge, corners);
        switch (corner) {
        case CornerType::TopLeft: {
            radius = topLeftRadius;
            break;
        }
        case CornerType::TopRight: {
            radius = topRightRadius;
            break;
        }
        case CornerType::BottomRight: {
            radius = bottomRightRadius;
            break;
        }
        case CornerType::BottomLeft: {
            radius = bottomLeftRadius;
            break;
        }
        case CornerType::Other: {
            // Do not apply border radius on corners that normal border painting skips. (multiline content)
            moveOrAddLineTo(*fromEdge.second);
            continue;
        }
        }
        auto [startPoint, endPoint] = startAndEndPointsForCorner(fromEdge, toEdge, radius);
        moveOrAddLineTo(startPoint);

        auto [cp1, cp2] = controlPointsForBezierCurve(corner, fromEdge, toEdge, radius);
        path.addBezierCurveTo(cp1, cp2, endPoint);
    }
    path.closeSubpath();
    return path;
}


}
