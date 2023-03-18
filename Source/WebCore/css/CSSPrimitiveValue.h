/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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

#include "CSSPropertyNames.h"
#include "CSSUnits.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "ExceptionOr.h"
#include "LayoutUnit.h"
#include <utility>
#include <wtf/Forward.h>
#include <wtf/MathExtras.h>

namespace WebCore {

class CSSCalcValue;
class CSSToLengthConversionData;
class FontCascadeDescription;
class FontMetrics;
class RenderView;

// Max/min values for CSS, needs to slightly smaller/larger than the true max/min values to allow for rounding without overflowing.
// Subtract two (rather than one) to allow for values to be converted to float and back without exceeding the LayoutUnit::max.
const int maxValueForCssLength = intMaxForLayoutUnit - 2;
const int minValueForCssLength = intMinForLayoutUnit + 2;

// Dimension calculations are imprecise, often resulting in values of e.g.
// 44.99998. We need to round if we're really close to the next integer value.
template<typename T> inline T roundForImpreciseConversion(double value)
{
    value += (value < 0) ? -0.01 : +0.01;
    return ((value > std::numeric_limits<T>::max()) || (value < std::numeric_limits<T>::min())) ? 0 : static_cast<T>(value);
}

template<> inline float roundForImpreciseConversion(double value)
{
    double ceiledValue = ceil(value);
    double proximityToNextInt = ceiledValue - value;
    if (proximityToNextInt <= 0.01 && value > 0)
        return static_cast<float>(ceiledValue);
    if (proximityToNextInt >= 0.99 && value < 0)
        return static_cast<float>(floor(value));
    return static_cast<float>(value);
}

class CSSPrimitiveValue final : public CSSValue {
public:
    static constexpr bool isLength(CSSUnitType);
    static double computeDegrees(CSSUnitType, double angle);

    // FIXME: Some of these use primitiveUnitType() and some use primitiveType(). Many that use primitiveUnitType() are likely broken with calc().
    bool isAngle() const { return unitCategory(primitiveType()) == CSSUnitCategory::Angle; }
    bool isFontIndependentLength() const { return isFontIndependentLength(primitiveUnitType()); }
    bool isFontRelativeLength() const { return isFontRelativeLength(primitiveUnitType()); }
    bool isParentFontRelativeLength() const { return isPercentage() || (isFontRelativeLength() && primitiveType() != CSSUnitType::CSS_REMS && primitiveType() != CSSUnitType::CSS_RLHS); }
    bool isQuirkyEms() const { return primitiveType() == CSSUnitType::CSS_QUIRKY_EMS; }
    bool isLength() const { return isLength(static_cast<CSSUnitType>(primitiveType())); }
    bool isNumber() const { return primitiveType() == CSSUnitType::CSS_NUMBER; }
    bool isInteger() const { return primitiveType() == CSSUnitType::CSS_INTEGER; }
    bool isNumberOrInteger() const { return isNumber() || isInteger(); }
    bool isPercentage() const { return primitiveType() == CSSUnitType::CSS_PERCENTAGE; }
    bool isPx() const { return primitiveType() == CSSUnitType::CSS_PX; }
    bool isTime() const { return unitCategory(primitiveUnitType()) == CSSUnitCategory::Time; }
    bool isFrequency() const { return unitCategory(primitiveType()) == CSSUnitCategory::Frequency; }
    bool isCalculated() const { return primitiveUnitType() == CSSUnitType::CSS_CALC; }
    bool isCalculatedPercentageWithNumber() const { return primitiveType() == CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER; }
    bool isCalculatedPercentageWithLength() const { return primitiveType() == CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH; }
    bool isDotsPerInch() const { return primitiveType() == CSSUnitType::CSS_DPI; }
    bool isDotsPerPixel() const { return primitiveType() == CSSUnitType::CSS_DPPX; }
    bool isDotsPerCentimeter() const { return primitiveType() == CSSUnitType::CSS_DPCM; }
    bool isX() const { return primitiveType() == CSSUnitType::CSS_X; }
    bool isResolution() const { return unitCategory(primitiveType()) == CSSUnitCategory::Resolution; }
    bool isViewportPercentageLength() const { return isViewportPercentageLength(primitiveUnitType()); }
    bool isFlex() const { return primitiveType() == CSSUnitType::CSS_FR; }

