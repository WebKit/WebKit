/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Parser.h"

#include "ASTBuilder.h"
#include "BuiltinNames.h"
#include "DebuggerParseData.h"
#include "JSCJSValueInlines.h"
#include "VM.h"
#include <utility>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/StringPrintStream.h>

#define updateErrorMessage(shouldPrintToken, ...) do {\
    propagateError(); \
    logError(shouldPrintToken, __VA_ARGS__); \
} while (0)

#define propagateError() do { if (UNLIKELY(hasError())) return 0; } while (0)
#define internalFailWithMessage(shouldPrintToken, ...) do { updateErrorMessage(shouldPrintToken, __VA_ARGS__); return 0; } while (0)
#define handleErrorToken() do { if (m_token.m_type == EOFTOK || m_token.m_type & CanBeErrorTokenFlag) { failDueToUnexpectedToken(); } } while (0)
#define failWithMessage(...) do { { handleErrorToken(); updateErrorMessage(true, __VA_ARGS__); } return 0; } while (0)
#define failWithStackOverflow() do { updateErrorMessage(false, "Stack exhausted"); m_hasStackOverflow = true; return 0; } while (0)
#define failIfFalse(cond, ...) do { if (!(cond)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define failIfTrue(cond, ...) do { if (cond) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define failIfTrueIfStrict(cond, ...) do { if ((cond) && strictMode()) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define failIfFalseIfStrict(cond, ...) do { if ((!(cond)) && strictMode()) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define consumeOrFail(tokenType, ...) do { if (!consume(tokenType)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define consumeOrFailWithFlags(tokenType, flags, ...) do { if (!consume(tokenType, flags)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define matchOrFail(tokenType, ...) do { if (!match(tokenType)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define failIfStackOverflow() do { if (UNLIKELY(!canRecurse())) failWithStackOverflow(); } while (0)
#define semanticFail(...) do { internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define semanticFailIfTrue(cond, ...) do { if (UNLIKELY(cond)) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define semanticFailIfFalse(cond, ...) do { if (UNLIKELY(!(cond))) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define regexFail(failure) do { setErrorMessage(failure); return 0; } while (0)
#define failDueToUnexpectedToken() do {\
        logError(true);\
    return 0;\
} while (0)

#define handleProductionOrFail(token, tokenString, operation, production) do {\
    consumeOrFail(token, "Expected '", tokenString, "' to ", operation, " a ", production);\
} while (0)

#define handleProductionOrFail2(token, tokenString, operation, production) do {\
    consumeOrFail(token, "Expected '", tokenString, "' to ", operation, " an ", production);\
} while (0)

#define semanticFailureDueToKeywordCheckingToken(token, ...) do { \
    if (strictMode() && token.m_type == RESERVED_IF_STRICT) \
        semanticFail("Cannot use the reserved word '", getToken(token), "' as a ", __VA_ARGS__, " in strict mode"); \
    if (token.m_type == RESERVED || token.m_type == RESERVED_IF_STRICT) \
        semanticFail("Cannot use the reserved word '", getToken(token), "' as a ", __VA_ARGS__); \
    if (token.m_type & KeywordTokenFlag) { \
        if (!isContextualKeyword(token)) \
            semanticFail("Cannot use the keyword '", getToken(token), "' as a ", __VA_ARGS__); \
        if (token.m_type == LET && strictMode())\
            semanticFail("Cannot use 'let' as a ", __VA_ARGS__, " ", disallowedIdentifierLetReason()); \
        if (token.m_type == AWAIT && !canUseIdentifierAwait()) \
            semanticFail("Cannot use 'await' as a ", __VA_ARGS__, " ", disallowedIdentifierAwaitReason()); \
        if (token.m_type == YIELD && !canUseIdentifierYield()) \
            semanticFail("Cannot use 'yield' as a ", __VA_ARGS__, " ", disallowedIdentifierYieldReason()); \
    } \
} while (0)

#define semanticFailureDueToKeyword(...) semanticFailureDueToKeywordCheckingToken(m_token, __VA_ARGS__);

namespace JSC {

std::atomic<unsigned> globalParseCount { 0 };

ALWAYS_INLINE static SourceParseMode getAsynFunctionBodyParseMode(SourceParseMode parseMode)
{
    if (isAsyncGeneratorWrapperParseMode(parseMode))
        return SourceParseMode::AsyncGeneratorBodyMode;

    if (parseMode == SourceParseMode::AsyncArrowFunctionMode)
        return SourceParseMode::AsyncArrowFunctionBodyMode;

    return SourceParseMode::AsyncFunctionBodyMode;
}

template <typename LexerType>
void Parser<LexerType>::logError(bool)
{
    if (hasError())
        return;
    StringPrintStream stream;
    printUnexpectedTokenText(stream);
    setErrorMessage(stream.toStringWithLatin1Fallback());
}

template <typename LexerType> template <typename... Args>
void Parser<LexerType>::logError(bool shouldPrintToken, Args&&... args)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(std::forward<Args>(args)..., ".");
    setErrorMessage(stream.toStringWithLatin1Fallback());
}

template <typename LexerType>
Parser<LexerType>::Parser(VM& vm, const SourceCode& source, JSParserBuiltinMode builtinMode, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, SourceParseMode parseMode, SuperBinding superBinding, ConstructorKind defaultConstructorKindForTopLevelFunction, DerivedContextType derivedContextType, bool isEvalContext, EvalContextType evalContextType, DebuggerParseData* debuggerParseData, bool isInsideOrdinaryFunction)
    : m_vm(vm)
    , m_source(&source)
    , m_hasStackOverflow(false)
    , m_allowsIn(true)
    , m_statementDepth(0)
    , m_parsingBuiltin(builtinMode == JSParserBuiltinMode::Builtin)
    , m_parseMode(parseMode)
    , m_scriptMode(scriptMode)
    , m_superBinding(superBinding)
    , m_defaultConstructorKindForTopLevelFunction(defaultConstructorKindForTopLevelFunction)
    , m_immediateParentAllowsFunctionDeclarationInStatement(false)
    , m_debuggerParseData(debuggerParseData)
    , m_isInsideOrdinaryFunction(isInsideOrdinaryFunction)
{
    m_lexer = makeUnique<LexerType>(vm, builtinMode, scriptMode);
    m_lexer->setCode(source, &m_parserArena);
    m_token.m_location.line = source.firstLine().oneBasedInt();
    m_token.m_location.startOffset = source.startOffset();
    m_token.m_location.endOffset = source.startOffset();
    m_token.m_location.lineStartOffset = source.startOffset();
    m_functionCache = vm.addSourceProviderCache(source.provider());
    m_expressionErrorClassifier = nullptr;

    ScopeRef scope = pushScope();
    scope->setSourceParseMode(parseMode);
    scope->setIsEvalContext(isEvalContext);
    if (isEvalContext)
        scope->setEvalContextType(evalContextType);
    
    if (derivedContextType == DerivedContextType::DerivedConstructorContext) {
        scope->setConstructorKind(ConstructorKind::Extends);
        scope->setExpectedSuperBinding(SuperBinding::Needed);
    }
    
    if (derivedContextType == DerivedContextType::DerivedMethodContext)
        scope->setExpectedSuperBinding(SuperBinding::Needed);

    if (strictMode == JSParserStrictMode::Strict)
        scope->setStrictMode();

    if (isModuleParseMode(parseMode))
        m_moduleScopeData = ModuleScopeData::create();

    if (isProgramOrModuleParseMode(parseMode))
        scope->setIsGlobalCodeScope();

    next();
}

class Scope::MaybeParseAsGeneratorForScope {
public:
    MaybeParseAsGeneratorForScope(ScopeRef& scope, bool shouldParseAsGenerator)
        : m_scope(scope)
        , m_oldValue(scope->m_isGenerator)
    {
        m_scope->m_isGenerator = shouldParseAsGenerator;
    }

    ~MaybeParseAsGeneratorForScope()
    {
        m_scope->m_isGenerator = m_oldValue;
    }

private:
    ScopeRef m_scope;
    bool m_oldValue;
};

struct DepthManager : private SetForScope<int> {
public:
    DepthManager(int* depth)
        : SetForScope<int>(*depth, *depth)
    {
    }
};

template <typename LexerType>
Parser<LexerType>::~Parser()
{
}

void JSToken::dump(PrintStream& out) const
{
    out.print(*m_data.cooked);
}

template <typename LexerType>
Expected<typename Parser<LexerType>::ParseInnerResult, String> Parser<LexerType>::parseInner(const Identifier& calleeName, ParsingContext parsingContext, Optional<int> functionConstructorParametersEndPosition, const Vector<JSTextPosition>* classFieldLocations)
{
    ASTBuilder context(const_cast<VM&>(m_vm), m_parserArena, const_cast<SourceCode*>(m_source));
    SourceParseMode parseMode = sourceParseMode();
    ScopeRef scope = currentScope();
    scope->setIsLexicalScope();
    SetForScope<FunctionParsePhase> functionParsePhasePoisoner(m_parserState.functionParsePhase, FunctionParsePhase::Body);

    FunctionParameters* parameters = nullptr;
    bool isArrowFunctionBodyExpression = parseMode == SourceParseMode::AsyncArrowFunctionBodyMode && !match(OPENBRACE);
    if (m_lexer->isReparsingFunction()) {
        ParserFunctionInfo<ASTBuilder> functionInfo;
        if (isGeneratorOrAsyncFunctionBodyParseMode(parseMode))
            parameters = createGeneratorParameters(context, functionInfo.parameterCount);
        else if (parseMode == SourceParseMode::ClassFieldInitializerMode)
            parameters = context.createFormalParameterList();
        else
            parameters = parseFunctionParameters(context, functionInfo);

        if (SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(parseMode) && !hasError()) {
            // FIXME:
            // Logically, this should be an assert, since we already successfully parsed the arrow
            // function when syntax checking. So logically, we should see the arrow token here.
            // But we're seeing crashes in the wild when making this an assert. Instead, we'll just
            // handle it as an error in release builds, and an assert on debug builds, with the hopes
            // of fixing it in the future.
            // https://bugs.webkit.org/show_bug.cgi?id=221633
            if (UNLIKELY(!match(ARROWFUNCTION))) {
                ASSERT_NOT_REACHED();
                return makeUnexpected("Parser error"_s);
            }
            next();
            isArrowFunctionBodyExpression = !match(OPENBRACE);
        }
    }

    if (!calleeName.isNull())
        scope->declareCallee(&calleeName);

    if (m_lexer->isReparsingFunction())
        m_statementDepth--;

    SourceElements* sourceElements = nullptr;
    // The only way we can error this early is if we reparse a function and we run out of stack space.
    if (!hasError()) {
        if (isAsyncFunctionWrapperParseMode(parseMode))
            sourceElements = parseAsyncFunctionSourceElements(context, isArrowFunctionBodyExpression, CheckForStrictMode);
        else if (isArrowFunctionBodyExpression)
            sourceElements = parseArrowFunctionSingleExpressionBodySourceElements(context);
        else if (isModuleParseMode(parseMode))
            sourceElements = parseModuleSourceElements(context);
        else if (isGeneratorWrapperParseMode(parseMode))
            sourceElements = parseGeneratorFunctionSourceElements(context, calleeName, CheckForStrictMode);
        else if (isAsyncGeneratorWrapperParseMode(parseMode))
            sourceElements = parseAsyncGeneratorFunctionSourceElements(context, isArrowFunctionBodyExpression, CheckForStrictMode);
        else if (parsingContext == ParsingContext::FunctionConstructor)
            sourceElements = parseSingleFunction(context, functionConstructorParametersEndPosition);
        else if (parseMode == SourceParseMode::ClassFieldInitializerMode) {
            ASSERT(classFieldLocations && !classFieldLocations->isEmpty());
            sourceElements = parseClassFieldInitializerSourceElements(context, *classFieldLocations);
        } else
            sourceElements = parseSourceElements(context, CheckForStrictMode);
    }

    bool validEnding = consume(EOFTOK);
    if (!sourceElements || !validEnding)
        return makeUnexpected(hasError() ? m_errorMessage : "Parser error"_s);

    IdentifierSet capturedVariables;
    UniquedStringImplPtrSet sloppyModeHoistedFunctions;
    scope->getSloppyModeHoistedFunctions(sloppyModeHoistedFunctions);
    scope->getCapturedVars(capturedVariables);

    VariableEnvironment& varDeclarations = scope->declaredVariables();
    for (auto& entry : capturedVariables)
        varDeclarations.markVariableAsCaptured(entry);

    if (isGeneratorWrapperParseMode(parseMode) || isAsyncFunctionOrAsyncGeneratorWrapperParseMode(parseMode)) {
        if (scope->usedVariablesContains(m_vm.propertyNames->arguments.impl()))
            context.propagateArgumentsUse();
    }

    CodeFeatures features = context.features();
    if (scope->strictMode())
        features |= StrictModeFeature;
    if (scope->shadowsArguments())
        features |= ShadowsArgumentsFeature;
    if (m_seenTaggedTemplate)
        features |= NoEvalCacheFeature;
    if (scope->hasNonSimpleParameterList())
        features |= NonSimpleParameterListFeature;

#if ASSERT_ENABLED
    if (m_parsingBuiltin && isProgramParseMode(parseMode)) {
        VariableEnvironment& lexicalVariables = scope->lexicalVariables();
        const HashSet<UniquedStringImpl*>& closedVariableCandidates = scope->closedVariableCandidates();
        for (UniquedStringImpl* candidate : closedVariableCandidates) {
            // FIXME: We allow async to leak because it appearing as a closed variable is a side effect of trying to parse async arrow functions.
            if (!lexicalVariables.contains(candidate) && !varDeclarations.contains(candidate) && !candidate->isSymbol() && candidate != m_vm.propertyNames->async.impl()) {
                dataLog("Bad global capture in builtin: '", candidate, "'\n");
                dataLog(m_source->view());
                CRASH();
            }
        }
    }
#endif // ASSERT_ENABLED

    return ParseInnerResult { parameters, sourceElements, scope->takeFunctionDeclarations(), WTFMove(varDeclarations), WTFMove(sloppyModeHoistedFunctions), features, context.numConstants() };
}

template <typename LexerType>
template <class TreeBuilder> bool Parser<LexerType>::isArrowFunctionParameters(TreeBuilder& context)
{
    if (match(OPENPAREN)) {
        SavePoint saveArrowFunctionPoint = createSavePoint(context);
        next();
        bool isArrowFunction = false;
        if (match(CLOSEPAREN)) {
            next();
            isArrowFunction = match(ARROWFUNCTION);
        } else {
            SyntaxChecker syntaxChecker(const_cast<VM&>(m_vm), m_lexer.get());
            // We make fake scope, otherwise parseFormalParameters will add variable to current scope that lead to errors
            AutoPopScopeRef fakeScope(this, pushScope());
            fakeScope->setSourceParseMode(SourceParseMode::ArrowFunctionMode);

            unsigned parametersCount = 0;
            bool isArrowFunctionParameterList = true;
            bool isMethod = false;
            isArrowFunction = parseFormalParameters(syntaxChecker, syntaxChecker.createFormalParameterList(), isArrowFunctionParameterList, isMethod, parametersCount) && consume(CLOSEPAREN) && match(ARROWFUNCTION);
            propagateError();
            popScope(fakeScope, syntaxChecker.NeedsFreeVariableInfo);
        }
        restoreSavePoint(context, saveArrowFunctionPoint);
        return isArrowFunction;
    }

    if (matchSpecIdentifier()) {
        semanticFailIfTrue(isDisallowedIdentifierAwait(m_token), "Cannot use 'await' as a parameter name in an async function");
        SavePoint saveArrowFunctionPoint = createSavePoint(context);
        next();
        bool isArrowFunction = match(ARROWFUNCTION);
        restoreSavePoint(context, saveArrowFunctionPoint);
        return isArrowFunction;
    }

    return false;
}

