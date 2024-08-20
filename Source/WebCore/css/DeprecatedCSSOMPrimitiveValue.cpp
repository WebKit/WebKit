/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DeprecatedCSSOMPrimitiveValue.h"

#include "CSSCounterValue.h"
#include "CSSRectValue.h"
#include "DeprecatedCSSOMCounter.h"
#include "DeprecatedCSSOMRGBColor.h"
#include "DeprecatedCSSOMRect.h"

namespace WebCore {
    
unsigned short DeprecatedCSSOMPrimitiveValue::primitiveType() const
{
    if (m_value->isCounter())
        return CSS_COUNTER;
    if (m_value->isRect())
        return CSS_RECT;

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(m_value.get());
    if (!primitiveValue)
        return CSS_UNKNOWN;

    switch (primitiveValue->primitiveType()) {
    case CSSUnitType::Attribute:                         return CSS_ATTR;
    case CSSUnitType::Centimeter:                           return CSS_CM;
    case CSSUnitType::Degree:                          return CSS_DEG;
    case CSSUnitType::Dimension:                    return CSS_DIMENSION;
    case CSSUnitType::Em:                           return CSS_EMS;
    case CSSUnitType::Ex:                           return CSS_EXS;
    case CSSUnitType::FontFamily:                  return CSS_STRING;
    case CSSUnitType::Gradian:                         return CSS_GRAD;
    case CSSUnitType::Hertz:                           return CSS_HZ;
    case CSSUnitType::Ident:                        return CSS_IDENT;
    case CSSUnitType::Integer:                      return CSS_NUMBER;
    case CSSUnitType::CustomIdent:                      return CSS_IDENT;
    case CSSUnitType::Inch:                           return CSS_IN;
    case CSSUnitType::Kilohertz:                          return CSS_KHZ;
    case CSSUnitType::Millimeter:                           return CSS_MM;
    case CSSUnitType::Millisecond:                           return CSS_MS;
    case CSSUnitType::Number:                       return CSS_NUMBER;
    case CSSUnitType::Pica:                           return CSS_PC;
    case CSSUnitType::Percentage:                   return CSS_PERCENTAGE;
    case CSSUnitType::PropertyID:                  return CSS_IDENT;
    case CSSUnitType::Point:                           return CSS_PT;
    case CSSUnitType::Pixel:                           return CSS_PX;
    case CSSUnitType::Radian:                          return CSS_RAD;
    case CSSUnitType::RgbColor:                     return CSS_RGBCOLOR;
    case CSSUnitType::Second:                            return CSS_S;
    case CSSUnitType::String:                       return CSS_STRING;
    case CSSUnitType::Uri:                          return CSS_URI;
    case CSSUnitType::ValueID:                     return CSS_IDENT;

    // All other, including newer types, should return UNKNOWN.
    default:                                            return CSS_UNKNOWN;
    }
}

ExceptionOr<float> DeprecatedCSSOMPrimitiveValue::getFloatValue(unsigned short unitType) const
{
    auto numericType = [&]() -> std::optional<CSSUnitType> {
        switch (unitType) {
        case CSS_CM:            return CSSUnitType::Centimeter;
        case CSS_DEG:           return CSSUnitType::Degree;
        case CSS_DIMENSION:     return CSSUnitType::Dimension;
        case CSS_EMS:           return CSSUnitType::Em;
        case CSS_EXS:           return CSSUnitType::Ex;
        case CSS_GRAD:          return CSSUnitType::Gradian;
        case CSS_HZ:            return CSSUnitType::Hertz;
        case CSS_IN:            return CSSUnitType::Inch;
        case CSS_KHZ:           return CSSUnitType::Kilohertz;
        case CSS_MM:            return CSSUnitType::Millimeter;
        case CSS_MS:            return CSSUnitType::Millisecond;
        case CSS_NUMBER:        return CSSUnitType::Number;
        case CSS_PC:            return CSSUnitType::Pica;
        case CSS_PERCENTAGE:    return CSSUnitType::Percentage;
        case CSS_PT:            return CSSUnitType::Point;
        case CSS_PX:            return CSSUnitType::Pixel;
        case CSS_RAD:           return CSSUnitType::Radian;
        case CSS_S:             return CSSUnitType::Second;
        default:                return std::nullopt;
        }
    }();

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(m_value.get());
    if (!numericType || !primitiveValue)
        return Exception { ExceptionCode::InvalidAccessError };
    return primitiveValue->getFloatValue(*numericType);
}

ExceptionOr<String> DeprecatedCSSOMPrimitiveValue::getStringValue() const
{
    switch (primitiveType()) {
    case CSS_ATTR:      return downcast<CSSPrimitiveValue>(m_value.get()).stringValue();
    case CSS_IDENT:     return downcast<CSSPrimitiveValue>(m_value.get()).stringValue();
    case CSS_STRING:    return downcast<CSSPrimitiveValue>(m_value.get()).stringValue();
    case CSS_URI:       return downcast<CSSPrimitiveValue>(m_value.get()).stringValue();

    // All other, including newer types, should raise an exception.
    default:            return Exception { ExceptionCode::InvalidAccessError };
    }
}

ExceptionOr<Ref<DeprecatedCSSOMCounter>> DeprecatedCSSOMPrimitiveValue::getCounterValue() const
{
    if (auto* value = dynamicDowncast<CSSCounterValue>(m_value.get()))
        return DeprecatedCSSOMCounter::create(value->identifier(), value->separator(), value->counterStyleCSSText());
    return Exception { ExceptionCode::InvalidAccessError };
}
    
ExceptionOr<Ref<DeprecatedCSSOMRect>> DeprecatedCSSOMPrimitiveValue::getRectValue() const
{
    if (auto* rectValue = dynamicDowncast<CSSRectValue>(m_value.get()))
        return DeprecatedCSSOMRect::create(rectValue->rect(), m_owner);
    return Exception { ExceptionCode::InvalidAccessError };
}

ExceptionOr<Ref<DeprecatedCSSOMRGBColor>> DeprecatedCSSOMPrimitiveValue::getRGBColorValue() const
{
    if (primitiveType() != CSS_RGBCOLOR)
        return Exception { ExceptionCode::InvalidAccessError };
    return DeprecatedCSSOMRGBColor::create(m_owner, downcast<CSSPrimitiveValue>(m_value.get()).color());
}

}
