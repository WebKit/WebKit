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

#include "CSSCalcTree+Evaluation.h"
#include "CSSCustomPropertySyntax.h"
#include "CSSCustomPropertyValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSRandomKeyParser.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSSelectorParser.h"
#include "CSSSerializationContext.h"
#include "CSSShorthandSubstitutionValue.h"
#include "CSSSubstitutionParser.h"
#include "CSSSubstitutionValue.h"
#include "CSSTokenizer.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include "CSSVariableData.h"
#include "CSSWideKeyword.h"
#include "ContainerQueryEvaluator.h"
#include "CustomFunctionRegistry.h"
#include "Document.h"
#include "Element.h"
#include "ElementInlines.h"
#include "HTMLSelectElement.h"
#include "IfConditionEvaluator.h"
#include "MatchResult.h"
#include "MutableStyleProperties.h"
#include "SelectPopoverElement.h"
#include "StyleBuilder.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleCustomProperty.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleDocumentScope.h"
#include "StyleEnvironmentVariables.h"
#include "StyleLocalPropertyRegistry.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/IndexedRange.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

// The maximum number of tokens that may be produced by a substitution function reference or fallback value.
// https://drafts.csswg.org/css-variables/#long-variables
static constexpr size_t maxSubstitutionTokens = 65536;

// ident() collapses its argument into one token, so maxSubstitutionTokens does not bound its length.
// https://drafts.csswg.org/css-values-5/#long-substitution
static constexpr size_t maxIdentFunctionLength = 1024;

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

void SubstitutionResolver::propagateAttrTaint(IsAttrTainted isAttrTainted, std::span<const CSSParserToken> tokens)
{
    if (isAttrTainted != IsAttrTainted::Yes)
        return;
    m_isAttrTainted = IsAttrTainted::Yes;
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
        return m_styleBuilder.state().document().styleScope().environmentVariables().valueForName(variableName);

    // Every parameter of the function whose arguments are being resolved shadows the calling element's
    // custom property of the same name, resolved or not, so a default can reference an earlier
    // parameter but never sees the calling element's value of a later one.
    if (!m_parameterValues.isEmpty() && functionId == CSSValueVar) {
        if (auto parameter = m_parameterValues.last().getOptional(variableName))
            return *parameter;
    }

    // Apply this variable first, in case it is still unresolved
    m_styleBuilder.applyCustomProperty(variableName);

    return protect(m_styleBuilder.state().style())->customPropertyValue(variableName);
}

auto SubstitutionResolver::substituteVarArgumentGrammar(CSSParserTokenRange range, const CSSParserContext& context) -> VarArgumentGrammarSubstitution
{
    // https://drafts.csswg.org/css-values-5/#argument-grammars
    // <var-args> = var( <declaration-value> , <declaration-value>? )
    // Splits at the first literal comma and substitutes the name argument, which is then parsed as a
    // <custom-property-name>. The name may itself come from other substitution functions, e.g.
    // var(var(--name)). A name that does not parse is left unset rather than failing the function, so
    // that the fallback still gets used.

    range.consumeWhitespace();

    auto nameArgStart = range;
    while (!range.atEnd() && range.peek().type() != CommaToken)
        range.consumeComponentValue();
    auto nameArgRange = unwrapArgumentBraces(nameArgStart.rangeUntil(range));

    std::optional<CSSParserTokenRange> fallbackRange;
    if (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range)) {
        range.trimTrailingWhitespace();
        fallbackRange = range;
    }

    auto parseName = [](CSSParserTokenRange nameRange) -> std::optional<AtomString> {
        nameRange.consumeWhitespace();
        auto& nameToken = nameRange.consumeIncludingWhitespace();
        if (!CSSSubstitutionParser::isValidCustomPropertyName(nameToken) || !nameRange.atEnd())
            return { };
        return nameToken.value().toAtomString();
    };

    // Fast path: a name argument that is already a literal <custom-property-name> needs no substitution.
    if (auto name = parseName(nameArgRange))
        return { name, fallbackRange };

    // https://drafts.csswg.org/css-values-5/#attr-security
    // Isolate the flag to see whether resolving the name itself involved attr()-tainted values. Diffing
    // it would miss the taint when something earlier in the same value had already set it.
    auto wasAttrTainted = std::exchange(m_isAttrTainted, IsAttrTainted::No);
    auto substitutedName = substituteTokenRange(nameArgRange, context);
    auto isNameAttrTainted = m_isAttrTainted;
    if (wasAttrTainted == IsAttrTainted::Yes)
        m_isAttrTainted = IsAttrTainted::Yes;

    if (!substitutedName)
        return { { }, fallbackRange, isNameAttrTainted };

    return { parseName(CSSParserTokenRange { *substitutedName }), fallbackRange, isNameAttrTainted };
}

