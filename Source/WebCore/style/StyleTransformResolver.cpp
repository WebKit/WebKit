/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleTransformResolver.h"

#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "MotionPath.h"
#include "RenderStyle.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TransformOperationData.h"
#include "TransformationMatrix.h"

namespace WebCore {
namespace Style {

TransformResolver::TransformResolver(TransformationMatrix& transform, const ComputedStyle& style)
    : m_transform { transform }
    , m_style { style }
{
}

TransformResolver::TransformResolver(TransformationMatrix& transform, const RenderStyle& style)
    : TransformResolver { transform, style.computedStyle() }
{
}

bool TransformResolver::affectedByTransformOrigin(const ComputedStyle& style)
{
    return style.rotate().affectedByTransformOrigin()
        || style.scale().affectedByTransformOrigin()
        || style.transform().affectedByTransformOrigin()
        || style.offsetPath().affectedByTransformOrigin();
}

bool TransformResolver::affectedByTransformOrigin(const RenderStyle& style)
{
    CheckedRef computedStyle = style.computedStyle();
    return affectedByTransformOrigin(computedStyle);
}

bool TransformResolver::affectedByTransformOrigin() const
{
    return affectedByTransformOrigin(m_style);
}

FloatPoint3D TransformResolver::computeTransformOrigin(const ComputedStyle& style, const FloatRect& boundingBox)
{
    auto zoom = style.usedZoomForLength();

    FloatPoint3D originTranslate;
    originTranslate.setXY(boundingBox.location() + evaluate<FloatPoint>(style.transformOrigin().xy(), boundingBox.size(), zoom));
    originTranslate.setZ(evaluate<float>(style.transformOriginZ(), zoom));
    return originTranslate;
}

FloatPoint3D TransformResolver::computeTransformOrigin(const RenderStyle& style, const FloatRect& boundingBox)
{
    CheckedRef computedStyle = style.computedStyle();
    return computeTransformOrigin(computedStyle, boundingBox);
}

FloatPoint3D TransformResolver::computeTransformOrigin(const FloatRect& boundingBox) const
{
    return computeTransformOrigin(m_style, boundingBox);
}

FloatPoint TransformResolver::computePerspectiveOrigin(const ComputedStyle& style, const FloatRect& boundingBox)
{
    return boundingBox.location() + evaluate<FloatPoint>(style.perspectiveOrigin(), boundingBox.size(), style.usedZoomForLength());
}

FloatPoint TransformResolver::computePerspectiveOrigin(const RenderStyle& style, const FloatRect& boundingBox)
{
    CheckedRef computedStyle = style.computedStyle();
    return computePerspectiveOrigin(computedStyle, boundingBox);
}

FloatPoint TransformResolver::computePerspectiveOrigin(const FloatRect& boundingBox) const
{
    return computePerspectiveOrigin(m_style, boundingBox);
}

void TransformResolver::applyPerspective(const FloatPoint& originTranslate)
{
    // https://www.w3.org/TR/css-transforms-2/#perspective
    // The perspective matrix is computed as follows:
    // 1. Start with the identity matrix.

    // 2. Translate by the computed X and Y values of perspective-origin
    m_transform.translate(originTranslate.x(), originTranslate.y());

    // 3. Multiply by the matrix that would be obtained from the perspective() transform function, where the length is provided by the value of the perspective property
    m_transform.applyPerspective(m_style->perspective().usedPerspective());

    // 4. Translate by the negated computed X and Y values of perspective-origin
    m_transform.translate(-originTranslate.x(), -originTranslate.y());
}

void TransformResolver::applyTransformOrigin(const FloatPoint3D& originTranslate)
{
    if (!originTranslate.isZero())
        m_transform.translate3d(originTranslate.x(), originTranslate.y(), originTranslate.z());
}

void TransformResolver::unapplyTransformOrigin(const FloatPoint3D& originTranslate)
{
    if (!originTranslate.isZero())
        m_transform.translate3d(-originTranslate.x(), -originTranslate.y(), -originTranslate.z());
}

void TransformResolver::applyCSSTransform(const TransformOperationData& transformData, OptionSet<Option> options)
{
    // https://www.w3.org/TR/css-transforms-2/#ctm
    // The transformation matrix is computed from the transform, transform-origin, translate, rotate, scale, and offset properties as follows:
    // 1. Start with the identity matrix.

    // 2. Translate by the computed X, Y, and Z values of transform-origin.
    // (implemented in applyTransformOrigin)
    auto& boundingBox = transformData.boundingBox;

    // 3. Translate by the computed X, Y, and Z values of translate.
    if (options.contains(Option::Translate))
        m_style->translate().apply(m_transform, boundingBox.size());

    // 4. Rotate by the computed <angle> about the specified axis of rotate.
    if (options.contains(Option::Rotate))
        m_style->rotate().apply(m_transform, boundingBox.size());

    // 5. Scale by the computed X, Y, and Z values of scale.
    if (options.contains(Option::Scale))
        m_style->scale().apply(m_transform, boundingBox.size());

    // 6. Translate and rotate by the transform specified by offset.
    if (options.contains(Option::Offset))
        applyMotionPathTransform(transformData, m_style->usedZoomForLength());

    // 7. Multiply by each of the transform functions in transform from left to right.
    m_style->transform().apply(m_transform, boundingBox.size());

    // 8. Translate by the negated computed X, Y and Z values of transform-origin.
    // (implemented in unapplyTransformOrigin)
}

void TransformResolver::applyTransform(const TransformOperationData& transformData, OptionSet<Option> options)
{
    if (!options.contains(Option::TransformOrigin) || !affectedByTransformOrigin()) {
        applyCSSTransform(transformData, options);
        return;
    }

    auto originTranslate = computeTransformOrigin(transformData.boundingBox);
    applyTransformOrigin(originTranslate);
    applyCSSTransform(transformData, options);
    unapplyTransformOrigin(originTranslate);
}

void TransformResolver::applyTransform(TransformationMatrix& transform, const ComputedStyle& style, const TransformOperationData& transformData, OptionSet<Option> options)
{
    TransformResolver { transform, style }.applyTransform(transformData, options);
}

void TransformResolver::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const TransformOperationData& transformData, OptionSet<Option> options)
{
    CheckedRef computedStyle = style.computedStyle();
    applyTransform(transform, computedStyle, transformData, options);
}

TransformationMatrix TransformResolver::computeTransform(const ComputedStyle& style, const TransformOperationData& transformData, OptionSet<Option> options)
{
    TransformationMatrix transform;
    TransformResolver::applyTransform(transform, style, transformData, options);
    return transform;
}

TransformationMatrix TransformResolver::computeTransform(const RenderStyle& style, const TransformOperationData& transformData, OptionSet<Option> options)
{
    CheckedRef computedStyle = style.computedStyle();
    return computeTransform(computedStyle, transformData, options);
}

void TransformResolver::applyMotionPathTransform(const TransformOperationData& transformData, ZoomFactor zoom)
{
    auto offsetPath = tryPath(m_style->offsetPath(), transformData, zoom);
    if (!offsetPath)
        return;

    auto& boundingBox = transformData.boundingBox;

    auto transformOrigin = computeTransformOrigin(boundingBox).xy();
    auto transformBox = m_style->transformBox();

    auto offsetDistance = evaluate<float>(m_style->offsetDistance(), offsetPath->length(), zoom);
    auto offsetAnchor = WTF::switchOn(m_style->offsetAnchor(),
        [&](const Position& position) -> std::optional<FloatPoint> {
            return evaluate<FloatPoint>(position, boundingBox.size(), zoom);
        },
        [&](const CSS::Keyword::Auto&) -> std::optional<FloatPoint> {
            return { };
        }
    );
    auto offsetRotate = m_style->offsetRotate().angle().value;
    auto offsetRotateHasAuto = m_style->offsetRotate().hasAuto();

    MotionPath::applyMotionPathTransform(
        m_transform,
        transformData,
        transformOrigin,
        transformBox,
        *offsetPath,
        offsetAnchor,
        offsetDistance,
        offsetRotate,
        offsetRotateHasAuto
    );
}

} // namespace Style
} // namespace WebCore
