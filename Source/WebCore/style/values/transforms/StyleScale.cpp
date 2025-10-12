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
#include "StyleScale.h"

#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

void Scale::apply(TransformationMatrix& transform, const FloatSize& size) const
{
    if (RefPtr protectedValue = value)
        protectedValue->apply(transform, size);
}

// MARK: - Conversion

auto CSSValueConversion<Scale>::operator()(BuilderState& state, const CSSValue& value) -> Scale
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-scale
    // none | [ <number> | <percentage> ]{1,3}

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return CSS::Keyword::None { };
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    auto sx = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, list->item(0));
    auto sy = list->size() > 1 ? toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, list->item(1)) : sx;
    auto sz = list->size() > 2 ? toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, list->item(2)) : NumberOrPercentageResolvedToNumber<> { 1_css_number };

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::Scale);
}

// MARK: - Blending

auto Blending<Scale>::blend(const Scale& from, const Scale& to, const BlendingContext& context) -> Scale
{
    RefPtr fromScale = from.value;
    RefPtr toScale = to.value;

    if (!fromScale && !toScale)
        return CSS::Keyword::None { };

    auto identity = [&](const auto& other) {
        return ScaleTransformFunction::create(1_css_number, 1_css_number, 1_css_number, other.type());
    };

    auto fromFunction = fromScale ? Ref(*fromScale) : identity(*toScale);
    auto toFunction = toScale ? Ref(*toScale) : identity(*fromScale);

    // Ensure the two transforms have the same type.
    if (!fromFunction->isSameType(toFunction)) {
        RefPtr<const ScaleTransformFunction> normalizedFrom;
        RefPtr<const ScaleTransformFunction> normalizedTo;
        if (fromFunction->is3DOperation() || toFunction->is3DOperation()) {
            normalizedFrom = ScaleTransformFunction::create(fromFunction->x(), fromFunction->y(), fromFunction->z(), TransformFunctionType::Scale3D);
            normalizedTo = ScaleTransformFunction::create(toFunction->x(), toFunction->y(), toFunction->z(), TransformFunctionType::Scale3D);
        } else {
            normalizedFrom = ScaleTransformFunction::create(fromFunction->x(), fromFunction->y(), TransformFunctionType::Scale);
            normalizedTo = ScaleTransformFunction::create(toFunction->x(), toFunction->y(), TransformFunctionType::Scale);
        }
        return blend(Scale { normalizedFrom.releaseNonNull() }, Scale { normalizedTo.releaseNonNull() }, context);
    }

    if (auto blendedFunction = toFunction->blend(fromFunction.ptr(), context); RefPtr scale = dynamicDowncast<ScaleTransformFunction>(blendedFunction))
        return ScaleTransformFunction::create(scale->x(), scale->y(), scale->z(), scale->type());

    return CSS::Keyword::None { };
}

// MARK: - Platform

auto ToPlatform<Scale>::operator()(const Scale& value, const FloatSize& size) -> RefPtr<TransformOperation>
{
    if (RefPtr function = value.value)
        return function->toPlatform(size);
    return nullptr;
}

} // namespace Style
} // namespace WebCore
