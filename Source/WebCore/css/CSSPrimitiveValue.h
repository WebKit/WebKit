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

#include "CSSAnchorValue.h"
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
class Color;
class FontCascade;
class RenderStyle;
class RenderView;

struct Length;

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
    static double computeRadians(CSSUnitType, double angle);

    // FIXME: Some of these use primitiveUnitType() and some use primitiveType(). Many that use primitiveUnitType() are likely broken with calc().
    bool isAngle() const { return unitCategory(primitiveType()) == CSSUnitCategory::Angle; }
    bool isFontIndependentLength() const { return isFontIndependentLength(primitiveUnitType()); }
    bool isFontRelativeLength() const { return isFontRelativeLength(primitiveUnitType()); }
    bool isParentFontRelativeLength() const { return isPercentage() || (isFontRelativeLength() && !isRootFontRelativeLength()); }
    bool isRootFontRelativeLength() const { return isRootFontRelativeLength(primitiveUnitType()); }
    bool isQuirkyEms() const { return primitiveType() == CSSUnitType::QuirkyEm; }
    bool isLength() const { return isLength(static_cast<CSSUnitType>(primitiveType())); }
    bool isNumber() const { return primitiveType() == CSSUnitType::Number; }
    bool isInteger() const { return primitiveType() == CSSUnitType::Integer; }
    bool isNumberOrInteger() const { return isNumber() || isInteger(); }
    bool isPercentage() const { return primitiveType() == CSSUnitType::Percentage; }
    bool isPx() const { return primitiveType() == CSSUnitType::Pixel; }
    bool isTime() const { return unitCategory(primitiveUnitType()) == CSSUnitCategory::Time; }
    bool isFrequency() const { return unitCategory(primitiveType()) == CSSUnitCategory::Frequency; }
    bool isCalculated() const { return primitiveUnitType() == CSSUnitType::Calc; }
    bool isCalculatedPercentageWithNumber() const { return primitiveType() == CSSUnitType::CalcPercentageWithNumber; }
    bool isCalculatedPercentageWithLength() const { return primitiveType() == CSSUnitType::CalcPercentageWithLength; }
    bool isDotsPerInch() const { return primitiveType() == CSSUnitType::DotsPerInch; }
    bool isDotsPerPixel() const { return primitiveType() == CSSUnitType::DotsPerPixel; }
    bool isDotsPerCentimeter() const { return primitiveType() == CSSUnitType::DotsPerCentimeter; }
    bool isX() const { return primitiveType() == CSSUnitType::MultiplicationFactor; }
    bool isResolution() const { return unitCategory(primitiveType()) == CSSUnitCategory::Resolution; }
    bool isViewportPercentageLength() const { return isViewportPercentageLength(primitiveUnitType()); }
    bool isContainerPercentageLength() const { return isContainerPercentageLength(primitiveUnitType()); }
    bool isFlex() const { return primitiveType() == CSSUnitType::Fraction; }
    bool isAnchor() const { return primitiveType() == CSSUnitType::Anchor; }

    static Ref<CSSPrimitiveValue> create(double);
    static Ref<CSSPrimitiveValue> create(double, CSSUnitType);
    static Ref<CSSPrimitiveValue> createInteger(double);
    static Ref<CSSPrimitiveValue> create(const Length&);
    static Ref<CSSPrimitiveValue> create(const Length&, const RenderStyle&);
    static Ref<CSSPrimitiveValue> create(Ref<CSSCalcValue>);
    static Ref<CSSPrimitiveValue> create(Ref<CSSAnchorValue>);

    static inline Ref<CSSPrimitiveValue> create(CSSValueID);
    bool isValueID() const { return primitiveUnitType() == CSSUnitType::ValueID; }
    CSSValueID valueID() const { return isValueID() ? m_value.valueID : CSSValueInvalid; }

    static Ref<CSSPrimitiveValue> create(CSSPropertyID);
    bool isPropertyID() const { return primitiveUnitType() == CSSUnitType::PropertyID; }
    CSSPropertyID propertyID() const { return isPropertyID() ? m_value.propertyID : CSSPropertyInvalid; }

    bool isString() const { return primitiveUnitType() == CSSUnitType::String; }
    static Ref<CSSPrimitiveValue> create(String);

    static Ref<CSSPrimitiveValue> create(CSSUnresolvedColor);
    bool isUnresolvedColor() const { return primitiveUnitType() == CSSUnitType::UnresolvedColor; }
    const CSSUnresolvedColor& unresolvedColor() const { ASSERT(isUnresolvedColor()); return *m_value.unresolvedColor; }

    static Ref<CSSPrimitiveValue> createAttr(String);
    bool isAttr() const { return primitiveUnitType() == CSSUnitType::Attribute; }

    bool isColor() const { return primitiveUnitType() == CSSUnitType::RgbColor; }
    const Color& color() const { ASSERT(isColor()); return *reinterpret_cast<const Color*>(&m_value.colorAsInteger); }

    // Return an absolute color if possible, otherwise an invalid color.
    // https://drafts.csswg.org/css-color-5/#absolute-color
    Color absoluteColor() const;

    static Ref<CSSPrimitiveValue> createCustomIdent(String);
    bool isCustomIdent() const { return primitiveUnitType() == CSSUnitType::CustomIdent; }

    static Ref<CSSPrimitiveValue> createFontFamily(String);
    bool isFontFamily() const { return primitiveUnitType() == CSSUnitType::FontFamily; }

    static Ref<CSSPrimitiveValue> createURI(String);
    bool isURI() const { return primitiveUnitType() == CSSUnitType::Uri; }

    static inline CSSPrimitiveValue& implicitInitialValue();

    ~CSSPrimitiveValue();

    CSSUnitType primitiveType() const;
    ExceptionOr<float> getFloatValue(CSSUnitType) const;

    double computeDegrees() const;

    enum TimeUnit { Seconds, Milliseconds };
    template<typename T, TimeUnit timeUnit> T computeTime() const;

    template<typename T> T computeLength(const CSSToLengthConversionData&) const;
    template<int> Length convertToLength(const CSSToLengthConversionData&) const;

    bool convertingToLengthHasRequiredConversionData(int lengthConversion, const CSSToLengthConversionData&) const;

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

    WEBCORE_EXPORT String stringValue() const;

    const CSSCalcValue* cssCalcValue() const { return isCalculated() ? m_value.calc : nullptr; }

    const CSSAnchorValue* cssAnchorValue() const { return isAnchor() ? m_value.anchor : nullptr; }

    String customCSSText() const;

    bool equals(const CSSPrimitiveValue&) const;

    static std::optional<double> conversionToCanonicalUnitsScaleFactor(CSSUnitType);
    static ASCIILiteral unitTypeString(CSSUnitType);

    static double computeUnzoomedNonCalcLengthDouble(CSSUnitType, double value, CSSPropertyID, const FontCascade* = nullptr, const RenderView* = nullptr);
    static double computeNonCalcLengthDouble(const CSSToLengthConversionData&, CSSUnitType, double value);
    // True if computeNonCalcLengthDouble would produce identical results when resolved against both these styles.
    static bool equalForLengthResolution(const RenderStyle&, const RenderStyle&);

    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>&) const;

