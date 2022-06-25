/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "SVGLengthContext.h"
#include "SVGLengthValue.h"
#include "SVGValueProperty.h"

namespace WebCore {

class SVGLength : public SVGValueProperty<SVGLengthValue> {
    using Base = SVGValueProperty<SVGLengthValue>;
    using Base::Base;
    using Base::m_value;

public:
    // Forward declare these enums in the w3c naming scheme, for IDL generation
    enum {
        SVG_LENGTHTYPE_UNKNOWN      = static_cast<unsigned>(SVGLengthType::Unknown),
        SVG_LENGTHTYPE_NUMBER       = static_cast<unsigned>(SVGLengthType::Number),
        SVG_LENGTHTYPE_PERCENTAGE   = static_cast<unsigned>(SVGLengthType::Percentage),
        SVG_LENGTHTYPE_EMS          = static_cast<unsigned>(SVGLengthType::Ems),
        SVG_LENGTHTYPE_EXS          = static_cast<unsigned>(SVGLengthType::Exs),
        SVG_LENGTHTYPE_PX           = static_cast<unsigned>(SVGLengthType::Pixels),
        SVG_LENGTHTYPE_CM           = static_cast<unsigned>(SVGLengthType::Centimeters),
        SVG_LENGTHTYPE_MM           = static_cast<unsigned>(SVGLengthType::Millimeters),
        SVG_LENGTHTYPE_IN           = static_cast<unsigned>(SVGLengthType::Inches),
        SVG_LENGTHTYPE_PT           = static_cast<unsigned>(SVGLengthType::Points),
        SVG_LENGTHTYPE_PC           = static_cast<unsigned>(SVGLengthType::Picas)
    };

    static Ref<SVGLength> create()
    {
        return adoptRef(*new SVGLength());
    }

    static Ref<SVGLength> create(const SVGLengthValue& value)
    {
        return adoptRef(*new SVGLength(value));
    }

    static Ref<SVGLength> create(SVGPropertyOwner* owner, SVGPropertyAccess access, const SVGLengthValue& value = { })
    {
        return adoptRef(*new SVGLength(owner, access, value));
    }

    template<typename T>
    static ExceptionOr<Ref<SVGLength>> create(ExceptionOr<T>&& value)
    {
        if (value.hasException())
            return value.releaseException();
        return adoptRef(*new SVGLength(value.releaseReturnValue()));
    }

    Ref<SVGLength> clone() const
    {
        return SVGLength::create(m_value);
    }

    unsigned short unitType()  const
    {
        return static_cast<unsigned>(m_value.lengthType());
    }

    ExceptionOr<float> valueForBindings()
    {
        return m_value.valueForBindings(SVGLengthContext { contextElement() });
    }

    ExceptionOr<void> setValueForBindings(float value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        auto result = m_value.setValue(SVGLengthContext { contextElement() }, value);
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }
    
    float valueInSpecifiedUnits()
    {
        return m_value.valueInSpecifiedUnits();
    }

    ExceptionOr<void> setValueInSpecifiedUnits(float valueInSpecifiedUnits)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value.setValueInSpecifiedUnits(valueInSpecifiedUnits);
        commitChange();
        return { };
    }
    
    ExceptionOr<void> setValueAsString(const String& value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        auto result = m_value.setValueAsString(value);
        if (result.hasException())
            return result;
        
        commitChange();
        return result;
    }

    ExceptionOr<void> newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        if (unitType == SVG_LENGTHTYPE_UNKNOWN || unitType > SVG_LENGTHTYPE_PC)
            return Exception { NotSupportedError };

        m_value = { valueInSpecifiedUnits, static_cast<SVGLengthType>(unitType), m_value.lengthMode() };
        commitChange();
        return { };
    }
    
    ExceptionOr<void> convertToSpecifiedUnits(unsigned short unitType)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        if (unitType == SVG_LENGTHTYPE_UNKNOWN || unitType > SVG_LENGTHTYPE_PC)
            return Exception { NotSupportedError };

        auto result = m_value.convertToSpecifiedUnits(SVGLengthContext { contextElement() }, static_cast<SVGLengthType>(unitType));
        if (result.hasException())
            return result;

        commitChange();
        return result;
    }
    
    String valueAsString() const override
    {
        return m_value.valueAsString();
    }
};

} // namespace WebCore
