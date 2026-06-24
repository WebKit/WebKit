/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <WebCore/CSSPrimitiveNumericRaw.h>
#include <WebCore/CSSUnevaluatedCalc.h>
#include <WebCore/CSSValue.h>
#include <utility>
#include <wtf/Forward.h>

namespace WebCore {

class CSSToLengthConversionData;
class DeprecatedCSSOMPrimitiveValue;
class FontCascade;
class RenderView;

template<typename> class ExceptionOr;

class CSSPrimitiveValue final : public CSSValue {
public:
    // FIXME: Some of these use primitiveUnitType() and some use NODELETE primitiveType(). Many that use primitiveUnitType() are likely broken with calc().

    bool isAngle() const { return unitCategory(primitiveType()) == CSSUnitCategory::Angle; }
    bool isFontIndependentLength() const { return isFontIndependentLength(primitiveUnitType()); }
    bool isFontRelativeLength() const { return isFontRelativeLength(primitiveUnitType()); }
    bool isParentFontRelativeLength() const { return isParentFontRelativeLength(primitiveUnitType()); }
    bool isPercentageOrParentFontRelativeLength() const { return isPercentage() || isParentFontRelativeLength(); }
    bool isRootFontRelativeLength() const { return isRootFontRelativeLength(primitiveUnitType()); }
    bool isLength() const { return isLength(static_cast<CSSUnitType>(primitiveType())); }
    bool isNumber() const { return primitiveType() == CSSUnitType::CSS_NUMBER; }
    bool isInteger() const { return primitiveType() == CSSUnitType::CSS_INTEGER; }
    bool isNumberOrInteger() const { return isNumber() || isInteger(); }
    bool isPercentage() const { return primitiveType() == CSSUnitType::CSS_PERCENTAGE; }
    bool isPx() const { return primitiveType() == CSSUnitType::CSS_PX; }
    bool isCalculated() const { return primitiveUnitType() == CSSUnitType::CSS_CALC; }
    bool isCalculatedPercentageWithLength() const { return primitiveType() == CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH; }
    bool isFlex() const { return primitiveType() == CSSUnitType::CSS_FR; }

    static Ref<CSSPrimitiveValue> create(double);
    static Ref<CSSPrimitiveValue> create(double, CSSUnitType);
    static Ref<CSSPrimitiveValue> NODELETE createInteger(double);
    static Ref<CSSPrimitiveValue> create(CSS::UnevaluatedCalcBase);

    ~CSSPrimitiveValue();

    CSSUnitType primitiveType() const;

    using Calc = CSS::UnevaluatedCalcBase;
    using Raw = CSS::UnconstrainedPrimitiveNumericRaw;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        if (isCalculated())
            SUPPRESS_FORWARD_DECL_ARG return visitor(Calc { const_cast<CSSCalc::Value&>(*m_value.calc) });
        return visitor(Raw { primitiveUnitType(), m_value.number });
    }

    std::optional<Raw> raw() const { return !isCalculated() ? std::optional { Raw { primitiveUnitType(), m_value.number } } : std::nullopt; }

    // These return nullopt for calc, for which range checking is not done at parse time: <https://www.w3.org/TR/css3-values/#calc-range>.
    std::optional<bool> NODELETE isZero() const;
    std::optional<bool> NODELETE isOne() const;
    std::optional<bool> NODELETE isPositive() const;
    std::optional<bool> NODELETE isNegative() const;

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSPrimitiveValue&) const;
    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    Ref<DeprecatedCSSOMValue> customCreateDeprecatedCSSOMWrapper(CSSStyleDeclaration&) const;

