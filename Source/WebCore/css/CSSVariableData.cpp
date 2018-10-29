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
#include "CSSVariableData.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserTokenRange.h"
#include "CSSValuePool.h"
#include "RenderStyle.h"
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace WebCore {

template<typename CharacterType> void CSSVariableData::updateTokens(const CSSParserTokenRange& range)
{
    const CharacterType* currentOffset = m_backingString.characters<CharacterType>();
    for (const CSSParserToken& token : range) {
        if (token.hasStringBacking()) {
            unsigned length = token.value().length();
            StringView string(currentOffset, length);
            m_tokens.append(token.copyWithUpdatedString(string));
            currentOffset += length;
        } else
            m_tokens.append(token);
    }
    ASSERT(currentOffset == m_backingString.characters<CharacterType>() + m_backingString.length());
}

bool CSSVariableData::operator==(const CSSVariableData& other) const
{
    return tokens() == other.tokens();
}

void CSSVariableData::consumeAndUpdateTokens(const CSSParserTokenRange& range)
{
    StringBuilder stringBuilder;
    CSSParserTokenRange localRange = range;

    while (!localRange.atEnd()) {
        CSSParserToken token = localRange.consume();
        if (token.hasStringBacking())
            stringBuilder.append(token.value());
    }
    m_backingString = stringBuilder.toString();
    if (m_backingString.is8Bit())
        updateTokens<LChar>(range);
    else
        updateTokens<UChar>(range);
}

CSSVariableData::CSSVariableData(const CSSParserTokenRange& range, bool needsVariableResolution)
    : m_needsVariableResolution(needsVariableResolution)
{
    consumeAndUpdateTokens(range);
}

bool CSSVariableData::checkVariablesForCycles(const AtomicString& name, const RenderStyle& style, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const
{
    if (invalidProperties.contains(name))
        return false;
    
    HashSet<AtomicString> newSeenProperties = seenProperties;
    newSeenProperties.add(name);
    
    bool valid = checkVariablesForCyclesWithRange(m_tokens, style, newSeenProperties, invalidProperties);
    if (!valid)
        invalidProperties.add(name);
    
    return valid;
}
    
bool CSSVariableData::checkVariablesForCyclesWithRange(CSSParserTokenRange range, const RenderStyle& style, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const
{
    while (!range.atEnd()) {
        if (range.peek().functionId() == CSSValueVar || range.peek().functionId() == CSSValueEnv) {
            CSSParserTokenRange block = range.consumeBlock();
            
            block.consumeWhitespace();
            ASSERT(block.peek().type() == IdentToken);
            AtomicString variableName = block.consumeIncludingWhitespace().value().toAtomicString();
            ASSERT(block.atEnd() || block.peek().type() == CommaToken);
            if (seenProperties.contains(variableName))
                return false;

            auto* value = style.getCustomProperty(variableName);
            if (value && value->containsVariables() && !value->checkVariablesForCycles(variableName, style, seenProperties, invalidProperties))
                return false;

            if (range.peek().type() == CommaToken) {
                // Fallback.
                range.consume();
                return checkVariablesForCyclesWithRange(block, style, seenProperties, invalidProperties);
            }
        } else
            range.consume();
    }
    return true;
}

bool CSSVariableData::resolveVariableFallback(const CSSRegisteredCustomPropertySet& registeredProperties, CSSParserTokenRange range, Vector<CSSParserToken>& result, const RenderStyle& style) const
{
    if (range.atEnd())
        return false;
    ASSERT(range.peek().type() == CommaToken);
    range.consume();
    return resolveTokenRange(registeredProperties, range, result, style);
}

bool CSSVariableData::resolveVariableReference(const CSSRegisteredCustomPropertySet& registeredProperties, CSSParserTokenRange range, Vector<CSSParserToken>& result, const RenderStyle& style) const
{
    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    AtomicString variableName = range.consumeIncludingWhitespace().value().toAtomicString();
    ASSERT(range.atEnd() || (range.peek().type() == CommaToken));
    
    auto* property = style.getCustomProperty(variableName);
    if (property && property->resolvedTypedValue()) {
        result.appendVector(property->tokens(registeredProperties, style));
        return true;
    }

    if (!property || !property->value()) {
        auto* registered = registeredProperties.get(variableName);
        if (registered && registered->initialValue())
            property = registered->initialValue();
        else
            return resolveVariableFallback(registeredProperties, range, result, style);
    }
    ASSERT(property);
    result.appendVector(property->tokens(registeredProperties, style));
    
    return true;
}

RefPtr<CSSVariableData> CSSVariableData::resolveVariableReferences(const CSSRegisteredCustomPropertySet& registeredProperties, const RenderStyle& style) const
{
    Vector<CSSParserToken> resolvedTokens;
    CSSParserTokenRange range = m_tokens;
    if (!resolveTokenRange(registeredProperties, range, resolvedTokens, style))
        return nullptr;
    return CSSVariableData::createResolved(resolvedTokens, *this);
}
    
bool CSSVariableData::resolveTokenRange(const CSSRegisteredCustomPropertySet& registeredProperties, CSSParserTokenRange range, Vector<CSSParserToken>& result, const RenderStyle& style) const
{
    bool success = true;
    while (!range.atEnd()) {
        if (range.peek().functionId() == CSSValueVar || range.peek().functionId() == CSSValueEnv)
            success &= resolveVariableReference(registeredProperties, range.consumeBlock(), result, style);
        else
            result.append(range.consume());
    }
    return success;
}

} // namespace WebCore
