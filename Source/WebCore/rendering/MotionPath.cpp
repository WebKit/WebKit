/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "MotionPath.h"

#include "BorderShape.h"
#include "GeometryUtilities.h"
#include "PathOperation.h"
#include "PathTraversalState.h"
#include "RenderBlock.h"
#include "RenderStyleInlines.h"
#include "TransformOperationData.h"
#include "TransformationMatrix.h"

namespace WebCore {

static FloatPoint offsetFromContainer(const RenderObject& renderer, RenderBlock& container, const FloatRect& referenceRect)
{
    auto offsetFromContainingBlock = renderer.offsetFromContainer(container, LayoutPoint());
    return FloatPoint(FloatPoint(offsetFromContainingBlock) - referenceRect.location());
}

static FloatRoundedRect containingBlockRectForRenderer(const RenderObject& renderer, RenderBlock& container, const PathOperation& operation)
{
    auto referenceRect = container.referenceBoxRect(operation.referenceBox());
    if (is<BoxPathOperation>(operation)) {
        auto borderShape = BorderShape::shapeForBorderRect(container.style(), LayoutRect(referenceRect));
        return borderShape.deprecatedPixelSnappedRoundedRect(container.document().deviceScaleFactor());
    }

    auto snappedRect = snapRectToDevicePixelsIfNeeded(container.referenceBoxRect(operation.referenceBox()), downcast<RenderLayerModelObject>(renderer));
    return FloatRoundedRect(snappedRect);
}

static FloatPoint normalPositionForOffsetPath(PathOperation* operation, const FloatRect& referenceRect)
{
    if (is<RayPathOperation>(operation) || is<ShapePathOperation>(operation))
        return referenceRect.center();
    return { };
}

std::optional<MotionPathData> MotionPath::motionPathDataForRenderer(const RenderElement& renderer)
{
    auto pathOperation = renderer.style().offsetPath();
    if (!is<RenderLayerModelObject>(renderer) || !pathOperation)
        return std::nullopt;

    auto* shapeOperation = dynamicDowncast<ShapePathOperation>(pathOperation);
    if (shapeOperation && std::holds_alternative<Style::PathFunction>(shapeOperation->shape()))
        return std::nullopt;

    auto startingPositionForOffsetPosition = [&](const LengthPoint& offsetPosition, const FloatRect& referenceRect, RenderBlock& container) -> FloatPoint {
        // If offset-position is normal, the element does not have an offset starting position.
        if (offsetPosition.x.isNormal())
            return normalPositionForOffsetPath(pathOperation, referenceRect);
        // If offset-position is auto, use top / left corner of the box.
        if (offsetPosition.x.isAuto())
            return offsetFromContainer(renderer, container, referenceRect);
        return floatPointForLengthPoint(offsetPosition, referenceRect.size());
    };

    auto* container = renderer.containingBlock();
    if (!container)
        return std::nullopt;

    MotionPathData data;
    data.containingBlockBoundingRect = containingBlockRectForRenderer(renderer, *container, *pathOperation);
    data.offsetFromContainingBlock = offsetFromContainer(renderer, *container, data.containingBlockBoundingRect.rect());

    auto offsetPosition = renderer.style().offsetPosition();
    if (is<ShapePathOperation>(pathOperation))
        data.usedStartingPosition = startingPositionForOffsetPosition(offsetPosition, data.containingBlockBoundingRect.rect(), *container);

    if (auto* rayPathOperation = dynamicDowncast<RayPathOperation>(pathOperation)) {
        auto startingPosition = rayPathOperation->ray()->position;
        data.usedStartingPosition = startingPosition ? Style::evaluate(*startingPosition, data.containingBlockBoundingRect.rect().size()) : startingPositionForOffsetPosition(offsetPosition, data.containingBlockBoundingRect.rect(), *container);
    }

    return data;
}

static PathTraversalState traversalStateAtDistance(const Path& path, const Length& distance)
{
    auto pathLength = path.length();
    auto distanceValue = floatValueForLength(distance, pathLength);

    float resolvedLength = 0;
    if (path.isClosed()) {
        if (pathLength) {
            resolvedLength = fmod(distanceValue, pathLength);
            if (resolvedLength < 0)
                resolvedLength += pathLength;
        }
    } else
        resolvedLength = clampTo<float>(distanceValue, 0, pathLength);

    ASSERT(resolvedLength >= 0);
    return path.traversalStateAtLength(resolvedLength);
}

void MotionPath::applyMotionPathTransform(TransformationMatrix& matrix, const TransformOperationData& transformData, const FloatPoint& transformOrigin, const PathOperation& offsetPath, const LengthPoint& offsetAnchor, const Length& offsetDistance, const OffsetRotation& offsetRotate, TransformBox transformBox)
{
    auto boundingBox = transformData.boundingBox;
    auto anchor = transformOrigin;
    if (!offsetAnchor.x.isAuto())
        anchor = floatPointForLengthPoint(offsetAnchor, boundingBox.size()) + boundingBox.location();

    // Shift element to the point on path specified by offset-path and offset-distance.
    auto path = offsetPath.getPath(transformData);
    if (!path)
        return;

    auto traversalState = traversalStateAtDistance(*path, offsetDistance);
    matrix.translate(traversalState.current().x(), traversalState.current().y());

    auto shiftToOrigin = anchor - transformOrigin;

    // Adjust anchor for SVG.
    if (transformData.isSVGRenderer && transformBox != TransformBox::ViewBox)
        anchor += boundingBox.location();

    // Shift element to the anchor specified by offset-anchor.
    matrix.translate(-anchor.x(), -anchor.y());

    matrix.translate(shiftToOrigin.width(), shiftToOrigin.height());

    // Apply rotation.
    auto& rotation = offsetRotate;
    if (rotation.hasAuto())
        matrix.rotate(traversalState.normalAngle() + rotation.angle());
    else
        matrix.rotate(rotation.angle());

    matrix.translate(-shiftToOrigin.width(), -shiftToOrigin.height());
}

void MotionPath::applyMotionPathTransform(const RenderStyle& style, const TransformOperationData& transformData, TransformationMatrix& matrix)
{
    auto* offsetPath = style.offsetPath();
    if (!offsetPath)
        return;

    auto transformOrigin = style.computeTransformOrigin(transformData.boundingBox).xy();
    applyMotionPathTransform(matrix, transformData, transformOrigin, *offsetPath, style.offsetAnchor(), style.offsetDistance(), style.offsetRotate(), style.transformBox());
}

bool MotionPath::needsUpdateAfterContainingBlockLayout(const PathOperation& pathOperation)
{
    return is<RayPathOperation>(pathOperation) || is<BoxPathOperation>(pathOperation) || is<ShapePathOperation>(pathOperation);
}

static double lengthForRayPath(const Style::Ray& ray, const MotionPathData& data)
{
    auto& boundingBox = data.containingBlockBoundingRect.rect();
    auto distances = distanceOfPointToSidesOfRect(boundingBox, data.usedStartingPosition);

    return WTF::switchOn(ray.size,
        [&](Style::ClosestSide) {
            return std::min( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
        },
        [&](Style::FarthestSide) {
            return std::max( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
        },
        [&](Style::FarthestCorner) {
            return std::hypot(std::max(distances.left(), distances.right()), std::max(distances.top(), distances.bottom()));
        },
        [&](Style::ClosestCorner) {
            return std::hypot(std::min(distances.left(), distances.right()), std::min(distances.top(), distances.bottom()));
        },
        [&](Style::Sides) {
            return lengthOfRayIntersectionWithBoundingBox(boundingBox, std::make_pair(data.usedStartingPosition, ray.angle.value));
        }
    );
}

static double lengthForRayContainPath(const FloatRect& elementRect, double computedPathLength)
{
    return std::max(0.0, computedPathLength - (std::max(elementRect.width(), elementRect.height()) / 2));
}

static FloatPoint currentOffsetForData(const MotionPathData& data)
{
    return FloatPoint(data.usedStartingPosition - data.offsetFromContainingBlock);
}

std::optional<Path> MotionPath::computePathForRay(const RayPathOperation& rayPathOperation, const TransformOperationData& transformData)
{
    auto motionPathData = transformData.motionPathData;
    if (!motionPathData || motionPathData->containingBlockBoundingRect.rect().isZero())
        return std::nullopt;

    auto elementBoundingBox = transformData.boundingBox;
    double length = lengthForRayPath(*rayPathOperation.ray(), *motionPathData);
    if (rayPathOperation.ray()->contain)
        length = lengthForRayContainPath(elementBoundingBox, length);

    auto radians = deg2rad(toPositiveAngle(rayPathOperation.ray()->angle.value) - 90.0);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);

    Path path;
    path.moveTo(currentOffsetForData(*motionPathData));
    path.addLineTo(currentOffsetForData(*motionPathData) + point);
    return path;
}

static FloatRoundedRect offsetRectForData(const MotionPathData& data)
{
    auto rect = data.containingBlockBoundingRect;
    auto shiftedPoint = data.offsetFromContainingBlock;
    shiftedPoint.scale(-1);
    rect.setLocation(shiftedPoint);
    return rect;
}

std::optional<Path> MotionPath::computePathForBox(const BoxPathOperation&, const TransformOperationData& transformData)
{
    if (auto motionPathData = transformData.motionPathData) {
        Path path;
        path.addRoundedRect(offsetRectForData(*motionPathData), PathRoundedRect::Strategy::PreferBezier);
        return path;
    }
    return std::nullopt;
}

std::optional<Path> MotionPath::computePathForShape(const ShapePathOperation& pathOperation, const TransformOperationData& transformData)
{
    if (auto motionPathData = transformData.motionPathData) {
        auto containingBlockRect = offsetRectForData(*motionPathData).rect();
        return WTF::switchOn(pathOperation.shape(),
            [&]<Style::ShapeWithCenterCoordinate T>(const T& shape) -> std::optional<Path> {
                if (!shape->position)
                    return Style::pathForCenterCoordinate(*shape, containingBlockRect, motionPathData->usedStartingPosition);
                return Style::path(shape, containingBlockRect);
            },
            [&](const auto& shape) -> std::optional<Path> {
                return Style::path(shape, containingBlockRect);
            }
        );
    }
    return pathOperation.pathForReferenceRect(transformData.boundingBox);
}

} // namespace WebCore
