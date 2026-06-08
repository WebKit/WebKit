/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include "CSSValueTypes+DeprecatedCSSOMValueCreation.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct DeprecatedCSSOMValueCreation<StyleType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool& pool, const RenderStyle& style, CSSStyleDeclaration& owner, const StyleType& value, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) -> Ref<DeprecatedCSSOMValue> { return createDeprecatedCSSOMValue(pool, style, owner, alternative, std::forward<Rest>(rest)...); });
    }
};

// Constrained for `TreatAsTupleLike`
template<TupleLike StyleType> struct DeprecatedCSSOMValueCreation<StyleType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool& pool, const RenderStyle& style, CSSStyleDeclaration& owner, const StyleType& value, Rest&&... rest)
    {
        if constexpr (std::tuple_size_v<StyleType> == 1 && SerializationSeparator<StyleType> == SerializationSeparatorType::None) {
            return createDeprecatedCSSOMValue(pool, style, get<0>(value), std::forward<Rest>(rest)...);
        } else if constexpr ((std::tuple_size_v<StyleType> == 2 || std::tuple_size_v<StyleType> == 4) && SerializationCoalescing<StyleType> == SerializationCoalescingType::Minimal) {
            return CSS::createDeprecatedCSSOMValue(pool, owner, toCSS(value, style), std::forward<Rest>(rest)...);
        } else {
            DeprecatedCSSOMValueListBuilder list;

            auto caller = WTF::makeVisitor(
                [&]<OptionalLike T>(const T& element) {
                    if (!element)
                        return;
                    list.append(createDeprecatedCSSOMValue(pool, style, owner, *element, rest...));
                },
                [&](const auto& element) {
                    list.append(createDeprecatedCSSOMValue(pool, style, owner, element, rest...));
                }
            );
            WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);

            return CSS::makeListDeprecatedCSSOMValue<SerializationSeparator<StyleType>>(WTF::move(list), owner);
        }
    }
};

// Constrained for `TreatAsRangeLike`
template<RangeLike StyleType> struct DeprecatedCSSOMValueCreation<StyleType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool& pool, const RenderStyle& style, CSSStyleDeclaration& owner, const StyleType& value, Rest&&... rest)
    {
        DeprecatedCSSOMValueListBuilder list;
        for (const auto& element : value)
            list.append(createDeprecatedCSSOMValue(pool, owner, style, element, rest...));

        return CSS::makeListDeprecatedCSSOMValue<SerializationSeparator<StyleType>>(WTF::move(list), owner);
    }
};

// Constrained for `TreatAsNonConverting`.
template<NonConverting StyleType> struct DeprecatedCSSOMValueCreation<StyleType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, const RenderStyle&, CSSStyleDeclaration& owner, const StyleType& value, Rest&&... rest)
    {
        return CSS::createDeprecatedCSSOMValue(pool, owner, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename StyleType> struct DeprecatedCSSOMValueCreation<FunctionNotation<Name, StyleType>> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool& pool, const RenderStyle& style, CSSStyleDeclaration& owner, const FunctionNotation<Name, StyleType>& value, Rest&&... rest)
    {
        return CSS::createDeprecatedCSSOMValue(pool, owner, toCSS(value, style), std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
