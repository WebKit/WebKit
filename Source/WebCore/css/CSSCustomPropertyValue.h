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

#pragma once

#include "CSSRegisteredCustomProperty.h"
#include "CSSValue.h"
#include "CSSVariableData.h"
#include "Length.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSParserToken;
class RenderStyle;

class CSSCustomPropertyValue final : public CSSValue {
public:
    static Ref<CSSCustomPropertyValue> createWithVariableData(const AtomicString& name, Ref<CSSVariableData>&& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, WTFMove(value)));
    }
    
    static Ref<CSSCustomPropertyValue> createWithID(const AtomicString& name, CSSValueID value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, value));
    }
    
    static Ref<CSSCustomPropertyValue> createInvalid()
    {
        return adoptRef(*new CSSCustomPropertyValue(emptyString(), emptyString()));
    }

    static Ref<CSSCustomPropertyValue> create(const CSSCustomPropertyValue& other)
    {
        return adoptRef(*new CSSCustomPropertyValue(other));
    }
    
    String customCSSText() const;

    const AtomicString& name() const { return m_name; }
    
    bool equals(const CSSCustomPropertyValue& other) const { return m_name == other.m_name && m_value == other.m_value && m_valueId == other.m_valueId; }

    bool containsVariables() const { ASSERT(!m_containsVariables || !m_resolvedTypedValue); return m_containsVariables; }
    bool checkVariablesForCycles(const AtomicString& name, const RenderStyle&, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const;

    void resolveVariableReferences(const CSSRegisteredCustomPropertySet&, Vector<Ref<CSSCustomPropertyValue>>&, const RenderStyle&) const;

    CSSValueID valueID() const { return m_valueId; }
    CSSVariableData* value() const { return m_value.get(); }
    Vector<CSSParserToken> tokens(const CSSRegisteredCustomPropertySet&, const RenderStyle&) const;

    const std::optional<Length>& resolvedTypedValue() const { return m_resolvedTypedValue; }
    void setResolvedTypedValue(Length);

private:
    CSSCustomPropertyValue(const AtomicString& name, const String& serializedValue)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_stringValue(serializedValue)
        , m_serialized(true)
    {
    }

    CSSCustomPropertyValue(const AtomicString& name, CSSValueID id)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_valueId(id)
    {
        ASSERT(id == CSSValueInherit || id == CSSValueInitial || id == CSSValueUnset || id == CSSValueRevert || id == CSSValueInvalid);
    }
    
    CSSCustomPropertyValue(const AtomicString& name, Ref<CSSVariableData>&& value)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_value(WTFMove(value))
        , m_valueId(CSSValueInternalVariableValue)
        , m_containsVariables(m_value->needsVariableResolution())
    {
    }

    CSSCustomPropertyValue(const CSSCustomPropertyValue& other)
        : CSSValue(CustomPropertyClass)
        , m_name(other.m_name)
        , m_value(other.m_value.copyRef())
        , m_valueId(other.m_valueId)
        , m_stringValue(other.m_stringValue)
        , m_containsVariables(other.m_containsVariables)
        , m_serialized(other.m_serialized)
        , m_resolvedTypedValue(other.m_resolvedTypedValue)
    {
    }
    
    const AtomicString m_name;
    
    RefPtr<CSSVariableData> m_value;
    CSSValueID m_valueId { CSSValueInvalid };
    
    mutable String m_stringValue;
    bool m_containsVariables { false };
    mutable bool m_serialized { false };

    // FIXME: It should not be possible to express an invalid state, such as containsVariables() && resolvedTypedValue().
    std::optional<Length> m_resolvedTypedValue;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyValue, isCustomPropertyValue())
