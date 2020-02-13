/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "Color.h"
#include "ExceptionOr.h"
#include "LayoutUnit.h"
#include <utility>
#include <wtf/Forward.h>
#include <wtf/MathExtras.h>

namespace WebCore {

class CSSBasicShape;
class CSSCalcValue;
class CSSToLengthConversionData;
class Counter;
class DeprecatedCSSOMPrimitiveValue;
class Pair;
class Quad;
class RGBColor;
class Rect;
class RenderStyle;

struct CSSFontFamily;
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

    static bool isFontRelativeLength(CSSUnitType);
    static bool isLength(CSSUnitType);
    static bool isResolution(CSSUnitType);
    static bool isViewportPercentageLength(CSSUnitType type) { return type >= CSSUnitType::CSS_VW && type <= CSSUnitType::CSS_VMAX; }

    // FIXME: Some of these use primitiveUnitType() and some use primitiveType(). Those that use primitiveUnitType() are broken with calc().
    bool isAngle() const { return unitCategory(primitiveType()) == CSSUnitCategory::Angle; }
    bool isAttr() const { return primitiveUnitType() == CSSUnitType::CSS_ATTR; }
    bool isCounter() const { return primitiveUnitType() == CSSUnitType::CSS_COUNTER; }
    bool isFontIndependentLength() const { return primitiveUnitType() >= CSSUnitType::CSS_PX && primitiveUnitType() <= CSSUnitType::CSS_PC; }
    bool isFontRelativeLength() const { return isFontRelativeLength(primitiveUnitType()); }
    bool isQuirkyEms() const { return primitiveType() == CSSUnitType::CSS_QUIRKY_EMS; }
    bool isLength() const { return isLength(static_cast<CSSUnitType>(primitiveType())); }
    bool isNumber() const { return primitiveType() == CSSUnitType::CSS_NUMBER; }
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
    bool isResolution() const { return unitCategory(primitiveType()) == CSSUnitCategory::Resolution; }
    bool isViewportPercentageLength() const { return isViewportPercentageLength(primitiveUnitType()); }
    bool isViewportPercentageWidth() const { return primitiveUnitType() == CSSUnitType::CSS_VW; }
    bool isViewportPercentageHeight() const { return primitiveUnitType() == CSSUnitType::CSS_VH; }
    bool isViewportPercentageMax() const { return primitiveUnitType() == CSSUnitType::CSS_VMAX; }
    bool isViewportPercentageMin() const { return primitiveUnitType() == CSSUnitType::CSS_VMIN; }
    bool isValueID() const { return primitiveUnitType() == CSSUnitType::CSS_VALUE_ID; }
    bool isFlex() const { return primitiveType() == CSSUnitType::CSS_FR; }

    static Ref<CSSPrimitiveValue> createIdentifier(CSSValueID valueID) { return adoptRef(*new CSSPrimitiveValue(valueID)); }
    static Ref<CSSPrimitiveValue> createIdentifier(CSSPropertyID propertyID) { return adoptRef(*new CSSPrimitiveValue(propertyID)); }

    static Ref<CSSPrimitiveValue> create(double value, CSSUnitType type) { return adoptRef(*new CSSPrimitiveValue(value, type)); }
    static Ref<CSSPrimitiveValue> create(const String& value, CSSUnitType type) { return adoptRef(*new CSSPrimitiveValue(value, type)); }
    static Ref<CSSPrimitiveValue> create(const Length& value, const RenderStyle& style) { return adoptRef(*new CSSPrimitiveValue(value, style)); }
    static Ref<CSSPrimitiveValue> create(const LengthSize& value, const RenderStyle& style) { return adoptRef(*new CSSPrimitiveValue(value, style)); }

    template<typename T> static Ref<CSSPrimitiveValue> create(T&&);

    // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
    // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em.
    // When the quirky value is used, if you're in quirks mode, the margin will collapse away
    // inside a table cell.
    static Ref<CSSPrimitiveValue> createAllowingMarginQuirk(double value, CSSUnitType);

    ~CSSPrimitiveValue();

    void cleanup();

