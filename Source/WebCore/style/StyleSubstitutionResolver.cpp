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
#include "CSSPropertyParserConsumer+Ident.h"
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
#include "CSSWideKeyword.h"
#include "ConstantPropertyMap.h"
#include "CustomFunctionRegistry.h"
#include "Document.h"
#include "Element.h"
#include "ElementInlines.h"
#include "HTMLSelectElement.h"
#include "MatchResult.h"
#include "MutableStyleProperties.h"
#include "SelectPopoverElement.h"
#include "StyleBuilder.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleCustomProperty.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleLocalPropertyRegistry.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/IndexedRange.h>

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

// A comma-containing value can be passed as a single free-form-production argument by wrapping it in
// curly braces. The braces are syntactic and the argument is the block's contents.
// https://drafts.csswg.org/css-values-5/#component-function-commas
static CSSParserTokenRange unwrapArgumentBraces(CSSParserTokenRange argument)
{
    auto range = argument;
    range.consumeWhitespace();
    if (range.peek().type() != LeftBraceToken)
        return argument;
    auto contents = range.consumeBlock();
    range.consumeWhitespace();
    return range.atEnd() ? contents : argument;
}

static Ref<CSSVariableData> createFirstValidVariableData(std::span<const std::span<const CSSParserToken>> candidates, const CSSParserContext& context)
{
    // Uses the internal -internal-first-valid name rather than the public first-valid(). The public
    // function must validate against any property's grammar, which is not implemented yet. See the
    // FIXME on substituteFirstValid().
    Vector<CSSParserToken> tokens;
    tokens.append(CSSParserToken(FunctionToken, "-internal-first-valid"_s, CSSParserToken::BlockStart));
    bool isFirst = true;
    for (auto& candidate : candidates) {
        if (!isFirst)
            tokens.append(CSSParserToken(CommaToken));
        isFirst = false;
        tokens.append(candidate);
    }
    tokens.append(CSSParserToken(RightParenthesisToken, CSSParserToken::BlockEnd));
    return CSSVariableData::create(CSSParserTokenRange { tokens }, context);
}

void SubstitutionResolver::propagateAttrTaint(IsAttrTainted isAttrTainted, std::span<const CSSParserToken> tokens)
{
    if (isAttrTainted != IsAttrTainted::Yes)
        return;
    m_isAttrTainted = true;
    if (isInURLContext() || containsURLTokens(tokens))
        m_hasTaintedURL = true;
}

SubstitutionResolver::SubstitutionResolver(Builder& builder, const CSSRegisteredCustomProperty* registration)
    : m_styleBuilder(builder)
    , m_registration(registration)
{
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

    // Substitute the optional `, <fallback>` only when the referenced value is guaranteed-invalid.
    // https://drafts.csswg.org/css-variables-2/#replace-a-var-function
    std::optional<CSSParserTokenRange> fallbackRange;
    if (!range.atEnd()) {
        if (range.peek().type() != CommaToken)
            return false;
        range.consumeIncludingWhitespace();
        range.trimTrailingWhitespace();
        fallbackRange = range;
    }

    RefPtr property = propertyValueForVariableName(variableName, functionId);

    if (property && !property->isGuaranteedInvalid()) {
        if (property->tokens().size() > maxSubstitutionTokens)
            return false;

        // https://drafts.csswg.org/css-values-5/#attr-security
        // Propagate attr()-taint through var() references.
        propagateAttrTaint(property->isAttrTainted(), property->tokens());

        tokens.appendVector(property->tokens());
        return true;
    }

    if (fallbackRange) {
        auto fallbackTokens = substituteTokenRange(*fallbackRange, context);
        if (!fallbackTokens || fallbackTokens->size() > maxSubstitutionTokens)
            return false;

        if (functionId == CSSValueVar) {
            auto* registered = m_styleBuilder.state().registeredProperty(variableName);
            // https://drafts.css-houdini.org/css-properties-values-api/#fallbacks-in-var-references
            // A used fallback must match the referenced registered property's syntax.
            if (registered && !registered->syntax.isUniversal()
                && !CSSPropertyParser::isValidCustomPropertyValueForSyntax(registered->syntax, *fallbackTokens, context))
                return false;
        }

        tokens.appendVector(*fallbackTokens);
        return true;
    }

    return false;
}