bool SubstitutionResolver::substituteVarFunction(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-variables-2/#replace-a-var-function
    auto arguments = substituteVarArgumentGrammar(range, context);

    auto startIndex = tokens.size();
    if (!substituteNamedValueOrFallback(arguments.name, arguments.fallbackRange, CSSValueVar, tokens, context))
        return false;

    // https://drafts.csswg.org/css-values-5/#attr-security
    // A name argument resolved from attr()-tainted values taints the whole substitution value. The
    // URL to catch is in the substituted tokens, not in the name.
    propagateAttrTaint(arguments.isNameAttrTainted, std::span(tokens).subspan(startIndex));

    return true;
}

bool SubstitutionResolver::substituteEnvFunction(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-env-1/#env-function
    // FIXME: env()'s argument grammar is env( <declaration-value>, <declaration-value>? ) like var()'s,
    // so the name argument should be substituted and only then parsed as <custom-ident> <integer>*.
    // Only a literal <ident> name is supported here, the same limitation as the unsupported indices.
    range.consumeWhitespace();
    if (range.peek().type() != IdentToken)
        return false;
    auto name = range.consumeIncludingWhitespace().value().toAtomString();

    std::optional<CSSParserTokenRange> fallbackRange;
    if (!range.atEnd()) {
        if (range.peek().type() != CommaToken)
            return false;
        range.consumeIncludingWhitespace();
        range.trimTrailingWhitespace();
        fallbackRange = range;
    }

    return substituteNamedValueOrFallback(name, fallbackRange, CSSValueEnv, tokens, context);
}

