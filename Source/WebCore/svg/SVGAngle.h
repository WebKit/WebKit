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

#include "SVGAngleValue.h"
#include "SVGPropertyTearOff.h"

namespace WebCore {

class SVGAngle : public SVGPropertyTearOff<SVGAngleValue> {
public:
    static Ref<SVGAngle> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGAngleValue& value)
    {
        return adoptRef(*new SVGAngle(animatedProperty, role, value));
    }

    static Ref<SVGAngle> create(const SVGAngleValue& initialValue = { })
    {
        return adoptRef(*new SVGAngle(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGAngle>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    SVGAngleValue::Type unitType()
    {
        return propertyReference().unitType();
    }

    ExceptionOr<void> setValueForBindings(float value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setValue(value);
        commitChange();

        return { };
    }
    
    float valueForBindings()
    {
        return propertyReference().value();
    }

    ExceptionOr<void> setValueInSpecifiedUnits(float valueInSpecifiedUnits)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setValueInSpecifiedUnits(valueInSpecifiedUnits);
        commitChange();
        
        return { };
    }
    
    float valueInSpecifiedUnits()
    {
        return propertyReference().valueInSpecifiedUnits();
    }

    ExceptionOr<void> setValueAsString(const String& value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        auto result = propertyReference().setValueAsString(value);
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }

    String valueAsString()
    {
        return propertyReference().valueAsString();
    }

    ExceptionOr<void> newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        auto result = propertyReference().newValueSpecifiedUnits(unitType, valueInSpecifiedUnits);
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }
    
    ExceptionOr<void> convertToSpecifiedUnits(unsigned short unitType)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        auto result = propertyReference().convertToSpecifiedUnits(unitType);
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }

private:
    SVGAngle(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGAngleValue& value)
        : SVGPropertyTearOff<SVGAngleValue>(&animatedProperty, role, value)
    {
    }

    explicit SVGAngle(const SVGAngleValue& initialValue)
        : SVGPropertyTearOff<SVGAngleValue>(initialValue)
    {
    }
};

} // namespace WebCore