private:
    friend class CSSValuePool;
    friend class StaticCSSValuePool;
    friend LazyNeverDestroyed<CSSPrimitiveValue>;
    friend bool CSSValue::addHash(Hasher&) const;

    explicit CSSPrimitiveValue(CSSPropertyID);
    explicit CSSPrimitiveValue(Color);
    explicit CSSPrimitiveValue(const Length&);
    CSSPrimitiveValue(const Length&, const RenderStyle&);
    CSSPrimitiveValue(const String&, CSSUnitType);
    CSSPrimitiveValue(double, CSSUnitType);
    explicit CSSPrimitiveValue(Ref<CSSCalcValue>);
    explicit CSSPrimitiveValue(CSSUnresolvedColor);
    explicit CSSPrimitiveValue(Ref<CSSAnchorValue>);

    CSSPrimitiveValue(StaticCSSValueTag, CSSValueID);
    CSSPrimitiveValue(StaticCSSValueTag, Color);
    CSSPrimitiveValue(StaticCSSValueTag, double, CSSUnitType);
    enum ImplicitInitialValueTag { ImplicitInitialValue };
    CSSPrimitiveValue(StaticCSSValueTag, ImplicitInitialValueTag);

    CSSUnitType primitiveUnitType() const { return static_cast<CSSUnitType>(m_primitiveUnitType); }
    void setPrimitiveUnitType(CSSUnitType type) { m_primitiveUnitType = enumToUnderlyingType(type); }

    std::optional<double> doubleValueInternal(CSSUnitType targetUnitType) const;

    double computeLengthDouble(const CSSToLengthConversionData&) const;

    bool addDerivedHash(Hasher&) const;

    ALWAYS_INLINE String serializeInternal() const;
    NEVER_INLINE String formatNumberValue(ASCIILiteral suffix) const;
    NEVER_INLINE String formatIntegerValue(ASCIILiteral suffix) const;
    static constexpr bool isFontIndependentLength(CSSUnitType);
    static constexpr bool isFontRelativeLength(CSSUnitType);
    static constexpr bool isRootFontRelativeLength(CSSUnitType);
    static constexpr bool isContainerPercentageLength(CSSUnitType);
    static constexpr bool isViewportPercentageLength(CSSUnitType);

    union {
        CSSPropertyID propertyID;
        CSSValueID valueID;
        double number;
        StringImpl* string;
        uint64_t colorAsInteger;
        const CSSUnresolvedColor* unresolvedColor;
        const CSSCalcValue* calc;
        const CSSAnchorValue* anchor;
    } m_value;
};

