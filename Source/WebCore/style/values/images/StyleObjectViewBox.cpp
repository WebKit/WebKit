/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StyleObjectViewBox.h"

#include "CSSBasicShapeValue.h"
#include "CSSKeywordValue.h"
#include "CSSValuePool.h"
#include "StyleBasicShape.h"
#include "StyleBuilderState.h"
#include "StyleComputedStyle.h"
#include "StyleKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ObjectViewBox>::operator()(BuilderState& state, const CSSValue& value) -> ObjectViewBox
{
    if (isValueID(value, CSSValueNone))
        return CSS::Keyword::None { };

    // <basic-shape-rect> variants (inset, rect, xywh) all resolve to InsetFunction
    auto& shapeValue = downcast<CSSBasicShapeValue>(value);
    auto styleShape = toStyle(shapeValue.shape(), state);
    if (auto* rect = std::get_if<BasicShapeRect>(&styleShape))
        return { WTF::move(*rect) };

    ASSERT_NOT_REACHED();
    return CSS::Keyword::None { };
}

// MARK: - CSSValue creation

Ref<CSSValue> CSSValueCreation<ObjectViewBox>::operator()(CSSValuePool&, const ComputedStyle& style, const ObjectViewBox& value)
{
    if (value.isNone())
        return CSSKeywordValue::create(CSSValueNone);
    return CSSBasicShapeValue::create(toCSS(BasicShape { *value.tryRect() }, style));
}

// MARK: - Serialization

void Serialize<ObjectViewBox>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const ComputedStyle& style, const ObjectViewBox& value)
{
    if (value.isNone()) {
        serializationForCSS(builder, context, style, CSS::Keyword::None { });
        return;
    }
    serializationForCSS(builder, context, style, BasicShape { *value.tryRect() });
}

// MARK: - Blending

auto Blending<ObjectViewBox>::canBlend(const ObjectViewBox& from, const ObjectViewBox& to) -> bool
{
    if (from.isNone() || to.isNone())
        return false;
    return Style::canBlend(*from.tryRect(), *to.tryRect());
}

auto Blending<ObjectViewBox>::blend(const ObjectViewBox& from, const ObjectViewBox& to, const BlendingContext& context) -> ObjectViewBox
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    ASSERT(canBlend(from, to));
    return { Style::blend(*from.tryRect(), *to.tryRect(), context) };
}

} // namespace Style
} // namespace WebCore
