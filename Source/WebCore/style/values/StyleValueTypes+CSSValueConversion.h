/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSKeywordValue.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// Specialization for `TupleLike` (wrapper).
template<TupleLike StyleType>
    requires(std::tuple_size_v<StyleType> == 1)
struct CSSValueConversion<StyleType> {
    template<typename... Rest> StyleType operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        return { toStyleFromCSSValue<std::remove_cvref_t<std::tuple_element_t<0, StyleType>>>(state, value, std::forward<Rest>(rest)...) };
    }
};

// Specialization for `SpaceSeparatedEnumSet`.
template<typename T> struct CSSValueConversion<SpaceSeparatedEnumSet<T>> {
    template<typename... Rest> SpaceSeparatedEnumSet<T> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::SpaceSeparator) {
            return SpaceSeparatedEnumSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element, rest...);
            });
        }
        return { toStyleFromCSSValue<T>(state, value, std::forward<Rest>(rest)...) };
    }
};

// Specialization for `CommaSeparatedEnumSet`.
template<typename T> struct CSSValueConversion<CommaSeparatedEnumSet<T>> {
    template<typename... Rest> CommaSeparatedEnumSet<T> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::CommaSeparator) {
            return CommaSeparatedEnumSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element, rest...);
            });
        }
        return { toStyleFromCSSValue<T>(state, value, std::forward<Rest>(rest)...) };
    }
};

// Specialization for `SpaceSeparatedListHashSet`.
template<typename T> struct CSSValueConversion<SpaceSeparatedListHashSet<T>> {
    template<typename... Rest> SpaceSeparatedListHashSet<T> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value)) {
            return SpaceSeparatedListHashSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element, rest...);
            });
        }
        return { toStyleFromCSSValue<T>(state, value, std::forward<Rest>(rest)...) };
    }
};

// Specialization for `CommaSeparatedListHashSet`.
template<typename T> struct CSSValueConversion<CommaSeparatedListHashSet<T>> {
    template<typename... Rest> CommaSeparatedListHashSet<T> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value)) {
            return CommaSeparatedListHashSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element, rest...);
            });
        }
        return { toStyleFromCSSValue<T>(state, value, std::forward<Rest>(rest)...) };
    }
};

// Specialization for `SpaceSeparatedFixedVector`.
template<typename StyleType> struct CSSValueConversion<SpaceSeparatedFixedVector<StyleType>> {
    template<typename... Rest> SpaceSeparatedFixedVector<StyleType> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::SpaceSeparator) {
            return SpaceSeparatedFixedVector<StyleType>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<StyleType>(state, element, rest...);
            });
        }
        return { toStyleFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...) };
    }
};

// Specialization for `CommaSeparatedFixedVector`.
template<typename StyleType> struct CSSValueConversion<CommaSeparatedFixedVector<StyleType>> {
    template<typename... Rest> CommaSeparatedFixedVector<StyleType> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::CommaSeparator) {
            return CommaSeparatedFixedVector<StyleType>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<StyleType>(state, element, rest...);
            });
        }
        return { toStyleFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...) };
    }
};

// Specialization for `ValueOrKeyword`.
template<typename StyleType, typename Keyword, typename StyleTypeMarkableTraits> struct CSSValueConversion<ValueOrKeyword<StyleType, Keyword, StyleTypeMarkableTraits>> {
    template<typename... Rest> ValueOrKeyword<StyleType, Keyword, StyleTypeMarkableTraits> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
            if (keywordValue->valueID() == Keyword { }.value)
                return Keyword { };
        }
        return toStyleFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for types derived from `ValueOrKeyword`.
template<ValueOrKeywordDerived T> struct CSSValueConversion<T> {
    template<typename... Rest> T operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
            if (keywordValue->valueID() == typename T::Keyword { }.value)
                return typename T::Keyword { };
        }
        return toStyleFromCSSValue<typename T::Value>(state, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for `ListOrNone`.
template<typename ListType> struct CSSValueConversion<ListOrNone<ListType>> {
    template<typename... Rest> ListOrNone<ListType> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
            if (keywordValue->valueID() == CSSValueNone)
                return CSS::Keyword::None { };
        }
        return toStyleFromCSSValue<ListType>(state, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for types derived from `ListOrNone`.
template<ListOrNoneDerived T> struct CSSValueConversion<T> {
    template<typename... Rest> T operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
            if (keywordValue->valueID() == CSSValueNone)
                return CSS::Keyword::None { };
        }
        return toStyleFromCSSValue<typename T::List>(state, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for `ListOrDefault`.
template<typename ListType, typename Defaulter> struct CSSValueConversion<ListOrDefault<ListType, Defaulter>> {
    template<typename... Rest> ListOrDefault<ListType, Defaulter> operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        return toStyleFromCSSValue<ListType>(state, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for types derived from `ListOrDefault`.
template<ListOrDefaultDerived T> struct CSSValueConversion<T> {
    template<typename... Rest> T operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        return toStyleFromCSSValue<typename T::List>(state, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for types derived from `EnumSetOrKeywordBase`.
template<EnumSetOrKeywordBaseDerived T> struct CSSValueConversion<T> {
    template<typename... Rest> T operator()(BuilderState& state, const CSSValue& value, Rest&&... rest)
    {
        if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
            if (keywordValue->valueID() == typename T::Keyword { }.value)
                return typename T::Keyword { };
        }
        return toStyleFromCSSValue<typename T::EnumSet>(state, value, std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
