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

#include "config.h"
#include "StyleSubstitutionResolver.h"

#include "CSSCustomPropertySyntax.h"
#include "CSSCustomPropertyValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSSelectorParser.h"
#include "CSSSerializationContext.h"
#include "CSSShorthandSubstitutionValue.h"
#include "CSSSubstitutionValue.h"
#include "CSSTokenizer.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include "CSSVariableData.h"
#include "ConstantPropertyMap.h"
#include "CustomFunctionRegistry.h"
#include "Document.h"
#include "Element.h"
#include "HTMLSelectElement.h"
#include "MatchResult.h"
#include "MutableStyleProperties.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderStyle+SettersInlines.h"
#include "SelectPopoverElement.h"
#include "StyleBuilder.h"
#include "StyleCustomProperty.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleLocalPropertyRegistry.h"
#include "StyleResolver.h"
#include "StyleScope.h"

namespace WebCore {
namespace Style {

static bool containsURLTokens(std::span<const CSSParserToken> tokens)
{
    for (auto& token : tokens) {
        if (token.type() == UrlToken)
            return true;
        if (token.type() == FunctionToken && (token.functionId() == CSSValueUrl || token.functionId() == CSSValueImageSet))
            return true;
    }
    return false;
}

void SubstitutionResolver::propagateAttrTaint(IsAttrTainted isAttrTainted, std::span<const CSSParserToken> tokens)
{
    if (isAttrTainted != IsAttrTainted::Yes)
        return;
    m_isAttrTainted = true;
    if (isInURLContext() || containsURLTokens(tokens))
        m_hasTaintedURL = true;
}

SubstitutionResolver::SubstitutionResolver(Builder& builder)
    : m_styleBuilder(builder)
{
}

auto SubstitutionResolver::substituteVariableFallback(const AtomString& variableName, CSSParserTokenRange range, CSSValueID functionId, const CSSParserContext& context) -> std::pair<FallbackResult, Vector<CSSParserToken>> {
    if (!range.atEnd() && range.peek().type() != CommaToken)
        return { FallbackResult::Invalid, { } };

    if (range.atEnd())
        return { FallbackResult::None, { } };

    range.consumeIncludingWhitespace();

    auto tokens = substituteTokenRange(range, context);

    if (functionId == CSSValueVar) {
        auto* registered = m_styleBuilder.state().registeredProperty(variableName);
        if (registered && !registered->syntax.isUniversal()) {
            // https://drafts.css-houdini.org/css-properties-values-api/#fallbacks-in-var-references
            // The fallback value must match the syntax definition of the custom property being referenced,
            // otherwise the declaration is invalid at computed-value time
            if (!tokens || !CSSPropertyParser::isValidCustomPropertyValueForSyntax(registered->syntax, *tokens, context))
                return { FallbackResult::Invalid, { } };

            return { FallbackResult::Valid, WTF::move(*tokens) };
        }
    }

    if (!tokens)
        return { FallbackResult::None, { } };

    return { FallbackResult::Valid, WTF::move(*tokens) };
}

RefPtr<const CustomProperty> SubstitutionResolver::propertyValueForVariableName(const AtomString& variableName, CSSValueID functionId)
{
    if (functionId == CSSValueEnv)
        return m_styleBuilder.state().document().constantProperties().values().get(variableName);

    // Apply this variable first, in case it is still unresolved
    m_styleBuilder.applyCustomProperty(variableName);

    return protect(m_styleBuilder.state().style())->customPropertyValue(variableName);
}

bool SubstitutionResolver::substituteVariableFunction(CSSParserTokenRange range, CSSValueID functionId, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // The maximum number of tokens that may be produced by a substitution function reference or fallback value.
    // https://drafts.csswg.org/css-variables/#long-variables
    static constexpr size_t maxSubstitutionTokens = 65536;

    ASSERT(functionId == CSSValueVar || functionId == CSSValueEnv);

    range.consumeWhitespace();
    if (range.peek().type() != IdentToken)
        return false;
    auto variableName = range.consumeIncludingWhitespace().value().toAtomString();

    // Fallback has to be resolved even when not used to detect cycles and invalid syntax.
    auto [fallbackResult, fallbackTokens] = substituteVariableFallback(variableName, range, functionId, context);
    if (fallbackResult == FallbackResult::Invalid)
        return false;

    RefPtr property = propertyValueForVariableName(variableName, functionId);

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

    // https://drafts.csswg.org/css-values-5/#attr-security
    // Propagate attr()-taint through var() references.
    propagateAttrTaint(property->isAttrTainted(), property->tokens());

    tokens.appendVector(property->tokens());
    return true;
}

// https://drafts.csswg.org/css-mixins/#evaluate-a-custom-function
// Registers each parameter with its type, resolves argument styles, then updates registrations
// to universal syntax with resolved values as initial values.
// Returns resolved argument properties to prepend to the body rule, or nullptr on failure.
RefPtr<MutableStyleProperties> SubstitutionResolver::resolveAndRegisterDashedFunctionArguments(const Vector<StyleRuleFunction::Parameter>& parameters, const Vector<Vector<CSSParserToken>>& arguments, LocalPropertyRegistry& registrations)
{
    // "For each function parameter, create a custom property registration with the parameter's type."
    auto argumentRegistrations = LocalPropertyRegistry { };
    for (auto& parameter : parameters) {
        argumentRegistrations.add({
            .name = AtomString { parameter.name },
            .syntax = parameter.type,
            .inherits = true,
        });
    }

    // "Let argument rule be an initially empty style rule" with first-valid(arg value, default value) for each parameter.
    auto argumentRule = MutableStyleProperties::create();
    for (unsigned i = 0; i < parameters.size(); ++i) {
        auto& parameter = parameters[i];
        auto argumentData = [&] -> RefPtr<CSSVariableData> {
            if (i < arguments.size() && !arguments[i].isEmpty())
                return CSSVariableData::create(CSSParserTokenRange { arguments[i] }, m_substitutionValue->context());
            return parameter.defaultValue;
        }();
        if (!argumentData)
            return nullptr;

        auto value = CSSCustomPropertyValue::createSyntaxAll(parameter.name, argumentData.releaseNonNull());
        argumentRule->addParsedProperty({ CSSPropertyCustom, WTF::move(value) });
    }

    // "Resolve function styles using custom function, argument rule, registrations, and calling context."
    Ref parentMatchResult = m_styleBuilder.matchResult();

    auto argumentMatchResult = MatchResult::create();
    argumentMatchResult->copyDeclarationsFrom(parentMatchResult);
    argumentMatchResult->authorDeclarations.append({ WTF::move(argumentRule) });

    auto builderContext = BuilderContext {
        .document = m_styleBuilder.state().document(),
        .parentStyle = &m_styleBuilder.state().renderStyle(),
        .element = m_styleBuilder.state().element(),
        .localPropertyRegistry = &argumentRegistrations
    };

    auto argumentStyles = RenderStyle::createPtr();
    Builder argumentBuilder(*argumentStyles, WTF::move(builderContext), argumentMatchResult.get());
    argumentBuilder.state().addGuardedFunctionContexts(m_styleBuilder.state());
    for (auto& parameter : parameters)
        argumentBuilder.applyCustomProperty(parameter.name);

    // "Set its initial value to the corresponding value in argument styles, set its syntax to the universal syntax definition,
    // and prepend a custom property to body rule with the property name and value in argument styles."
    auto resolvedArgumentProperties = MutableStyleProperties::create();
    for (auto& parameter : parameters) {
        RefPtr resolvedValue = argumentStyles->customPropertyValue(parameter.name);

        registrations.add({
            .name = AtomString { parameter.name },
            .syntax = CSSCustomPropertySyntax::universal(),
            .inherits = true,
            .initialValue = resolvedValue,
        });

        if (resolvedValue && !resolvedValue->isGuaranteedInvalid()) {
            auto tokenData = CSSVariableData::create(CSSParserTokenRange { resolvedValue->tokens() });
            auto value = CSSCustomPropertyValue::createSyntaxAll(parameter.name, WTF::move(tokenData));
            resolvedArgumentProperties->addParsedProperty({ CSSPropertyCustom, WTF::move(value) });
        }
    }

    return resolvedArgumentProperties;
}

bool SubstitutionResolver::substituteDashedFunction(StringView functionName, CSSParserTokenRange range, Vector<CSSParserToken>& tokens)
{
    // https://drafts.csswg.org/css-mixins/#evaluating-custom-functions

    if (!m_styleBuilder.state().element())
        return false;

    auto scopedFunctionName = ScopedName { functionName.toAtomString(), m_styleBuilder.state().styleScopeOrdinal() };

    CheckedPtr element = m_styleBuilder.state().element();
    auto customFunction = Scope::resolveTreeScopedReference(*element, scopedFunctionName, [](const Scope& scope, const AtomString& name) -> CheckedPtr<const CustomFunction> {
        RefPtr resolver = scope.resolverIfExists();
        CheckedPtr registry = resolver ? resolver->customFunctionRegistry() : nullptr;
        return registry ? registry->functionForName(name) : nullptr;
    });

    if (!customFunction)
        return false;

    auto guard = m_styleBuilder.state().guardSubstitutionContext({ SubstitutionContext::Type::Function, scopedFunctionName.name });

    if (guard.isCyclicContext())
        return false;

    auto& parameters = customFunction->parameters;

    // Parse and substitute arguments.
    auto substitutedArguments = [&] -> std::optional<Vector<Vector<CSSParserToken>>> {
        Vector<Vector<CSSParserToken>> result;
        for (unsigned i = 0; !range.atEnd(); ++i) {
            auto argumentRange = CSSPropertyParserHelpers::consumeArgument(range, i);
            if (!argumentRange)
                break;
            auto substituted = substituteTokenRange(*argumentRange, m_substitutionValue->context());
            if (!substituted)
                return { };
            result.append(WTF::move(*substituted));
        }
        if (result.size() > parameters.size())
            return { };
        return result;
    }();

    if (!substitutedArguments)
        return false;

    auto resultValue = dynamicDowncast<CSSCustomPropertyValue>(protect(customFunction->properties)->getPropertyCSSValue(CSSPropertyResult));
    if (!resultValue)
        return false;

    // "Let registrations be an initially empty set of custom property registrations."
    auto registrations = LocalPropertyRegistry { };

    auto resolvedArgumentProperties = resolveAndRegisterDashedFunctionArguments(parameters, *substitutedArguments, registrations);
    if (!resolvedArgumentProperties)
        return false;

    // "If custom function has a return type, create a custom property registration with the name 'result'."
    if (!customFunction->returnType.isUniversal()) {
        registrations.add({
            .name = "result"_s,
            .syntax = customFunction->returnType,
            .inherits = false,
        });
    }

    // "Let body rule be the function body."
    Ref parentMatchResult = m_styleBuilder.matchResult();

    auto bodyMatchResult = MatchResult::create();
    bodyMatchResult->copyDeclarationsFrom(parentMatchResult);
    bodyMatchResult->authorDeclarations.append({ *resolvedArgumentProperties });
    bodyMatchResult->authorDeclarations.append({ customFunction->properties });

    // "Resolve function styles using custom function, body rule, registrations, and calling context."
    auto builderContext = BuilderContext {
        .document = m_styleBuilder.state().document(),
        .parentStyle = &m_styleBuilder.state().renderStyle(),
        .element = m_styleBuilder.state().element(),
        .localPropertyRegistry = &registrations
    };

    auto bodyStyles = RenderStyle::createPtr();
    Builder bodyBuilder(*bodyStyles, WTF::move(builderContext), bodyMatchResult.get());
    bodyBuilder.state().addGuardedFunctionContexts(m_styleBuilder.state());

    // "Return the value of the result property in body styles."
    auto resolvedResult = bodyBuilder.resolveFunctionResult(*resultValue);
    if (!resolvedResult)
        return false;

    // "If substitution context is marked as cyclic, return the guaranteed-invalid value."
    if (guard.isCyclicContext())
        return false;

    tokens.appendVector(resolvedResult->tokens());
    return true;
}

auto SubstitutionResolver::substituteAttrArgumentGrammar(CSSParserTokenRange range, const CSSParserContext& context) -> std::optional<AttrArgumentGrammarSubstitution>
{
    // https://drafts.csswg.org/css-values-5/#argument-grammars
    // <attr-args> = attr( <declaration-value>, <declaration-value>? )
    // Splits at the first literal comma and substitutes the first argument.

    range.consumeWhitespace();

    auto start = range;
    while (!range.atEnd() && range.peek().type() != CommaToken)
        range.consumeComponentValue();
    auto firstArgRange = start.rangeUntil(range);

    std::optional<CSSParserTokenRange> fallbackRange;
    if (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range))
        fallbackRange = range;

