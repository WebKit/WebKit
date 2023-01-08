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

#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParser.h"
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

auto CSSVariableReferenceValue::resolveVariableFallback(const AtomString& variableName, CSSParserTokenRange range, Style::BuilderState& builderState) const -> std::pair<FallbackResult, Vector<CSSParserToken>>
{
    ASSERT(range.atEnd() || range.peek().type() == CommaToken);

    if (range.atEnd())
        return { FallbackResult::None, { } };

    range.consumeIncludingWhitespace();

    auto tokens = resolveTokenRange(range, builderState);

    auto* registered = builderState.document().customPropertyRegistry().get(variableName);
    if (!registered || registered->syntax.isUniversal()) {
        if (!tokens)
            return { FallbackResult::None, { } };

        return { FallbackResult::Valid, WTFMove(*tokens) };
    }

    // https://drafts.css-houdini.org/css-properties-values-api/#fallbacks-in-var-references
    // The fallback value must match the syntax definition of the custom property being referenced,
    // otherwise the declaration is invalid at computed-value time
    if (!tokens || !CSSPropertyParser::isValidCustomPropertyValueForSyntax(registered->syntax, *tokens, m_context))
        return { FallbackResult::Invalid, { } };

    return { FallbackResult::Valid, WTFMove(*tokens) };
}

bool CSSVariableReferenceValue::resolveVariableReference(CSSParserTokenRange range, CSSValueID functionId, Vector<CSSParserToken>& tokens, Style::BuilderState& builderState) const
{
    ASSERT(functionId == CSSValueVar || functionId == CSSValueEnv);

    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    auto variableName = range.consumeIncludingWhitespace().value().toAtomString();

    // Apply this variable first, in case it is still unresolved
    builderState.builder().applyCustomProperty(variableName);

    // Fallback has to be resolved even when not used to detect cycles and invalid syntax.
    auto [fallbackResult, fallbackTokens] = resolveVariableFallback(variableName, range, builderState);
    if (fallbackResult == FallbackResult::Invalid)
        return false;

    auto* property = [&]() -> const CSSCustomPropertyValue* {
        if (functionId == CSSValueEnv)
            return builderState.document().constantProperties().values().get(variableName);

        auto* customProperty = builderState.style().getCustomProperty(variableName);
        if (customProperty)
            return customProperty;

        auto* registered = builderState.document().customPropertyRegistry().get(variableName);
        return registered && registered->initialValue() ? registered->initialValue() : nullptr;
    }();

    if (!property || property->isInvalid()) {
        if (fallbackTokens.size() > maxSubstitutionTokens)
            return false;

        if (fallbackResult == FallbackResult::Valid) {
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

std::optional<Vector<CSSParserToken>> CSSVariableReferenceValue::resolveTokenRange(CSSParserTokenRange range, Style::BuilderState& builderState) const
{
    Vector<CSSParserToken> tokens;
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
    if (!success)
        return { };

    return tokens;
}

RefPtr<CSSVariableData> CSSVariableReferenceValue::resolveVariableReferences(Style::BuilderState& builderState) const 
{
    auto resolvedTokens = resolveTokenRange(m_data->tokenRange(), builderState);
    if (!resolvedTokens)
        return nullptr;

    return CSSVariableData::create(*resolvedTokens);
}

} // namespace WebCore
