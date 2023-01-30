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

class CSSBasicShape;
class CSSCalcValue;
class CSSToLengthConversionData;
class CSSUnresolvedColor;
class Color;
class DeprecatedCSSOMPrimitiveValue;
class FontCascadeDescription;
class FontMetrics;
class Pair;
class Quad;
class RGBColor;
class Rect;
class RenderStyle;
class RenderView;

struct Counter;
struct Length;
struct LengthSize;

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
    bool isAttr() const { return primitiveUnitType() == CSSUnitType::CSS_ATTR; }
    bool isCounter() const { return primitiveUnitType() == CSSUnitType::CSS_COUNTER; }
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
    bool isRect() const { return primitiveUnitType() == CSSUnitType::CSS_RECT; }
    bool isPair() const { return primitiveUnitType() == CSSUnitType::CSS_PAIR; }
    bool isPropertyID() const { return primitiveUnitType() == CSSUnitType::CSS_PROPERTY_ID; }
    bool isRGBColor() const { return primitiveUnitType() == CSSUnitType::CSS_RGBCOLOR; }
    bool isShape() const { return primitiveUnitType() == CSSUnitType::CSS_SHAPE; }
    bool isString() const { return primitiveUnitType() == CSSUnitType::CSS_STRING; }
    bool isFontFamily() const { return primitiveUnitType() == CSSUnitType::CSS_FONT_FAMILY; }
    bool isTime() const { return unitCategory(primitiveUnitType()) == CSSUnitCategory::Time; }
    bool isFrequency() const { return unitCategory(primitiveType()) == CSSUnitCategory::Frequency; }
    bool isURI() const { return primitiveUnitType() == CSSUnitType::CSS_URI; }
    bool isCalculated() const { return primitiveUnitType() == CSSUnitType::CSS_CALC; }
    bool isCalculatedPercentageWithNumber() const { return primitiveType() == CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER; }
    bool isCalculatedPercentageWithLength() const { return primitiveType() == CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH; }
    bool isDotsPerInch() const { return primitiveType() == CSSUnitType::CSS_DPI; }
    bool isDotsPerPixel() const { return primitiveType() == CSSUnitType::CSS_DPPX; }
    bool isDotsPerCentimeter() const { return primitiveType() == CSSUnitType::CSS_DPCM; }
    bool isX() const { return primitiveType() == CSSUnitType::CSS_X; }
    bool isResolution() const { return unitCategory(primitiveType()) == CSSUnitCategory::Resolution; }
    bool isUnresolvedColor() const { return primitiveUnitType() == CSSUnitType::CSS_UNRESOLVED_COLOR; }
    bool isViewportPercentageLength() const { return isViewportPercentageLength(primitiveUnitType()); }
    bool isValueID() const { return primitiveUnitType() == CSSUnitType::CSS_VALUE_ID; }
    bool isFlex() const { return primitiveType() == CSSUnitType::CSS_FR; }
    bool isCustomIdent() const { return primitiveUnitType() == CSSUnitType::CustomIdent; }

    static inline Ref<CSSPrimitiveValue> create(CSSValueID);
    static Ref<CSSPrimitiveValue> create(CSSPropertyID);
    static Ref<CSSPrimitiveValue> create(double, CSSUnitType);
    static Ref<CSSPrimitiveValue> create(const String&, CSSUnitType);
    static Ref<CSSPrimitiveValue> create(const Length&);
    static Ref<CSSPrimitiveValue> create(const Length&, const RenderStyle&);
    static Ref<CSSPrimitiveValue> create(const LengthSize&, const RenderStyle&);
    static Ref<CSSPrimitiveValue> create(Ref<CSSBasicShape>&&);
    static Ref<CSSPrimitiveValue> create(Ref<CSSCalcValue>&&);
    static Ref<CSSPrimitiveValue> create(Ref<Counter>&&);
    static Ref<CSSPrimitiveValue> create(Ref<Pair>&&);
    static Ref<CSSPrimitiveValue> create(Ref<Quad>&&);
    static Ref<CSSPrimitiveValue> create(Ref<Rect>&&);
    static Ref<CSSPrimitiveValue> create(Ref<CSSUnresolvedColor>&&);

    template<typename T> static Ref<CSSPrimitiveValue> create(const T&); // Specializations are in CSSPrimitiveValueMappings.h.

    static Ref<CSSPrimitiveValue> createCustomIdent(const String& value) { return create(value, CSSUnitType::CustomIdent); }

    static inline CSSPrimitiveValue& implicitInitialValue();

    ~CSSPrimitiveValue();

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
    bool isCenterPosition() const;

    template<typename T> inline T value(CSSUnitType type) const { return clampTo<T>(doubleValue(type)); }
    template<typename T> inline T value() const { return clampTo<T>(doubleValue()); }

    float floatValue(CSSUnitType type) const { return value<float>(type); }
    float floatValue() const { return value<float>(); }

    int intValue(CSSUnitType type) const { return value<int>(type); }
    int intValue() const { return value<int>(); }

    WEBCORE_EXPORT String stringValue() const;

    const Color& color() const { ASSERT(primitiveUnitType() == CSSUnitType::CSS_RGBCOLOR); return *reinterpret_cast<const Color*>(&m_value.colorAsInteger); }
    const CSSUnresolvedColor& unresolvedColor() const { ASSERT(primitiveUnitType() == CSSUnitType::CSS_UNRESOLVED_COLOR); return *m_value.unresolvedColor; }
    Counter* counterValue() const { return primitiveUnitType() != CSSUnitType::CSS_COUNTER ? nullptr : m_value.counter; }
    CSSCalcValue* cssCalcValue() const { return primitiveUnitType() != CSSUnitType::CSS_CALC ? nullptr : m_value.calc; }
    Pair* pairValue() const { return primitiveUnitType() != CSSUnitType::CSS_PAIR ? nullptr : m_value.pair; }
    CSSPropertyID propertyID() const { return primitiveUnitType() == CSSUnitType::CSS_PROPERTY_ID ? m_value.propertyID : CSSPropertyInvalid; }
    Quad* quadValue() const { return primitiveUnitType() != CSSUnitType::CSS_QUAD ? nullptr : m_value.quad; }
    Rect* rectValue() const { return primitiveUnitType() != CSSUnitType::CSS_RECT ? nullptr : m_value.rect; }
    CSSBasicShape* shapeValue() const { return primitiveUnitType() != CSSUnitType::CSS_SHAPE ? nullptr : m_value.shape; }
    CSSValueID valueID() const { return primitiveUnitType() == CSSUnitType::CSS_VALUE_ID ? m_value.valueID : CSSValueInvalid; }

    operator unsigned short() const;
    operator int() const;
    operator unsigned() const;
    operator float() const;

    template<typename T> operator T() const; // Specializations are in CSSPrimitiveValueMappings.h.

    String customCSSText() const;

    bool equals(const CSSPrimitiveValue&) const;

    static std::optional<double> conversionToCanonicalUnitsScaleFactor(CSSUnitType);
    static ASCIILiteral unitTypeString(CSSUnitType);

    static double computeUnzoomedNonCalcLengthDouble(CSSUnitType, double value, CSSPropertyID, const FontMetrics* = nullptr, const FontCascadeDescription* = nullptr, const FontCascadeDescription* rootFontDescription = nullptr, const RenderView* = nullptr);
    static double computeNonCalcLengthDouble(const CSSToLengthConversionData&, CSSUnitType, double value);
    // True if computeNonCalcLengthDouble would produce identical results when resolved against both these styles.
    static bool equalForLengthResolution(const RenderStyle&, const RenderStyle&);

    Ref<DeprecatedCSSOMPrimitiveValue> createDeprecatedCSSOMPrimitiveWrapper(CSSStyleDeclaration&) const;

    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

