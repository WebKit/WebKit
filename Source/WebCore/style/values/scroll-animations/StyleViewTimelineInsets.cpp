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
#include "StyleViewTimelineInsets.h"

#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "StyleBuilderChecking.h"
#include "StyleBuilderConverter.h"
#include "StyleExtractorSerializer.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

auto ViewTimelineInsetDefaulter::operator()() const -> const ViewTimelineInsetItem&
{
    static NeverDestroyed staticValue = ViewTimelineInsetItem { CSS::Keyword::Auto { } };
    return staticValue.get();
}

// MARK: - Conversion

auto CSSValueConversion<ViewTimelineInsetItem>::operator()(BuilderState& state, const CSSPrimitiveValue& value) -> ViewTimelineInsetItem
{
    return toStyleFromCSSValue<ViewTimelineInsetItem::Length>(state, value);
}

auto CSSValueConversion<ViewTimelineInsetItem>::operator()(BuilderState& state, const CSSValuePair& value) -> ViewTimelineInsetItem
{
    return {
        toStyleFromCSSValue<ViewTimelineInsetItem::Length>(state, value.first()),
        toStyleFromCSSValue<ViewTimelineInsetItem::Length>(state, value.second()),
    };
}

auto CSSValueConversion<ViewTimelineInsetItem>::operator()(BuilderState& state, const CSSValue& value) -> ViewTimelineInsetItem
{
    if (RefPtr pair = dynamicDowncast<CSSValuePair>(value))
        return this->operator()(state, *pair);

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Auto { };

    return this->operator()(state, *primitiveValue);
}

auto CSSValueConversion<ViewTimelineInsets>::operator()(BuilderState& state, const CSSValue& value) -> ViewTimelineInsets
{
    if (RefPtr pair = dynamicDowncast<CSSValuePair>(value))
        return { toStyleFromCSSValue<ViewTimelineInsetItem>(state, *pair) };

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return { toStyleFromCSSValue<ViewTimelineInsetItem>(state, *primitiveValue) };

    auto list = requiredListDowncast<CSSValueList, CSSValue>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    return ViewTimelineInsetList::map(*list, [&](auto& element) -> ViewTimelineInsetItem {
        return toStyleFromCSSValue<ViewTimelineInsetItem>(state, element);
    });
}

} // namespace Style
} // namespace WebCore
