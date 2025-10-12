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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleWebKitBoxReflect.h"

#include "CSSReflectValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<WebkitBoxReflect>::operator()(BuilderState& state, const CSSValue& value) -> WebkitBoxReflect
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone)
            return CSS::Keyword::None { };

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    RefPtr reflectValue = requiredDowncast<CSSReflectValue>(state, value);
    if (!reflectValue)
        return CSS::Keyword::None { };

    auto convertMask = [&](RefPtr<const CSSValue> maskValue) {
        Style::MaskBorder mask { };
        if (maskValue)
            mask = toStyleFromCSSValue<Style::MaskBorder>(state, *maskValue);

        auto maskSlice = mask.slice();
        maskSlice.fill = CSS::Keyword::Fill { };
        mask.setSlice(WTFMove(maskSlice));

        return mask;
    };

    return WebkitBoxReflection {
        .direction = fromCSSValueID<ReflectionDirection>(reflectValue->direction()),
        .offset = toStyleFromCSSValue<WebkitBoxReflectionOffset>(state, reflectValue->offset()),
        .mask = convertMask(reflectValue->mask()),
    };
}

Ref<CSSValue> CSSValueCreation<WebkitBoxReflection>::operator()(CSSValuePool& pool, const RenderStyle& style, const WebkitBoxReflection& value)
{
    auto convertMask = [&](auto& mask) -> RefPtr<CSSValue> {
        if (mask.source().isNone())
            return createCSSValue(pool, style, CSS::Keyword::None { });
        else
            return createCSSValue(pool, style, mask);
    };

    return CSSReflectValue::create(
        toCSSValueID(value.direction),
        createCSSValue(pool, style, value.offset),
        convertMask(value.mask)
    );
}

// MARK: - Serialization

void Serialize<WebkitBoxReflection>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const WebkitBoxReflection& value)
{
    auto serializeMask = [&](auto& mask) {
        if (mask.source().isNone())
            serializationForCSS(builder, context, style, CSS::Keyword::None { });
        else
            serializationForCSS(builder, context, style, mask);
    };

    serializationForCSS(builder, context, style, value.direction);
    builder.append(' ');
    serializationForCSS(builder, context, style, value.offset);
    builder.append(' ');
    serializeMask(value.mask);
}

} // namespace Style
} // namespace WebCore