bool SubstitutionResolver::substituteNamedValueOrFallback(const std::optional<AtomString>& name, const std::optional<CSSParserTokenRange>& fallbackRange, CSSValueID functionId, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    ASSERT(functionId == CSSValueVar || functionId == CSSValueEnv);

    // A name that failed to parse leaves the reference guaranteed-invalid, which still permits the fallback.
    RefPtr property = name ? propertyValueForVariableName(*name, functionId) : nullptr;

    if (property && !property->isGuaranteedInvalid()) {
        if (property->tokens().size() > maxSubstitutionTokens)
            return false;

        // https://drafts.csswg.org/css-values-5/#attr-security
        // Propagate attr()-taint through var() references.
        propagateAttrTaint(property->isAttrTainted(), property->tokens());

        tokens.appendVector(property->tokens());
        return true;
    }

    if (!fallbackRange)
        return false;

    auto fallbackTokens = substituteTokenRange(*fallbackRange, context);
    if (!fallbackTokens || fallbackTokens->size() > maxSubstitutionTokens)
        return false;

    if (functionId == CSSValueVar && name) {
        auto* registered = m_styleBuilder.state().registeredProperty(*name);
        // A custom function's parameters and locals are registered to carry their type, but they are
        // not author registrations, so a fallback for one is not held to its syntax.
        auto* localRegistry = m_styleBuilder.state().localPropertyRegistry();
        bool isLocalRegistration = localRegistry && localRegistry->get(*name);
        // https://drafts.css-houdini.org/css-properties-values-api/#fallbacks-in-var-references
        // A used fallback must match the referenced registered property's syntax.
        if (registered && !isLocalRegistration && !registered->syntax.isUniversal()
            && !CSSPropertyParser::isValidCustomPropertyValueForSyntax(registered->syntax, *fallbackTokens, context))
            return false;
    }

    tokens.appendVector(*fallbackTokens);
    return true;
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

// https://drafts.csswg.org/css-values-5/#funcdef-inherit
// inherit() = inherit( <custom-property-name> , <declaration-value>? )
// https://drafts.csswg.org/css-values-5/#replace-an-inherit-function
bool SubstitutionResolver::substituteInheritFunction(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // <inherit-args> is the same argument grammar as var()'s, including leaving an unparseable name
    // unset so that the fallback still gets used.
    auto arguments = substituteVarArgumentGrammar(range, context);

    auto inheritedValue = [&]() -> RefPtr<const CustomProperty> {
        if (!arguments.name)
            return nullptr;

        // Inside a custom function the parent is the calling context's element, whose custom
        // properties are resolved lazily, so force this one before reading it. Otherwise the result
        // would depend on the order the calling element's declarations happen to be applied in.
        // https://drafts.csswg.org/css-mixins/#evaluating-custom-functions
        if (auto* callingContextBuilder = m_styleBuilder.state().callingContextBuilder())
            callingContextBuilder->applyCustomProperty(*arguments.name);

        // The property read may be one that does not itself inherit, so a change to the parent's
        // non-inherited properties has to re-resolve this element. Set this even when the fallback
        // ends up being used, since the parent gaining the property later flips that choice.
        protect(m_styleBuilder.state().style())->setHasExplicitlyInheritedProperties();

        return protect(m_styleBuilder.state().parentStyle())->customPropertyValue(*arguments.name);
    }();

    auto startIndex = tokens.size();

    if (inheritedValue && !inheritedValue->isGuaranteedInvalid()) {
        if (inheritedValue->tokens().size() > maxSubstitutionTokens)
            return false;

        // https://drafts.csswg.org/css-values-5/#attr-security
        // Propagate attr()-taint through inherit() references.
        propagateAttrTaint(inheritedValue->isAttrTainted(), inheritedValue->tokens());

        tokens.appendVector(inheritedValue->tokens());
    } else if (arguments.fallbackRange) {
        auto fallbackTokens = substituteTokenRange(*arguments.fallbackRange, context);
        if (!fallbackTokens || fallbackTokens->size() > maxSubstitutionTokens)
            return false;

        tokens.appendVector(*fallbackTokens);
    } else
        return false;

    // https://drafts.csswg.org/css-values-5/#attr-security
    // A name argument resolved from attr()-tainted values taints the whole substitution value.
    propagateAttrTaint(arguments.isNameAttrTainted, std::span(tokens).subspan(startIndex));

    return true;
}

// https://drafts.csswg.org/css-mixins/#evaluate-a-custom-function
// Computes each parameter from its argument, falling back to the default when there is no argument or
// the argument does not match the parameter's type. Values are computed against the calling element.
// Registers each parameter with its declared type and computed value, and returns the properties to
// prepend to the body rule, or nullptr on failure.
RefPtr<MutableStyleProperties> SubstitutionResolver::resolveAndRegisterDashedFunctionArguments(const Vector<StyleRuleFunction::Parameter>& parameters, const Vector<Vector<CSSParserToken>>& arguments, LocalPropertyRegistry& registrations, ScopeOrdinal definitionScope)
{
    // A parameter without a default requires a corresponding argument. A missing one makes the whole
    // invocation guaranteed-invalid (unlike a supplied-but-invalid argument, which defaults below).
    for (auto [i, parameter] : indexedRange(parameters)) {
        if (!parameter.defaultValue && i >= arguments.size())
            return nullptr;
    }

    auto& context = m_substitutionValue->context();

    // A default may invoke another custom function, which resolves its own parameters, so these need a
    // frame of their own rather than a single set.
    m_parameterValues.append({ });
    auto popParameterValues = makeScopeExit([&] {
        m_parameterValues.removeLast();
    });
    // Seeded before any is resolved, so that a default referencing a parameter that is not resolved
    // yet gets the guaranteed-invalid value instead of the calling element's property of that name.
    for (auto& parameter : parameters)
        m_parameterValues.last().set(parameter.name, nullptr);

    SetForScope scopedLookupScope(m_dashedFunctionLookupScope, definitionScope);

    auto resolvedArgumentProperties = MutableStyleProperties::create();

    for (auto [i, parameter] : indexedRange(parameters)) {
        bool hasArgument = i < arguments.size() && !arguments[i].isEmpty();

        // Computes a candidate against the parameter's type, or returns null if it does not match.
        auto computeCandidate = [&](std::span<const CSSParserToken> candidateTokens) -> RefPtr<const CustomProperty> {
            if (candidateTokens.empty())
                return nullptr;

            auto data = CSSVariableData::create(CSSParserTokenRange { candidateTokens }, m_isAttrTainted, context);

            if (parameter.type.isUniversal())
                return CustomProperty::createForVariableData(parameter.name, WTF::move(data));

            // https://drafts.csswg.org/css-values-5/#first-valid
            if (!CSSPropertyParser::isValidCustomPropertyValueForSyntax(parameter.type, data->tokenRange(), context))
                return nullptr;

            auto computed = m_styleBuilder.computeCustomPropertyValueForSyntax(parameter.name, parameter.type, data);
            if (!computed)
                return nullptr;

            return WTF::switchOn(*computed,
                [](const Ref<const CustomProperty>& property) -> RefPtr<const CustomProperty> {
                    return property.ptr();
                },
                // A declared type cannot be a CSS-wide keyword.
                [](CSSWideKeyword) -> RefPtr<const CustomProperty> {
                    return nullptr;
                });
        };

        auto resolvedValue = [&] -> RefPtr<const CustomProperty> {
            // A bare CSS-wide keyword keeps its keyword semantics rather than becoming a literal value.
            // https://drafts.csswg.org/css-mixins/#evaluating-custom-functions
            // An argument that failed to substitute is empty, and may have no default to fall back to.
            auto primaryTokens = [&] -> CSSParserTokenRange {
                if (hasArgument)
                    return CSSParserTokenRange { arguments[i].span() };
                if (parameter.defaultValue)
                    return parameter.defaultValue->tokenRange();
                return { };
            }();
            primaryTokens.consumeWhitespace();
            if (auto keyword = CSSPropertyParserHelpers::consumeCSSWideKeyword(primaryTokens); keyword && primaryTokens.atEnd()) {
                // `initial` is the parameter's registered initial value, which does not exist yet at this
                // point, and any keyword other than `inherit` is guaranteed-invalid.
                if (*keyword != CSSWideKeyword::Inherit)
                    return nullptr;
                // `inherit` resolves like inherit() with the parameter name, reinterpreted with the
                // parameter's type.
                RefPtr inherited = propertyValueForVariableName(parameter.name, CSSValueInherit);
                if (!inherited || inherited->isGuaranteedInvalid())
                    return nullptr;
                return computeCandidate(inherited->tokens());
            }

            // first-valid(arg value, default value): the default is used when there is no argument, or
            // when the argument does not match the parameter's type.
            // The argument is already substituted; a default is not.
            if (hasArgument) {
                if (RefPtr value = computeCandidate(arguments[i].span()))
                    return value;
            }
            if (parameter.defaultValue) {
                if (auto substituted = substituteTokenRange(parameter.defaultValue->tokenRange(), context))
                    return computeCandidate(substituted->span());
            }
            return nullptr;
        }();

        m_parameterValues.last().set(parameter.name, resolvedValue);

        // A typed parameter keeps its type and acts as a local registration, so a body declaration
        // assigning to its name is computed against it too.
        // https://github.com/w3c/csswg-drafts/issues/12315
        registrations.add({
            .name = AtomString { parameter.name },
            .syntax = parameter.type,
            .inherits = true,
            .initialValue = resolvedValue,
        });

        if (resolvedValue && !resolvedValue->isGuaranteedInvalid()) {
            auto tokenData = CSSVariableData::create(CSSParserTokenRange { resolvedValue->tokens() }, resolvedValue->isAttrTainted(), context);
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

    // A <dashed-function> in a parameter default resolves in the scope the enclosing function was
    // defined in, not the calling element's scope.
    auto scopeOrdinal = m_dashedFunctionLookupScope.value_or(m_styleBuilder.state().styleScopeOrdinal());
    auto scopedFunctionName = ScopedName { functionName.toAtomString(), scopeOrdinal };

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

    auto& parameters = customFunction->parameters;

    // Arguments are substituted before the context is guarded, so an argument that calls the same
    // function is not a cycle. https://drafts.csswg.org/css-mixins/#replace-a-dashed-function
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

    // Parameter default values and the body are resolved inside the guard, so a function that reaches
    // itself through either is still cyclic.
    auto guard = m_styleBuilder.state().guardSubstitutionContext({ SubstitutionContext::Type::Function, scopedFunctionName.name, foundScopeOrdinal });

    if (guard.isCyclicContext())
        return false;

    // "Let registrations be an initially empty set of custom property registrations."
    auto registrations = LocalPropertyRegistry { m_styleBuilder.state().localPropertyRegistry() };

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
    // Merge the body's declaration blocks now, dropping blocks whose @container conditions do not
    // match the calling element. https://drafts.csswg.org/css-mixins/#evaluating-custom-functions
    auto bodyProperties = [&]() -> Ref<const StyleProperties> {
        auto& blocks = customFunction->declarationBlocks;

        // The common case is a single block with no container queries.
        if (blocks.size() == 1 && blocks.first().containerQueries.isEmpty())
            return blocks.first().properties;

        // A pseudo-element's queries select containers from its originating element inclusive.
        auto selectionMode = m_styleBuilder.state().style().pseudoElementIdentifier()
            ? ContainerQueryEvaluator::SelectionMode::PseudoElement
            : ContainerQueryEvaluator::SelectionMode::Element;
        ContainerQueryEvaluator evaluator(*element, selectionMode, foundScopeOrdinal, nullptr);
        auto containerQueriesMatch = [&](const auto& chain) {
            for (auto& containerRule : chain) {
                if (!evaluator.evaluate(containerRule->containerQuery()))
                    return false;
            }
            return true;
        };

        auto mutableProperties = MutableStyleProperties::create();
        for (auto& block : blocks) {
            // A container query in the body makes the result depend on the calling element's
            // container, not just the matched declarations, so it must not be cached.
            if (!block.containerQueries.isEmpty())
                m_styleBuilder.state().setIsContainerDependent();
            if (containerQueriesMatch(block.containerQueries))
                mutableProperties->mergeAndOverrideOnConflict(block.properties.get());
        }
        return mutableProperties;
    }();

    // A local is not the document-registered property of the same name, so registering it as universal
    // keeps it untyped. A parameter keeps its own registration, whose initial value is the argument.
    for (auto property : bodyProperties.get()) {
        if (property.id() != CSSPropertyCustom)
            continue;
        auto& name = downcast<CSSCustomPropertyValue>(*property.value()).name();
        if (registrations.get(name))
            continue;
        registrations.add({
            .name = name,
            .syntax = CSSCustomPropertySyntax::universal(),
            .inherits = true,
        });
    }

    auto bodyMatchResult = MatchResult::create();
    bodyMatchResult->authorDeclarations.append({ *resolvedArgumentProperties });
    bodyMatchResult->authorDeclarations.append({ .properties = bodyProperties, .styleScopeOrdinal = foundScopeOrdinal });

    // "Resolve function styles using custom function, body rule, registrations, and calling context."
    // The hypothetical element acts as a child of the calling element, inheriting its computed custom
    // properties on demand via the calling context's builder (https://drafts.csswg.org/css-mixins/#evaluating-custom-functions).
    auto builderContext = BuilderContext {
        .document = m_styleBuilder.state().document(),
        .parentStyle = &m_styleBuilder.state().style(),
        .element = m_styleBuilder.state().element(),
        .localPropertyRegistry = &registrations,
        .callingContextBuilder = &m_styleBuilder
    };

    auto bodyStyles = Style::ComputedStyle::createPtr();
    // Font-relative units in the body resolve against the hypothetical element's own font, so it has to
    // inherit one. Custom properties are resolved lazily through the calling context's builder instead.
    bodyStyles->inheritIgnoringCustomPropertiesFrom(protect(m_styleBuilder.state().style()));
    Builder bodyBuilder(*bodyStyles, WTF::move(builderContext), bodyMatchResult);
    bodyBuilder.state().addGuardedFunctionContexts(m_styleBuilder.state());

    // "Return the value of the result property in body styles."
    auto resolvedResult = bodyBuilder.resolveFunctionResult();
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
        CSSUnitType unitType { CSSUnitType::Unknown };
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
        auto trimmedValue = attributeValue.string().trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
        CSSTokenizer tokenizer(trimmedValue);
        auto tokenRange = tokenizer.tokenRange();
        tokenRange.consumeWhitespace();
        if (tokenRange.peek().type() != NumberToken)
            return substituteFailure();
        auto numberToken = tokenRange.consumeIncludingWhitespace();
        if (!tokenRange.atEnd())
            return substituteFailure();
        m_intermediateTokenStrings.append(WTF::move(trimmedValue));
        m_intermediateTokenStrings.appendVector(tokenizer.escapedStringsForAdoption());
        tokens.append(CSSParserToken(numberToken.numericValue(), numberToken.numericValueType(), numberToken.numericSign(), numberToken.value()));
        return true;
    }

    // "If given as an <attr-unit> value, the value is first parsed as if number keyword was specified,
    //  then the resulting numeric value is turned into a dimension with the corresponding unit,
    //  or a percentage if % was given."
    case AttrType::Unit:
    case AttrType::Percentage: {
        // "If the <attr-unit> does not match a known CSS unit, it triggers fallback."
        if (attrType == AttrType::Unit && parsedAttrType->unitType == CSSUnitType::Unknown)
            return substituteFailure();
        auto trimmedValue = attributeValue.string().trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
        CSSTokenizer tokenizer(trimmedValue);
        auto tokenRange = tokenizer.tokenRange();
        tokenRange.consumeWhitespace();
        if (tokenRange.peek().type() != NumberToken)
            return substituteFailure();
        auto numberToken = tokenRange.consumeIncludingWhitespace();
        if (!tokenRange.atEnd())
            return substituteFailure();
        auto token = CSSParserToken(numberToken.numericValue(), numberToken.numericValueType(), numberToken.numericSign(), numberToken.value());
        if (attrType == AttrType::Percentage)
            token.convertToPercentage();
        else
            token.convertToDimensionWithUnit(parsedAttrType->unitType);
        m_intermediateTokenStrings.append(WTF::move(trimmedValue));
        m_intermediateTokenStrings.appendVector(tokenizer.escapedStringsForAdoption());
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

auto SubstitutionResolver::substituteRandomItemArgumentGrammar(CSSParserTokenRange range, const CSSParserContext& context) -> std::optional<RandomItemArgumentGrammarSubstitution>
{
    // https://drafts.csswg.org/css-values-5/#argument-grammars
    // <random-item-args> = random-item( <declaration-value>, [ <declaration-value>? ]# )
    // Splits at the literal commas and substitutes the first argument (the <random-key>).

    range.consumeWhitespace();

    // Split at the first literal comma: the first argument is the <random-key>, the rest are items.
    auto randomKeyStart = range;
    while (!range.atEnd() && range.peek().type() != CommaToken)
        range.consumeComponentValue();
    auto randomKeyRange = randomKeyStart.rangeUntil(range);

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range))
        return { };

    // Collect the item ranges. Each item is a (possibly empty) <declaration-value>; a {}-wrapped
    // block groups internal commas and is unwrapped when the selected item is substituted.
    Vector<CSSParserTokenRange> items;
    do {
        auto itemStart = range;
        while (!range.atEnd() && range.peek().type() != CommaToken)
            range.consumeComponentValue();
        items.append(itemStart.rangeUntil(range));
    } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));

    if (items.isEmpty())
        return { };

    // <random-key> may itself contain arbitrary substitution functions (var(), attr(), ...);
    // substitute them before parsing it as a <random-key>. Items are substituted lazily, once
    // the selected one is known.
    auto substitutedRandomKey = substituteTokenRange(randomKeyRange, context);
    if (!substitutedRandomKey)
        return { };

    return RandomItemArgumentGrammarSubstitution { WTF::move(*substitutedRandomKey), WTF::move(items) };
}