    WEBCORE_EXPORT CSSUnitType primitiveType() const;
    WEBCORE_EXPORT ExceptionOr<void> setFloatValue(CSSUnitType , double);
    WEBCORE_EXPORT ExceptionOr<float> getFloatValue(CSSUnitType) const;
    WEBCORE_EXPORT ExceptionOr<void> setStringValue(CSSUnitType, const String&);
    WEBCORE_EXPORT ExceptionOr<String> getStringValue() const;

    double computeDegrees() const;
    
    enum TimeUnit { Seconds, Milliseconds };
    template<typename T, TimeUnit timeUnit> T computeTime() const;

    template<typename T> T computeLength(const CSSToLengthConversionData&) const;
    template<int> Length convertToLength(const CSSToLengthConversionData&) const;

    bool convertingToLengthRequiresNonNullStyle(int lengthConversion) const;

    double doubleValue(CSSUnitType) const;
    // It's usually wrong to call this; it can trigger type conversion in calc without sufficient context to resolve relative length units.
    double doubleValue() const;

    // These return nullopt for calc, for which range checking is not done at parse time: <https://www.w3.org/TR/css3-values/#calc-range>.
    Optional<bool> isZero() const;
    Optional<bool> isPositive() const;
    Optional<bool> isNegative() const;

    template<typename T> inline T value(CSSUnitType type) const { return clampTo<T>(doubleValue(type)); }
    template<typename T> inline T value() const { return clampTo<T>(doubleValue()); }

    float floatValue(CSSUnitType type) const { return value<float>(type); }
    float floatValue() const { return value<float>(); }

    int intValue(CSSUnitType type) const { return value<int>(type); }
    int intValue() const { return value<int>(); }

    WEBCORE_EXPORT String stringValue() const;

    const Color& color() const { ASSERT(primitiveUnitType() == CSSUnitType::CSS_RGBCOLOR); return *m_value.color; }
    Counter* counterValue() const { return primitiveUnitType() != CSSUnitType::CSS_COUNTER ? nullptr : m_value.counter; }
    CSSCalcValue* cssCalcValue() const { return primitiveUnitType() != CSSUnitType::CSS_CALC ? nullptr : m_value.calc; }
    const CSSFontFamily& fontFamily() const { ASSERT(primitiveUnitType() == CSSUnitType::CSS_FONT_FAMILY); return *m_value.fontFamily; }
    Pair* pairValue() const { return primitiveUnitType() != CSSUnitType::CSS_PAIR ? nullptr : m_value.pair; }
    CSSPropertyID propertyID() const { return primitiveUnitType() == CSSUnitType::CSS_PROPERTY_ID ? m_value.propertyID : CSSPropertyInvalid; }
    Quad* quadValue() const { return primitiveUnitType() != CSSUnitType::CSS_QUAD ? nullptr : m_value.quad; }
    Rect* rectValue() const { return primitiveUnitType() != CSSUnitType::CSS_RECT ? nullptr : m_value.rect; }
    CSSBasicShape* shapeValue() const { return primitiveUnitType() != CSSUnitType::CSS_SHAPE ? nullptr : m_value.shape; }
    CSSValueID valueID() const { return primitiveUnitType() == CSSUnitType::CSS_VALUE_ID ? m_value.valueID : CSSValueInvalid; }

    template<typename T> inline operator T() const; // Defined in CSSPrimitiveValueMappings.h

    String customCSSText() const;

    // FIXME-NEWPARSER: Can ditch the boolean and just use the unit type once old parser is gone.
    bool isQuirkValue() const { return m_isQuirkValue || primitiveType() == CSSUnitType::CSS_QUIRKY_EMS; }

    bool equals(const CSSPrimitiveValue&) const;

    static double conversionToCanonicalUnitsScaleFactor(CSSUnitType);
    static String unitTypeString(CSSUnitType);

    static double computeNonCalcLengthDouble(const CSSToLengthConversionData&, CSSUnitType, double value);
    // True if computeNonCalcLengthDouble would produce identical results when resolved against both these styles.
    static bool equalForLengthResolution(const RenderStyle&, const RenderStyle&);

    Ref<DeprecatedCSSOMPrimitiveValue> createDeprecatedCSSOMPrimitiveWrapper(CSSStyleDeclaration&) const;

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>&) const;
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>&) const;

