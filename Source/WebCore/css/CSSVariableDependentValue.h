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

#ifndef CSSVariableDependentValue_h
#define CSSVariableDependentValue_h

#include "CSSPropertyNames.h"
#include "CSSValueList.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSVariableDependentValue final : public CSSValue {
public:
    static Ref<CSSVariableDependentValue> create(Ref<CSSValueList>&& valueList, CSSPropertyID propId)
    {
        return adoptRef(*new CSSVariableDependentValue(WTFMove(valueList), propId));
    }
    
    String customCSSText() const
    {
        if (!m_serialized) {
            m_serialized = true;
            m_stringValue = m_valueList->customCSSText();
        }
        return m_stringValue;
    }
    
    // FIXME: Should arguably implement equals on all of the CSSParserValues, but CSSValue equivalence
    // is rarely used, so serialization to compare is probably fine.
    bool equals(const CSSVariableDependentValue& other) const { return customCSSText() == other.customCSSText(); }
    
    CSSPropertyID propertyID() const { return m_propertyID; }
    
    CSSValueList& valueList() { return m_valueList.get(); }
    const CSSValueList& valueList() const { return m_valueList.get(); }

    bool checkVariablesForCycles(const AtomicString& name, CustomPropertyValueMap& customProperties, const HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties);

private:
    CSSVariableDependentValue(Ref<CSSValueList>&& valueList, CSSPropertyID propId)
        : CSSValue(VariableDependentClass)
        , m_valueList(WTFMove(valueList))
        , m_propertyID(propId)
    {
    }

    Ref<CSSValueList> m_valueList;
    CSSPropertyID m_propertyID;
    mutable String m_stringValue;
    mutable bool m_serialized { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSVariableDependentValue, isVariableDependentValue())

#endif // CSSCustomPropertyValue_h
