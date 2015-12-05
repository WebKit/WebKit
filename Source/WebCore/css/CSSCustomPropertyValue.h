/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef CSSCustomPropertyValue_h
#define CSSCustomPropertyValue_h

#include "CSSValue.h"

namespace WebCore {

class CSSCustomPropertyValue : public CSSValue {
public:
    static Ref<CSSCustomPropertyValue> create(const AtomicString& name, const String& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, value));
    }
    
    bool equals(const CSSCustomPropertyValue& other) const { return m_name == other.m_name && m_value == other.m_value; }

    String customCSSText() const { return value(); }

    const AtomicString& name() const { return m_name; }
    const String& value() const { return m_value; }
    
private:
    CSSCustomPropertyValue(const AtomicString& name, const String& value)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_value(value)
    {
    }

    const AtomicString m_name;
    const String m_value;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyValue, isCustomPropertyValue())

#endif // CSSCustomPropertyValue_h
