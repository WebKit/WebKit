// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "CSSRegisteredCustomProperty.h"
#include "CSSVariableData.h"
#include "ConstantPropertyMap.h"
#include "CustomPropertyRegistry.h"
#include "RenderStyle.h"
#include "StyleBuilder.h"
#include "StyleResolver.h"

namespace WebCore {

CSSVariableReferenceValue::CSSVariableReferenceValue(Ref<CSSVariableData>&& data, const CSSParserContext& context)
    : CSSValue(VariableReferenceClass)
    , m_data(WTFMove(data))
    , m_context(context)
{
}

Ref<CSSVariableReferenceValue> CSSVariableReferenceValue::create(const CSSParserTokenRange& range, const CSSParserContext& context)
{
    return adoptRef(*new CSSVariableReferenceValue(CSSVariableData::create(range), context));
}

Ref<CSSVariableReferenceValue> CSSVariableReferenceValue::create(Ref<CSSVariableData>&& data, const CSSParserContext& context)
{
    return adoptRef(*new CSSVariableReferenceValue(WTFMove(data), context));
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

bool CSSVariableReferenceValue::resolveVariableFallback(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, Style::BuilderState& builderState)
{
    if (range.atEnd())
        return false;
    ASSERT(range.peek().type() == CommaToken);
    range.consumeIncludingWhitespace();
    return resolveTokenRange(range, tokens, builderState);
}

bool CSSVariableReferenceValue::resolveVariableReference(CSSParserTokenRange range, CSSValueID functionId, Vector<CSSParserToken>& tokens, Style::BuilderState& builderState)
{
    ASSERT(functionId == CSSValueVar || functionId == CSSValueEnv);

    auto& style = builderState.style();

    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    auto variableName = range.consumeIncludingWhitespace().value().toAtomString();
    ASSERT(range.atEnd() || (range.peek().type() == CommaToken));

    // Apply this variable first, in case it is still unresolved
    builderState.builder().applyCustomProperty(variableName);

    // Apply fallback to detect cycles
    Vector<CSSParserToken> fallbackTokens;
    bool fallbackSuccess = resolveVariableFallback(CSSParserTokenRange(range), fallbackTokens, builderState);

    auto* property = [&]() -> const CSSCustomPropertyValue* {
        if (functionId == CSSValueEnv)
            return builderState.document().constantProperties().values().get(variableName);

        auto* customProperty = style.getCustomProperty(variableName);
        if (customProperty)
            return customProperty;

        auto* registered = builderState.document().customPropertyRegistry().get(variableName);
        return registered && registered->initialValue() ? registered->initialValue() : nullptr;
    }();

    if (!property || property->isInvalid()) {
        if (fallbackTokens.size() > maxSubstitutionTokens)
            return false;

        if (fallbackSuccess) {
            tokens.appendVector(fallbackTokens);
            return true;
        }
        return false;
    }

    ASSERT(property->isResolved());
    if (property->tokens().size() > maxSubstitutionTokens)
        return false;

    tokens.appendVector(property->tokens());
    return true;
}

bool CSSVariableReferenceValue::resolveTokenRange(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, Style::BuilderState& builderState)
{
    bool success = true;
    while (!range.atEnd()) {
        auto functionId = range.peek().functionId();
        if (functionId == CSSValueVar || functionId == CSSValueEnv) {
            if (!resolveVariableReference(range.consumeBlock(), functionId, tokens, builderState))
                success = false;
            continue;
        }
        tokens.append(range.consume());
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
