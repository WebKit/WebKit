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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionCode.h"
#include "SVGLengthValue.h"
#include "SVGPropertyTearOff.h"

namespace WebCore {

class SVGLength : public SVGPropertyTearOff<SVGLengthValue> {
public:
    // Forward declare these enums in the w3c naming scheme, for IDL generation
    enum {
        SVG_LENGTHTYPE_UNKNOWN = LengthTypeUnknown,
        SVG_LENGTHTYPE_NUMBER = LengthTypeNumber,
        SVG_LENGTHTYPE_PERCENTAGE = LengthTypePercentage,
        SVG_LENGTHTYPE_EMS = LengthTypeEMS,
        SVG_LENGTHTYPE_EXS = LengthTypeEXS,
        SVG_LENGTHTYPE_PX = LengthTypePX,
        SVG_LENGTHTYPE_CM = LengthTypeCM,
        SVG_LENGTHTYPE_MM = LengthTypeMM,
        SVG_LENGTHTYPE_IN = LengthTypeIN,
        SVG_LENGTHTYPE_PT = LengthTypePT,
        SVG_LENGTHTYPE_PC = LengthTypePC
    };

    static Ref<SVGLength> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGLengthValue& value)
    {
        return adoptRef(*new SVGLength(animatedProperty, role, value));
    }

    static Ref<SVGLength> create(const SVGLengthValue& initialValue = { })
    {
        return adoptRef(*new SVGLength(initialValue));
    }

    static Ref<SVGLength> create(const SVGLengthValue* initialValue)
    {
        return adoptRef(*new SVGLength(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGLength>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    unsigned short unitType()
    {
        return propertyReference().unitType();
    }

    ExceptionOr<float> valueForBindings()
    {
        return propertyReference().valueForBindings(SVGLengthContext { contextElement() });
    }

    ExceptionOr<void> setValueForBindings(float value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().setValue(value, SVGLengthContext { contextElement() });
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }
    
    float valueInSpecifiedUnits()
    {
        return propertyReference().valueInSpecifiedUnits();
    }

    ExceptionOr<void> setValueInSpecifiedUnits(float valueInSpecifiedUnits)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setValueInSpecifiedUnits(valueInSpecifiedUnits);
        commitChange();
        
        return { };
    }
    
    String valueAsString()
    {
        return propertyReference().valueAsString();
    }

    ExceptionOr<void> setValueAsString(const String& value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().setValueAsString(value);
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }

    ExceptionOr<void> newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().newValueSpecifiedUnits(unitType, valueInSpecifiedUnits);
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }
    
    ExceptionOr<void> convertToSpecifiedUnits(unsigned short unitType)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().convertToSpecifiedUnits(unitType, SVGLengthContext { contextElement() });
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }

private:
    SVGLength(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGLengthValue& value)
        : SVGPropertyTearOff<SVGLengthValue>(&animatedProperty, role, value)
    {
    }

    explicit SVGLength(const SVGLengthValue& initialValue)
        : SVGPropertyTearOff<SVGLengthValue>(initialValue)
    {
    }

    explicit SVGLength(const SVGLengthValue* initialValue)
        : SVGPropertyTearOff<SVGLengthValue>(initialValue)
    {
    }
};

} // namespace WebCore
