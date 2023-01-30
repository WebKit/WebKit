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

#include "DeprecatedCSSOMCounter.h"
#include "DeprecatedCSSOMRGBColor.h"
#include "DeprecatedCSSOMRect.h"

namespace WebCore {
    
unsigned short DeprecatedCSSOMPrimitiveValue::primitiveType() const
{
    switch (m_value->primitiveType()) {
    case CSSUnitType::CSS_ATTR:                         return CSS_ATTR;
    case CSSUnitType::CSS_IC:                           return CSS_UNKNOWN;
    case CSSUnitType::CSS_CM:                           return CSS_CM;
    case CSSUnitType::CSS_COUNTER:                      return CSS_COUNTER;
    case CSSUnitType::CSS_DEG:                          return CSS_DEG;
    case CSSUnitType::CSS_DIMENSION:                    return CSS_DIMENSION;
    case CSSUnitType::CSS_EMS:                          return CSS_EMS;
    case CSSUnitType::CSS_EXS:                          return CSS_EXS;
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
    case CSSUnitType::CSS_RECT:                         return CSS_RECT;
    case CSSUnitType::CSS_RGBCOLOR:                     return CSS_RGBCOLOR;
    case CSSUnitType::CSS_S:                            return CSS_S;
    case CSSUnitType::CSS_STRING:                       return CSS_STRING;
    case CSSUnitType::CSS_UNKNOWN:                      return CSS_UNKNOWN;
    case CSSUnitType::CSS_URI:                          return CSS_URI;
    case CSSUnitType::CSS_VALUE_ID:                     return CSS_IDENT;

    // All other, including newer types, should return UNKNOWN.
    default:                                            return CSS_UNKNOWN;
    }
}

ExceptionOr<float> DeprecatedCSSOMPrimitiveValue::getFloatValue(unsigned short unitType) const
{
    switch (unitType) {
    case CSS_CM:            return m_value->getFloatValue(CSSUnitType::CSS_CM);
    case CSS_DEG:           return m_value->getFloatValue(CSSUnitType::CSS_DEG);
    case CSS_DIMENSION:     return m_value->getFloatValue(CSSUnitType::CSS_DIMENSION);
    case CSS_EMS:           return m_value->getFloatValue(CSSUnitType::CSS_EMS);
    case CSS_EXS:           return m_value->getFloatValue(CSSUnitType::CSS_EXS);
    case CSS_GRAD:          return m_value->getFloatValue(CSSUnitType::CSS_GRAD);
    case CSS_HZ:            return m_value->getFloatValue(CSSUnitType::CSS_HZ);
    case CSS_IN:            return m_value->getFloatValue(CSSUnitType::CSS_IN);
    case CSS_KHZ:           return m_value->getFloatValue(CSSUnitType::CSS_KHZ);
    case CSS_MM:            return m_value->getFloatValue(CSSUnitType::CSS_MM);
    case CSS_MS:            return m_value->getFloatValue(CSSUnitType::CSS_MS);
    case CSS_NUMBER:        return m_value->getFloatValue(CSSUnitType::CSS_NUMBER);
    case CSS_PC:            return m_value->getFloatValue(CSSUnitType::CSS_PC);
    case CSS_PERCENTAGE:    return m_value->getFloatValue(CSSUnitType::CSS_PERCENTAGE);
    case CSS_PT:            return m_value->getFloatValue(CSSUnitType::CSS_PT);
    case CSS_PX:            return m_value->getFloatValue(CSSUnitType::CSS_PX);
    case CSS_RAD:           return m_value->getFloatValue(CSSUnitType::CSS_RAD);
    case CSS_S:             return m_value->getFloatValue(CSSUnitType::CSS_S);

    // All other, including newer types, should raise an exception.
    default:                return Exception { InvalidAccessError };
    }
}

ExceptionOr<String> DeprecatedCSSOMPrimitiveValue::getStringValue() const
{
    switch (primitiveType()) {
    case CSS_ATTR:      return m_value->stringValue();
    case CSS_IDENT:     return m_value->stringValue();
    case CSS_STRING:    return m_value->stringValue();
    case CSS_URI:       return m_value->stringValue();

    // All other, including newer types, should raise an exception.
    default:            return Exception { InvalidAccessError };
    }
}

ExceptionOr<Ref<DeprecatedCSSOMCounter>> DeprecatedCSSOMPrimitiveValue::getCounterValue() const
{
    auto* counter = m_value->counterValue();
    if (!counter)
        return Exception { InvalidAccessError };
    return DeprecatedCSSOMCounter::create(*counter);
}
    
ExceptionOr<Ref<DeprecatedCSSOMRect>> DeprecatedCSSOMPrimitiveValue::getRectValue() const
{
    if (primitiveType() != CSS_RECT)
        return Exception { InvalidAccessError };
    return DeprecatedCSSOMRect::create(*m_value->rectValue(), m_owner);
}

ExceptionOr<Ref<DeprecatedCSSOMRGBColor>> DeprecatedCSSOMPrimitiveValue::getRGBColorValue() const
{
    if (primitiveType() != CSS_RGBCOLOR)
        return Exception { InvalidAccessError };
    return DeprecatedCSSOMRGBColor::create(m_owner, m_value->color());
}

}
