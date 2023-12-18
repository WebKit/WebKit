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
    case CSSUnitType::CSS_ATTR:                         return CSS_ATTR;
    case CSSUnitType::CSS_CM:                           return CSS_CM;
    case CSSUnitType::CSS_DEG:                          return CSS_DEG;
    case CSSUnitType::CSS_DIMENSION:                    return CSS_DIMENSION;
    case CSSUnitType::CSS_EM:                           return CSS_EMS;
    case CSSUnitType::CSS_EX:                           return CSS_EXS;
    case CSSUnitType::CSS_FONT_FAMILY:                  return CSS_STRING;
    case CSSUnitType::CSS_GRAD:                         return CSS_GRAD;
    case CSSUnitType::CSS_HZ:                           return CSS_HZ;
    case CSSUnitType::CSS_IDENT:                        return CSS_IDENT;
    case CSSUnitType::CSS_INTEGER:                      return CSS_NUMBER;
    case CSSUnitType::CustomIdent:                      return CSS_IDENT;
    case CSSUnitType::CSS_IN:                           return CSS_IN;
    case CSSUnitType::CSS_KHZ:                          return CSS_KHZ;
    case CSSUnitType::CSS_MM:                           return CSS_MM;
    case CSSUnitType::CSS_MS:                           return CSS_MS;
    case CSSUnitType::CSS_NUMBER:                       return CSS_NUMBER;
    case CSSUnitType::CSS_PC:                           return CSS_PC;
    case CSSUnitType::CSS_PERCENTAGE:                   return CSS_PERCENTAGE;
    case CSSUnitType::CSS_PROPERTY_ID:                  return CSS_IDENT;
    case CSSUnitType::CSS_PT:                           return CSS_PT;
    case CSSUnitType::CSS_PX:                           return CSS_PX;
    case CSSUnitType::CSS_RAD:                          return CSS_RAD;
    case CSSUnitType::CSS_RGBCOLOR:                     return CSS_RGBCOLOR;
    case CSSUnitType::CSS_S:                            return CSS_S;
    case CSSUnitType::CSS_STRING:                       return CSS_STRING;
    case CSSUnitType::CSS_URI:                          return CSS_URI;
    case CSSUnitType::CSS_VALUE_ID:                     return CSS_IDENT;

    // All other, including newer types, should return UNKNOWN.
    default:                                            return CSS_UNKNOWN;
    }
}

ExceptionOr<float> DeprecatedCSSOMPrimitiveValue::getFloatValue(unsigned short unitType) const
{
    auto numericType = [&]() -> std::optional<CSSUnitType> {
        switch (unitType) {
        case CSS_CM:            return CSSUnitType::CSS_CM;
        case CSS_DEG:           return CSSUnitType::CSS_DEG;
        case CSS_DIMENSION:     return CSSUnitType::CSS_DIMENSION;
        case CSS_EMS:           return CSSUnitType::CSS_EM;
        case CSS_EXS:           return CSSUnitType::CSS_EX;
        case CSS_GRAD:          return CSSUnitType::CSS_GRAD;
        case CSS_HZ:            return CSSUnitType::CSS_HZ;
        case CSS_IN:            return CSSUnitType::CSS_IN;
        case CSS_KHZ:           return CSSUnitType::CSS_KHZ;
        case CSS_MM:            return CSSUnitType::CSS_MM;
        case CSS_MS:            return CSSUnitType::CSS_MS;
        case CSS_NUMBER:        return CSSUnitType::CSS_NUMBER;
        case CSS_PC:            return CSSUnitType::CSS_PC;
        case CSS_PERCENTAGE:    return CSSUnitType::CSS_PERCENTAGE;
        case CSS_PT:            return CSSUnitType::CSS_PT;
        case CSS_PX:            return CSSUnitType::CSS_PX;
        case CSS_RAD:           return CSSUnitType::CSS_RAD;
        case CSS_S:             return CSSUnitType::CSS_S;
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