void formatCSSNumberValue(StringBuilder&, double, ASCIILiteral suffix);
String formatCSSNumberValue(double, ASCIILiteral suffix);

template<typename TargetType> constexpr TargetType fromCSSValueID(CSSValueID);

constexpr bool CSSPrimitiveValue::isFontIndependentLength(CSSUnitType type)
{
    return type == CSSUnitType::Pixel
        || type == CSSUnitType::Centimeter
        || type == CSSUnitType::Millimeter
        || type == CSSUnitType::Inch
        || type == CSSUnitType::Point
        || type == CSSUnitType::Pica;
}

constexpr bool CSSPrimitiveValue::isRootFontRelativeLength(CSSUnitType type)
{
    return type == CSSUnitType::RootCapHeight
        || type == CSSUnitType::RootCharacterWidth
        || type == CSSUnitType::RootEm
        || type == CSSUnitType::RootEx
        || type == CSSUnitType::RootIdeographicCharacter
        || type == CSSUnitType::RootLineHeight;
}

constexpr bool CSSPrimitiveValue::isFontRelativeLength(CSSUnitType type)
{
    return type == CSSUnitType::Em
        || type == CSSUnitType::Ex
        || type == CSSUnitType::LineHeight
        || type == CSSUnitType::CapHeight
        || type == CSSUnitType::CharacterWidth
        || type == CSSUnitType::IdeographicCharacter
        || type == CSSUnitType::QuirkyEm
        || isRootFontRelativeLength(type);
}

constexpr bool CSSPrimitiveValue::isContainerPercentageLength(CSSUnitType type)
{
    return type == CSSUnitType::ContainerQueryWidth
        || type == CSSUnitType::ContainerQueryHeight
        || type == CSSUnitType::ContainerQueryInline
        || type == CSSUnitType::ContainerQueryBlock
        || type == CSSUnitType::ContainerQueryMinimum
        || type == CSSUnitType::ContainerQueryMaximum;
}

