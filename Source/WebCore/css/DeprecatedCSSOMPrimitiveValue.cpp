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
    // FIXME: Some of these are never exposed to the web because code elsewhere prevents it. We can ASSERT_NOT_REACHED for those.
    // FIXME: Some of these should return CSS_UNKNOWN instead of hard-coded numbers not mentioned in the IDL file.

    switch (m_value->primitiveType()) {
    case CSSUnitType::CSS_ATTR:                         return CSS_ATTR;
    case CSSUnitType::CSS_CALC:                         return 113;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:  return 115;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:  return 114;
    case CSSUnitType::CSS_CHS:                          return 109;
    case CSSUnitType::CSS_CM:                           return CSS_CM;
    case CSSUnitType::CSS_COUNTER:                      return CSS_COUNTER;
    case CSSUnitType::CSS_COUNTER_NAME:                 return 110;
    case CSSUnitType::CSS_DEG:                          return CSS_DEG;
    case CSSUnitType::CSS_DIMENSION:                    return CSS_DIMENSION;
    case CSSUnitType::CSS_DPCM:                         return 32;
    case CSSUnitType::CSS_DPI:                          return 31;
    case CSSUnitType::CSS_DPPX:                         return 30;
    case CSSUnitType::CSS_EMS:                          return CSS_EMS;
    case CSSUnitType::CSS_EXS:                          return CSS_EXS;
    case CSSUnitType::CSS_FONT_FAMILY:                  return CSS_STRING;
    case CSSUnitType::CSS_FR:                           return 33;
    case CSSUnitType::CSS_GRAD:                         return CSS_GRAD;
    case CSSUnitType::CSS_HZ:                           return CSS_HZ;
    case CSSUnitType::CSS_IDENT:                        return CSS_IDENT;
    case CSSUnitType::CustomIdent:                      return CSS_IDENT;
    case CSSUnitType::CSS_IN:                           return CSS_IN;
    case CSSUnitType::CSS_KHZ:                          return CSS_KHZ;
    case CSSUnitType::CSS_LHS:                          return 35;
    case CSSUnitType::CSS_MM:                           return CSS_MM;
    case CSSUnitType::CSS_MS:                           return CSS_MS;
    case CSSUnitType::CSS_NUMBER:                       return CSS_NUMBER;
    case CSSUnitType::CSS_PAIR:                         return 100;
    case CSSUnitType::CSS_PC:                           return CSS_PC;
    case CSSUnitType::CSS_PERCENTAGE:                   return CSS_PERCENTAGE;
    case CSSUnitType::CSS_PROPERTY_ID:                  return CSS_IDENT;
    case CSSUnitType::CSS_PT:                           return CSS_PT;
    case CSSUnitType::CSS_PX:                           return CSS_PX;
    case CSSUnitType::CSS_Q:                            return 34;
    case CSSUnitType::CSS_QUAD:                         return 112;
    case CSSUnitType::CSS_QUIRKY_EMS:                   return 120;
    case CSSUnitType::CSS_RAD:                          return CSS_RAD;
    case CSSUnitType::CSS_RECT:                         return CSS_RECT;
    case CSSUnitType::CSS_REMS:                         return 108;
    case CSSUnitType::CSS_RGBCOLOR:                     return CSS_RGBCOLOR;
    case CSSUnitType::CSS_RLHS:                         return 36;
    case CSSUnitType::CSS_S:                            return CSS_S;
    case CSSUnitType::CSS_SHAPE:                        return 111;
    case CSSUnitType::CSS_STRING:                       return CSS_STRING;
    case CSSUnitType::CSS_TURN:                         return 107;
    case CSSUnitType::CSS_UNICODE_RANGE:                return 102;
    case CSSUnitType::CSS_UNKNOWN:                      return CSS_UNKNOWN;
    case CSSUnitType::CSS_URI:                          return CSS_URI;
    case CSSUnitType::CSS_VALUE_ID:                     return CSS_IDENT;
    case CSSUnitType::CSS_VH:                           return CSS_VH;
    case CSSUnitType::CSS_VMAX:                         return CSS_VMAX;
    case CSSUnitType::CSS_VMIN:                         return CSS_VMIN;
    case CSSUnitType::CSS_VW:                           return CSS_VW;
    }

    ASSERT_NOT_REACHED();
    return CSS_UNKNOWN;
}

ExceptionOr<float> DeprecatedCSSOMPrimitiveValue::getFloatValue(unsigned short unitType) const
{
    // FIXME: Some of these values do not need to be exposed as a destination type. Remove cases below and give InvalidAccessError instead.

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
    case CSS_VH:            return m_value->getFloatValue(CSSUnitType::CSS_VH);
    case CSS_VMAX:          return m_value->getFloatValue(CSSUnitType::CSS_VMAX);
    case CSS_VMIN:          return m_value->getFloatValue(CSSUnitType::CSS_VMIN);
    case CSS_VW:            return m_value->getFloatValue(CSSUnitType::CSS_VW);

    case 30:                return m_value->getFloatValue(CSSUnitType::CSS_DPPX);
    case 31:                return m_value->getFloatValue(CSSUnitType::CSS_DPI);
    case 32:                return m_value->getFloatValue(CSSUnitType::CSS_DPCM);
    case 33:                return m_value->getFloatValue(CSSUnitType::CSS_FR);
    case 34:                return m_value->getFloatValue(CSSUnitType::CSS_Q);
    case 35:                return m_value->getFloatValue(CSSUnitType::CSS_LHS);
    case 36:                return m_value->getFloatValue(CSSUnitType::CSS_RLHS);
    case 107:               return m_value->getFloatValue(CSSUnitType::CSS_TURN);
    case 108:               return m_value->getFloatValue(CSSUnitType::CSS_REMS);
    case 109:               return m_value->getFloatValue(CSSUnitType::CSS_CHS);
    case 113:               return m_value->getFloatValue(CSSUnitType::CSS_CALC);
    case 114:               return m_value->getFloatValue(CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER);
    case 115:               return m_value->getFloatValue(CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH);
    case 120:               return m_value->getFloatValue(CSSUnitType::CSS_QUIRKY_EMS);

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
    default:            return Exception { InvalidAccessError };
    }
}

ExceptionOr<Ref<DeprecatedCSSOMCounter>> DeprecatedCSSOMPrimitiveValue::getCounterValue() const
{
    if (primitiveType() != CSS_COUNTER)
        return Exception { InvalidAccessError };
    return DeprecatedCSSOMCounter::create(*m_value->counterValue(), m_owner);
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
