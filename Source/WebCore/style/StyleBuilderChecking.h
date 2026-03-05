/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSFunctionValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "StyleBuilderState.h"

namespace WebCore {
namespace Style {

template<typename ValueType>
const ValueType* requiredDowncast(BuilderState&, const CSSValue&);

template<typename ValueType>
std::optional<std::pair<Ref<const ValueType>, Ref<const ValueType>>> requiredPairDowncast(BuilderState&, const CSSValue&);

template<typename ValueType> struct TypedRequiredListIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = const ValueType&;
    using difference_type = ptrdiff_t;
    using pointer = const ValueType*;
    using reference = const ValueType&;

    TypedRequiredListIterator(CSSValueList::iterator iterator)
        : iterator(iterator)
    {
    }

    TypedRequiredListIterator& operator++() { ++iterator; return *this; }
    const ValueType& operator*() const { return downcast<ValueType>(*iterator); }
    bool operator!=(const TypedRequiredListIterator& other) const { return iterator != other.iterator; }

    CSSValueList::iterator iterator;
};

template<typename ListType, typename ValueType> struct TypedRequiredList {
    TypedRequiredList(const ListType& list)
        : list(list)
    {
    }

    unsigned size() const { return list->size(); }
    const ValueType& item(unsigned index) const LIFETIME_BOUND { return downcast<ValueType>(*list->item(index)); }
    TypedRequiredListIterator<ValueType> begin() const LIFETIME_BOUND { return list->begin(); }
    TypedRequiredListIterator<ValueType> end() const LIFETIME_BOUND { return list->end(); }

    Ref<const ListType> list;
};

template<typename ListType, typename ValueType, unsigned minimumLength = 1>
std::optional<TypedRequiredList<ListType, ValueType>> requiredListDowncast(BuilderState&, const CSSValue&);

template<CSSValueID, typename ValueType, unsigned minimumLength = 1>
std::optional<TypedRequiredList<CSSFunctionValue, ValueType>> requiredFunctionDowncast(BuilderState&, const CSSValue&);

template<typename ValueType>
inline const ValueType* requiredDowncast(BuilderState& builderState, const CSSValue& value)
{
    auto* typedValue = dynamicDowncast<ValueType>(value);
    if (!typedValue) [[unlikely]] {
        builderState.setCurrentPropertyInvalidAtComputedValueTime();
        return nullptr;
    }
    return typedValue;
}

template<typename ValueType>
inline std::optional<std::pair<Ref<const ValueType>, Ref<const ValueType>>> requiredPairDowncast(BuilderState& builderState, const CSSValue& value)
{
    RefPtr pairValue = requiredDowncast<CSSValuePair>(builderState, value);
    if (!pairValue) [[unlikely]]
        return { };
    RefPtr firstValue = requiredDowncast<ValueType>(builderState, pairValue->first());
    if (!firstValue) [[unlikely]]
        return { };
    RefPtr secondValue = requiredDowncast<ValueType>(builderState, pairValue->second());
    if (!secondValue) [[unlikely]]
        return { };
    return { { firstValue.releaseNonNull(), secondValue.releaseNonNull() } };
}

template<typename ListType, typename ValueType, unsigned minimumSize>
inline auto requiredListDowncast(BuilderState& builderState, const CSSValue& value) -> std::optional<TypedRequiredList<ListType, ValueType>>
{
    RefPtr listValue = requiredDowncast<ListType>(builderState, value);
    if (!listValue) [[unlikely]]
        return { };
    if (listValue->size() < minimumSize) [[unlikely]] {
        builderState.setCurrentPropertyInvalidAtComputedValueTime();
        return { };
    }
    for (Ref value : *listValue) {
        if (!requiredDowncast<ValueType>(builderState, value)) [[unlikely]]
            return { };
    }
    return TypedRequiredList<ListType, ValueType> { *listValue };
}

template<CSSValueID functionName, typename ValueType, unsigned minimumSize>
inline auto requiredFunctionDowncast(BuilderState& builderState, const CSSValue& value) -> std::optional<TypedRequiredList<CSSFunctionValue, ValueType>>
{
    auto function = requiredListDowncast<CSSFunctionValue, ValueType, minimumSize>(builderState, value);
    if (!function) [[unlikely]]
        return { };
    if (function->list->name() != functionName) {
        builderState.setCurrentPropertyInvalidAtComputedValueTime();
        return { };
    }
    return function;
}

} // namespace Style
} // namespace WebCore
