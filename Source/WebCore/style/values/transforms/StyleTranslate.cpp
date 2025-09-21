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
#include "StyleTranslate.h"

#include "CSSPrimitiveValueMappings.h"
#include "StyleBuilderChecking.h"
#include "StyleBuilderConverter.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

bool Translate::apply(TransformationMatrix& transform, const FloatSize& size) const
{
    RefPtr protectedValue = value;
    return protectedValue && protectedValue->apply(transform, size);
}

// MARK: - Conversion

auto CSSValueConversion<Translate>::operator()(BuilderState& state, const CSSValue& value) -> Translate
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    // none | <length-percentage> [ <length-percentage> <length>? ]?

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return CSS::Keyword::None { };
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    auto type = list->size() > 2 ? TransformOperation::Type::Translate3D : TransformOperation::Type::Translate;
    auto tx = BuilderConverter::convertLengthUsingDoubleNoClamping(state, list->item(0));
    auto ty = list->size() > 1 ? BuilderConverter::convertLengthUsingDoubleNoClamping(state, list->item(1)) : WebCore::Length(0, LengthType::Fixed);
    auto tz = list->size() > 2 ? BuilderConverter::convertLengthUsingDoubleNoClamping(state, list->item(2)) : WebCore::Length(0, LengthType::Fixed);

    return Translate { TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), type) };
}

// MARK: - Blending

auto Blending<Translate>::blend(const Translate& from, const Translate& to, const BlendingContext& context) -> Translate
{
    RefPtr fromTranslate = from.value;
    RefPtr toTranslate = to.value;

    if (!fromTranslate && !toTranslate)
        return CSS::Keyword::None { };

    auto identity = [&](const auto& other) {
        return TranslateTransformOperation::create(WebCore::Length(0, LengthType::Fixed), WebCore::Length(0, LengthType::Fixed), WebCore::Length(0, LengthType::Fixed), other.type());
    };

    auto fromOperation = fromTranslate ? Ref(*fromTranslate) : identity(*toTranslate);
    auto toOperation = toTranslate ? Ref(*toTranslate) : identity(*fromTranslate);

    // Ensure the two transforms have the same type.
    if (!fromOperation->isSameType(toOperation)) {
        RefPtr<TranslateTransformOperation> normalizedFrom;
        RefPtr<TranslateTransformOperation> normalizedTo;
        if (fromOperation->is3DOperation() || toOperation->is3DOperation()) {
            normalizedFrom = TranslateTransformOperation::create(fromOperation->x(), fromOperation->y(), fromOperation->z(), TransformOperation::Type::Translate3D);
            normalizedTo = TranslateTransformOperation::create(toOperation->x(), toOperation->y(), toOperation->z(), TransformOperation::Type::Translate3D);
        } else {
            normalizedFrom = TranslateTransformOperation::create(fromOperation->x(), fromOperation->y(), TransformOperation::Type::Translate);
            normalizedTo = TranslateTransformOperation::create(toOperation->x(), toOperation->y(), TransformOperation::Type::Translate);
        }
        return blend(Translate { normalizedFrom.releaseNonNull() }, Translate { normalizedTo.releaseNonNull() }, context);
    }

    if (auto blendedOperation = toOperation->blend(fromOperation.ptr(), context); RefPtr translate = dynamicDowncast<TranslateTransformOperation>(blendedOperation))
        return Translate { TranslateTransformOperation::create(translate->x(), translate->y(), translate->z(), translate->type()) };

    return CSS::Keyword::None { };
}

// MARK: - Platform

auto ToPlatform<Translate>::operator()(const Translate& value) -> RefPtr<Translate::Platform>
{
    return value.value;
}

} // namespace Style
} // namespace WebCore