private:
    friend class CSSValuePool;
    friend LazyNeverDestroyed<CSSPrimitiveValue>;

    CSSPrimitiveValue(CSSValueID);
    CSSPrimitiveValue(CSSPropertyID);
    CSSPrimitiveValue(const Color&);
    CSSPrimitiveValue(const Length&);
    CSSPrimitiveValue(const Length&, const RenderStyle&);
    CSSPrimitiveValue(const LengthSize&, const RenderStyle&);
    CSSPrimitiveValue(const String&, CSSUnitType);
    CSSPrimitiveValue(double, CSSUnitType);

    CSSPrimitiveValue(StaticCSSValueTag, CSSValueID);
    CSSPrimitiveValue(StaticCSSValueTag, const Color&);
    CSSPrimitiveValue(StaticCSSValueTag, double, CSSUnitType);

    template<typename T> CSSPrimitiveValue(T); // Defined in CSSPrimitiveValueMappings.h
    template<typename T> CSSPrimitiveValue(RefPtr<T>&&);
    template<typename T> CSSPrimitiveValue(Ref<T>&&);

    static void create(int); // compile-time guard
    static void create(unsigned); // compile-time guard
    template<typename T> operator T*(); // compile-time guard

    void init(const Length&);
    void init(const LengthSize&, const RenderStyle&);
    void init(Ref<CSSBasicShape>&&);
    void init(RefPtr<CSSCalcValue>&&);
    void init(Ref<Counter>&&);
    void init(Ref<Pair>&&);
    void init(Ref<Quad>&&);
    void init(Ref<Rect>&&);

    CSSUnitType primitiveUnitType() const { return static_cast<CSSUnitType>(m_primitiveUnitType); }
    void setPrimitiveUnitType(CSSUnitType type) { m_primitiveUnitType = static_cast<unsigned>(type); }

    Optional<double> doubleValueInternal(CSSUnitType targetUnitType) const;

    double computeLengthDouble(const CSSToLengthConversionData&) const;

    ALWAYS_INLINE String formatNumberForCustomCSSText() const;
    NEVER_INLINE String formatNumberValue(StringView) const;

    union {
        CSSPropertyID propertyID;
        CSSValueID valueID;
        double num;
        StringImpl* string;
        Counter* counter;
        Rect* rect;
        Quad* quad;
        const Color* color;
        Pair* pair;
        CSSBasicShape* shape;
        CSSCalcValue* calc;
        const CSSFontFamily* fontFamily;
    } m_value;
};

inline bool CSSPrimitiveValue::isFontRelativeLength(CSSUnitType type)
{
    return type == CSSUnitType::CSS_EMS
        || type == CSSUnitType::CSS_EXS
        || type == CSSUnitType::CSS_REMS
        || type == CSSUnitType::CSS_CHS
        || type == CSSUnitType::CSS_QUIRKY_EMS;
}

inline bool CSSPrimitiveValue::isLength(CSSUnitType type)
{
    return (type >= CSSUnitType::CSS_EMS && type <= CSSUnitType::CSS_PC)
        || type == CSSUnitType::CSS_REMS
        || type == CSSUnitType::CSS_CHS
        || type == CSSUnitType::CSS_Q
        || isViewportPercentageLength(type)
        || type == CSSUnitType::CSS_QUIRKY_EMS;
}

inline bool CSSPrimitiveValue::isResolution(CSSUnitType type)
{
    return type >= CSSUnitType::CSS_DPPX && type <= CSSUnitType::CSS_DPCM;
}

template<typename T> inline Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(T&& value)
{
    return adoptRef(*new CSSPrimitiveValue(std::forward<T>(value)));
}

inline Ref<CSSPrimitiveValue> CSSPrimitiveValue::createAllowingMarginQuirk(double value, CSSUnitType type)
{
    auto result = adoptRef(*new CSSPrimitiveValue(value, type));
    result->m_isQuirkValue = true;
    return result;
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

template<typename T> inline CSSPrimitiveValue::CSSPrimitiveValue(RefPtr<T>&& value)
    : CSSValue(PrimitiveClass)
{
    init(WTFMove(value));
}

template<typename T> inline CSSPrimitiveValue::CSSPrimitiveValue(Ref<T>&& value)
    : CSSValue(PrimitiveClass)
{
    init(WTFMove(value));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrimitiveValue, isPrimitiveValue())