    static Ref<CSSPrimitiveValue> create(double);
    static Ref<CSSPrimitiveValue> create(double, CSSUnitType);
    static Ref<CSSPrimitiveValue> createInteger(double);
    static Ref<CSSPrimitiveValue> create(Ref<CSSCalcValue>&&);

    static Ref<CSSPrimitiveValue> create(const Length&);
    static Ref<CSSPrimitiveValue> create(const Length&, const RenderStyle&);

    ~CSSPrimitiveValue();

    // FIXME: Consider renaming this to unit.
    CSSUnitType primitiveType() const;

    ExceptionOr<float> getFloatValue(CSSUnitType) const;

    double computeDegrees() const;

    enum TimeUnit { Seconds, Milliseconds };
    template<typename T, TimeUnit timeUnit> T computeTime() const;

    template<typename T> T computeLength(const CSSToLengthConversionData&) const;
    template<int> Length convertToLength(const CSSToLengthConversionData&) const;

    bool convertingToLengthRequiresNonNullStyle(int lengthConversion) const;

    double doubleValue(CSSUnitType) const;

    // It's usually wrong to call this; it can trigger type conversion in calc without sufficient context to resolve relative length units.
    double doubleValue() const;

    double doubleValueDividingBy100IfPercentage() const;

    // These return nullopt for calc, for which range checking is not done at parse time: <https://www.w3.org/TR/css3-values/#calc-range>.
    std::optional<bool> isZero() const;
    std::optional<bool> isPositive() const;
    std::optional<bool> isNegative() const;

    template<typename T> inline T value(CSSUnitType type) const { return clampTo<T>(doubleValue(type)); }
    template<typename T> inline T value() const { return clampTo<T>(doubleValue()); }

    float floatValue(CSSUnitType type) const { return value<float>(type); }
    float floatValue() const { return value<float>(); }

    int intValue(CSSUnitType type) const { return value<int>(type); }
    int intValue() const { return value<int>(); }

    const CSSCalcValue* cssCalcValue() const { return isCalculated() ? m_value.calc : nullptr; }

    String customCSSText() const;

    bool equals(const CSSPrimitiveValue&) const;

    static std::optional<double> conversionToCanonicalUnitsScaleFactor(CSSUnitType);
    static ASCIILiteral unitTypeString(CSSUnitType);

    static double computeUnzoomedNonCalcLengthDouble(CSSUnitType, double value, CSSPropertyID, const FontMetrics* = nullptr, const FontCascadeDescription* = nullptr, const FontCascadeDescription* rootFontDescription = nullptr, const RenderView* = nullptr);
    static double computeNonCalcLengthDouble(const CSSToLengthConversionData&, CSSUnitType, double value);
    // True if computeNonCalcLengthDouble would produce identical results when resolved against both these styles.
    static bool equalForLengthResolution(const RenderStyle&, const RenderStyle&);

    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

private:
    // Encode values in the scalar.
    static constexpr uintptr_t UnitShift = PayloadShift;
    static constexpr uintptr_t IntegerShift = UnitShift + UnitBits;
    static CSSPrimitiveValue* scalarValue(double value, CSSUnitType);
    uintptr_t scalarInteger() const;

    CSSPrimitiveValue(double, CSSUnitType);
    explicit CSSPrimitiveValue(Ref<CSSCalcValue>&&);

    // FIXME: Consider renaming this to rawUnit.
    CSSUnitType primitiveUnitType() const;

    double number() const;
    std::optional<double> doubleValueInternal(CSSUnitType) const;

    double computeLengthDouble(const CSSToLengthConversionData&) const;

    ALWAYS_INLINE String serializeInternal() const;
    NEVER_INLINE String formatNumberValue(ASCIILiteral suffix) const;
    NEVER_INLINE String formatIntegerValue(ASCIILiteral suffix) const;
    static constexpr bool isFontIndependentLength(CSSUnitType);
    static constexpr bool isFontRelativeLength(CSSUnitType);
    static constexpr bool isViewportPercentageLength(CSSUnitType);

    // We can eventually delete these. They help us catch cases which used to be correct.
    void create(CSSValueID) = delete;
    void create(CSSPropertyID) = delete;
    void isAttr() const = delete;
    void isColor() const = delete;
    void isCustomIdent() const = delete;
    void isFontFamily() const = delete;
    void isPair() const = delete;
    void isPropertyID() const = delete;
    void isQuad() const = delete;
    void isRect() const = delete;
    void isString() const = delete;
    void isURI() const = delete;
    void isUnresolvedColor() const = delete;
    void isValueID() const = delete;
    void propertyID() const = delete;
    void stringValue() const = delete;
    void unresolvedColor() const = delete;
    void valueID() const = delete;

