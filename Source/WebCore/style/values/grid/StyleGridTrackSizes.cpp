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
#include "StyleGridTrackSizes.h"

#include "CSSGridTrackSizesValue.h"
#include "StyleBuilderChecking.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

auto GridTrackSizeDefaulter::operator()() const -> const GridTrackSize&
{
    static NeverDestroyed staticValue = GridTrackSize { CSS::Keyword::Auto { } };
    return staticValue.get();
}

// MARK: - Conversion

auto ToCSS<GridTrackSizes>::operator()(const GridTrackSizes& value, const RenderStyle& style) -> CSS::GridTrackSizes
{
    return { CSS::GridTrackSizeList::map(value, [&](auto& trackSize) -> CSS::GridTrackSize {
        return toCSS(trackSize, style);
    }) };
}

auto ToStyle<CSS::GridTrackSizes>::operator()(const CSS::GridTrackSizes& value, const BuilderState& state) -> GridTrackSizes
{
    return { GridTrackSizeList::map(value, [&](auto& trackSize) -> GridTrackSize {
        return toStyle(trackSize, state);
    }) };
}

auto CSSValueConversion<GridTrackSizes>::operator()(BuilderState& state, const CSSValue& value) -> GridTrackSizes
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return GridTrackSizes { GridTrackSize { toStyleFromCSSValue<GridTrackSize::Breadth>(state, *primitiveValue) } };
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value))
        return GridTrackSizes { GridTrackSize { toStyleFromCSSValue<GridTrackSize::Breadth>(state, *keywordValue) } };

    RefPtr gridTrackSizesValue = requiredDowncast<CSSGridTrackSizesValue>(state, value);
    if (!gridTrackSizesValue)
        return CSS::Keyword::Auto { };

    return GridTrackSizes { toStyle(gridTrackSizesValue->list(), state) };
}

auto CSSValueCreation<GridTrackSizes>::operator()(CSSValuePool&, const RenderStyle& style, const GridTrackSizes& value) -> Ref<CSSValue>
{
    return CSSGridTrackSizesValue::create(toCSS(value, style));
}

} // namespace Style
} // namespace WebCore
