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
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "TransformOperationData.h"
#include "TransformationMatrix.h"

namespace WebCore {

static FloatPoint offsetFromContainer(const RenderObject& renderer, RenderBlock& container, const FloatRect& referenceRect)
{
    auto offsetFromContainingBlock = renderer.offsetFromContainer(container, LayoutPoint());
    return FloatPoint(FloatPoint(offsetFromContainingBlock) - referenceRect.location());
}

static FloatRoundedRect containingBlockRectForRenderer(const RenderObject& renderer, RenderBlock& container, const Style::OffsetPath& offsetPath)
{
    return WTF::switchOn(offsetPath,
        [&](const Style::BoxPath& offsetPath) -> FloatRoundedRect {
            auto referenceBox = offsetPath.referenceBox();
            auto referenceRect = container.referenceBoxRect(referenceBox);
            auto borderShape = BorderShape::shapeForBorderRect(container.style(), LayoutRect(referenceRect));
            return borderShape.deprecatedPixelSnappedRoundedRect(protect(container.document())->deviceScaleFactor());
        },
        [&](const auto& offsetPath) -> FloatRoundedRect {
            auto referenceBox = offsetPath.referenceBox();
            auto snappedRect = snapRectToDevicePixelsIfNeeded(container.referenceBoxRect(referenceBox), downcast<RenderLayerModelObject>(renderer));
            return FloatRoundedRect { snappedRect };
        },
        [&](const CSS::Keyword::None&) -> FloatRoundedRect {
            RELEASE_ASSERT_NOT_REACHED();
        }
    );
}

static FloatPoint NODELETE normalPositionForOffsetPath(const Style::OffsetPath& offsetPath, const FloatRect& referenceRect)
{
    if (WTF::holdsAlternative<Style::RayPath>(offsetPath) || WTF::holdsAlternative<Style::BasicShapePath>(offsetPath))
        return referenceRect.center();
    return { };
}

std::optional<MotionPathData> MotionPath::motionPathDataForRenderer(const RenderElement& renderer)
{
    if (!is<RenderLayerModelObject>(renderer))
        return std::nullopt;

    auto& offsetPath = renderer.style().offsetPath();
    bool canBuildMotionPathData = WTF::switchOn(offsetPath,
        [](const CSS::Keyword::None&) {
            return false;
        },
        [](const Style::BasicShapePath& offsetPath) {
            return !std::holds_alternative<Style::PathFunction>(offsetPath.shape());
        },
        [](const auto&) {
            return true;
        }
    );
    if (!canBuildMotionPathData)
        return std::nullopt;

    auto startingPositionForOffsetPosition = [&](const Style::OffsetPosition& offsetPosition, const FloatRect& referenceRect, RenderBlock& container, Style::ZoomFactor zoom) -> FloatPoint {
        return WTF::switchOn(offsetPosition,
            [&](const CSS::Keyword::Normal&) {
                // If offset-position is normal, the element does not have an offset starting position.
                return normalPositionForOffsetPath(offsetPath, referenceRect);
            },
            [&](const CSS::Keyword::Auto&)  {
                // If offset-position is auto, use top / left corner of the box.
                return offsetFromContainer(renderer, container, referenceRect);
            },
            [&](const Style::Position& position) {
                return Style::evaluate<FloatPoint>(position, referenceRect.size(), zoom);
            }
        );
    };

    CheckedPtr container = renderer.containingBlock();
    if (!container)
        return std::nullopt;

    MotionPathData data;
    data.containingBlockBoundingRect = containingBlockRectForRenderer(renderer, *container, offsetPath);
    data.offsetFromContainingBlock = offsetFromContainer(renderer, *container, data.containingBlockBoundingRect.rect());

    auto zoom = renderer.style().usedZoomForLength();
    auto& offsetPosition = renderer.style().offsetPosition();

    WTF::switchOn(offsetPath,
        [&](const Style::BasicShapePath&) {
            data.usedStartingPosition = startingPositionForOffsetPosition(offsetPosition, data.containingBlockBoundingRect.rect(), *container, zoom);
        },
        [&](const Style::RayPath& offsetPath) {
            auto startingPosition = offsetPath.ray()->position;
            data.usedStartingPosition = startingPosition
                ? Style::evaluate<FloatPoint>(*startingPosition, data.containingBlockBoundingRect.rect().size(), zoom)
                : startingPositionForOffsetPosition(offsetPosition, data.containingBlockBoundingRect.rect(), *container, zoom);
        },
        [&](const auto&) { }
    );

    return data;
}

static PathTraversalState traversalStateAtDistance(const Path& path, float distanceValue)
{
    auto pathLength = path.length();
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

void MotionPath::applyMotionPathTransform(TransformationMatrix& matrix, const TransformOperationData& transformData, FloatPoint transformOrigin, TransformBox transformBox, const Path& offsetPath, std::optional<FloatPoint> offsetAnchor, float offsetDistance, float offsetRotate, bool offsetRotateHasAuto)
{
    auto boundingBox = transformData.boundingBox;
    auto anchor = transformOrigin;

    if (offsetAnchor)
        anchor = *offsetAnchor + boundingBox.location();

    auto shiftToOrigin = anchor - transformOrigin;

    if (transformData.isSVGRenderer && transformBox != TransformBox::ViewBox)
        anchor += boundingBox.location();

    auto traversalState = traversalStateAtDistance(offsetPath, offsetDistance);
    matrix.translate(traversalState.current().x(), traversalState.current().y());

    // Shift element to the anchor specified by offset-anchor.
    matrix.translate(-anchor.x(), -anchor.y());

    matrix.translate(shiftToOrigin.width(), shiftToOrigin.height());

    // Apply rotation.
    if (offsetRotateHasAuto)
        matrix.rotate(traversalState.normalAngle() + offsetRotate);
    else
        matrix.rotate(offsetRotate);

    matrix.translate(-shiftToOrigin.width(), -shiftToOrigin.height());
}

bool MotionPath::needsUpdateAfterContainingBlockLayout(const Style::OffsetPath& offsetPath)
{
    return WTF::holdsAlternative<Style::RayPath>(offsetPath)
        || WTF::holdsAlternative<Style::BoxPath>(offsetPath)
        || WTF::holdsAlternative<Style::BasicShapePath>(offsetPath);
}

FloatPoint NODELETE MotionPathData::currentOffset() const
{
    return FloatPoint(usedStartingPosition - offsetFromContainingBlock);
}

FloatRoundedRect NODELETE MotionPathData::offsetRect() const
{
    auto rect = containingBlockBoundingRect;
    auto shiftedPoint = offsetFromContainingBlock;
    shiftedPoint.scale(-1);
    rect.setLocation(shiftedPoint);
    return rect;
}

} // namespace WebCore
