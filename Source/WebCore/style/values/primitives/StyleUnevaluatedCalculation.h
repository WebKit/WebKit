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

#include <WebCore/StylePrimitiveNumericConcepts.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WebCore {
namespace Style {

namespace Calculation {
class Value;
struct Child;
Ref<Value> CLANG_POINTER_CONVERSION protect(Value&);
}

struct ZoomFactor;
struct ZoomNeeded;

// Non-generic base type to allow code sharing and out-of-line definitions.
class UnevaluatedCalculationBase {
public:
    explicit UnevaluatedCalculationBase(Calculation::Value&);
    explicit UnevaluatedCalculationBase(Ref<Calculation::Value>&&);
    explicit UnevaluatedCalculationBase(Calculation::Child&&);

    WEBCORE_EXPORT UnevaluatedCalculationBase(const UnevaluatedCalculationBase&);
    WEBCORE_EXPORT UnevaluatedCalculationBase(UnevaluatedCalculationBase&&);
    UnevaluatedCalculationBase& operator=(const UnevaluatedCalculationBase&);
    UnevaluatedCalculationBase& operator=(UnevaluatedCalculationBase&&);

    WEBCORE_EXPORT ~UnevaluatedCalculationBase();

    Calculation::Value& calculation() const { return m_calc; }
    [[nodiscard]] Calculation::Value& NODELETE leakRef();

    bool equal(const UnevaluatedCalculationBase&) const;

protected:
    double evaluateBase(CSS::Range, double percentageBasis, const ZoomFactor&) const;
    double evaluateBase(CSS::Range, double percentageBasis, const ZoomNeeded&) const;

private:
    Ref<Calculation::Value> m_calc;
};

// Wrapper for `Ref<Calculation::Value>` that includes range and category as part of the type.
template<CSS::Numeric CSSType> struct UnevaluatedCalculation : UnevaluatedCalculationBase {
    using UnevaluatedCalculationBase::UnevaluatedCalculationBase;
    using UnevaluatedCalculationBase::operator=;

    using CSS = CSSType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    explicit UnevaluatedCalculation(Calculation::Value& calculationValue)
        : UnevaluatedCalculationBase(calculationValue)
    {
    }

    explicit UnevaluatedCalculation(Calculation::Child&& child)
        : UnevaluatedCalculationBase(WTF::move(child))
    {
    }

    explicit UnevaluatedCalculation(UnevaluatedCalculationBase&& base)
        : UnevaluatedCalculationBase(WTF::move(base))
    {
    }

    explicit UnevaluatedCalculation(const UnevaluatedCalculationBase& base)
        : UnevaluatedCalculationBase(base)
    {
    }

    bool operator==(const UnevaluatedCalculation& other) const
    {
        return UnevaluatedCalculationBase::equal(static_cast<const UnevaluatedCalculationBase&>(other));
    }

    double evaluate(double percentageBasis, const ZoomFactor& zoom) const
    {
        return UnevaluatedCalculationBase::evaluateBase(range, percentageBasis, zoom);
    }

    double evaluate(double percentageBasis, const ZoomNeeded& zoomNeeded) const
    {
        return UnevaluatedCalculationBase::evaluateBase(range, percentageBasis, zoomNeeded);
    }
};

WTF::TextStream& operator<<(WTF::TextStream&, const UnevaluatedCalculationBase&);

} // namespace Style
} // namespace WebCore

namespace WTF {
template<WebCore::Style::Calc T> struct IsSmartPtr<T> {
    static constexpr bool value = true;
};

} // namespace WTF
