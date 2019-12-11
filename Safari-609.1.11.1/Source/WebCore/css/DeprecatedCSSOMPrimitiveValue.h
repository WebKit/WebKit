/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSPrimitiveValue.h"
#include "DeprecatedCSSOMValue.h"

namespace WebCore {

class DeprecatedCSSOMCounter;
class DeprecatedCSSOMRGBColor;
class DeprecatedCSSOMRect;
    
class DeprecatedCSSOMPrimitiveValue : public DeprecatedCSSOMValue {
public:
    // Only expose what's in the IDL file.
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
        CSS_VW = 26,
        CSS_VH = 27,
        CSS_VMIN = 28,
        CSS_VMAX = 29
    };

    static Ref<DeprecatedCSSOMPrimitiveValue> create(const CSSPrimitiveValue& value, CSSStyleDeclaration& owner)
    {
        return adoptRef(*new DeprecatedCSSOMPrimitiveValue(value, owner));
    }

    bool equals(const DeprecatedCSSOMPrimitiveValue& other) const { return m_value->equals(other.m_value); }
    unsigned cssValueType() const { return m_value->cssValueType(); }
    String cssText() const { return m_value->cssText(); }
    
    WEBCORE_EXPORT unsigned short primitiveType() const;
    WEBCORE_EXPORT ExceptionOr<void> setFloatValue(unsigned short unitType, double);
    WEBCORE_EXPORT ExceptionOr<float> getFloatValue(unsigned short unitType) const;
    WEBCORE_EXPORT ExceptionOr<void> setStringValue(unsigned short stringType, const String&);
    WEBCORE_EXPORT ExceptionOr<String> getStringValue() const;
    WEBCORE_EXPORT ExceptionOr<Ref<DeprecatedCSSOMCounter>> getCounterValue() const;
    WEBCORE_EXPORT ExceptionOr<Ref<DeprecatedCSSOMRect>> getRectValue() const;
    WEBCORE_EXPORT ExceptionOr<Ref<DeprecatedCSSOMRGBColor>> getRGBColorValue() const;

    String stringValue() const { return m_value->stringValue(); }

protected:
    DeprecatedCSSOMPrimitiveValue(const CSSPrimitiveValue& value, CSSStyleDeclaration& owner)
        : DeprecatedCSSOMValue(DeprecatedPrimitiveValueClass, owner)
        , m_value(const_cast<CSSPrimitiveValue&>(value))
    {
    }

private:
    Ref<CSSPrimitiveValue> m_value;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSSOM_VALUE(DeprecatedCSSOMPrimitiveValue, isPrimitiveValue())
