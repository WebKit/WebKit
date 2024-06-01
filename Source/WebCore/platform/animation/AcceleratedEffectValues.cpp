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
#include "MotionPath.h"
#include "Path.h"
#include "RenderElementInlines.h"
#include "RenderLayerModelObject.h"
#include "RenderStyleInlines.h"
#include "TransformOperationData.h"

namespace WebCore {

AcceleratedEffectValues AcceleratedEffectValues::clone() const
{
    std::optional<TransformOperationData> clonedTransformOperationData;
    if (transformOperationData)
        clonedTransformOperationData = transformOperationData;

    auto clonedTransformOrigin = transformOrigin;
    auto clonedTransform = transform.clone();

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

    auto clonedFilter = filter.clone();
    auto clonedBackdropFilter = backdropFilter.clone();

    return {
        opacity,
        WTFMove(clonedTransformOperationData),
        WTFMove(clonedTransformOrigin),
        transformBox,
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

AcceleratedEffectValues::AcceleratedEffectValues(const RenderStyle& style, const IntRect& borderBoxRect, const RenderLayerModelObject* renderer)
{
    opacity = style.opacity();

    auto borderBoxSize = borderBoxRect.size();

    if (renderer)
        transformOperationData = TransformOperationData(renderer->transformReferenceBoxRect(style), renderer);

    transformBox = style.transformBox();
    transform = style.transform().selfOrCopyWithResolvedCalculatedValues(borderBoxSize);

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

    filter = style.filter();
    backdropFilter = style.backdropFilter();
}

TransformationMatrix AcceleratedEffectValues::computedTransformationMatrix(const FloatRect& boundingBox) const
{
    // https://www.w3.org/TR/css-transforms-2/#ctm
    // The transformation matrix is computed from the transform, transform-origin, translate, rotate, scale, and offset properties as follows:
    // 1. Start with the identity matrix.
    TransformationMatrix matrix;

    // 2. Translate by the computed X, Y, and Z values of transform-origin.
    // (not needed, the GraphicsLayer handles that)

    // 3. Translate by the computed X, Y, and Z values of translate.
    if (translate)
        translate->apply(matrix, boundingBox.size());

    // 4. Rotate by the computed <angle> about the specified axis of rotate.
    if (rotate)
        rotate->apply(matrix, boundingBox.size());

    // 5. Scale by the computed X, Y, and Z values of scale.
    if (scale)
        scale->apply(matrix, boundingBox.size());

    // 6. Translate and rotate by the transform specified by offset.
    if (transformOperationData && offsetPath) {
        auto computedTransformOrigin = boundingBox.location() + floatPointForLengthPoint(transformOrigin, boundingBox.size());
        MotionPath::applyMotionPathTransform(matrix, *transformOperationData, computedTransformOrigin, *offsetPath, offsetAnchor, offsetDistance, offsetRotate, transformBox);
    }

    // 7. Multiply by each of the transform functions in transform from left to right.
    transform.apply(matrix, boundingBox.size());

    // 8. Translate by the negated computed X, Y and Z values of transform-origin.
    // (not needed, the GraphicsLayer handles that)

    return matrix;
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
