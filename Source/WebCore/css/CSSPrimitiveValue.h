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
    enum UnitType {
        CSS_UNKNOWN = 0,
        CSS_NUMBER = 1,
        CSS_PERCENTAGE = 2,
        CSS_EMS = 3,
        CSS_EXS = 4,
        CSS_PX = 5,
        CSS_CM = 6,
        CSS_MM = 7,
        CSS_IN = 8,
        CSS_PT = 9,
        CSS_PC = 10,
        CSS_DEG = 11,
        CSS_RAD = 12,
        CSS_GRAD = 13,
        CSS_MS = 14,
        CSS_S = 15,
        CSS_HZ = 16,
        CSS_KHZ = 17,
        CSS_DIMENSION = 18,
        CSS_STRING = 19,
        CSS_URI = 20,
        CSS_IDENT = 21,
        CSS_ATTR = 22,
        CSS_COUNTER = 23,
        CSS_RECT = 24,
        CSS_RGBCOLOR = 25,
        // From CSS Values and Units. Viewport-percentage Lengths (vw/vh/vmin/vmax).
        CSS_VW = 26,
        CSS_VH = 27,
        CSS_VMIN = 28,
        CSS_VMAX = 29,
        CSS_DPPX = 30,
        CSS_DPI = 31,
        CSS_DPCM = 32,
        CSS_FR = 33,
        CSS_PAIR = 100, // We envision this being exposed as a means of getting computed style values for pairs (border-spacing/radius, background-position, etc.)
        CSS_UNICODE_RANGE = 102,

        // These are from CSS3 Values and Units, but that isn't a finished standard yet
        CSS_TURN = 107,
        CSS_REMS = 108,
        CSS_CHS = 109,

        // This is used internally for counter names (as opposed to counter values)
        CSS_COUNTER_NAME = 110,

        // This is used by the CSS Shapes draft
        CSS_SHAPE = 111,

        // Used by border images.
        CSS_QUAD = 112,

        CSS_CALC = 113,
        CSS_CALC_PERCENTAGE_WITH_NUMBER = 114,
        CSS_CALC_PERCENTAGE_WITH_LENGTH = 115,

        CSS_FONT_FAMILY = 116,

        CSS_PROPERTY_ID = 117,
        CSS_VALUE_ID = 118,
        
        // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
        // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em.
        // When the quirky value is used, if you're in quirks mode, the margin will collapse away
        // inside a table cell. This quirk is specified in the HTML spec but our impl is different.
        CSS_QUIRKY_EMS = 120
    };

    // This enum follows the CSSParser::Units enum augmented with UNIT_FREQUENCY for frequencies.
    enum UnitCategory {
        UNumber,
        UPercent,
        ULength,
        UAngle,
        UTime,
        UFrequency,
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
        UResolution,
#endif
        UOther
    };
    static UnitCategory unitCategory(UnitType);

    bool isAngle() const;
    bool isAttr() const { return m_primitiveUnitType == CSS_ATTR; }
    bool isCounter() const { return m_primitiveUnitType == CSS_COUNTER; }
    bool isFontIndependentLength() const { return m_primitiveUnitType >= CSS_PX && m_primitiveUnitType <= CSS_PC; }
    static bool isFontRelativeLength(UnitType);
    bool isFontRelativeLength() const { return isFontRelativeLength(static_cast<UnitType>(m_primitiveUnitType)); }
    
    bool isQuirkyEms() const { return primitiveType() == UnitType::CSS_QUIRKY_EMS; }

    static bool isViewportPercentageLength(UnitType type) { return type >= CSS_VW && type <= CSS_VMAX; }
    bool isViewportPercentageLength() const { return isViewportPercentageLength(static_cast<UnitType>(m_primitiveUnitType)); }

    static bool isLength(UnitType);
    bool isLength() const { return isLength(static_cast<UnitType>(primitiveType())); }
    bool isNumber() const { return primitiveType() == CSS_NUMBER; }
    bool isPercentage() const { return primitiveType() == CSS_PERCENTAGE; }
    bool isPx() const { return primitiveType() == CSS_PX; }
    bool isRect() const { return m_primitiveUnitType == CSS_RECT; }
    bool isPair() const { return m_primitiveUnitType == CSS_PAIR; }
    bool isPropertyID() const { return m_primitiveUnitType == CSS_PROPERTY_ID; }
    bool isRGBColor() const { return m_primitiveUnitType == CSS_RGBCOLOR; }
    bool isShape() const { return m_primitiveUnitType == CSS_SHAPE; }
    bool isString() const { return m_primitiveUnitType == CSS_STRING; }
    bool isFontFamily() const { return m_primitiveUnitType == CSS_FONT_FAMILY; }
    bool isTime() const { return m_primitiveUnitType == CSS_S || m_primitiveUnitType == CSS_MS; }
    bool isURI() const { return m_primitiveUnitType == CSS_URI; }
    bool isCalculated() const { return m_primitiveUnitType == CSS_CALC; }
    bool isCalculatedPercentageWithNumber() const { return primitiveType() == CSS_CALC_PERCENTAGE_WITH_NUMBER; }
    bool isCalculatedPercentageWithLength() const { return primitiveType() == CSS_CALC_PERCENTAGE_WITH_LENGTH; }
    bool isDotsPerInch() const { return primitiveType() == CSS_DPI; }
    bool isDotsPerPixel() const { return primitiveType() == CSS_DPPX; }
    bool isDotsPerCentimeter() const { return primitiveType() == CSS_DPCM; }

    static bool isResolution(UnitType);
    bool isResolution() const { return isResolution(static_cast<UnitType>(primitiveType())); }
    bool isViewportPercentageWidth() const { return m_primitiveUnitType == CSS_VW; }
    bool isViewportPercentageHeight() const { return m_primitiveUnitType == CSS_VH; }
    bool isViewportPercentageMax() const { return m_primitiveUnitType == CSS_VMAX; }
    bool isViewportPercentageMin() const { return m_primitiveUnitType == CSS_VMIN; }
    bool isValueID() const { return m_primitiveUnitType == CSS_VALUE_ID; }
    bool isFlex() const { return primitiveType() == CSS_FR; }

    static Ref<CSSPrimitiveValue> createIdentifier(CSSValueID valueID) { return adoptRef(*new CSSPrimitiveValue(valueID)); }
    static Ref<CSSPrimitiveValue> createIdentifier(CSSPropertyID propertyID) { return adoptRef(*new CSSPrimitiveValue(propertyID)); }

    static Ref<CSSPrimitiveValue> create(double value, UnitType type) { return adoptRef(*new CSSPrimitiveValue(value, type)); }
    static Ref<CSSPrimitiveValue> create(const String& value, UnitType type) { return adoptRef(*new CSSPrimitiveValue(value, type)); }
    static Ref<CSSPrimitiveValue> create(const Length& value, const RenderStyle& style) { return adoptRef(*new CSSPrimitiveValue(value, style)); }
    static Ref<CSSPrimitiveValue> create(const LengthSize& value, const RenderStyle& style) { return adoptRef(*new CSSPrimitiveValue(value, style)); }

    template<typename T> static Ref<CSSPrimitiveValue> create(T&&);

    // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
    // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em.
    // When the quirky value is used, if you're in quirks mode, the margin will collapse away
    // inside a table cell.
    static Ref<CSSPrimitiveValue> createAllowingMarginQuirk(double value, UnitType);

    ~CSSPrimitiveValue();

    void cleanup();

    WEBCORE_EXPORT unsigned short primitiveType() const;
    WEBCORE_EXPORT ExceptionOr<void> setFloatValue(unsigned short unitType, double floatValue);
    WEBCORE_EXPORT ExceptionOr<float> getFloatValue(unsigned short unitType) const;
    WEBCORE_EXPORT ExceptionOr<void> setStringValue(unsigned short stringType, const String& stringValue);
    WEBCORE_EXPORT ExceptionOr<String> getStringValue() const;
    WEBCORE_EXPORT ExceptionOr<Counter&> getCounterValue() const;
    WEBCORE_EXPORT ExceptionOr<Rect&> getRectValue() const;
    WEBCORE_EXPORT ExceptionOr<Ref<RGBColor>> getRGBColorValue() const;

    double computeDegrees() const;
    
    enum TimeUnit { Seconds, Milliseconds };
    template<typename T, TimeUnit timeUnit> T computeTime() const;

    template<typename T> T computeLength(const CSSToLengthConversionData&) const;
    template<int> Length convertToLength(const CSSToLengthConversionData&) const;

    bool convertingToLengthRequiresNonNullStyle(int lengthConversion) const;

    double doubleValue(UnitType) const;
    double doubleValue() const;

    template<typename T> inline T value(UnitType type) const { return clampTo<T>(doubleValue(type)); }
    template<typename T> inline T value() const { return clampTo<T>(doubleValue()); }

    float floatValue(UnitType type) const { return value<float>(type); }
    float floatValue() const { return value<float>(); }

    int intValue(UnitType type) const { return value<int>(type); }
    int intValue() const { return value<int>(); }

    WEBCORE_EXPORT String stringValue() const;

    const Color& color() const { ASSERT(m_primitiveUnitType == CSS_RGBCOLOR); return *m_value.color; }
    Counter* counterValue() const { return m_primitiveUnitType != CSS_COUNTER ? nullptr : m_value.counter; }
    CSSCalcValue* cssCalcValue() const { return m_primitiveUnitType != CSS_CALC ? nullptr : m_value.calc; }
    const CSSFontFamily& fontFamily() const { ASSERT(m_primitiveUnitType == CSS_FONT_FAMILY); return *m_value.fontFamily; }
    Pair* pairValue() const { return m_primitiveUnitType != CSS_PAIR ? nullptr : m_value.pair; }
    CSSPropertyID propertyID() const { return m_primitiveUnitType == CSS_PROPERTY_ID ? m_value.propertyID : CSSPropertyInvalid; }
    Quad* quadValue() const { return m_primitiveUnitType != CSS_QUAD ? nullptr : m_value.quad; }
    Rect* rectValue() const { return m_primitiveUnitType != CSS_RECT ? nullptr : m_value.rect; }
    CSSBasicShape* shapeValue() const { return m_primitiveUnitType != CSS_SHAPE ? nullptr : m_value.shape; }
    CSSValueID valueID() const { return m_primitiveUnitType == CSS_VALUE_ID ? m_value.valueID : CSSValueInvalid; }

    template<typename T> inline operator T() const; // Defined in CSSPrimitiveValueMappings.h

    String customCSSText() const;

    // FIXME-NEWPARSER: Can ditch the boolean and just use the unit type once old parser is gone.
    bool isQuirkValue() const { return m_isQuirkValue || primitiveType() == CSS_QUIRKY_EMS; }

    bool equals(const CSSPrimitiveValue&) const;

    static UnitType canonicalUnitTypeForCategory(UnitCategory);
    static double conversionToCanonicalUnitsScaleFactor(UnitType);

    static double computeNonCalcLengthDouble(const CSSToLengthConversionData&, UnitType, double value);

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
    CSSPrimitiveValue(const String&, UnitType);
    CSSPrimitiveValue(double, UnitType);

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

    Optional<double> doubleValueInternal(UnitType targetUnitType) const;

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