private:
    friend class CSSValuePool;
    friend class StaticCSSValuePool;
    friend LazyNeverDestroyed<CSSPrimitiveValue>;

    explicit CSSPrimitiveValue(CSSPropertyID);
    explicit CSSPrimitiveValue(const Color&);
    explicit CSSPrimitiveValue(const Length&);
    CSSPrimitiveValue(const Length&, const RenderStyle&);
    CSSPrimitiveValue(const LengthSize&, const RenderStyle&);
    CSSPrimitiveValue(const String&, CSSUnitType);
    CSSPrimitiveValue(double, CSSUnitType);
    explicit CSSPrimitiveValue(Ref<CSSBasicShape>&&);
    explicit CSSPrimitiveValue(Ref<CSSCalcValue>&&);
    explicit CSSPrimitiveValue(Ref<Counter>&&);
    explicit CSSPrimitiveValue(Ref<Pair>&&);
    explicit CSSPrimitiveValue(Ref<Quad>&&);
    explicit CSSPrimitiveValue(Ref<Rect>&&);
    explicit CSSPrimitiveValue(Ref<CSSUnresolvedColor>&&);

    CSSPrimitiveValue(StaticCSSValueTag, CSSValueID);
    CSSPrimitiveValue(StaticCSSValueTag, const Color&);
    CSSPrimitiveValue(StaticCSSValueTag, double, CSSUnitType);
    enum ImplicitInitialValueTag { ImplicitInitialValue };
    CSSPrimitiveValue(StaticCSSValueTag, ImplicitInitialValueTag);

    CSSUnitType primitiveUnitType() const { return static_cast<CSSUnitType>(m_primitiveUnitType); }
    void setPrimitiveUnitType(CSSUnitType type) { m_primitiveUnitType = static_cast<unsigned>(type); }

    std::optional<double> doubleValueInternal(CSSUnitType targetUnitType) const;

    double computeLengthDouble(const CSSToLengthConversionData&) const;

    ALWAYS_INLINE String serializeInternal() const;
    NEVER_INLINE String formatNumberValue(ASCIILiteral suffix) const;
    NEVER_INLINE String formatIntegerValue(ASCIILiteral suffix) const;
    static constexpr bool isFontIndependentLength(CSSUnitType);
    static constexpr bool isFontRelativeLength(CSSUnitType);
    static constexpr bool isViewportPercentageLength(CSSUnitType);

    union {
        CSSPropertyID propertyID;
        CSSValueID valueID;
        double number;
        StringImpl* string;
        Counter* counter;
        Rect* rect;
        Quad* quad;
        uint64_t colorAsInteger;
        CSSUnresolvedColor* unresolvedColor;
        Pair* pair;
        CSSBasicShape* shape;
        CSSCalcValue* calc;
    } m_value;
};