bool SubstitutionResolver::substituteIdentFunction(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-values-5/#ident
    // <ident-args> = ident( <declaration-value> )
    // ident() = ident( <ident-arg>+ ), <ident-arg> = <string> | <integer> | <ident>
    // The argument is substituted first, then parsed as <ident-arg>+. The parts are concatenated with
    // no separator, so ident("--" attr(id)) makes a <dashed-ident> out of an attribute value.

    auto substitutedArgument = substituteTokenRange(unwrapArgumentBraces(range), context);
    if (!substitutedArgument)
        return false;

    // https://drafts.csswg.org/css-variables/#long-variables
    if (substitutedArgument->size() > maxSubstitutionTokens)
        return false;

    StringBuilder builder;
    auto parserState = CSS::PropertyParserState { .context = context, .currentProperty = m_styleBuilder.state().cssPropertyID() };

    auto argumentRange = CSSParserTokenRange { *substitutedArgument };
    argumentRange.consumeWhitespace();
    while (!argumentRange.atEnd()) {
        auto tokenType = argumentRange.peek().type();
        if (tokenType == IdentToken || tokenType == StringToken) {
            auto part = argumentRange.consumeIncludingWhitespace().value();
            if (builder.length() + part.length() > maxIdentFunctionLength)
                return false;
            builder.append(part);
            continue;
        }

        // <integer> covers math functions too, so ident("--prop" calc(1 + 2)) names --prop3.
        auto integer = CSSPropertyParserHelpers::MetaConsumer<CSS::Integer<>>::consume(argumentRange, parserState);
        if (!integer)
            return false;
        builder.append(toStyle(*integer, m_styleBuilder.state()).value);
        if (builder.length() > maxIdentFunctionLength)
            return false;
    }

    // <ident-arg>+ is one or more arguments, and an empty identifier is not one.
    if (builder.isEmpty())
        return false;

    m_intermediateTokenStrings.append(builder.toString());
    tokens.append(CSSParserToken(IdentToken, StringView { m_intermediateTokenStrings.last() }));
    return true;
}