private:
    friend class CSSValuePool;
    friend class StaticCSSValuePool;
    friend LazyNeverDestroyed<CSSPrimitiveValue>;
    friend bool CSSValue::addHash(Hasher&) const;

    CSSPrimitiveValue(const String&, CSSUnitType);
    CSSPrimitiveValue(double, CSSUnitType);
    CSSPrimitiveValue(StaticCSSValueTag, double, CSSUnitType);
    CSSPrimitiveValue(CSS::UnevaluatedCalcBase&&);

    CSSUnitType primitiveUnitType() const { return static_cast<CSSUnitType>(m_primitiveUnitType); }
    void setPrimitiveUnitType(CSSUnitType type) { m_primitiveUnitType = std::to_underlying(type); }

    bool NODELETE addDerivedHash(Hasher&) const;

    ALWAYS_INLINE String serializeInternal(const CSS::SerializationContext&) const;
    NEVER_INLINE String formatNumberValue(ASCIILiteral suffix) const;
    NEVER_INLINE String formatIntegerValue(ASCIILiteral suffix) const;

    static constexpr bool isLength(CSSUnitType);
    static constexpr bool isFontIndependentLength(CSSUnitType);
    static constexpr bool isFontRelativeLength(CSSUnitType);
    static constexpr bool isParentFontRelativeLength(CSSUnitType);
    static constexpr bool isRootFontRelativeLength(CSSUnitType);
    static constexpr bool isContainerPercentageLength(CSSUnitType);
    static constexpr bool isViewportPercentageLength(CSSUnitType);

    const CSSCalc::Value* cssCalcValue() const { return isCalculated() ? m_value.calc : nullptr; }

    union {
        double number;
        const CSSCalc::Value* calc;
    } m_value;
};

constexpr bool CSSPrimitiveValue::isFontIndependentLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_PX
        || type == CSSUnitType::CSS_CM
        || type == CSSUnitType::CSS_MM
        || type == CSSUnitType::CSS_IN
        || type == CSSUnitType::CSS_PT
        || type == CSSUnitType::CSS_PC;
}

constexpr bool CSSPrimitiveValue::isParentFontRelativeLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_EM
        || type == CSSUnitType::CSS_EX
        || type == CSSUnitType::CSS_LH
        || type == CSSUnitType::CSS_CAP
        || type == CSSUnitType::CSS_CH
        || type == CSSUnitType::CSS_IC
        || type == CSSUnitType::CSS_QUIRKY_EM;
}

constexpr bool CSSPrimitiveValue::isRootFontRelativeLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_RCAP
        || type == CSSUnitType::CSS_RCH
        || type == CSSUnitType::CSS_REM
        || type == CSSUnitType::CSS_REX
        || type == CSSUnitType::CSS_RIC
        || type == CSSUnitType::CSS_RLH;
}

constexpr bool CSSPrimitiveValue::isFontRelativeLength(CSSUnitType type)
{
    return isParentFontRelativeLength(type)
        || isRootFontRelativeLength(type);
}

constexpr bool CSSPrimitiveValue::isContainerPercentageLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_CQW
        || type == CSSUnitType::CSS_CQH
        || type == CSSUnitType::CSS_CQI
        || type == CSSUnitType::CSS_CQB
        || type == CSSUnitType::CSS_CQMIN
        || type == CSSUnitType::CSS_CQMAX;
}

constexpr bool CSSPrimitiveValue::isLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_EM
        || type == CSSUnitType::CSS_EX
        || type == CSSUnitType::CSS_PX
        || type == CSSUnitType::CSS_CM
        || type == CSSUnitType::CSS_MM
        || type == CSSUnitType::CSS_IN
        || type == CSSUnitType::CSS_PT
        || type == CSSUnitType::CSS_PC
        || type == CSSUnitType::CSS_Q
        || isFontRelativeLength(type)
        || isViewportPercentageLength(type)
        || isContainerPercentageLength(type)
        || type == CSSUnitType::CSS_QUIRKY_EM;
}

constexpr bool CSSPrimitiveValue::isViewportPercentageLength(CSSUnitType type)
{
    return type >= CSSUnitType::FirstViewportCSSUnitType && type <= CSSUnitType::LastViewportCSSUnitType;
}

void add(Hasher&, const CSSPrimitiveValue&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrimitiveValue, isPrimitiveValue())