constexpr bool CSSPrimitiveValue::isLength(CSSUnitType type)
{
    return type == CSSUnitType::Em
        || type == CSSUnitType::Ex
        || type == CSSUnitType::Pixel
        || type == CSSUnitType::Centimeter
        || type == CSSUnitType::Millimeter
        || type == CSSUnitType::Inch
        || type == CSSUnitType::Point
        || type == CSSUnitType::Pica
        || type == CSSUnitType::Quarter
        || isFontRelativeLength(type)
        || isViewportPercentageLength(type)
        || isContainerPercentageLength(type)
        || type == CSSUnitType::QuirkyEm;
}

constexpr bool CSSPrimitiveValue::isViewportPercentageLength(CSSUnitType type)
{
    return type >= CSSUnitType::FirstViewportCSSUnitType && type <= CSSUnitType::LastViewportCSSUnitType;
}

template<typename T, CSSPrimitiveValue::TimeUnit timeUnit> inline T CSSPrimitiveValue::computeTime() const
{
    if (timeUnit == Seconds && primitiveType() == CSSUnitType::Second)
        return value<T>();
    if (timeUnit == Seconds && primitiveType() == CSSUnitType::Millisecond)
        return value<T>() / 1000;
    if (timeUnit == Milliseconds && primitiveType() == CSSUnitType::Millisecond)
        return value<T>();
    if (timeUnit == Milliseconds && primitiveType() == CSSUnitType::Second)
        return value<T>() * 1000;
    ASSERT_NOT_REACHED();
    return 0;
}

inline double CSSPrimitiveValue::computeDegrees(CSSUnitType type, double angle)
{
    switch (type) {
    case CSSUnitType::Degree:
        return angle;
    case CSSUnitType::Radian:
        return rad2deg(angle);
    case CSSUnitType::Gradian:
        return grad2deg(angle);
    case CSSUnitType::Turn:
        return turn2deg(angle);
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

inline double CSSPrimitiveValue::computeRadians(CSSUnitType type, double angle)
{
    switch (type) {
    case CSSUnitType::Degree:
        return deg2rad(angle);
    case CSSUnitType::Radian:
        return angle;
    case CSSUnitType::Gradian:
        return grad2rad(angle);
    case CSSUnitType::Turn:
        return turn2rad(angle);
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
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    return primitiveValue ? valueID(*primitiveValue) : CSSValueInvalid;
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

inline bool CSSValue::isValueID() const
{
    auto* value = dynamicDowncast<CSSPrimitiveValue>(*this);
    return value && value->isValueID();
}

inline CSSValueID CSSValue::valueID() const
{
    auto* value = dynamicDowncast<CSSPrimitiveValue>(*this);
    return value ? value->valueID() : CSSValueInvalid;
}

inline bool CSSValue::isColor() const
{
    auto* value = dynamicDowncast<CSSPrimitiveValue>(*this);
    return value && value->isColor();
}

inline const Color& CSSValue::color() const
{
    return downcast<CSSPrimitiveValue>(*this).color();
}

inline bool CSSValue::isCustomIdent() const
{
    auto* value = dynamicDowncast<CSSPrimitiveValue>(*this);
    return value && value->isCustomIdent();
}

inline String CSSValue::customIdent() const
{
    ASSERT(isCustomIdent());
    return downcast<CSSPrimitiveValue>(*this).stringValue();
}

inline bool CSSValue::isInteger() const
{
    auto* value = dynamicDowncast<CSSPrimitiveValue>(*this);
    return value && value->isInteger();
}

inline int CSSValue::integer() const
{
    ASSERT(isInteger());
    return downcast<CSSPrimitiveValue>(*this).intValue();
}

void add(Hasher&, const CSSPrimitiveValue&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrimitiveValue, isPrimitiveValue())