// https://drafts.csswg.org/css-values-5/#first-valid
// FIXME: This only validates against a custom property's registered syntax. The real, author-exposed
// first-valid() is a <whole-value> usable on any property, so candidates must be validated against the
// target property's grammar (e.g. by feeding them to the property parser) rather than only a registration.
bool SubstitutionResolver::substituteFirstValid(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    for (unsigned i = 0; !range.atEnd(); ++i) {
        auto candidateRange = CSSPropertyParserHelpers::consumeArgument(range, i);
        if (!candidateRange)
            break;

        auto substituted = substituteTokenRange(*candidateRange, context);
        if (!substituted || substituted->isEmpty())
            continue;

        if (m_registration && !m_registration->syntax.isUniversal()
            && !CSSPropertyParser::isValidCustomPropertyValueForSyntax(m_registration->syntax, CSSParserTokenRange { *substituted }, context))
            continue;

        tokens.appendVector(*substituted);
        return true;
    }
    return false;
}

// https://drafts.csswg.org/css-mixins/#evaluate-a-custom-function
// Registers each parameter with its type, resolves argument styles, then updates registrations
// to universal syntax with resolved values as initial values.
// Returns resolved argument properties to prepend to the body rule, or nullptr on failure.
RefPtr<MutableStyleProperties> SubstitutionResolver::resolveAndRegisterDashedFunctionArguments(const Vector<StyleRuleFunction::Parameter>& parameters, const Vector<Vector<CSSParserToken>>& arguments, LocalPropertyRegistry& registrations, ScopeOrdinal definitionScope)
{
    // A parameter without a default requires a corresponding argument. A missing one makes the whole
    // invocation guaranteed-invalid (unlike a supplied-but-invalid argument, which defaults below).
    for (auto [i, parameter] : indexedRange(parameters)) {
        if (!parameter.defaultValue && i >= arguments.size())
            return nullptr;
    }

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
    for (auto [i, parameter] : indexedRange(parameters)) {
        bool hasArgument = i < arguments.size() && !arguments[i].isEmpty();

        // first-valid(arg value, default value). A parameter with neither is omitted from the rule,
        // resolving to the guaranteed-invalid value via its (valueless) registration.
        auto candidates = [&] {
            Vector<std::span<const CSSParserToken>, 2> candidates;
            if (hasArgument)
                candidates.append(arguments[i].span());
            if (parameter.defaultValue)
                candidates.append(parameter.defaultValue->tokens().span());
            return candidates;
        }();
        if (candidates.isEmpty())
            continue;

        // A bare CSS-wide keyword (e.g. a parameter defaulting to `inherit`) keeps its keyword semantics
        // rather than becoming a literal value. https://drafts.csswg.org/css-mixins/#evaluating-custom-functions
        auto primaryTokens = CSSParserTokenRange { candidates.first() };
        primaryTokens.consumeWhitespace();
        if (auto keyword = CSSPropertyParserHelpers::consumeCSSWideKeyword(primaryTokens); keyword && primaryTokens.atEnd()) {
            argumentRule->addParsedProperty({ CSSPropertyCustom, CSSCustomPropertyValue::createWithCSSWideKeyword(parameter.name, *keyword) });
            continue;
        }

        // Fast path: an untyped parameter with an argument needs no first-valid(). The argument is
        // already substituted and, being non-empty, is always valid for the universal syntax, so it
        // wins outright and the default is irrelevant.
        if (hasArgument && parameter.type.isUniversal()) {
            auto value = CSSCustomPropertyValue::createSyntaxAll(parameter.name, CSSVariableData::create(arguments[i], m_substitutionValue->context()));
            argumentRule->addParsedProperty({ CSSPropertyCustom, WTF::move(value) });
            continue;
        }

        auto firstValidData = createFirstValidVariableData(candidates.span(), m_substitutionValue->context());
        auto value = CSSCustomPropertyValue::createUnresolved(parameter.name, CSSSubstitutionValue::create(WTF::move(firstValidData)));
        argumentRule->addParsedProperty({ CSSPropertyCustom, WTF::move(value) });
    }

    // "Resolve function styles using custom function, argument rule, registrations, and calling context."
    // The hypothetical element acts as a child of the calling element, inheriting its computed custom
    // properties on demand, so defaults like `var(--caller-prop)` or `inherit` resolve against it.
    auto argumentMatchResult = MatchResult::create();
    argumentMatchResult->authorDeclarations.append({ .properties = WTF::move(argumentRule), .styleScopeOrdinal = definitionScope });

    auto builderContext = BuilderContext {
        .document = m_styleBuilder.state().document(),
        .parentStyle = &m_styleBuilder.state().renderStyle(),
        .element = m_styleBuilder.state().element(),
        .localPropertyRegistry = &argumentRegistrations,
        .callingContextBuilder = &m_styleBuilder
    };

    auto argumentStyles = Style::ComputedStyle::createPtr();
    Builder argumentBuilder(*argumentStyles, WTF::move(builderContext), argumentMatchResult);
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
            auto tokenData = CSSVariableData::create(CSSParserTokenRange { resolvedValue->tokens() }, resolvedValue->isAttrTainted(), m_substitutionValue->context());
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
    auto resolved = resolveTreeScopedReference(*element, scopedFunctionName, [](const Scope& scope, const ScopedName& scopedName) -> std::optional<std::pair<CheckedRef<const CustomFunction>, ScopeOrdinal>> {
        RefPtr resolver = scope.resolverIfExists();
        CheckedPtr registry = resolver ? resolver->customFunctionRegistry() : nullptr;
        CheckedPtr function = registry ? registry->functionForName(scopedName.name) : nullptr;
        if (!function)
            return { };
        return std::pair { function.releaseNonNull(), scopedName.scopeOrdinal };
    });

    if (!resolved)
        return false;

    auto& [customFunction, foundScopeOrdinal] = *resolved;

    auto guard = m_styleBuilder.state().guardSubstitutionContext({ SubstitutionContext::Type::Function, scopedFunctionName.name, foundScopeOrdinal });

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
            auto substituted = substituteTokenRange(unwrapArgumentBraces(*argumentRange), m_substitutionValue->context());
            // A failed substitution leaves the argument guaranteed-invalid (empty) so it defaults via
            // first-valid(), rather than aborting. https://drafts.csswg.org/css-mixins/#replace-a-dashed-function
            result.append(substituted.value_or(Vector<CSSParserToken> { }));
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

    auto resolvedArgumentProperties = resolveAndRegisterDashedFunctionArguments(parameters, *substitutedArguments, registrations, foundScopeOrdinal);
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
    // The body resolves tree-scoped references (var(), nested dashed-functions) relative to the scope
    // where the function was defined, not the calling element's scope.
    auto bodyMatchResult = MatchResult::create();
    bodyMatchResult->authorDeclarations.append({ *resolvedArgumentProperties });
    bodyMatchResult->authorDeclarations.append({ .properties = customFunction->properties, .styleScopeOrdinal = foundScopeOrdinal });

    // "Resolve function styles using custom function, body rule, registrations, and calling context."
    // The hypothetical element acts as a child of the calling element, inheriting its computed custom
    // properties on demand via the calling context's builder (https://drafts.csswg.org/css-mixins/#evaluating-custom-functions).
    auto builderContext = BuilderContext {
        .document = m_styleBuilder.state().document(),
        .parentStyle = &m_styleBuilder.state().renderStyle(),
        .element = m_styleBuilder.state().element(),
        .localPropertyRegistry = &registrations,
        .callingContextBuilder = &m_styleBuilder
    };

    auto bodyStyles = Style::ComputedStyle::createPtr();
    Builder bodyBuilder(*bodyStyles, WTF::move(builderContext), bodyMatchResult);
    bodyBuilder.state().addGuardedFunctionContexts(m_styleBuilder.state());

    // "Return the value of the result property in body styles."
    // Body custom properties are applied lazily as result's var() references reach them.
    auto resolvedResult = bodyBuilder.resolveFunctionResult(*resultValue);
    if (!resolvedResult)
        return false;

    // "If substitution context is marked as cyclic, return the guaranteed-invalid value."
    if (guard.isCyclicContext())
        return false;

    return WTF::switchOn(*resolvedResult,
        [&](const Ref<const CustomProperty>& result) {
            // https://drafts.csswg.org/css-values-5/#attr-security
            // Propagate attr()-taint from the function result into the calling context.
            propagateAttrTaint(result->isAttrTainted(), result->tokens());
            // Tokens reference result's string backing; keep it alive until CSSVariableData re-captures.
            tokens.appendVector(result->tokens());
            m_intermediateCustomProperties.append(result.copyRef());
            return true;
        },
        [&](CSSWideKeyword keyword) {
            // https://drafts.csswg.org/css-mixins/#evaluating-custom-functions
            // CSS-wide keywords are left unresolved on result: the function evaluates to the keyword
            // itself (e.g. result: inherit makes the dashed-function evaluate to the inherit keyword).
            // A declared (typed) return type cannot be a CSS-wide keyword, so it is invalid then.
            if (!customFunction->returnType.isUniversal())
                return false;
            tokens.append(CSSParserToken { IdentToken, nameLiteral(toValueID(keyword)) });
            return true;
        });
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

    CheckedPtr element = m_styleBuilder.state().element();
    if (!element)
        return false;

    // https://drafts.csswg.org/css-values-5/#typedef-attr-name
    // "As with attribute selectors, the case-sensitivity of <attr-name> depends on the document language."
    auto attributeName = shouldIgnoreAttributeCase(*element) ? parsedName->name.convertToASCIILowercase() : parsedName->name;

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
            // https://drafts.csswg.org/css-values-5/#typedef-attr-type
            // "For this purpose, <url> is invalid as a <syntax-single-component>."
            for (auto& component : syntax->definition) {
                if (component.type == CSSCustomPropertySyntax::Type::URL)
                    return { };
            }
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
            // <attr-unit> = <custom-ident>. Unknown units are accepted here; substitution triggers fallback.
            auto unit = CSSParserToken::stringToUnitType(value);
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

    protect(m_styleBuilder.state().style())->setHasAttrContent();

    if (!m_styleBuilder.state().element())
        return false;

    // https://drafts.csswg.org/css-values-5/#funcdef-attr
    // "If [attr()] is applied to a pseudo-element, the attribute is looked up on the pseudo-element's
    //  originating element." For rules cascading from outside the styled element's tree scope (::part(),
    //  document author rules matching UA-shadow pseudos like ::placeholder), the originating element is
    //  the rule's match target rather than the styled element.
    auto originatingElement = [&] -> Ref<const Element> {
        Ref styled = *m_styleBuilder.state().element();
        auto scopeOrdinal = m_styleBuilder.state().styleScopeOrdinal();
        if (scopeOrdinal <= ScopeOrdinal::ContainingHost) {
            if (RefPtr host = hostForScopeOrdinal(styled, scopeOrdinal))
                return host.releaseNonNull();
        }
        return styled;
    };
    Ref attributeElement = originatingElement();

    // Register the substitution dependency on the originating element's scope so attribute changes
    // trigger AttributeChangeInvalidation against the right rule features.
    m_styleBuilder.state().registerSubstitutionAttribute(attributeName, protect(Scope::forNode(attributeElement)).ptr());
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

    auto& attributeValue = attributeElement->getAttribute(QualifiedName { nullAtom(), attributeName, namespaceURI });

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
        // "If the <attr-unit> does not match a known CSS unit, it triggers fallback."
        if (attrType == AttrType::Unit && parsedAttrType->unitType == CSSUnitType::CSS_UNKNOWN)
            return substituteFailure();
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
            token.convertToDimensionWithUnit(parsedAttrType->unitType);
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

    auto selectedTokens = substituteTokenRange(unwrapArgumentBraces(selectedRange), context);
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
            if (token.value() == "-internal-first-valid"_s) {
                if (!substituteFirstValid(range.consumeBlock(), tokens, context))
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
        m_intermediateCustomProperties.clear();
        return nullptr;
    }

    auto data = CSSVariableData::create(*substitutedTokens, m_isAttrTainted ? IsAttrTainted::Yes : IsAttrTainted::No, context);
    m_intermediateTokenStrings.clear();
    m_intermediateCustomProperties.clear();
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
