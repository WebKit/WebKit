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

#include "FloatPoint.h"
#include "IntSize.h"
#include "MotionPath.h"
#include "Path.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayerModelObject.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleOffsetAnchor.h"
#include "StyleOffsetDistance.h"
#include "StyleOffsetPath.h"
#include "StyleOffsetPosition.h"
#include "StyleTransformResolver.h"
#include "TransformationMatrix.h"
#include "TransformOperationData.h"
#include "TranslateTransformOperation.h"

namespace WebCore {

AcceleratedEffectValues AcceleratedEffectValues::clone() const
{
    auto clonedOpacity = opacity;
    auto clonedTransformOperationData = transformOperationData;
    auto clonedTransformOrigin = transformOrigin;
    auto clonedTransformBox = transformBox;
    auto clonedTransform = transform.clone();
    RefPtr clonedTranslate = translate ? RefPtr { protect(translate)->clone() } : nullptr;
    RefPtr clonedScale = scale ? RefPtr { protect(scale)->clone() } : nullptr;
    RefPtr clonedRotate = rotate ? RefPtr { protect(rotate)->clone() } : nullptr;
    auto clonedOffsetPath = offsetPath;
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

static constexpr AcceleratedEffectTransformBox LEGACY_NODELETE toAcceleratedEffectTransformBox(TransformBox transformBox)
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

static constexpr TransformBox LEGACY_NODELETE toTransformBox(AcceleratedEffectTransformBox transformBox)
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

AcceleratedEffectValues::AcceleratedEffectValues(const Style::ComputedStyle& style, const IntRect& borderBoxRect, const RenderLayerModelObject* renderer, OptionSet<AcceleratedEffectProperty> explicitProperties)
{
    auto borderBoxSize = borderBoxRect.size();

    if (renderer)
        transformOperationData = TransformOperationData(renderer->transformReferenceBoxRect(style), renderer);

    // FIXME: Style::ComputedStyle::applyCSSTransform uses `transformOperationData.boundingBox` for all the reference boxes, but this uses a mixture of `transformOperationData.boundingBox` and the passed in `borderBoxSize`. Instead, probably `TransformOperationData` should be passed in directly and `borderBoxRect` removed.

    auto zoom = style.usedZoomForLength();

    opacity = Style::evaluate<AcceleratedEffectOpacity>(style.opacity());
    transformBox = toAcceleratedEffectTransformBox(style.transformBox());
    transform = Style::toPlatform(style.transform(), borderBoxSize, zoom);
    translate = Style::toPlatform(style.translate(), borderBoxSize, zoom);
    if (!translate && explicitProperties.contains(AcceleratedEffectProperty::Translate))
        translate = TranslateTransformOperation::create();
    scale = Style::toPlatform(style.scale(), borderBoxSize, zoom);
    if (!scale && explicitProperties.contains(AcceleratedEffectProperty::Scale))
        scale = ScaleTransformOperation::create();
    rotate = Style::toPlatform(style.rotate(), borderBoxSize, zoom);
    if (!rotate && explicitProperties.contains(AcceleratedEffectProperty::Rotate))
        rotate = RotateTransformOperation::create();

    if (transformOperationData && transformOperationData->motionPathData && !style.offsetPath().isNone()) {
        auto evaluatedOffsetPath = Style::evaluate<AcceleratedEffectOffsetPath>(style.offsetPath(), *transformOperationData, zoom);
        if (auto path = tryPath(evaluatedOffsetPath, *transformOperationData)) {
            offsetPath = WTF::move(evaluatedOffsetPath);
            transformOrigin = { .value = Style::TransformResolver::computeTransformOrigin(style, transformOperationData->boundingBox).xy() };
            offsetDistance = Style::evaluate<AcceleratedEffectOffsetDistance>(style.offsetDistance(), path->length(), zoom);
            offsetRotate = Style::evaluate<AcceleratedEffectOffsetRotate>(style.offsetRotate());
            offsetAnchor = Style::evaluate<AcceleratedEffectOffsetAnchor>(style.offsetAnchor(), transformOperationData->boundingBox.size(), zoom);

            // FIXME: Its not clear if this is the right bounding box for this. MotionPath::motionPathDataForRenderer() uses MotionPathData::containingBlockBoundingRect and its not apparent that they are necessarily the same rect.
            offsetPosition = Style::evaluate<AcceleratedEffectOffsetPosition>(style.offsetPosition(), transformOperationData->boundingBox.size(), zoom);
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
        protect(translate)->apply(matrix);

    // 4. Rotate by the computed <angle> about the specified axis of rotate.
    if (rotate)
        protect(rotate)->apply(matrix);

    // 5. Scale by the computed X, Y, and Z values of scale.
    if (scale)
        protect(scale)->apply(matrix);

    // 6. Translate and rotate by the transform specified by offset.
    if (transformOperationData) {
        if (auto path = tryPath(offsetPath, *transformOperationData)) {
            // FIXME: This transform of `transformOrigin` is not present in the overload of MotionPath::applyMotionPathTransform() that takes a `StyleComputedStyle`.
            auto computedTransformOrigin = boundingBox.location() + transformOrigin.value;

            // FIXME: It is a layering violation to use `MotionPath::applyMotionPathTransform` here, as it is defined in the rendering directory.
            // FIXME: The path is walked here for its length on every frame, even though the constructor
            // already walked it for the same value. Carrying the length would need it serialized with the rest.
            MotionPath::applyMotionPathTransform(
                matrix,
                *transformOperationData,
                computedTransformOrigin,
                toTransformBox(transformBox),
                *path,
                path->length(),
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

String AcceleratedEffectValues::dumpProperty(AcceleratedEffectProperty property) const
{
    TextStream ts;
    switch (property) {
    case AcceleratedEffectProperty::Opacity:
        ts << "opacity: " << opacity.value;
        break;
    case AcceleratedEffectProperty::Transform:
        if (transform.isEmpty())
            ts << "transform: none";
        else
            ts << "transform: " << transform;
        break;
    case AcceleratedEffectProperty::Translate:
        if (RefPtr translate = this->translate)
            ts << "translate: " << *translate;
        else
            ts << "translate: none";
        break;
    case AcceleratedEffectProperty::Rotate:
        if (RefPtr rotate = this->rotate)
            ts << "rotate: " << *rotate;
        else
            ts << "rotate: none";
        break;
    case AcceleratedEffectProperty::Scale:
        if (RefPtr scale = this->scale)
            ts << "scale: " << *scale;
        else
            ts << "scale: none";
        break;
    case AcceleratedEffectProperty::OffsetPath:
        ts << "offset-path: n/a";
        break;
    case AcceleratedEffectProperty::OffsetDistance:
        ts << "offset-distance: n/a";
        break;
    case AcceleratedEffectProperty::OffsetPosition:
        ts << "offset-position: n/a";
        break;
    case AcceleratedEffectProperty::OffsetAnchor:
        ts << "offset-anchor: n/a";
        break;
    case AcceleratedEffectProperty::OffsetRotate:
        ts << "offset-rotate: n/a";
        break;
    case AcceleratedEffectProperty::Filter:
        if (filter.isEmpty())
            ts << "filter: none";
        else
            ts << "filter: " << filter;
        break;
    case AcceleratedEffectProperty::BackdropFilter:
        if (backdropFilter.isEmpty())
            ts << "backdrop-filter: none";
        else
            ts << "backdrop-filter: " << backdropFilter;
        break;
    default:
        break;
    }
    return ts.release();
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