bool SubstitutionResolver::substituteRandomItemFunction(CSSParserTokenRange range, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-values-5/#funcdef-random-item

    // Reject random-item() when resolving a @container style() query value: random functions are
    // disallowed outside an element context. (random() is rejected via a separate parse-time guard.)
    // Whether they should be allowed here is an open question: https://github.com/w3c/csswg-drafts/issues/10982
    if (m_styleBuilder.state().isResolvingContainerQueries())
        return false;

    // <random-item-args> = random-item( <declaration-value>, [ <declaration-value>? ]# )
    auto randomItemArgs = substituteRandomItemArgumentGrammar(range, context);
    if (!randomItemArgs)
        return false;

    // random-item() = random-item( <random-key> , [ <declaration-value>? ]# )
    auto baseValue = randomItemBaseValue(WTF::move(randomItemArgs->randomKey));
    if (!baseValue)
        return false;

    auto& items = randomItemArgs->items;

    // https://drafts.csswg.org/css-values-5/#random-item
    // "Let index be a random integer less than N (the number of items), given the base value R:
    //  round(down, R * N, 1)." fixed accepts the closed range [0, 1], so R can be exactly 1,
    // which makes R * N == N; the clamp below is required to keep the index in range.
    auto index = static_cast<size_t>(*baseValue * items.size());
    if (index >= items.size())
        index = items.size() - 1;

    auto selectedTokens = substituteTokenRange(unwrapArgumentBraces(items[index]), context);
    if (!selectedTokens)
        return false;

    // https://drafts.csswg.org/css-variables/#long-variables
    if (selectedTokens->size() > maxSubstitutionTokens)
        return false;

    tokens.appendVector(*selectedTokens);
    return true;
}

