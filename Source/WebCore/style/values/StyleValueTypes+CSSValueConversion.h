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

#pragma once

#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// Specialization for `String`.
template<> struct CSSValueConversion<String> {
    String operator()(BuilderState& state, const CSSPrimitiveValue& value)
    {
        if (!value.isString()) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return emptyString();
        }
        return value.string();
    }
    String operator()(BuilderState& state, const CSSValue& value)
    {
        RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!primitiveValue) [[unlikely]]
            return emptyString();
        return this->operator()(state, *primitiveValue);
    }
};

// Specialization for `AtomString`.
template<> struct CSSValueConversion<AtomString> {
    AtomString operator()(BuilderState& state, const CSSPrimitiveValue& value)
    {
        if (!value.isString()) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return emptyAtom();
        }
        return AtomString { value.string() };
    }
    AtomString operator()(BuilderState& state, const CSSValue& value)
    {
        RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!primitiveValue) [[unlikely]]
            return emptyAtom();
        return this->operator()(state, *primitiveValue);
    }
};

// Specialization for `CustomIdentifier`.
template<> struct CSSValueConversion<CustomIdentifier> {
    CustomIdentifier operator()(BuilderState& state, const CSSPrimitiveValue& value)
    {
        if (!value.isCustomIdent()) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { .value = emptyAtom() };
        }
        return { .value = AtomString { value.customIdent() } };
    }
    CustomIdentifier operator()(BuilderState& state, const CSSValue& value)
    {
        RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!primitiveValue) [[unlikely]]
            return { .value = emptyAtom() };
        return this->operator()(state, *primitiveValue);
    }
};

// Specialization for `PropertyIdentifier`.
template<> struct CSSValueConversion<PropertyIdentifier> {
    PropertyIdentifier operator()(BuilderState& state, const CSSPrimitiveValue& value)
    {
        if (!value.isPropertyID()) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { .value = CSSPropertyInvalid };
        }
        return { .value = value.propertyID() };
    }
    PropertyIdentifier operator()(BuilderState& state, const CSSValue& value)
    {
        RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!primitiveValue) [[unlikely]]
            return { .value = CSSPropertyInvalid };
        return this->operator()(state, *primitiveValue);
    }
};

// Specialization for `TupleLike` (wrapper).
template<TupleLike StyleType> requires (std::tuple_size_v<StyleType> == 1) struct CSSValueConversion<StyleType> {
    StyleType operator()(BuilderState& state, const CSSValue& value)
    {
        return { toStyleFromCSSValue<std::remove_cvref_t<std::tuple_element_t<0, StyleType>>>(state, value) };
    }
};

// Specialization for `SpaceSeparatedEnumSet`.
template<typename T> struct CSSValueConversion<SpaceSeparatedEnumSet<T>> {
    SpaceSeparatedEnumSet<T> operator()(BuilderState& state, const CSSValue& value)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::SpaceSeparator) {
            return SpaceSeparatedEnumSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element);
            });
        }
        return { toStyleFromCSSValue<T>(state, value) };
    }
};

// Specialization for `CommaSeparatedEnumSet`.
template<typename T> struct CSSValueConversion<CommaSeparatedEnumSet<T>> {
    CommaSeparatedEnumSet<T> operator()(BuilderState& state, const CSSValue& value)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::CommaSeparator) {
            return CommaSeparatedEnumSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element);
            });
        }
        return { toStyleFromCSSValue<T>(state, value) };
    }
};

// Specialization for `SpaceSeparatedListHashSet`.
template<typename T> struct CSSValueConversion<SpaceSeparatedListHashSet<T>> {
    SpaceSeparatedListHashSet<T> operator()(BuilderState& state, const CSSValue& value)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value)) {
            return SpaceSeparatedListHashSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element);
            });
        }
        return { toStyleFromCSSValue<T>(state, value) };
    }
};

