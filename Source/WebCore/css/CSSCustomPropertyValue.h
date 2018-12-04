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
#include "CSSVariableReferenceValue.h"
#include "Length.h"
#include "StyleImage.h"
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSParserToken;
class CSSVariableReferenceValue;
class RenderStyle;

class CSSCustomPropertyValue final : public CSSValue {
public:
    using VariantValue = Variant<Ref<CSSVariableReferenceValue>, CSSValueID, Ref<CSSVariableData>, Length, Ref<StyleImage>>;

    static Ref<CSSCustomPropertyValue> createUnresolved(const AtomicString& name, Ref<CSSVariableReferenceValue>&& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, { WTFMove(value) }));
    }

    static Ref<CSSCustomPropertyValue> createUnresolved(const AtomicString& name, CSSValueID value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, { value }));
    }

    static Ref<CSSCustomPropertyValue> createWithID(const AtomicString& name, CSSValueID id)
    {
        ASSERT(id == CSSValueInherit || id == CSSValueInitial || id == CSSValueUnset || id == CSSValueRevert || id == CSSValueInvalid);
        return adoptRef(*new CSSCustomPropertyValue(name, { id }));
    }

    static Ref<CSSCustomPropertyValue> createSyntaxAll(const AtomicString& name, Ref<CSSVariableData>&& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, { WTFMove(value) }));
    }
    
    static Ref<CSSCustomPropertyValue> createSyntaxLength(const AtomicString& name, Length value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, { WTFMove(value) }));
    }

    static Ref<CSSCustomPropertyValue> createSyntaxImage(const AtomicString& name, Ref<StyleImage>&& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, { WTFMove(value) }));
    }

    static Ref<CSSCustomPropertyValue> create(const CSSCustomPropertyValue& other)
    {
        return adoptRef(*new CSSCustomPropertyValue(other));
    }
    
    String customCSSText() const;

    const AtomicString& name() const { return m_name; }
    bool isResolved() const  { return !WTF::holds_alternative<Ref<CSSVariableReferenceValue>>(m_value); }
    bool isUnset() const  { return WTF::holds_alternative<CSSValueID>(m_value) && WTF::get<CSSValueID>(m_value) == CSSValueUnset; }
    bool isInvalid() const  { return WTF::holds_alternative<CSSValueID>(m_value) && WTF::get<CSSValueID>(m_value) == CSSValueInvalid; }

    const VariantValue& value() const { return m_value; }

    Vector<CSSParserToken> tokens() const;
    bool equals(const CSSCustomPropertyValue& other) const;

private:
    CSSCustomPropertyValue(const AtomicString& name, VariantValue&& value)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_value(WTFMove(value))
        , m_serialized(false)
    {
    }

    CSSCustomPropertyValue(const CSSCustomPropertyValue& other)
        : CSSValue(CustomPropertyClass)
        , m_name(other.m_name)
        , m_value(CSSValueUnset)
        , m_stringValue(other.m_stringValue)
        , m_serialized(other.m_serialized)
    {
        // No copy constructor for Ref<CSSVariableData>, so we have to do this ourselves
        auto visitor = WTF::makeVisitor([&](const Ref<CSSVariableReferenceValue>& value) {
            m_value = value.copyRef();
        }, [&](const CSSValueID& value) {
            m_value = value;
        }, [&](const Ref<CSSVariableData>& value) {
            m_value = value.copyRef();
        }, [&](const Length& value) {
            m_value = value;
        }, [&](const Ref<StyleImage>& value) {
            m_value = value.copyRef();
        });
        WTF::visit(visitor, other.m_value);
    }
    
    const AtomicString m_name;
    VariantValue m_value;
    
    mutable String m_stringValue;
    mutable bool m_serialized { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyValue, isCustomPropertyValue())
