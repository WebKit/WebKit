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

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "FloatPlane3D.h"
#include "FloatPolygon3D.h"
#include "FloatQuad.h"
#include "GeometryUtilities.h"
#include "SkiaCompositingLayer.h"
#include <numeric>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkM44.h>
#include <skia/core/SkPathBuilder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

static inline FloatQuad projectPolygonToZYPlane(const FloatPolygon3D& polygon)
{
    auto p1 = polygon.vertexAt(0);
    auto p2 = polygon.vertexAt(1);
    auto p3 = polygon.vertexAt(2);
    auto p4 = polygon.vertexAt(3);
    return { { p1.z(), p1.y() }, { p2.z(), p2.y() }, { p3.z(), p3.y() }, { p4.z(), p4.y() } };
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
static inline std::pair<float, float> projectQuadOnAxis(const FloatQuad& quad, const FloatPoint& axis)
{
    float p1 = quad.p1().dot(axis);
    float p2 = quad.p2().dot(axis);
    float p3 = quad.p3().dot(axis);
    float p4 = quad.p4().dot(axis);

    float min = min4(p1, p2, p3, p4);
    float max = max4(p1, p2, p3, p4);

    return { min, max };
}

// Intersection check using Separating Axis Theorem
// For more information:
// https://en.wikipedia.org/wiki/Hyperplane_separation_theorem
static inline bool quadsIntersect(const FloatQuad& quadA, const FloatQuad& quadB)
{
    std::array<FloatPoint, 8> axes;

    // QuadA edges: (1->2), (2->3), (3->4), (4->0)
    axes[0] = edgeNormal(quadA.p1(), quadA.p2());
    axes[1] = edgeNormal(quadA.p2(), quadA.p3());
    axes[2] = edgeNormal(quadA.p3(), quadA.p4());
    axes[3] = edgeNormal(quadA.p4(), quadA.p1());

    // QuadB edges: (1->2), (2->3), (3->4), (4->0)
    axes[4] = edgeNormal(quadB.p1(), quadB.p2());
    axes[5] = edgeNormal(quadB.p2(), quadB.p3());
    axes[6] = edgeNormal(quadB.p3(), quadB.p4());
    axes[7] = edgeNormal(quadB.p4(), quadB.p1());

    for (auto& axis : axes) {
        auto [minA, maxA] = projectQuadOnAxis(quadA, axis);
        auto [minB, maxB] = projectQuadOnAxis(quadB, axis);

        // Check if two intervals [minA, maxA] and [minB, maxB] do not overlap
        if (maxA < minB || maxB < minA)
            return false; // Separating axis found
    }

    return true; // No separating axis found
}

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(SkiaCompositingLayer3DRenderingContext::LayerNode);

void SkiaCompositingLayer3DRenderingContext::paint(const Vector<Ref<SkiaCompositingLayer>>& compositingLayers, const std::function<void(SkiaCompositingLayer&, std::optional<SkPath>)>& paintLayerFunction)
{
    if (compositingLayers.isEmpty())
        return;

    Vector<Layer> layers;
    for (auto& compositingLayer : compositingLayers) {
        FloatPolygon3D geometry(compositingLayer->effectiveLayerRect(), compositingLayer->toSurfaceTransform());
        BoundingBox boundingBox = computeBoundingBox(geometry);
        layers.append({ geometry, boundingBox, compositingLayer });
    }

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
        auto quadA = projectPolygonToZYPlane(layers[p.first].geometry);
        auto quadB = projectPolygonToZYPlane(layers[p.second].geometry);
        if (quadsIntersect(quadA, quadB)) {
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

        for (auto& layer : layers)
            paintLayerFunction(layer.compositingLayer.get(), std::nullopt);
        return;
    }

    Deque<Layer> layerDeque;
    for (auto& layer : layers)
        layerDeque.append(WTF::move(layer));
    layers.clear();

    auto root = makeUnique<LayerNode>(layerDeque.takeFirst());
    buildTree(*root, layerDeque);

    auto buildClipPath = [](const Layer& layer) -> std::optional<SkPath> {
        auto toLayerTransform = layer.compositingLayer->toSurfaceTransform().inverse();
        unsigned numVertices = layer.geometry.numberOfVertices();
        if (!toLayerTransform || numVertices < 3)
            return std::nullopt;

        SkPathBuilder builder;
        auto v = toLayerTransform->mapPoint(layer.geometry.vertexAt(0));
        builder.moveTo(v.x(), v.y());
        for (unsigned i = 1; i < numVertices; ++i) {
            v = toLayerTransform->mapPoint(layer.geometry.vertexAt(i));
            builder.lineTo(v.x(), v.y());
        }
        builder.close();

        return builder.detach().makeTransform(SkM44(layer.compositingLayer->toSurfaceTransform()).asM33());
    };

    // Paint in BSP order, building SkPath clip paths for split layers.
    traverseTree(*root, [&paintLayerFunction, &buildClipPath](LayerNode& node) {
        for (auto& layer : node.layers) {
            if (!layer.isSplit) {
                paintLayerFunction(layer.compositingLayer.get(), std::nullopt);
                continue;
            }

            paintLayerFunction(layer.compositingLayer.get(), buildClipPath(layer));
        }
    });
}

SkiaCompositingLayer3DRenderingContext::BoundingBox SkiaCompositingLayer3DRenderingContext::computeBoundingBox(const FloatPolygon3D& polygon)
{
    auto minCorner = polygon.vertexAt(0);
    auto maxCorner = polygon.vertexAt(0);

    for (unsigned i = 1; i < polygon.numberOfVertices(); i++) {
        auto point = polygon.vertexAt(i);
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

    auto& rootGeometry = root.firstLayer().geometry;
    FloatPlane3D rootPlane(rootGeometry.normal(), rootGeometry.vertexAt(0));

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
                backList.append({ backGeometry, { }, layer.compositingLayer, true });
            if (frontGeometry.numberOfVertices() > 2)
                frontList.append({ frontGeometry, { }, layer.compositingLayer, true });
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
    auto& geometry = node.firstLayer().geometry;
    FloatPlane3D plane(geometry.normal(), geometry.vertexAt(0));

    auto* frontNode = node.frontNode.get();
    auto* backNode = node.backNode.get();

    // if polygon is facing away from camera then swap nodes to reverse
    // the traversal order
    if (plane.normal().z() < 0)
        std::swap(frontNode, backNode);

    if (backNode)
        traverseTree(*backNode, processNode);

    processNode(node);

    if (frontNode)
        traverseTree(*frontNode, processNode);
}

SkiaCompositingLayer3DRenderingContext::LayerPosition SkiaCompositingLayer3DRenderingContext::classifyLayer(const Layer& layer, const FloatPlane3D& plane)
{
    const float epsilon = 0.05f; // Tolerance for intersection check

    int inFrontCount = 0;
    int behindCount = 0;
    for (unsigned i = 0; i < layer.geometry.numberOfVertices(); ++i) {
        const auto& vertex = layer.geometry.vertexAt(i);
        float distance = plane.distanceToPoint(vertex);

        if (distance > epsilon)
            inFrontCount++;
        else if (distance < -epsilon)
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

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
