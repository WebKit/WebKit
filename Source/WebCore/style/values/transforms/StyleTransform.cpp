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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleTransform.h"

#include "CSSTransformListValue.h"
#include "LayoutSize.h"
#include "RenderBox.h"
#include "StyleBuilderChecking.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "TransformOperations.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<Transform>::operator()(BuilderState& state, const CSSValue& value) -> Transform
{
    if (value.valueID() == CSSValueNone)
        return CSS::Keyword::None { };

    RefPtr transformList = requiredDowncast<CSSTransformListValue>(state, value);
    if (!transformList)
        return CSS::Keyword::None { };

    return TransformList {
        TransformList::Container::map(*transformList, [&](auto& transform) {
            return toStyleFromCSSValue<TransformFunction>(state, transform);
        })
    };
}

auto CSSValueCreation<Transform>::operator()(CSSValuePool& pool, const RenderStyle& style, const Transform& value) -> Ref<CSSValue>
{
    CSSValueListBuilder list;
    for (auto& transformFunction : value)
        list.append(createCSSValue(pool, style, transformFunction));

    if (list.isEmpty())
        return createCSSValue(pool, style, CSS::Keyword::None { });

    return CSSTransformListValue::create(WTFMove(list));
}

// MARK: - Blending

auto Blending<Transform>::canBlend(const Transform& from, const Transform& to, CompositeOperation compositeOperation) -> bool
{
    return Style::canBlend(from.m_value, to.m_value, compositeOperation);
}

auto Blending<Transform>::blend(const Transform& from, const Transform& to, const Interpolation::Context& context) -> Transform
{
    auto blendedTransformList = Style::blend(from.m_value, to.m_value, context);

    if (blendedTransformList.isEmpty())
        return CSS::Keyword::None { };

    return Transform { WTFMove(blendedTransformList) };
}

// MARK: - Platform

auto ToPlatform<Transform>::operator()(const Transform& value, const FloatSize& size) -> TransformOperations
{
    return TransformOperations { WTF::map(value, [&](auto& transformFunction) {
        return Style::toPlatform(transformFunction, size);
    }) };
}

} // namespace Style
} // namespace WebCore
