/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StylePrimitiveNumericConcepts.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WebCore {

namespace Calculation {
struct Child;
}

class CalculationValue;

namespace Style {

// Non-generic base type to allow code sharing and out-of-line definitions.
struct UnevaluatedCalculationBase {
    explicit UnevaluatedCalculationBase(CalculationValue&);
    explicit UnevaluatedCalculationBase(Ref<CalculationValue>&&);
    explicit UnevaluatedCalculationBase(Calculation::Child&&, Calculation::Category, CSS::Range);

    WEBCORE_EXPORT UnevaluatedCalculationBase(const UnevaluatedCalculationBase&);
    WEBCORE_EXPORT UnevaluatedCalculationBase(UnevaluatedCalculationBase&&);
    UnevaluatedCalculationBase& operator=(const UnevaluatedCalculationBase&);
    UnevaluatedCalculationBase& operator=(UnevaluatedCalculationBase&&);

    WEBCORE_EXPORT ~UnevaluatedCalculationBase();

    Ref<CalculationValue> protectedCalculation() const;

    bool equal(const UnevaluatedCalculationBase&) const;

private:
    Ref<CalculationValue> calc;
};

// Wrapper for `Ref<CalculationValue>` that includes range and category as part of the type.
template<CSS::Numeric CSSType> struct UnevaluatedCalculation : UnevaluatedCalculationBase {
    using UnevaluatedCalculationBase::UnevaluatedCalculationBase;
    using UnevaluatedCalculationBase::operator=;

    using CSS = CSSType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    explicit UnevaluatedCalculation(CalculationValue& calculationValue)
        : UnevaluatedCalculationBase(calculationValue)
    {
    }

    explicit UnevaluatedCalculation(Calculation::Child&& child)
        : UnevaluatedCalculationBase(WTFMove(child), category, range)
    {
    }

    bool operator==(const UnevaluatedCalculation& other) const
    {
        return UnevaluatedCalculationBase::equal(static_cast<const UnevaluatedCalculationBase&>(other));
    }
};

} // namespace Style
} // namespace WebCore

namespace WTF {
template<WebCore::Style::Calc T> struct IsSmartPtr<T> {
    static constexpr bool value = true;
};

} // namespace WTF