std::optional<double> SubstitutionResolver::randomItemBaseValue(Vector<CSSParserToken> randomKey)
{
    // <random-key> = auto | <random-cache-key> | fixed <number [0,1]>
    // Parsed with the shared consumer so random-item()'s <random-key> stays in sync with random()'s
    // (<dashed-ident>, element-scoped, property-scoped, property-index-scoped, fixed). <random-ua-ident>
    // is a follow-up.
    CSSParserTokenRange randomKeyRange { randomKey };
    randomKeyRange.consumeWhitespace();

    auto parserState = CSS::PropertyParserState { .context = m_substitutionValue->context() };

    // This index is counted in its own space, separate from random()'s parse-time
    // cssRandomFunctionCount, which is why the key records which function it came from.
    auto keySource = CSSPropertyParserHelpers::RandomKeySource {
        .property = { m_styleBuilder.state().cssPropertyID(), m_styleBuilder.state().customPropertyName(), CSSCalc::RandomFunction::RandomItem },
        .autoElementScoped = CSS::Keyword::ElementScoped { }
    };
    auto sharing = CSSPropertyParserHelpers::consumeUnresolvedRandomKey(randomKeyRange, parserState, keySource, [&] {
        return m_randomItemAutoIndex++;
    });
    if (!sharing || !randomKeyRange.atEnd())
        return { };

    return CSSCalc::resolveRandomBaseValue(*sharing, m_styleBuilder.state());
}

