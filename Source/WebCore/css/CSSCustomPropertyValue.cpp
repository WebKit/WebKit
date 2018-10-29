/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "CSSCustomPropertyValue.h"
#include "CSSTokenizer.h"


namespace WebCore {

String CSSCustomPropertyValue::customCSSText() const
{
    if (!m_serialized) {
        m_serialized = true;
        if (m_resolvedTypedValue) // FIXME: Unit should be based on syntax.
            m_stringValue = CSSPrimitiveValue::create(m_resolvedTypedValue->value(), CSSPrimitiveValue::CSS_PX)->cssText();
        else if (m_value)
            m_stringValue = m_value->tokenRange().serialize();
        else if (m_valueId != CSSValueInvalid)
            m_stringValue = getValueName(m_valueId);
        else
            m_stringValue = emptyString();
    }
    return m_stringValue;
}

Vector<CSSParserToken> CSSCustomPropertyValue::tokens(const CSSRegisteredCustomPropertySet& registeredProperties, const RenderStyle& style) const
{
    if (m_resolvedTypedValue) {
        Vector<CSSParserToken> result;
        CSSTokenizer tokenizer(cssText());

        auto tokenizerRange = tokenizer.tokenRange();
        while (!tokenizerRange.atEnd())
            result.append(tokenizerRange.consume());

        return result;
    }

    if (!m_value)
        return { };

    if (m_containsVariables) {
        Vector<CSSParserToken> result;
        // FIXME: Avoid doing this work more than once.
        RefPtr<CSSVariableData> resolvedData = m_value->resolveVariableReferences(registeredProperties, style);
        if (resolvedData)
            result.appendVector(resolvedData->tokens());

        return result;
    }

    return m_value->tokens();
}

bool CSSCustomPropertyValue::checkVariablesForCycles(const AtomicString& name, const RenderStyle& style, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const
{
    ASSERT(containsVariables());
    if (m_value)
        return m_value->checkVariablesForCycles(name, style, seenProperties, invalidProperties);
    return true;
}

void CSSCustomPropertyValue::resolveVariableReferences(const CSSRegisteredCustomPropertySet& registeredProperties, Vector<Ref<CSSCustomPropertyValue>>& resolvedValues, const RenderStyle& style) const
{
    ASSERT(containsVariables());
    if (!m_value)
        return;
    
    ASSERT(m_value->needsVariableResolution());
    RefPtr<CSSVariableData> resolvedData = m_value->resolveVariableReferences(registeredProperties, style);
    if (resolvedData)
        resolvedValues.append(CSSCustomPropertyValue::createWithVariableData(m_name, resolvedData.releaseNonNull()));
    else
        resolvedValues.append(CSSCustomPropertyValue::createWithID(m_name, CSSValueInvalid));
}

void CSSCustomPropertyValue::setResolvedTypedValue(Length length)
{
    ASSERT(length.isSpecified());
    m_resolvedTypedValue = WTFMove(length);
}

}
