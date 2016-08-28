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
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSCustomPropertyValue final : public CSSValue {
public:
    static Ref<CSSCustomPropertyValue> create(const AtomicString& name, Ref<CSSValue>&& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, WTFMove(value)));
    }
    
    static Ref<CSSCustomPropertyValue> createInvalid()
    {
        return adoptRef(*new CSSCustomPropertyValue(emptyString(), emptyString()));
    }
    
    String customCSSText() const
    {
        if (!m_serialized) {
            m_serialized = true;
            m_stringValue = m_value ? m_value->cssText() : emptyString();
        }
        return m_stringValue;
    }

    const AtomicString& name() const { return m_name; }
    
    // FIXME: Should arguably implement equals on all of the CSSParserValues, but CSSValue equivalence
    // is rarely used, so serialization to compare is probably fine.
    bool equals(const CSSCustomPropertyValue& other) const { return m_name == other.m_name && customCSSText() == other.customCSSText(); }

    bool isInvalid() const { return !m_value; }
    bool containsVariables() const { return m_containsVariables; }

    const RefPtr<CSSValue> value() const { return m_value.get(); }

private:
    CSSCustomPropertyValue(const AtomicString& name, Ref<CSSValue>&& value)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_value(WTFMove(value))
        , m_containsVariables(m_value->isVariableDependentValue())
        , m_serialized(false)
    {
    }
    
    CSSCustomPropertyValue(const AtomicString& name, const String& serializedValue)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_stringValue(serializedValue)
        , m_serialized(true)
    {
    }

    const AtomicString m_name;
    RefPtr<CSSValue> m_value;
    mutable String m_stringValue;
    bool m_containsVariables { false };
    mutable bool m_serialized;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyValue, isCustomPropertyValue())

#endif // CSSCustomPropertyValue_h