template <typename LexerType>
bool Parser<LexerType>::allowAutomaticSemicolon()
{
    return match(CLOSEBRACE) || match(EOFTOK) || m_lexer->hasLineTerminatorBeforeToken();
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseSourceElements(TreeBuilder& context, SourceElementsMode mode)
{
    const unsigned lengthOfUseStrictLiteral = 12; // "use strict".length
    TreeSourceElements sourceElements = context.createSourceElements();
    const Identifier* directive = nullptr;
    unsigned directiveLiteralLength = 0;
    auto savePoint = createSavePoint(context);
    bool shouldCheckForUseStrict = mode == CheckForStrictMode;
    
    while (TreeStatement statement = parseStatementListItem(context, directive, &directiveLiteralLength)) {
        if (shouldCheckForUseStrict) {
            if (directive) {
                // "use strict" must be the exact literal without escape sequences or line continuation.
                if (directiveLiteralLength == lengthOfUseStrictLiteral && m_vm.propertyNames->useStrictIdentifier == *directive) {
                    setStrictMode();
                    shouldCheckForUseStrict = false; // We saw "use strict", there is no need to keep checking for it.
                    if (!isValidStrictMode()) {
                        if (m_parserState.lastFunctionName) {
                            if (m_vm.propertyNames->arguments == *m_parserState.lastFunctionName)
                                semanticFail("Cannot name a function 'arguments' in strict mode");
                            if (m_vm.propertyNames->eval == *m_parserState.lastFunctionName)
                                semanticFail("Cannot name a function 'eval' in strict mode");
                        }
                        if (hasDeclaredVariable(m_vm.propertyNames->arguments))
                            semanticFail("Cannot declare a variable named 'arguments' in strict mode");
                        if (hasDeclaredVariable(m_vm.propertyNames->eval))
                            semanticFail("Cannot declare a variable named 'eval' in strict mode");
                        semanticFailIfTrue(currentScope()->hasNonSimpleParameterList(), "'use strict' directive not allowed inside a function with a non-simple parameter list");
                        semanticFailIfFalse(isValidStrictMode(), "Invalid parameters or function name in strict mode");
                    }
                    // Since strict mode is changed, restoring lexer state by calling next() may cause errors.
                    restoreSavePoint(context, savePoint);
                    propagateError();
                    continue;
                }

                // We saw a directive, but it wasn't "use strict". We reset our state to
                // see if the next statement we parse is also a directive.
                directive = nullptr;
            } else {
                // We saw a statement that wasn't in the form of a directive. The spec says that "use strict"
                // is only allowed as the first statement, or after a sequence of directives before it, but
                // not after non-directive statements.
                shouldCheckForUseStrict = false;
            }
        }
        context.appendStatement(sourceElements, statement);
    }

    propagateError();
    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseModuleSourceElements(TreeBuilder& context)
{
    TreeSourceElements sourceElements = context.createSourceElements();
    SyntaxChecker syntaxChecker(const_cast<VM&>(m_vm), m_lexer.get());

    while (true) {
        TreeStatement statement = 0;
        switch (m_token.m_type) {
        case EXPORT_:
            statement = parseExportDeclaration(context);
            if (statement)
                recordPauseLocation(context.breakpointLocation(statement));
            break;

        case IMPORT: {
            SavePoint savePoint = createSavePoint(context);
            next();
            bool isImportDeclaration = !match(OPENPAREN) && !match(DOT);
            restoreSavePoint(context, savePoint);
            if (isImportDeclaration) {
                statement = parseImportDeclaration(context);
                if (statement)
                    recordPauseLocation(context.breakpointLocation(statement));
                break;
            }

            // This is `import("...")` call or `import.meta` meta property case.
            FALLTHROUGH;
        }

        default: {
            const Identifier* directive = nullptr;
            unsigned directiveLiteralLength = 0;
            if (sourceParseMode() == SourceParseMode::ModuleAnalyzeMode) {
                if (!parseStatementListItem(syntaxChecker, directive, &directiveLiteralLength))
                    goto end;
                continue;
            }
            statement = parseStatementListItem(context, directive, &directiveLiteralLength);
            break;
        }
        }

        if (!statement)
            goto end;
        context.appendStatement(sourceElements, statement);
    }

end:
    propagateError();

    for (const auto& pair : m_moduleScopeData->exportedBindings()) {
        const auto& uid = pair.key;
        if (currentScope()->hasDeclaredVariable(uid)) {
            currentScope()->declaredVariables().markVariableAsExported(uid);
            continue;
        }

        if (currentScope()->hasLexicallyDeclaredVariable(uid)) {
            currentScope()->lexicalVariables().markVariableAsExported(uid);
            continue;
        }

        semanticFail("Exported binding '", uid.get(), "' needs to refer to a top-level declared variable");
    }

    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseGeneratorFunctionSourceElements(TreeBuilder& context, const Identifier& name, SourceElementsMode mode)
{
    auto sourceElements = context.createSourceElements();

    unsigned functionKeywordStart = tokenStart();
    JSTokenLocation startLocation(tokenLocation());
    JSTextPosition start = tokenStartPosition();
    unsigned startColumn = tokenColumn();
    int functionNameStart = m_token.m_location.startOffset;
    int parametersStart = m_token.m_location.startOffset;

    ParserFunctionInfo<TreeBuilder> info;
    info.name = &m_vm.propertyNames->nullIdentifier;
    createGeneratorParameters(context, info.parameterCount);
    info.startOffset = parametersStart;
    info.startLine = tokenLine();

    {
        AutoPopScopeRef generatorBodyScope(this, pushScope());
        generatorBodyScope->setSourceParseMode(SourceParseMode::GeneratorBodyMode);
        generatorBodyScope->setConstructorKind(ConstructorKind::None);
        generatorBodyScope->setExpectedSuperBinding(m_superBinding);

        SyntaxChecker generatorFunctionContext(const_cast<VM&>(m_vm), m_lexer.get());
        failIfFalse(parseSourceElements(generatorFunctionContext, mode), "Cannot parse the body of a generator");
        popScope(generatorBodyScope, TreeBuilder::NeedsFreeVariableInfo);
    }
    info.body = context.createFunctionMetadata(startLocation, tokenLocation(), startColumn, tokenColumn(), functionKeywordStart, functionNameStart, parametersStart, strictMode(), ConstructorKind::None, m_superBinding, info.parameterCount, SourceParseMode::GeneratorBodyMode, false);

    info.endLine = tokenLine();
    info.endOffset = m_token.m_data.offset;
    info.parametersStartColumn = startColumn;

    auto functionExpr = context.createGeneratorFunctionBody(startLocation, info, name);
    auto statement = context.createExprStatement(startLocation, functionExpr, start, m_lastTokenEndPosition.line);
    context.appendStatement(sourceElements, statement);

    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseAsyncFunctionSourceElements(TreeBuilder& context, bool isArrowFunctionBodyExpression, SourceElementsMode mode)
{
    ASSERT(isAsyncFunctionOrAsyncGeneratorWrapperParseMode(sourceParseMode()));
    auto sourceElements = context.createSourceElements();

    unsigned functionKeywordStart = tokenStart();
    JSTokenLocation startLocation(tokenLocation());
    JSTextPosition start = tokenStartPosition();
    unsigned startColumn = tokenColumn();
    int functionNameStart = m_token.m_location.startOffset;
    int parametersStart = m_token.m_location.startOffset;

    ParserFunctionInfo<TreeBuilder> info;
    info.name = &m_vm.propertyNames->nullIdentifier;
    createGeneratorParameters(context, info.parameterCount);
    info.startOffset = parametersStart;
    info.startLine = tokenLine();

    SourceParseMode parseMode = getAsynFunctionBodyParseMode(sourceParseMode());
    SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);
    {
        AutoPopScopeRef asyncFunctionBodyScope(this, pushScope());
        asyncFunctionBodyScope->setSourceParseMode(sourceParseMode());
        SyntaxChecker syntaxChecker(const_cast<VM&>(m_vm), m_lexer.get());
        if (isArrowFunctionBodyExpression) {
            if (m_debuggerParseData)
                failIfFalse(parseArrowFunctionSingleExpressionBodySourceElements(context), "Cannot parse the body of async arrow function");
            else
                failIfFalse(parseArrowFunctionSingleExpressionBodySourceElements(syntaxChecker), "Cannot parse the body of async arrow function");
        } else {
            if (m_debuggerParseData)
                failIfFalse(parseSourceElements(context, mode), "Cannot parse the body of async function");
            else
                failIfFalse(parseSourceElements(syntaxChecker, mode), "Cannot parse the body of async function");
        }
        popScope(asyncFunctionBodyScope, TreeBuilder::NeedsFreeVariableInfo);
    }
    info.body = context.createFunctionMetadata(startLocation, tokenLocation(), startColumn, tokenColumn(), functionKeywordStart, functionNameStart, parametersStart, strictMode(), ConstructorKind::None, m_superBinding, info.parameterCount, sourceParseMode(), isArrowFunctionBodyExpression);

    info.endLine = tokenLine();
    info.endOffset = isArrowFunctionBodyExpression ? tokenLocation().endOffset : m_token.m_data.offset;
    info.parametersStartColumn = startColumn;

    auto functionExpr = context.createAsyncFunctionBody(startLocation, info, parseMode);
    auto statement = context.createExprStatement(startLocation, functionExpr, start, m_lastTokenEndPosition.line);
    context.appendStatement(sourceElements, statement);

    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseAsyncGeneratorFunctionSourceElements(TreeBuilder& context, bool isArrowFunctionBodyExpression, SourceElementsMode mode)
{
    ASSERT(isAsyncGeneratorWrapperParseMode(sourceParseMode()));
    auto sourceElements = context.createSourceElements();
        
    unsigned functionKeywordStart = tokenStart();
    JSTokenLocation startLocation(tokenLocation());
    JSTextPosition start = tokenStartPosition();
    unsigned startColumn = tokenColumn();
    int functionNameStart = m_token.m_location.startOffset;
    int parametersStart = m_token.m_location.startOffset;
    
    ParserFunctionInfo<TreeBuilder> info;
    info.name = &m_vm.propertyNames->nullIdentifier;
    createGeneratorParameters(context, info.parameterCount);
    info.startOffset = parametersStart;
    info.startLine = tokenLine();

    SourceParseMode parseMode = SourceParseMode::AsyncGeneratorBodyMode;
    SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);
    {
        AutoPopScopeRef asyncFunctionBodyScope(this, pushScope());
        asyncFunctionBodyScope->setSourceParseMode(sourceParseMode());
        SyntaxChecker syntaxChecker(const_cast<VM&>(m_vm), m_lexer.get());
        if (isArrowFunctionBodyExpression) {
            if (m_debuggerParseData)
                failIfFalse(parseArrowFunctionSingleExpressionBodySourceElements(context), "Cannot parse the body of async arrow function");
            else
                failIfFalse(parseArrowFunctionSingleExpressionBodySourceElements(syntaxChecker), "Cannot parse the body of async arrow function");
        } else {
            if (m_debuggerParseData)
                failIfFalse(parseSourceElements(context, mode), "Cannot parse the body of async function");
            else
                failIfFalse(parseSourceElements(syntaxChecker, mode), "Cannot parse the body of async function");
        }
        popScope(asyncFunctionBodyScope, TreeBuilder::NeedsFreeVariableInfo);
    }
    info.body = context.createFunctionMetadata(startLocation, tokenLocation(), startColumn, tokenColumn(), functionKeywordStart, functionNameStart, parametersStart, strictMode(), ConstructorKind::None, m_superBinding, info.parameterCount, parseMode, isArrowFunctionBodyExpression);

    info.endLine = tokenLine();
    info.endOffset = isArrowFunctionBodyExpression ? tokenLocation().endOffset : m_token.m_data.offset;
    info.parametersStartColumn = startColumn;

    auto functionExpr = context.createAsyncFunctionBody(startLocation, info, parseMode);
    auto statement = context.createExprStatement(startLocation, functionExpr, start, m_lastTokenEndPosition.line);
    context.appendStatement(sourceElements, statement);
        
    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseSingleFunction(TreeBuilder& context, Optional<int> functionConstructorParametersEndPosition)
{
    TreeSourceElements sourceElements = context.createSourceElements();
    TreeStatement statement = 0;
    switch (m_token.m_type) {
    case FUNCTION:
        statement = parseFunctionDeclaration(context, FunctionDeclarationType::Declaration, ExportType::NotExported, DeclarationDefaultContext::Standard, functionConstructorParametersEndPosition);
        break;
    case IDENT:
        if (*m_token.m_data.ident == m_vm.propertyNames->async && !m_token.m_data.escaped) {
            next();
            failIfFalse(match(FUNCTION) && !m_lexer->hasLineTerminatorBeforeToken(), "Cannot parse the async function");
            statement = parseAsyncFunctionDeclaration(context, ExportType::NotExported, DeclarationDefaultContext::Standard, functionConstructorParametersEndPosition);
            break;
        }
        FALLTHROUGH;
    default:
        failDueToUnexpectedToken();
        break;
    }

    if (statement) {
        context.setEndOffset(statement, m_lastTokenEndPosition.offset);
        context.appendStatement(sourceElements, statement);
    }

    propagateError();
    return sourceElements;
}

    
template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseStatementListItem(TreeBuilder& context, const Identifier*& directive, unsigned* directiveLiteralLength)
{
    // The grammar is documented here:
    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-statements
    DepthManager statementDepth(&m_statementDepth);
    m_statementDepth++;
    failIfStackOverflow();
    TreeStatement result = 0;
    bool shouldSetEndOffset = true;
    bool shouldSetPauseLocation = false;

    switch (m_token.m_type) {
    case CONSTTOKEN:
        result = parseVariableDeclaration(context, DeclarationType::ConstDeclaration);
        shouldSetPauseLocation = true;
        break;
    case LET: {
        bool shouldParseVariableDeclaration = true;
        if (!strictMode()) {
            SavePoint savePoint = createSavePoint(context);
            next();
            // Intentionally use `matchIdentifierOrPossiblyEscapedContextualKeyword()` and not `matchSpecIdentifier()`.
            // We would like contextual keywords to fall under parseVariableDeclaration even when not used as identifiers.
            // For example, under a generator context, matchSpecIdentifier() for "yield" returns `false`.
            // But we would like to enter parseVariableDeclaration and raise an error under the context of parseVariableDeclaration
            // to raise consistent errors between "var", "const" and "let".
            if (!matchIdentifierOrPossiblyEscapedContextualKeyword() && !match(OPENBRACE) && !match(OPENBRACKET))
                shouldParseVariableDeclaration = false;
            restoreSavePoint(context, savePoint);
        }
        if (shouldParseVariableDeclaration)
            result = parseVariableDeclaration(context, DeclarationType::LetDeclaration);
        else {
            bool allowFunctionDeclarationAsStatement = true;
            result = parseExpressionOrLabelStatement(context, allowFunctionDeclarationAsStatement);
        }
        shouldSetPauseLocation = !context.shouldSkipPauseLocation(result);
        break;
    }
    case CLASSTOKEN:
        result = parseClassDeclaration(context);
        break;
    case FUNCTION:
        result = parseFunctionDeclaration(context);
        break;
    case ESCAPED_KEYWORD:
        if (!matchAllowedEscapedContextualKeyword()) {
            failDueToUnexpectedToken();
            break;
        }
        FALLTHROUGH;
    case IDENT:
        if (UNLIKELY(*m_token.m_data.ident == m_vm.propertyNames->async && !m_token.m_data.escaped)) {
            // Eagerly parse as AsyncFunctionDeclaration. This is the uncommon case,
            // but could be mistakenly parsed as an AsyncFunctionExpression.
            SavePoint savePoint = createSavePoint(context);
            next();
            if (UNLIKELY(match(FUNCTION) && !m_lexer->hasLineTerminatorBeforeToken())) {
                result = parseAsyncFunctionDeclaration(context);
                break;
            }
            restoreSavePoint(context, savePoint);
        }
        FALLTHROUGH;
    case AWAIT:
    case YIELD: {
        // This is a convenient place to notice labeled statements
        // (even though we also parse them as normal statements)
        // because we allow the following type of code in sloppy mode:
        // ``` function foo() { label: function bar() { } } ```
        bool allowFunctionDeclarationAsStatement = true;
        result = parseExpressionOrLabelStatement(context, allowFunctionDeclarationAsStatement);
        shouldSetPauseLocation = !context.shouldSkipPauseLocation(result);
        break;
    }
    default:
        m_statementDepth--; // parseStatement() increments the depth.
        result = parseStatement(context, directive, directiveLiteralLength);
        shouldSetEndOffset = false;
        break;
    }

    if (result) {
        if (shouldSetEndOffset)
            context.setEndOffset(result, m_lastTokenEndPosition.offset);
        if (shouldSetPauseLocation)
            recordPauseLocation(context.breakpointLocation(result));
    }

    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseVariableDeclaration(TreeBuilder& context, DeclarationType declarationType, ExportType exportType)
{
    ASSERT(match(VAR) || match(LET) || match(CONSTTOKEN));
    JSTokenLocation location(tokenLocation());
    int start = tokenLine();
    int end = 0;
    int scratch;
    TreeDestructuringPattern scratch1 = 0;
    TreeExpression scratch2 = 0;
    JSTextPosition scratch3;
    bool scratchBool;
    TreeExpression variableDecls = parseVariableDeclarationList(context, scratch, scratch1, scratch2, scratch3, scratch3, scratch3, VarDeclarationContext, declarationType, exportType, scratchBool);
    propagateError();
    failIfFalse(autoSemiColon(), "Expected ';' after variable declaration");
    
    return context.createDeclarationStatement(location, variableDecls, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseDoWhileStatement(TreeBuilder& context)
{
    ASSERT(match(DO));
    int startLine = tokenLine();
    next();
    const Identifier* unused = nullptr;
    startLoop();
    TreeStatement statement = parseStatement(context, unused);
    endLoop();
    failIfFalse(statement, "Expected a statement following 'do'");
    int endLine = tokenLine();
    JSTokenLocation location(tokenLocation());
    handleProductionOrFail(WHILE, "while", "end", "do-while loop");
    handleProductionOrFail(OPENPAREN, "(", "start", "do-while loop condition");
    semanticFailIfTrue(match(CLOSEPAREN), "Must provide an expression as a do-while loop condition");
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr, "Unable to parse do-while loop condition");
    recordPauseLocation(context.breakpointLocation(expr));
    handleProductionOrFail(CLOSEPAREN, ")", "end", "do-while loop condition");
    if (match(SEMICOLON))
        next(); // Always performs automatic semicolon insertion.
    return context.createDoWhileStatement(location, statement, expr, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseWhileStatement(TreeBuilder& context)
{
    ASSERT(match(WHILE));
    JSTokenLocation location(tokenLocation());
    int startLine = tokenLine();
    next();
    
    handleProductionOrFail(OPENPAREN, "(", "start", "while loop condition");
    semanticFailIfTrue(match(CLOSEPAREN), "Must provide an expression as a while loop condition");
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr, "Unable to parse while loop condition");
    recordPauseLocation(context.breakpointLocation(expr));
    int endLine = tokenLine();
    handleProductionOrFail(CLOSEPAREN, ")", "end", "while loop condition");

    const Identifier* unused = nullptr;
    startLoop();
    TreeStatement statement = parseStatement(context, unused);
    endLoop();
    failIfFalse(statement, "Expected a statement as the body of a while loop");
    return context.createWhileStatement(location, expr, statement, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseVariableDeclarationList(TreeBuilder& context, int& declarations, TreeDestructuringPattern& lastPattern, TreeExpression& lastInitializer, JSTextPosition& identStart, JSTextPosition& initStart, JSTextPosition& initEnd, VarDeclarationListContext declarationListContext, DeclarationType declarationType, ExportType exportType, bool& forLoopConstDoesNotHaveInitializer)
{
    ASSERT(declarationType == DeclarationType::LetDeclaration || declarationType == DeclarationType::VarDeclaration || declarationType == DeclarationType::ConstDeclaration);
    TreeExpression head = 0;
    JSTokenLocation headLocation;
    TreeExpression tail = 0;
    const Identifier* lastIdent;
    JSToken lastIdentToken; 
    AssignmentContext assignmentContext = assignmentContextFromDeclarationType(declarationType);
    do {
        lastPattern = TreeDestructuringPattern(0);
        lastIdent = nullptr;
        JSTokenLocation location(tokenLocation());
        next();
        if (head) {
            // Move the location of subsequent declarations after the comma.
            location = tokenLocation();
        }
        TreeExpression node = 0;
        declarations++;
        bool hasInitializer = false;

        failIfTrue(match(PRIVATENAME), "Cannot use a private name to declare a variable");
        if (matchSpecIdentifier()) {
            failIfTrue(isPossiblyEscapedLet(m_token) && (declarationType == DeclarationType::LetDeclaration || declarationType == DeclarationType::ConstDeclaration), 
                "Cannot use 'let' as an identifier name for a LexicalDeclaration");
            semanticFailIfTrue(isDisallowedIdentifierAwait(m_token), "Cannot use 'await' as a ", declarationTypeToVariableKind(declarationType), " ", disallowedIdentifierAwaitReason());
            JSTextPosition varStart = tokenStartPosition();
            JSTokenLocation varStartLocation(tokenLocation());
            identStart = varStart;
            const Identifier* name = m_token.m_data.ident;
            lastIdent = name;
            lastIdentToken = m_token;
            next();
            hasInitializer = match(EQUAL);
            DeclarationResultMask declarationResult = declareVariable(name, declarationType);
            if (declarationResult != DeclarationResult::Valid) {
                failIfTrueIfStrict(declarationResult & DeclarationResult::InvalidStrictMode, "Cannot declare a variable named ", name->impl(), " in strict mode");
                if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration) {
                    if (declarationType == DeclarationType::LetDeclaration) 
                        internalFailWithMessage(false, "Cannot declare a let variable twice: '", name->impl(), "'");
                    if (declarationType == DeclarationType::ConstDeclaration)
                        internalFailWithMessage(false, "Cannot declare a const variable twice: '", name->impl(), "'");
                    ASSERT(declarationType == DeclarationType::VarDeclaration);
                    internalFailWithMessage(false, "Cannot declare a var variable that shadows a let/const/class variable: '", name->impl(), "'");
                }
            }
            if (exportType == ExportType::Exported) {
                semanticFailIfFalse(exportName(*name), "Cannot export a duplicate name '", name->impl(), "'");
                m_moduleScopeData->exportBinding(*name);
            }

            if (hasInitializer) {
                JSTextPosition varDivot = tokenStartPosition() + 1;
                initStart = tokenStartPosition();
                next(TreeBuilder::DontBuildStrings); // consume '='
                propagateError();
                TreeExpression initializer = parseAssignmentExpression(context);
                initEnd = lastTokenEndPosition();
                lastInitializer = initializer;
                failIfFalse(initializer, "Expected expression as the intializer for the variable '", name->impl(), "'");
                
                node = context.createAssignResolve(location, *name, initializer, varStart, varDivot, lastTokenEndPosition(), assignmentContext);
            } else {
                if (declarationListContext == ForLoopContext && declarationType == DeclarationType::ConstDeclaration)
                    forLoopConstDoesNotHaveInitializer = true;
                failIfTrue(declarationListContext != ForLoopContext && declarationType == DeclarationType::ConstDeclaration, "const declared variable '", name->impl(), "'", " must have an initializer");
                if (declarationType == DeclarationType::VarDeclaration)
                    node = context.createEmptyVarExpression(varStartLocation, *name);
                else
                    node = context.createEmptyLetExpression(varStartLocation, *name);
            }
        } else {
            lastIdent = nullptr;
            auto pattern = parseDestructuringPattern(context, destructuringKindFromDeclarationType(declarationType), exportType, nullptr, nullptr, assignmentContext);
            failIfFalse(pattern, "Cannot parse this destructuring pattern");
            hasInitializer = match(EQUAL);
            failIfTrue(declarationListContext == VarDeclarationContext && !hasInitializer, "Expected an initializer in destructuring variable declaration");
            lastPattern = pattern;
            if (hasInitializer) {
                next(TreeBuilder::DontBuildStrings); // consume '='
                TreeExpression rhs = parseAssignmentExpression(context);
                propagateError();
                ASSERT(rhs);
                node = context.createDestructuringAssignment(location, pattern, rhs);
                lastInitializer = rhs;
            }
        }

        if (node) {
            if (!head) {
                head = node;
                headLocation = location;
            } else {
                if (!tail) {
                    head = tail = context.createCommaExpr(headLocation, head);
                    recordPauseLocation(context.breakpointLocation(head));
                }
                tail = context.appendToCommaExpr(location, head, tail, node);
                recordPauseLocation(context.breakpointLocation(tail));
            }
        }
    } while (match(COMMA));
    if (lastIdent)
        lastPattern = context.createBindingLocation(lastIdentToken.m_location, *lastIdent, lastIdentToken.m_startPosition, lastIdentToken.m_endPosition, assignmentContext);

    return head;
}

template <typename LexerType>
bool Parser<LexerType>::declareRestOrNormalParameter(const Identifier& name, const Identifier** duplicateIdentifier)
{
    DeclarationResultMask declarationResult = declareParameter(&name);
    if ((declarationResult & DeclarationResult::InvalidStrictMode) && strictMode()) {
        semanticFailIfTrue(isEvalOrArguments(&name), "Cannot destructure to a parameter name '", name.impl(), "' in strict mode");
        if (m_parserState.lastFunctionName && name == *m_parserState.lastFunctionName)
            semanticFail("Cannot declare a parameter named '", name.impl(), "' as it shadows the name of a strict mode function");
        semanticFailureDueToKeyword("parameter name");
        if (!m_lexer->isReparsingFunction() && hasDeclaredParameter(name))
            semanticFail("Cannot declare a parameter named '", name.impl(), "' in strict mode as it has already been declared");
        semanticFail("Cannot declare a parameter named '", name.impl(), "' in strict mode");
    }
    if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration) {
        // It's not always an error to define a duplicate parameter.
        // It's only an error when there are default parameter values or destructuring parameters.
        // We note this value now so we can check it later.
        if (duplicateIdentifier)
            *duplicateIdentifier = &name;
    }

    return true;
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::createBindingPattern(TreeBuilder& context, DestructuringKind kind, ExportType exportType, const Identifier& name, const JSToken& token, AssignmentContext bindingContext, const Identifier** duplicateIdentifier)
{
    ASSERT(!name.isNull());
    
    ASSERT(name.impl()->isAtom() || name.impl()->isSymbol());

    switch (kind) {
    case DestructuringKind::DestructureToVariables: {
        DeclarationResultMask declarationResult = declareVariable(&name);
        failIfTrueIfStrict(declarationResult & DeclarationResult::InvalidStrictMode, "Cannot declare a variable named '", name.impl(), "' in strict mode");
        if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration)
            internalFailWithMessage(false, "Cannot declare a var variable that shadows a let/const/class variable: '", name.impl(), "'");
        break;
    }

    case DestructuringKind::DestructureToLet:
    case DestructuringKind::DestructureToConst:
    case DestructuringKind::DestructureToCatchParameters: {
        DeclarationResultMask declarationResult = declareVariable(&name, kind == DestructuringKind::DestructureToConst ? DeclarationType::ConstDeclaration : DeclarationType::LetDeclaration);
        if (declarationResult != DeclarationResult::Valid) {
            failIfTrueIfStrict(declarationResult & DeclarationResult::InvalidStrictMode, "Cannot destructure to a variable named '", name.impl(), "' in strict mode");
            failIfTrue(declarationResult & DeclarationResult::InvalidDuplicateDeclaration, "Cannot declare a lexical variable twice: '", name.impl(), "'");
        }
        break;
    }

    case DestructuringKind::DestructureToParameters: {
        declareRestOrNormalParameter(name, duplicateIdentifier);
        propagateError();
        break;
    }

    case DestructuringKind::DestructureToExpressions: {
        break;
    }
    }

    if (exportType == ExportType::Exported) {
        semanticFailIfFalse(exportName(name), "Cannot export a duplicate name '", name.impl(), "'");
        m_moduleScopeData->exportBinding(name);
    }
    return context.createBindingLocation(token.m_location, name, token.m_startPosition, token.m_endPosition, bindingContext);
}

template <typename LexerType>
template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern Parser<LexerType>::createAssignmentElement(TreeBuilder& context, TreeExpression& assignmentTarget, const JSTextPosition& startPosition, const JSTextPosition& endPosition)
{
    return context.createAssignmentElement(assignmentTarget, startPosition, endPosition);
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseArrowFunctionSingleExpressionBodySourceElements(TreeBuilder& context)
{
    ASSERT(!match(OPENBRACE));

    JSTokenLocation location(tokenLocation());
    JSTextPosition start = tokenStartPosition();

    failIfStackOverflow();
    TreeExpression expr = parseAssignmentExpression(context);
    failIfFalse(expr, "Cannot parse the arrow function expression");
    
    context.setEndOffset(expr, m_lastTokenEndPosition.offset);

    JSTextPosition end = tokenEndPosition();
    
    TreeSourceElements sourceElements = context.createSourceElements();
    TreeStatement body = context.createReturnStatement(location, expr, start, end);
    context.setEndOffset(body, m_lastTokenEndPosition.offset);
    recordPauseLocation(context.breakpointLocation(body));
    context.appendStatement(sourceElements, body);

    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::tryParseDestructuringPatternExpression(TreeBuilder& context, AssignmentContext bindingContext)
{
    return parseDestructuringPattern(context, DestructuringKind::DestructureToExpressions, ExportType::NotExported, nullptr, nullptr, bindingContext);
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::parseBindingOrAssignmentElement(TreeBuilder& context, DestructuringKind kind, ExportType exportType, const Identifier** duplicateIdentifier, bool* hasDestructuringPattern, AssignmentContext bindingContext, int depth)
{
    if (kind == DestructuringKind::DestructureToExpressions)
        return parseAssignmentElement(context, kind, exportType, duplicateIdentifier, hasDestructuringPattern, bindingContext, depth);
    return parseDestructuringPattern(context, kind, exportType, duplicateIdentifier, hasDestructuringPattern, bindingContext, depth);
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::parseObjectRestAssignmentElement(TreeBuilder& context)
{
    JSTextPosition startPosition = tokenStartPosition();
    auto element = parseMemberExpression(context);

    if (!element || !context.isAssignmentLocation(element)) {
        reclassifyExpressionError(ErrorIndicatesPattern, ErrorIndicatesNothing);
        semanticFail("Invalid destructuring assignment target");
    }

    if (strictMode() && m_parserState.lastIdentifier && context.isResolve(element)) {
        bool isEvalOrArguments = m_vm.propertyNames->eval == *m_parserState.lastIdentifier || m_vm.propertyNames->arguments == *m_parserState.lastIdentifier;
        if (isEvalOrArguments && strictMode())
            reclassifyExpressionError(ErrorIndicatesPattern, ErrorIndicatesNothing);
        failIfTrueIfStrict(isEvalOrArguments, "Cannot modify '", m_parserState.lastIdentifier->impl(), "' in strict mode");
    }

    return createAssignmentElement(context, element, startPosition, lastTokenEndPosition());
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::parseAssignmentElement(TreeBuilder& context, DestructuringKind kind, ExportType exportType, const Identifier** duplicateIdentifier, bool* hasDestructuringPattern, AssignmentContext bindingContext, int depth)
{
    TreeDestructuringPattern assignmentTarget = 0;

    if (match(OPENBRACE) || match(OPENBRACKET)) {
        SavePoint savePoint = createSavePoint(context);
        assignmentTarget = parseDestructuringPattern(context, kind, exportType, duplicateIdentifier, hasDestructuringPattern, bindingContext, depth);
        if (assignmentTarget && !match(DOT) && !match(OPENBRACKET) && !match(OPENPAREN) && !match(BACKQUOTE))
            return assignmentTarget;
        restoreSavePoint(context, savePoint);
    }

    JSTextPosition startPosition = tokenStartPosition();
    auto element = parseMemberExpression(context);

    semanticFailIfFalse(element && context.isAssignmentLocation(element), "Invalid destructuring assignment target");

    if (strictMode() && m_parserState.lastIdentifier && context.isResolve(element)) {
        bool isEvalOrArguments = m_vm.propertyNames->eval == *m_parserState.lastIdentifier || m_vm.propertyNames->arguments == *m_parserState.lastIdentifier;
        failIfTrueIfStrict(isEvalOrArguments, "Cannot modify '", m_parserState.lastIdentifier->impl(), "' in strict mode");
    }

    return createAssignmentElement(context, element, startPosition, lastTokenEndPosition());
}

static const char* destructuringKindToVariableKindName(DestructuringKind kind)
{
    switch (kind) {
    case DestructuringKind::DestructureToLet:
    case DestructuringKind::DestructureToConst:
        return "lexical variable name";
    case DestructuringKind::DestructureToVariables:
        return "variable name";
    case DestructuringKind::DestructureToParameters:
        return "parameter name";
    case DestructuringKind::DestructureToCatchParameters:
        return "catch parameter name";
    case DestructuringKind::DestructureToExpressions:
        return "expression name";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return "invalid";
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::parseObjectRestElement(TreeBuilder& context, DestructuringKind kind, ExportType exportType, const Identifier** duplicateIdentifier, AssignmentContext bindingContext)
{
    ASSERT(kind != DestructuringKind::DestructureToExpressions);
    failIfStackOverflow();
    TreeDestructuringPattern pattern;
    
    if (!matchSpecIdentifier()) {
        semanticFailureDueToKeyword(destructuringKindToVariableKindName(kind));
        failWithMessage("Expected a binding element");
    }
    failIfTrue(match(LET) && (kind == DestructuringKind::DestructureToLet || kind == DestructuringKind::DestructureToConst), "Cannot use 'let' as an identifier name for a LexicalDeclaration");
    semanticFailIfTrue(isDisallowedIdentifierAwait(m_token), "Cannot use 'await' as a ", destructuringKindToVariableKindName(kind), " ", disallowedIdentifierAwaitReason());
    pattern = createBindingPattern(context, kind, exportType, *m_token.m_data.ident, m_token, bindingContext, duplicateIdentifier);
    next();
    return pattern;
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::parseObjectRestBindingOrAssignmentElement(TreeBuilder& context, DestructuringKind kind, ExportType exportType, const Identifier** duplicateIdentifier, AssignmentContext bindingContext)
{
    if (kind == DestructuringKind::DestructureToExpressions)
        return parseObjectRestAssignmentElement(context);
    return parseObjectRestElement(context, kind, exportType, duplicateIdentifier, bindingContext);
}

template <typename LexerType>
template <class TreeBuilder> TreeDestructuringPattern Parser<LexerType>::parseDestructuringPattern(TreeBuilder& context, DestructuringKind kind, ExportType exportType, const Identifier** duplicateIdentifier, bool* hasDestructuringPattern, AssignmentContext bindingContext, int depth)
{
    failIfStackOverflow();
    m_parserState.assignmentCount++;
    int nonLHSCount = m_parserState.nonLHSCount;
    TreeDestructuringPattern pattern;
    switch (m_token.m_type) {
    case OPENBRACKET: {
        JSTextPosition divotStart = tokenStartPosition();
        auto arrayPattern = context.createArrayPattern(m_token.m_location);
        next();

        if (hasDestructuringPattern)
            *hasDestructuringPattern = true;

        bool restElementWasFound = false;

        do {
            while (match(COMMA)) {
                context.appendArrayPatternSkipEntry(arrayPattern, m_token.m_location);
                next();
            }
            propagateError();

            if (match(CLOSEBRACKET))
                break;

            if (UNLIKELY(match(DOTDOTDOT))) {
                JSTokenLocation location = m_token.m_location;
                next();
                auto innerPattern = parseBindingOrAssignmentElement(context, kind, exportType, duplicateIdentifier, hasDestructuringPattern, bindingContext, depth + 1);
                if (kind == DestructuringKind::DestructureToExpressions && !innerPattern)
                    return 0;
                failIfFalse(innerPattern, "Cannot parse this destructuring pattern");
                context.appendArrayPatternRestEntry(arrayPattern, location, innerPattern);
                restElementWasFound = true;
                break;
            }

            JSTokenLocation location = m_token.m_location;
            auto innerPattern = parseBindingOrAssignmentElement(context, kind, exportType, duplicateIdentifier, hasDestructuringPattern, bindingContext, depth + 1);
            if (kind == DestructuringKind::DestructureToExpressions && !innerPattern)
                return 0;
            failIfFalse(innerPattern, "Cannot parse this destructuring pattern");
            TreeExpression defaultValue = parseDefaultValueForDestructuringPattern(context);
            propagateError();
            context.appendArrayPatternEntry(arrayPattern, location, innerPattern, defaultValue);
        } while (consume(COMMA));

        consumeOrFail(CLOSEBRACKET, restElementWasFound ? "Expected a closing ']' following a rest element destructuring pattern" : "Expected either a closing ']' or a ',' following an element destructuring pattern");
        context.finishArrayPattern(arrayPattern, divotStart, divotStart, lastTokenEndPosition());
        pattern = arrayPattern;
        break;
    }
    case OPENBRACE: {
        auto objectPattern = context.createObjectPattern(m_token.m_location);
        next();

        if (hasDestructuringPattern)
            *hasDestructuringPattern = true;

        bool restElementWasFound = false;

        do {
            bool wasString = false;

            if (match(CLOSEBRACE))
                break;

            if (match(DOTDOTDOT)) {
                JSTokenLocation location = m_token.m_location;
                next();
                auto innerPattern = parseObjectRestBindingOrAssignmentElement(context, kind, exportType, duplicateIdentifier, bindingContext);
                propagateError();
                if (!innerPattern)
                    return 0;
                context.appendObjectPatternRestEntry(m_vm, objectPattern, location, innerPattern);
                restElementWasFound = true;
                context.setContainsObjectRestElement(objectPattern, restElementWasFound);
                break;
            }

            const Identifier* propertyName = nullptr;
            TreeExpression propertyExpression = 0;
            TreeDestructuringPattern innerPattern = 0;
            JSTokenLocation location = m_token.m_location;
            bool escapedKeyword = match(ESCAPED_KEYWORD);
            if (escapedKeyword || matchSpecIdentifier()) {
                failIfTrue(match(LET) && (kind == DestructuringKind::DestructureToLet || kind == DestructuringKind::DestructureToConst), "Cannot use 'let' as an identifier name for a LexicalDeclaration");
                propertyName = m_token.m_data.ident;
                JSToken identifierToken = m_token;
                next();
                if (consume(COLON))
                    innerPattern = parseBindingOrAssignmentElement(context, kind, exportType, duplicateIdentifier, hasDestructuringPattern, bindingContext, depth + 1);
                else {
                    semanticFailIfTrue(escapedKeyword, "Cannot use abbreviated destructuring syntax for keyword '", propertyName->impl(), "'");
                    if (kind == DestructuringKind::DestructureToExpressions) {
                        bool isEvalOrArguments = m_vm.propertyNames->eval == *propertyName || m_vm.propertyNames->arguments == *propertyName;
                        if (isEvalOrArguments && strictMode())
                            reclassifyExpressionError(ErrorIndicatesPattern, ErrorIndicatesNothing);
                        failIfTrueIfStrict(isEvalOrArguments, "Cannot modify '", propertyName->impl(), "' in strict mode");
                    }
                    semanticFailIfTrue(isDisallowedIdentifierAwait(identifierToken), "Cannot use 'await' as a ", destructuringKindToVariableKindName(kind), " ", disallowedIdentifierAwaitReason());
                    innerPattern = createBindingPattern(context, kind, exportType, *propertyName, identifierToken, bindingContext, duplicateIdentifier);
                }
            } else {
                JSTokenType tokenType = m_token.m_type;
                switch (m_token.m_type) {
                case DOUBLE:
                case INTEGER:
                    propertyName = &m_parserArena.identifierArena().makeNumericIdentifier(const_cast<VM&>(m_vm), m_token.m_data.doubleValue);
                    break;
                case STRING:
                    propertyName = m_token.m_data.ident;
                    wasString = true;
                    break;
                case BIGINT:
                    propertyName = &m_parserArena.identifierArena().makeBigIntDecimalIdentifier(const_cast<VM&>(m_vm), *m_token.m_data.bigIntString, m_token.m_data.radix);
                    break;
                case OPENBRACKET:
                    next();
                    propertyExpression = parseAssignmentExpression(context);
                    failIfFalse(propertyExpression, "Cannot parse computed property name");
                    matchOrFail(CLOSEBRACKET, "Expected ']' to end end a computed property name");
                    break;
                default:
                    if (m_token.m_type != RESERVED && m_token.m_type != RESERVED_IF_STRICT && !(m_token.m_type & KeywordTokenFlag)) {
                        if (kind == DestructuringKind::DestructureToExpressions)
                            return 0;
                        failWithMessage("Expected a property name");
                    }
                    propertyName = m_token.m_data.ident;
                    break;
                }
                next();
                if (!consume(COLON)) {
                    if (kind == DestructuringKind::DestructureToExpressions)
                        return 0;
                    semanticFailIfTrue(tokenType == RESERVED, "Cannot use abbreviated destructuring syntax for reserved name '", propertyName->impl(), "'");
                    semanticFailIfTrue(tokenType == RESERVED_IF_STRICT, "Cannot use abbreviated destructuring syntax for reserved name '", propertyName->impl(), "' in strict mode");
                    semanticFailIfTrue(tokenType & KeywordTokenFlag, "Cannot use abbreviated destructuring syntax for keyword '", propertyName->impl(), "'");
                    
                    failWithMessage("Expected a ':' prior to a named destructuring property");
                }
                innerPattern = parseBindingOrAssignmentElement(context, kind, exportType, duplicateIdentifier, hasDestructuringPattern, bindingContext, depth + 1);
            }
            if (kind == DestructuringKind::DestructureToExpressions && !innerPattern)
                return 0;
            failIfFalse(innerPattern, "Cannot parse this destructuring pattern");
            TreeExpression defaultValue = parseDefaultValueForDestructuringPattern(context);
            propagateError();
            if (propertyExpression) {
                context.appendObjectPatternEntry(m_vm, objectPattern, location, propertyExpression, innerPattern, defaultValue);
                context.setContainsComputedProperty(objectPattern, true);
            } else {
                ASSERT(propertyName);
                context.appendObjectPatternEntry(objectPattern, location, wasString, *propertyName, innerPattern, defaultValue);
            }
        } while (consume(COMMA));

        if (kind == DestructuringKind::DestructureToExpressions && !match(CLOSEBRACE))
            return 0;
        consumeOrFail(CLOSEBRACE, restElementWasFound ? "Expected a closing '}' following a rest element destructuring pattern" : "Expected either a closing '}' or an ',' after a property destructuring pattern");
        pattern = objectPattern;
        break;
    }

    default: {
        if (!matchSpecIdentifier()) {
            if (kind == DestructuringKind::DestructureToExpressions)
                return 0;
            semanticFailureDueToKeyword(destructuringKindToVariableKindName(kind));
            if (kind != DestructuringKind::DestructureToParameters)
                failIfTrue(match(PRIVATENAME), "Cannot use a private name as a ", destructuringKindToVariableKindName(kind));
            failWithMessage("Expected a parameter pattern or a ')' in parameter list");
        }
        failIfTrue(match(LET) && (kind == DestructuringKind::DestructureToLet || kind == DestructuringKind::DestructureToConst), "Cannot use 'let' as an identifier name for a LexicalDeclaration");
        semanticFailIfTrue(isDisallowedIdentifierAwait(m_token), "Cannot use 'await' as a ", destructuringKindToVariableKindName(kind), " ", disallowedIdentifierAwaitReason());
        pattern = createBindingPattern(context, kind, exportType, *m_token.m_data.ident, m_token, bindingContext, duplicateIdentifier);
        next();
        break;
    }
    }
    m_parserState.nonLHSCount = nonLHSCount;
    return pattern;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseDefaultValueForDestructuringPattern(TreeBuilder& context)
{
    if (!match(EQUAL))
        return 0;

    next(TreeBuilder::DontBuildStrings); // consume '='
    return parseAssignmentExpression(context);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseForStatement(TreeBuilder& context)
{
    ASSERT(match(FOR));
    JSTokenLocation location(tokenLocation());
    int startLine = tokenLine();
    bool isAwaitFor = false;
    next();

    DepthManager statementDepth(&m_statementDepth);
    m_statementDepth++;

    if (match(AWAIT)) {
        semanticFailIfFalse(currentScope()->isAsyncFunction() || (isModuleParseMode(sourceParseMode()) && Options::useTopLevelAwait()), "for-await-of can only be used in an async function or async generator");
        isAwaitFor = true;
        next();
    }

    handleProductionOrFail(OPENPAREN, "(", "start", "for-loop header");
    int nonLHSCount = m_parserState.nonLHSCount;
    int declarations = 0;
    JSTokenLocation declLocation(tokenLocation());
    JSTextPosition declsStart;
    JSTextPosition declsEnd;
    TreeExpression decls = 0;
    TreeDestructuringPattern pattern = 0;
    bool isVarDeclaration = match(VAR);
    bool isLetDeclaration = match(LET);
    bool isConstDeclaration = match(CONSTTOKEN);
    bool forLoopConstDoesNotHaveInitializer = false;

    VariableEnvironment dummySet;
    VariableEnvironment* lexicalVariables = nullptr;
    AutoCleanupLexicalScope lexicalScope;

    auto gatherLexicalVariablesIfNecessary = [&] {
        if (isLetDeclaration || isConstDeclaration) {
            ScopeRef scope = lexicalScope.scope();
            lexicalVariables = &scope->finalizeLexicalEnvironment();
        } else
            lexicalVariables = &dummySet;
    };

    auto popLexicalScopeIfNecessary = [&] {
        if (isLetDeclaration || isConstDeclaration)
            popScope(lexicalScope, TreeBuilder::NeedsFreeVariableInfo);
    };

    if (isVarDeclaration || isLetDeclaration || isConstDeclaration) {
        /*
         for (var/let/const IDENT in/of expression) statement
         for (var/let/const varDeclarationList; expressionOpt; expressionOpt)
         */
        if (isLetDeclaration || isConstDeclaration) {
            ScopeRef newScope = pushScope();
            newScope->setIsLexicalScope();
            newScope->preventVarDeclarations();
            lexicalScope.setIsValid(newScope, this);
        }

        TreeDestructuringPattern forInTarget = 0;
        TreeExpression forInInitializer = 0;
        m_allowsIn = false;
        JSTextPosition initStart;
        JSTextPosition initEnd;
        DeclarationType declarationType;
        if (isVarDeclaration)
            declarationType = DeclarationType::VarDeclaration;
        else if (isLetDeclaration)
            declarationType = DeclarationType::LetDeclaration;
        else if (isConstDeclaration)
            declarationType = DeclarationType::ConstDeclaration;
        else
            RELEASE_ASSERT_NOT_REACHED();
        decls = parseVariableDeclarationList(context, declarations, forInTarget, forInInitializer, declsStart, initStart, initEnd, ForLoopContext, declarationType, ExportType::NotExported, forLoopConstDoesNotHaveInitializer);
        m_allowsIn = true;
        propagateError();

        // Remainder of a standard for loop is handled identically
        if (match(SEMICOLON))
            goto standardForLoop;

        failIfFalse(declarations == 1, "can only declare a single variable in an enumeration");

        // Handle for-in with var declaration
        JSTextPosition inLocation = tokenStartPosition();
        bool isOfEnumeration = false;
        if (!match(INTOKEN)) {
            failIfFalse(matchContextualKeyword(m_vm.propertyNames->of), "Expected either 'in' or 'of' in enumeration syntax");
            isOfEnumeration = true;
            next();
        } else {
            failIfFalse(!isAwaitFor, "Expected 'of' in for-await syntax");
            next();
        }

        bool hasAnyAssignments = !!forInInitializer;
        if (hasAnyAssignments) {
            if (isOfEnumeration)
                internalFailWithMessage(false, "Cannot assign to the loop variable inside a for-of loop header");
            if (strictMode() || (isLetDeclaration || isConstDeclaration) || !context.isBindingNode(forInTarget))
                internalFailWithMessage(false, "Cannot assign to the loop variable inside a for-in loop header");
        }

        // While for-in uses Expression, for-of / for-await-of use AssignmentExpression.
        // https://tc39.es/ecma262/#sec-for-in-and-for-of-statements
        TreeExpression expr = 0;
        if (isOfEnumeration)
            expr = parseAssignmentExpression(context);
        else
            expr = parseExpression(context);
        failIfFalse(expr, "Expected expression to enumerate");
        recordPauseLocation(context.breakpointLocation(expr));
        JSTextPosition exprEnd = lastTokenEndPosition();
        
        int endLine = tokenLine();
        
        handleProductionOrFail(CLOSEPAREN, ")", "end", (isOfEnumeration ? "for-of header" : "for-in header"));
        
        const Identifier* unused = nullptr;
        startLoop();
        TreeStatement statement = parseStatement(context, unused);
        endLoop();
        failIfFalse(statement, "Expected statement as body of for-", isOfEnumeration ? "of" : "in", " statement");
        gatherLexicalVariablesIfNecessary();
        TreeStatement result;
        if (isOfEnumeration)
            result = context.createForOfLoop(isAwaitFor, location, forInTarget, expr, statement, declLocation, declsStart, inLocation, exprEnd, startLine, endLine, *lexicalVariables);
        else {
            ASSERT(!isAwaitFor);
            if (isVarDeclaration && forInInitializer)
                result = context.createForInLoop(location, decls, expr, statement, declLocation, declsStart, inLocation, exprEnd, startLine, endLine, *lexicalVariables);
            else
                result = context.createForInLoop(location, forInTarget, expr, statement, declLocation, declsStart, inLocation, exprEnd, startLine, endLine, *lexicalVariables);
        }
        popLexicalScopeIfNecessary();
        return result;
    }
    
    if (!match(SEMICOLON)) {
        if (match(OPENBRACE) || match(OPENBRACKET)) {
            SavePoint savePoint = createSavePoint(context);
            declsStart = tokenStartPosition();
            pattern = tryParseDestructuringPatternExpression(context, AssignmentContext::DeclarationStatement);
            declsEnd = lastTokenEndPosition();
            if (pattern && (match(INTOKEN) || matchContextualKeyword(m_vm.propertyNames->of)))
                goto enumerationLoop;
            pattern = TreeDestructuringPattern(0);
            restoreSavePoint(context, savePoint);
        }
        m_allowsIn = false;
        declsStart = tokenStartPosition();
        decls = parseExpression(context);
        declsEnd = lastTokenEndPosition();
        m_allowsIn = true;
        failIfFalse(decls, "Cannot parse for loop declarations");
        recordPauseLocation(context.breakpointLocation(decls));
    }
    
    if (match(SEMICOLON)) {
    standardForLoop:
        failIfFalse(!isAwaitFor, "Unexpected a ';' in for-await-of header");
        // Standard for loop
        if (decls)
            recordPauseLocation(context.breakpointLocation(decls));
        next();
        TreeExpression condition = 0;
        failIfTrue(forLoopConstDoesNotHaveInitializer && isConstDeclaration, "const variables in for loops must have initializers");
        
        if (!match(SEMICOLON)) {
            condition = parseExpression(context);
            failIfFalse(condition, "Cannot parse for loop condition expression");
            recordPauseLocation(context.breakpointLocation(condition));
        }
        consumeOrFail(SEMICOLON, "Expected a ';' after the for loop condition expression");
        
        TreeExpression increment = 0;
        if (!match(CLOSEPAREN)) {
            increment = parseExpression(context);
            failIfFalse(increment, "Cannot parse for loop iteration expression");
            recordPauseLocation(context.breakpointLocation(increment));
        }
        int endLine = tokenLine();
        handleProductionOrFail(CLOSEPAREN, ")", "end", "for-loop header");
        const Identifier* unused = nullptr;
        startLoop();
        TreeStatement statement = parseStatement(context, unused);
        endLoop();
        failIfFalse(statement, "Expected a statement as the body of a for loop");
        gatherLexicalVariablesIfNecessary();
        TreeStatement result = context.createForLoop(location, decls, condition, increment, statement, startLine, endLine, *lexicalVariables);
        popLexicalScopeIfNecessary();
        return result;
    }
    
    // For-in and For-of loop
enumerationLoop:
    failIfFalse(nonLHSCount == m_parserState.nonLHSCount, "Expected a reference on the left hand side of an enumeration statement");
    bool isOfEnumeration = false;
    if (!match(INTOKEN)) {
        failIfFalse(matchContextualKeyword(m_vm.propertyNames->of), "Expected either 'in' or 'of' in enumeration syntax");
        isOfEnumeration = true;
        next();
    } else {
        failIfFalse(!isAwaitFor, "Expected 'of' in for-await syntax");
        next();
    }

    // While for-in uses Expression, for-of / for-await-of use AssignmentExpression.
    // https://tc39.es/ecma262/#sec-for-in-and-for-of-statements
    TreeExpression expr = 0;
    if (isOfEnumeration)
        expr = parseAssignmentExpression(context);
    else
        expr = parseExpression(context);
    failIfFalse(expr, "Cannot parse subject for-", isOfEnumeration ? "of" : "in", " statement");
    recordPauseLocation(context.breakpointLocation(expr));
    JSTextPosition exprEnd = lastTokenEndPosition();
    int endLine = tokenLine();
    
    handleProductionOrFail(CLOSEPAREN, ")", "end", (isOfEnumeration ? "for-of header" : "for-in header"));
    const Identifier* unused = nullptr;
    startLoop();
    TreeStatement statement = parseStatement(context, unused);
    endLoop();
    failIfFalse(statement, "Expected a statement as the body of a for-", isOfEnumeration ? "of" : "in", "loop");
    gatherLexicalVariablesIfNecessary();
    TreeStatement result;
    if (pattern) {
        ASSERT(!decls);
        if (isOfEnumeration)
            result = context.createForOfLoop(isAwaitFor, location, pattern, expr, statement, declLocation, declsStart, declsEnd, exprEnd, startLine, endLine, *lexicalVariables);
        else {
            ASSERT(!isAwaitFor);
            result = context.createForInLoop(location, pattern, expr, statement, declLocation, declsStart, declsEnd, exprEnd, startLine, endLine, *lexicalVariables);
        }

        popLexicalScopeIfNecessary();
        return result;
    }

    semanticFailIfFalse(isSimpleAssignmentTarget(context, decls), "Left side of assignment is not a reference");
    if (isOfEnumeration)
        result = context.createForOfLoop(isAwaitFor, location, decls, expr, statement, declLocation, declsStart, declsEnd, exprEnd, startLine, endLine, *lexicalVariables);
    else {
        ASSERT(!isAwaitFor);
        result = context.createForInLoop(location, decls, expr, statement, declLocation, declsStart, declsEnd, exprEnd, startLine, endLine, *lexicalVariables);
    }
    popLexicalScopeIfNecessary();
    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseBreakStatement(TreeBuilder& context)
{
    ASSERT(match(BREAK));
    JSTokenLocation location(tokenLocation());
    JSTextPosition start = tokenStartPosition();
    JSTextPosition end = tokenEndPosition();
    next();
    
    if (autoSemiColon()) {
        semanticFailIfFalse(breakIsValid(), "'break' is only valid inside a switch or loop statement");
        return context.createBreakStatement(location, &m_vm.propertyNames->nullIdentifier, start, end);
    }
    failIfFalse(matchSpecIdentifier(), "Expected an identifier as the target for a break statement");
    const Identifier* ident = m_token.m_data.ident;
    semanticFailIfFalse(getLabel(ident), "Cannot use the undeclared label '", ident->impl(), "'");
    end = tokenEndPosition();
    next();
    failIfFalse(autoSemiColon(), "Expected a ';' following a targeted break statement");
    return context.createBreakStatement(location, ident, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseContinueStatement(TreeBuilder& context)
{
    ASSERT(match(CONTINUE));
    JSTokenLocation location(tokenLocation());
    JSTextPosition start = tokenStartPosition();
    JSTextPosition end = tokenEndPosition();
    next();
    
    if (autoSemiColon()) {
        semanticFailIfFalse(continueIsValid(), "'continue' is only valid inside a loop statement");
        return context.createContinueStatement(location, &m_vm.propertyNames->nullIdentifier, start, end);
    }
    failIfFalse(matchSpecIdentifier(), "Expected an identifier as the target for a continue statement");
    const Identifier* ident = m_token.m_data.ident;
    ScopeLabelInfo* label = getLabel(ident);
    semanticFailIfFalse(label, "Cannot use the undeclared label '", ident->impl(), "'");
    semanticFailIfFalse(label->isLoop, "Cannot continue to the label '", ident->impl(), "' as it is not targeting a loop");
    end = tokenEndPosition();
    next();
    failIfFalse(autoSemiColon(), "Expected a ';' following a targeted continue statement");
    return context.createContinueStatement(location, ident, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseReturnStatement(TreeBuilder& context)
{
    ASSERT(match(RETURN));
    JSTokenLocation location(tokenLocation());
    semanticFailIfFalse(currentScope()->isFunction(), "Return statements are only valid inside functions");
    JSTextPosition start = tokenStartPosition();
    JSTextPosition end = tokenEndPosition();
    next();
    // We do the auto semicolon check before attempting to parse expression
    // as we need to ensure the a line break after the return correctly terminates
    // the statement
    if (match(SEMICOLON))
        end = tokenEndPosition();

    if (autoSemiColon())
        return context.createReturnStatement(location, 0, start, end);
    TreeExpression expr = parseExpression(context, IsOnlyChildOfStatement::Yes);
    failIfFalse(expr, "Cannot parse the return expression");
    end = lastTokenEndPosition();
    if (match(SEMICOLON))
        end  = tokenEndPosition();
    if (!autoSemiColon())
        failWithMessage("Expected a ';' following a return statement");
    return context.createReturnStatement(location, expr, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseThrowStatement(TreeBuilder& context)
{
    ASSERT(match(THROW));
    JSTokenLocation location(tokenLocation());
    JSTextPosition start = tokenStartPosition();
    next();
    failIfTrue(match(SEMICOLON), "Expected expression after 'throw'");
    semanticFailIfTrue(autoSemiColon(), "Cannot have a newline after 'throw'");
    
    TreeExpression expr = parseExpression(context, IsOnlyChildOfStatement::Yes);
    failIfFalse(expr, "Cannot parse expression for throw statement");
    JSTextPosition end = lastTokenEndPosition();
    failIfFalse(autoSemiColon(), "Expected a ';' after a throw statement");
    
    return context.createThrowStatement(location, expr, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseWithStatement(TreeBuilder& context)
{
    ASSERT(match(WITH));
    JSTokenLocation location(tokenLocation());
    semanticFailIfTrue(strictMode(), "'with' statements are not valid in strict mode");
    currentScope()->setNeedsFullActivation();
    int startLine = tokenLine();
    next();

    handleProductionOrFail(OPENPAREN, "(", "start", "subject of a 'with' statement");
    int start = tokenStart();
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr, "Cannot parse 'with' subject expression");
    recordPauseLocation(context.breakpointLocation(expr));
    JSTextPosition end = lastTokenEndPosition();
    int endLine = tokenLine();
    handleProductionOrFail(CLOSEPAREN, ")", "start", "subject of a 'with' statement");
    const Identifier* unused = nullptr;
    TreeStatement statement = parseStatement(context, unused);
    failIfFalse(statement, "A 'with' statement must have a body");
    
    return context.createWithStatement(location, expr, statement, start, end, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseSwitchStatement(TreeBuilder& context)
{
    ASSERT(match(SWITCH));
    JSTokenLocation location(tokenLocation());
    int startLine = tokenLine();
    next();
    handleProductionOrFail(OPENPAREN, "(", "start", "subject of a 'switch'");
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr, "Cannot parse switch subject expression");
    recordPauseLocation(context.breakpointLocation(expr));
    int endLine = tokenLine();
    
    handleProductionOrFail(CLOSEPAREN, ")", "end", "subject of a 'switch'");
    handleProductionOrFail(OPENBRACE, "{", "start", "body of a 'switch'");
    AutoPopScopeRef lexicalScope(this, pushScope());
    lexicalScope->setIsLexicalScope();
    lexicalScope->preventVarDeclarations();
    startSwitch();
    TreeClauseList firstClauses = parseSwitchClauses(context);
    propagateError();
    
    TreeClause defaultClause = parseSwitchDefaultClause(context);
    propagateError();
    
    TreeClauseList secondClauses = parseSwitchClauses(context);
    propagateError();
    endSwitch();
    handleProductionOrFail(CLOSEBRACE, "}", "end", "body of a 'switch'");
    
    TreeStatement result = context.createSwitchStatement(location, expr, firstClauses, defaultClause, secondClauses, startLine, endLine, lexicalScope->finalizeLexicalEnvironment(), lexicalScope->takeFunctionDeclarations());
    popScope(lexicalScope, TreeBuilder::NeedsFreeVariableInfo);
    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeClauseList Parser<LexerType>::parseSwitchClauses(TreeBuilder& context)
{
    if (!match(CASE))
        return 0;
    unsigned startOffset = tokenStart();
    next();
    TreeExpression condition = parseExpression(context);
    failIfFalse(condition, "Cannot parse switch clause");
    consumeOrFail(COLON, "Expected a ':' after switch clause expression");
    TreeSourceElements statements = parseSourceElements(context, DontCheckForStrictMode);
    failIfFalse(statements, "Cannot parse the body of a switch clause");
    TreeClause clause = context.createClause(condition, statements);
    context.setStartOffset(clause, startOffset);
    TreeClauseList clauseList = context.createClauseList(clause);
    TreeClauseList tail = clauseList;
    
    while (match(CASE)) {
        startOffset = tokenStart();
        next();
        TreeExpression condition = parseExpression(context);
        failIfFalse(condition, "Cannot parse switch case expression");
        consumeOrFail(COLON, "Expected a ':' after switch clause expression");
        TreeSourceElements statements = parseSourceElements(context, DontCheckForStrictMode);
        failIfFalse(statements, "Cannot parse the body of a switch clause");
        clause = context.createClause(condition, statements);
        context.setStartOffset(clause, startOffset);
        tail = context.createClauseList(tail, clause);
    }
    return clauseList;
}

template <typename LexerType>
template <class TreeBuilder> TreeClause Parser<LexerType>::parseSwitchDefaultClause(TreeBuilder& context)
{
    if (!match(DEFAULT))
        return 0;
    unsigned startOffset = tokenStart();
    next();
    consumeOrFail(COLON, "Expected a ':' after switch default clause");
    TreeSourceElements statements = parseSourceElements(context, DontCheckForStrictMode);
    failIfFalse(statements, "Cannot parse the body of a switch default clause");
    TreeClause result = context.createClause(0, statements);
    context.setStartOffset(result, startOffset);
    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseTryStatement(TreeBuilder& context)
{
    ASSERT(match(TRY));
    JSTokenLocation location(tokenLocation());
    TreeStatement tryBlock = 0;
    TreeDestructuringPattern catchPattern = 0;
    TreeStatement catchBlock = 0;
    TreeStatement finallyBlock = 0;
    int firstLine = tokenLine();
    next();
    matchOrFail(OPENBRACE, "Expected a block statement as body of a try statement");
    
    tryBlock = parseBlockStatement(context);
    failIfFalse(tryBlock, "Cannot parse the body of try block");
    int lastLine = m_lastTokenEndPosition.line;
    VariableEnvironment catchEnvironment; 
    if (match(CATCH)) {
        next();
        
        if (match(OPENBRACE)) {
            catchBlock = parseBlockStatement(context);
            failIfFalse(catchBlock, "Unable to parse 'catch' block");
        } else {
            handleProductionOrFail(OPENPAREN, "(", "start", "'catch' target");
            DepthManager statementDepth(&m_statementDepth);
            m_statementDepth++;
            AutoPopScopeRef catchScope(this, pushScope());
            catchScope->setIsLexicalScope();
            catchScope->preventVarDeclarations();
            const Identifier* ident = nullptr;
            if (matchSpecIdentifier()) {
                catchScope->setIsSimpleCatchParameterScope();
                ident = m_token.m_data.ident;
                catchPattern = context.createBindingLocation(m_token.m_location, *ident, m_token.m_startPosition, m_token.m_endPosition, AssignmentContext::DeclarationStatement);
                next();
                failIfTrueIfStrict(catchScope->declareLexicalVariable(ident, false) & DeclarationResult::InvalidStrictMode, "Cannot declare a catch variable named '", ident->impl(), "' in strict mode");
            } else {
                catchPattern = parseDestructuringPattern(context, DestructuringKind::DestructureToCatchParameters, ExportType::NotExported);
                failIfFalse(catchPattern, "Cannot parse this destructuring pattern");
            }
            handleProductionOrFail(CLOSEPAREN, ")", "end", "'catch' target");
            matchOrFail(OPENBRACE, "Expected exception handler to be a block statement");
            constexpr bool isCatchBlock = true;
            catchBlock = parseBlockStatement(context, isCatchBlock);
            failIfFalse(catchBlock, "Unable to parse 'catch' block");
            catchEnvironment = catchScope->finalizeLexicalEnvironment();
            RELEASE_ASSERT(!ident || (catchEnvironment.size() == 1 && catchEnvironment.contains(ident->impl())));
            popScope(catchScope, TreeBuilder::NeedsFreeVariableInfo);
        }
    }
    
    if (match(FINALLY)) {
        next();
        matchOrFail(OPENBRACE, "Expected block statement for finally body");
        finallyBlock = parseBlockStatement(context);
        failIfFalse(finallyBlock, "Cannot parse finally body");
    }
    failIfFalse(catchBlock || finallyBlock, "Try statements must have at least a catch or finally block");
    return context.createTryStatement(location, tryBlock, catchPattern, catchBlock, finallyBlock, firstLine, lastLine, catchEnvironment);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseDebuggerStatement(TreeBuilder& context)
{
    ASSERT(match(DEBUGGER));
    JSTokenLocation location(tokenLocation());
    int startLine = tokenLine();
    int endLine = startLine;
    next();
    if (match(SEMICOLON))
        startLine = tokenLine();
    failIfFalse(autoSemiColon(), "Debugger keyword must be followed by a ';'");
    return context.createDebugger(location, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseBlockStatement(TreeBuilder& context, bool isCatchBlock)
{
    ASSERT(match(OPENBRACE));

    // We should treat the first block statement of the function (the body of the function) as the lexical 
    // scope of the function itself, and not the lexical scope of a 'block' statement within the function.
    AutoCleanupLexicalScope lexicalScope;
    bool shouldPushLexicalScope = m_statementDepth > 0;
    if (shouldPushLexicalScope) {
        ScopeRef newScope = pushScope();
        newScope->setIsLexicalScope();
        newScope->preventVarDeclarations();
        if (isCatchBlock)
            newScope->setIsCatchBlockScope();
        lexicalScope.setIsValid(newScope, this);
    }
    JSTokenLocation location(tokenLocation());
    int startOffset = m_token.m_data.offset;
    int start = tokenLine();
    VariableEnvironment emptyEnvironment;
    DeclarationStacks::FunctionStack emptyFunctionStack;
    next();
    if (match(CLOSEBRACE)) {
        int endOffset = m_token.m_data.offset;
        next();
        TreeStatement result = context.createBlockStatement(location, 0, start, m_lastTokenEndPosition.line, shouldPushLexicalScope ? currentScope()->finalizeLexicalEnvironment() : emptyEnvironment, shouldPushLexicalScope ? currentScope()->takeFunctionDeclarations() : WTFMove(emptyFunctionStack));
        context.setStartOffset(result, startOffset);
        context.setEndOffset(result, endOffset);
        if (shouldPushLexicalScope)
            popScope(lexicalScope, TreeBuilder::NeedsFreeVariableInfo);
        return result;
    }
    TreeSourceElements subtree = parseSourceElements(context, DontCheckForStrictMode);
    failIfFalse(subtree, "Cannot parse the body of the block statement");
    matchOrFail(CLOSEBRACE, "Expected a closing '}' at the end of a block statement");
    int endOffset = m_token.m_data.offset;
    next();
    TreeStatement result = context.createBlockStatement(location, subtree, start, m_lastTokenEndPosition.line, shouldPushLexicalScope ? currentScope()->finalizeLexicalEnvironment() : emptyEnvironment, shouldPushLexicalScope ? currentScope()->takeFunctionDeclarations() : WTFMove(emptyFunctionStack));
    context.setStartOffset(result, startOffset);
    context.setEndOffset(result, endOffset);
    if (shouldPushLexicalScope)
        popScope(lexicalScope, TreeBuilder::NeedsFreeVariableInfo);

    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseStatement(TreeBuilder& context, const Identifier*& directive, unsigned* directiveLiteralLength)
{
    DepthManager statementDepth(&m_statementDepth);
    m_statementDepth++;
    int nonTrivialExpressionCount = 0;
    failIfStackOverflow();
    TreeStatement result = 0;
    bool shouldSetEndOffset = true;
    bool shouldSetPauseLocation = false;
    bool parentAllowsFunctionDeclarationAsStatement = m_immediateParentAllowsFunctionDeclarationInStatement;
    m_immediateParentAllowsFunctionDeclarationInStatement = false;

    switch (m_token.m_type) {
    case OPENBRACE:
        result = parseBlockStatement(context);
        shouldSetEndOffset = false;
        break;
    case VAR:
        result = parseVariableDeclaration(context, DeclarationType::VarDeclaration);
        shouldSetPauseLocation = true;
        break;
    case FUNCTION: {
        result = parseFunctionDeclarationStatement(context, parentAllowsFunctionDeclarationAsStatement);
        break;
    }
    case SEMICOLON: {
        JSTokenLocation location(tokenLocation());
        next();
        result = context.createEmptyStatement(location);
        shouldSetPauseLocation = true;
        break;
    }
    case IF:
        result = parseIfStatement(context);
        break;
    case DO:
        result = parseDoWhileStatement(context);
        break;
    case WHILE:
        result = parseWhileStatement(context);
        break;
    case FOR:
        result = parseForStatement(context);
        break;
    case CONTINUE:
        result = parseContinueStatement(context);
        shouldSetPauseLocation = true;
        break;
    case BREAK:
        result = parseBreakStatement(context);
        shouldSetPauseLocation = true;
        break;
    case RETURN:
        result = parseReturnStatement(context);
        shouldSetPauseLocation = true;
        break;
    case WITH:
        result = parseWithStatement(context);
        break;
    case SWITCH:
        result = parseSwitchStatement(context);
        break;
    case THROW:
        result = parseThrowStatement(context);
        shouldSetPauseLocation = true;
        break;
    case TRY:
        result = parseTryStatement(context);
        break;
    case DEBUGGER:
        result = parseDebuggerStatement(context);
        shouldSetPauseLocation = true;
        break;
    case EOFTOK:
    case CASE:
    case CLOSEBRACE:
    case DEFAULT:
        // These tokens imply the end of a set of source elements
        return 0;
    case LET: {
        // https://tc39.es/ecma262/#sec-expression-statement
        // ExpressionStatement's lookahead includes `let [` sequence.
        SavePoint savePoint = createSavePoint(context);
        next();
        failIfTrue(match(OPENBRACKET), "Cannot use lexical declaration in single-statement context");
        restoreSavePoint(context, savePoint);
        if (!strictMode())
            goto identcase;
        goto defaultCase;
    }
    case IDENT:
        if (UNLIKELY(*m_token.m_data.ident == m_vm.propertyNames->async && !m_token.m_data.escaped)) {
            SavePoint savePoint = createSavePoint(context);
            next();
            failIfTrue(match(FUNCTION) && !m_lexer->hasLineTerminatorBeforeToken(), "Cannot use async function declaration in single-statement context");
            restoreSavePoint(context, savePoint);
        }
        FALLTHROUGH;
    case AWAIT:
    case YIELD: {
        identcase:
        bool allowFunctionDeclarationAsStatement = false;
        result = parseExpressionOrLabelStatement(context, allowFunctionDeclarationAsStatement);
        shouldSetPauseLocation = !context.shouldSkipPauseLocation(result);
        break;
    }
    case STRING:
        directive = m_token.m_data.ident;
        if (directiveLiteralLength)
            *directiveLiteralLength = m_token.m_location.endOffset - m_token.m_location.startOffset;
        nonTrivialExpressionCount = m_parserState.nonTrivialExpressionCount;
        FALLTHROUGH;
    default:
        defaultCase:
        TreeStatement exprStatement = parseExpressionStatement(context);
        if (directive && nonTrivialExpressionCount != m_parserState.nonTrivialExpressionCount)
            directive = nullptr;
        result = exprStatement;
        shouldSetPauseLocation = true;
        break;
    }

    if (result) {
        if (shouldSetEndOffset)
            context.setEndOffset(result, m_lastTokenEndPosition.offset);
        if (shouldSetPauseLocation)
            recordPauseLocation(context.breakpointLocation(result));
    }

    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseFunctionDeclarationStatement(TreeBuilder& context, bool parentAllowsFunctionDeclarationAsStatement)
{
    semanticFailIfTrue(strictMode(), "Function declarations are only allowed inside blocks or switch statements in strict mode");
    failIfFalse(parentAllowsFunctionDeclarationAsStatement, "Function declarations are only allowed inside block statements or at the top level of a program");
    if (!currentScope()->isFunction() && !closestParentOrdinaryFunctionNonLexicalScope()->isEvalContext()) {
        // We only implement annex B.3.3 if we're in function mode or eval mode. Otherwise, we fall back
        // to hoisting behavior.
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=155813
        DepthManager statementDepth(&m_statementDepth);
        m_statementDepth = 1;
        return parseFunctionDeclaration(context, FunctionDeclarationType::Statement);
    }

    // Any function declaration that isn't in a block is a syntax error unless it's
    // in an if/else statement. If it's in an if/else statement, we will magically
    // treat it as if the if/else statement is inside a block statement.
    // to the very top like a "var". For example:
    // function a() {
    //     if (cond) function foo() { }
    // }
    // will be rewritten as:
    // function a() {
    //     if (cond) { function foo() { } }
    // }
    AutoPopScopeRef blockScope(this, pushScope());
    blockScope->setIsLexicalScope();
    blockScope->preventVarDeclarations();
    JSTokenLocation location(tokenLocation());
    int start = tokenLine();

    TreeStatement function = parseFunctionDeclaration(context, FunctionDeclarationType::Statement);
    propagateError();
    failIfFalse(function, "Expected valid function statement after 'function' keyword");
    TreeSourceElements sourceElements = context.createSourceElements();
    context.appendStatement(sourceElements, function);
    TreeStatement result = context.createBlockStatement(location, sourceElements, start, m_lastTokenEndPosition.line, currentScope()->finalizeLexicalEnvironment(), currentScope()->takeFunctionDeclarations());
    popScope(blockScope, TreeBuilder::NeedsFreeVariableInfo);
    return result;
}

template <typename LexerType>
template <class TreeBuilder> bool Parser<LexerType>::parseFormalParameters(TreeBuilder& context, TreeFormalParameterList list, bool isArrowFunction, bool isMethod, unsigned& parameterCount)
{
#define failIfDuplicateIfViolation() \
    if (duplicateParameter) {\
        semanticFailIfTrue(hasDefaultParameterValues, "Duplicate parameter '", duplicateParameter->impl(), "' not allowed in function with default parameter values");\
        semanticFailIfTrue(hasDestructuringPattern, "Duplicate parameter '", duplicateParameter->impl(), "' not allowed in function with destructuring parameters");\
        semanticFailIfTrue(isRestParameter, "Duplicate parameter '", duplicateParameter->impl(), "' not allowed in function with a rest parameter");\
        semanticFailIfTrue(isArrowFunction, "Duplicate parameter '", duplicateParameter->impl(), "' not allowed in an arrow function");\
        semanticFailIfTrue(isMethod, "Duplicate parameter '", duplicateParameter->impl(), "' not allowed in a method");\
    }

    bool hasDefaultParameterValues = false;
    bool hasDestructuringPattern = false;
    bool isRestParameter = false;
    const Identifier* duplicateParameter = nullptr;
    unsigned restParameterStart = 0;
    do {
        TreeDestructuringPattern parameter = 0;
        TreeExpression defaultValue = 0;

        if (UNLIKELY(match(CLOSEPAREN)))
            break;
        
        if (match(DOTDOTDOT)) {
            next();
            semanticFailIfTrue(isDisallowedIdentifierAwait(m_token), "Cannot use 'await' as a parameter name in an async function");
            TreeDestructuringPattern destructuringPattern = parseDestructuringPattern(context, DestructuringKind::DestructureToParameters, ExportType::NotExported, &duplicateParameter, &hasDestructuringPattern);
            propagateError();
            parameter = context.createRestParameter(destructuringPattern, restParameterStart);
            failIfTrue(match(COMMA), "Rest parameter should be the last parameter in a function declaration"); // Let's have a good error message for this common case.
            isRestParameter = true;
        } else
            parameter = parseDestructuringPattern(context, DestructuringKind::DestructureToParameters, ExportType::NotExported, &duplicateParameter, &hasDestructuringPattern);
        failIfFalse(parameter, "Cannot parse parameter pattern");
        if (!isRestParameter) {
            defaultValue = parseDefaultValueForDestructuringPattern(context);
            if (defaultValue)
                hasDefaultParameterValues = true;
        }
        propagateError();
        failIfDuplicateIfViolation();
        if (isRestParameter || defaultValue || hasDestructuringPattern)
            currentScope()->setHasNonSimpleParameterList();
        context.appendParameter(list, parameter, defaultValue);
        if (!isRestParameter) {
            restParameterStart++;
            if (!hasDefaultParameterValues)
                parameterCount++;
        }
    } while (!isRestParameter && consume(COMMA));

    return true;
#undef failIfDuplicateIfViolation
}

template <typename LexerType>
template <class TreeBuilder> TreeFunctionBody Parser<LexerType>::parseFunctionBody(
    TreeBuilder& context, SyntaxChecker& syntaxChecker, const JSTokenLocation& startLocation, int startColumn, int functionKeywordStart, int functionNameStart, int parametersStart,
    ConstructorKind constructorKind, SuperBinding superBinding, FunctionBodyType bodyType, unsigned parameterCount)
{
    SetForScope<bool> overrideParsingClassFieldInitializer(m_parserState.isParsingClassFieldInitializer, bodyType == StandardFunctionBodyBlock ? false : m_parserState.isParsingClassFieldInitializer);
    bool isArrowFunctionBodyExpression = bodyType == ArrowFunctionBodyExpression;
    if (!isArrowFunctionBodyExpression) {
        next();
        if (match(CLOSEBRACE)) {
            unsigned endColumn = tokenColumn();
            SuperBinding functionSuperBinding = adjustSuperBindingForBaseConstructor(constructorKind, superBinding, currentScope());
            return context.createFunctionMetadata(startLocation, tokenLocation(), startColumn, endColumn, functionKeywordStart, functionNameStart, parametersStart, strictMode(), constructorKind, functionSuperBinding, parameterCount, sourceParseMode(), isArrowFunctionBodyExpression);
        }
    }

    DepthManager statementDepth(&m_statementDepth);
    m_statementDepth = 0;
    if (bodyType == ArrowFunctionBodyExpression) {
        if (m_debuggerParseData)
            failIfFalse(parseArrowFunctionSingleExpressionBodySourceElements(context), "Cannot parse body of this arrow function");
        else
            failIfFalse(parseArrowFunctionSingleExpressionBodySourceElements(syntaxChecker), "Cannot parse body of this arrow function");
    } else {
        if (m_debuggerParseData)
            failIfFalse(parseSourceElements(context, CheckForStrictMode), bodyType == StandardFunctionBodyBlock ? "Cannot parse body of this function" : "Cannot parse body of this arrow function");
        else
            failIfFalse(parseSourceElements(syntaxChecker, CheckForStrictMode), bodyType == StandardFunctionBodyBlock ? "Cannot parse body of this function" : "Cannot parse body of this arrow function");
    }
    unsigned endColumn = tokenColumn();
    SuperBinding functionSuperBinding = adjustSuperBindingForBaseConstructor(constructorKind, superBinding, currentScope());
    return context.createFunctionMetadata(startLocation, tokenLocation(), startColumn, endColumn, functionKeywordStart, functionNameStart, parametersStart, strictMode(), constructorKind, functionSuperBinding, parameterCount, sourceParseMode(), isArrowFunctionBodyExpression);
}

static const char* stringArticleForFunctionMode(SourceParseMode mode)
{
    switch (mode) {
    case SourceParseMode::GetterMode:
    case SourceParseMode::SetterMode:
    case SourceParseMode::NormalFunctionMode:
    case SourceParseMode::MethodMode:
    case SourceParseMode::GeneratorBodyMode:
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GeneratorWrapperMethodMode:
        return "a ";
    case SourceParseMode::ArrowFunctionMode:
    case SourceParseMode::AsyncFunctionMode:
    case SourceParseMode::AsyncFunctionBodyMode:
    case SourceParseMode::AsyncMethodMode:
    case SourceParseMode::AsyncArrowFunctionBodyMode:
    case SourceParseMode::AsyncArrowFunctionMode:
    case SourceParseMode::AsyncGeneratorWrapperFunctionMode:
    case SourceParseMode::AsyncGeneratorBodyMode:
    case SourceParseMode::AsyncGeneratorWrapperMethodMode:
        return "an ";
    case SourceParseMode::ProgramMode:
    case SourceParseMode::ModuleAnalyzeMode:
    case SourceParseMode::ModuleEvaluateMode:
    case SourceParseMode::ClassFieldInitializerMode:
        RELEASE_ASSERT_NOT_REACHED();
        return "";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}
    
static const char* stringForFunctionMode(SourceParseMode mode)
{
    switch (mode) {
    case SourceParseMode::GetterMode:
        return "getter";
    case SourceParseMode::SetterMode:
        return "setter";
    case SourceParseMode::NormalFunctionMode:
        return "function";
    case SourceParseMode::MethodMode:
        return "method";
    case SourceParseMode::GeneratorBodyMode:
        return "generator";
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GeneratorWrapperMethodMode:
        return "generator function";
    case SourceParseMode::ArrowFunctionMode:
        return "arrow function";
    case SourceParseMode::AsyncFunctionMode:
    case SourceParseMode::AsyncFunctionBodyMode:
        return "async function";
    case SourceParseMode::AsyncMethodMode:
        return "async method";
    case SourceParseMode::AsyncArrowFunctionBodyMode:
    case SourceParseMode::AsyncArrowFunctionMode:
        return "async arrow function";
    case SourceParseMode::AsyncGeneratorWrapperFunctionMode:
    case SourceParseMode::AsyncGeneratorBodyMode:
        return "async generator function";
    case SourceParseMode::AsyncGeneratorWrapperMethodMode:
        return "async generator method";
    case SourceParseMode::ProgramMode:
    case SourceParseMode::ModuleAnalyzeMode:
    case SourceParseMode::ModuleEvaluateMode:
    case SourceParseMode::ClassFieldInitializerMode:
        RELEASE_ASSERT_NOT_REACHED();
        return "";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

template <typename LexerType> template <class TreeBuilder, class FunctionInfoType> typename TreeBuilder::FormalParameterList Parser<LexerType>::parseFunctionParameters(TreeBuilder& context, FunctionInfoType& functionInfo)
{
    auto mode = sourceParseMode();
    RELEASE_ASSERT(!(SourceParseModeSet(SourceParseMode::ProgramMode, SourceParseMode::ModuleAnalyzeMode, SourceParseMode::ModuleEvaluateMode).contains(mode)));
    TreeFormalParameterList parameterList = context.createFormalParameterList();
    SetForScope<FunctionParsePhase> functionParsePhasePoisoner(m_parserState.functionParsePhase, FunctionParsePhase::Parameters);
    
    if (UNLIKELY((SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(mode)))) {
        if (!matchSpecIdentifier() && !match(OPENPAREN)) {
            semanticFailureDueToKeyword(stringForFunctionMode(mode), " name");
            failWithMessage("Expected an arrow function input parameter");
        } else {
            if (match(OPENPAREN)) {
                next();
                
                if (match(CLOSEPAREN)) {
                    functionInfo.parameterCount = 0;
                } else {
                    bool isArrowFunction = true;
                    bool isMethod = false;
                    failIfFalse(parseFormalParameters(context, parameterList, isArrowFunction, isMethod, functionInfo.parameterCount), "Cannot parse parameters for this ", stringForFunctionMode(mode));
                }
                
                consumeOrFail(CLOSEPAREN, "Expected a ')' or a ',' after a parameter declaration");
            } else {
                functionInfo.parameterCount = 1;
                auto parameter = parseDestructuringPattern(context, DestructuringKind::DestructureToParameters, ExportType::NotExported);
                failIfFalse(parameter, "Cannot parse parameter pattern");
                context.appendParameter(parameterList, parameter, 0);
            }
        }

        return parameterList;
    }

    if (!consume(OPENPAREN)) {
        semanticFailureDueToKeyword(stringForFunctionMode(mode), " name");
        failWithMessage("Expected an opening '(' before a ", stringForFunctionMode(mode), "'s parameter list");
    }

    if (mode == SourceParseMode::GetterMode) {
        consumeOrFail(CLOSEPAREN, "getter functions must have no parameters");
        functionInfo.parameterCount = 0;
    } else if (mode == SourceParseMode::SetterMode) {
        failIfTrue(match(CLOSEPAREN), "setter functions must have one parameter");
        const Identifier* duplicateParameter = nullptr;
        bool hasDestructuringPattern = false;
        auto parameter = parseDestructuringPattern(context, DestructuringKind::DestructureToParameters, ExportType::NotExported, &duplicateParameter, &hasDestructuringPattern);
        failIfFalse(parameter, "setter functions must have one parameter");
        auto defaultValue = parseDefaultValueForDestructuringPattern(context);
        propagateError();
        if (defaultValue || hasDestructuringPattern) {
            semanticFailIfTrue(duplicateParameter, "Duplicate parameter '", duplicateParameter->impl(), "' not allowed in function with non-simple parameter list");
            currentScope()->setHasNonSimpleParameterList();
        }
        context.appendParameter(parameterList, parameter, defaultValue);
        functionInfo.parameterCount = defaultValue ? 0 : 1;
        failIfTrue(match(COMMA), "setter functions must have one parameter");
        consumeOrFail(CLOSEPAREN, "Expected a ')' after a parameter declaration");
    } else {
        if (match(CLOSEPAREN)) {
            functionInfo.parameterCount = 0;
        } else {
            bool isArrowFunction = false;
            bool isMethod = isMethodParseMode(mode);
            failIfFalse(parseFormalParameters(context, parameterList, isArrowFunction, isMethod, functionInfo.parameterCount), "Cannot parse parameters for this ", stringForFunctionMode(mode));
        }
        consumeOrFail(CLOSEPAREN, "Expected a ')' or a ',' after a parameter declaration");
    }

    return parameterList;
}

template <typename LexerType>
template <class TreeBuilder> typename TreeBuilder::FormalParameterList Parser<LexerType>::createGeneratorParameters(TreeBuilder& context, unsigned& parameterCount)
{
    auto parameters = context.createFormalParameterList();

    JSTokenLocation location(tokenLocation());
    JSTextPosition position = tokenStartPosition();

    auto addParameter = [&](const Identifier& name) {
        declareParameter(&name);
        auto binding = context.createBindingLocation(location, name, position, position, AssignmentContext::DeclarationStatement);
        context.appendParameter(parameters, binding, 0);
        ++parameterCount;
    };

    // @generator
    addParameter(m_vm.propertyNames->generatorPrivateName);
    // @generatorState
    addParameter(m_vm.propertyNames->generatorStatePrivateName);
    // @generatorValue
    addParameter(m_vm.propertyNames->generatorValuePrivateName);
    // @generatorResumeMode
    addParameter(m_vm.propertyNames->generatorResumeModePrivateName);
    // @generatorFrame
    addParameter(m_vm.propertyNames->generatorFramePrivateName);

    return parameters;
}

template <typename LexerType>
template <class TreeBuilder> bool Parser<LexerType>::parseFunctionInfo(TreeBuilder& context, FunctionNameRequirements requirements, bool nameIsInContainingScope, ConstructorKind constructorKind, SuperBinding expectedSuperBinding, int functionKeywordStart, ParserFunctionInfo<TreeBuilder>& functionInfo, FunctionDefinitionType functionDefinitionType, Optional<int> functionConstructorParametersEndPosition)
{
    auto mode = sourceParseMode();
    RELEASE_ASSERT(isFunctionParseMode(mode));

    ScopeRef parentScope = currentScope();

    bool functionNameIsAwait = isPossiblyEscapedAwait(m_token);
    const char* isDisallowedAwaitFunctionNameReason = functionNameIsAwait && !canUseIdentifierAwait() ? disallowedIdentifierAwaitReason() : nullptr;

    AutoPopScopeRef functionScope(this, pushScope());
    functionScope->setSourceParseMode(mode);
    functionScope->setExpectedSuperBinding(expectedSuperBinding);
    functionScope->setConstructorKind(constructorKind);
    SetForScope<FunctionParsePhase> functionParsePhasePoisoner(m_parserState.functionParsePhase, FunctionParsePhase::Body);
    int functionNameStart = m_token.m_location.startOffset;
    const Identifier* lastFunctionName = m_parserState.lastFunctionName;
    m_parserState.lastFunctionName = nullptr;
    int parametersStart = -1;
    JSTokenLocation startLocation;
    int startColumn = -1;
    FunctionBodyType functionBodyType;

    auto loadCachedFunction = [&] () -> bool {
        if (UNLIKELY(!Options::useSourceProviderCache()))
            return false;

        if (UNLIKELY(m_debuggerParseData))
            return false;

        ASSERT(parametersStart != -1);
        ASSERT(startColumn != -1);

        // If we know about this function already, we can use the cached info and skip the parser to the end of the function.
        if (const SourceProviderCacheItem* cachedInfo = TreeBuilder::CanUseFunctionCache ? findCachedFunctionInfo(parametersStart) : nullptr) {
            // If we're in a strict context, the cached function info must say it was strict too.
            ASSERT(!strictMode() || cachedInfo->strictMode);
            JSTokenLocation endLocation;

            ConstructorKind constructorKind = static_cast<ConstructorKind>(cachedInfo->constructorKind);
            SuperBinding expectedSuperBinding = static_cast<SuperBinding>(cachedInfo->expectedSuperBinding);

            endLocation.line = cachedInfo->lastTokenLine;
            endLocation.startOffset = cachedInfo->lastTokenStartOffset;
            endLocation.lineStartOffset = cachedInfo->lastTokenLineStartOffset;
            ASSERT(endLocation.startOffset >= endLocation.lineStartOffset);

            bool endColumnIsOnStartLine = endLocation.line == functionInfo.startLine;
            unsigned currentLineStartOffset = m_lexer->currentLineStartOffset();
            unsigned bodyEndColumn = endColumnIsOnStartLine ? endLocation.startOffset - currentLineStartOffset : endLocation.startOffset - endLocation.lineStartOffset;

            ASSERT(endLocation.startOffset >= endLocation.lineStartOffset);
            
            FunctionBodyType functionBodyType;
            if (UNLIKELY(SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(mode)))
                functionBodyType = cachedInfo->isBodyArrowExpression ?  ArrowFunctionBodyExpression : ArrowFunctionBodyBlock;
            else
                functionBodyType = StandardFunctionBodyBlock;

            SuperBinding functionSuperBinding = adjustSuperBindingForBaseConstructor(constructorKind, expectedSuperBinding, cachedInfo->needsSuperBinding, cachedInfo->usesEval, cachedInfo->innerArrowFunctionFeatures);

            functionInfo.body = context.createFunctionMetadata(
                startLocation, endLocation, startColumn, bodyEndColumn, 
                functionKeywordStart, functionNameStart, parametersStart, 
                cachedInfo->strictMode, constructorKind, functionSuperBinding,
                cachedInfo->parameterCount,
                mode, functionBodyType == ArrowFunctionBodyExpression);
            functionInfo.endOffset = cachedInfo->endFunctionOffset;
            functionInfo.parameterCount = cachedInfo->parameterCount;

            functionScope->restoreFromSourceProviderCache(cachedInfo);
            popScope(functionScope, TreeBuilder::NeedsFreeVariableInfo);
            
            m_token = cachedInfo->endFunctionToken();

            if (endColumnIsOnStartLine)
                m_token.m_location.lineStartOffset = currentLineStartOffset;

            m_lexer->setOffset(m_token.m_location.endOffset, m_token.m_location.lineStartOffset);
            m_lexer->setLineNumber(m_token.m_location.line);

            switch (functionBodyType) {
            case ArrowFunctionBodyExpression:
                next();
                context.setEndOffset(functionInfo.body, m_lexer->currentOffset());
                break;
            case ArrowFunctionBodyBlock:
            case StandardFunctionBodyBlock:
                context.setEndOffset(functionInfo.body, m_lexer->currentOffset());
                next();
                break;
            }
            functionInfo.endLine = m_lastTokenEndPosition.line;
            return true;
        }

        return false;
    };

    SyntaxChecker syntaxChecker(const_cast<VM&>(m_vm), m_lexer.get());

    if (UNLIKELY((SourceParseModeSet(SourceParseMode::ArrowFunctionMode, SourceParseMode::AsyncArrowFunctionMode).contains(mode)))) {
        startLocation = tokenLocation();
        functionInfo.startLine = tokenLine();
        startColumn = tokenColumn();

        parametersStart = m_token.m_location.startOffset;
        functionInfo.startOffset = parametersStart;
        functionInfo.parametersStartColumn = startColumn;

        if (loadCachedFunction())
            return true;

        {
            // Parse formal parameters with [+Yield] parameterization, in order to ban YieldExpressions
            // in ArrowFormalParameters, per ES6 #sec-arrow-function-definitions-static-semantics-early-errors.
            Scope::MaybeParseAsGeneratorForScope parseAsGenerator(functionScope, parentScope->isGenerator());
            SetForScope<bool> overrideAllowAwait(m_parserState.allowAwait, !parentScope->isAsyncFunction() && !isAsyncFunctionParseMode(mode));
            parseFunctionParameters(syntaxChecker, functionInfo);
            propagateError();
        }

        matchOrFail(ARROWFUNCTION, "Expected a '=>' after arrow function parameter declaration");

        if (m_lexer->hasLineTerminatorBeforeToken())
            failDueToUnexpectedToken();

        ASSERT(constructorKind == ConstructorKind::None);

        // Check if arrow body start with {. If it true it mean that arrow function is Fat arrow function
        // and we need use common approach to parse function body
        next();
        functionBodyType = match(OPENBRACE) ? ArrowFunctionBodyBlock : ArrowFunctionBodyExpression;
    } else {
        // http://ecma-international.org/ecma-262/6.0/#sec-function-definitions
        // FunctionExpression :
        //     function BindingIdentifieropt ( FormalParameters ) { FunctionBody }
        //
        // FunctionDeclaration[Yield, Default] :
        //     function BindingIdentifier[?Yield] ( FormalParameters ) { FunctionBody }
        //     [+Default] function ( FormalParameters ) { FunctionBody }
        //
        // GeneratorDeclaration[Yield, Default] :
        //     function * BindingIdentifier[?Yield] ( FormalParameters[Yield] ) { GeneratorBody }
        //     [+Default] function * ( FormalParameters[Yield] ) { GeneratorBody }
        //
        // GeneratorExpression :
        //     function * BindingIdentifier[Yield]opt ( FormalParameters[Yield] ) { GeneratorBody }
        //
        // The name of FunctionExpression and AsyncFunctionExpression can accept "yield" even in the context of generator.
        bool canUseYield = !strictMode();
        if (!(functionDefinitionType == FunctionDefinitionType::Expression && SourceParseModeSet(SourceParseMode::NormalFunctionMode, SourceParseMode::AsyncFunctionMode).contains(mode)))
            canUseYield &= !parentScope->isGenerator();

        if (requirements != FunctionNameRequirements::Unnamed) {
            ASSERT_WITH_MESSAGE(!(requirements == FunctionNameRequirements::None && !functionInfo.name), "When specifying FunctionNameRequirements::None, we need to initialize functionInfo.name with the default value in the caller side.");
            if (matchSpecIdentifier(canUseYield, functionNameIsAwait)) {
                functionInfo.name = m_token.m_data.ident;
                m_parserState.lastFunctionName = functionInfo.name;
                if (UNLIKELY(isDisallowedAwaitFunctionNameReason))
                    semanticFailIfTrue(functionDefinitionType == FunctionDefinitionType::Declaration || isAsyncFunctionOrAsyncGeneratorWrapperParseMode(mode), "Cannot declare function named 'await' ", isDisallowedAwaitFunctionNameReason);
                else if (isAsyncFunctionOrAsyncGeneratorWrapperParseMode(mode) && match(AWAIT) && functionDefinitionType == FunctionDefinitionType::Expression)
                    semanticFail("Cannot declare ", stringForFunctionMode(mode), " named 'await'");
                else if (isGeneratorWrapperParseMode(mode) && match(YIELD) && functionDefinitionType == FunctionDefinitionType::Expression)
                    semanticFail("Cannot declare generator function named 'yield'");
                next();
                if (!nameIsInContainingScope)
                    failIfTrueIfStrict(functionScope->declareCallee(functionInfo.name) & DeclarationResult::InvalidStrictMode, "'", functionInfo.name->impl(), "' is not a valid ", stringForFunctionMode(mode), " name in strict mode");
            } else if (requirements == FunctionNameRequirements::Named) {
                if (match(OPENPAREN)) {
                    semanticFailIfTrue(mode == SourceParseMode::NormalFunctionMode, "Function statements must have a name");
                    semanticFailIfTrue(mode == SourceParseMode::AsyncFunctionMode, "Async function statements must have a name");
                }
                semanticFailureDueToKeyword(stringForFunctionMode(mode), " name");
                failDueToUnexpectedToken();
                return false;
            }
            ASSERT(functionInfo.name);
        }

        startLocation = tokenLocation();
        functionInfo.startLine = tokenLine();
        startColumn = tokenColumn();
        functionInfo.parametersStartColumn = startColumn;

        parametersStart = m_token.m_location.startOffset;
        functionInfo.startOffset = parametersStart;

        if (loadCachedFunction())
            return true;
        {
            SetForScope<bool> overrideAllowAwait(m_parserState.allowAwait, !isAsyncFunctionParseMode(mode));
            parseFunctionParameters(syntaxChecker, functionInfo);
            propagateError();
        }
        
        matchOrFail(OPENBRACE, "Expected an opening '{' at the start of a ", stringForFunctionMode(mode), " body");

        // If the code is invoked from function constructor, we need to ensure that parameters are only composed by the string offered as parameters.
        if (UNLIKELY(functionConstructorParametersEndPosition))
            semanticFailIfFalse(lastTokenEndPosition().offset == *functionConstructorParametersEndPosition, "Parameters should match arguments offered as parameters in Function constructor");
        
        // BytecodeGenerator emits code to throw TypeError when a class constructor is "call"ed.
        // Set ConstructorKind to None for non-constructor methods of classes.
    
        if (parentScope->isGlobalCodeScope() && m_defaultConstructorKindForTopLevelFunction != ConstructorKind::None) {
            constructorKind = m_defaultConstructorKindForTopLevelFunction;
            expectedSuperBinding = m_defaultConstructorKindForTopLevelFunction == ConstructorKind::Extends ? SuperBinding::Needed : SuperBinding::NotNeeded;
        }

        functionBodyType = StandardFunctionBodyBlock;
    }

    functionScope->setConstructorKind(constructorKind);
    functionScope->setExpectedSuperBinding(expectedSuperBinding);

    m_parserState.lastFunctionName = lastFunctionName;
    ParserState oldState = internalSaveParserState(context);

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=156962
    // This loop collects the set of capture candidates that aren't
    // part of the set of this function's declared parameters. We will
    // figure out which parameters are captured for this function when
    // we actually generate code for it. For now, we just propagate to
    // our parent scopes which variables we might have closed over that
    // belong to them. This is necessary for correctness when using
    // the source provider cache because we can't close over a variable
    // that we don't claim to close over. The source provider cache must
    // know this information to properly cache this function.
    // This might work itself out nicer if we declared a different
    // Scope struct for the parameters (because they are indeed implemented
    // as their own scope).
    UniquedStringImplPtrSet nonLocalCapturesFromParameterExpressions;
    functionScope->forEachUsedVariable([&] (UniquedStringImpl* impl) {
        if (!functionScope->hasDeclaredParameter(impl)) {
            nonLocalCapturesFromParameterExpressions.add(impl);
            if (TreeBuilder::NeedsFreeVariableInfo)
                parentScope->addClosedVariableCandidateUnconditionally(impl);
        }
    });

    auto performParsingFunctionBody = [&] {
        return parseFunctionBody(context, syntaxChecker, startLocation, startColumn, functionKeywordStart, functionNameStart, parametersStart, constructorKind, expectedSuperBinding, functionBodyType, functionInfo.parameterCount);
    };

    if (isGeneratorOrAsyncFunctionWrapperParseMode(mode)) {
        AutoPopScopeRef generatorBodyScope(this, pushScope());
        SourceParseMode innerParseMode = SourceParseMode::GeneratorBodyMode;
        if (isAsyncFunctionOrAsyncGeneratorWrapperParseMode(mode))
            innerParseMode = getAsynFunctionBodyParseMode(mode);

        generatorBodyScope->setSourceParseMode(innerParseMode);
        generatorBodyScope->setConstructorKind(ConstructorKind::None);
        generatorBodyScope->setExpectedSuperBinding(expectedSuperBinding);

        // Disallow 'use strict' directives in the implicit inner function if
        // needed.
        if (functionScope->hasNonSimpleParameterList())
            generatorBodyScope->setHasNonSimpleParameterList();

        functionInfo.body = performParsingFunctionBody();

        // When a generator has a "use strict" directive, a generator function wrapping it should be strict mode.
        if  (generatorBodyScope->strictMode())
            functionScope->setStrictMode();

        popScope(generatorBodyScope, TreeBuilder::NeedsFreeVariableInfo);
    } else
        functionInfo.body = performParsingFunctionBody();
    
    restoreParserState(context, oldState);
    failIfFalse(functionInfo.body, "Cannot parse the body of this ", stringForFunctionMode(mode));
    context.setEndOffset(functionInfo.body, m_lexer->currentOffset());
    if (functionScope->strictMode() && requirements != FunctionNameRequirements::Unnamed) {
        ASSERT(functionInfo.name);
        RELEASE_ASSERT(SourceParseModeSet(SourceParseMode::NormalFunctionMode, SourceParseMode::MethodMode, SourceParseMode::ArrowFunctionMode, SourceParseMode::GeneratorBodyMode, SourceParseMode::GeneratorWrapperFunctionMode).contains(mode) || isAsyncFunctionOrAsyncGeneratorWrapperParseMode(mode));
        semanticFailIfTrue(m_vm.propertyNames->arguments == *functionInfo.name, "'", functionInfo.name->impl(), "' is not a valid function name in strict mode");
        semanticFailIfTrue(m_vm.propertyNames->eval == *functionInfo.name, "'", functionInfo.name->impl(), "' is not a valid function name in strict mode");
    }

    JSTokenLocation location = JSTokenLocation(m_token.m_location);
    functionInfo.endOffset = m_token.m_data.offset;
    
    if (functionBodyType == ArrowFunctionBodyExpression) {
        location = locationBeforeLastToken();
        functionInfo.endOffset = location.endOffset;
    } else {
        recordFunctionEntryLocation(JSTextPosition(startLocation.line, startLocation.startOffset, startLocation.lineStartOffset));
        recordFunctionLeaveLocation(JSTextPosition(location.line, location.startOffset, location.lineStartOffset));
    }

    // Cache the tokenizer state and the function scope the first time the function is parsed.
    // Any future reparsing can then skip the function.
    // For arrow function is 8 = x=>x + 4 symbols;
    // For ordinary function is 16  = function(){} + 4 symbols
    const int minimumSourceLengthToCache = functionBodyType == StandardFunctionBodyBlock ? 16 : 8;
    std::unique_ptr<SourceProviderCacheItem> newInfo;
    int sourceLength = functionInfo.endOffset - functionInfo.startOffset;
    if (TreeBuilder::CanUseFunctionCache && m_functionCache && sourceLength > minimumSourceLengthToCache) {
        SourceProviderCacheItemCreationParameters parameters;
        parameters.endFunctionOffset = functionInfo.endOffset;
        parameters.lastTokenLine = location.line;
        parameters.lastTokenStartOffset = location.startOffset;
        parameters.lastTokenEndOffset = location.endOffset;
        parameters.lastTokenLineStartOffset = location.lineStartOffset;
        parameters.parameterCount = functionInfo.parameterCount;
        parameters.constructorKind = constructorKind;
        parameters.expectedSuperBinding = expectedSuperBinding;
        if (functionBodyType == ArrowFunctionBodyExpression) {
            parameters.isBodyArrowExpression = true;
            parameters.tokenType = m_token.m_type;
        }
        functionScope->fillParametersForSourceProviderCache(parameters, nonLocalCapturesFromParameterExpressions);
        newInfo = SourceProviderCacheItem::create(parameters);
    }

    bool functionScopeWasStrictMode = functionScope->strictMode();
    
    popScope(functionScope, TreeBuilder::NeedsFreeVariableInfo);
    
    if (functionBodyType != ArrowFunctionBodyExpression) {
        matchOrFail(CLOSEBRACE, "Expected a closing '}' after a ", stringForFunctionMode(mode), " body");
        next();
    } else {
        // We need to lex the last token again because the last token is lexed under the different context because of the following possibilities.
        // 1. which may have different strict mode.
        // 2. which may not build strings for tokens.
        // But (1) is not possible because we do not recognize the string literal in ArrowFunctionBodyExpression as directive and this is correct in terms of the spec (`value => "use strict"`).
        // So we only check TreeBuilder's type here.
        ASSERT_UNUSED(functionScopeWasStrictMode, functionScopeWasStrictMode == currentScope()->strictMode());
        if (!std::is_same<TreeBuilder, SyntaxChecker>::value)
            lexCurrentTokenAgainUnderCurrentContext(context);
    }

    if (newInfo)
        m_functionCache->add(functionInfo.startOffset, WTFMove(newInfo));
    
    functionInfo.endLine = m_lastTokenEndPosition.line;
    return true;
}

static NO_RETURN_DUE_TO_CRASH FunctionMetadataNode* getMetadata(ParserFunctionInfo<SyntaxChecker>&) { RELEASE_ASSERT_NOT_REACHED(); }
static FunctionMetadataNode* getMetadata(ParserFunctionInfo<ASTBuilder>& info) { return info.body; }

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseFunctionDeclaration(TreeBuilder& context, FunctionDeclarationType declarationType, ExportType exportType, DeclarationDefaultContext declarationDefaultContext, Optional<int> functionConstructorParametersEndPosition)
{
    ASSERT(match(FUNCTION));
    JSTokenLocation location(tokenLocation());
    unsigned functionKeywordStart = tokenStart();
    next();
    SourceParseMode parseMode = SourceParseMode::NormalFunctionMode;
    if (match(TIMES)) {
        failIfTrue(declarationType == FunctionDeclarationType::Statement, "Cannot use generator function declaration in single-statement context");
        next();
        parseMode = SourceParseMode::GeneratorWrapperFunctionMode;
    }
    SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);

    ParserFunctionInfo<TreeBuilder> functionInfo;
    FunctionNameRequirements requirements = FunctionNameRequirements::Named;
    if (declarationDefaultContext == DeclarationDefaultContext::ExportDefault) {
        // Under the "export default" context, function declaration does not require the function name.
        //
        //     ExportDeclaration:
        //         ...
        //         export default HoistableDeclaration[~Yield, +Default]
        //         ...
        //
        //     HoistableDeclaration[Yield, Default]:
        //         FunctionDeclaration[?Yield, ?Default]
        //         GeneratorDeclaration[?Yield, ?Default]
        //
        //     FunctionDeclaration[Yield, Default]:
        //         ...
        //         [+Default] function ( FormalParameters[~Yield] ) { FunctionBody[~Yield] }
        //
        //     GeneratorDeclaration[Yield, Default]:
        //         ...
        //         [+Default] function * ( FormalParameters[+Yield] ) { GeneratorBody }
        //
        // In this case, we use "*default*" as this function declaration's name.
        requirements = FunctionNameRequirements::None;
        functionInfo.name = &m_vm.propertyNames->starDefaultPrivateName;
    }

    failIfFalse((parseFunctionInfo(context, requirements, true, ConstructorKind::None, SuperBinding::NotNeeded, functionKeywordStart, functionInfo, FunctionDefinitionType::Declaration, functionConstructorParametersEndPosition)), "Cannot parse this function");
    ASSERT(functionInfo.name);

    std::pair<DeclarationResultMask, ScopeRef> functionDeclaration = declareFunction(functionInfo.name);
    DeclarationResultMask declarationResult = functionDeclaration.first;
    failIfTrueIfStrict(declarationResult & DeclarationResult::InvalidStrictMode, "Cannot declare a function named '", functionInfo.name->impl(), "' in strict mode");
    if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration)
        internalFailWithMessage(false, "Cannot declare a function that shadows a let/const/class/function variable '", functionInfo.name->impl(), "' in strict mode");
    if (exportType == ExportType::Exported) {
        ASSERT_WITH_MESSAGE(declarationDefaultContext != DeclarationDefaultContext::ExportDefault, "Export default case will export the name and binding in the caller.");
        semanticFailIfFalse(exportName(*functionInfo.name), "Cannot export a duplicate function name: '", functionInfo.name->impl(), "'");
        m_moduleScopeData->exportBinding(*functionInfo.name);
    }

    TreeStatement result = context.createFuncDeclStatement(location, functionInfo);
    if (TreeBuilder::CreatesAST)
        functionDeclaration.second->appendFunction(getMetadata(functionInfo));
    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseAsyncFunctionDeclaration(TreeBuilder& context, ExportType exportType, DeclarationDefaultContext declarationDefaultContext, Optional<int> functionConstructorParametersEndPosition)
{
    ASSERT(match(FUNCTION));
    JSTokenLocation location(tokenLocation());
    unsigned functionKeywordStart = tokenStart();
    next();
    ParserFunctionInfo<TreeBuilder> functionInfo;
    SourceParseMode parseMode = SourceParseMode::AsyncFunctionMode;
    if (consume(TIMES))
        parseMode = SourceParseMode::AsyncGeneratorWrapperFunctionMode;
    SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);

    FunctionNameRequirements requirements = FunctionNameRequirements::Named;
    if (declarationDefaultContext == DeclarationDefaultContext::ExportDefault) {
        // Under the "export default" context, function declaration does not require the function name.
        //
        //     ExportDeclaration:
        //         ...
        //         export default HoistableDeclaration[~Yield, +Default]
        //         ...
        //
        //     HoistableDeclaration[Yield, Default]:
        //         FunctionDeclaration[?Yield, ?Default]
        //         GeneratorDeclaration[?Yield, ?Default]
        //
        //     FunctionDeclaration[Yield, Default]:
        //         ...
        //         [+Default] function ( FormalParameters[~Yield] ) { FunctionBody[~Yield] }
        //
        //     GeneratorDeclaration[Yield, Default]:
        //         ...
        //         [+Default] function * ( FormalParameters[+Yield] ) { GeneratorBody }
        //
        // In this case, we use "*default*" as this function declaration's name.
        requirements = FunctionNameRequirements::None;
        functionInfo.name = &m_vm.propertyNames->starDefaultPrivateName;
    }

    failIfFalse((parseFunctionInfo(context, requirements, true, ConstructorKind::None, SuperBinding::NotNeeded, functionKeywordStart, functionInfo, FunctionDefinitionType::Declaration, functionConstructorParametersEndPosition)), "Cannot parse this async function");
    failIfFalse(functionInfo.name, "Async function statements must have a name");

    std::pair<DeclarationResultMask, ScopeRef> functionDeclaration = declareFunction(functionInfo.name);
    DeclarationResultMask declarationResult = functionDeclaration.first;
    failIfTrueIfStrict(declarationResult & DeclarationResult::InvalidStrictMode, "Cannot declare an async function named '", functionInfo.name->impl(), "' in strict mode");
    if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration)
        internalFailWithMessage(false, "Cannot declare an async function that shadows a let/const/class/function variable '", functionInfo.name->impl(), "' in strict mode");
    if (exportType == ExportType::Exported) {
        semanticFailIfFalse(exportName(*functionInfo.name), "Cannot export a duplicate function name: '", functionInfo.name->impl(), "'");
        m_moduleScopeData->exportBinding(*functionInfo.name);
    }

    TreeStatement result = context.createFuncDeclStatement(location, functionInfo);
    if (TreeBuilder::CreatesAST)
        functionDeclaration.second->appendFunction(getMetadata(functionInfo));
    return result;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseClassDeclaration(TreeBuilder& context, ExportType exportType, DeclarationDefaultContext declarationDefaultContext)
{
    ASSERT(match(CLASSTOKEN));
    JSTokenLocation location(tokenLocation());
    JSTextPosition classStart = tokenStartPosition();
    unsigned classStartLine = tokenLine();

    ParserClassInfo<TreeBuilder> info;
    FunctionNameRequirements requirements = FunctionNameRequirements::Named;
    if (declarationDefaultContext == DeclarationDefaultContext::ExportDefault) {
        // Under the "export default" context, class declaration does not require the class name.
        //
        //     ExportDeclaration:
        //         ...
        //         export default ClassDeclaration[~Yield, +Default]
        //         ...
        //
        //     ClassDeclaration[Yield, Default]:
        //         ...
        //         [+Default] class ClassTail[?Yield]
        //
        // In this case, we use "*default*" as this class declaration's name.
        requirements = FunctionNameRequirements::None;
        info.className = &m_vm.propertyNames->starDefaultPrivateName;
    }

    TreeClassExpression classExpr = parseClass(context, requirements, info);
    failIfFalse(classExpr, "Failed to parse class");
    ASSERT(info.className);

    DeclarationResultMask declarationResult = declareVariable(info.className, DeclarationType::LetDeclaration);
    if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration)
        internalFailWithMessage(false, "Cannot declare a class twice: '", info.className->impl(), "'");
    if (exportType == ExportType::Exported) {
        ASSERT_WITH_MESSAGE(declarationDefaultContext != DeclarationDefaultContext::ExportDefault, "Export default case will export the name and binding in the caller.");
        semanticFailIfFalse(exportName(*info.className), "Cannot export a duplicate class name: '", info.className->impl(), "'");
        m_moduleScopeData->exportBinding(*info.className);
    }

    JSTextPosition classEnd = lastTokenEndPosition();
    unsigned classEndLine = tokenLine();

    return context.createClassDeclStatement(location, classExpr, classStart, classEnd, classStartLine, classEndLine);
}

static constexpr ASCIILiteral instanceComputedNamePrefix { "instanceComputedName"_s };
static constexpr ASCIILiteral staticComputedNamePrefix { "staticComputedName"_s };

template <typename LexerType>
template <class TreeBuilder> TreeClassExpression Parser<LexerType>::parseClass(TreeBuilder& context, FunctionNameRequirements requirements, ParserClassInfo<TreeBuilder>& info)
{
    ASSERT(match(CLASSTOKEN));
    JSTokenLocation location(tokenLocation());
    info.startLine = location.line;
    info.startColumn = tokenColumn();
    info.startOffset = location.startOffset;

    AutoPopScopeRef classScope(this, pushScope());
    classScope->setIsLexicalScope();
    classScope->preventVarDeclarations();
    classScope->setStrictMode();
    bool declaresPrivateMethod = false;
    bool declaresPrivateAccessor = false;
    bool declaresStaticPrivateMethod = false;
    bool declaresStaticPrivateAccessor = false;
    next();

    ASSERT_WITH_MESSAGE(requirements != FunctionNameRequirements::Unnamed, "Currently, there is no caller that uses FunctionNameRequirements::Unnamed for class syntax.");
    ASSERT_WITH_MESSAGE(!(requirements == FunctionNameRequirements::None && !info.className), "When specifying FunctionNameRequirements::None, we need to initialize info.className with the default value in the caller side.");
    if (match(IDENT) || isAllowedIdentifierAwait(m_token)) {
        info.className = m_token.m_data.ident;
        next();
        failIfTrue(classScope->declareLexicalVariable(info.className, true) & DeclarationResult::InvalidStrictMode, "'", info.className->impl(), "' is not a valid class name");
    } else if (requirements == FunctionNameRequirements::Named) {
        if (match(OPENBRACE))
            semanticFail("Class statements must have a name");
        semanticFailureDueToKeyword("class name");
        failDueToUnexpectedToken();
    }
    ASSERT(info.className);

    TreeExpression parentClass = 0;
    if (consume(EXTENDS)) {
        parentClass = parseMemberExpression(context);
        failIfFalse(parentClass, "Cannot parse the parent class name");
    }
    classScope->setIsClassScope();
    const ConstructorKind constructorKind = parentClass ? ConstructorKind::Extends : ConstructorKind::Base;

    consumeOrFail(OPENBRACE, "Expected opening '{' at the start of a class body");

    TreeExpression constructor = 0;
    TreePropertyList classElements = 0;
    TreePropertyList classElementsTail = 0;
    unsigned nextInstanceComputedFieldID = 0;
    unsigned nextStaticComputedFieldID = 0;
    while (!match(CLOSEBRACE)) {
        if (match(SEMICOLON)) {
            next();
            continue;
        }

        JSTokenLocation methodLocation(tokenLocation());
        unsigned methodStart = tokenStart();

        // For backwards compatibility, "static" is a non-reserved keyword in non-strict mode.
        ClassElementTag tag = ClassElementTag::Instance;
        auto type = PropertyNode::Constant;
        if (match(RESERVED_IF_STRICT) && *m_token.m_data.ident == m_vm.propertyNames->staticKeyword) {
            SavePoint savePoint = createSavePoint(context);
            next();
            if (match(OPENPAREN) || match(SEMICOLON) || match(EQUAL)) {
                // Reparse "static()" as a method or "static" as a class field.
                restoreSavePoint(context, savePoint);
            } else
                tag = ClassElementTag::Static;
        }

        // FIXME: Figure out a way to share more code with parseProperty.
        const CommonIdentifiers& propertyNames = *m_vm.propertyNames;
        const Identifier* ident = &propertyNames.nullIdentifier;
        TreeExpression computedPropertyName = 0;
        bool isGetter = false;
        bool isSetter = false;
        SourceParseMode parseMode = SourceParseMode::MethodMode;
        if (consume(TIMES))
            parseMode = SourceParseMode::GeneratorWrapperMethodMode;

parseMethod:
        switch (m_token.m_type) {
        namedKeyword:
        case STRING:
            ident = m_token.m_data.ident;
            ASSERT(ident);
            next();
            break;
        case BIGINT:
            ident = &m_parserArena.identifierArena().makeBigIntDecimalIdentifier(const_cast<VM&>(m_vm), *m_token.m_data.bigIntString, m_token.m_data.radix);
            ASSERT(ident);
            next();
            break;
        case ESCAPED_KEYWORD:
        case IDENT:
            if (UNLIKELY(*m_token.m_data.ident == m_vm.propertyNames->async && !m_token.m_data.escaped)) {
                if (!isGeneratorMethodParseMode(parseMode) && !isAsyncMethodParseMode(parseMode)) {
                    ident = m_token.m_data.ident;
                    next();
                    // We match SEMICOLON as a special case for a field called 'async' without initializer.
                    if (match(OPENPAREN) || match(COLON) || match(SEMICOLON) || match(EQUAL) || m_lexer->hasLineTerminatorBeforeToken())
                        break;
                    if (UNLIKELY(consume(TIMES)))
                        parseMode = SourceParseMode::AsyncGeneratorWrapperMethodMode;
                    else
                        parseMode = SourceParseMode::AsyncMethodMode;
                    goto parseMethod;
                }
            }
            FALLTHROUGH;
        case AWAIT: {
            ident = m_token.m_data.ident;
            bool escaped = m_token.m_data.escaped;
            ASSERT(ident);
            next();
            if (parseMode == SourceParseMode::MethodMode && !escaped && (matchIdentifierOrKeyword() || match(STRING) || match(DOUBLE) || match(INTEGER) || match(BIGINT) || match(OPENBRACKET) || (Options::usePrivateMethods() && match(PRIVATENAME)))) {
                isGetter = *ident == propertyNames.get;
                isSetter = *ident == propertyNames.set;
            }
            break;
        }
        case DOUBLE:
        case INTEGER:
            ident = &m_parserArena.identifierArena().makeNumericIdentifier(const_cast<VM&>(m_vm), m_token.m_data.doubleValue);
            ASSERT(ident);
            next();
            break;
        case OPENBRACKET:
            next();
            computedPropertyName = parseAssignmentExpression(context);
            type = static_cast<PropertyNode::Type>(type | PropertyNode::Computed);
            failIfFalse(computedPropertyName, "Cannot parse computed property name");
            handleProductionOrFail(CLOSEBRACKET, "]", "end", "computed property name");
            break;
        case PRIVATENAME: {
            ASSERT(Options::usePrivateClassFields());
            ident = m_token.m_data.ident;
            if (!Options::usePrivateStaticClassFields())
                failIfTrue(tag == ClassElementTag::Static, "Static class element cannot be private");
            failIfTrue(isGetter || isSetter, "Cannot parse class method with private name");
            ASSERT(ident);
            next();
            if (Options::usePrivateMethods() && match(OPENPAREN)) {
                semanticFailIfTrue(classScope->declarePrivateMethod(*ident, tag) & DeclarationResult::InvalidDuplicateDeclaration, "Cannot declare private method twice");
                semanticFailIfTrue(tag == ClassElementTag::Static && *ident == propertyNames.constructorPrivateField, "Cannot declare a static private method named 'constructor'");

                if (tag == ClassElementTag::Static)
                    declaresStaticPrivateAccessor = true;
                else
                    declaresPrivateMethod = true;

                type = static_cast<PropertyNode::Type>(type | PropertyNode::PrivateMethod);
                break;
            }

            failIfTrue(match(OPENPAREN), "Cannot parse class method with private name");
            semanticFailIfTrue(classScope->declarePrivateField(*ident) & DeclarationResult::InvalidDuplicateDeclaration, "Cannot declare private field twice");
            type = static_cast<PropertyNode::Type>(type | PropertyNode::PrivateField);
            break;
        }
        default:
            if (m_token.m_type & KeywordTokenFlag)
                goto namedKeyword;
            failDueToUnexpectedToken();
        }

        TreeProperty property;
        if (isGetter || isSetter) {
            if (Options::usePrivateMethods() && match(PRIVATENAME)) {
                ident = m_token.m_data.ident;

                auto declarationResult = isSetter ? classScope->declarePrivateSetter(*ident, tag) : classScope->declarePrivateGetter(*ident, tag);
                semanticFailIfTrue(declarationResult & DeclarationResult::InvalidDuplicateDeclaration, "Declared private setter with an already used name");
                if (tag == ClassElementTag::Static) {
                    semanticFailIfTrue(declarationResult & DeclarationResult::InvalidPrivateStaticNonStatic, "Cannot declare a private static ", (isSetter ? "setter" : "getter")  , " if there is a non-static private ", (isSetter ? "getter" : "setter"), " with used name");
                    declaresStaticPrivateAccessor = true;
                } else {
                    semanticFailIfTrue(declarationResult & DeclarationResult::InvalidPrivateStaticNonStatic, "Cannot declare a private non-static ", (isSetter ? "setter" : "getter"), " if there is a static private ", (isSetter ? "getter" : "setter"), " with used name");
                    declaresPrivateAccessor = true;
                }

                if (isSetter)
                    type = static_cast<PropertyNode::Type>(type | PropertyNode::PrivateSetter);
                else 
                    type = static_cast<PropertyNode::Type>(type | PropertyNode::PrivateGetter);
            } else {
                type = static_cast<PropertyNode::Type>(type & ~PropertyNode::Constant);
                type = static_cast<PropertyNode::Type>(type | (isGetter ? PropertyNode::Getter : PropertyNode::Setter));
            }
            property = parseGetterSetter(context, type, methodStart, ConstructorKind::None, tag);
            failIfFalse(property, "Cannot parse this method");
        } else if (!match(OPENPAREN) && (tag == ClassElementTag::Instance || Options::usePublicStaticClassFields()) && parseMode == SourceParseMode::MethodMode) {
            ASSERT(!isGetter && !isSetter);
            if (ident) {
                semanticFailIfTrue(*ident == propertyNames.constructor, "Cannot declare class field named 'constructor'");
                semanticFailIfTrue(*ident == propertyNames.constructorPrivateField, "Cannot declare private class field named '#constructor'");
                if (tag == ClassElementTag::Static)
                    semanticFailIfTrue(*ident == propertyNames.prototype, "Cannot declare a static field named 'prototype'");
            }

            if (computedPropertyName) {
                if (tag == ClassElementTag::Instance)
                    ident = &m_parserArena.identifierArena().makePrivateIdentifier(m_vm, instanceComputedNamePrefix, nextInstanceComputedFieldID++);
                else
                    ident = &m_parserArena.identifierArena().makePrivateIdentifier(m_vm, staticComputedNamePrefix, nextStaticComputedFieldID++);
                DeclarationResultMask declarationResult = classScope->declareLexicalVariable(ident, true);
                ASSERT_UNUSED(declarationResult, declarationResult == DeclarationResult::Valid);
                classScope->useVariable(ident, false);
                classScope->addClosedVariableCandidateUnconditionally(ident->impl());
            }

            TreeExpression initializer = 0;
            if (consume(EQUAL)) {
                SetForScope<bool> overrideParsingClassFieldInitializer(m_parserState.isParsingClassFieldInitializer, true);
                classScope->setExpectedSuperBinding(SuperBinding::Needed);
                initializer = parseAssignmentExpression(context);
                classScope->setExpectedSuperBinding(SuperBinding::NotNeeded);
                failIfFalse(initializer, "Cannot parse initializer for class field");
                classScope->markLastUsedVariablesSetAsCaptured();
            }
            failIfFalse(autoSemiColon(), "Expected a ';' following a class field");
            auto inferName = initializer ? InferName::Allowed : InferName::Disallowed;
            if (computedPropertyName)
                property = context.createProperty(ident, computedPropertyName, initializer, type, SuperBinding::NotNeeded, tag);
            else
                property = context.createProperty(ident, initializer, type, SuperBinding::NotNeeded, inferName, tag);
        } else {
            ParserFunctionInfo<TreeBuilder> methodInfo;
            bool isConstructor = tag == ClassElementTag::Instance && *ident == propertyNames.constructor;
            semanticFailIfTrue(isConstructor && parseMode != SourceParseMode::MethodMode,
                "Cannot declare ", stringArticleForFunctionMode(parseMode), stringForFunctionMode(parseMode), " named 'constructor'");

            methodInfo.name = isConstructor ? info.className : ident;
            SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);
            failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::Unnamed, false, isConstructor ? constructorKind : ConstructorKind::None, SuperBinding::Needed, methodStart, methodInfo, FunctionDefinitionType::Method)), "Cannot parse this method");

            TreeExpression method = context.createMethodDefinition(methodLocation, methodInfo);
            if (isConstructor) {
                semanticFailIfTrue(constructor, "Cannot declare multiple constructors in a single class");
                constructor = method;
                continue;
            }

            semanticFailIfTrue(tag == ClassElementTag::Static && methodInfo.name && *methodInfo.name == propertyNames.prototype,
                "Cannot declare a static method named 'prototype'");

            if (computedPropertyName) {
                property = context.createProperty(computedPropertyName, method, type, SuperBinding::Needed, tag);
            } else {
                property = context.createProperty(methodInfo.name, method, type, SuperBinding::Needed, InferName::Allowed, tag);
            }
        }

        if (classElementsTail)
            classElementsTail = context.createPropertyList(methodLocation, property, classElementsTail);
        else
            classElements = classElementsTail = context.createPropertyList(methodLocation, property);
    }

    info.endOffset = tokenLocation().endOffset - 1;
    consumeOrFail(CLOSEBRACE, "Expected a closing '}' after a class body");

    if (declaresPrivateMethod || declaresPrivateAccessor) {
        Identifier privateBrandIdentifier = m_vm.propertyNames->builtinNames().privateBrandPrivateName();
        DeclarationResultMask declarationResult = classScope->declareLexicalVariable(&privateBrandIdentifier, true);
        ASSERT_UNUSED(declarationResult, declarationResult == DeclarationResult::Valid);
        classScope->useVariable(&privateBrandIdentifier, false);
        classScope->addClosedVariableCandidateUnconditionally(privateBrandIdentifier.impl());
    }

    if constexpr (std::is_same_v<TreeBuilder, ASTBuilder>) {
        if (classElements)
            classElements->setHasPrivateAccessors(declaresPrivateAccessor || declaresStaticPrivateAccessor);
    }

    if (declaresStaticPrivateMethod || declaresPrivateAccessor) {
        Identifier privateClassBrandIdentifier = m_vm.propertyNames->builtinNames().privateClassBrandPrivateName();
        DeclarationResultMask declarationResult = classScope->declareLexicalVariable(&privateClassBrandIdentifier, true);
        ASSERT_UNUSED(declarationResult, declarationResult == DeclarationResult::Valid);
        classScope->useVariable(&privateClassBrandIdentifier, false);
        classScope->addClosedVariableCandidateUnconditionally(privateClassBrandIdentifier.impl());
    }

    if (Options::usePrivateClassFields()) {
        // Fail if there are no parent private name scopes and any used-but-undeclared private names.
        semanticFailIfFalse(copyUndeclaredPrivateNamesToOuterScope(), "Cannot reference undeclared private names");
    }

    auto classExpression = context.createClassExpr(location, info, classScope->finalizeLexicalEnvironment(), constructor, parentClass, classElements);
    popScope(classScope, TreeBuilder::NeedsFreeVariableInfo);
    return classExpression;
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseClassFieldInitializerSourceElements(TreeBuilder& context, const Vector<JSTextPosition>& classFieldLocations)
{
    TreeSourceElements sourceElements = context.createSourceElements();
    currentScope()->setIsClassScope();

    unsigned numComputedFields = 0;
    for (auto location : classFieldLocations) {
        // This loop will either parse only static fields or only
        // instance fields, but never a mix; we could make it slightly
        // smarter about parsing given that fact, but it's probably
        // not worth the hassle, so begin each iteration without
        // knowing which kind the next field will be.
        bool isStaticField = false;
        // We don't need to worry about hasLineTerminatorBeforeToken
        // on class fields, so we set this value to false.
        LexerState lexerState { location.offset, static_cast<unsigned>(location.lineStartOffset), static_cast<unsigned>(location.line), static_cast<unsigned>(location.line), false };
        restoreLexerState(lexerState);

        JSTokenLocation fieldLocation = tokenLocation();
        const Identifier* ident = nullptr;
        DefineFieldNode::Type type = DefineFieldNode::Type::Name;

        if (match(RESERVED_IF_STRICT) && *m_token.m_data.ident == m_vm.propertyNames->staticKeyword) {
            auto* staticIdentifier = m_token.m_data.ident;
            ASSERT(staticIdentifier);
            next();
            if (match(SEMICOLON) || match (EQUAL) || match(CLOSEBRACE) || m_lexer->hasLineTerminatorBeforeToken())
                ident = staticIdentifier;
            else
                isStaticField = true;
        }

        if (!ident) {
            switch (m_token.m_type) {
            case PRIVATENAME:
                type = DefineFieldNode::Type::PrivateName;
                FALLTHROUGH;
            case STRING:
            case IDENT:
            namedKeyword:
                ident = m_token.m_data.ident;
                ASSERT(ident);
                next();
                break;
            case BIGINT:
                ident = &m_parserArena.identifierArena().makeBigIntDecimalIdentifier(const_cast<VM&>(m_vm), *m_token.m_data.bigIntString, m_token.m_data.radix);
                ASSERT(ident);
                next();
                break;
            case DOUBLE:
            case INTEGER:
                ident = &m_parserArena.identifierArena().makeNumericIdentifier(const_cast<VM&>(m_vm), m_token.m_data.doubleValue);
                ASSERT(ident);
                next();
                break;
            case OPENBRACKET: {
                next();
                TreeExpression computedPropertyName = parseAssignmentExpression(context);
                failIfFalse(computedPropertyName, "Cannot parse computed property name");
                handleProductionOrFail(CLOSEBRACKET, "]", "end", "computed property name");
                ident = &m_parserArena.identifierArena().makePrivateIdentifier(m_vm,
                    isStaticField ? staticComputedNamePrefix : instanceComputedNamePrefix,
                    numComputedFields++);
                type = DefineFieldNode::Type::ComputedName;
                break;
            }
            default:
                if (m_token.m_type & KeywordTokenFlag)
                    goto namedKeyword;
                failDueToUnexpectedToken();
            }
        }

        // Only valid class fields are handled in this function.
        ASSERT(match(EQUAL) || match(SEMICOLON) || match(CLOSEBRACE) || m_lexer->hasLineTerminatorBeforeToken());

        TreeExpression initializer = 0;
        if (consume(EQUAL))
            initializer = parseAssignmentExpression(context);

        if (type == DefineFieldNode::Type::PrivateName)
            currentScope()->useVariable(ident, false);

        TreeStatement defineField = context.createDefineField(fieldLocation, ident, initializer, type);
        context.appendStatement(sourceElements, defineField);
    }

    ASSERT(!hasError());
    // Trick parseInner() into believing we've parsed the entire SourceCode, in order to prevent it from producing an error.
    m_token.m_type = EOFTOK;
    return sourceElements;
}

struct LabelInfo {
    LabelInfo(const Identifier* ident, const JSTextPosition& start, const JSTextPosition& end)
    : m_ident(ident)
    , m_start(start)
    , m_end(end)
    {
    }
    
    const Identifier* m_ident;
    JSTextPosition m_start;
    JSTextPosition m_end;
};

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseExpressionOrLabelStatement(TreeBuilder& context, bool allowFunctionDeclarationAsStatement)
{
    
    /* Expression and Label statements are ambiguous at LL(1), so we have a
     * special case that looks for a colon as the next character in the input.
     */
    Vector<LabelInfo> labels;
    JSTokenLocation location;
    do {
        JSTextPosition start = tokenStartPosition();
        location = tokenLocation();
        if (!nextTokenIsColon()) {
            // If we hit this path we're making a expression statement, which
            // by definition can't make use of continue/break so we can just
            // ignore any labels we might have accumulated.
            TreeExpression expression = parseExpression(context, IsOnlyChildOfStatement::Yes);
            failIfFalse(expression, "Cannot parse expression statement");
            if (!autoSemiColon())
                failDueToUnexpectedToken();
            return context.createExprStatement(location, expression, start, m_lastTokenEndPosition.line);
        }

        semanticFailIfTrue(isDisallowedIdentifierAwait(m_token), "Cannot use 'await' as a label ", disallowedIdentifierAwaitReason());
        semanticFailIfTrue(isDisallowedIdentifierYield(m_token), "Cannot use 'yield' as a label ", disallowedIdentifierYieldReason());

        const Identifier* ident = m_token.m_data.ident;
        JSTextPosition end = tokenEndPosition();
        next();
        consumeOrFail(COLON, "Labels must be followed by a ':'");

        // This is O(N^2) over the current list of consecutive labels, but I
        // have never seen more than one label in a row in the real world.
        for (size_t i = 0; i < labels.size(); i++)
            failIfTrue(ident->impl() == labels[i].m_ident->impl(), "Attempted to redeclare the label '", ident->impl(), "'");
        failIfTrue(getLabel(ident), "Cannot find scope for the label '", ident->impl(), "'");
        labels.append(LabelInfo(ident, start, end));
    } while (matchSpecIdentifier());
    bool isLoop = false;
    switch (m_token.m_type) {
    case FOR:
    case WHILE:
    case DO:
        isLoop = true;
        break;
        
    default:
        break;
    }
    const Identifier* unused = nullptr;
    ScopeRef labelScope = currentScope();
    for (size_t i = 0; i < labels.size(); i++)
        pushLabel(labels[i].m_ident, isLoop);
    m_immediateParentAllowsFunctionDeclarationInStatement = allowFunctionDeclarationAsStatement;
    TreeStatement statement = parseStatement(context, unused);
    for (size_t i = 0; i < labels.size(); i++)
        popLabel(labelScope);
    failIfFalse(statement, "Cannot parse statement");
    for (size_t i = 0; i < labels.size(); i++) {
        const LabelInfo& info = labels[labels.size() - i - 1];
        statement = context.createLabelStatement(location, info.m_ident, statement, info.m_start, info.m_end);
    }
    return statement;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseExpressionStatement(TreeBuilder& context)
{
    switch (m_token.m_type) {
    // Consult: http://www.ecma-international.org/ecma-262/6.0/index.html#sec-expression-statement
    // The ES6 spec mandates that we should fail from FUNCTION token here. We handle this case 
    // in parseStatement() which is the only caller of parseExpressionStatement().
    // We actually allow FUNCTION in situations where it should not be allowed unless we're in strict mode.
    case CLASSTOKEN:
        failWithMessage("'class' declaration is not directly within a block statement");
        break;
    default:
        // FIXME: when implementing 'let' we should fail when we see the token sequence "let [".
        // https://bugs.webkit.org/show_bug.cgi?id=142944
        break;
    }
    JSTextPosition start = tokenStartPosition();
    JSTokenLocation location(tokenLocation());
    TreeExpression expression = parseExpression(context, IsOnlyChildOfStatement::Yes);
    failIfFalse(expression, "Cannot parse expression statement");
    failIfFalse(autoSemiColon(), "Parse error");
    return context.createExprStatement(location, expression, start, m_lastTokenEndPosition.line);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseIfStatement(TreeBuilder& context)
{
    ASSERT(match(IF));
    JSTokenLocation ifLocation(tokenLocation());
    int start = tokenLine();
    next();
    handleProductionOrFail2(OPENPAREN, "(", "start", "'if' condition");

    TreeExpression condition = parseExpression(context);
    failIfFalse(condition, "Expected an expression as the condition for an if statement");
    recordPauseLocation(context.breakpointLocation(condition));
    int end = tokenLine();
    handleProductionOrFail2(CLOSEPAREN, ")", "end", "'if' condition");

    const Identifier* unused = nullptr;
    m_immediateParentAllowsFunctionDeclarationInStatement = true;
    TreeStatement trueBlock = parseStatement(context, unused);
    failIfFalse(trueBlock, "Expected a statement as the body of an if block");

    if (!match(ELSE))
        return context.createIfStatement(ifLocation, condition, trueBlock, 0, start, end);

    Vector<TreeExpression> exprStack;
    Vector<std::pair<int, int>> posStack;
    Vector<JSTokenLocation> tokenLocationStack;
    Vector<TreeStatement> statementStack;
    bool trailingElse = false;
    do {
        JSTokenLocation tempLocation = tokenLocation();
        next();
        if (!match(IF)) {
            const Identifier* unused = nullptr;
            m_immediateParentAllowsFunctionDeclarationInStatement = true;
            TreeStatement block = parseStatement(context, unused);
            failIfFalse(block, "Expected a statement as the body of an else block");
            statementStack.append(block);
            trailingElse = true;
            break;
        }
        int innerStart = tokenLine();
        next();
        
        handleProductionOrFail2(OPENPAREN, "(", "start", "'if' condition");

        TreeExpression innerCondition = parseExpression(context);
        failIfFalse(innerCondition, "Expected an expression as the condition for an if statement");
        recordPauseLocation(context.breakpointLocation(innerCondition));
        int innerEnd = tokenLine();
        handleProductionOrFail2(CLOSEPAREN, ")", "end", "'if' condition");
        const Identifier* unused = nullptr;
        m_immediateParentAllowsFunctionDeclarationInStatement = true;
        TreeStatement innerTrueBlock = parseStatement(context, unused);
        failIfFalse(innerTrueBlock, "Expected a statement as the body of an if block");
        tokenLocationStack.append(tempLocation);
        exprStack.append(innerCondition);
        posStack.append(std::make_pair(innerStart, innerEnd));
        statementStack.append(innerTrueBlock);
    } while (match(ELSE));

    if (!trailingElse) {
        TreeExpression condition = exprStack.last();
        exprStack.removeLast();
        TreeStatement trueBlock = statementStack.last();
        statementStack.removeLast();
        std::pair<int, int> pos = posStack.last();
        posStack.removeLast();
        JSTokenLocation elseLocation = tokenLocationStack.last();
        tokenLocationStack.removeLast();
        TreeStatement ifStatement = context.createIfStatement(elseLocation, condition, trueBlock, 0, pos.first, pos.second);
        context.setEndOffset(ifStatement, context.endOffset(trueBlock));
        statementStack.append(ifStatement);
    }

    while (!exprStack.isEmpty()) {
        TreeExpression condition = exprStack.last();
        exprStack.removeLast();
        TreeStatement falseBlock = statementStack.last();
        statementStack.removeLast();
        TreeStatement trueBlock = statementStack.last();
        statementStack.removeLast();
        std::pair<int, int> pos = posStack.last();
        posStack.removeLast();
        JSTokenLocation elseLocation = tokenLocationStack.last();
        tokenLocationStack.removeLast();
        TreeStatement ifStatement = context.createIfStatement(elseLocation, condition, trueBlock, falseBlock, pos.first, pos.second);
        context.setEndOffset(ifStatement, context.endOffset(falseBlock));
        statementStack.append(ifStatement);
    }

    return context.createIfStatement(ifLocation, condition, trueBlock, statementStack.last(), start, end);
}

template <typename LexerType>
template <class TreeBuilder> typename TreeBuilder::ModuleName Parser<LexerType>::parseModuleName(TreeBuilder& context)
{
    // ModuleName (ModuleSpecifier in the spec) represents the module name imported by the script.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-imports
    // http://www.ecma-international.org/ecma-262/6.0/#sec-exports
    JSTokenLocation specifierLocation(tokenLocation());
    failIfFalse(match(STRING), "Imported modules names must be string literals");
    const Identifier* moduleName = m_token.m_data.ident;
    next();
    return context.createModuleName(specifierLocation, *moduleName);
}

template <typename LexerType>
template <class TreeBuilder> typename TreeBuilder::ImportSpecifier Parser<LexerType>::parseImportClauseItem(TreeBuilder& context, ImportSpecifierType specifierType)
{
    // Produced node is the item of the ImportClause.
    // That is the ImportSpecifier, ImportedDefaultBinding or NameSpaceImport.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-imports
    JSTokenLocation specifierLocation(tokenLocation());
    JSToken localNameToken;
    const Identifier* importedName = nullptr;
    const Identifier* localName = nullptr;

    switch (specifierType) {
    case ImportSpecifierType::NamespaceImport: {
        // NameSpaceImport :
        // * as ImportedBinding
        // e.g.
        //     * as namespace
        ASSERT(match(TIMES));
        importedName = &m_vm.propertyNames->timesIdentifier;
        next();

        failIfFalse(matchContextualKeyword(m_vm.propertyNames->as), "Expected 'as' before imported binding name");
        next();

        failIfFalse(matchSpecIdentifier(), "Expected a variable name for the import declaration");
        localNameToken = m_token;
        localName = m_token.m_data.ident;
        next();
        break;
    }

    case ImportSpecifierType::NamedImport: {
        // ImportSpecifier :
        // ImportedBinding
        // IdentifierName as ImportedBinding
        // ModuleExportName as ImportedBinding
        // e.g.
        //     A
        //     A as B
        ASSERT(matchIdentifierOrKeyword() || match(STRING));
        bool isModuleExportName = match(STRING);
        localName = m_token.m_data.ident;
        importedName = localName;
        localNameToken = m_token;
        if (isModuleExportName)
            failIfTrue(hasUnpairedSurrogate(localName->string()), "Expected a well-formed-unicode string for the module export name");
        next();

        bool useAs = matchContextualKeyword(m_vm.propertyNames->as);
        if (isModuleExportName)
            failIfFalse(useAs, "Expected 'as' after the module export name string");
        if (useAs) {
            next();
            failIfFalse(matchSpecIdentifier(), "Expected a variable name for the import declaration");
            localNameToken = m_token;
            localName = m_token.m_data.ident;
            next();
        }
        break;
    }

    case ImportSpecifierType::DefaultImport: {
        // ImportedDefaultBinding :
        // ImportedBinding
        ASSERT(matchSpecIdentifier());
        localNameToken = m_token;
        localName = m_token.m_data.ident;
        importedName = &m_vm.propertyNames->defaultKeyword;
        next();
        break;
    }
    }

    semanticFailIfTrue(localNameToken.m_type == AWAIT, "Cannot use 'await' as an imported binding name");
    semanticFailIfTrue(localNameToken.m_type & KeywordTokenFlag, "Cannot use keyword as imported binding name");
    DeclarationResultMask declarationResult = declareVariable(localName, DeclarationType::ConstDeclaration, (specifierType == ImportSpecifierType::NamespaceImport) ? DeclarationImportType::ImportedNamespace : DeclarationImportType::Imported);
    if (declarationResult != DeclarationResult::Valid) {
        failIfTrueIfStrict(declarationResult & DeclarationResult::InvalidStrictMode, "Cannot declare an imported binding named ", localName->impl(), " in strict mode");
        if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration)
            internalFailWithMessage(false, "Cannot declare an imported binding name twice: '", localName->impl(), "'");
    }

    return context.createImportSpecifier(specifierLocation, *importedName, *localName);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseImportDeclaration(TreeBuilder& context)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-imports
    ASSERT(match(IMPORT));
    JSTokenLocation importLocation(tokenLocation());
    next();

    auto specifierList = context.createImportSpecifierList();

    if (match(STRING)) {
        // import ModuleSpecifier ;
        auto moduleName = parseModuleName(context);
        failIfFalse(moduleName, "Cannot parse the module name");
        failIfFalse(autoSemiColon(), "Expected a ';' following a targeted import declaration");
        return context.createImportDeclaration(importLocation, specifierList, moduleName);
    }

    bool isFinishedParsingImport = false;
    if (matchSpecIdentifier()) {
        // ImportedDefaultBinding :
        // ImportedBinding
        auto specifier = parseImportClauseItem(context, ImportSpecifierType::DefaultImport);
        failIfFalse(specifier, "Cannot parse the default import");
        context.appendImportSpecifier(specifierList, specifier);
        if (match(COMMA))
            next();
        else
            isFinishedParsingImport = true;
    }

    if (!isFinishedParsingImport) {
        if (match(TIMES)) {
            // import NameSpaceImport FromClause ;
            auto specifier = parseImportClauseItem(context, ImportSpecifierType::NamespaceImport);
            failIfFalse(specifier, "Cannot parse the namespace import");
            context.appendImportSpecifier(specifierList, specifier);
        } else if (match(OPENBRACE)) {
            // NamedImports :
            // { }
            // { ImportsList }
            // { ImportsList , }
            next();

            while (!match(CLOSEBRACE)) {
                failIfFalse(matchIdentifierOrKeyword() || match(STRING), "Expected an imported name or a module export name string for the import declaration");
                auto specifier = parseImportClauseItem(context, ImportSpecifierType::NamedImport);
                failIfFalse(specifier, "Cannot parse the named import");
                context.appendImportSpecifier(specifierList, specifier);
                if (!consume(COMMA))
                    break;
            }
            handleProductionOrFail2(CLOSEBRACE, "}", "end", "import list");
        } else
            failWithMessage("Expected namespace import or import list");
    }

    // FromClause :
    // from ModuleSpecifier

    failIfFalse(matchContextualKeyword(m_vm.propertyNames->from), "Expected 'from' before imported module name");
    next();

    auto moduleName = parseModuleName(context);
    failIfFalse(moduleName, "Cannot parse the module name");
    failIfFalse(autoSemiColon(), "Expected a ';' following a targeted import declaration");

    return context.createImportDeclaration(importLocation, specifierList, moduleName);
}

template <typename LexerType>
template <class TreeBuilder> typename TreeBuilder::ExportSpecifier Parser<LexerType>::parseExportSpecifier(TreeBuilder& context, Vector<std::pair<const Identifier*, const Identifier*>>& maybeExportedLocalNames, bool& hasKeywordForLocalBindings, bool& hasReferencedModuleExportNames)
{
    // ExportSpecifier :
    // IdentifierName
    // IdentifierName as IdentifierName
    // IdentifierName as ModuleExportName
    // ModuleExportName
    // ModuleExportName as IdentifierName
    // ModuleExportName as ModuleExportName
    // http://www.ecma-international.org/ecma-262/6.0/#sec-exports
    ASSERT(matchIdentifierOrKeyword() || match(STRING));
    JSTokenLocation specifierLocation(tokenLocation());
    const Identifier* localName = m_token.m_data.ident;
    const Identifier* exportedName = localName;
    if (match(STRING)) {
        hasReferencedModuleExportNames = true;
        failIfTrue(hasUnpairedSurrogate(exportedName->string()), "Expected a well-formed-unicode string for the module export name");
    } else {
        if (m_token.m_type & KeywordTokenFlag)
            hasKeywordForLocalBindings = true;
    }
    next();

    if (matchContextualKeyword(m_vm.propertyNames->as)) {
        next();
        failIfFalse(matchIdentifierOrKeyword() || match(STRING), "Expected an exported name or a module export name string for the export declaration");
        exportedName = m_token.m_data.ident;
        if (match(STRING))
            failIfTrue(hasUnpairedSurrogate(exportedName->string()), "Expected a well-formed-unicode string for the module export name");
        next();
    }

    semanticFailIfFalse(exportName(*exportedName), "Cannot export a duplicate name '", exportedName->impl(), "'");
    maybeExportedLocalNames.append(std::make_pair(localName, exportedName));
    return context.createExportSpecifier(specifierLocation, *localName, *exportedName);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseExportDeclaration(TreeBuilder& context)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-exports
    ASSERT(match(EXPORT_));
    JSTokenLocation exportLocation(tokenLocation());
    next();

    switch (m_token.m_type) {
    case TIMES: {
        // export * FromClause ;
        // export * as IdentifierName FromClause ;
        // export * as ModuleExportName FromClause ;
        next();

        const Identifier* exportedName = nullptr;
        JSTokenLocation specifierLocation;
        if (matchContextualKeyword(m_vm.propertyNames->as)) {
            next();
            specifierLocation = JSTokenLocation(tokenLocation());
            failIfFalse(matchIdentifierOrKeyword() || match(STRING), "Expected an exported name or a module export name string for the export declaration");
            exportedName = m_token.m_data.ident;
            if (match(STRING))
                failIfTrue(hasUnpairedSurrogate(exportedName->string()), "Expected a well-formed-unicode string for the module export name");
            next();
        }

        failIfFalse(matchContextualKeyword(m_vm.propertyNames->from), "Expected 'from' before exported module name");
        next();
        auto moduleName = parseModuleName(context);
        failIfFalse(moduleName, "Cannot parse the 'from' clause");
        failIfFalse(autoSemiColon(), "Expected a ';' following a targeted export declaration");

        if (exportedName) {
            semanticFailIfFalse(exportName(*exportedName), "Cannot export a duplicate name '", exportedName->impl(), "'");
            auto specifierList = context.createExportSpecifierList();
            auto localName = &m_vm.propertyNames->starNamespacePrivateName;
            auto specifier = context.createExportSpecifier(specifierLocation, *localName, *exportedName);
            context.appendExportSpecifier(specifierList, specifier);
            return context.createExportNamedDeclaration(exportLocation, specifierList, moduleName);
        }

        return context.createExportAllDeclaration(exportLocation, moduleName);
    }

    case DEFAULT: {
        // export default HoistableDeclaration[~Yield, ~Await, +Default]
        // export default ClassDeclaration[~Yield, ~Await, +Default]
        // export default [lookahead not-in { function, async [no LineTerminator here] function, class }] AssignmentExpression[+In, ~Yield, ~Await]

        next();

        TreeStatement result = 0;
        bool isFunctionOrClassDeclaration = false;
        const Identifier* localName = nullptr;

        bool startsWithFunction = match(FUNCTION);
        if (startsWithFunction || match(CLASSTOKEN)) {
            SavePoint savePoint = createSavePoint(context);
            isFunctionOrClassDeclaration = true;
            next();

            // ES6 Generators
            if (startsWithFunction && match(TIMES))
                next();
            if (match(IDENT))
                localName = m_token.m_data.ident;
            restoreSavePoint(context, savePoint);
        } else if (matchContextualKeyword(m_vm.propertyNames->async)) {
            // export default async function xxx() { }
            // export default async function * yyy() { }
            SavePoint savePoint = createSavePoint(context);
            next();
            if (match(FUNCTION) && !m_lexer->hasLineTerminatorBeforeToken()) {
                next();
                // Async Generators
                if (match(TIMES))
                    next();
                if (match(IDENT))
                    localName = m_token.m_data.ident;
                isFunctionOrClassDeclaration = true;
            }
            restoreSavePoint(context, savePoint);
        }

        if (!localName)
            localName = &m_vm.propertyNames->starDefaultPrivateName;

        if (isFunctionOrClassDeclaration) {
            if (startsWithFunction) {
                ASSERT(match(FUNCTION));
                DepthManager statementDepth(&m_statementDepth);
                m_statementDepth = 1;
                result = parseFunctionDeclaration(context, FunctionDeclarationType::Declaration, ExportType::NotExported, DeclarationDefaultContext::ExportDefault);
            } else if (match(CLASSTOKEN)) {
                result = parseClassDeclaration(context, ExportType::NotExported, DeclarationDefaultContext::ExportDefault);
            } else {
                ASSERT(matchContextualKeyword(m_vm.propertyNames->async));
                next();
                DepthManager statementDepth(&m_statementDepth);
                m_statementDepth = 1;
                result = parseAsyncFunctionDeclaration(context, ExportType::NotExported, DeclarationDefaultContext::ExportDefault);
            }
        } else {
            // export default expr;
            //
            // It should be treated as the same to the following.
            //
            // const *default* = expr;
            // export { *default* as default }
            //
            // In the above example, *default* is the invisible variable to the users.
            // We use the private symbol to represent the name of this variable.
            JSTokenLocation location(tokenLocation());
            JSTextPosition start = tokenStartPosition();
            TreeExpression expression = parseAssignmentExpression(context);
            failIfFalse(expression, "Cannot parse expression");

            DeclarationResultMask declarationResult = declareVariable(&m_vm.propertyNames->starDefaultPrivateName, DeclarationType::ConstDeclaration);
            if (declarationResult & DeclarationResult::InvalidDuplicateDeclaration)
                internalFailWithMessage(false, "Only one 'default' export is allowed");

            TreeExpression assignment = context.createAssignResolve(location, m_vm.propertyNames->starDefaultPrivateName, expression, start, start, tokenEndPosition(), AssignmentContext::ConstDeclarationStatement);
            result = context.createExprStatement(location, assignment, start, tokenEndPosition());
            failIfFalse(autoSemiColon(), "Expected a ';' following a targeted export declaration");
        }
        failIfFalse(result, "Cannot parse the declaration");

        semanticFailIfFalse(exportName(m_vm.propertyNames->defaultKeyword), "Only one 'default' export is allowed");
        m_moduleScopeData->exportBinding(*localName, m_vm.propertyNames->defaultKeyword);
        return context.createExportDefaultDeclaration(exportLocation, result, *localName);
    }

    case OPENBRACE: {
        // export ExportClause FromClause ;
        // export ExportClause ;
        //
        // ExportClause :
        // { }
        // { ExportsList }
        // { ExportsList , }
        //
        // ExportsList :
        // ExportSpecifier
        // ExportsList , ExportSpecifier

        next();

        auto specifierList = context.createExportSpecifierList();
        Vector<std::pair<const Identifier*, const Identifier*>> maybeExportedLocalNames;

        bool hasKeywordForLocalBindings = false;
        bool hasReferencedModuleExportNames = false;
        while (!match(CLOSEBRACE)) {
            failIfFalse(matchIdentifierOrKeyword() || match(STRING), "Expected a variable name or a module export name string for the export declaration");
            auto specifier = parseExportSpecifier(context, maybeExportedLocalNames, hasKeywordForLocalBindings, hasReferencedModuleExportNames);
            failIfFalse(specifier, "Cannot parse the named export");
            context.appendExportSpecifier(specifierList, specifier);
            if (!consume(COMMA))
                break;
        }
        handleProductionOrFail2(CLOSEBRACE, "}", "end", "export list");

        typename TreeBuilder::ModuleName moduleName = 0;
        if (matchContextualKeyword(m_vm.propertyNames->from)) {
            next();
            moduleName = parseModuleName(context);
            failIfFalse(moduleName, "Cannot parse the 'from' clause");
        } else
            semanticFailIfTrue(hasReferencedModuleExportNames, "Cannot use module export names if they reference variable names in the current module");
        failIfFalse(autoSemiColon(), "Expected a ';' following a targeted export declaration");

        if (!moduleName) {
            semanticFailIfTrue(hasKeywordForLocalBindings, "Cannot use keyword as exported variable name");
            // Since this export declaration does not have module specifier part, it exports the local bindings.
            // While the export declaration with module specifier does not have any effect on the current module's scope,
            // the export named declaration without module specifier references the local binding names.
            // For example,
            //   export { A, B, C as D } from "mod"
            // does not have effect on the current module's scope. But,
            //   export { A, B, C as D }
            // will reference the current module's bindings.
            for (const auto& pair : maybeExportedLocalNames) {
                const Identifier* localName = pair.first;
                const Identifier* exportedName = pair.second;
                m_moduleScopeData->exportBinding(*localName, *exportedName);
            }
        }

        return context.createExportNamedDeclaration(exportLocation, specifierList, moduleName);
    }

    default: {
        // export VariableStatement
        // export Declaration
        TreeStatement result = 0;
        switch (m_token.m_type) {
        case VAR:
            result = parseVariableDeclaration(context, DeclarationType::VarDeclaration, ExportType::Exported);
            break;

        case CONSTTOKEN:
            result = parseVariableDeclaration(context, DeclarationType::ConstDeclaration, ExportType::Exported);
            break;

        case LET:
            result = parseVariableDeclaration(context, DeclarationType::LetDeclaration, ExportType::Exported);
            break;

        case FUNCTION: {
            DepthManager statementDepth(&m_statementDepth);
            m_statementDepth = 1;
            result = parseFunctionDeclaration(context, FunctionDeclarationType::Declaration, ExportType::Exported);
            break;
        }

        case CLASSTOKEN:
            result = parseClassDeclaration(context, ExportType::Exported);
            break;

        case IDENT:
            if (*m_token.m_data.ident == m_vm.propertyNames->async && !m_token.m_data.escaped) {
                next();
                semanticFailIfFalse(match(FUNCTION) && !m_lexer->hasLineTerminatorBeforeToken(), "Expected 'function' keyword following 'async' keyword with no preceding line terminator");
                DepthManager statementDepth(&m_statementDepth);
                m_statementDepth = 1;
                result = parseAsyncFunctionDeclaration(context, ExportType::Exported);
                break;
            }
            FALLTHROUGH;
        default:
            failWithMessage("Expected either a declaration or a variable statement");
            break;
        }

        failIfFalse(result, "Cannot parse the declaration");
        return context.createExportLocalDeclaration(exportLocation, result);
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseExpression(TreeBuilder& context, IsOnlyChildOfStatement isStatement)
{
    failIfStackOverflow();
    JSTokenLocation headLocation(tokenLocation());
    TreeExpression node = parseAssignmentExpression(context);
    failIfFalse(node, "Cannot parse expression");
    context.setEndOffset(node, m_lastTokenEndPosition.offset);
    if (!match(COMMA))
        return node;
    next();
    m_parserState.nonTrivialExpressionCount++;
    m_parserState.nonLHSCount++;
    JSTokenLocation tailLocation(tokenLocation());
    TreeExpression right = parseAssignmentExpression(context);
    failIfFalse(right, "Cannot parse expression in a comma expression");
    context.setEndOffset(right, m_lastTokenEndPosition.offset);
    typename TreeBuilder::Comma head = context.createCommaExpr(headLocation, node);
    if (isStatement == IsOnlyChildOfStatement::Yes)
        recordPauseLocation(context.breakpointLocation(head));
    typename TreeBuilder::Comma tail = context.appendToCommaExpr(tailLocation, head, head, right);
    if (isStatement == IsOnlyChildOfStatement::Yes)
        recordPauseLocation(context.breakpointLocation(tail));
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        tailLocation = tokenLocation();
        right = parseAssignmentExpression(context);
        failIfFalse(right, "Cannot parse expression in a comma expression");
        context.setEndOffset(right, m_lastTokenEndPosition.offset);
        tail = context.appendToCommaExpr(tailLocation, head, tail, right);
        if (isStatement == IsOnlyChildOfStatement::Yes)
            recordPauseLocation(context.breakpointLocation(tail));
    }
    context.setEndOffset(head, m_lastTokenEndPosition.offset);
    return head;
}

template <typename LexerType>
template <typename TreeBuilder> TreeExpression Parser<LexerType>::parseAssignmentExpressionOrPropagateErrorClass(TreeBuilder& context)
{
    ExpressionErrorClassifier classifier(this);
    auto assignment = parseAssignmentExpression(context, classifier);
    if (!assignment)
        classifier.propagateExpressionErrorClass();
    return assignment;
}

template <typename LexerType>
template <typename TreeBuilder> TreeExpression Parser<LexerType>::parseAssignmentExpression(TreeBuilder& context)
{
    ExpressionErrorClassifier classifier(this);
    return parseAssignmentExpression(context, classifier);
}


template <typename LexerType>
template <typename TreeBuilder> NEVER_INLINE const char* Parser<LexerType>::metaPropertyName(TreeBuilder& context, TreeExpression expr)
{
    if (context.isNewTarget(expr))
        return "new.target";
    if (context.isImportMeta(expr))
        return "import.meta";
    RELEASE_ASSERT_NOT_REACHED();
    return "error";
}

template <typename LexerType>
template <typename TreeBuilder> bool Parser<LexerType>::isSimpleAssignmentTarget(TreeBuilder& context, TreeExpression expr)
{
    // Web compatibility concerns prevent us from handling a function call LHS as an early error in sloppy mode.
    // This behavior is currently unspecified, but see: https://github.com/tc39/ecma262/issues/257#issuecomment-195106880
    return context.isLocation(expr) || (!strictMode() && context.isFunctionCall(expr));
}
    
template <typename LexerType>
template <typename TreeBuilder> TreeExpression Parser<LexerType>::parseAssignmentExpression(TreeBuilder& context, ExpressionErrorClassifier& classifier)
{
    ASSERT(!hasError());
    
    failIfStackOverflow();

    if (match(YIELD) && !canUseIdentifierYield())
        return parseYieldExpression(context);

    JSTextPosition start = tokenStartPosition();
    JSTokenLocation location(tokenLocation());
    int initialAssignmentCount = m_parserState.assignmentCount;
    int initialNonLHSCount = m_parserState.nonLHSCount;
    bool maybeAssignmentPattern = match(OPENBRACE) || match(OPENBRACKET);
    bool wasOpenParen = match(OPENPAREN);
    // Do not use matchSpecIdentifier() here since it is slower than isIdentifierOrKeyword.
    // Whether spec identifier is will be validated by isArrowFunctionParameters().
    bool wasIdentifierOrKeyword = matchIdentifierOrKeyword() || (m_token.m_type == ESCAPED_KEYWORD);
    bool maybeValidArrowFunctionStart = wasOpenParen || wasIdentifierOrKeyword;
    SavePoint savePoint = createSavePoint(context);
    size_t usedVariablesSize = 0;

    if (wasOpenParen) {
        usedVariablesSize = currentScope()->currentUsedVariablesSize();
        currentScope()->pushUsedVariableSet();
    }

    TreeExpression lhs = parseConditionalExpression(context);

    if (maybeValidArrowFunctionStart && !match(EOFTOK)) {
        bool isArrowFunctionToken = match(ARROWFUNCTION);
        if (!lhs || isArrowFunctionToken) {
            SavePointWithError errorRestorationSavePoint = swapSavePointForError(context, savePoint);
            bool isAsyncArrow = false;
            if (UNLIKELY(classifier.indicatesPossibleAsyncArrowFunction())) {
                if (matchContextualKeyword(m_vm.propertyNames->async)) {
                    next();
                    isAsyncArrow = !m_lexer->hasLineTerminatorBeforeToken();
                }
            }
            if (isArrowFunctionParameters(context)) {
                if (wasOpenParen)
                    currentScope()->revertToPreviousUsedVariables(usedVariablesSize);
                return parseArrowFunctionExpression(context, isAsyncArrow);
            }
            if (isArrowFunctionToken)
                propagateError();
            restoreSavePointWithError(context, errorRestorationSavePoint);
            if (isArrowFunctionToken)
                failDueToUnexpectedToken();
        }
    }

    if (!lhs && (!maybeAssignmentPattern || !classifier.indicatesPossiblePattern()))
        propagateError();

    if (maybeAssignmentPattern && (!lhs || (context.isObjectOrArrayLiteral(lhs) && match(EQUAL)))) {
        SavePointWithError expressionErrorLocation = swapSavePointForError(context, savePoint);
        auto pattern = tryParseDestructuringPatternExpression(context, AssignmentContext::AssignmentExpression);
        if (classifier.indicatesPossiblePattern() && (!pattern || !match(EQUAL))) {
            restoreSavePointWithError(context, expressionErrorLocation);
            return 0;
        }
        failIfFalse(pattern, "Cannot parse assignment pattern");
        consumeOrFail(EQUAL, "Expected '=' following assignment pattern");
        auto rhs = parseAssignmentExpression(context);
        if (!rhs)
            propagateError();
        return context.createDestructuringAssignment(location, pattern, rhs);
    }

    failIfFalse(lhs, "Cannot parse expression");
    if (initialNonLHSCount != m_parserState.nonLHSCount) {
        if (m_token.m_type >= EQUAL && m_token.m_type <= ANDEQUAL)
            semanticFail("Left hand side of operator '", getToken(), "' must be a reference");

        return lhs;
    }

    int assignmentStack = 0;
    Operator op;
    bool hadAssignment = false;
    while (true) {
        switch (m_token.m_type) {
        case EQUAL: op = Operator::Equal; break;
        case PLUSEQUAL: op = Operator::PlusEq; break;
        case MINUSEQUAL: op = Operator::MinusEq; break;
        case MULTEQUAL: op = Operator::MultEq; break;
        case DIVEQUAL: op = Operator::DivEq; break;
        case LSHIFTEQUAL: op = Operator::LShift; break;
        case RSHIFTEQUAL: op = Operator::RShift; break;
        case URSHIFTEQUAL: op = Operator::URShift; break;
        case BITANDEQUAL: op = Operator::BitAndEq; break;
        case BITXOREQUAL: op = Operator::BitXOrEq; break;
        case BITOREQUAL: op = Operator::BitOrEq; break;
        case MODEQUAL: op = Operator::ModEq; break;
        case POWEQUAL: op = Operator::PowEq; break;
        case COALESCEEQUAL: op = Operator::CoalesceEq; break;
        case OREQUAL: op = Operator::OrEq; break;
        case ANDEQUAL: op = Operator::AndEq; break;
        default:
            goto end;
        }
        m_parserState.nonTrivialExpressionCount++;
        hadAssignment = true;
        semanticFailIfTrue(context.isMetaProperty(lhs), metaPropertyName(context, lhs), " can't be the left hand side of an assignment expression");
        semanticFailIfFalse(isSimpleAssignmentTarget(context, lhs), "Left side of assignment is not a reference");
        context.assignmentStackAppend(assignmentStack, lhs, start, tokenStartPosition(), m_parserState.assignmentCount, op);
        start = tokenStartPosition();
        m_parserState.assignmentCount++;
        next(TreeBuilder::DontBuildStrings);
        if (strictMode() && m_parserState.lastIdentifier && context.isResolve(lhs)) {
            failIfTrueIfStrict(m_vm.propertyNames->eval == *m_parserState.lastIdentifier, "Cannot modify 'eval' in strict mode");
            failIfTrueIfStrict(m_vm.propertyNames->arguments == *m_parserState.lastIdentifier, "Cannot modify 'arguments' in strict mode");
            m_parserState.lastIdentifier = nullptr;
        }
        lhs = parseAssignmentExpression(context);
        failIfFalse(lhs, "Cannot parse the right hand side of an assignment expression");
        if (initialNonLHSCount != m_parserState.nonLHSCount) {
            if (m_token.m_type >= EQUAL && m_token.m_type <= ANDEQUAL)
                semanticFail("Left hand side of operator '", getToken(), "' must be a reference");
            break;
        }
    }
end:
    if (hadAssignment)
        m_parserState.nonLHSCount++;
    
    while (assignmentStack)
        lhs = context.createAssignment(location, assignmentStack, lhs, initialAssignmentCount, m_parserState.assignmentCount, lastTokenEndPosition());
    
    return lhs;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseYieldExpression(TreeBuilder& context)
{
    // YieldExpression[In] :
    //     yield
    //     yield [no LineTerminator here] AssignmentExpression[?In, Yield]
    //     yield [no LineTerminator here] * AssignmentExpression[?In, Yield]

    // http://ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions
    failIfFalse(currentScope()->isGenerator() && !currentScope()->isArrowFunctionBoundary(), "Cannot use yield expression out of generator");

    // http://ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions-static-semantics-early-errors
    failIfTrue(m_parserState.functionParsePhase == FunctionParsePhase::Parameters, "Cannot use yield expression within parameters");

    JSTokenLocation location(tokenLocation());
    JSTextPosition divotStart = tokenStartPosition();
    ASSERT(match(YIELD));
    SavePoint savePoint = createSavePoint(context);
    next();
    if (m_lexer->hasLineTerminatorBeforeToken())
        return context.createYield(location);

    bool delegate = consume(TIMES);
    JSTextPosition argumentStart = tokenStartPosition();
    TreeExpression argument = parseAssignmentExpression(context);
    if (!argument) {
        restoreSavePoint(context, savePoint);
        next();
        return context.createYield(location);
    }
    return context.createYield(location, argument, delegate, divotStart, argumentStart, lastTokenEndPosition());
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseAwaitExpression(TreeBuilder& context)
{
    ASSERT(match(AWAIT));
    ASSERT(currentScope()->isAsyncFunction() || isModuleParseMode(sourceParseMode()));
    ASSERT(isAsyncFunctionParseMode(sourceParseMode()) || isModuleParseMode(sourceParseMode()));
    ASSERT(m_parserState.functionParsePhase != FunctionParsePhase::Parameters);
    JSTokenLocation location(tokenLocation());
    JSTextPosition divotStart = tokenStartPosition();
    next();
    JSTextPosition argumentStart = tokenStartPosition();
    ExpressionErrorClassifier classifier(this);
    TreeExpression argument = parseUnaryExpression(context);
    failIfFalse(argument, "Failed to parse await expression");
    return context.createAwait(location, argument, divotStart, argumentStart, lastTokenEndPosition());
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseConditionalExpression(TreeBuilder& context)
{
    JSTokenLocation location(tokenLocation());
    TreeExpression cond = parseBinaryExpression(context);
    failIfFalse(cond, "Cannot parse expression");
    if (!match(QUESTION))
        return cond;
    m_parserState.nonTrivialExpressionCount++;
    m_parserState.nonLHSCount++;
    next(TreeBuilder::DontBuildStrings);
    TreeExpression lhs = 0;
    {
        // this block is necessary so that we don't leave `in` enabled for the rhs
        AllowInOverride allowInOverride(this);
        lhs = parseAssignmentExpression(context);
    }
    failIfFalse(lhs, "Cannot parse left hand side of ternary operator");
    context.setEndOffset(lhs, m_lastTokenEndPosition.offset);
    consumeOrFailWithFlags(COLON, TreeBuilder::DontBuildStrings, "Expected ':' in ternary operator");
    
    TreeExpression rhs = parseAssignmentExpression(context);
    failIfFalse(rhs, "Cannot parse right hand side of ternary operator");
    context.setEndOffset(rhs, m_lastTokenEndPosition.offset);
    return context.createConditionalExpr(location, cond, lhs, rhs);
}

ALWAYS_INLINE static bool isUnaryOpExcludingUpdateOp(JSTokenType token)
{
    if (isUpdateOp(token))
        return false;
    return isUnaryOp(token);
}

template <typename LexerType>
int Parser<LexerType>::isBinaryOperator(JSTokenType token)
{
    if (m_allowsIn)
        return token & (BinaryOpTokenPrecedenceMask << BinaryOpTokenAllowsInPrecedenceAdditionalShift);
    return token & BinaryOpTokenPrecedenceMask;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseBinaryExpression(TreeBuilder& context)
{
    int operandStackDepth = 0;
    int operatorStackDepth = 0;
    typename TreeBuilder::BinaryExprContext binaryExprContext(context);
    JSTokenLocation location(tokenLocation());
    bool hasLogicalOperator = false;
    bool hasCoalesceOperator = false;

    while (true) {
        JSTextPosition exprStart = tokenStartPosition();
        int initialAssignments = m_parserState.assignmentCount;
        JSTokenType leadingTokenTypeForUnaryExpression = m_token.m_type;
        TreeExpression current = parseUnaryExpression(context);
        failIfFalse(current, "Cannot parse expression");

        context.appendBinaryExpressionInfo(operandStackDepth, current, exprStart, lastTokenEndPosition(), lastTokenEndPosition(), initialAssignments != m_parserState.assignmentCount);
        int precedence = isBinaryOperator(m_token.m_type);
        if (!precedence)
            break;

        // 12.6 https://tc39.github.io/ecma262/#sec-exp-operator
        // ExponentiationExpresion is described as follows.
        //
        //     ExponentiationExpression[Yield]:
        //         UnaryExpression[?Yield]
        //         UpdateExpression[?Yield] ** ExponentiationExpression[?Yield]
        //
        // As we can see, the left hand side of the ExponentiationExpression is UpdateExpression, not UnaryExpression.
        // So placing UnaryExpression not included in UpdateExpression here is a syntax error.
        // This is intentional. For example, if UnaryExpression is allowed, we can have the code like `-x**y`.
        // But this is confusing: `-(x**y)` OR `(-x)**y`, which interpretation is correct?
        // To avoid this problem, ECMA262 makes unparenthesized exponentiation expression as operand of unary operators an early error.
        // More rationale: https://mail.mozilla.org/pipermail/es-discuss/2015-September/044232.html
        //
        // Here, we guarantee that the left hand side of this expression is not unary expression by checking the leading operator of the parseUnaryExpression.
        // This check just works. Let's consider the example,
        //     y <> -x ** z
        //          ^
        //          Check this.
        // If the binary operator <> has higher precedence than one of "**", this check does not work.
        // But it's OK for ** because the operator "**" has the highest operator precedence in the binary operators.
        failIfTrue(match(POW) && isUnaryOpExcludingUpdateOp(leadingTokenTypeForUnaryExpression), "Ambiguous unary expression in the left hand side of the exponentiation expression; parentheses must be used to disambiguate the expression");

        // Mixing ?? with || or && is currently specified as an early error.
        // Since ?? is the lowest-precedence binary operator, it suffices to check whether these ever coexist in the operator stack.
        if (match(AND) || match(OR))
            hasLogicalOperator = true;
        else if (match(COALESCE))
            hasCoalesceOperator = true;
        failIfTrue(hasLogicalOperator && hasCoalesceOperator, "Coalescing and logical operators used together in the same expression; parentheses must be used to disambiguate");

        m_parserState.nonTrivialExpressionCount++;
        m_parserState.nonLHSCount++;
        int operatorToken = m_token.m_type;
        next(TreeBuilder::DontBuildStrings);
        
        while (operatorStackDepth && context.operatorStackShouldReduce(precedence)) {
            ASSERT(operandStackDepth > 1);
            
            typename TreeBuilder::BinaryOperand rhs = context.getFromOperandStack(-1);
            typename TreeBuilder::BinaryOperand lhs = context.getFromOperandStack(-2);
            context.shrinkOperandStackBy(operandStackDepth, 2);
            context.appendBinaryOperation(location, operandStackDepth, operatorStackDepth, lhs, rhs);
            context.operatorStackPop(operatorStackDepth);
        }
        context.operatorStackAppend(operatorStackDepth, operatorToken, precedence);
    }
    while (operatorStackDepth) {
        ASSERT(operandStackDepth > 1);
        
        typename TreeBuilder::BinaryOperand rhs = context.getFromOperandStack(-1);
        typename TreeBuilder::BinaryOperand lhs = context.getFromOperandStack(-2);
        context.shrinkOperandStackBy(operandStackDepth, 2);
        context.appendBinaryOperation(location, operandStackDepth, operatorStackDepth, lhs, rhs);
        context.operatorStackPop(operatorStackDepth);
    }
    return context.popOperandStack(operandStackDepth);
}

template <typename LexerType>
template <class TreeBuilder> TreeProperty Parser<LexerType>::parseProperty(TreeBuilder& context)
{
    SourceParseMode parseMode = SourceParseMode::MethodMode;
    bool wasIdent = false;

    if (consume(TIMES))
        parseMode = SourceParseMode::GeneratorWrapperMethodMode;

parseProperty:
    switch (m_token.m_type) {
    case ESCAPED_KEYWORD:
    case IDENT:
        if (UNLIKELY(*m_token.m_data.ident == m_vm.propertyNames->async && !m_token.m_data.escaped)) {
            if (parseMode == SourceParseMode::MethodMode) {
                SavePoint savePoint = createSavePoint(context);
                next();

                if (match(COLON) || match(OPENPAREN) || match(COMMA) || match(CLOSEBRACE)) {
                    restoreSavePoint(context, savePoint);
                    wasIdent = true;
                    goto namedProperty;
                }

                failIfTrue(m_lexer->hasLineTerminatorBeforeToken(), "Expected a property name following keyword 'async'");
                if (UNLIKELY(consume(TIMES)))
                    parseMode = SourceParseMode::AsyncGeneratorWrapperMethodMode;
                else
                    parseMode = SourceParseMode::AsyncMethodMode;
                goto parseProperty;
            }
        }
        FALLTHROUGH;
    case YIELD:
    case AWAIT:
        wasIdent = true;
        FALLTHROUGH;
    case STRING: {
namedProperty:
        const Identifier* ident = m_token.m_data.ident;
        bool escaped = m_token.m_data.escaped;
        unsigned getterOrSetterStartOffset = tokenStart();
        JSToken identToken = m_token;

        if (wasIdent && !isGeneratorMethodParseMode(parseMode) && (!escaped && (*ident == m_vm.propertyNames->get || *ident == m_vm.propertyNames->set)))
            nextExpectIdentifier(LexerFlags::IgnoreReservedWords);
        else
            nextExpectIdentifier(TreeBuilder::DontBuildKeywords | LexerFlags::IgnoreReservedWords);

        if (!isGeneratorMethodParseMode(parseMode) && !isAsyncMethodParseMode(parseMode) && match(COLON)) {
            next();
            TreeExpression node = parseAssignmentExpressionOrPropagateErrorClass(context);
            failIfFalse(node, "Cannot parse expression for property declaration");
            context.setEndOffset(node, m_lexer->currentOffset());
            InferName inferName = ident && *ident == m_vm.propertyNames->underscoreProto ? InferName::Disallowed : InferName::Allowed;
            return context.createProperty(ident, node, PropertyNode::Constant, SuperBinding::NotNeeded, inferName, ClassElementTag::No);
        }

        if (match(OPENPAREN)) {
            SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);
            auto method = parsePropertyMethod(context, ident);
            propagateError();
            return context.createProperty(ident, method, PropertyNode::Constant, SuperBinding::Needed, InferName::Allowed, ClassElementTag::No);
        }
        failIfTrue(parseMode != SourceParseMode::MethodMode, "Expected a parenthesis for argument list");

        failIfFalse(wasIdent, "Expected an identifier as property name");

        if (match(COMMA) || match(CLOSEBRACE)) {
            semanticFailureDueToKeywordCheckingToken(identToken, "shorthand property name");
            JSTextPosition start = tokenStartPosition();
            JSTokenLocation location(tokenLocation());
            currentScope()->useVariable(ident, m_vm.propertyNames->eval == *ident);
            if (currentScope()->isArrowFunction())
                currentScope()->setInnerArrowFunctionUsesEval();
            TreeExpression node = context.createResolve(location, *ident, start, lastTokenEndPosition());
            return context.createProperty(ident, node, static_cast<PropertyNode::Type>(PropertyNode::Constant | PropertyNode::Shorthand), SuperBinding::NotNeeded, InferName::Allowed, ClassElementTag::No);
        }

        if (match(EQUAL)) // CoverInitializedName is exclusive to BindingPattern and AssignmentPattern
            classifyExpressionError(ErrorIndicatesPattern);

        Optional<PropertyNode::Type> type;
        if (!escaped) {
            if (*ident == m_vm.propertyNames->get)
                type = PropertyNode::Getter;
            else if (*ident == m_vm.propertyNames->set)
                type = PropertyNode::Setter;
        }
        if (!type)
            failWithMessage("Expected a ':' following the property name '", ident->impl(), "'");
        return parseGetterSetter(context, type.value(), getterOrSetterStartOffset, ConstructorKind::None, ClassElementTag::No);
    }
    case DOUBLE:
    case INTEGER: {
        double propertyName = m_token.m_data.doubleValue;
        next();

        if (match(OPENPAREN)) {
            SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);
            const Identifier& ident = m_parserArena.identifierArena().makeNumericIdentifier(const_cast<VM&>(m_vm), propertyName);
            auto method = parsePropertyMethod(context, &ident);
            propagateError();
            return context.createProperty(&ident, method, PropertyNode::Constant, SuperBinding::Needed, InferName::Allowed, ClassElementTag::No);
        }
        failIfTrue(parseMode != SourceParseMode::MethodMode, "Expected a parenthesis for argument list");

        consumeOrFail(COLON, "Expected ':' after property name");
        TreeExpression node = parseAssignmentExpression(context);
        failIfFalse(node, "Cannot parse expression for property declaration");
        context.setEndOffset(node, m_lexer->currentOffset());
        return context.createProperty(const_cast<VM&>(m_vm), m_parserArena, propertyName, node, PropertyNode::Constant, SuperBinding::NotNeeded, ClassElementTag::No);
    }
    case BIGINT: {
        const Identifier* ident = &m_parserArena.identifierArena().makeBigIntDecimalIdentifier(const_cast<VM&>(m_vm), *m_token.m_data.bigIntString, m_token.m_data.radix);
        next();

        if (match(OPENPAREN)) {
            SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);
            auto method = parsePropertyMethod(context, ident);
            propagateError();
            return context.createProperty(ident, method, PropertyNode::Constant, SuperBinding::Needed, InferName::Allowed, ClassElementTag::No);
        }
        failIfTrue(parseMode != SourceParseMode::MethodMode, "Expected a parenthesis for argument list");

        consumeOrFail(COLON, "Expected ':' after property name");
        TreeExpression node = parseAssignmentExpression(context);
        failIfFalse(node, "Cannot parse expression for property declaration");
        context.setEndOffset(node, m_lexer->currentOffset());
        return context.createProperty(ident, node, PropertyNode::Constant, SuperBinding::NotNeeded, InferName::Allowed, ClassElementTag::No);
    }
    case OPENBRACKET: {
        next();
        auto propertyName = parseAssignmentExpression(context);
        failIfFalse(propertyName, "Cannot parse computed property name");
        handleProductionOrFail(CLOSEBRACKET, "]", "end", "computed property name");

        if (match(OPENPAREN)) {
            SetForScope<SourceParseMode> innerParseMode(m_parseMode, parseMode);
            auto method = parsePropertyMethod(context, &m_vm.propertyNames->nullIdentifier);
            propagateError();
            return context.createProperty(propertyName, method, static_cast<PropertyNode::Type>(PropertyNode::Constant | PropertyNode::Computed), SuperBinding::Needed, ClassElementTag::No);
        }
        failIfTrue(parseMode != SourceParseMode::MethodMode, "Expected a parenthesis for argument list");

        consumeOrFail(COLON, "Expected ':' after property name");
        TreeExpression node = parseAssignmentExpression(context);
        failIfFalse(node, "Cannot parse expression for property declaration");
        context.setEndOffset(node, m_lexer->currentOffset());
        return context.createProperty(propertyName, node, static_cast<PropertyNode::Type>(PropertyNode::Constant | PropertyNode::Computed), SuperBinding::NotNeeded, ClassElementTag::No);
    }
    case DOTDOTDOT: {
        auto spreadLocation = m_token.m_location;
        auto start = m_token.m_startPosition;
        auto divot = m_token.m_endPosition;
        next();
        TreeExpression elem = parseAssignmentExpressionOrPropagateErrorClass(context);
        failIfFalse(elem, "Cannot parse subject of a spread operation");
        auto node = context.createObjectSpreadExpression(spreadLocation, elem, start, divot, m_lastTokenEndPosition);
        return context.createProperty(node, PropertyNode::Spread, SuperBinding::NotNeeded, ClassElementTag::No);
    }
    default:
        failIfFalse(m_token.m_type & KeywordTokenFlag, "Expected a property name");
        wasIdent = true; // Treat keyword token as an identifier
        goto namedProperty;
    }
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parsePropertyMethod(TreeBuilder& context, const Identifier* methodName)
{
    ASSERT(isMethodParseMode(sourceParseMode()));
    JSTokenLocation methodLocation(tokenLocation());
    unsigned methodStart = tokenStart();
    ParserFunctionInfo<TreeBuilder> methodInfo;
    methodInfo.name = methodName;
    failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::Unnamed, false, ConstructorKind::None, SuperBinding::Needed, methodStart, methodInfo, FunctionDefinitionType::Method)), "Cannot parse this method");
    return context.createMethodDefinition(methodLocation, methodInfo);
}

template <typename LexerType>
template <class TreeBuilder> TreeProperty Parser<LexerType>::parseGetterSetter(TreeBuilder& context, PropertyNode::Type type, unsigned getterOrSetterStartOffset,
    ConstructorKind constructorKind, ClassElementTag tag)
{
    const Identifier* stringPropertyName = nullptr;
    double numericPropertyName = 0;
    TreeExpression computedPropertyName = 0;

    JSTokenLocation location(tokenLocation());

    bool matchesPrivateName = match(PRIVATENAME);
    if (matchSpecIdentifier() || match(STRING) || (Options::usePrivateMethods() && matchesPrivateName) || m_token.m_type & KeywordTokenFlag) {
        stringPropertyName = m_token.m_data.ident;
        semanticFailIfTrue(tag == ClassElementTag::Static && *stringPropertyName == m_vm.propertyNames->prototype,
            "Cannot declare a static method named 'prototype'");
        semanticFailIfTrue(tag == ClassElementTag::Instance && *stringPropertyName == m_vm.propertyNames->constructor,
            "Cannot declare a getter or setter named 'constructor'");

        if (match(PRIVATENAME))
            semanticFailIfTrue(tag == ClassElementTag::No, "Cannot declare a private setter or getter outside a class");
        next();
    } else if (match(DOUBLE) || match(INTEGER)) {
        numericPropertyName = m_token.m_data.doubleValue;
        next();
    } else if (match(BIGINT)) {
        stringPropertyName = &m_parserArena.identifierArena().makeBigIntDecimalIdentifier(const_cast<VM&>(m_vm), *m_token.m_data.bigIntString, m_token.m_data.radix);
        next();
    } else if (match(OPENBRACKET)) {
        next();
        computedPropertyName = parseAssignmentExpression(context);
        failIfFalse(computedPropertyName, "Cannot parse computed property name");
        handleProductionOrFail(CLOSEBRACKET, "]", "end", "computed property name");
    } else
        failDueToUnexpectedToken();

    ParserFunctionInfo<TreeBuilder> info;
    if (type & PropertyNode::Getter) {
        failIfFalse(match(OPENPAREN), "Expected a parameter list for getter definition");
        SetForScope<SourceParseMode> innerParseMode(m_parseMode, SourceParseMode::GetterMode);
        failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::Unnamed, false, constructorKind, SuperBinding::Needed, getterOrSetterStartOffset, info, FunctionDefinitionType::Method)), "Cannot parse getter definition");
    } else if (type & PropertyNode::Setter) {
        failIfFalse(match(OPENPAREN), "Expected a parameter list for setter definition");
        SetForScope<SourceParseMode> innerParseMode(m_parseMode, SourceParseMode::SetterMode);
        failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::Unnamed, false, constructorKind, SuperBinding::Needed, getterOrSetterStartOffset, info, FunctionDefinitionType::Method)), "Cannot parse setter definition");
    } else if (type & PropertyNode::PrivateSetter) {
        failIfFalse(match(OPENPAREN), "Expected a parameter list for private setter definition");
        SetForScope<SourceParseMode> innerParseMode(m_parseMode, SourceParseMode::SetterMode);
        failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::Unnamed, false, constructorKind, SuperBinding::Needed, getterOrSetterStartOffset, info, FunctionDefinitionType::Method)), "Cannot parse private setter definition");
    } else if (type & PropertyNode::PrivateGetter) {
        failIfFalse(match(OPENPAREN), "Expected a parameter list for private getter definition");
        SetForScope<SourceParseMode> innerParseMode(m_parseMode, SourceParseMode::GetterMode);
        failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::Unnamed, false, constructorKind, SuperBinding::Needed, getterOrSetterStartOffset, info, FunctionDefinitionType::Method)), "Cannot parse private getter definition");
    }

    if (stringPropertyName)
        return context.createGetterOrSetterProperty(location, type, stringPropertyName, info, tag);

    if (computedPropertyName)
        return context.createGetterOrSetterProperty(location, static_cast<PropertyNode::Type>(type | PropertyNode::Computed), computedPropertyName, info, tag);

    return context.createGetterOrSetterProperty(const_cast<VM&>(m_vm), m_parserArena, location, type, numericPropertyName, info, tag);
}

template <typename LexerType>
void Parser<LexerType>::recordPauseLocation(const JSTextPosition& position)
{
    if (LIKELY(!m_debuggerParseData))
        return;

    if (position.line < 0)
        return;

    m_debuggerParseData->pausePositions.appendPause(position);
}

template <typename LexerType>
void Parser<LexerType>::recordFunctionEntryLocation(const JSTextPosition& position)
{
    if (LIKELY(!m_debuggerParseData))
        return;

    m_debuggerParseData->pausePositions.appendEntry(position);
}

template <typename LexerType>
void Parser<LexerType>::recordFunctionLeaveLocation(const JSTextPosition& position)
{
    if (LIKELY(!m_debuggerParseData))
        return;

    m_debuggerParseData->pausePositions.appendLeave(position);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseObjectLiteral(TreeBuilder& context)
{
    consumeOrFail(OPENBRACE, "Expected opening '{' at the start of an object literal");
    
    int oldNonLHSCount = m_parserState.nonLHSCount;

    JSTokenLocation location(tokenLocation());
    if (match(CLOSEBRACE)) {
        next();
        return context.createObjectLiteral(location);
    }
    
    TreeProperty property = parseProperty(context);
    failIfFalse(property, "Cannot parse object literal property");

    bool seenProtoSetter = context.isUnderscoreProtoSetter(property);

    TreePropertyList propertyList = context.createPropertyList(location, property);
    TreePropertyList tail = propertyList;
    while (match(COMMA)) {
        next();
        if (match(CLOSEBRACE))
            break;
        JSTokenLocation propertyLocation(tokenLocation());
        property = parseProperty(context);
        failIfFalse(property, "Cannot parse object literal property");
        if (context.isUnderscoreProtoSetter(property)) {
            // https://tc39.es/ecma262/#sec-__proto__-property-names-in-object-initializers
            semanticFailIfTrue(seenProtoSetter, "Attempted to redefine __proto__ property");
            seenProtoSetter = true;
        }
        tail = context.createPropertyList(propertyLocation, property, tail);
    }

    location = tokenLocation();
    handleProductionOrFail2(CLOSEBRACE, "}", "end", "object literal");

    m_parserState.nonLHSCount = oldNonLHSCount;

    return context.createObjectLiteral(location, propertyList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseArrayLiteral(TreeBuilder& context)
{
    consumeOrFailWithFlags(OPENBRACKET, TreeBuilder::DontBuildStrings, "Expected an opening '[' at the beginning of an array literal");
    
    int oldNonLHSCount = m_parserState.nonLHSCount;
    
    int elisions = 0;
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        elisions++;
    }
    if (match(CLOSEBRACKET)) {
        JSTokenLocation location(tokenLocation());
        next(TreeBuilder::DontBuildStrings);
        return context.createArray(location, elisions);
    }
    
    TreeExpression elem;
    if (UNLIKELY(match(DOTDOTDOT))) {
        auto spreadLocation = m_token.m_location;
        auto start = m_token.m_startPosition;
        auto divot = m_token.m_endPosition;
        next();
        auto spreadExpr = parseAssignmentExpressionOrPropagateErrorClass(context);
        failIfFalse(spreadExpr, "Cannot parse subject of a spread operation");
        elem = context.createSpreadExpression(spreadLocation, spreadExpr, start, divot, m_lastTokenEndPosition);
    } else
        elem = parseAssignmentExpressionOrPropagateErrorClass(context);
    failIfFalse(elem, "Cannot parse array literal element");
    typename TreeBuilder::ElementList elementList = context.createElementList(elisions, elem);
    typename TreeBuilder::ElementList tail = elementList;
    elisions = 0;
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        elisions = 0;
        
        while (match(COMMA)) {
            next();
            elisions++;
        }
        
        if (match(CLOSEBRACKET)) {
            JSTokenLocation location(tokenLocation());
            next(TreeBuilder::DontBuildStrings);
            return context.createArray(location, elisions, elementList);
        }
        if (UNLIKELY(match(DOTDOTDOT))) {
            auto spreadLocation = m_token.m_location;
            auto start = m_token.m_startPosition;
            auto divot = m_token.m_endPosition;
            next();
            TreeExpression elem = parseAssignmentExpressionOrPropagateErrorClass(context);
            failIfFalse(elem, "Cannot parse subject of a spread operation");
            auto spread = context.createSpreadExpression(spreadLocation, elem, start, divot, m_lastTokenEndPosition);
            tail = context.createElementList(tail, elisions, spread);
            continue;
        }
        TreeExpression elem = parseAssignmentExpressionOrPropagateErrorClass(context);
        failIfFalse(elem, "Cannot parse array literal element");
        tail = context.createElementList(tail, elisions, elem);
    }

    JSTokenLocation location(tokenLocation());
    if (!consume(CLOSEBRACKET)) {
        failIfFalse(match(DOTDOTDOT), "Expected either a closing ']' or a ',' following an array element");
        semanticFail("The '...' operator should come before a target expression");
    }
    
    m_parserState.nonLHSCount = oldNonLHSCount;
    
    return context.createArray(location, elementList);
}

template <typename LexerType>
template <class TreeBuilder> TreeClassExpression Parser<LexerType>::parseClassExpression(TreeBuilder& context)
{
    ASSERT(match(CLASSTOKEN));
    ParserClassInfo<TreeBuilder> info;
    info.className = &m_vm.propertyNames->nullIdentifier;
    return parseClass(context, FunctionNameRequirements::None, info);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseFunctionExpression(TreeBuilder& context)
{
    ASSERT(match(FUNCTION));
    JSTokenLocation location(tokenLocation());
    unsigned functionKeywordStart = tokenStart();
    next();
    ParserFunctionInfo<TreeBuilder> functionInfo;
    functionInfo.name = &m_vm.propertyNames->nullIdentifier;
    SourceParseMode parseMode = SourceParseMode::NormalFunctionMode;
    if (consume(TIMES))
        parseMode = SourceParseMode::GeneratorWrapperFunctionMode;
    SetForScope<SourceParseMode> setInnerParseMode(m_parseMode, parseMode);
    failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::None, false, ConstructorKind::None, SuperBinding::NotNeeded, functionKeywordStart, functionInfo, FunctionDefinitionType::Expression)), "Cannot parse function expression");
    return context.createFunctionExpr(location, functionInfo);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseAsyncFunctionExpression(TreeBuilder& context)
{
    ASSERT(match(FUNCTION));
    JSTokenLocation location(tokenLocation());
    unsigned functionKeywordStart = tokenStart();
    next();
    SourceParseMode parseMode = SourceParseMode::AsyncFunctionMode;

    if (consume(TIMES))
        parseMode = SourceParseMode::AsyncGeneratorWrapperFunctionMode;
    SetForScope<SourceParseMode> setInnerParseMode(m_parseMode, parseMode);

    ParserFunctionInfo<TreeBuilder> functionInfo;
    functionInfo.name = &m_vm.propertyNames->nullIdentifier;
    failIfFalse(parseFunctionInfo(context, FunctionNameRequirements::None, false, ConstructorKind::None, SuperBinding::NotNeeded, functionKeywordStart, functionInfo, FunctionDefinitionType::Expression), parseMode == SourceParseMode::AsyncFunctionMode ? "Cannot parse async function expression" : "Cannot parse async generator function expression");
    return context.createFunctionExpr(location, functionInfo);
}

template <typename LexerType>
template <class TreeBuilder> typename TreeBuilder::TemplateString Parser<LexerType>::parseTemplateString(TreeBuilder& context, bool isTemplateHead, typename LexerType::RawStringsBuildMode rawStringsBuildMode, bool& elementIsTail)
{
    if (isTemplateHead)
        ASSERT(match(BACKQUOTE));
    else
        matchOrFail(CLOSEBRACE, "Expected a closing '}' following an expression in template literal");

    // Re-scan the token to recognize it as Template Element.
    m_token.m_type = m_lexer->scanTemplateString(&m_token, rawStringsBuildMode);
    matchOrFail(TEMPLATE, "Expected an template element");
    const Identifier* cooked = m_token.m_data.cooked;
    const Identifier* raw = m_token.m_data.raw;
    elementIsTail = m_token.m_data.isTail;
    JSTokenLocation location(tokenLocation());
    next();
    return context.createTemplateString(location, cooked, raw);
}

template <typename LexerType>
template <class TreeBuilder> typename TreeBuilder::TemplateLiteral Parser<LexerType>::parseTemplateLiteral(TreeBuilder& context, typename LexerType::RawStringsBuildMode rawStringsBuildMode)
{
    ASSERT(match(BACKQUOTE));
    JSTokenLocation location(tokenLocation());
    bool elementIsTail = false;

    auto headTemplateString = parseTemplateString(context, true, rawStringsBuildMode, elementIsTail);
    failIfFalse(headTemplateString, "Cannot parse head template element");

    typename TreeBuilder::TemplateStringList templateStringList = context.createTemplateStringList(headTemplateString);
    typename TreeBuilder::TemplateStringList templateStringTail = templateStringList;

    if (elementIsTail)
        return context.createTemplateLiteral(location, templateStringList);

    failIfTrue(match(CLOSEBRACE), "Template literal expression cannot be empty");
    TreeExpression expression = parseExpression(context);
    failIfFalse(expression, "Cannot parse expression in template literal");

    typename TreeBuilder::TemplateExpressionList templateExpressionList = context.createTemplateExpressionList(expression);
    typename TreeBuilder::TemplateExpressionList templateExpressionTail = templateExpressionList;

    auto templateString = parseTemplateString(context, false, rawStringsBuildMode, elementIsTail);
    failIfFalse(templateString, "Cannot parse template element");
    templateStringTail = context.createTemplateStringList(templateStringTail, templateString);

    while (!elementIsTail) {
        failIfTrue(match(CLOSEBRACE), "Template literal expression cannot be empty");
        TreeExpression expression = parseExpression(context);
        failIfFalse(expression, "Cannot parse expression in template literal");

        templateExpressionTail = context.createTemplateExpressionList(templateExpressionTail, expression);

        auto templateString = parseTemplateString(context, false, rawStringsBuildMode, elementIsTail);
        failIfFalse(templateString, "Cannot parse template element");
        templateStringTail = context.createTemplateStringList(templateStringTail, templateString);
    }

    return context.createTemplateLiteral(location, templateStringList, templateExpressionList);
}

template <class LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::createResolveAndUseVariable(TreeBuilder& context, const Identifier* ident, bool isEval, const JSTextPosition& start, const JSTokenLocation& location)
{
    currentScope()->useVariable(ident, isEval);
    m_parserState.lastIdentifier = ident;
    return context.createResolve(location, *ident, start, lastTokenEndPosition());
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parsePrimaryExpression(TreeBuilder& context)
{
    failIfStackOverflow();
    switch (m_token.m_type) {
    case FUNCTION:
        return parseFunctionExpression(context);
    case CLASSTOKEN:
        return parseClassExpression(context);
    case OPENBRACE:
        return parseObjectLiteral(context);
    case OPENBRACKET:
        return parseArrayLiteral(context);
    case OPENPAREN: {
        next();
        int oldNonLHSCount = m_parserState.nonLHSCount;
        TreeExpression result = parseExpression(context);
        m_parserState.nonLHSCount = oldNonLHSCount;
        handleProductionOrFail(CLOSEPAREN, ")", "end", "compound expression");
        return result;
    }
    case THISTOKEN: {
        JSTokenLocation location(tokenLocation());
        next();
        if (currentScope()->isArrowFunction())
            currentScope()->setInnerArrowFunctionUsesThis();
        return context.createThisExpr(location);
    }
    case AWAIT:
        if (m_parserState.functionParsePhase == FunctionParsePhase::Parameters)
            semanticFailIfFalse(m_parserState.allowAwait, "Cannot use 'await' within a parameter default expression");
        else if (currentFunctionScope()->isAsyncFunctionBoundary() || (isModuleParseMode(sourceParseMode()) && Options::useTopLevelAwait()))
            return parseAwaitExpression(context);

        goto identifierExpression;
    case IDENT: {
        if (UNLIKELY(*m_token.m_data.ident == m_vm.propertyNames->async && !m_token.m_data.escaped)) {
            JSTextPosition start = tokenStartPosition();
            const Identifier* ident = m_token.m_data.ident;
            JSTokenLocation location(tokenLocation());
            next();
            if (match(FUNCTION) && !m_lexer->hasLineTerminatorBeforeToken())
                return parseAsyncFunctionExpression(context);

            // Avoid using variable if it is an arrow function parameter
            if (UNLIKELY(match(ARROWFUNCTION)))
                return 0;

            const bool isEval = false;
            return createResolveAndUseVariable(context, ident, isEval, start, location);
        }
        if (UNLIKELY(m_parserState.isParsingClassFieldInitializer))
            failIfTrue(*m_token.m_data.ident == m_vm.propertyNames->arguments, "Cannot reference 'arguments' in class field initializer");
    identifierExpression:
        JSTextPosition start = tokenStartPosition();
        const Identifier* ident = m_token.m_data.ident;
        if (UNLIKELY(currentScope()->evalContextType() == EvalContextType::InstanceFieldEvalContext))
            failIfTrue(*ident == m_vm.propertyNames->arguments, "arguments is not valid in this context");
        JSTokenLocation location(tokenLocation());
        next();

        // Avoid using variable if it is an arrow function parameter
        if (UNLIKELY(match(ARROWFUNCTION)))
            return 0;

        return createResolveAndUseVariable(context, ident, *ident == m_vm.propertyNames->eval, start, location);
    }
    case BIGINT: {
        const Identifier* ident = m_token.m_data.bigIntString;
        uint8_t radix = m_token.m_data.radix;
        JSTokenLocation location(tokenLocation());
        next();
        return context.createBigInt(location, ident, radix);
    }
    case STRING: {
        const Identifier* ident = m_token.m_data.ident;
        JSTokenLocation location(tokenLocation());
        next();
        return context.createString(location, ident);
    }
    case DOUBLE: {
        double d = m_token.m_data.doubleValue;
        JSTokenLocation location(tokenLocation());
        next();
        return context.createDoubleExpr(location, d);
    }
    case INTEGER: {
        double d = m_token.m_data.doubleValue;
        JSTokenLocation location(tokenLocation());
        next();
        return context.createIntegerExpr(location, d);
    }
    case NULLTOKEN: {
        JSTokenLocation location(tokenLocation());
        next();
        return context.createNull(location);
    }
    case TRUETOKEN: {
        JSTokenLocation location(tokenLocation());
        next();
        return context.createBoolean(location, true);
    }
    case FALSETOKEN: {
        JSTokenLocation location(tokenLocation());
        next();
        return context.createBoolean(location, false);
    }
    case DIVEQUAL:
    case DIVIDE: {
        /* regexp */
        if (match(DIVEQUAL))
            m_token.m_type = m_lexer->scanRegExp(&m_token, '=');
        else
            m_token.m_type = m_lexer->scanRegExp(&m_token);
        matchOrFail(REGEXP, "Invalid regular expression");

        const Identifier* pattern = m_token.m_data.pattern;
        const Identifier* flags = m_token.m_data.flags;
        JSTextPosition start = tokenStartPosition();
        JSTokenLocation location(tokenLocation());
        next();
        TreeExpression re = context.createRegExp(location, *pattern, *flags, start);
        if (!re) {
            Yarr::ErrorCode errorCode = Yarr::checkSyntax(pattern->string(), flags->string());
            regexFail(Yarr::errorMessage(errorCode));
        }
        return re;
    }
    case BACKQUOTE:
        return parseTemplateLiteral(context, LexerType::RawStringsBuildMode::DontBuildRawStrings);
    case YIELD:
        if (canUseIdentifierYield())
            goto identifierExpression;
        failDueToUnexpectedToken();
    case LET:
        if (!strictMode())
            goto identifierExpression;
        failDueToUnexpectedToken();
    case ESCAPED_KEYWORD:
        if (matchAllowedEscapedContextualKeyword())
            goto identifierExpression;
        FALLTHROUGH;
    default:
        failDueToUnexpectedToken();
    }
}

template <typename LexerType>
template <class TreeBuilder> TreeArguments Parser<LexerType>::parseArguments(TreeBuilder& context)
{
    consumeOrFailWithFlags(OPENPAREN, TreeBuilder::DontBuildStrings, "Expected opening '(' at start of argument list");
    JSTokenLocation location(tokenLocation());
    if (match(CLOSEPAREN)) {
        next(TreeBuilder::DontBuildStrings);
        return context.createArguments();
    }
    auto argumentsStart = m_token.m_startPosition;
    auto argumentsDivot = m_token.m_endPosition;

    int initialAssignments = m_parserState.assignmentCount;
    ArgumentType argType = ArgumentType::Normal;
    TreeExpression firstArg = parseArgument(context, argType);
    failIfFalse(firstArg, "Cannot parse function argument");
    semanticFailIfTrue(match(DOTDOTDOT), "The '...' operator should come before the target expression");

    bool hasSpread = false;
    if (argType == ArgumentType::Spread)
        hasSpread = true;
    TreeArgumentsList argList = context.createArgumentsList(location, firstArg);
    TreeArgumentsList tail = argList;

    while (match(COMMA)) {
        JSTokenLocation argumentLocation(tokenLocation());
        next(TreeBuilder::DontBuildStrings);

        if (UNLIKELY(match(CLOSEPAREN)))
            break;
        
        TreeExpression arg = parseArgument(context, argType);
        propagateError();
        semanticFailIfTrue(match(DOTDOTDOT), "The '...' operator should come before the target expression");

        if (argType == ArgumentType::Spread)
            hasSpread = true;

        tail = context.createArgumentsList(argumentLocation, tail, arg);
    }

    handleProductionOrFail2(CLOSEPAREN, ")", "end", "argument list");
    if (hasSpread) {
        TreeExpression spreadArray = context.createSpreadExpression(location, context.createArray(location, context.createElementList(argList)), argumentsStart, argumentsDivot, m_lastTokenEndPosition);
        return context.createArguments(context.createArgumentsList(location, spreadArray), initialAssignments != m_parserState.assignmentCount);
    }

    return context.createArguments(argList, initialAssignments != m_parserState.assignmentCount);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseArgument(TreeBuilder& context, ArgumentType& type)
{
    if (UNLIKELY(match(DOTDOTDOT))) {
        JSTokenLocation spreadLocation(tokenLocation());
        auto start = m_token.m_startPosition;
        auto divot = m_token.m_endPosition;
        next();
        TreeExpression spreadExpr = parseAssignmentExpression(context);
        propagateError();
        auto end = m_lastTokenEndPosition;
        type = ArgumentType::Spread;
        return context.createSpreadExpression(spreadLocation, spreadExpr, start, divot, end);
    }

    type = ArgumentType::Normal;
    return parseAssignmentExpression(context);
}

template <typename TreeBuilder, typename ParserType, typename = typename std::enable_if<std::is_same<TreeBuilder, ASTBuilder>::value>::type>
static inline void recordCallOrApplyDepth(ParserType* parser, VM& vm, Optional<typename ParserType::CallOrApplyDepthScope>& callOrApplyDepthScope, ExpressionNode* expression)
{
    if (expression->isDotAccessorNode()) {
        DotAccessorNode* dot = static_cast<DotAccessorNode*>(expression);
        bool isCallOrApply = dot->identifier() == vm.propertyNames->builtinNames().callPublicName() || dot->identifier() == vm.propertyNames->builtinNames().applyPublicName();
        if (isCallOrApply)
            callOrApplyDepthScope.emplace(parser);
    }
}

template <typename TreeBuilder, typename ParserType, typename = typename std::enable_if<std::is_same<TreeBuilder, SyntaxChecker>::value>::type>
static inline void recordCallOrApplyDepth(ParserType*, VM&, Optional<typename ParserType::CallOrApplyDepthScope>&, SyntaxChecker::Expression)
{
}

template <typename LexerType>
bool Parser<LexerType>::usePrivateName(const Identifier* ident)
{
    if (m_lexer->isReparsingFunction())
        return true;

    if (auto maybeCurrent = findPrivateNameScope()) {
        ScopeRef current = maybeCurrent.value();
        if (!current->hasPrivateName(*ident))
            current->usePrivateName(*ident);
        return true;
    }

    return false;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseMemberExpression(TreeBuilder& context)
{
    TreeExpression base = 0;
    JSTextPosition expressionStart = tokenStartPosition();
    int newCount = 0;
    JSTokenLocation startLocation = tokenLocation();
    JSTokenLocation lastNewTokenLocation;
    while (match(NEW)) {
        lastNewTokenLocation = tokenLocation();
        next();
        newCount++;
    }
    JSTokenLocation location = tokenLocation();

    bool baseIsSuper = match(SUPER);
    bool previousBaseWasSuper = false;
    bool baseIsImport = match(IMPORT);

    bool baseIsNewTarget = false;
    if (newCount && match(DOT)) {
        next();
        if (matchContextualKeyword(m_vm.propertyNames->target)) {
            ScopeRef closestOrdinaryFunctionScope = closestParentOrdinaryFunctionNonLexicalScope();
            bool isClassFieldInitializer = m_parserState.isParsingClassFieldInitializer;
            bool isFunctionEvalContextType = m_isInsideOrdinaryFunction && (closestOrdinaryFunctionScope->evalContextType() == EvalContextType::FunctionEvalContext || closestOrdinaryFunctionScope->evalContextType() == EvalContextType::InstanceFieldEvalContext);
            semanticFailIfFalse(currentScope()->isFunction() || isFunctionEvalContextType || isClassFieldInitializer, "new.target is only valid inside functions");
            baseIsNewTarget = true;
            if (currentScope()->isArrowFunction()) {
                semanticFailIfFalse(!closestOrdinaryFunctionScope->isGlobalCodeScope() || isFunctionEvalContextType || isClassFieldInitializer, "new.target is not valid inside arrow functions in global code");
                currentScope()->setInnerArrowFunctionUsesNewTarget();
            }
            ASSERT(lastNewTokenLocation.line);
            base = context.createNewTargetExpr(lastNewTokenLocation);
            newCount--;
            next();
        } else {
            failIfTrue(match(IDENT), "\"new.\" can only followed with target");
            failDueToUnexpectedToken();
        }
    }

    bool baseIsAsyncKeyword = false;

    if (baseIsSuper) {
        ScopeRef closestOrdinaryFunctionScope = closestParentOrdinaryFunctionNonLexicalScope();
        ScopeRef classScope = closestClassScopeOrTopLevelScope();
        bool isClassFieldInitializer = classScope.index() > closestOrdinaryFunctionScope.index();
        semanticFailIfFalse(currentScope()->isFunction() || isClassFieldInitializer || (closestOrdinaryFunctionScope->isEvalContext() && closestOrdinaryFunctionScope->expectedSuperBinding() == SuperBinding::Needed), "super is not valid in this context");
        base = context.createSuperExpr(location);
        next();
        failIfTrue(match(OPENPAREN) && currentScope()->evalContextType() == EvalContextType::InstanceFieldEvalContext, "super call is not valid in this context");
        ScopeRef functionScope = currentFunctionScope();
        if (!functionScope->setNeedsSuperBinding()) {
            // It unnecessary to check of using super during reparsing one more time. Also it can lead to syntax error
            // in case of arrow function because during reparsing we don't know whether we currently parse the arrow function
            // inside of the constructor or method.
            if (!m_lexer->isReparsingFunction()) {
                SuperBinding functionSuperBinding = !functionScope->isArrowFunction() && !closestOrdinaryFunctionScope->isEvalContext()
                    ? functionScope->expectedSuperBinding()
                    : closestOrdinaryFunctionScope->expectedSuperBinding();
                semanticFailIfTrue(functionSuperBinding == SuperBinding::NotNeeded && !isClassFieldInitializer, "super is not valid in this context");
            }
        }
    } else if (baseIsImport) {
        next();
        JSTextPosition expressionEnd = lastTokenEndPosition();
        if (consume(DOT)) {
            if (matchContextualKeyword(m_vm.propertyNames->builtinNames().metaPublicName())) {
                semanticFailIfFalse(m_scriptMode == JSParserScriptMode::Module, "import.meta is only valid inside modules");
                base = context.createImportMetaExpr(location, createResolveAndUseVariable(context, &m_vm.propertyNames->metaPrivateName, false, expressionStart, location));
                next();
            } else {
                failIfTrue(match(IDENT), "\"import.\" can only followed with meta");
                failDueToUnexpectedToken();
            }
        } else {
            semanticFailIfTrue(newCount, "Cannot use new with import");
            consumeOrFail(OPENPAREN, "import call expects exactly one argument");
            TreeExpression expr = parseAssignmentExpression(context);
            failIfFalse(expr, "Cannot parse expression");
            consumeOrFail(CLOSEPAREN, "import call expects exactly one argument");
            base = context.createImportExpr(location, expr, expressionStart, expressionEnd, lastTokenEndPosition());
        }
    } else if (!baseIsNewTarget) {
        const bool isAsync = matchContextualKeyword(m_vm.propertyNames->async);

        base = parsePrimaryExpression(context);
        failIfFalse(base, "Cannot parse base expression");
        if (UNLIKELY(isAsync && context.isResolve(base) && !m_lexer->hasLineTerminatorBeforeToken())) {
            if (matchSpecIdentifier()) {
                // AsyncArrowFunction
                forceClassifyExpressionError(ErrorIndicatesAsyncArrowFunction);
                failDueToUnexpectedToken();
            }
            baseIsAsyncKeyword = true;
        }
    }

    failIfFalse(base, "Cannot parse base expression");

    do {
        TreeExpression optionalChainBase = 0;
        JSTokenLocation optionalChainLocation;
        JSTokenType type = m_token.m_type;

        if (match(QUESTIONDOT)) {
            semanticFailIfTrue(newCount, "Cannot call constructor in an optional chain");
            semanticFailIfTrue(baseIsSuper, "Cannot use super as the base of an optional chain");
            optionalChainBase = base;
            optionalChainLocation = tokenLocation();

            SavePoint savePoint = createSavePoint(context);
            next();
            if (match(OPENBRACKET) || match(OPENPAREN) || match(BACKQUOTE))
                type = m_token.m_type;
            else {
                type = DOT;
                restoreSavePoint(context, savePoint);
            }
        }

        while (true) {
            location = tokenLocation();
            switch (type) {
            case OPENBRACKET: {
                m_parserState.nonTrivialExpressionCount++;
                JSTextPosition expressionEnd = lastTokenEndPosition();
                next();
                int nonLHSCount = m_parserState.nonLHSCount;
                int initialAssignments = m_parserState.assignmentCount;
                TreeExpression property = parseExpression(context);
                failIfFalse(property, "Cannot parse subscript expression");
                base = context.createBracketAccess(startLocation, base, property, initialAssignments != m_parserState.assignmentCount, expressionStart, expressionEnd, tokenEndPosition());

                if (UNLIKELY(baseIsSuper && currentScope()->isArrowFunction()))
                    currentFunctionScope()->setInnerArrowFunctionUsesSuperProperty();

                handleProductionOrFail(CLOSEBRACKET, "]", "end", "subscript expression");
                m_parserState.nonLHSCount = nonLHSCount;
                break;
            }
            case OPENPAREN: {
                if (baseIsSuper)
                    failIfTrue(m_parserState.isParsingClassFieldInitializer, "super call is not valid in class field initializer context");
                m_parserState.nonTrivialExpressionCount++;
                int nonLHSCount = m_parserState.nonLHSCount;
                if (newCount) {
                    newCount--;
                    semanticFailIfTrue(baseIsSuper, "Cannot use new with super call");
                    JSTextPosition expressionEnd = lastTokenEndPosition();
                    TreeArguments arguments = parseArguments(context);
                    failIfFalse(arguments, "Cannot parse call arguments");
                    base = context.createNewExpr(location, base, arguments, expressionStart, expressionEnd, lastTokenEndPosition());
                } else {
                    size_t usedVariablesSize = currentScope()->currentUsedVariablesSize();
                    JSTextPosition expressionEnd = lastTokenEndPosition();
                    Optional<CallOrApplyDepthScope> callOrApplyDepthScope;
                    recordCallOrApplyDepth<TreeBuilder>(this, m_vm, callOrApplyDepthScope, base);

                    TreeArguments arguments = parseArguments(context);

                    if (baseIsAsyncKeyword && (!arguments || match(ARROWFUNCTION))) {
                        currentScope()->revertToPreviousUsedVariables(usedVariablesSize);
                        forceClassifyExpressionError(ErrorIndicatesAsyncArrowFunction);
                        failDueToUnexpectedToken();
                    }

                    failIfFalse(arguments, "Cannot parse call arguments");
                    if (baseIsSuper) {
                        ScopeRef functionScope = currentFunctionScope();
                        if (!functionScope->setHasDirectSuper()) {
                            // It unnecessary to check of using super during reparsing one more time. Also it can lead to syntax error
                            // in case of arrow function because during reparsing we don't know whether we currently parse the arrow function
                            // inside of the constructor or method.
                            if (!m_lexer->isReparsingFunction()) {
                                ScopeRef closestOrdinaryFunctionScope = closestParentOrdinaryFunctionNonLexicalScope();
                                ConstructorKind functionConstructorKind = !functionScope->isArrowFunction() && !closestOrdinaryFunctionScope->isEvalContext()
                                    ? functionScope->constructorKind()
                                    : closestOrdinaryFunctionScope->constructorKind();
                                semanticFailIfTrue(functionConstructorKind == ConstructorKind::None, "super is not valid in this context");
                                semanticFailIfTrue(functionConstructorKind != ConstructorKind::Extends, "super is not valid in this context");
                            }
                        }
                        if (currentScope()->isArrowFunction())
                            functionScope->setInnerArrowFunctionUsesSuperCall();
                    }

                    bool isOptionalCall = optionalChainLocation.endOffset == static_cast<unsigned>(expressionEnd.offset);
                    base = context.makeFunctionCallNode(startLocation, base, previousBaseWasSuper, arguments, expressionStart,
                        expressionEnd, lastTokenEndPosition(), callOrApplyDepthScope ? callOrApplyDepthScope->distanceToInnermostChild() : 0, isOptionalCall);

                    if (isOptionalCall)
                        optionalChainBase = base;
                }
                m_parserState.nonLHSCount = nonLHSCount;
                break;
            }
            case DOT: {
                m_parserState.nonTrivialExpressionCount++;
                JSTextPosition expressionEnd = lastTokenEndPosition();
                nextExpectIdentifier(TreeBuilder::DontBuildKeywords | LexerFlags::IgnoreReservedWords);
                const Identifier* ident = m_token.m_data.ident;
                auto type = DotType::Name;
                if (match(PRIVATENAME)) {
                    ASSERT(ident);
                    failIfTrue(baseIsSuper, "Cannot access private names from super");
                    if (UNLIKELY(currentScope()->evalContextType() == EvalContextType::InstanceFieldEvalContext))
                        semanticFailIfFalse(currentScope()->hasPrivateName(*ident), "Cannot reference undeclared private field '", ident->impl(), "'");
                    semanticFailIfFalse(usePrivateName(ident), "Cannot reference private names outside of class");
                    m_parserState.lastPrivateName = ident;
                    currentScope()->useVariable(ident, false);
                    type = DotType::PrivateMember;
                    m_token.m_type = IDENT;
                }
                matchOrFail(IDENT, "Expected a property name after ", optionalChainBase ? "'?.'" : "'.'");
                base = context.createDotAccess(startLocation, base, ident, type, expressionStart, expressionEnd, tokenEndPosition());
                if (UNLIKELY(baseIsSuper && currentScope()->isArrowFunction()))
                    currentFunctionScope()->setInnerArrowFunctionUsesSuperProperty();
                next();
                break;
            }
            case BACKQUOTE: {
                semanticFailIfTrue(optionalChainBase, "Cannot use tagged templates in an optional chain");
                semanticFailIfTrue(baseIsSuper, "Cannot use super as tag for tagged templates");
                JSTextPosition expressionEnd = lastTokenEndPosition();
                int nonLHSCount = m_parserState.nonLHSCount;
                typename TreeBuilder::TemplateLiteral templateLiteral = parseTemplateLiteral(context, LexerType::RawStringsBuildMode::BuildRawStrings);
                failIfFalse(templateLiteral, "Cannot parse template literal");
                base = context.createTaggedTemplate(startLocation, base, templateLiteral, expressionStart, expressionEnd, lastTokenEndPosition());
                m_parserState.nonLHSCount = nonLHSCount;
                m_seenTaggedTemplate = true;
                break;
            }
            default:
                goto endOfChain;
            }
            previousBaseWasSuper = baseIsSuper;
            baseIsSuper = false;
            type = m_token.m_type;
        }
endOfChain:
        if (optionalChainBase)
            base = context.createOptionalChain(optionalChainLocation, optionalChainBase, base, !match(QUESTIONDOT));
    } while (match(QUESTIONDOT));

    semanticFailIfTrue(baseIsSuper, newCount ? "Cannot use new with super call" : "super is not valid in this context");
    while (newCount--)
        base = context.createNewExpr(location, base, expressionStart, lastTokenEndPosition());
    return base;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseArrowFunctionExpression(TreeBuilder& context, bool isAsync)
{
    JSTokenLocation location;

    unsigned functionKeywordStart = tokenStart();
    location = tokenLocation();
    ParserFunctionInfo<TreeBuilder> info;
    info.name = &m_vm.propertyNames->nullIdentifier;

    SetForScope<SourceParseMode> innerParseMode(m_parseMode, isAsync ? SourceParseMode::AsyncArrowFunctionMode : SourceParseMode::ArrowFunctionMode);
    failIfFalse((parseFunctionInfo(context, FunctionNameRequirements::Unnamed, true, ConstructorKind::None, SuperBinding::NotNeeded, functionKeywordStart, info, FunctionDefinitionType::Expression)), "Cannot parse arrow function expression");

    return context.createArrowFunctionExpr(location, info);
}

static const char* operatorString(bool prefix, unsigned tok)
{
    switch (tok) {
    case MINUSMINUS:
    case AUTOMINUSMINUS:
        return prefix ? "prefix-decrement" : "decrement";

    case PLUSPLUS:
    case AUTOPLUSPLUS:
        return prefix ? "prefix-increment" : "increment";

    case EXCLAMATION:
        return "logical-not";

    case TILDE:
        return "bitwise-not";
    
    case TYPEOF:
        return "typeof";
    
    case VOIDTOKEN:
        return "void";
    
    case DELETETOKEN:
        return "delete";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return "error";
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseUnaryExpression(TreeBuilder& context)
{
    typename TreeBuilder::UnaryExprContext unaryExprContext(context);
    AllowInOverride allowInOverride(this);
    int tokenStackDepth = 0;
    bool hasPrefixUpdateOp = false;
    unsigned lastOperator = 0;

    if (UNLIKELY(match(AWAIT) && (currentFunctionScope()->isAsyncFunctionBoundary() || (isModuleParseMode(sourceParseMode()) && Options::useTopLevelAwait()))))
        return parseAwaitExpression(context);

    JSTokenLocation location(tokenLocation());

    int oldTokenStackDepth = context.unaryTokenStackDepth();
    auto scopeExit = makeScopeExit([&] {
        ASSERT_UNUSED(oldTokenStackDepth, oldTokenStackDepth <= context.unaryTokenStackDepth());
    });

    while (isUnaryOp(m_token.m_type)) {
        semanticFailIfTrue(hasPrefixUpdateOp, "The ", operatorString(true, lastOperator), " operator requires a reference expression");
        if (isUpdateOp(m_token.m_type))
            hasPrefixUpdateOp = true;
        lastOperator = m_token.m_type;
        m_parserState.nonLHSCount++;
        context.appendUnaryToken(tokenStackDepth, m_token.m_type, tokenStartPosition());
        next();
        m_parserState.nonTrivialExpressionCount++;
    }
    JSTextPosition subExprStart = tokenStartPosition();
    ASSERT(subExprStart.offset >= subExprStart.lineStartOffset);
    TreeExpression expr = parseMemberExpression(context);
    if (!expr) {
        if (lastOperator)
            failWithMessage("Cannot parse subexpression of ", operatorString(true, lastOperator), "operator");
        failWithMessage("Cannot parse member expression");
    }
    if constexpr (std::is_same_v<TreeBuilder, ASTBuilder>)
        ASSERT(oldTokenStackDepth + tokenStackDepth == context.unaryTokenStackDepth());
    if (isUpdateOp(static_cast<JSTokenType>(lastOperator))) {
        semanticFailIfTrue(context.isMetaProperty(expr), metaPropertyName(context, expr), " can't come after a prefix operator");
        semanticFailIfFalse(isSimpleAssignmentTarget(context, expr), "Prefix ", lastOperator == PLUSPLUS ? "++" : "--", " operator applied to value that is not a reference");
    }
    bool isEvalOrArguments = false;
    if (strictMode()) {
        if (context.isResolve(expr))
            isEvalOrArguments = *m_parserState.lastIdentifier == m_vm.propertyNames->eval || *m_parserState.lastIdentifier == m_vm.propertyNames->arguments;
    }
    failIfTrueIfStrict(isEvalOrArguments && hasPrefixUpdateOp, "Cannot modify '", m_parserState.lastIdentifier->impl(), "' in strict mode");
    switch (m_token.m_type) {
    case PLUSPLUS:
        semanticFailIfTrue(context.isMetaProperty(expr), metaPropertyName(context, expr), " can't come before a postfix operator");
        semanticFailIfFalse(isSimpleAssignmentTarget(context, expr), "Postfix ++ operator applied to value that is not a reference");
        m_parserState.nonTrivialExpressionCount++;
        m_parserState.nonLHSCount++;
        expr = context.makePostfixNode(location, expr, Operator::PlusPlus, subExprStart, lastTokenEndPosition(), tokenEndPosition());
        m_parserState.assignmentCount++;
        failIfTrueIfStrict(isEvalOrArguments, "Cannot modify '", m_parserState.lastIdentifier->impl(), "' in strict mode");
        semanticFailIfTrue(hasPrefixUpdateOp, "The ", operatorString(false, lastOperator), " operator requires a reference expression");
        next();
        break;
    case MINUSMINUS:
        semanticFailIfTrue(context.isMetaProperty(expr), metaPropertyName(context, expr), " can't come before a postfix operator");
        semanticFailIfFalse(isSimpleAssignmentTarget(context, expr), "Postfix -- operator applied to value that is not a reference");
        m_parserState.nonTrivialExpressionCount++;
        m_parserState.nonLHSCount++;
        expr = context.makePostfixNode(location, expr, Operator::MinusMinus, subExprStart, lastTokenEndPosition(), tokenEndPosition());
        m_parserState.assignmentCount++;
        failIfTrueIfStrict(isEvalOrArguments, "'", m_parserState.lastIdentifier->impl(), "' cannot be modified in strict mode");
        semanticFailIfTrue(hasPrefixUpdateOp, "The ", operatorString(false, lastOperator), " operator requires a reference expression");
        next();
        break;
    default:
        break;
    }
    
    JSTextPosition end = lastTokenEndPosition();
    while (tokenStackDepth) {
        subExprStart = context.unaryTokenStackLastStart(tokenStackDepth);
        auto tokenType = context.unaryTokenStackLastType(tokenStackDepth);
        switch (tokenType) {
        case EXCLAMATION:
            expr = context.createLogicalNot(location, expr);
            break;
        case TILDE:
            expr = context.makeBitwiseNotNode(location, expr);
            break;
        case MINUS:
            expr = context.makeNegateNode(location, expr);
            break;
        case PLUS:
            expr = context.createUnaryPlus(location, expr);
            break;
        case PLUSPLUS:
        case AUTOPLUSPLUS:
            ASSERT(isSimpleAssignmentTarget(context, expr));
            expr = context.makePrefixNode(location, expr, Operator::PlusPlus, subExprStart, subExprStart + 2, end);
            m_parserState.assignmentCount++;
            break;
        case MINUSMINUS:
        case AUTOMINUSMINUS:
            ASSERT(isSimpleAssignmentTarget(context, expr));
            expr = context.makePrefixNode(location, expr, Operator::MinusMinus, subExprStart, subExprStart + 2, end);
            m_parserState.assignmentCount++;
            break;
        case TYPEOF:
            expr = context.makeTypeOfNode(location, expr);
            break;
        case VOIDTOKEN:
            expr = context.createVoid(location, expr);
            break;
        case DELETETOKEN:
            failIfTrueIfStrict(context.isResolve(expr), "Cannot delete unqualified property '", m_parserState.lastIdentifier->impl(), "' in strict mode");
            semanticFailIfTrue(context.isPrivateLocation(expr), "Cannot delete private field ", m_parserState.lastPrivateName->impl());
            expr = context.makeDeleteNode(location, expr, context.unaryTokenStackLastStart(tokenStackDepth), end, end);
            break;
        default:
            // If we get here something has gone horribly horribly wrong
            CRASH();
        }
        context.unaryTokenStackRemoveLast(tokenStackDepth);
    }
    return expr;
}

template <typename LexerType> void Parser<LexerType>::printUnexpectedTokenText(WTF::PrintStream& out)
{
    switch (m_token.m_type) {
    case EOFTOK:
        out.print("Unexpected end of script");
        return;
    case UNTERMINATED_IDENTIFIER_ESCAPE_ERRORTOK:
    case UNTERMINATED_IDENTIFIER_UNICODE_ESCAPE_ERRORTOK:
        out.print("Incomplete unicode escape in identifier: '", getToken(), "'");
        return;
    case UNTERMINATED_MULTILINE_COMMENT_ERRORTOK:
        out.print("Unterminated multiline comment");
        return;
    case UNTERMINATED_NUMERIC_LITERAL_ERRORTOK:
        out.print("Unterminated numeric literal '", getToken(), "'");
        return;
    case UNTERMINATED_STRING_LITERAL_ERRORTOK:
        out.print("Unterminated string literal '", getToken(), "'");
        return;
    case INVALID_IDENTIFIER_ESCAPE_ERRORTOK:
        out.print("Invalid escape in identifier: '", getToken(), "'");
        return;
    case ESCAPED_KEYWORD:
        out.print("Unexpected escaped characters in keyword token: '", getToken(), "'");
        return;
    case INVALID_IDENTIFIER_UNICODE_ESCAPE_ERRORTOK:
        out.print("Invalid unicode escape in identifier: '", getToken(), "'");
        return;
    case INVALID_NUMERIC_LITERAL_ERRORTOK:
        out.print("Invalid numeric literal: '", getToken(), "'");
        return;
    case UNTERMINATED_OCTAL_NUMBER_ERRORTOK:
        out.print("Invalid use of octal: '", getToken(), "'");
        return;
    case INVALID_STRING_LITERAL_ERRORTOK:
        out.print("Invalid string literal: '", getToken(), "'");
        return;
    case INVALID_UNICODE_ENCODING_ERRORTOK:
        out.print("Invalid unicode encoding: '", getToken(), "'");
        return;
    case INVALID_IDENTIFIER_UNICODE_ERRORTOK:
        out.print("Invalid unicode code point in identifier: '", getToken(), "'");
        return;
    case ERRORTOK:
        out.print("Unrecognized token '", getToken(), "'");
        return;
    case STRING:
        out.print("Unexpected string literal ", getToken());
        return;
    case INTEGER:
    case DOUBLE:
        out.print("Unexpected number '", getToken(), "'");
        return;
    
    case RESERVED_IF_STRICT:
        out.print("Unexpected use of reserved word '", getToken(), "' in strict mode");
        return;
        
    case RESERVED:
        out.print("Unexpected use of reserved word '", getToken(), "'");
        return;

    case INVALID_PRIVATE_NAME_ERRORTOK:
        out.print("Invalid private name '", getToken(), "'");
        return;

    case PRIVATENAME:
        out.print("Unexpected private name ", getToken());
        return;

    case AWAIT:
    case IDENT:
        out.print("Unexpected identifier '", getToken(), "'");
        return;

    default:
        break;
    }

    if (m_token.m_type & KeywordTokenFlag) {
        out.print("Unexpected keyword '", getToken(), "'");
        return;
    }
    
    out.print("Unexpected token '", getToken(), "'");
}

// Instantiate the two flavors of Parser we need instead of putting most of this file in Parser.h
template class Parser<Lexer<LChar>>;
template class Parser<Lexer<UChar>>;
    
} // namespace JSC
