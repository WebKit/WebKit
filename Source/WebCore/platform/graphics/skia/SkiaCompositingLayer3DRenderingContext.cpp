/*
 * Copyright (C) 2024 Jani Hautakangas <jani@kodegood.com>
 * Copyright (C) 2026 Igalia S.L.
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
#include "SkiaCompositingLayer3DRenderingContext.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
#include "Polygon4D.h"
#include "SkiaCompositingLayer.h"
#include <limits>
#include <numeric>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPathBuilder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using PolygonZY = Vector<FloatPoint, 5>;

static inline PolygonZY projectPolygonToZYPlane(const Polygon4D& polygon)
{
    PolygonZY projected;
    projected.reserveInitialCapacity(polygon.numberOfVertices());
    for (unsigned i = 0; i < polygon.numberOfVertices(); ++i) {
        const auto& vertex = polygon.vertexAt(i);
        projected.append({ static_cast<float>(vertex.z), static_cast<float>(vertex.y) });
    }
    return projected;
}

// Given two points defining an edge, returns the perpendicular axis (normalized)
static inline FloatPoint edgeNormal(const FloatPoint& p1, const FloatPoint& p2)
{
    auto edge = p2 - p1;

    // A perpendicular to (x,y) is (y,-x)
    FloatPoint normal = { edge.height(), -edge.width() };
    normal.normalize();

    return normal;
}

// Project a convex polygon onto an axis and return min/max scalar values
static inline std::pair<float, float> projectPolygonOnAxis(const PolygonZY& polygon, const FloatPoint& axis)
{
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();
    for (const auto& vertex : polygon) {
        float distance = vertex.dot(axis);
        min = std::min(min, distance);
        max = std::max(max, distance);
    }
    return { min, max };
}

// Intersection check using Separating Axis Theorem
// For more information:
// https://en.wikipedia.org/wiki/Hyperplane_separation_theorem
static inline bool polygonsIntersect(const PolygonZY& polygonA, const PolygonZY& polygonB)
{
    auto hasSeparatingAxis = [&](const PolygonZY& polygon) {
        for (size_t i = 0; i < polygon.size(); ++i) {
            auto axis = edgeNormal(polygon[i], polygon[(i + 1) % polygon.size()]);

            auto [minA, maxA] = projectPolygonOnAxis(polygonA, axis);
            auto [minB, maxB] = projectPolygonOnAxis(polygonB, axis);

            // Check if two intervals [minA, maxA] and [minB, maxB] do not overlap
            if (maxA < minB || maxB < minA)
                return true;
        }
        return false;
    };

    return !hasSeparatingAxis(polygonA) && !hasSeparatingAxis(polygonB);
}

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(SkiaCompositingLayer3DRenderingContext::LayerNode);

void SkiaCompositingLayer3DRenderingContext::paint(Vector<Layer>&& layers, const std::function<void(SkiaCompositingLayer&, std::optional<SkPath>)>& paintLayerFunction)
{
    if (layers.isEmpty())
        return;

    // Perform a broad-phase sweep-and-prune to identify potential intersections.
    // By determining which layers might intersect, we can limit BSP cutting planes to those areas only,
    // preventing unnecessary splitting of layers that are spatially distant.
    // Reducing unnecessary splits helps improve performance, as each split layer fragment requires
    // an additional clip path and paint call.
    auto potentialIntersections = sweepAndPrune(layers);

    // Determine if any layer pairs intersect on the ZY plane. An intersection implies that the rendering
    // order is either ambiguous or overlapping, necessitating the use of a BSP tree for proper ordering.
    bool hasIntersections = false;
    for (const auto& p : potentialIntersections) {
        auto polygonA = projectPolygonToZYPlane(layers[p.first].geometry);
        auto polygonB = projectPolygonToZYPlane(layers[p.second].geometry);
        if (polygonsIntersect(polygonA, polygonB)) {
            hasIntersections = true;
            break;
        }
    }

    // If no intersections are detected, the BSP tree building process is skipped and a fast path is taken.
    // Other scenarios are not currently considered for optimization.
    if (!hasIntersections) {
        // Sort back to front
        std::sort(layers.begin(), layers.end(), [](const Layer& layerA, const Layer& layerB) {
            return layerA.boundingBox.min.z() < layerB.boundingBox.min.z();
        });

        for (const auto& layer : layers)
            paintLayerFunction(layer.compositingLayer.get(), std::nullopt);
        return;
    }

    Deque<Layer> layerDeque;
    for (auto& layer : layers)
        layerDeque.append(WTF::move(layer));

    auto root = makeUnique<LayerNode>(layerDeque.takeFirst());
    buildTree(*root, layerDeque);

    auto buildClipPath = [](const Layer& layer) -> std::optional<SkPath> {
        if (layer.isSplit == Layer::IsSplit::No)
            return std::nullopt;

        unsigned numVertices = layer.geometry.numberOfVertices();

        // minimumW matches Skia's perspective clip threshold (kW0PlaneDistance in SkPathPriv.h). A vertex
        // below it lies on the camera plane, so instead of dividing, it is moved 2^20 pixels along the
        // direction of its x and y. That is beyond any canvas, and small enough to stay precise in float.
        static constexpr double minimumW = 1.0 / (1 << 14);
        static constexpr double beyondAnyCanvas = 1 << 20;
        auto project = [&](unsigned index) {
            const auto& vertex = layer.geometry.vertexAt(index);
            if (vertex.w > minimumW)
                return SkPoint::Make(vertex.x / vertex.w, vertex.y / vertex.w);

            const double length = std::hypot(vertex.x, vertex.y);
            if (!length)
                return SkPoint::Make(0, 0);

            return SkPoint::Make(vertex.x / length * beyondAnyCanvas, vertex.y / length * beyondAnyCanvas);
        };

        SkPathBuilder builder;
        builder.moveTo(project(0));
        for (unsigned i = 1; i < numVertices; ++i)
            builder.lineTo(project(i));
        builder.close();

        return builder.detach();
    };

    // Paint in BSP order, building SkPath clip paths for split layers.
    traverseTree(*root, [&paintLayerFunction, &buildClipPath](LayerNode& node) {
        for (const auto& layer : node.layers)
            paintLayerFunction(layer.compositingLayer.get(), buildClipPath(layer));
    });
}

SkiaCompositingLayer3DRenderingContext::BoundingBox SkiaCompositingLayer3DRenderingContext::computeBoundingBox(const Polygon4D& polygon)
{
    ASSERT(polygon.numberOfVertices() >= 3);

    auto corner = [&](unsigned index) {
        const auto& vertex = polygon.vertexAt(index);
        return FloatPoint3D(vertex.x, vertex.y, vertex.z);
    };

    auto minCorner = corner(0);
    auto maxCorner = minCorner;

    for (unsigned i = 1; i < polygon.numberOfVertices(); i++) {
        auto point = corner(i);
        minCorner.setX(std::min(minCorner.x(), point.x()));
        minCorner.setY(std::min(minCorner.y(), point.y()));
        minCorner.setZ(std::min(minCorner.z(), point.z()));

        maxCorner.setX(std::max(maxCorner.x(), point.x()));
        maxCorner.setY(std::max(maxCorner.y(), point.y()));
        maxCorner.setZ(std::max(maxCorner.z(), point.z()));
    }

    return { minCorner, maxCorner };
}

SkiaCompositingLayer3DRenderingContext::SweepAndPrunePairs SkiaCompositingLayer3DRenderingContext::sweepAndPrune(const Vector<Layer>& layers)
{
    Vector<size_t> indices(layers.size());
    std::iota(indices.begin(), indices.end(), 0);

    // Sort left to right along axis
    std::sort(indices.begin(), indices.end(), [&layers](size_t a, size_t b) {
        return layers[a].boundingBox.min.x() < layers[b].boundingBox.min.x();
    });

    SweepAndPrunePairs potentialIntersectionPairs;
    for (size_t i = 0; i < indices.size(); ++i) {
        for (size_t j = i + 1; j < indices.size(); ++j) {
            auto firstIndex = indices[i];
            auto secondIndex = indices[j];

            // Check overlap on sorted X-axis
            if (layers[secondIndex].boundingBox.min.x() >= layers[firstIndex].boundingBox.max.x())
                break; // No further overlap possible

            // Check overlap on Y-axis
            if (layers[firstIndex].boundingBox.min.y() >= layers[secondIndex].boundingBox.max.y()
                || layers[firstIndex].boundingBox.max.y() <= layers[secondIndex].boundingBox.min.y())
                continue;

            // Check overlap on Z-axis
            if (layers[firstIndex].boundingBox.min.z() >= layers[secondIndex].boundingBox.max.z()
                || layers[firstIndex].boundingBox.max.z() <= layers[secondIndex].boundingBox.min.z())
                continue;

            // Ensure canonical order (smaller index first)
            if (firstIndex > secondIndex)
                std::swap(firstIndex, secondIndex);

            potentialIntersectionPairs.add({ firstIndex, secondIndex });
        }
    }
    return potentialIntersectionPairs;
}

// Build BSP tree for rendering layers with painter's algorithm.
// For more information:
// https://en.wikipedia.org/wiki/Binary_space_partitioning
void SkiaCompositingLayer3DRenderingContext::buildTree(LayerNode& root, Deque<Layer>& layers)
{
    if (layers.isEmpty())
        return;

    const auto& rootPlane = root.firstLayer().geometry.plane();

    Deque<Layer> backList, frontList;
    for (auto& layer : layers) {
        switch (classifyLayer(layer, rootPlane)) {
        case LayerPosition::InFront:
            frontList.append(WTF::move(layer));
            break;
        case LayerPosition::Behind:
            backList.append(WTF::move(layer));
            break;
        case LayerPosition::Coplanar:
            root.layers.append(WTF::move(layer));
            break;
        case LayerPosition::Intersecting:
            auto [backGeometry, frontGeometry] = layer.geometry.split(rootPlane);
            if (backGeometry.numberOfVertices() > 2)
                backList.append(Layer(layer.compositingLayer.copyRef(), WTF::move(backGeometry), Layer::IsSplit::Yes));
            if (frontGeometry.numberOfVertices() > 2)
                frontList.append(Layer(layer.compositingLayer.copyRef(), WTF::move(frontGeometry), Layer::IsSplit::Yes));
            break;
        }
    }

    if (!frontList.isEmpty()) {
        root.frontNode = makeUnique<LayerNode>(frontList.takeFirst());
        buildTree(*root.frontNode, frontList);
    }

    if (!backList.isEmpty()) {
        root.backNode = makeUnique<LayerNode>(backList.takeFirst());
        buildTree(*root.backNode, backList);
    }
}

void SkiaCompositingLayer3DRenderingContext::traverseTree(LayerNode& node, const std::function<void(LayerNode&)>& processNode)
{
    const auto& plane = node.firstLayer().geometry.plane();

    auto* frontNode = node.frontNode.get();
    auto* backNode = node.backNode.get();

    // if polygon is facing away from camera then swap nodes to reverse
    // the traversal order
    if (plane.z < 0)
        std::swap(frontNode, backNode);

    if (backNode)
        traverseTree(*backNode, processNode);

    processNode(node);

    if (frontNode)
        traverseTree(*frontNode, processNode);
}

SkiaCompositingLayer3DRenderingContext::LayerPosition SkiaCompositingLayer3DRenderingContext::classifyLayer(const Layer& layer, const Point4D& plane)
{
    int inFrontCount = 0;
    int behindCount = 0;
    for (unsigned i = 0; i < layer.geometry.numberOfVertices(); ++i) {
        const auto& vertex = layer.geometry.vertexAt(i);
        double distance = signedDistanceToPlane(plane, vertex);
        const double tolerance = Polygon4D::planeTolerance * vertex.w;

        if (distance > tolerance)
            inFrontCount++;
        else if (distance < -tolerance)
            behindCount++;
    }

    if (inFrontCount > 0 && behindCount > 0)
        return LayerPosition::Intersecting;
    if (inFrontCount > 0)
        return LayerPosition::InFront;
    if (behindCount > 0)
        return LayerPosition::Behind;
    return LayerPosition::Coplanar;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