// Specialization for `CommaSeparatedListHashSet`.
template<typename T> struct CSSValueConversion<CommaSeparatedListHashSet<T>> {
    CommaSeparatedListHashSet<T> operator()(BuilderState& state, const CSSValue& value)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value)) {
            return CommaSeparatedListHashSet<T>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<T>(state, element);
            });
        }
        return { toStyleFromCSSValue<T>(state, value) };
    }
};

// Specialization for `SpaceSeparatedFixedVector`.
template<typename StyleType> struct CSSValueConversion<SpaceSeparatedFixedVector<StyleType>> {
    SpaceSeparatedFixedVector<StyleType> operator()(BuilderState& state, const CSSValue& value)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::SpaceSeparator) {
            return SpaceSeparatedFixedVector<StyleType>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<StyleType>(state, element);
            });
        }
        return { toStyleFromCSSValue<StyleType>(state, value) };
    }
};

// Specialization for `CommaSeparatedFixedVector`.
template<typename StyleType> struct CSSValueConversion<CommaSeparatedFixedVector<StyleType>> {
    CommaSeparatedFixedVector<StyleType> operator()(BuilderState& state, const CSSValue& value)
    {
        if (auto list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::CommaSeparator) {
            return CommaSeparatedFixedVector<StyleType>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<StyleType>(state, element);
            });
        }
        return { toStyleFromCSSValue<StyleType>(state, value) };
    }
};

// Specialization for `ValueOrKeyword`.
template<typename StyleType, typename Keyword, typename StyleTypeMarkableTraits> struct CSSValueConversion<ValueOrKeyword<StyleType, Keyword, StyleTypeMarkableTraits>> {
    ValueOrKeyword<StyleType, Keyword, StyleTypeMarkableTraits> operator()(BuilderState& state, const CSSValue& value)
    {
        if (value.valueID() == Keyword { }.value)
            return Keyword { };
        return toStyleFromCSSValue<StyleType>(state, value);
    }
};

// Specialization for types derived from `ValueOrKeyword`.
template<ValueOrKeywordDerived T> struct CSSValueConversion<T> {
    T operator()(BuilderState& state, const CSSValue& value)
    {
        if (value.valueID() == typename T::Keyword { }.value)
            return typename T::Keyword { };
        return toStyleFromCSSValue<typename T::Value>(state, value);
    }
};

// Specialization for `ListOrNone`.
template<typename ListType> struct CSSValueConversion<ListOrNone<ListType>> {
    ListOrNone<ListType> operator()(BuilderState& state, const CSSValue& value)
    {
        if (value.valueID() == CSSValueNone)
            return CSS::Keyword::None { };
        return toStyleFromCSSValue<ListType>(state, value);
    }
};

// Specialization for types derived from `ListOrNone`.
template<ListOrNoneDerived T> struct CSSValueConversion<T> {
    T operator()(BuilderState& state, const CSSValue& value)
    {
        if (value.valueID() == CSSValueNone)
            return CSS::Keyword::None { };
        return toStyleFromCSSValue<typename T::List>(state, value);
    }
};

// Specialization for `ListOrDefault`.
template<typename ListType, typename Defaulter> struct CSSValueConversion<ListOrDefault<ListType, Defaulter>> {
    ListOrDefault<ListType, Defaulter> operator()(BuilderState& state, const CSSValue& value)
    {
        return toStyleFromCSSValue<ListType>(state, value);
    }
};

// Specialization for types derived from `ListOrDefault`.
template<ListOrDefaultDerived T> struct CSSValueConversion<T> {
    T operator()(BuilderState& state, const CSSValue& value)
    {
        return toStyleFromCSSValue<typename T::List>(state, value);
    }
};

// Specialization for types derived from `EnumSetOrKeywordBase`.
template<EnumSetOrKeywordBaseDerived T> struct CSSValueConversion<T> {
    T operator()(BuilderState& state, const CSSValue& value)
    {
        if (value.valueID() == typename T::Keyword { }.value)
            return typename T::Keyword { };
        return toStyleFromCSSValue<typename T::EnumSet>(state, value);
    }
};

} // namespace Style
} // namespace WebCore