template<typename TargetType> constexpr TargetType fromCSSValueID(CSSValueID);

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

inline CSSValueID valueID(const CSSPrimitiveValue& value)
{
    return value.valueID();
}

inline CSSValueID valueID(const CSSPrimitiveValue* value)
{
    return value ? valueID(*value) : CSSValueInvalid;
}

inline CSSValueID valueID(const CSSValue& value)
{
    return value.isPrimitiveValue() ? valueID(downcast<CSSPrimitiveValue>(value)) : CSSValueInvalid;
}

inline CSSValueID valueID(const CSSValue* value)
{
    return value ? valueID(*value) : CSSValueInvalid;
}

inline bool isValueID(const CSSPrimitiveValue& value, CSSValueID id)
{
    return valueID(value) == id;
}

inline bool isValueID(const CSSPrimitiveValue* value, CSSValueID id)
{
    return valueID(value) == id;
}

inline bool isValueID(const RefPtr<CSSPrimitiveValue>& value, CSSValueID id)
{
    return valueID(value.get()) == id;
}

inline bool isValueID(const Ref<CSSPrimitiveValue>& value, CSSValueID id)
{
    return valueID(value.get()) == id;
}

inline bool isValueID(const CSSValue& value, CSSValueID id)
{
    return valueID(value) == id;
}

inline bool isValueID(const CSSValue* value, CSSValueID id)
{
    return valueID(value) == id;
}

inline bool isValueID(const RefPtr<CSSValue>& value, CSSValueID id)
{
    return isValueID(value.get(), id);
}

inline bool isValueID(const Ref<CSSValue>& value, CSSValueID id)
{
    return isValueID(value.get(), id);
}

inline CSSPrimitiveValue::operator unsigned short() const
{
    ASSERT(primitiveType() == CSSUnitType::CSS_NUMBER || primitiveType() == CSSUnitType::CSS_INTEGER);
    return value<unsigned short>();
}

inline CSSPrimitiveValue::operator int() const
{
    ASSERT(primitiveType() == CSSUnitType::CSS_NUMBER || primitiveType() == CSSUnitType::CSS_INTEGER);
    return value<int>();
}

inline CSSPrimitiveValue::operator unsigned() const
{
    ASSERT(primitiveType() == CSSUnitType::CSS_NUMBER || primitiveType() == CSSUnitType::CSS_INTEGER);
    return value<unsigned>();
}

inline CSSPrimitiveValue::operator float() const
{
    ASSERT(primitiveType() == CSSUnitType::CSS_NUMBER || primitiveType() == CSSUnitType::CSS_INTEGER);
    return value<float>();
}

template<typename ConvertibleType> inline Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(const ConvertibleType& value)
{
    return create(toCSSValueID(value));
}

template<typename TargetType> inline CSSPrimitiveValue::operator TargetType() const
{
    return fromCSSValueID<TargetType>(valueID());
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrimitiveValue, isPrimitiveValue())
