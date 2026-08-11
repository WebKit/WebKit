/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSValueTypes.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {
namespace CSS {

// MARK: - DeprecatedCSSOMValue Creation

Ref<DeprecatedCSSOMValue> makeCustomDeprecatedCSSOMValue(Function<WTF::String(const SerializationContext&)>&&, CSSStyleDeclaration&);

Ref<DeprecatedCSSOMValue> makePrimitiveDeprecatedCSSOMValue(Function<WTF::String(const SerializationContext&)>&&, CSSStyleDeclaration&);
Ref<DeprecatedCSSOMValue> makePrimitiveDeprecatedCSSOMValue(CSSValueID, CSSStyleDeclaration&);

template<SerializationSeparatorType> Ref<DeprecatedCSSOMValue> makeListDeprecatedCSSOMValue(DeprecatedCSSOMValueListBuilder&&, CSSStyleDeclaration&);
template<> Ref<DeprecatedCSSOMValue> makeListDeprecatedCSSOMValue<SerializationSeparatorType::Space>(DeprecatedCSSOMValueListBuilder&&, CSSStyleDeclaration&);
template<> Ref<DeprecatedCSSOMValue> makeListDeprecatedCSSOMValue<SerializationSeparatorType::Comma>(DeprecatedCSSOMValueListBuilder&&, CSSStyleDeclaration&);
template<> Ref<DeprecatedCSSOMValue> makeListDeprecatedCSSOMValue<SerializationSeparatorType::Slash>(DeprecatedCSSOMValueListBuilder&&, CSSStyleDeclaration&);

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct DeprecatedCSSOMValueCreation<CSSType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool& pool, CSSStyleDeclaration& owner, const CSSType& value, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return createDeprecatedCSSOMValue(pool, owner, alternative, std::forward<Rest>(rest)...); });
    }
};

// Constrained for `TreatAsTupleLike`
template<TupleLike CSSType> struct DeprecatedCSSOMValueCreation<CSSType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool& pool, CSSStyleDeclaration& owner, const CSSType& value, Rest&&... rest)
    {
        if constexpr (std::tuple_size_v<CSSType> == 1 && SerializationSeparator<CSSType> == SerializationSeparatorType::None) {
            return createDeprecatedCSSOMValue(pool, owner, get<0>(value), std::forward<Rest>(rest)...);
        } else if constexpr ((std::tuple_size_v<CSSType> == 2 || std::tuple_size_v<CSSType> == 4) && SerializationCoalescing<CSSType> == SerializationCoalescingType::Minimal) {
            // For historical reasons, coalescing tuples with two or four elements manifest as DeprecatedCSSOMPrimitiveValues.
            return makePrimitiveDeprecatedCSSOMValue([copy = CSSType { value }](const SerializationContext& context) {
                return serializationForCSS(context, copy);
            }, owner);
        } else {
            DeprecatedCSSOMValueListBuilder list;

            auto caller = WTF::makeVisitor(
                [&]<OptionalLike T>(const T& element) {
                    if (!element)
                        return;
                    list.append(createDeprecatedCSSOMValue(pool, owner, *element, rest...));
                },
                [&](const auto& element) {
                    list.append(createDeprecatedCSSOMValue(pool, owner, element, rest...));
                }
            );
            WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);

            return makeListDeprecatedCSSOMValue<SerializationSeparator<CSSType>>(WTF::move(list), owner);
        }
    }
};

// Constrained for `TreatAsRangeLike`
template<RangeLike CSSType> struct DeprecatedCSSOMValueCreation<CSSType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool& pool, CSSStyleDeclaration& owner, const CSSType& value, Rest&&... rest)
    {
        DeprecatedCSSOMValueListBuilder list;
        for (const auto& element : value)
            list.append(createDeprecatedCSSOMValue(pool, owner, element, rest...));

        return makeListDeprecatedCSSOMValue<SerializationSeparator<CSSType>>(WTF::move(list), owner);
    }
};

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct DeprecatedCSSOMValueCreation<CSSType> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration& owner, const CSSType& value, Rest&&...)
    {
        return makeCustomDeprecatedCSSOMValue([copy = CSSType { value }](const SerializationContext& context) {
            return serializationForCSS(context, copy);
        }, owner);
    }
};

// Specialization for `Constant`.
template<CSSValueID Id> struct DeprecatedCSSOMValueCreation<Constant<Id>> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration& owner, const Constant<Id>&, Rest&&...)
    {
        return makePrimitiveDeprecatedCSSOMValue(Id, owner);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct DeprecatedCSSOMValueCreation<FunctionNotation<Name, CSSType>> {
    template<typename... Rest> Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration& owner, const FunctionNotation<Name, CSSType>& value, Rest&&...)
    {
        return makeCustomDeprecatedCSSOMValue([copy = FunctionNotation<Name, CSSType> { value }](const SerializationContext& context) {
            return serializationForCSS(context, copy);
        }, owner);
    }
};

} // namespace CSS
} // namespace WebCore
