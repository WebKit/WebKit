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

#include "CSSCustomPropertySyntax.h"
#include "CSSCustomPropertyValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParser.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSTokenizer.h"
#include "CSSVariableData.h"
#include "ConstantPropertyMap.h"
#include "CustomFunctionRegistry.h"
#include "Document.h"
#include "Element.h"
#include "QualifiedName.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleBuilder.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/Scope.h>

namespace WebCore {

CSSVariableReferenceValue::CSSVariableReferenceValue(Ref<CSSVariableData>&& data)
    : CSSValue(ClassType::VariableReference)
    , m_data(WTF::move(data))
{
    cacheSimpleReference();
}

Ref<CSSVariableReferenceValue> CSSVariableReferenceValue::create(const CSSParserTokenRange& range, const CSSParserContext& context)
{
    return adoptRef(*new CSSVariableReferenceValue(CSSVariableData::create(range, context)));
}

Ref<CSSVariableReferenceValue> CSSVariableReferenceValue::create(Ref<CSSVariableData>&& data)
{
    return adoptRef(*new CSSVariableReferenceValue(WTF::move(data)));
}

bool CSSVariableReferenceValue::equals(const CSSVariableReferenceValue& other) const
{
    return arePointingToEqualData(m_data, other.m_data);
}

String CSSVariableReferenceValue::customCSSText(const CSS::SerializationContext&) const
{
    if (m_stringValue.isNull())
        m_stringValue = m_data->serialize();
    return m_stringValue;
}

const CSSParserContext& CSSVariableReferenceValue::context() const
{
    return m_data->context();
}

auto CSSVariableReferenceValue::resolveVariableFallback(const AtomString& variableName, CSSParserTokenRange range, CSSValueID functionId, Style::Builder& builder) const -> std::pair<FallbackResult, Vector<CSSParserToken>>
{
    ASSERT(range.atEnd() || range.peek().type() == CommaToken);

    if (range.atEnd())
        return { FallbackResult::None, { } };

    range.consumeIncludingWhitespace();

    auto tokens = resolveTokenRange(range, builder);

    if (functionId == CSSValueVar) {
        auto* registered = builder.state().document().customPropertyRegistry().get(variableName);
        if (registered && !registered->syntax.isUniversal()) {
            // https://drafts.css-houdini.org/css-properties-values-api/#fallbacks-in-var-references
            // The fallback value must match the syntax definition of the custom property being referenced,
            // otherwise the declaration is invalid at computed-value time
            if (!tokens || !CSSPropertyParser::isValidCustomPropertyValueForSyntax(registered->syntax, *tokens, context()))
                return { FallbackResult::Invalid, { } };

            return { FallbackResult::Valid, WTF::move(*tokens) };
        }
    }

    if (!tokens)
        return { FallbackResult::None, { } };

    return { FallbackResult::Valid, WTF::move(*tokens) };
}

static const Style::CustomProperty* propertyValueForVariableName(const AtomString& variableName, CSSValueID functionId, Style::Builder& builder)
{
    if (functionId == CSSValueEnv)
        return builder.state().document().constantProperties().values().get(variableName);

    // Apply this variable first, in case it is still unresolved
    builder.applyCustomProperty(variableName);

    return protect(builder.state().style())->customPropertyValue(variableName);
}

bool CSSVariableReferenceValue::resolveVariableReference(CSSParserTokenRange range, CSSValueID functionId, Vector<CSSParserToken>& tokens, Style::Builder& builder) const
{
    ASSERT(functionId == CSSValueVar || functionId == CSSValueEnv);

    range.consumeWhitespace();
    ASSERT(range.peek().type() == IdentToken);
    auto variableName = range.consumeIncludingWhitespace().value().toAtomString();

    // Fallback has to be resolved even when not used to detect cycles and invalid syntax.
    auto [fallbackResult, fallbackTokens] = resolveVariableFallback(variableName, range, functionId, builder);
    if (fallbackResult == FallbackResult::Invalid)
        return false;

    RefPtr property = propertyValueForVariableName(variableName, functionId, builder);

    if (!property || property->isGuaranteedInvalid()) {
        if (fallbackTokens.size() > maxSubstitutionTokens)
            return false;

        if (fallbackResult == FallbackResult::Valid) {
            tokens.appendVector(fallbackTokens);
            return true;
        }
        return false;
    }

    if (property->tokens().size() > maxSubstitutionTokens)
        return false;

    tokens.appendVector(property->tokens());
    return true;
}

static std::optional<CSSCustomPropertySyntax> parseAttrTypeSyntax(CSSParserTokenRange range)
{
    range.consumeWhitespace();
    if (range.atEnd())
        return { };

    // type() can contain either a quoted string or bare syntax tokens.
    if (range.peek().type() == StringToken) {
        auto syntax = range.consumeIncludingWhitespace().value();
        if (!range.atEnd())
            return { };
        return CSSCustomPropertySyntax::parse(syntax);
    }

    return CSSCustomPropertySyntax::parse(range.serialize());
}

static bool isValidAttrUnit(const CSSParserToken& token)
{
    if (token.type() == DelimiterToken)
        return token.delimiter() == '%';
    if (token.type() != IdentToken)
        return false;
    return CSSParserToken::stringToUnitType(token.value()) != CSSUnitType::CSS_UNKNOWN;
}

// Parses an attribute value as a single number token. Returns the numeric value and type.
static std::optional<std::pair<double, NumericValueType>> parseAttrValueAsNumber(const AtomString& attributeValue)
{
    auto tokenizer = CSSTokenizer::tryCreate(attributeValue.string());
    if (!tokenizer)
        return { };

    auto range = tokenizer->tokenRange();
    range.consumeWhitespace();
    if (range.peek().type() != NumberToken)
        return { };

    double value = range.peek().numericValue();
    auto valueType = range.peek().numericValueType();
    range.consumeIncludingWhitespace();
    if (!range.atEnd())
        return { };

    return { { value, valueType } };
}

bool CSSVariableReferenceValue::resolveAttrReference(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, Style::Builder& builder, Vector<AtomString, 8>& activeAttrNames) const
{
    // <attr()> = attr( <attr-name> <attr-type>? , <declaration-value>?)
    // <attr-type> = type( <syntax> ) | raw-string | number | <attr-unit>
    range.consumeWhitespace();

    if (range.peek().type() != IdentToken)
        return false;

    auto attrName = range.consumeIncludingWhitespace().value().toAtomString();

    // Parse optional <attr-type>.
    enum class AttrTypeKind { None, Syntax, RawString, Number, Unit };
    auto attrTypeKind = AttrTypeKind::None;
    std::optional<CSSCustomPropertySyntax> syntax;
    CSSParserToken unitToken(IdentToken);

    if (!range.atEnd() && range.peek().type() == FunctionToken && equalLettersIgnoringASCIICase(range.peek().value(), "type"_s)) {
        attrTypeKind = AttrTypeKind::Syntax;
        syntax = parseAttrTypeSyntax(range.consumeBlock());
        range.consumeWhitespace();

        if (!syntax || syntax->containsUnknownType())
            return false;
    } else if (!range.atEnd() && range.peek().type() != CommaToken) {
        if (range.peek().type() == IdentToken && equalLettersIgnoringASCIICase(range.peek().value(), "raw-string"_s)) {
            attrTypeKind = AttrTypeKind::RawString;
            range.consumeIncludingWhitespace();
        } else if (range.peek().type() == IdentToken && equalLettersIgnoringASCIICase(range.peek().value(), "number"_s)) {
            attrTypeKind = AttrTypeKind::Number;
            range.consumeIncludingWhitespace();
        } else if (isValidAttrUnit(range.peek())) {
            attrTypeKind = AttrTypeKind::Unit;
            unitToken = range.consumeIncludingWhitespace();
        } else
            return false;
    }

    // Parse optional fallback after comma.
    bool hasFallback = false;
    CSSParserTokenRange fallbackRange;
    if (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        hasFallback = true;
        fallbackRange = range;
    }

    auto resolveFallback = [&]() -> bool {
        if (!hasFallback)
            return false;
        auto fallbackTokens = resolveTokenRange(fallbackRange, builder, activeAttrNames);
        if (!fallbackTokens)
            return false;
        tokens.appendVector(*fallbackTokens);
        return true;
    };

    // Get the element and read the attribute value.
    RefPtr element = builder.state().element();
    if (!element)
        return resolveFallback();

    // Cycle detection for nested attr() references.
    if (activeAttrNames.contains(attrName))
        return false;
    activeAttrNames.append(attrName);
    auto popAttrName = makeScopeExit([&] {
        activeAttrNames.removeLast();
    });

    QualifiedName qualifiedName(nullAtom(), attrName.impl(), nullAtom());
    const AtomString& attributeValue = element->getAttribute(qualifiedName);

    // Step 5: raw-string or omitted type → substitute as CSS string.
    if (attrTypeKind == AttrTypeKind::None || attrTypeKind == AttrTypeKind::RawString) {
        if (attributeValue.isNull()) {
            if (hasFallback)
                return resolveFallback();
            // Spec step 7.1: If no fallback and type was omitted, return empty string.
            if (attrTypeKind == AttrTypeKind::None) {
                tokens.append(CSSParserToken(StringToken, emptyAtom()));
                return true;
            }
            // raw-string with no fallback: guaranteed-invalid.
            return false;
        }
        tokens.append(CSSParserToken(StringToken, attributeValue));
        return true;
    }

    // Step 4: number or <attr-unit>.
    if (attrTypeKind == AttrTypeKind::Number || attrTypeKind == AttrTypeKind::Unit) {
        if (!attributeValue.isNull()) {
            if (parseAttrValueAsNumber(attributeValue)) {
                // Construct the final value string and tokenize it.
                // Use AtomString to intern it so token StringViews remain valid.
                AtomString valueString;
                if (attrTypeKind == AttrTypeKind::Unit) {
                    if (unitToken.type() == DelimiterToken)
                        valueString = AtomString(makeString(attributeValue, '%'));
                    else
                        valueString = AtomString(makeString(attributeValue, unitToken.value()));
                } else
                    valueString = attributeValue;

                auto valueTokenizer = CSSTokenizer::tryCreate(valueString.string());
                if (valueTokenizer) {
                    auto valueRange = valueTokenizer->tokenRange();
                    valueRange.consumeWhitespace();
                    while (!valueRange.atEnd())
                        tokens.append(valueRange.consume());
                    return true;
                }
            }
        }
        return resolveFallback();
    }

    // Step 6: type(<syntax>) — parse attribute value according to the syntax.
    ASSERT(attrTypeKind == AttrTypeKind::Syntax && syntax);

    if (!attributeValue.isNull()) {
        auto tokenizer = CSSTokenizer::tryCreate(attributeValue.string());
        if (tokenizer) {
            auto attrTokenRange = tokenizer->tokenRange();
            attrTokenRange.consumeWhitespace();
            if (!attrTokenRange.atEnd()) {
                // FIXME: Substitute arbitrary substitution functions (var/attr/env) in the attr value per spec step 6.
                if (syntax->isUniversal() || CSSPropertyParser::isValidCustomPropertyValueForSyntax(*syntax, attrTokenRange, context())) {
                    // Re-read tokens for output (validation consumed the range).
                    auto outputRange = tokenizer->tokenRange();
                    outputRange.consumeWhitespace();
                    while (!outputRange.atEnd())
                        tokens.append(outputRange.consume());
                    return true;
                }
            }
        }
    }

    // Attribute value is invalid or missing; use fallback.
    return resolveFallback();
}

bool CSSVariableReferenceValue::evaluateDashedFunction(StringView functionName, CSSParserTokenRange, Vector<CSSParserToken>& tokens, Style::Builder& builder) const
{
    // https://drafts.csswg.org/css-mixins/#evaluating-custom-functions

    if (!builder.state().element())
        return false;

    auto scopedFunctionName = Style::ScopedName { functionName.toAtomString(), builder.state().styleScopeOrdinal() };

    CheckedPtr element = builder.state().element();
    auto customFunction = Style::Scope::resolveTreeScopedReference(*element, scopedFunctionName, [](const Style::Scope& scope, const AtomString& name) -> CheckedPtr<const Style::CustomFunction> {
        RefPtr resolver = scope.resolverIfExists();
        CheckedPtr registry = resolver ? resolver->customFunctionRegistry() : nullptr;
        return registry ? registry->functionForName(name) : nullptr;
    });

    if (!customFunction)
        return false;

    // FIXME: Evaluate the function instead of just substituting.

    auto properties = customFunction->properties;
    auto resultValue = dynamicDowncast<CSSCustomPropertyValue>(properties->getPropertyCSSValue(CSSPropertyResult));
    if (!resultValue)
        return false;

    auto data = resultValue->asVariableData();
    tokens.appendVector(data->tokens());
    return true;
}

std::optional<Vector<CSSParserToken>> CSSVariableReferenceValue::resolveTokenRange(CSSParserTokenRange range, Style::Builder& builder) const
{
    Vector<AtomString, 8> activeAttrNames;
    return resolveTokenRange(range, builder, activeAttrNames);
}

std::optional<Vector<CSSParserToken>> CSSVariableReferenceValue::resolveTokenRange(CSSParserTokenRange range, Style::Builder& builder, Vector<AtomString, 8>& activeAttrNames) const
{
    Vector<CSSParserToken> tokens;
    bool success = true;
    while (!range.atEnd()) {
        auto token = range.peek();
        if (token.type() == FunctionToken) {
            auto functionId = token.functionId();
            if (functionId == CSSValueVar || functionId == CSSValueEnv) {
                if (!resolveVariableReference(range.consumeBlock(), functionId, tokens, builder))
                    success = false;
                continue;
            }
            if (functionId == CSSValueAttr) {
                if (!resolveAttrReference(range.consumeBlock(), tokens, builder, activeAttrNames))
                    success = false;
                continue;
            }
            if (isCustomPropertyName(token.value())) {
                // <dashed-function>
                if (!evaluateDashedFunction(token.value(), range.consumeBlock(), tokens, builder))
                    success = false;
                continue;
            }
        }
        tokens.append(range.consume());
    }
    if (!success)
        return { };

    return tokens;
}

void CSSVariableReferenceValue::cacheSimpleReference()
{
    ASSERT(!m_simpleReference);

    auto range = m_data->tokenRange();

    auto functionId = range.peek().functionId();
    if (functionId != CSSValueVar && functionId != CSSValueEnv)
        return;

    auto variableRange = range.consumeBlock();
    if (!range.atEnd())
        return;

    variableRange.consumeWhitespace();

    auto variableName = variableRange.consumeIncludingWhitespace().value().toAtomString();

    // No fallback support on this path.
    if (!variableRange.atEnd())
        return;

    m_simpleReference = SimpleReference { variableName, functionId };
}

RefPtr<CSSVariableData> CSSVariableReferenceValue::tryResolveSimpleReference(Style::Builder& builder) const
{
    if (!m_simpleReference)
        return nullptr;

    // Shortcut for the simple common case of property:var(--foo)

    RefPtr property = propertyValueForVariableName(m_simpleReference->name, m_simpleReference->functionId, builder);
    if (!property || !std::holds_alternative<Ref<CSSVariableData>>(property->value()))
        return nullptr;

    return std::get<Ref<CSSVariableData>>(property->value()).ptr();
}

RefPtr<CSSVariableData> CSSVariableReferenceValue::resolveVariableReferences(Style::Builder& builder) const
{
    if (auto data = tryResolveSimpleReference(builder))
        return data;

    auto resolvedTokens = resolveTokenRange(m_data->tokenRange(), builder);
    if (!resolvedTokens)
        return nullptr;

    return CSSVariableData::create(*resolvedTokens, context());
}

RefPtr<CSSValue> CSSVariableReferenceValue::resolveSingleValue(Style::Builder& builder, CSSPropertyID propertyID) const
{
    if (!resolveAndCacheValue(builder, [this, propertyID](auto data) {
        m_cachedValue = CSSPropertyParser::parseStylePropertyLonghand(propertyID, data->tokens(), context());
#if ASSERT_ENABLED
        m_cachePropertyID = propertyID;
#endif
    }))
        return nullptr;

    ASSERT(m_cachePropertyID == propertyID);
    return m_cachedValue;
}

} // namespace WebCore