auto SubstitutionResolver::substituteIfArgumentGrammar(CSSParserTokenRange range, const CSSParserContext& context) -> std::optional<Vector<IfBranch>>
{
    // https://drafts.csswg.org/css-values-5/#argument-grammars
    // <if-args> = if( [ <if-args-branch>; ]* <if-args-branch> ;? )
    // <if-args-branch> = <declaration-value> : <declaration-value>?
    // The condition excludes top-level colons, so the first top-level colon separates it from the
    // value and top-level semicolons separate branches. A single pass consumes both in source order.

    range.consumeWhitespace();

    Vector<IfBranch> branches;
    while (!range.atEnd()) {
        auto conditionStart = range;
        while (!range.atEnd() && range.peek().type() != ColonToken && range.peek().type() != SemicolonToken)
            range.consumeComponentValue();
        auto conditionRange = conditionStart.rangeUntil(range);

        // A branch without a colon is invalid, but an empty segment (from a doubled or trailing
        // semicolon) is skipped.
        if (range.atEnd() || range.peek().type() == SemicolonToken) {
            conditionRange.consumeWhitespace();
            if (!conditionRange.atEnd())
                return { };
            if (!range.atEnd())
                range.consumeIncludingWhitespace(); // semicolon
            continue;
        }

        range.consume(); // colon

        auto valueStart = range;
        while (!range.atEnd() && range.peek().type() != SemicolonToken)
            range.consumeComponentValue();
        auto valueRange = valueStart.rangeUntil(range);
        valueRange.consumeWhitespace();

        if (!range.atEnd())
            range.consumeIncludingWhitespace(); // semicolon

        auto substitutedCondition = substituteTokenRange(conditionRange, context);
        if (!substitutedCondition)
            continue;

        // <if-condition> = <boolean-expr[ <if-test> ]> | else. The else keyword is recognized after
        // substitution (it may come from a var()), and always matches, so it is stored as a null
        // condition. Other conditions are evaluated later.
        auto substitutedRange = CSSParserTokenRange { *substitutedCondition };
        substitutedRange.consumeWhitespace();
        if (CSSPropertyParserHelpers::consumeIdentRaw<CSSValueElse>(substitutedRange) && substitutedRange.atEnd()) {
            branches.append({ std::nullopt, valueRange });
            continue;
        }

        branches.append({ WTF::move(*substitutedCondition), valueRange });
    }

    return branches;
}