inline bool CSSPrimitiveValue::isAngle() const
{
    auto primitiveType = this->primitiveType();
    return primitiveType == CSS_DEG
        || primitiveType == CSS_RAD
        || primitiveType == CSS_GRAD
        || primitiveType == CSS_TURN;
}

inline bool CSSPrimitiveValue::isFontRelativeLength(UnitType type)
{
    return type == CSS_EMS
        || type == CSS_EXS
        || type == CSS_REMS
        || type == CSS_CHS
        || type == CSS_QUIRKY_EMS;
}

inline bool CSSPrimitiveValue::isLength(UnitType type)
{
    return (type >= CSS_EMS && type <= CSS_PC)
        || type == CSS_REMS
        || type == CSS_CHS
        || isViewportPercentageLength(type)
        || type == CSS_QUIRKY_EMS;
}

inline bool CSSPrimitiveValue::isResolution(UnitType type)
{
    return type >= CSS_DPPX && type <= CSS_DPCM;
}

template<typename T> inline Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(T&& value)
{
    return adoptRef(*new CSSPrimitiveValue(std::forward<T>(value)));
}

inline Ref<CSSPrimitiveValue> CSSPrimitiveValue::createAllowingMarginQuirk(double value, UnitType type)
{
    auto result = adoptRef(*new CSSPrimitiveValue(value, type));
    result->m_isQuirkValue = true;
    return result;
}

template<typename T, CSSPrimitiveValue::TimeUnit timeUnit> inline T CSSPrimitiveValue::computeTime() const
{
    if (timeUnit == Seconds && primitiveType() == CSS_S)
        return value<T>();
    if (timeUnit == Seconds && primitiveType() == CSS_MS)
        return value<T>() / 1000;
    if (timeUnit == Milliseconds && primitiveType() == CSS_MS)
        return value<T>();
    if (timeUnit == Milliseconds && primitiveType() == CSS_S)
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
