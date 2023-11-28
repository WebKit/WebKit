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
#include "AcceleratedEffectValues.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "IntSize.h"
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderStyleInlines.h"
#include "TransformOperationData.h"

namespace WebCore {

AcceleratedEffectValues::AcceleratedEffectValues(const AcceleratedEffectValues& src)
{
    opacity = src.opacity;

    auto& transformOperations = transform.operations();
    auto& srcTransformOperations = src.transform.operations();
    transformOperations.appendVector(srcTransformOperations);

    translate = src.translate.copyRef();
    scale = src.scale.copyRef();
    rotate = src.rotate.copyRef();
    transformOrigin = src.transformOrigin;

    offsetPath = src.offsetPath.copyRef();
    offsetDistance = src.offsetDistance;
    offsetPosition = src.offsetPosition;
    offsetAnchor = src.offsetAnchor;
    offsetRotate = src.offsetRotate;

    filter.setOperations(src.filter.operations().map([](const auto& operation) {
        return operation.copyRef();
    }));

    backdropFilter.setOperations(src.backdropFilter.operations().map([](const auto& operation) {
        return operation.copyRef();
    }));
}

AcceleratedEffectValues AcceleratedEffectValues::clone() const
{
    auto clonedTransformOrigin = transformOrigin;

    TransformOperations clonedTransform { transform.operations().map([](const auto& operation) {
        return RefPtr { operation->clone() };
    }) };

    RefPtr<TransformOperation> clonedTranslate;
    if (auto* srcTranslate = translate.get())
        clonedTranslate = srcTranslate->clone();

    RefPtr<TransformOperation> clonedScale;
    if (auto* srcScale = scale.get())
        clonedScale = srcScale->clone();

    RefPtr<TransformOperation> clonedRotate;
    if (auto* srcRotate = rotate.get())
        clonedRotate = srcRotate->clone();

    RefPtr<PathOperation> clonedOffsetPath;
    if (auto* srcOffsetPath = offsetPath.get())
        clonedOffsetPath = srcOffsetPath->clone();

    auto clonedOffsetDistance = offsetDistance;
    auto clonedOffsetPosition = offsetPosition;
    auto clonedOffsetAnchor = offsetAnchor;
    auto clonedOffsetRotate = offsetRotate;

    FilterOperations clonedFilter { filter.operations().map([](const auto& operation) {
        return RefPtr { operation->clone() };
    }) };

    FilterOperations clonedBackdropFilter { backdropFilter.operations().map([](const auto& operation) {
        return RefPtr { operation->clone() };
    }) };

    return {
        opacity,
        WTFMove(clonedTransformOrigin),
        WTFMove(clonedTransform),
        WTFMove(clonedTranslate),
        WTFMove(clonedScale),
        WTFMove(clonedRotate),
        WTFMove(clonedOffsetPath),
        WTFMove(clonedOffsetDistance),
        WTFMove(clonedOffsetPosition),
        WTFMove(clonedOffsetAnchor),
        WTFMove(clonedOffsetRotate),
        WTFMove(clonedFilter),
        WTFMove(clonedBackdropFilter)
    };
}

static LengthPoint nonCalculatedLengthPoint(LengthPoint lengthPoint, const IntSize& borderBoxSize)
{
    if (!lengthPoint.x().isCalculated() && !lengthPoint.y().isCalculated())
        return lengthPoint;
    return {
        { floatValueForLength(lengthPoint.x(), borderBoxSize.width()), LengthType::Fixed },
        { floatValueForLength(lengthPoint.y(), borderBoxSize.height()), LengthType::Fixed }
    };
}

AcceleratedEffectValues::AcceleratedEffectValues(const RenderStyle& style, const IntRect& borderBoxRect)
{
    opacity = style.opacity();

    auto borderBoxSize = borderBoxRect.size();

    auto& transformOperations = transform.operations();
    auto& srcTransformOperations = style.transform().operations();
    transformOperations.appendContainerWithMapping(srcTransformOperations, [&](auto& srcTransformOperation) {
        return srcTransformOperation->selfOrCopyWithResolvedCalculatedValues(borderBoxSize);
    });

    if (auto* srcTranslate = style.translate())
        translate = srcTranslate->selfOrCopyWithResolvedCalculatedValues(borderBoxSize);
    if (auto* srcScale = style.scale())
        scale = srcScale->selfOrCopyWithResolvedCalculatedValues(borderBoxSize);
    if (auto* srcRotate = style.rotate())
        rotate = srcRotate->selfOrCopyWithResolvedCalculatedValues(borderBoxSize);
    transformOrigin = nonCalculatedLengthPoint(style.transformOriginXY(), borderBoxSize);

    offsetPath = style.offsetPath();
    offsetPosition = nonCalculatedLengthPoint(style.offsetPosition(), borderBoxSize);
    offsetAnchor = nonCalculatedLengthPoint(style.offsetAnchor(), borderBoxSize);
    offsetRotate = style.offsetRotate();
    offsetDistance = style.offsetDistance();
    if (offsetDistance.isCalculated() && offsetPath) {
        auto anchor = borderBoxRect.location() + floatPointForLengthPoint(transformOrigin, borderBoxSize);
        if (!offsetAnchor.x().isAuto())
            anchor = floatPointForLengthPoint(offsetAnchor, borderBoxRect.size()) + borderBoxRect.location();

        auto path = offsetPath->getPath(TransformOperationData(FloatRect(borderBoxRect)));
        offsetDistance = { path ? path->length() : 0.0f, LengthType:: Fixed };
    }

    filter.setOperations(style.filter().operations().map([](const auto& operation) {
        return operation.copyRef();
    }));

    backdropFilter.setOperations(style.backdropFilter().operations().map([](const auto& operation) {
        return operation.copyRef();
    }));
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
