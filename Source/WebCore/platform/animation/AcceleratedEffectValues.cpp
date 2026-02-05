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

#if ENABLE(THREADED_ANIMATIONS)

#include "IntSize.h"
#include "MotionPath.h"
#include "Path.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayerModelObject.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleOffsetAnchor.h"
#include "StyleOffsetDistance.h"
#include "StyleOffsetPath.h"
#include "StyleOffsetPosition.h"
#include "StyleTransformResolver.h"
#include "TransformOperationData.h"

namespace WebCore {

AcceleratedEffectValues AcceleratedEffectValues::clone() const
{
    auto clonedOpacity = opacity;
    auto clonedTransformOperationData = transformOperationData;
    auto clonedTransformOrigin = transformOrigin;
    auto clonedTransformBox = transformBox;
    auto clonedTransform = transform.clone();
    RefPtr clonedTranslate = translate ? RefPtr { translate->clone() } : nullptr;
    RefPtr clonedScale = scale ? RefPtr { scale->clone() } : nullptr;
    RefPtr clonedRotate = rotate ? RefPtr { rotate->clone() } : nullptr;
    RefPtr clonedOffsetPath = offsetPath ? RefPtr { offsetPath->clone() } : nullptr;
    auto clonedOffsetDistance = offsetDistance;
    auto clonedOffsetPosition = offsetPosition;
    auto clonedOffsetAnchor = offsetAnchor;
    auto clonedOffsetRotate = offsetRotate;
    auto clonedFilter = filter.clone();
    auto clonedBackdropFilter = backdropFilter.clone();

    return {
        WTF::move(clonedOpacity),
        WTF::move(clonedTransformOperationData),
        WTF::move(clonedTransformOrigin),
        WTF::move(clonedTransformBox),
        WTF::move(clonedTransform),
        WTF::move(clonedTranslate),
        WTF::move(clonedScale),
        WTF::move(clonedRotate),
        WTF::move(clonedOffsetPath),
        WTF::move(clonedOffsetDistance),
        WTF::move(clonedOffsetPosition),
        WTF::move(clonedOffsetAnchor),
        WTF::move(clonedOffsetRotate),
        WTF::move(clonedFilter),
        WTF::move(clonedBackdropFilter)
    };
}

static constexpr AcceleratedEffectTransformBox toAcceleratedEffectTransformBox(TransformBox transformBox)
{
    switch (transformBox) {
    case TransformBox::StrokeBox:   return AcceleratedEffectTransformBox::StrokeBox;
    case TransformBox::ContentBox:  return AcceleratedEffectTransformBox::ContentBox;
    case TransformBox::BorderBox:   return AcceleratedEffectTransformBox::BorderBox;
    case TransformBox::FillBox:     return AcceleratedEffectTransformBox::FillBox;
    case TransformBox::ViewBox:     return AcceleratedEffectTransformBox::ViewBox;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static constexpr TransformBox toTransformBox(AcceleratedEffectTransformBox transformBox)
{
    switch (transformBox) {
    case AcceleratedEffectTransformBox::StrokeBox:   return TransformBox::StrokeBox;
    case AcceleratedEffectTransformBox::ContentBox:  return TransformBox::ContentBox;
    case AcceleratedEffectTransformBox::BorderBox:   return TransformBox::BorderBox;
    case AcceleratedEffectTransformBox::FillBox:     return TransformBox::FillBox;
    case AcceleratedEffectTransformBox::ViewBox:     return TransformBox::ViewBox;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

AcceleratedEffectValues::AcceleratedEffectValues(const RenderStyle& style, const IntRect& borderBoxRect, const RenderLayerModelObject* renderer)
{
    auto borderBoxSize = borderBoxRect.size();

    if (renderer)
        transformOperationData = TransformOperationData(renderer->transformReferenceBoxRect(style), renderer);

    // FIXME: RenderStyle::applyCSSTransform uses `transformOperationData.boundingBox` for all the reference boxes, but this uses a mixture of `transformOperationData.boundingBox` and the passed in `borderBoxSize`. Instead, probably `TransformOperationData` should be passed in directly and `borderBoxRect` removed.

    opacity = Style::evaluate<AcceleratedEffectOpacity>(style.opacity());
    transformBox = toAcceleratedEffectTransformBox(style.transformBox());
    transform = Style::toPlatform(style.transform(), borderBoxSize);
    translate = Style::toPlatform(style.translate(), borderBoxSize);
    scale = Style::toPlatform(style.scale(), borderBoxSize);
    rotate = Style::toPlatform(style.rotate(), borderBoxSize);

    if (!style.offsetPath().isNone() && transformOperationData) {
        if (auto path = Style::tryPath(style.offsetPath(), *transformOperationData)) {
            transformOrigin = { .value = Style::TransformResolver::computeTransformOrigin(style, transformOperationData->boundingBox).xy() };
            offsetPath = Style::toPlatform(style.offsetPath());
            offsetDistance = Style::evaluate<AcceleratedEffectOffsetDistance>(style.offsetDistance(), path->length(), Style::ZoomNeeded { });
            offsetRotate = Style::evaluate<AcceleratedEffectOffsetRotate>(style.offsetRotate());
            offsetAnchor = Style::evaluate<AcceleratedEffectOffsetAnchor>(style.offsetAnchor(), transformOperationData->boundingBox.size(), Style::ZoomNeeded { });

            // FIXME: Its not clear if this is the right bounding box for this. MotionPath::motionPathDataForRenderer() uses MotionPathData::containingBlockBoundingRect and its not apparent that they are necessarily the same rect.
            offsetPosition = Style::evaluate<AcceleratedEffectOffsetPosition>(style.offsetPosition(), transformOperationData->boundingBox.size(), Style::ZoomNeeded { });
        }
    }

    if (!style.filter().hasReferenceFilter())
        filter = Style::toPlatform(style.filter(), style);
    if (!style.backdropFilter().hasReferenceFilter())
        backdropFilter = Style::toPlatform(style.backdropFilter(), style);
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
        translate->apply(matrix);

    // 4. Rotate by the computed <angle> about the specified axis of rotate.
    if (rotate)
        rotate->apply(matrix);

    // 5. Scale by the computed X, Y, and Z values of scale.
    if (scale)
        scale->apply(matrix);

    // 6. Translate and rotate by the transform specified by offset.
    if (transformOperationData && offsetPath) {
        if (auto path = Style::tryPath(Style::OffsetPath { *offsetPath }, *transformOperationData)) {
            // FIXME: This transform of `transformOrigin` is not present in the overload of MotionPath::applyMotionPathTransform() that takes a `RenderStyle`.
            auto computedTransformOrigin = boundingBox.location() + transformOrigin.value;

            // FIXME: It is a layering violation to use `MotionPath::applyMotionPathTransform` here, as it is defined in the rendering directory.
            MotionPath::applyMotionPathTransform(
                matrix,
                *transformOperationData,
                computedTransformOrigin,
                toTransformBox(transformBox),
                *path,
                offsetAnchor.value,
                offsetDistance.value,
                offsetRotate.angle,
                offsetRotate.hasAuto
            );
        }
    }

    // 7. Multiply by each of the transform functions in transform from left to right.
    transform.apply(matrix);

    // 8. Translate by the negated computed X, Y and Z values of transform-origin.
    // (not needed, the GraphicsLayer handles that)

    return matrix;
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