    auto substitutedFirstArg = substituteTokenRange(firstArgRange, context);
    if (!substitutedFirstArg)
        return { };

    return AttrArgumentGrammarSubstitution { WTF::move(*substitutedFirstArg), fallbackRange };
}

bool SubstitutionResolver::substituteAttrFunction(CSSParserTokenRange argumentsRange, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-values-5/#funcdef-attr

    // <attr-args> = attr( <declaration-value>, <declaration-value>? )
    auto attrArgs = substituteAttrArgumentGrammar(argumentsRange, context);
    if (!attrArgs)
        return false;

    // attr() = attr( <attr-name> <attr-type>? , <declaration-value>?)
    auto range = CSSParserTokenRange { attrArgs->firstArg };

    // Consume <attr-name> = <wq-name> = [ <ident> | * ]? '|' <ident>  or  <ident>
    auto parsedName = consumeQualifiedName(range);
    if (!parsedName)
        return false;
    range.consumeWhitespace();

    auto attributeName = parsedName->name;

    // Consume optional <attr-type>.
    // https://drafts.csswg.org/css-values-5/#typedef-attr-type
    // <attr-type> = type( <syntax> ) | raw-string | number | <attr-unit>
    enum class AttrType { RawString, Number, Unit, Percentage, Syntax };
    struct AttrTypeResult {
        AttrType type;
        CSSUnitType unitType { CSSUnitType::CSS_UNKNOWN };
        CSSCustomPropertySyntax syntax { };
    };

    auto consumeAttrType = [&] -> std::optional<AttrTypeResult> {
        if (range.peek().type() == FunctionToken) {
            auto syntax = CSSCustomPropertySyntax::consumeType(range);
            if (!syntax)
                return { };
            return AttrTypeResult { AttrType::Syntax, { }, WTF::move(*syntax) };
        }

        if (range.peek().type() == IdentToken) {
            auto value = range.peek().value();
            if (equalLettersIgnoringASCIICase(value, "raw-string"_s)) {
                range.consumeIncludingWhitespace();
                return AttrTypeResult { AttrType::RawString };
            }
            if (equalLettersIgnoringASCIICase(value, "number"_s)) {
                range.consumeIncludingWhitespace();
                return AttrTypeResult { AttrType::Number };
            }
            auto unit = CSSParserToken::stringToUnitType(value);
            if (unit == CSSUnitType::CSS_UNKNOWN)
                return { };
            range.consumeIncludingWhitespace();
            return AttrTypeResult { AttrType::Unit, unit };
        }

        if (range.peek().type() == DelimiterToken && range.peek().delimiter() == '%') {
            range.consumeIncludingWhitespace();
            return AttrTypeResult { AttrType::Percentage };
        }

        return { };
    };

    std::optional<AttrTypeResult> parsedAttrType;
    if (!range.atEnd()) {
        parsedAttrType = consumeAttrType();
        if (!parsedAttrType)
            return false;
    }

    if (!range.atEnd())
        return false;

    m_styleBuilder.state().registerSubstitutionAttribute(attributeName);
    protect(m_styleBuilder.state().style())->setHasAttrContent();

    CheckedPtr element = m_styleBuilder.state().element();
    if (!element)
        return false;

    // Resolve namespace prefix to URI.
    auto namespaceURI = [&] -> AtomString {
        auto& prefix = parsedName->namespacePrefix;
        if (prefix.isEmpty())
            return nullAtom();
        return m_substitutionValue->m_namespacePrefixMap.get(prefix);
    }();

    // https://drafts.csswg.org/css-values-5/#guarded
    auto guard = m_styleBuilder.state().guardSubstitutionContext({ SubstitutionContext::Type::Attribute, attributeName });

    // Resolve fallback lazily to avoid var() cycle detection side effects during primary resolution.
    auto resolveFallback = [&] -> std::optional<Vector<CSSParserToken>> {
        if (!attrArgs->fallbackRange)
            return { };
        return substituteTokenRange(*attrArgs->fallbackRange, context);
    };

    // https://drafts.csswg.org/css-values-5/#replace-an-attr-function
    auto substituteFailure = [&] -> bool {
        // "If second arg is null, and syntax was omitted, return an empty CSS <string>."
        if (!attrArgs->fallbackRange && !parsedAttrType) {
            tokens.append(CSSParserToken(StringToken, emptyAtom()));
            return true;
        }
        auto fallbackTokens = resolveFallback();
        // "If second arg is null, return the guaranteed-invalid value."
        if (!fallbackTokens)
            return false;
        // "Substitute arbitrary substitution functions in second arg, and return the result."
        tokens.appendVector(*fallbackTokens);
        return true;
    };

    // If a non-empty prefix was given but couldn't be resolved, trigger fallback.
    if (!parsedName->namespacePrefix.isEmpty() && namespaceURI.isNull())
        return substituteFailure();

    if (guard.isCyclicContext()) {
        if (parsedAttrType)
            return false;
        return substituteFailure();
    }

    auto& attributeValue = element->getAttribute(QualifiedName { nullAtom(), attributeName, namespaceURI });

    if (attributeValue.isNull())
        return substituteFailure();

    // "If syntax is null or the keyword raw-string, return a CSS <string> whose value is attr value."
    auto attrType = parsedAttrType ? parsedAttrType->type : AttrType::RawString;

    switch (attrType) {
    // "If given as the raw-string keyword, or omitted entirely, it causes the attribute’s literal
    //  value to be treated as the value of a CSS string, with no CSS parsing performed at all."
    case AttrType::RawString:
        tokens.append(CSSParserToken(StringToken, attributeValue));
        return true;

    // "If given as the number keyword, it causes the attribute's literal value, after stripping
    //  leading and trailing whitespace, to be parsed as a <number-token>. Values that fail to
    //  parse trigger fallback."
    case AttrType::Number: {
        CSSTokenizer tokenizer(attributeValue.string().trim(isUnicodeCompatibleASCIIWhitespace<UChar>));
        auto tokenRange = tokenizer.tokenRange();
        tokenRange.consumeWhitespace();
        if (tokenRange.peek().type() != NumberToken)
            return substituteFailure();
        auto numberToken = tokenRange.consumeIncludingWhitespace();
        if (!tokenRange.atEnd())
            return substituteFailure();
        tokens.append(CSSParserToken(numberToken.numericValue(), numberToken.numericValueType(), numberToken.numericSign(), StringView()));
        return true;
    }

    // "If given as an <attr-unit> value, the value is first parsed as if number keyword was specified,
    //  then the resulting numeric value is turned into a dimension with the corresponding unit,
    //  or a percentage if % was given."
    case AttrType::Unit:
    case AttrType::Percentage: {
        CSSTokenizer tokenizer(attributeValue.string().trim(isUnicodeCompatibleASCIIWhitespace<UChar>));
        auto tokenRange = tokenizer.tokenRange();
        tokenRange.consumeWhitespace();
        if (tokenRange.peek().type() != NumberToken)
            return substituteFailure();
        auto numberToken = tokenRange.consumeIncludingWhitespace();
        if (!tokenRange.atEnd())
            return substituteFailure();
        auto token = CSSParserToken(numberToken.numericValue(), numberToken.numericValueType(), numberToken.numericSign(), StringView());
        if (attrType == AttrType::Percentage)
            token.convertToPercentage();
        else
            token.convertToDimensionWithUnit(CSSPrimitiveValue::unitTypeString(parsedAttrType->unitType));
        tokens.append(token);
        return true;
    }

    // "If given as a type() function, the value is parsed according to the <syntax> argument,
    //  and substitutes as the resulting tokens. Values that fail to parse according to the syntax
    //  trigger fallback."
    case AttrType::Syntax: {
        CSSTokenizer tokenizer(attributeValue.string());
        m_intermediateTokenStrings.appendVector(tokenizer.escapedStringsForAdoption());

        auto substitutedTokens = substituteTokenRange(tokenizer.tokenRange(), context);
        if (!substitutedTokens)
            return substituteFailure();

        // If the context became cyclic during substitution, the value is invalid.
        if (guard.isCyclicContext())
            return substituteFailure();

        if (parsedAttrType->syntax.isUniversal()) {
            tokens.appendVector(*substitutedTokens);
            return true;
        }

        // Parse against the syntax and re-tokenize from the normalized serialization.
        CSSParserTokenRange substitutedRange(*substitutedTokens);
        auto parsedValue = CSSPropertyParser::parseWithSyntax(parsedAttrType->syntax, substitutedRange, context);
        if (!parsedValue)
            return substituteFailure();

        auto serialized = parsedValue->cssText(CSS::defaultSerializationContext());
        CSSTokenizer resultTokenizer(serialized);
        m_intermediateTokenStrings.appendVector(resultTokenizer.escapedStringsForAdoption());
        m_intermediateTokenStrings.append(WTF::move(serialized));

        tokens.append(resultTokenizer.tokenRange().span());

        return true;
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool SubstitutionResolver::substituteInternalAutoBaseFunction(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // -internal-auto-base(autoValue, baseValue)
    // Picks between the two arguments based on whether the element has base appearance.

    auto firstArgRange = CSSPropertyParserHelpers::consumeArgument(range, 0);
    if (!firstArgRange)
        return false;

    auto secondArgRange = CSSPropertyParserHelpers::consumeArgument(range, 1);
    if (!secondArgRange)
        return false;

    auto selectedRange = isBaseAppearance() ? *secondArgRange : *firstArgRange;

    // Strip outer braces if present, allowing comma-containing values like:
    // -internal-auto-base({value1, value2}, {value3, value4})
    if (!selectedRange.atEnd() && selectedRange.peek().type() == LeftBraceToken)
        selectedRange = selectedRange.consumeBlock();

    auto selectedTokens = substituteTokenRange(selectedRange, context);
    if (!selectedTokens)
        return false;

    tokens.appendVector(*selectedTokens);
    return true;
}

std::optional<Vector<CSSParserToken>> SubstitutionResolver::substituteTokenRange(CSSParserTokenRange range, const CSSParserContext& context)
{
    Vector<CSSParserToken> tokens;
    bool success = true;

    while (!range.atEnd()) {
        auto token = range.peek();
        if (token.type() == FunctionToken) {
            auto functionId = token.functionId();
            if (functionId == CSSValueVar || functionId == CSSValueEnv) {
                if (!substituteVariableFunction(range.consumeBlock(), functionId, tokens, context))
                    success = false;
                continue;
            }
            if (functionId == CSSValueAttr) {
                auto startIndex = tokens.size();
                if (substituteAttrFunction(range.consumeBlock(), tokens, context))
                    propagateAttrTaint(IsAttrTainted::Yes, std::span(tokens).subspan(startIndex));
                else
                    success = false;
                continue;
            }
            if (functionId == CSSValueInternalAutoBase) {
                if (!substituteInternalAutoBaseFunction(range.consumeBlock(), tokens, context))
                    success = false;
                continue;
            }
            if (isCustomPropertyName(token.value())) {
                // <dashed-function>
                if (!substituteDashedFunction(token.value(), range.consumeBlock(), tokens))
                    success = false;
                continue;
            }
        }

        updateURLContext(token);

        tokens.append(range.consume());
    }
    if (!success)
        return { };

    return tokens;
}

void SubstitutionResolver::updateURLContext(const CSSParserToken& token)
{
    if (token.getBlockType() == CSSParserToken::BlockStart) {
        if (m_urlContextDepth)
            ++m_urlContextDepth;
        else if (token.type() == FunctionToken && (token.functionId() == CSSValueUrl || token.functionId() == CSSValueImageSet))
            m_urlContextDepth = 1;
        return;
    }
    if (token.getBlockType() == CSSParserToken::BlockEnd && m_urlContextDepth)
        --m_urlContextDepth;
}

RefPtr<CSSVariableData> SubstitutionResolver::trySimpleSubstitution(const CSSSubstitutionValue& value)
{
    if (!value.m_simpleReference)
        return nullptr;

    // Shortcut for simple -internal-auto-base(val1, val2): return cached data if appearance hasn't changed.
    if (value.m_simpleReference->functionId == CSSValueInternalAutoBase)
        return value.m_cache.isBaseAppearance == isBaseAppearance() ? value.m_cache.dependencyData : nullptr;

    // Shortcut for the simple common case of property:var(--foo)
    RefPtr property = propertyValueForVariableName(value.m_simpleReference->name, value.m_simpleReference->functionId);
    if (!property || !std::holds_alternative<Ref<CSSVariableData>>(property->value()))
        return nullptr;

    return std::get<Ref<CSSVariableData>>(property->value()).ptr();
}

bool SubstitutionResolver::isBaseAppearance()
{
    auto& state = m_styleBuilder.state();
    if (state.style().appearance() == StyleAppearance::Base)
        return true;
    if (state.style().appearance() == StyleAppearance::BaseSelect) {
        CheckedPtr element = state.element();
        return element && isAnyOf<HTMLSelectElement, SelectPopoverElement>(*element);
    }
    return false;
}

RefPtr<CSSVariableData> SubstitutionResolver::substitute(const CSSSubstitutionValue& value)
{
    m_isAttrTainted = false;
    m_hasTaintedURL = false;
    m_substitutionValue = &value;

    if (auto data = trySimpleSubstitution(value)) {
        propagateAttrTaint(data->isAttrTainted(), data->tokens());
        return data;
    }

    auto& context = value.context();
    auto substitutedTokens = substituteTokenRange(value.m_data->tokenRange(), context);
    if (!substitutedTokens) {
        m_intermediateTokenStrings.clear();
        return nullptr;
    }

    auto data = CSSVariableData::create(*substitutedTokens, m_isAttrTainted ? IsAttrTainted::Yes : IsAttrTainted::No, context);
    m_intermediateTokenStrings.clear();
    return data;
}

RefPtr<CSSValue> SubstitutionResolver::substituteAndParse(const CSSSubstitutionValue& substitutionValue, CSSPropertyID propertyID)
{
    auto data = substitute(substitutionValue);
    if (!data)
        return nullptr;

    // https://drafts.csswg.org/css-values-5/#attr-security
    // Using an attr()-tainted value as or in a <url> makes a declaration invalid at computed-value time.
    if (propertyID != CSSPropertyCustom && m_hasTaintedURL)
        return nullptr;

    if (!arePointingToEqualData(substitutionValue.m_cache.dependencyData, data) || substitutionValue.m_cache.propertyID != propertyID) {
        substitutionValue.m_cache.value = CSSPropertyParser::parseStylePropertyLonghand(propertyID, data->tokens(), substitutionValue.context());
        substitutionValue.m_cache.propertyID = propertyID;
    }
    substitutionValue.m_cache.dependencyData = WTF::move(data);

    if (substitutionValue.m_simpleReference && substitutionValue.m_simpleReference->functionId == CSSValueInternalAutoBase)
        substitutionValue.m_cache.isBaseAppearance = isBaseAppearance();

    return substitutionValue.m_cache.value;
}

RefPtr<CSSValue> SubstitutionResolver::substituteAndParseShorthand(const CSSShorthandSubstitutionValue& substitution, CSSPropertyID propertyID)
{
    ASSERT(!CSSProperty::isDirectionAwareProperty(propertyID));

    auto& substitutionValue = substitution.shorthandValue();

    auto data = substitute(substitutionValue);
    if (!data)
        return nullptr;

    if (m_hasTaintedURL)
        return nullptr;

    if (!arePointingToEqualData(substitutionValue.m_cache.dependencyData, data)) {
        ParsedPropertyVector parsedProperties;
        if (!CSSPropertyParser::parseValue(substitution.m_shorthandPropertyId, IsImportant::No, data->tokens(), data->context(), parsedProperties, StyleRuleType::Style))
            substitution.m_cachedPropertyValues = { };
        else
            substitution.m_cachedPropertyValues = parsedProperties;
    }
    substitutionValue.m_cache.dependencyData = WTF::move(data);

    if (substitutionValue.m_simpleReference && substitutionValue.m_simpleReference->functionId == CSSValueInternalAutoBase)
        substitutionValue.m_cache.isBaseAppearance = isBaseAppearance();

    for (auto& property : substitution.m_cachedPropertyValues) {
        if (CSSProperty::resolveDirectionAwareProperty(property.id(), m_styleBuilder.state().style().writingMode()) == propertyID)
            return property.value();
    }

    return nullptr;
}

} // namespace Style
} // namespace WebCore
