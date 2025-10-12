/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleSVGPathData.h"

#include "AnimationUtilities.h"
#include "CSSPathValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<SVGPathData>::operator()(BuilderState& state, const CSSValue& value) -> SVGPathData
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone)
            return CSS::Keyword::None { };

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    RefPtr pathValue = requiredDowncast<CSSPathValue>(state, value);
    if (!pathValue)
        return CSS::Keyword::None { };

    return toStyle(pathValue->path(), state);
}

auto CSSValueCreation<SVGPathData>::operator()(CSSValuePool& pool, const RenderStyle& style, const SVGPathData& value) -> Ref<CSSValue>
{
    if (value.isNone())
        return createCSSValue(pool, style, CSS::Keyword::None { });
    return CSSPathValue::create(toCSS(*value.tryPath(), style, PathConversion::ForceAbsolute));
}

// MARK: - Serialization

void Serialize<SVGPathData>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const SVGPathData& value)
{
    if (value.isNone()) {
        serializationForCSS(builder, context, style, CSS::Keyword::None { });
        return;
    }
    serializationForCSS(builder, context, style, *value.tryPath(), PathConversion::ForceAbsolute);
}

// MARK: - Blending

auto Blending<SVGPathData>::canBlend(const SVGPathData& a, const SVGPathData& b) -> bool
{
    auto aPath = a.tryPath();
    auto bPath = b.tryPath();
    return aPath && bPath && Style::canBlend(*aPath, *bPath);
}

auto Blending<SVGPathData>::blend(const SVGPathData& a, const SVGPathData& b, const BlendingContext& context) -> SVGPathData
{
    if (context.isDiscrete)
        return context.progress < 0.5 ? a : b;

    ASSERT(Style::canBlend(a, b));
    return Style::blend(*a.tryPath(), *b.tryPath(), context);
}

} // namespace Style
} // namespace WebCore