bool SubstitutionResolver::substituteIfFunction(CSSParserTokenRange argumentsRange, Vector<CSSParserToken>& tokens, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-values-5/#funcdef-if
    auto branches = substituteIfArgumentGrammar(argumentsRange, context);
    if (!branches)
        return false;

    for (auto& branch : *branches) {
        IfConditionEvaluator conditionEvaluator { m_styleBuilder, context };
        auto conditionResult = branch.condition
            ? conditionEvaluator.evaluate(*branch.condition)
            : IfConditionEvaluator::Result::True;

        // An invalid condition makes the whole if() IACVT. This matches the imported WPT but not the
        // spec algorithm, which continues to the next branch on a condition parse failure.
        // FIXME: Revisit once the spec/WPT discrepancy is resolved upstream.
        if (conditionResult == IfConditionEvaluator::Result::Invalid)
            return false;

        if (conditionResult != IfConditionEvaluator::Result::True)
            continue;

        // Substitute the value part and return.
        auto startIndex = tokens.size();
        auto substitutedValue = substituteTokenRange(branch.valueRange, context);
        if (!substitutedValue)
            return false;

        tokens.appendVector(*substitutedValue);

        // A condition that read an attr()-tainted property taints the whole substitution value.
        if (conditionEvaluator.referencedAttrTaintedValue())
            propagateAttrTaint(IsAttrTainted::Yes, std::span(tokens).subspan(startIndex));

        return true;
    }

    // No condition matched. Empty token stream.
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
            if (functionId == CSSValueVar) {
                if (!substituteVarFunction(range.consumeBlock(), tokens, context))
                    success = false;
                continue;
            }
            if (functionId == CSSValueEnv) {
                if (!substituteEnvFunction(range.consumeBlock(), tokens, context))
                    success = false;
                continue;
            }
            if (functionId == CSSValueInherit && context.cssInheritFunctionEnabled) {
                if (!substituteInheritFunction(range.consumeBlock(), tokens, context))
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
            if (functionId == CSSValueIf) {
                if (!substituteIfFunction(range.consumeBlock(), tokens, context))
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
            if (functionId == CSSValueRandomItem) {
                if (!substituteRandomItemFunction(range.consumeBlock(), tokens, context))
                    success = false;
                continue;
            }
            if (functionId == CSSValueIdent) {
                if (!substituteIdentFunction(range.consumeBlock(), tokens, context))
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
    m_isAttrTainted = IsAttrTainted::No;
    m_hasTaintedURL = false;
    m_randomItemAutoIndex = 0;
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

    auto data = CSSVariableData::create(*substitutedTokens, m_isAttrTainted, context);
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
