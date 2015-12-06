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

#include "CSSParserValues.h"
#include "CSSValue.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSCustomPropertyValue : public CSSValue {
public:
    static Ref<CSSCustomPropertyValue> create(const AtomicString& name, std::unique_ptr<CSSParserValueList>& valueList)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, valueList));
    }
    
    String customCSSText() const
    {
        if (!m_serialized) {
            m_serialized = true;
            m_stringValue = m_parserValue ? m_parserValue->toString() : "";
        }
        return m_stringValue;
    }

    const AtomicString& name() const { return m_name; }
    
    // FIXME: Should arguably implement equals on all of the CSSParserValues, but CSSValue equivalence
    // is rarely used, so serialization to compare is probably fine.
    bool equals(const CSSCustomPropertyValue& other) const { return m_name == other.m_name && customCSSText() == other.customCSSText(); }

private:
    CSSCustomPropertyValue(const AtomicString& name, std::unique_ptr<CSSParserValueList>& valueList)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_parserValue(WTF::move(valueList))
        , m_serialized(false)
    {
    }

    const AtomicString m_name;
    std::unique_ptr<CSSParserValueList> m_parserValue;
    mutable String m_stringValue;
    mutable bool m_serialized;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyValue, isCustomPropertyValue())

#endif // CSSCustomPropertyValue_h
