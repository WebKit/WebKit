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

#include "CSSValueList.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// Specialization for `CustomIdentifier`.
template<> struct CSSValueConversion<CustomIdentifier> {
   CustomIdentifier operator()(BuilderState& state, const CSSValue& value)
   {
        if (!value.isCustomIdent()) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { .value = emptyAtom() };
        }
        return { .value = AtomString { value.customIdent() } };
   }
};

// Specialization for `SpaceSeparatedFixedVector`.
template<typename StyleType> struct CSSValueConversion<SpaceSeparatedFixedVector<StyleType>> {
   SpaceSeparatedFixedVector<StyleType> operator()(BuilderState& state, const CSSValue& value)
   {
        if (auto list = dynamicDowncast<CSSValueList>(value)) {
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
        if (auto list = dynamicDowncast<CSSValueList>(value)) {
            return CommaSeparatedFixedVector<StyleType>::map(*list, [&](const CSSValue& element) {
                return toStyleFromCSSValue<StyleType>(state, element);
            });
        }
        return { toStyleFromCSSValue<StyleType>(state, value) };
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

} // namespace Style
} // namespace WebCore
