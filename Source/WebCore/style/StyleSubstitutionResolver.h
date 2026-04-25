/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSParserTokenRange.h"
#include "StyleCustomProperty.h"
#include "StyleRuleFunction.h"

namespace WebCore {

class CSSShorthandSubstitutionValue;
class CSSValue;
class CSSVariableData;
class CSSSubstitutionValue;
struct CSSParserContext;
enum CSSPropertyID : uint16_t;
enum CSSValueID : uint16_t;

class MutableStyleProperties;

namespace Style {

class Builder;
class CustomProperty;
class LocalPropertyRegistry;

// https://drafts.csswg.org/css-values-5/#arbitrary-substitution
class SubstitutionResolver {
public:
    explicit SubstitutionResolver(Builder&);

    RefPtr<CSSValue> substituteAndParse(const CSSSubstitutionValue&, CSSPropertyID);
    RefPtr<CSSValue> substituteAndParseShorthand(const CSSShorthandSubstitutionValue&, CSSPropertyID);
    RefPtr<CSSVariableData> substitute(const CSSSubstitutionValue&);

private:
    std::optional<Vector<CSSParserToken>> substituteTokenRange(CSSParserTokenRange, const CSSParserContext&);

    bool substituteVariableFunction(CSSParserTokenRange, CSSValueID, Vector<CSSParserToken>&, const CSSParserContext&);
    bool substituteDashedFunction(StringView functionName, CSSParserTokenRange, Vector<CSSParserToken>&);
    RefPtr<MutableStyleProperties> resolveAndRegisterDashedFunctionArguments(const Vector<StyleRuleFunction::Parameter>&, const Vector<Vector<CSSParserToken>>&, LocalPropertyRegistry&);
    bool substituteAttrFunction(CSSParserTokenRange, Vector<CSSParserToken>&, const CSSParserContext&);
    bool substituteInternalAutoBaseFunction(CSSParserTokenRange, Vector<CSSParserToken>&, const CSSParserContext&);

    struct AttrArgumentGrammarSubstitution {
        Vector<CSSParserToken> firstArg;
        std::optional<CSSParserTokenRange> fallbackRange;
    };
    std::optional<AttrArgumentGrammarSubstitution> substituteAttrArgumentGrammar(CSSParserTokenRange, const CSSParserContext&);

    enum class FallbackResult : uint8_t { None, Valid, Invalid };
    std::pair<FallbackResult, Vector<CSSParserToken>> substituteVariableFallback(const AtomString& variableName, CSSParserTokenRange, CSSValueID functionId, const CSSParserContext&);

    RefPtr<const CustomProperty> propertyValueForVariableName(const AtomString&, CSSValueID);
    RefPtr<CSSVariableData> trySimpleSubstitution(const CSSSubstitutionValue&);
    bool isBaseAppearance();
    bool isInURLContext() const { return m_urlContextDepth; }
    void updateURLContext(const CSSParserToken&);
    void propagateAttrTaint(IsAttrTainted, std::span<const CSSParserToken>);

    Builder& m_styleBuilder;
    RefPtr<const CSSSubstitutionValue> m_substitutionValue;
    Vector<String> m_intermediateTokenStrings;
    unsigned m_urlContextDepth { 0 };
    bool m_isAttrTainted { false };
    bool m_hasTaintedURL { false };
};

} // namespace Style
} // namespace WebCore
