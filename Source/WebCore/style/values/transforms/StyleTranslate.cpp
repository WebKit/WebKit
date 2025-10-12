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
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

TransformFunctionSizeDependencies Translate::computeSizeDependencies() const
{
    if (RefPtr protectedValue = value)
        return protectedValue->computeSizeDependencies();
    return { };
}

void Translate::apply(TransformationMatrix& transform, const FloatSize& size) const
{
    if (RefPtr protectedValue = value)
        protectedValue->apply(transform, size);
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

    auto type = list->size() > 2 ? TransformFunctionType::Translate3D : TransformFunctionType::Translate;
    auto tx = toStyleFromCSSValue<TranslateTransformFunction::LengthPercentage>(state, list->item(0));
    auto ty = list->size() > 1 ? toStyleFromCSSValue<TranslateTransformFunction::LengthPercentage>(state, list->item(1)) : TranslateTransformFunction::LengthPercentage { 0_css_px };
    auto tz = list->size() > 2 ? toStyleFromCSSValue<TranslateTransformFunction::Length>(state, list->item(2)) : TranslateTransformFunction::Length { 0_css_px };

    return TranslateTransformFunction::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), type);
}

// MARK: - Blending

auto Blending<Translate>::blend(const Translate& from, const Translate& to, const BlendingContext& context) -> Translate
{
    RefPtr fromTranslate = from.value;
    RefPtr toTranslate = to.value;

    if (!fromTranslate && !toTranslate)
        return CSS::Keyword::None { };

    auto identity = [&](const auto& other) {
        return TranslateTransformFunction::create(0_css_px, 0_css_px, 0_css_px, other.type());
    };

    auto fromFunction = fromTranslate ? Ref(*fromTranslate) : identity(*toTranslate);
    auto toFunction = toTranslate ? Ref(*toTranslate) : identity(*fromTranslate);

    // Ensure the two transforms have the same type.
    if (!fromFunction->isSameType(toFunction)) {
        RefPtr<const TranslateTransformFunction> normalizedFrom;
        RefPtr<const TranslateTransformFunction> normalizedTo;
        if (fromFunction->is3DOperation() || toFunction->is3DOperation()) {
            normalizedFrom = TranslateTransformFunction::create(fromFunction->x(), fromFunction->y(), fromFunction->z(), TransformFunctionType::Translate3D);
            normalizedTo = TranslateTransformFunction::create(toFunction->x(), toFunction->y(), toFunction->z(), TransformFunctionType::Translate3D);
        } else {
            normalizedFrom = TranslateTransformFunction::create(fromFunction->x(), fromFunction->y(), TransformFunctionType::Translate);
            normalizedTo = TranslateTransformFunction::create(toFunction->x(), toFunction->y(), TransformFunctionType::Translate);
        }
        return blend(Translate { normalizedFrom.releaseNonNull() }, Translate { normalizedTo.releaseNonNull() }, context);
    }

    if (auto blendedFunction = toFunction->blend(fromFunction.ptr(), context); RefPtr translate = dynamicDowncast<TranslateTransformFunction>(blendedFunction))
        return TranslateTransformFunction::create(translate->x(), translate->y(), translate->z(), translate->type());

    return CSS::Keyword::None { };
}

// MARK: - Platform

auto ToPlatform<Translate>::operator()(const Translate& value, const FloatSize& size) -> RefPtr<TransformOperation>
{
    if (RefPtr function = value.value)
        return function->toPlatform(size);
    return nullptr;
}

} // namespace Style
} // namespace WebCore
