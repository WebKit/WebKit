/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include "SVGProperty.h"

namespace WebCore {
    
template<typename PropertyType>
class SVGValueProperty : public SVGProperty {
public:
    using ValueType = PropertyType;

    static Ref<SVGValueProperty> create()
    {
        return adoptRef(*new SVGValueProperty());
    }

    // Getter/Setter for the value.
    const PropertyType& value() const { return m_value; }
    void setValue(const PropertyType& value) { m_value = value; }

    // Used by the SVGAnimatedPropertyAnimator to pass m_value to SVGAnimationFunction.
    PropertyType& value() { return m_value; }

    // Visual Studio doesn't seem to see these private constructors from subclasses.
    // FIXME: See what it takes to remove this hack.
#if !COMPILER(MSVC)
protected:
#endif
    // Create an initialized property, e.g creating an item to be appended in an SVGList.
    SVGValueProperty(const PropertyType& value)
        : m_value(value)
    {
    }

    // Needed when value should not be copied, e.g. SVGTransfromValue.
    SVGValueProperty(PropertyType&& value)
        : m_value(WTFMove(value))
    {
    }

    // Base and default constructor. Do not use "using SVGProperty::SVGProperty" because of Windows and GTK ports.
    SVGValueProperty(SVGPropertyOwner* owner = nullptr, SVGPropertyAccess access = SVGPropertyAccess::ReadWrite)
        : SVGProperty(owner, access)
    {
    }

    // Create an initialized and attached property.
    SVGValueProperty(SVGPropertyOwner* owner, SVGPropertyAccess access, const PropertyType& value)
        : SVGProperty(owner, access)
        , m_value(value)
    {
    }

    PropertyType m_value;
};

}
