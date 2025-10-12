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
#include "StyleRotate.h"

#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

void Rotate::apply(TransformationMatrix& transform, const FloatSize& size) const
{
    if (RefPtr protectedValue = value)
        protectedValue->apply(transform, size);
}

// MARK: - Conversion

auto CSSValueConversion<Rotate>::operator()(BuilderState& state, const CSSValue& value) -> Rotate
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-rotate
    // none | <angle> | [ x | y | z | <number>{3} ] && <angle>

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return CSS::Keyword::None { };
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    // Only an angle was specified.
    if (list->size() == 1)
        return RotateTransformFunction::create(toStyleFromCSSValue<Angle<>>(state, list->item(0)), TransformFunctionBase::Type::Rotate);

    // An axis identifier and angle were specified.
    if (list->size() == 2) {
        auto axis = list->item(0).valueID();
        auto angle = toStyleFromCSSValue<Angle<>>(state, list->item(1));

        switch (axis) {
        case CSSValueX:
            return RotateTransformFunction::create(1_css_number, 0_css_number, 0_css_number, angle, TransformFunctionBase::Type::RotateX);
        case CSSValueY:
            return RotateTransformFunction::create(0_css_number, 1_css_number, 0_css_number, angle, TransformFunctionBase::Type::RotateY);
        case CSSValueZ:
            return RotateTransformFunction::create(0_css_number, 0_css_number, 1_css_number, angle, TransformFunctionBase::Type::RotateZ);
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return RotateTransformFunction::create(angle, TransformFunctionType::Rotate);
    }

    ASSERT(list->size() == 4);

    // An axis vector and angle were specified.
    auto x = toStyleFromCSSValue<Number<>>(state, list->item(0));
    auto y = toStyleFromCSSValue<Number<>>(state, list->item(1));
    auto z = toStyleFromCSSValue<Number<>>(state, list->item(2));
    auto angle = toStyleFromCSSValue<Angle<>>(state, list->item(3));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::Rotate3D);
}

// MARK: - Blending

auto Blending<Rotate>::blend(const Rotate& from, const Rotate& to, const BlendingContext& context) -> Rotate
{
    RefPtr fromRotate = from.value;
    RefPtr toRotate = to.value;

    if (!fromRotate && !toRotate)
        return CSS::Keyword::None { };

    auto identity = [&](const auto& other) {
        return RotateTransformFunction::create(0_css_deg, other.type());
    };

    auto fromFunction = fromRotate ? Ref(*fromRotate) : identity(*toRotate);
    auto toFunction = toRotate ? Ref(*toRotate) : identity(*fromRotate);

    // Ensure the two transforms have the same type.
    if (!fromFunction->isSameType(toFunction)) {
        RefPtr<const RotateTransformFunction> normalizedFrom;
        RefPtr<const RotateTransformFunction> normalizedTo;
        if (fromFunction->is3DOperation() || toFunction->is3DOperation()) {
            normalizedFrom = RotateTransformFunction::create(fromFunction->x(), fromFunction->y(), fromFunction->z(), fromFunction->angle(), TransformFunctionType::Rotate3D);
            normalizedTo = RotateTransformFunction::create(toFunction->x(), toFunction->y(), toFunction->z(), toFunction->angle(), TransformFunctionType::Rotate3D);
        } else {
            normalizedFrom = RotateTransformFunction::create(fromFunction->angle(), TransformFunctionType::Rotate);
            normalizedTo = RotateTransformFunction::create(toFunction->angle(), TransformFunctionType::Rotate);
        }
        return blend(Rotate { normalizedFrom.releaseNonNull() }, Rotate { normalizedTo.releaseNonNull() }, context);
    }

    if (auto blendedFunction = toFunction->blend(fromFunction.ptr(), context); RefPtr rotate = dynamicDowncast<RotateTransformFunction>(blendedFunction))
        return RotateTransformFunction::create(rotate->x(), rotate->y(), rotate->z(), rotate->angle(), rotate->type());

    return CSS::Keyword::None { };
}

// MARK: - Platform

auto ToPlatform<Rotate>::operator()(const Rotate& value, const FloatSize& size) -> RefPtr<TransformOperation>
{
    if (RefPtr function = value.value)
        return function->toPlatform(size);
    return nullptr;
}

} // namespace Style
} // namespace WebCore
