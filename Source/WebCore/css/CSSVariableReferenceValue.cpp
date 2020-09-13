// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2020 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSVariableReferenceValue.h"

#include "CSSVariableData.h"
#include "ConstantPropertyMap.h"
#include "RenderStyle.h"
#include "StyleBuilder.h"
#include "StyleResolver.h"

namespace WebCore {

static bool resolveTokenRange(CSSParserTokenRange, Vector<CSSParserToken>&, Style::BuilderState&);

CSSVariableReferenceValue::CSSVariableReferenceValue(Ref<CSSVariableData>&& data)
    : CSSValue(VariableReferenceClass)
    , m_data(WTFMove(data))
{
}

Ref<CSSVariableReferenceValue> CSSVariableReferenceValue::create(const CSSParserTokenRange& range)
{
    return adoptRef(*new CSSVariableReferenceValue(CSSVariableData::create(range)));
}

bool CSSVariableReferenceValue::equals(const CSSVariableReferenceValue& other) const
{
    return m_data.get() == other.m_data.get();
}

String CSSVariableReferenceValue::customCSSText() const
{
    if (m_stringValue.isNull())
        m_stringValue = m_data->tokenRange().serialize();
    return m_stringValue;
}

static bool resolveVariableFallback(CSSParserTokenRange range, Vector<CSSParserToken>& result, Style::BuilderState& builderState)
{
    if (range.atEnd())
        return false;
    ASSERT(range.peek().type() == CommaToken);
    range.consume();
    return resolveTokenRange(range, result, builderState);
}

static bool resolveVariableReference(CSSParserTokenRange range, CSSValueID functionId, Vector<CSSParserToken>& result, Style::BuilderState& builderState)
{
    ASSERT(functionId == CSSValueVar || functionId == CSSValueEnv);

    auto& registeredProperties = builderState.document().getCSSRegisteredCustomPropertySet();
    auto& style = builderState.style();

    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    String variableName = range.consumeIncludingWhitespace().value().toString();
    ASSERT(range.atEnd() || (range.peek().type() == CommaToken));

    // Apply this variable first, in case it is still unresolved
    builderState.builder().applyCustomProperty(variableName);

    // Apply fallback to detect cycles
    Vector<CSSParserToken> fallbackResult;
    bool fallbackReturn = resolveVariableFallback(CSSParserTokenRange(range), fallbackResult, builderState);


    auto* property = functionId == CSSValueVar
        ? style.getCustomProperty(variableName)
        : builderState.document().constantProperties().values().get(variableName);
    if (!property || property->isUnset()) {
        auto* registered = registeredProperties.get(variableName);
        if (registered && registered->initialValue())
            property = registered->initialValue();
    }

    if (!property || property->isInvalid()) {
        if (fallbackResult.size() > CSSVariableReferenceValue::maxSubstitutionTokens)
            return false;

        if (fallbackReturn)
            result.appendVector(fallbackResult);
        return fallbackReturn;
    }

    ASSERT(property->isResolved());
    if (property->tokens().size() > CSSVariableReferenceValue::maxSubstitutionTokens)
        return false;

    result.appendVector(property->tokens());
    return true;
}

static bool resolveTokenRange(CSSParserTokenRange range, Vector<CSSParserToken>& result, Style::BuilderState& builderState)
{
    bool success = true;
    while (!range.atEnd()) {
        auto functionId = range.peek().functionId();
        if (functionId == CSSValueVar || functionId == CSSValueEnv)
            success &= resolveVariableReference(range.consumeBlock(), functionId, result, builderState);
        else
            result.append(range.consume());
    }
    return success;
}

RefPtr<CSSVariableData> CSSVariableReferenceValue::resolveVariableReferences(Style::BuilderState& builderState) const
{
    Vector<CSSParserToken> resolvedTokens;
    if (!resolveTokenRange(m_data->tokenRange(), resolvedTokens, builderState))
        return nullptr;

    return CSSVariableData::create(resolvedTokens);
}

} // namespace WebCore
