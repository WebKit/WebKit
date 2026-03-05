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
#include "StyleScrollFunction.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSScrollValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ScrollFunction>::operator()(BuilderState& state, const CSSValue& value) -> ScrollFunction
{
    RefPtr scrollValue = requiredDowncast<CSSScrollValue>(state, value);
    if (!scrollValue)
        return ScrollFunction { ScrollFunctionParameters { Scroller::Nearest, ScrollAxis::Block } };
    return this->operator()(state, *scrollValue);
}

auto CSSValueConversion<ScrollFunction>::operator()(BuilderState&, const CSSScrollValue& value) -> ScrollFunction
{
    return ScrollFunction {
        ScrollFunctionParameters {
            value.scroller() ? fromCSSValueID<Scroller>(value.scroller()->valueID()) : Scroller::Nearest,
            value.axis() ? fromCSSValueID<ScrollAxis>(value.axis()->valueID()) : ScrollAxis::Block
        }
    };
}

Ref<CSSValue> CSSValueCreation<ScrollFunction>::operator()(CSSValuePool& pool, const RenderStyle& style, const ScrollFunction& value)
{
    return CSSScrollValue::create(
        createCSSValue(pool, style, value.parameters.scroller),
        createCSSValue(pool, style, value.parameters.axis)
    );
}

// MARK: - Serialization

void Serialize<ScrollFunctionParameters>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const ScrollFunctionParameters& value)
{
    auto hasScroller = value.scroller != Scroller::Nearest;
    auto hasAxis = value.axis != ScrollAxis::Block;

    bool needsSpace = false;
    if (hasScroller) {
        serializationForCSS(builder, context, style, value.scroller);
        needsSpace = true;
    }

    if (hasAxis) {
        if (needsSpace)
            builder.append(' ');
        serializationForCSS(builder, context, style, value.axis);
    }
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const ScrollFunctionParameters& value)
{
    return ts << value.scroller << " "_s << value.axis;
}

} // namespace Style
} // namespace WebCore
