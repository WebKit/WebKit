/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "CSSValue.h"
#include "CSSVariableData.h"
#include "CSSVariableReferenceValue.h"
#include "Length.h"
#include "StyleColor.h"
#include "StyleImage.h"
#include "TransformOperation.h"
#include <wtf/URL.h>

namespace WebCore {

class CSSParserToken;

class CSSCustomPropertyValue final : public CSSValue {
public:
    struct NumericSyntaxValue {
        double value;
        CSSUnitType unitType;

        bool operator==(const NumericSyntaxValue& other) const { return value == other.value && unitType == other.unitType; }
    };

    using SyntaxValue = std::variant<Length, NumericSyntaxValue, StyleColor, RefPtr<StyleImage>, URL, String, RefPtr<TransformOperation>>;

    struct SyntaxValueList {
        Vector<SyntaxValue> values;
        ValueSeparator separator;

        bool operator==(const SyntaxValueList& other) const { return values == other.values && separator == other.separator; }
    };

    using VariantValue = std::variant<std::monostate, Ref<CSSVariableReferenceValue>, CSSValueID, Ref<CSSVariableData>, SyntaxValue, SyntaxValueList>;

    static Ref<CSSCustomPropertyValue> createEmpty(const AtomString& name);

    static Ref<CSSCustomPropertyValue> createUnresolved(const AtomString& name, Ref<CSSVariableReferenceValue>&& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, VariantValue { std::in_place_type<Ref<CSSVariableReferenceValue>>, WTFMove(value) }));
    }

    static Ref<CSSCustomPropertyValue> createUnresolved(const AtomString& name, CSSValueID value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, { value }));
    }

    static Ref<CSSCustomPropertyValue> createWithID(const AtomString& name, CSSValueID);

    static Ref<CSSCustomPropertyValue> createSyntaxAll(const AtomString& name, Ref<CSSVariableData>&& value)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, VariantValue { std::in_place_type<Ref<CSSVariableData>>, WTFMove(value) }));
    }

    static Ref<CSSCustomPropertyValue> createForSyntaxValue(const AtomString& name, SyntaxValue&& syntaxValue)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, VariantValue { WTFMove(syntaxValue) }));
    }

    static Ref<CSSCustomPropertyValue> createForSyntaxValueList(const AtomString& name, SyntaxValueList&& syntaxValueList)
    {
        return adoptRef(*new CSSCustomPropertyValue(name, VariantValue { WTFMove(syntaxValueList) }));
    }

    static Ref<CSSCustomPropertyValue> create(const CSSCustomPropertyValue& other)
    {
        return adoptRef(*new CSSCustomPropertyValue(other));
    }

    String customCSSText() const;

    const AtomString& name() const { return m_name; }
    bool isResolved() const { return !std::holds_alternative<Ref<CSSVariableReferenceValue>>(m_value); }
    bool isUnset() const { return std::holds_alternative<CSSValueID>(m_value) && std::get<CSSValueID>(m_value) == CSSValueUnset; }
    bool isInvalid() const { return std::holds_alternative<CSSValueID>(m_value) && std::get<CSSValueID>(m_value) == CSSValueInvalid; }
    bool containsCSSWideKeyword() const;

    const VariantValue& value() const { return m_value; }

    Vector<CSSParserToken> tokens() const;
    bool equals(const CSSCustomPropertyValue&) const;

private:
    CSSCustomPropertyValue(const AtomString& name, VariantValue&& value)
        : CSSValue(CustomPropertyClass)
        , m_name(name)
        , m_value(WTFMove(value))
    {
    }

    CSSCustomPropertyValue(const CSSCustomPropertyValue& other)
        : CSSValue(CustomPropertyClass)
        , m_name(other.m_name)
        , m_value(other.m_value)
        , m_stringValue(other.m_stringValue)
    {
    }

    const AtomString m_name;
    const VariantValue m_value;
    mutable String m_stringValue;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyValue, isCustomPropertyValue())