    union {
        double number;
        const CSSCalcValue* calc;
    } m_value;
};

// We can eventually delete these. They help us catch cases which used to be correct.
void operator==(const CSSPrimitiveValue&, CSSValueID) = delete;
void operator==(const CSSPrimitiveValue*, CSSValueID) = delete;
void operator==(const RefPtr<CSSPrimitiveValue>&, CSSValueID) = delete;
void operator!=(const CSSPrimitiveValue&, CSSValueID) = delete;
void operator!=(const CSSPrimitiveValue*, CSSValueID) = delete;
void operator!=(const RefPtr<CSSPrimitiveValue>&, CSSValueID) = delete;

template<typename TargetType> constexpr TargetType fromCSSValueID(CSSValueID);

inline CSSUnitType CSSPrimitiveValue::primitiveUnitType() const
{
    if (hasScalarInPointer())
        return static_cast<CSSUnitType>(scalar() >> UnitShift & UnitMask);
    return static_cast<CSSUnitType>(opaque(this)->m_unit);
}

constexpr bool CSSPrimitiveValue::isFontIndependentLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_PX
        || type == CSSUnitType::CSS_CM
        || type == CSSUnitType::CSS_MM
        || type == CSSUnitType::CSS_IN
        || type == CSSUnitType::CSS_PT
        || type == CSSUnitType::CSS_PC;
}

constexpr bool CSSPrimitiveValue::isFontRelativeLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_EMS
        || type == CSSUnitType::CSS_EXS
        || type == CSSUnitType::CSS_LHS
        || type == CSSUnitType::CSS_RLHS
        || type == CSSUnitType::CSS_REMS
        || type == CSSUnitType::CSS_CHS
        || type == CSSUnitType::CSS_IC
        || type == CSSUnitType::CSS_QUIRKY_EMS;
}

constexpr bool CSSPrimitiveValue::isLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_EMS
        || type == CSSUnitType::CSS_EXS
        || type == CSSUnitType::CSS_PX
        || type == CSSUnitType::CSS_CM
        || type == CSSUnitType::CSS_MM
        || type == CSSUnitType::CSS_IN
        || type == CSSUnitType::CSS_PT
        || type == CSSUnitType::CSS_PC
        || type == CSSUnitType::CSS_REMS
        || type == CSSUnitType::CSS_CHS
        || type == CSSUnitType::CSS_IC
        || type == CSSUnitType::CSS_Q
        || type == CSSUnitType::CSS_LHS
        || type == CSSUnitType::CSS_RLHS
        || type == CSSUnitType::CSS_CQW
        || type == CSSUnitType::CSS_CQH
        || type == CSSUnitType::CSS_CQI
        || type == CSSUnitType::CSS_CQB
        || type == CSSUnitType::CSS_CQMIN
        || type == CSSUnitType::CSS_CQMAX
        || isViewportPercentageLength(type)
        || type == CSSUnitType::CSS_QUIRKY_EMS;
}

constexpr bool CSSPrimitiveValue::isViewportPercentageLength(CSSUnitType type)
{
    return type >= CSSUnitType::FirstViewportCSSUnitType && type <= CSSUnitType::LastViewporCSSUnitType;
}

template<typename T, CSSPrimitiveValue::TimeUnit timeUnit> inline T CSSPrimitiveValue::computeTime() const
{
    if (timeUnit == Seconds && primitiveType() == CSSUnitType::CSS_S)
        return value<T>();
    if (timeUnit == Seconds && primitiveType() == CSSUnitType::CSS_MS)
        return value<T>() / 1000;
    if (timeUnit == Milliseconds && primitiveType() == CSSUnitType::CSS_MS)
        return value<T>();
    if (timeUnit == Milliseconds && primitiveType() == CSSUnitType::CSS_S)
        return value<T>() * 1000;
    ASSERT_NOT_REACHED();
    return 0;
}

inline double CSSPrimitiveValue::computeDegrees(CSSUnitType type, double angle)
{
    switch (type) {
    case CSSUnitType::CSS_DEG:
        return angle;
    case CSSUnitType::CSS_RAD:
        return rad2deg(angle);
    case CSSUnitType::CSS_GRAD:
        return grad2deg(angle);
    case CSSUnitType::CSS_TURN:
        return turn2deg(angle);
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrimitiveValue, isPrimitiveValue())
