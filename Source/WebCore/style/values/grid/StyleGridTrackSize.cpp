/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "StyleGridTrackSize.h"

#include "AnimationUtilities.h"
#include "CSSFunctionValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<GridTrackSize>::operator()(BuilderState& state, const CSSValue& value) -> GridTrackSize
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return GridTrackSize { toStyleFromCSSValue<GridTrackBreadth>(state, *primitiveValue) };

    auto function = requiredListDowncast<CSSFunctionValue, CSSPrimitiveValue>(state, value);
    if (!function)
        return { };

    if (function->size() == 1) {
        return GridTrackSize {
            toStyleFromCSSValue<GridTrackBreadth>(state, function->item(0)),
            GridTrackSizeType::FitContent
        };
    }

    return GridTrackSize {
        toStyleFromCSSValue<GridTrackBreadth>(state, function->item(0)),
        toStyleFromCSSValue<GridTrackBreadth>(state, function->item(1))
    };
}

auto CSSValueCreation<GridTrackSize>::operator()(CSSValuePool& pool, const RenderStyle& style, const GridTrackSize& value) -> Ref<CSSValue>
{
    switch (value.type()) {
    case GridTrackSizeType::Length:
        return createCSSValue(pool, style, value.minTrackBreadth());

    case GridTrackSizeType::FitContent:
        return CSSFunctionValue::create(
            CSSValueFitContent,
            createCSSValue(pool, style, value.fitContentTrackBreadth().length())
        );

    case GridTrackSizeType::MinMax:
        if (value.minTrackBreadth().isAuto() && value.maxTrackBreadth().isFlex())
            return createCSSValue(pool, style, value.maxTrackBreadth().flex());

        return CSSFunctionValue::create(
            CSSValueMinmax,
            createCSSValue(pool, style, value.minTrackBreadth()),
            createCSSValue(pool, style, value.maxTrackBreadth())
        );
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Serialization

void Serialize<GridTrackSize>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const GridTrackSize& value)
{
    switch (value.type()) {
    case GridTrackSizeType::Length:
        serializationForCSS(builder, context, style, value.minTrackBreadth());
        return;

    case GridTrackSizeType::FitContent:
        builder.append(nameLiteral(CSSValueFitContent), '(');
        serializationForCSS(builder, context, style, value.fitContentTrackBreadth().length());
        builder.append(')');
        return;

    case GridTrackSizeType::MinMax:
        if (value.minTrackBreadth().isAuto() && value.maxTrackBreadth().isFlex()) {
            serializationForCSS(builder, context, style, value.maxTrackBreadth().flex());
            return;
        }

        builder.append(nameLiteral(CSSValueMinmax), '(');
        serializationForCSS(builder, context, style, value.minTrackBreadth());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, value.maxTrackBreadth());
        builder.append(')');
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Blending

auto Blending<GridTrackSize>::blend(const GridTrackSize& from, const GridTrackSize& to, const BlendingContext& context) -> GridTrackSize
{
    if (from.type() != to.type())
        return context.progress < 0.5 ? from : to;

    switch (from.type()) {
    case GridTrackSizeType::Length:
        return GridTrackSize {
            Style::blend(from.minTrackBreadth(), to.minTrackBreadth(), context),
            GridTrackSizeType::Length,
        };

    case GridTrackSizeType::FitContent:
        return GridTrackSize {
            Style::blend(from.fitContentTrackBreadth(), to.fitContentTrackBreadth(), context),
            GridTrackSizeType::FitContent,
        };

    case GridTrackSizeType::MinMax:
        return GridTrackSize {
            Style::blend(from.minTrackBreadth(), to.minTrackBreadth(), context),
            Style::blend(from.maxTrackBreadth(), to.maxTrackBreadth(), context),
        };
    }

    RELEASE_ASSERT_NOT_REACHED();
}
// MARK: - Logging

TextStream& operator<<(TextStream& ts, const GridTrackSize& value)
{
    // FIXME: this should be expanded to use the other class members.
    switch (value.type()) {
    case GridTrackSizeType::Length:
        return ts << "size"_s;
    case GridTrackSizeType::MinMax:
        return ts << "minmax()"_s;
    case GridTrackSizeType::FitContent:
        return ts << "fit-content()"_s;
    }
    return ts;
}

} // namespace Style
} // namespace WebCore
