/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedEffectRayPath.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "TransformOperationData.h"

namespace WebCore {

// MARK: - Path Evaluation

static double lengthForRayPath(const AcceleratedEffectRayPath& path, const MotionPathData& data, const FloatRect& elementRect)
{
    auto length = [&] {
        auto& boundingBox = data.containingBlockBoundingRect.rect();
        auto distances = distanceOfPointToSidesOfRect(boundingBox, data.usedStartingPosition);

        switch (path.ray.size) {
        case AcceleratedEffectRayFunction::RaySize::ClosestSide:
            return std::min({ distances.top(), distances.bottom(), distances.left(), distances.right() });
        case AcceleratedEffectRayFunction::RaySize::FarthestSide:
            return std::max({ distances.top(), distances.bottom(), distances.left(), distances.right() });
        case AcceleratedEffectRayFunction::RaySize::FarthestCorner:
            return std::hypot(std::max(distances.left(), distances.right()), std::max(distances.top(), distances.bottom()));
        case AcceleratedEffectRayFunction::RaySize::ClosestCorner:
            return std::hypot(std::min(distances.left(), distances.right()), std::min(distances.top(), distances.bottom()));
        case AcceleratedEffectRayFunction::RaySize::Sides:
            return lengthOfRayIntersectionWithBoundingBox(boundingBox, std::make_pair(data.usedStartingPosition, path.ray.angle));
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    if (path.ray.contain)
        return std::max(0.0, length - (std::max(elementRect.width(), elementRect.height()) / 2));
    return length;
}

std::optional<WebCore::Path> tryPath(const AcceleratedEffectRayPath& path, const TransformOperationData& data)
{
    auto motionPathData = data.motionPathData;
    if (!motionPathData || motionPathData->containingBlockBoundingRect.rect().isZero())
        return std::nullopt;

    auto currentOffset = motionPathData->currentOffset();

    auto length = lengthForRayPath(path, *motionPathData, data.boundingBox);
    auto radians = deg2rad(toPositiveAngle(path.ray.angle) - 90.0);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);

    WebCore::Path result;
    result.moveTo(currentOffset);
    result.addLineTo(currentOffset + point);
    return result;
}

// MARK: - Blending

bool AcceleratedEffectInterpolation<AcceleratedEffectRayPath>::canBlend(const AcceleratedEffectRayPath& from, const AcceleratedEffectRayPath& to)
{
    return WebCore::canBlendForAcceleratedEffect(from.ray, to.ray)
        && from.box == to.box;
}

AcceleratedEffectRayPath AcceleratedEffectInterpolation<AcceleratedEffectRayPath>::blend(const AcceleratedEffectRayPath& from, const AcceleratedEffectRayPath& to, const BlendingContext& context)
{
    return {
        .ray = WebCore::blendForAcceleratedEffect(from.ray, to.ray, context),
        .box = from.box,
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
