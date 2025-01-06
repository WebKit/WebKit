/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveNumericTypes.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"

namespace WebCore {
namespace CSS {

// MARK: - Conversion from strongly typed `CSS::` value types to `WebCore::CSSValue` types.

template<typename CSSType> struct CSSValueCreation;

template<typename CSSType> Ref<CSSValue> createCSSValue(const CSSType& value)
{
    return CSSValueCreation<CSSType>{}(value);
}

template<NumericRaw T> struct CSSValueCreation<T> {
    Ref<CSSValue> operator()(const T& raw)
    {
        return CSSPrimitiveValue::create(raw.value, toCSSUnitType(raw.unit));
    }
};

template<Calc T> struct CSSValueCreation<T> {
    Ref<CSSValue> operator()(const T& calc)
    {
        return CSSPrimitiveValue::create(calc.protectedCalc());
    }
};

template<Numeric T> struct CSSValueCreation<T> {
    Ref<CSSValue> operator()(const T& value)
    {
        return WTF::switchOn(value,
            [](const typename T::Raw& raw) {
                return CSSPrimitiveValue::create(raw.value, toCSSUnitType(raw.unit));
            },
            [](const typename T::Calc& calc) {
                return CSSPrimitiveValue::create(calc.protectedCalc());
            }
        );
    }
};

template<typename T> struct CSSValueCreation<SpaceSeparatedPoint<T>> {
    Ref<CSSValue> operator()(const SpaceSeparatedPoint<T>& value)
    {
        return CSSValuePair::create(
            WebCore::CSS::createCSSValue(value.x()),
            WebCore::CSS::createCSSValue(value.y())
        );
    }
};

template<typename T> struct CSSValueCreation<SpaceSeparatedSize<T>> {
    Ref<CSSValue> operator()(const SpaceSeparatedSize<T>& value)
    {
        return CSSValuePair::create(
            WebCore::CSS::createCSSValue(value.width()),
            WebCore::CSS::createCSSValue(value.height())
        );
    }
};

} // namespace CSS
} // namespace WebCore
