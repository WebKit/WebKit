// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "RenderStyle.h"
#include "StyleResolver.h"

namespace WebCore {

String CSSVariableReferenceValue::customCSSText() const
{
    if (!m_serialized) {
        m_serialized = true;
        m_stringValue = m_data->tokenRange().serialize();
    }
    return m_stringValue;
}

static bool resolveTokenRange(CSSParserTokenRange, Vector<CSSParserToken>&, ApplyCascadedPropertyState&);

static bool resolveVariableFallback(CSSParserTokenRange range, Vector<CSSParserToken>& result, ApplyCascadedPropertyState& state)
{
    if (range.atEnd())
        return false;
    ASSERT(range.peek().type() == CommaToken);
    range.consume();
    return resolveTokenRange(range, result, state);
}

static bool resolveVariableReference(CSSParserTokenRange range, Vector<CSSParserToken>& result, ApplyCascadedPropertyState& state)
{
    auto& registeredProperties = state.styleResolver->document().getCSSRegisteredCustomPropertySet();
    auto& style = *state.styleResolver->style();

    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    String variableName = range.consumeIncludingWhitespace().value().toString();
    ASSERT(range.atEnd() || (range.peek().type() == CommaToken));

    // Apply this variable first, in case it is still unresolved
    state.styleResolver->applyCascadedCustomProperty(variableName, state);

    // Apply fallback to detect cycles
    Vector<CSSParserToken> fallbackResult;
    resolveVariableFallback(CSSParserTokenRange(range), fallbackResult, state);

    auto* property = style.getCustomProperty(variableName);

    if (!property || property->isUnset() || property->isInvalid()) {
        auto* registered = registeredProperties.get(variableName);
        if (registered && registered->initialValue())
            property = registered->initialValue();
        else
            return resolveVariableFallback(range, result, state);
    }

    ASSERT(property->isResolved());
    result.appendVector(property->tokens());

    return true;
}

static bool resolveTokenRange(CSSParserTokenRange range, Vector<CSSParserToken>& result, ApplyCascadedPropertyState& state)
{
    bool success = true;
    while (!range.atEnd()) {
        if (range.peek().functionId() == CSSValueVar || range.peek().functionId() == CSSValueEnv)
            success &= resolveVariableReference(range.consumeBlock(), result, state);
        else
            result.append(range.consume());
    }
    return success;
}

RefPtr<CSSVariableData> CSSVariableReferenceValue::resolveVariableReferences(ApplyCascadedPropertyState& state) const
{
    Vector<CSSParserToken> resolvedTokens;
    CSSParserTokenRange range = m_data->tokenRange();

    if (!resolveTokenRange(range, resolvedTokens, state))
        return nullptr;

    return CSSVariableData::create(resolvedTokens);
}

} // namespace WebCore
