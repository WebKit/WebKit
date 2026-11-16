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
#include "AcceleratedEffectBasicShapePath.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "TransformOperationData.h"

namespace WebCore {

// MARK: - Path Evaluation

std::optional<WebCore::Path> tryPath(const AcceleratedEffectBasicShapePath& path, const TransformOperationData& data)
{
    if (auto motionPathData = data.motionPathData) {
        auto containingBlockRect = motionPathData->offsetRect().rect();
        return WTF::switchOn(path.basicShape.function,
            [&]<AcceleratedEffectShapeWithCenterCoordinate T>(const T& shape) -> std::optional<WebCore::Path> {
                if (!shape.position)
                    return pathForCenterCoordinate(shape, containingBlockRect, motionPathData->usedStartingPosition);
                return WebCore::path(shape, containingBlockRect);
            },
            [&](const auto& shape) -> std::optional<Path> {
                return WebCore::path(shape, containingBlockRect);
            }
        );
    }

    return WTF::switchOn(path.basicShape.function,
        [&](const auto& shape) -> std::optional<WebCore::Path> {
            return WebCore::path(shape, data.boundingBox);
        }
    );
}

// MARK: - Blending

bool AcceleratedEffectInterpolation<AcceleratedEffectBasicShapePath>::canBlend(const AcceleratedEffectBasicShapePath& from, const AcceleratedEffectBasicShapePath& to)
{
    return WebCore::canBlendForAcceleratedEffect(from.basicShape, to.basicShape)
        && from.box == to.box;
}

AcceleratedEffectBasicShapePath AcceleratedEffectInterpolation<AcceleratedEffectBasicShapePath>::blend(const AcceleratedEffectBasicShapePath& from, const AcceleratedEffectBasicShapePath& to, const BlendingContext& context)
{
    return {
        .basicShape = WebCore::blendForAcceleratedEffect(from.basicShape, to.basicShape, context),
        .box = from.box,
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
