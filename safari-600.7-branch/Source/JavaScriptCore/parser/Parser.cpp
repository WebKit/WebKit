/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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
#include "CodeBlock.h"
#include "Debugger.h"
#include "JSCJSValueInlines.h"
#include "Lexer.h"
#include "NodeInfo.h"
#include "JSCInlines.h"
#include "SourceProvider.h"
#include "VM.h"
#include <utility>
#include <wtf/HashFunctions.h>
#include <wtf/OwnPtr.h>
#include <wtf/StringPrintStream.h>
#include <wtf/WTFThreadData.h>


#define updateErrorMessage(shouldPrintToken, ...) do {\
    propagateError(); \
    logError(shouldPrintToken, __VA_ARGS__); \
} while (0)

#define propagateError() do { if (hasError()) return 0; } while (0)
#define internalFailWithMessage(shouldPrintToken, ...) do { updateErrorMessage(shouldPrintToken, __VA_ARGS__); return 0; } while (0)
#define handleErrorToken() do { if (m_token.m_type == EOFTOK || m_token.m_type & ErrorTokenFlag) { failDueToUnexpectedToken(); } } while (0)
#define failWithMessage(...) do { { handleErrorToken(); updateErrorMessage(true, __VA_ARGS__); } return 0; } while (0)
#define failWithStackOverflow() do { updateErrorMessage(false, "Stack exhausted"); m_hasStackOverflow = true; return 0; } while (0)
#define failIfFalse(cond, ...) do { if (!(cond)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define failIfTrue(cond, ...) do { if (cond) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define failIfTrueIfStrict(cond, ...) do { if ((cond) && strictMode()) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define failIfFalseIfStrict(cond, ...) do { if ((!(cond)) && strictMode()) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define consumeOrFail(tokenType, ...) do { if (!consume(tokenType)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define consumeOrFailWithFlags(tokenType, flags, ...) do { if (!consume(tokenType, flags)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define matchOrFail(tokenType, ...) do { if (!match(tokenType)) { handleErrorToken(); internalFailWithMessage(true, __VA_ARGS__); } } while (0)
#define failIfStackOverflow() do { if (!canRecurse()) failWithStackOverflow(); } while (0)
#define semanticFail(...) do { internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define semanticFailIfTrue(cond, ...) do { if (cond) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define semanticFailIfFalse(cond, ...) do { if (!(cond)) internalFailWithMessage(false, __VA_ARGS__); } while (0)
#define regexFail(failure) do { setErrorMessage(failure); return 0; } while (0)
#define failDueToUnexpectedToken() do {\
        logError(true);\
    return 0;\
} while (0)

#define handleProductionOrFail(token, tokenString, operation, production) do {\
    consumeOrFail(token, "Expected '", tokenString, "' to ", operation, " a ", production);\
} while (0)

#define semanticFailureDueToKeyword(...) do { \
    if (strictMode() && m_token.m_type == RESERVED_IF_STRICT) \
        semanticFail("Cannot use the reserved word '", getToken(), "' as a ", __VA_ARGS__, " in strict mode"); \
    if (m_token.m_type == RESERVED || m_token.m_type == RESERVED_IF_STRICT) \
        semanticFail("Cannot use the reserved word '", getToken(), "' as a ", __VA_ARGS__); \
    if (m_token.m_type & KeywordTokenFlag) \
        semanticFail("Cannot use the keyword '", getToken(), "' as a ", __VA_ARGS__); \
} while (0)

using namespace std;

namespace JSC {

template <typename LexerType>
void Parser<LexerType>::logError(bool)
{
    if (hasError())
        return;
    StringPrintStream stream;
    printUnexpectedTokenText(stream);
    setErrorMessage(stream.toString());
}

template <typename LexerType> template <typename A>
void Parser<LexerType>::logError(bool shouldPrintToken, const A& value1)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(value1, ".");
    setErrorMessage(stream.toString());
}

template <typename LexerType> template <typename A, typename B>
void Parser<LexerType>::logError(bool shouldPrintToken, const A& value1, const B& value2)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(value1, value2, ".");
    setErrorMessage(stream.toString());
}

template <typename LexerType> template <typename A, typename B, typename C>
void Parser<LexerType>::logError(bool shouldPrintToken, const A& value1, const B& value2, const C& value3)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(value1, value2, value3, ".");
    setErrorMessage(stream.toString());
}

template <typename LexerType> template <typename A, typename B, typename C, typename D>
void Parser<LexerType>::logError(bool shouldPrintToken, const A& value1, const B& value2, const C& value3, const D& value4)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(value1, value2, value3, value4, ".");
    setErrorMessage(stream.toString());
}

template <typename LexerType> template <typename A, typename B, typename C, typename D, typename E>
void Parser<LexerType>::logError(bool shouldPrintToken, const A& value1, const B& value2, const C& value3, const D& value4, const E& value5)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(value1, value2, value3, value4, value5, ".");
    setErrorMessage(stream.toString());
}

template <typename LexerType> template <typename A, typename B, typename C, typename D, typename E, typename F>
void Parser<LexerType>::logError(bool shouldPrintToken, const A& value1, const B& value2, const C& value3, const D& value4, const E& value5, const F& value6)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(value1, value2, value3, value4, value5, value6, ".");
    setErrorMessage(stream.toString());
}

template <typename LexerType> template <typename A, typename B, typename C, typename D, typename E, typename F, typename G>
void Parser<LexerType>::logError(bool shouldPrintToken, const A& value1, const B& value2, const C& value3, const D& value4, const E& value5, const F& value6, const G& value7)
{
    if (hasError())
        return;
    StringPrintStream stream;
    if (shouldPrintToken) {
        printUnexpectedTokenText(stream);
        stream.print(". ");
    }
    stream.print(value1, value2, value3, value4, value5, value6, value7, ".");
    setErrorMessage(stream.toString());
}

template <typename LexerType>
Parser<LexerType>::Parser(VM* vm, const SourceCode& source, FunctionParameters* parameters, const Identifier& name, JSParserStrictness strictness, JSParserMode parserMode)
    : m_vm(vm)
    , m_source(&source)
    , m_hasStackOverflow(false)
    , m_allowsIn(true)
    , m_assignmentCount(0)
    , m_nonLHSCount(0)
    , m_syntaxAlreadyValidated(source.provider()->isValid())
    , m_statementDepth(0)
    , m_nonTrivialExpressionCount(0)
    , m_lastIdentifier(0)
    , m_lastFunctionName(nullptr)
    , m_sourceElements(0)
    , m_parsingBuiltin(strictness == JSParseBuiltin)
{
    m_lexer = adoptPtr(new LexerType(vm, strictness));
    m_arena = m_vm->parserArena.get();
    m_lexer->setCode(source, m_arena);
    m_token.m_location.line = source.firstLine();
    m_token.m_location.startOffset = source.startOffset();
    m_token.m_location.endOffset = source.startOffset();
    m_token.m_location.lineStartOffset = source.startOffset();
    m_functionCache = vm->addSourceProviderCache(source.provider());
    ScopeRef scope = pushScope();
    if (parserMode == JSParseFunctionCode)
        scope->setIsFunction();
    if (strictness == JSParseStrict)
        scope->setStrictMode();
    if (parameters) {
        bool hadBindingParameters = false;
        for (unsigned i = 0; i < parameters->size(); i++) {
            auto parameter = parameters->at(i);
            if (!parameter->isBindingNode()) {
                hadBindingParameters = true;
                continue;
            }
            scope->declareParameter(&static_cast<BindingNode*>(parameter)->boundProperty());
        }
        if (hadBindingParameters) {
            Vector<Identifier> boundParameterNames;
            for (unsigned i = 0; i < parameters->size(); i++) {
                auto parameter = parameters->at(i);
                if (parameter->isBindingNode())
                    continue;
                parameter->collectBoundIdentifiers(boundParameterNames);
            }
            for (auto& boundParameterName : boundParameterNames)
                scope->declareVariable(&boundParameterName);
        }
    }
    if (!name.isNull())
        scope->declareCallee(&name);
    next();
}

template <typename LexerType>
Parser<LexerType>::~Parser()
{
}

template <typename LexerType>
String Parser<LexerType>::parseInner()
{
    String parseError = String();
    
    ASTBuilder context(const_cast<VM*>(m_vm), const_cast<SourceCode*>(m_source));
    if (m_lexer->isReparsing())
        m_statementDepth--;
    ScopeRef scope = currentScope();
    SourceElements* sourceElements = parseSourceElements(context, CheckForStrictMode);
    if (!sourceElements || !consume(EOFTOK)) {
        if (hasError())
            parseError = m_errorMessage;
        else
            parseError = ASCIILiteral("Parser error");
    }

    IdentifierSet capturedVariables;
    bool modifiedParameter = false;
    scope->getCapturedVariables(capturedVariables, modifiedParameter);
    
    CodeFeatures features = context.features();
    if (scope->strictMode())
        features |= StrictModeFeature;
    if (scope->shadowsArguments())
        features |= ShadowsArgumentsFeature;
    if (modifiedParameter)
        features |= ModifiedParameterFeature;
    
    Vector<RefPtr<StringImpl>> closedVariables;
    if (m_parsingBuiltin) {
        RELEASE_ASSERT(!capturedVariables.size());
        IdentifierSet usedVariables;
        scope->getUsedVariables(usedVariables);
        for (const auto& variable : usedVariables) {
            if (scope->hasDeclaredVariable(Identifier(m_vm, variable.get())))
                continue;
            
            if (scope->hasDeclaredParameter(Identifier(m_vm, variable.get())))
                continue;
            closedVariables.append(variable);
        }
    }
    didFinishParsing(sourceElements, context.varDeclarations(), context.funcDeclarations(), features,
        context.numConstants(), capturedVariables, WTF::move(closedVariables));

    return parseError;
}

template <typename LexerType>
void Parser<LexerType>::didFinishParsing(SourceElements* sourceElements, ParserArenaData<DeclarationStacks::VarStack>* varStack, 
    ParserArenaData<DeclarationStacks::FunctionStack>* funcStack, CodeFeatures features, int numConstants, IdentifierSet& capturedVars, const Vector<RefPtr<StringImpl>>&& closedVariables)
{
    m_sourceElements = sourceElements;
    m_varDeclarations = varStack;
    m_funcDeclarations = funcStack;
    m_capturedVariables.swap(capturedVars);
    m_closedVariables = closedVariables;
    m_features = features;
    m_numConstants = numConstants;
}

template <typename LexerType>
bool Parser<LexerType>::allowAutomaticSemicolon()
{
    return match(CLOSEBRACE) || match(EOFTOK) || m_lexer->prevTerminator();
}

template <typename LexerType>
template <class TreeBuilder> TreeSourceElements Parser<LexerType>::parseSourceElements(TreeBuilder& context, SourceElementsMode mode)
{
    const unsigned lengthOfUseStrictLiteral = 12; // "use strict".length
    TreeSourceElements sourceElements = context.createSourceElements();
    bool seenNonDirective = false;
    const Identifier* directive = 0;
    unsigned directiveLiteralLength = 0;
    auto savePoint = createSavePoint();
    bool hasSetStrict = false;
    while (TreeStatement statement = parseStatement(context, directive, &directiveLiteralLength)) {
        if (mode == CheckForStrictMode && !seenNonDirective) {
            if (directive) {
                // "use strict" must be the exact literal without escape sequences or line continuation.
                if (!hasSetStrict && directiveLiteralLength == lengthOfUseStrictLiteral && m_vm->propertyNames->useStrictIdentifier == *directive) {
                    setStrictMode();
                    hasSetStrict = true;
                    if (!isValidStrictMode()) {
                        if (m_lastFunctionName) {
                            if (m_vm->propertyNames->arguments == *m_lastFunctionName)
                                semanticFail("Cannot name a function 'arguments' in strict mode");
                            if (m_vm->propertyNames->eval == *m_lastFunctionName)
                                semanticFail("Cannot name a function 'eval' in strict mode");
                        }
                        if (hasDeclaredVariable(m_vm->propertyNames->arguments))
                            semanticFail("Cannot declare a variable named 'arguments' in strict mode");
                        if (hasDeclaredVariable(m_vm->propertyNames->eval))
                            semanticFail("Cannot declare a variable named 'eval' in strict mode");
                        semanticFailIfFalse(isValidStrictMode(), "Invalid parameters or function name in strict mode");
                    }
                    restoreSavePoint(savePoint);
                    propagateError();
                    continue;
                }
            } else
                seenNonDirective = true;
        }
        context.appendStatement(sourceElements, statement);
    }

    propagateError();
    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseVarDeclaration(TreeBuilder& context)
{
    ASSERT(match(VAR));
    JSTokenLocation location(tokenLocation());
    int start = tokenLine();
    int end = 0;
    int scratch;
    TreeDeconstructionPattern scratch1 = 0;
    TreeExpression scratch2 = 0;
    JSTextPosition scratch3;
    TreeExpression varDecls = parseVarDeclarationList(context, scratch, scratch1, scratch2, scratch3, scratch3, scratch3);
    propagateError();
    failIfFalse(autoSemiColon(), "Expected ';' after var declaration");
    
    return context.createVarStatement(location, varDecls, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseConstDeclaration(TreeBuilder& context)
{
    ASSERT(match(CONSTTOKEN));
    JSTokenLocation location(tokenLocation());
    int start = tokenLine();
    int end = 0;
    TreeConstDeclList constDecls = parseConstDeclarationList(context);
    propagateError();
    failIfFalse(autoSemiColon(), "Expected ';' after const declaration");
    
    return context.createConstStatement(location, constDecls, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseDoWhileStatement(TreeBuilder& context)
{
    ASSERT(match(DO));
    int startLine = tokenLine();
    next();
    const Identifier* unused = 0;
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
    int endLine = tokenLine();
    handleProductionOrFail(CLOSEPAREN, ")", "end", "while loop condition");
    
    const Identifier* unused = 0;
    startLoop();
    TreeStatement statement = parseStatement(context, unused);
    endLoop();
    failIfFalse(statement, "Expected a statement as the body of a while loop");
    return context.createWhileStatement(location, expr, statement, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseVarDeclarationList(TreeBuilder& context, int& declarations, TreeDeconstructionPattern& lastPattern, TreeExpression& lastInitializer, JSTextPosition& identStart, JSTextPosition& initStart, JSTextPosition& initEnd)
{
    TreeExpression varDecls = 0;
    const Identifier* lastIdent;
    do {
        lastIdent = 0;
        lastPattern = 0;
        JSTokenLocation location(tokenLocation());
        next();
        TreeExpression node = 0;
        declarations++;
        bool hasInitializer = false;
        if (match(IDENT)) {
            JSTextPosition varStart = tokenStartPosition();
            identStart = varStart;
            const Identifier* name = m_token.m_data.ident;
            lastIdent = name;
            next();
            hasInitializer = match(EQUAL);
            failIfFalseIfStrict(declareVariable(name), "Cannot declare a variable named ", name->impl(), " in strict mode");
            context.addVar(name, (hasInitializer || (!m_allowsIn && (match(INTOKEN) || isofToken()))) ? DeclarationStacks::HasInitializer : 0);
            if (hasInitializer) {
                JSTextPosition varDivot = tokenStartPosition() + 1;
                initStart = tokenStartPosition();
                next(TreeBuilder::DontBuildStrings); // consume '='
                TreeExpression initializer = parseAssignmentExpression(context);
                initEnd = lastTokenEndPosition();
                lastInitializer = initializer;
                failIfFalse(initializer, "Expected expression as the intializer for the variable '", name->impl(), "'");
                
                node = context.createAssignResolve(location, *name, initializer, varStart, varDivot, lastTokenEndPosition());
            }
        } else {
            lastIdent = 0;
            auto pattern = parseDeconstructionPattern(context, DeconstructToVariables);
            failIfFalse(pattern, "Cannot parse this deconstruction pattern");
            hasInitializer = match(EQUAL);
            lastPattern = pattern;
            if (hasInitializer) {
                next(TreeBuilder::DontBuildStrings); // consume '='
                TreeExpression rhs = parseExpression(context);
                node = context.createDeconstructingAssignment(location, pattern, rhs);
                lastInitializer = rhs;
            }
        }
        
        if (hasInitializer) {
            if (!varDecls)
                varDecls = node;
            else
                varDecls = context.combineCommaNodes(location, varDecls, node);
        }
    } while (match(COMMA));
    if (lastIdent)
        lastPattern = createBindingPattern(context, DeconstructToVariables, *lastIdent, 0);
    return varDecls;
}

template <typename LexerType>
template <class TreeBuilder> TreeDeconstructionPattern Parser<LexerType>::createBindingPattern(TreeBuilder& context, DeconstructionKind kind, const Identifier& name, int depth)
{
    ASSERT(!name.isEmpty());
    ASSERT(!name.isNull());
    
    ASSERT(name.impl()->isAtomic());
    if (depth) {
        if (kind == DeconstructToVariables)
            failIfFalseIfStrict(declareVariable(&name), "Cannot deconstruct to a variable named '", name.impl(), "' in strict mode");
        if (kind == DeconstructToParameters) {
            auto bindingResult = declareBoundParameter(&name);
            if (bindingResult == Scope::StrictBindingFailed && strictMode()) {
                semanticFailIfTrue(m_vm->propertyNames->arguments == name || m_vm->propertyNames->eval == name, "Cannot deconstruct to a parameter name '", name.impl(), "' in strict mode");
                if (m_lastFunctionName && name == *m_lastFunctionName)
                    semanticFail("Cannot deconstruct to '", name.impl(), "' as it shadows the name of a strict mode function");
                semanticFailureDueToKeyword("bound parameter name");
                if (hasDeclaredParameter(name))
                    semanticFail("Cannot deconstruct to '", name.impl(), "' as it has already been declared");
                semanticFail("Cannot bind to a parameter named '", name.impl(), "' in strict mode");
            }
            if (bindingResult == Scope::BindingFailed) {
                semanticFailureDueToKeyword("bound parameter name");
                if (hasDeclaredParameter(name))
                    semanticFail("Cannot deconstruct to '", name.impl(), "' as it has already been declared");
                semanticFail("Cannot deconstruct to a parameter named '", name.impl(), "'");
            }
        }
        if (kind != DeconstructToExpressions)
            context.addVar(&name, DeclarationStacks::HasInitializer);

    } else {
        if (kind == DeconstructToVariables) {
            failIfFalseIfStrict(declareVariable(&name), "Cannot declare a variable named '", name.impl(), "' in strict mode");
            context.addVar(&name, DeclarationStacks::HasInitializer);
        }
        
        if (kind == DeconstructToParameters) {
            bool declarationResult = declareParameter(&name);
            if (!declarationResult && strictMode()) {
                semanticFailIfTrue(m_vm->propertyNames->arguments == name || m_vm->propertyNames->eval == name, "Cannot deconstruct to a parameter name '", name.impl(), "' in strict mode");
                if (m_lastFunctionName && name == *m_lastFunctionName)
                    semanticFail("Cannot declare a parameter named '", name.impl(), "' as it shadows the name of a strict mode function");
                semanticFailureDueToKeyword("parameter name");
                if (hasDeclaredParameter(name))
                    semanticFail("Cannot declare a parameter named '", name.impl(), "' in strict mode as it has already been declared");
                semanticFail("Cannot declare a parameter named '", name.impl(), "' in strict mode");
            }
        }
    }
    return context.createBindingLocation(m_token.m_location, name, m_token.m_startPosition, m_token.m_endPosition);
}

template <typename LexerType>
template <class TreeBuilder> TreeDeconstructionPattern Parser<LexerType>::tryParseDeconstructionPatternExpression(TreeBuilder& context)
{
    return parseDeconstructionPattern(context, DeconstructToExpressions);
}

template <typename LexerType>
template <class TreeBuilder> TreeDeconstructionPattern Parser<LexerType>::parseDeconstructionPattern(TreeBuilder& context, DeconstructionKind kind, int depth)
{
    failIfStackOverflow();
    int nonLHSCount = m_nonLHSCount;
    TreeDeconstructionPattern pattern;
    switch (m_token.m_type) {
    case OPENBRACKET: {
        auto arrayPattern = context.createArrayPattern(m_token.m_location);
        next();
        if (kind == DeconstructToExpressions && match(CLOSEBRACKET))
            return 0;
        failIfTrue(match(CLOSEBRACKET), "There must be at least one bound property in an array deconstruction pattern");
        do {
            while (match(COMMA)) {
                context.appendArrayPatternSkipEntry(arrayPattern, m_token.m_location);
                next();
            }
            propagateError();
            JSTokenLocation location = m_token.m_location;
            auto innerPattern = parseDeconstructionPattern(context, kind, depth + 1);
            if (kind == DeconstructToExpressions && !innerPattern)
                return 0;
            failIfFalse(innerPattern, "Cannot parse this deconstruction pattern");
            context.appendArrayPatternEntry(arrayPattern, location, innerPattern);
        } while (consume(COMMA));
        
        if (kind == DeconstructToExpressions && !match(CLOSEBRACKET))
            return 0;

        consumeOrFail(CLOSEBRACKET, "Expected either a closing ']' or a ',' following an element deconstruction pattern");
        pattern = arrayPattern;
        break;
    }
    case OPENBRACE: {
        next();
        
        if (kind == DeconstructToExpressions && match(CLOSEBRACE))
            return 0;

        failIfTrue(match(CLOSEBRACE), "There must be at least one bound property in an object deconstruction pattern");
        auto objectPattern = context.createObjectPattern(m_token.m_location);
        bool wasString = false;
        do {
            Identifier propertyName;
            TreeDeconstructionPattern innerPattern = 0;
            JSTokenLocation location = m_token.m_location;
            if (match(IDENT)) {
                propertyName = *m_token.m_data.ident;
                next();
                if (consume(COLON))
                    innerPattern = parseDeconstructionPattern(context, kind, depth + 1);
                else
                    innerPattern = createBindingPattern(context, kind, propertyName, depth);
            } else {
                JSTokenType tokenType = m_token.m_type;
                switch (m_token.m_type) {
                case NUMBER:
                    propertyName = Identifier::from(m_vm, m_token.m_data.doubleValue);
                    break;
                case STRING:
                    propertyName = *m_token.m_data.ident;
                    wasString = true;
                    break;
                default:
                    if (m_token.m_type != RESERVED && m_token.m_type != RESERVED_IF_STRICT && !(m_token.m_type & KeywordTokenFlag)) {
                        if (kind == DeconstructToExpressions)
                            return 0;
                        failWithMessage("Expected a property name");
                    }
                    propertyName = *m_token.m_data.ident;
                    break;
                }
                next();
                if (!consume(COLON)) {
                    if (kind == DeconstructToExpressions)
                        return 0;
                    semanticFailIfTrue(tokenType == RESERVED, "Cannot use abbreviated deconstruction syntax for reserved name '", propertyName.impl(), "'");
                    semanticFailIfTrue(tokenType == RESERVED_IF_STRICT, "Cannot use abbreviated deconstruction syntax for reserved name '", propertyName.impl(), "' in strict mode");
                    semanticFailIfTrue(tokenType & KeywordTokenFlag, "Cannot use abbreviated deconstruction syntax for keyword '", propertyName.impl(), "'");
                    
                    failWithMessage("Expected a ':' prior to named property deconstruction");
                }
                innerPattern = parseDeconstructionPattern(context, kind, depth + 1);
            }
            if (kind == DeconstructToExpressions && !innerPattern)
                return 0;
            failIfFalse(innerPattern, "Cannot parse this deconstruction pattern");
            context.appendObjectPatternEntry(objectPattern, location, wasString, propertyName, innerPattern);
        } while (consume(COMMA));
        if (kind == DeconstructToExpressions && !match(CLOSEBRACE))
            return 0;
        consumeOrFail(CLOSEBRACE, "Expected either a closing '}' or an ',' after a property deconstruction pattern");
        pattern = objectPattern;
        break;
    }

    default: {
        if (!match(IDENT)) {
            if (kind == DeconstructToExpressions)
                return 0;
            semanticFailureDueToKeyword("variable name");
            failWithMessage("Expected a parameter pattern or a ')' in parameter list");
        }
        pattern = createBindingPattern(context, kind, *m_token.m_data.ident, depth);
        next();
        break;
    }
    }
    m_nonLHSCount = nonLHSCount;
    return pattern;
}

template <typename LexerType>
template <class TreeBuilder> TreeConstDeclList Parser<LexerType>::parseConstDeclarationList(TreeBuilder& context)
{
    failIfTrue(strictMode(), "Const declarations are not supported in strict mode");
    TreeConstDeclList constDecls = 0;
    TreeConstDeclList tail = 0;
    do {
        JSTokenLocation location(tokenLocation());
        next();
        matchOrFail(IDENT, "Expected an identifier name in const declaration");
        const Identifier* name = m_token.m_data.ident;
        next();
        bool hasInitializer = match(EQUAL);
        declareVariable(name);
        context.addVar(name, DeclarationStacks::IsConstant | (hasInitializer ? DeclarationStacks::HasInitializer : 0));

        TreeExpression initializer = 0;
        if (hasInitializer) {
            next(TreeBuilder::DontBuildStrings); // consume '='
            initializer = parseAssignmentExpression(context);
            failIfFalse(!!initializer, "Unable to parse initializer");
        }
        tail = context.appendConstDecl(location, tail, name, initializer);
        if (!constDecls)
            constDecls = tail;
    } while (match(COMMA));
    return constDecls;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseForStatement(TreeBuilder& context)
{
    ASSERT(match(FOR));
    JSTokenLocation location(tokenLocation());
    int startLine = tokenLine();
    next();
    handleProductionOrFail(OPENPAREN, "(", "start", "for-loop header");
    int nonLHSCount = m_nonLHSCount;
    int declarations = 0;
    JSTextPosition declsStart;
    JSTextPosition declsEnd;
    TreeExpression decls = 0;
    TreeDeconstructionPattern pattern = 0;
    if (match(VAR)) {
        /*
         for (var IDENT in expression) statement
         for (var varDeclarationList; expressionOpt; expressionOpt)
         */
        TreeDeconstructionPattern forInTarget = 0;
        TreeExpression forInInitializer = 0;
        m_allowsIn = false;
        JSTextPosition initStart;
        JSTextPosition initEnd;
        decls = parseVarDeclarationList(context, declarations, forInTarget, forInInitializer, declsStart, initStart, initEnd);
        m_allowsIn = true;
        propagateError();

        // Remainder of a standard for loop is handled identically
        if (match(SEMICOLON))
            goto standardForLoop;
        
        failIfFalse(declarations == 1, "can only declare a single variable in an enumeration");
        failIfTrueIfStrict(forInInitializer, "Cannot use initialiser syntax in a strict mode enumeration");

        if (forInInitializer)
            failIfFalse(context.isBindingNode(forInTarget), "Cannot use initialiser syntax when binding to a pattern during enumeration");

        // Handle for-in with var declaration
        JSTextPosition inLocation = tokenStartPosition();
        bool isOfEnumeration = false;
        if (!consume(INTOKEN)) {
            failIfFalse(match(IDENT) && *m_token.m_data.ident == m_vm->propertyNames->of, "Expected either 'in' or 'of' in enumeration syntax");
            isOfEnumeration = true;
            failIfTrue(forInInitializer, "Cannot use initialiser syntax in a for-of enumeration");
            next();
        }
        TreeExpression expr = parseExpression(context);
        failIfFalse(expr, "Expected expression to enumerate");
        JSTextPosition exprEnd = lastTokenEndPosition();
        
        int endLine = tokenLine();
        
        handleProductionOrFail(CLOSEPAREN, ")", "end", (isOfEnumeration ? "for-of header" : "for-in header"));
        
        const Identifier* unused = 0;
        startLoop();
        TreeStatement statement = parseStatement(context, unused);
        endLoop();
        failIfFalse(statement, "Expected statement as body of for-", isOfEnumeration ? "of" : "in", " statement");
        if (isOfEnumeration)
            return context.createForOfLoop(location, forInTarget, expr, statement, declsStart, inLocation, exprEnd, startLine, endLine);
        return context.createForInLoop(location, forInTarget, expr, statement, declsStart, inLocation, exprEnd, startLine, endLine);
    }
    
    if (!match(SEMICOLON)) {
        if (match(OPENBRACE) || match(OPENBRACKET)) {
            SavePoint savePoint = createSavePoint();
            declsStart = tokenStartPosition();
            pattern = tryParseDeconstructionPatternExpression(context);
            declsEnd = lastTokenEndPosition();
            if (pattern && (match(INTOKEN) || (match(IDENT) && *m_token.m_data.ident == m_vm->propertyNames->of)))
                goto enumerationLoop;
            pattern = 0;
            restoreSavePoint(savePoint);
        }
        m_allowsIn = false;
        declsStart = tokenStartPosition();
        decls = parseExpression(context);
        declsEnd = lastTokenEndPosition();
        m_allowsIn = true;
        failIfFalse(decls, "Cannot parse for loop declarations");
    }
    
    if (match(SEMICOLON)) {
    standardForLoop:
        // Standard for loop
        next();
        TreeExpression condition = 0;
        
        if (!match(SEMICOLON)) {
            condition = parseExpression(context);
            failIfFalse(condition, "Cannot parse for loop condition expression");
        }
        consumeOrFail(SEMICOLON, "Expected a ';' after the for loop condition expression");
        
        TreeExpression increment = 0;
        if (!match(CLOSEPAREN)) {
            increment = parseExpression(context);
            failIfFalse(increment, "Cannot parse for loop iteration expression");
        }
        int endLine = tokenLine();
        handleProductionOrFail(CLOSEPAREN, ")", "end", "for-loop header");
        const Identifier* unused = 0;
        startLoop();
        TreeStatement statement = parseStatement(context, unused);
        endLoop();
        failIfFalse(statement, "Expected a statement as the body of a for loop");
        return context.createForLoop(location, decls, condition, increment, statement, startLine, endLine);
    }
    
    // For-in loop
enumerationLoop:
    failIfFalse(nonLHSCount == m_nonLHSCount, "Expected a reference on the left hand side of an enumeration statement");
    bool isOfEnumeration = false;
    if (!consume(INTOKEN)) {
        failIfFalse(match(IDENT) && *m_token.m_data.ident == m_vm->propertyNames->of, "Expected either 'in' or 'of' in enumeration syntax");
        isOfEnumeration = true;
        next();
    }
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr, "Cannot parse subject for-", isOfEnumeration ? "of" : "in", " statement");
    JSTextPosition exprEnd = lastTokenEndPosition();
    int endLine = tokenLine();
    
    handleProductionOrFail(CLOSEPAREN, ")", "end", (isOfEnumeration ? "for-of header" : "for-in header"));
    const Identifier* unused = 0;
    startLoop();
    TreeStatement statement = parseStatement(context, unused);
    endLoop();
    failIfFalse(statement, "Expected a statement as the body of a for-", isOfEnumeration ? "of" : "in", "loop");
    if (pattern) {
        ASSERT(!decls);
        if (isOfEnumeration)
            return context.createForOfLoop(location, pattern, expr, statement, declsStart, declsEnd, exprEnd, startLine, endLine);
        return context.createForInLoop(location, pattern, expr, statement, declsStart, declsEnd, exprEnd, startLine, endLine);
    }
    if (isOfEnumeration)
        return context.createForOfLoop(location, decls, expr, statement, declsStart, declsEnd, exprEnd, startLine, endLine);
    return context.createForInLoop(location, decls, expr, statement, declsStart, declsEnd, exprEnd, startLine, endLine);
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
        return context.createBreakStatement(location, start, end);
    }
    matchOrFail(IDENT, "Expected an identifier as the target for a break statement");
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
        return context.createContinueStatement(location, start, end);
    }
    matchOrFail(IDENT, "Expected an identifier as the target for a continue statement");
    const Identifier* ident = m_token.m_data.ident;
    ScopeLabelInfo* label = getLabel(ident);
    semanticFailIfFalse(label, "Cannot use the undeclared label '", ident->impl(), "'");
    semanticFailIfFalse(label->m_isLoop, "Cannot continue to the label '", ident->impl(), "' as it is not targeting a loop");
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
    TreeExpression expr = parseExpression(context);
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
    
    TreeExpression expr = parseExpression(context);
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
    JSTextPosition end = lastTokenEndPosition();
    int endLine = tokenLine();
    handleProductionOrFail(CLOSEPAREN, ")", "start", "subject of a 'with' statement");
    const Identifier* unused = 0;
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
    int endLine = tokenLine();
    
    handleProductionOrFail(CLOSEPAREN, ")", "end", "subject of a 'switch'");
    handleProductionOrFail(OPENBRACE, "{", "start", "body of a 'switch'");
    startSwitch();
    TreeClauseList firstClauses = parseSwitchClauses(context);
    propagateError();
    
    TreeClause defaultClause = parseSwitchDefaultClause(context);
    propagateError();
    
    TreeClauseList secondClauses = parseSwitchClauses(context);
    propagateError();
    endSwitch();
    handleProductionOrFail(CLOSEBRACE, "}", "end", "body of a 'switch'");
    
    return context.createSwitchStatement(location, expr, firstClauses, defaultClause, secondClauses, startLine, endLine);
    
}

template <typename LexerType>
template <class TreeBuilder> TreeClauseList Parser<LexerType>::parseSwitchClauses(TreeBuilder& context)
{
    if (!match(CASE))
        return 0;
    next();
    TreeExpression condition = parseExpression(context);
    failIfFalse(condition, "Cannot parse switch clause");
    consumeOrFail(COLON, "Expected a ':' after switch clause expression");
    TreeSourceElements statements = parseSourceElements(context, DontCheckForStrictMode);
    failIfFalse(statements, "Cannot parse the body of a switch clause");
    TreeClause clause = context.createClause(condition, statements);
    TreeClauseList clauseList = context.createClauseList(clause);
    TreeClauseList tail = clauseList;
    
    while (match(CASE)) {
        next();
        TreeExpression condition = parseExpression(context);
        failIfFalse(condition, "Cannot parse switch case expression");
        consumeOrFail(COLON, "Expected a ':' after switch clause expression");
        TreeSourceElements statements = parseSourceElements(context, DontCheckForStrictMode);
        failIfFalse(statements, "Cannot parse the body of a switch clause");
        clause = context.createClause(condition, statements);
        tail = context.createClauseList(tail, clause);
    }
    return clauseList;
}

template <typename LexerType>
template <class TreeBuilder> TreeClause Parser<LexerType>::parseSwitchDefaultClause(TreeBuilder& context)
{
    if (!match(DEFAULT))
        return 0;
    next();
    consumeOrFail(COLON, "Expected a ':' after switch default clause");
    TreeSourceElements statements = parseSourceElements(context, DontCheckForStrictMode);
    failIfFalse(statements, "Cannot parse the body of a switch default clause");
    return context.createClause(0, statements);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseTryStatement(TreeBuilder& context)
{
    ASSERT(match(TRY));
    JSTokenLocation location(tokenLocation());
    TreeStatement tryBlock = 0;
    const Identifier* ident = &m_vm->propertyNames->nullIdentifier;
    TreeStatement catchBlock = 0;
    TreeStatement finallyBlock = 0;
    int firstLine = tokenLine();
    next();
    matchOrFail(OPENBRACE, "Expected a block statement as body of a try statement");
    
    tryBlock = parseBlockStatement(context);
    failIfFalse(tryBlock, "Cannot parse the body of try block");
    int lastLine = m_lastTokenEndPosition.line;
    
    if (match(CATCH)) {
        currentScope()->setNeedsFullActivation();
        next();
        
        handleProductionOrFail(OPENPAREN, "(", "start", "'catch' target");
        if (!match(IDENT)) {
            semanticFailureDueToKeyword("catch variable name");
            failWithMessage("Expected identifier name as catch target");
        }
        ident = m_token.m_data.ident;
        next();
        AutoPopScopeRef catchScope(this, pushScope());
        failIfFalseIfStrict(declareVariable(ident), "Cannot declare a catch variable named '", ident->impl(), "' in strict mode");
        catchScope->preventNewDecls();
        handleProductionOrFail(CLOSEPAREN, ")", "end", "'catch' target");
        matchOrFail(OPENBRACE, "Expected exception handler to be a block statement");
        catchBlock = parseBlockStatement(context);
        failIfFalse(catchBlock, "Unable to parse 'catch' block");
        failIfFalse(popScope(catchScope, TreeBuilder::NeedsFreeVariableInfo), "Parse error");
    }
    
    if (match(FINALLY)) {
        next();
        matchOrFail(OPENBRACE, "Expected block statement for finally body");
        finallyBlock = parseBlockStatement(context);
        failIfFalse(finallyBlock, "Cannot parse finally body");
    }
    failIfFalse(catchBlock || finallyBlock, "Try statements must have at least a catch or finally block");
    return context.createTryStatement(location, tryBlock, ident, catchBlock, finallyBlock, firstLine, lastLine);
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
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseBlockStatement(TreeBuilder& context)
{
    ASSERT(match(OPENBRACE));
    JSTokenLocation location(tokenLocation());
    int start = tokenLine();
    next();
    if (match(CLOSEBRACE)) {
        next();
        return context.createBlockStatement(location, 0, start, m_lastTokenEndPosition.line);
    }
    TreeSourceElements subtree = parseSourceElements(context, DontCheckForStrictMode);
    failIfFalse(subtree, "Cannot parse the body of the block statement");
    matchOrFail(CLOSEBRACE, "Expected a closing '}' at the end of a block statement");
    next();
    return context.createBlockStatement(location, subtree, start, m_lastTokenEndPosition.line);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseStatement(TreeBuilder& context, const Identifier*& directive, unsigned* directiveLiteralLength)
{
    DepthManager statementDepth(&m_statementDepth);
    m_statementDepth++;
    directive = 0;
    int nonTrivialExpressionCount = 0;
    failIfStackOverflow();
    switch (m_token.m_type) {
    case OPENBRACE:
        return parseBlockStatement(context);
    case VAR:
        return parseVarDeclaration(context);
    case CONSTTOKEN:
        return parseConstDeclaration(context);
    case FUNCTION:
        failIfFalseIfStrict(m_statementDepth == 1, "Strict mode does not allow function declarations in a lexically nested statement");
        return parseFunctionDeclaration(context);
    case SEMICOLON: {
        JSTokenLocation location(tokenLocation());
        next();
        return context.createEmptyStatement(location);
    }
    case IF:
        return parseIfStatement(context);
    case DO:
        return parseDoWhileStatement(context);
    case WHILE:
        return parseWhileStatement(context);
    case FOR:
        return parseForStatement(context);
    case CONTINUE:
        return parseContinueStatement(context);
    case BREAK:
        return parseBreakStatement(context);
    case RETURN:
        return parseReturnStatement(context);
    case WITH:
        return parseWithStatement(context);
    case SWITCH:
        return parseSwitchStatement(context);
    case THROW:
        return parseThrowStatement(context);
    case TRY:
        return parseTryStatement(context);
    case DEBUGGER:
        return parseDebuggerStatement(context);
    case EOFTOK:
    case CASE:
    case CLOSEBRACE:
    case DEFAULT:
        // These tokens imply the end of a set of source elements
        return 0;
    case IDENT:
        return parseExpressionOrLabelStatement(context);
    case STRING:
        directive = m_token.m_data.ident;
        if (directiveLiteralLength)
            *directiveLiteralLength = m_token.m_location.endOffset - m_token.m_location.startOffset;
        nonTrivialExpressionCount = m_nonTrivialExpressionCount;
        FALLTHROUGH;
    default:
        TreeStatement exprStatement = parseExpressionStatement(context);
        if (directive && nonTrivialExpressionCount != m_nonTrivialExpressionCount)
            directive = 0;
        return exprStatement;
    }
}

template <typename LexerType>
template <class TreeBuilder> TreeFormalParameterList Parser<LexerType>::parseFormalParameters(TreeBuilder& context)
{
    auto parameter = parseDeconstructionPattern(context, DeconstructToParameters);
    failIfFalse(parameter, "Cannot parse parameter pattern");
    TreeFormalParameterList list = context.createFormalParameterList(parameter);
    TreeFormalParameterList tail = list;
    while (consume(COMMA)) {
        parameter = parseDeconstructionPattern(context, DeconstructToParameters);
        failIfFalse(parameter, "Cannot parse parameter pattern");
        tail = context.createFormalParameterList(tail, parameter);
    }
    return list;
}

template <typename LexerType>
template <class TreeBuilder> TreeFunctionBody Parser<LexerType>::parseFunctionBody(TreeBuilder& context)
{
    JSTokenLocation startLocation(tokenLocation());
    unsigned startColumn = tokenColumn();
    next();

    if (match(CLOSEBRACE)) {
        unsigned endColumn = tokenColumn();
        return context.createFunctionBody(startLocation, tokenLocation(), startColumn, endColumn, strictMode());
    }
    DepthManager statementDepth(&m_statementDepth);
    m_statementDepth = 0;
    typename TreeBuilder::FunctionBodyBuilder bodyBuilder(const_cast<VM*>(m_vm), m_lexer.get());
    failIfFalse(parseSourceElements(bodyBuilder, CheckForStrictMode), "Cannot parse body of this function");
    unsigned endColumn = tokenColumn();
    return context.createFunctionBody(startLocation, tokenLocation(), startColumn, endColumn, strictMode());
}

static const char* stringForFunctionMode(FunctionParseMode mode)
{
    switch (mode) {
    case GetterMode:
        return "getter";
    case SetterMode:
        return "setter";
    case FunctionMode:
        return "function";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

template <typename LexerType>
template <class TreeBuilder> bool Parser<LexerType>::parseFunctionInfo(TreeBuilder& context, FunctionRequirements requirements, FunctionParseMode mode, bool nameIsInContainingScope, const Identifier*& name, TreeFormalParameterList& parameters, TreeFunctionBody& body, unsigned& openBraceOffset, unsigned& closeBraceOffset, int& bodyStartLine, unsigned& bodyStartColumn)
{
    AutoPopScopeRef functionScope(this, pushScope());
    functionScope->setIsFunction();
    int functionNameStart = m_token.m_location.startOffset;
    const Identifier* lastFunctionName = m_lastFunctionName;
    m_lastFunctionName = nullptr;
    if (match(IDENT)) {
        name = m_token.m_data.ident;
        m_lastFunctionName = name;
        next();
        if (!nameIsInContainingScope)
            failIfFalseIfStrict(functionScope->declareVariable(name), "'", name->impl(), "' is not a valid ", stringForFunctionMode(mode), " name in strict mode");
    } else if (requirements == FunctionNeedsName) {
        if (match(OPENPAREN) && mode == FunctionMode)
            semanticFail("Function statements must have a name");
        semanticFailureDueToKeyword(stringForFunctionMode(mode), " name");
        failDueToUnexpectedToken();
        return false;
    }
    if (!consume(OPENPAREN)) {
        semanticFailureDueToKeyword(stringForFunctionMode(mode), " name");
        failWithMessage("Expected an opening '(' before a ", stringForFunctionMode(mode), "'s parameter list");
    }
    if (!match(CLOSEPAREN)) {
        parameters = parseFormalParameters(context);
        failIfFalse(parameters, "Cannot parse parameters for this ", stringForFunctionMode(mode));
    }
    consumeOrFail(CLOSEPAREN, "Expected a ')' or a ',' after a parameter declaration");
    matchOrFail(OPENBRACE, "Expected an opening '{' at the start of a ", stringForFunctionMode(mode), " body");
    
    openBraceOffset = m_token.m_data.offset;
    bodyStartLine = tokenLine();
    bodyStartColumn = m_token.m_data.offset - m_token.m_data.lineStartOffset;
    JSTokenLocation startLocation(tokenLocation());
    
    // If we know about this function already, we can use the cached info and skip the parser to the end of the function.
    if (const SourceProviderCacheItem* cachedInfo = TreeBuilder::CanUseFunctionCache ? findCachedFunctionInfo(openBraceOffset) : 0) {
        // If we're in a strict context, the cached function info must say it was strict too.
        ASSERT(!strictMode() || cachedInfo->strictMode);
        JSTokenLocation endLocation;

        endLocation.line = cachedInfo->closeBraceLine;
        endLocation.startOffset = cachedInfo->closeBraceOffset;
        endLocation.lineStartOffset = cachedInfo->closeBraceLineStartOffset;

        bool endColumnIsOnStartLine = (endLocation.line == bodyStartLine);
        ASSERT(endLocation.startOffset >= endLocation.lineStartOffset);
        unsigned bodyEndColumn = endColumnIsOnStartLine ?
            endLocation.startOffset - m_token.m_data.lineStartOffset :
            endLocation.startOffset - endLocation.lineStartOffset;

        body = context.createFunctionBody(startLocation, endLocation, bodyStartColumn, bodyEndColumn, cachedInfo->strictMode);
        
        functionScope->restoreFromSourceProviderCache(cachedInfo);
        failIfFalse(popScope(functionScope, TreeBuilder::NeedsFreeVariableInfo), "Parser error");
        
        closeBraceOffset = cachedInfo->closeBraceOffset;

        context.setFunctionNameStart(body, functionNameStart);
        m_token = cachedInfo->closeBraceToken();

        m_lexer->setOffset(m_token.m_location.endOffset, m_token.m_location.lineStartOffset);
        m_lexer->setLineNumber(m_token.m_location.line);
        
        next();
        return true;
    }
    m_lastFunctionName = lastFunctionName;
    ParserState oldState = saveState();
    body = parseFunctionBody(context);
    restoreState(oldState);
    failIfFalse(body, "Cannot parse the body of this ", stringForFunctionMode(mode));
    if (functionScope->strictMode() && name) {
        RELEASE_ASSERT(mode == FunctionMode);
        semanticFailIfTrue(m_vm->propertyNames->arguments == *name, "'", name->impl(), "' is not a valid function name in strict mode");
        semanticFailIfTrue(m_vm->propertyNames->eval == *name, "'", name->impl(), "' is not a valid function name in strict mode");
    }
    closeBraceOffset = m_token.m_data.offset;
    unsigned closeBraceLine = m_token.m_data.line;
    unsigned closeBraceLineStartOffset = m_token.m_data.lineStartOffset;
    
    // Cache the tokenizer state and the function scope the first time the function is parsed.
    // Any future reparsing can then skip the function.
    static const int minimumFunctionLengthToCache = 16;
    std::unique_ptr<SourceProviderCacheItem> newInfo;
    int functionLength = closeBraceOffset - openBraceOffset;
    if (TreeBuilder::CanUseFunctionCache && m_functionCache && functionLength > minimumFunctionLengthToCache) {
        SourceProviderCacheItemCreationParameters parameters;
        parameters.functionNameStart = functionNameStart;
        parameters.closeBraceLine = closeBraceLine;
        parameters.closeBraceOffset = closeBraceOffset;
        parameters.closeBraceLineStartOffset = closeBraceLineStartOffset;
        functionScope->fillParametersForSourceProviderCache(parameters);
        newInfo = SourceProviderCacheItem::create(parameters);

    }
    context.setFunctionNameStart(body, functionNameStart);
    
    failIfFalse(popScope(functionScope, TreeBuilder::NeedsFreeVariableInfo), "Parser error");
    matchOrFail(CLOSEBRACE, "Expected a closing '}' after a ", stringForFunctionMode(mode), " body");
    
    if (newInfo)
        m_functionCache->add(openBraceOffset, WTF::move(newInfo));
    
    next();
    return true;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseFunctionDeclaration(TreeBuilder& context)
{
    ASSERT(match(FUNCTION));
    JSTokenLocation location(tokenLocation());
    next();
    const Identifier* name = 0;
    TreeFormalParameterList parameters = 0;
    TreeFunctionBody body = 0;
    unsigned openBraceOffset = 0;
    unsigned closeBraceOffset = 0;
    int bodyStartLine = 0;
    unsigned bodyStartColumn = 0;
    failIfFalse((parseFunctionInfo(context, FunctionNeedsName, FunctionMode, true, name, parameters, body, openBraceOffset, closeBraceOffset, bodyStartLine, bodyStartColumn)), "Cannot parse this function");
    failIfFalse(name, "Function statements must have a name");
    failIfFalseIfStrict(declareVariable(name), "Cannot declare a function named '", name->impl(), "' in strict mode");
    return context.createFuncDeclStatement(location, name, body, parameters, openBraceOffset, closeBraceOffset, bodyStartLine, m_lastTokenEndPosition.line, bodyStartColumn);
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
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseExpressionOrLabelStatement(TreeBuilder& context)
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
            TreeExpression expression = parseExpression(context);
            failIfFalse(expression, "Cannot parse expression statement");
            if (!autoSemiColon())
                failDueToUnexpectedToken();
            return context.createExprStatement(location, expression, start, m_lastTokenEndPosition.line);
        }
        const Identifier* ident = m_token.m_data.ident;
        JSTextPosition end = tokenEndPosition();
        next();
        consumeOrFail(COLON, "Labels must be followed by a ':'");
        if (!m_syntaxAlreadyValidated) {
            // This is O(N^2) over the current list of consecutive labels, but I
            // have never seen more than one label in a row in the real world.
            for (size_t i = 0; i < labels.size(); i++)
                failIfTrue(ident->impl() == labels[i].m_ident->impl(), "Attempted to redeclare the label '", ident->impl(), "'");
            failIfTrue(getLabel(ident), "Cannot find scope for the label '", ident->impl(), "'");
            labels.append(LabelInfo(ident, start, end));
        }
    } while (match(IDENT));
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
    const Identifier* unused = 0;
    if (!m_syntaxAlreadyValidated) {
        for (size_t i = 0; i < labels.size(); i++)
            pushLabel(labels[i].m_ident, isLoop);
    }
    TreeStatement statement = parseStatement(context, unused);
    if (!m_syntaxAlreadyValidated) {
        for (size_t i = 0; i < labels.size(); i++)
            popLabel();
    }
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
    JSTextPosition start = tokenStartPosition();
    JSTokenLocation location(tokenLocation());
    TreeExpression expression = parseExpression(context);
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
    handleProductionOrFail(OPENPAREN, "(", "start", "'if' condition");

    TreeExpression condition = parseExpression(context);
    failIfFalse(condition, "Expected a expression as the condition for an if statement");
    int end = tokenLine();
    handleProductionOrFail(CLOSEPAREN, ")", "end", "'if' condition");

    const Identifier* unused = 0;
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
            const Identifier* unused = 0;
            TreeStatement block = parseStatement(context, unused);
            failIfFalse(block, "Expected a statement as the body of an else block");
            statementStack.append(block);
            trailingElse = true;
            break;
        }
        int innerStart = tokenLine();
        next();
        
        handleProductionOrFail(OPENPAREN, "(", "start", "'if' condition");

        TreeExpression innerCondition = parseExpression(context);
        failIfFalse(innerCondition, "Expected a expression as the condition for an if statement");
        int innerEnd = tokenLine();
        handleProductionOrFail(CLOSEPAREN, ")", "end", "'if' condition");
        const Identifier* unused = 0;
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
        statementStack.append(context.createIfStatement(elseLocation, condition, trueBlock, 0, pos.first, pos.second));
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
        statementStack.append(context.createIfStatement(elseLocation, condition, trueBlock, falseBlock, pos.first, pos.second));
    }

    return context.createIfStatement(ifLocation, condition, trueBlock, statementStack.last(), start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseExpression(TreeBuilder& context)
{
    failIfStackOverflow();
    JSTokenLocation location(tokenLocation());
    TreeExpression node = parseAssignmentExpression(context);
    failIfFalse(node, "Cannot parse expression");
    if (!match(COMMA))
        return node;
    next();
    m_nonTrivialExpressionCount++;
    m_nonLHSCount++;
    TreeExpression right = parseAssignmentExpression(context);
    failIfFalse(right, "Cannot parse expression in a comma expression");
    typename TreeBuilder::Comma commaNode = context.createCommaExpr(location, node, right);
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        right = parseAssignmentExpression(context);
        failIfFalse(right, "Cannot parse expression in a comma expression");
        context.appendToComma(commaNode, right);
    }
    return commaNode;
}

template <typename LexerType>
template <typename TreeBuilder> TreeExpression Parser<LexerType>::parseAssignmentExpression(TreeBuilder& context)
{
    failIfStackOverflow();
    JSTextPosition start = tokenStartPosition();
    JSTokenLocation location(tokenLocation());
    int initialAssignmentCount = m_assignmentCount;
    int initialNonLHSCount = m_nonLHSCount;
    if (match(OPENBRACE) || match(OPENBRACKET)) {
        SavePoint savePoint = createSavePoint();
        auto pattern = tryParseDeconstructionPatternExpression(context);
        if (pattern && consume(EQUAL)) {
            auto rhs = parseAssignmentExpression(context);
            if (rhs)
                return context.createDeconstructingAssignment(location, pattern, rhs);
        }
        restoreSavePoint(savePoint);
    }
    TreeExpression lhs = parseConditionalExpression(context);
    failIfFalse(lhs, "Cannot parse expression");
    if (initialNonLHSCount != m_nonLHSCount) {
        if (m_token.m_type >= EQUAL && m_token.m_type <= OREQUAL)
            semanticFail("Left hand side of operator '", getToken(), "' must be a reference");

        return lhs;
    }
    
    int assignmentStack = 0;
    Operator op;
    bool hadAssignment = false;
    while (true) {
        switch (m_token.m_type) {
        case EQUAL: op = OpEqual; break;
        case PLUSEQUAL: op = OpPlusEq; break;
        case MINUSEQUAL: op = OpMinusEq; break;
        case MULTEQUAL: op = OpMultEq; break;
        case DIVEQUAL: op = OpDivEq; break;
        case LSHIFTEQUAL: op = OpLShift; break;
        case RSHIFTEQUAL: op = OpRShift; break;
        case URSHIFTEQUAL: op = OpURShift; break;
        case ANDEQUAL: op = OpAndEq; break;
        case XOREQUAL: op = OpXOrEq; break;
        case OREQUAL: op = OpOrEq; break;
        case MODEQUAL: op = OpModEq; break;
        default:
            goto end;
        }
        m_nonTrivialExpressionCount++;
        hadAssignment = true;
        context.assignmentStackAppend(assignmentStack, lhs, start, tokenStartPosition(), m_assignmentCount, op);
        start = tokenStartPosition();
        m_assignmentCount++;
        next(TreeBuilder::DontBuildStrings);
        if (strictMode() && m_lastIdentifier && context.isResolve(lhs)) {
            failIfTrueIfStrict(m_vm->propertyNames->eval == *m_lastIdentifier, "Cannot modify 'eval' in strict mode");
            failIfTrueIfStrict(m_vm->propertyNames->arguments == *m_lastIdentifier, "Cannot modify 'arguments' in strict mode");
            declareWrite(m_lastIdentifier);
            m_lastIdentifier = 0;
        }
        lhs = parseAssignmentExpression(context);
        failIfFalse(lhs, "Cannot parse the right hand side of an assignment expression");
        if (initialNonLHSCount != m_nonLHSCount) {
            if (m_token.m_type >= EQUAL && m_token.m_type <= OREQUAL)
                semanticFail("Left hand side of operator '", getToken(), "' must be a reference");
            break;
        }
    }
end:
    if (hadAssignment)
        m_nonLHSCount++;
    
    if (!TreeBuilder::CreatesAST)
        return lhs;
    
    while (assignmentStack)
        lhs = context.createAssignment(location, assignmentStack, lhs, initialAssignmentCount, m_assignmentCount, lastTokenEndPosition());
    
    return lhs;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseConditionalExpression(TreeBuilder& context)
{
    JSTokenLocation location(tokenLocation());
    TreeExpression cond = parseBinaryExpression(context);
    failIfFalse(cond, "Cannot parse expression");
    if (!match(QUESTION))
        return cond;
    m_nonTrivialExpressionCount++;
    m_nonLHSCount++;
    next(TreeBuilder::DontBuildStrings);
    TreeExpression lhs = parseAssignmentExpression(context);
    failIfFalse(lhs, "Cannot parse left hand side of ternary operator");
    consumeOrFailWithFlags(COLON, TreeBuilder::DontBuildStrings, "Expected ':' in ternary operator");
    
    TreeExpression rhs = parseAssignmentExpression(context);
    failIfFalse(rhs, "Cannot parse right hand side of ternary operator");
    return context.createConditionalExpr(location, cond, lhs, rhs);
}

ALWAYS_INLINE static bool isUnaryOp(JSTokenType token)
{
    return token & UnaryOpTokenFlag;
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
    while (true) {
        JSTextPosition exprStart = tokenStartPosition();
        int initialAssignments = m_assignmentCount;
        TreeExpression current = parseUnaryExpression(context);
        failIfFalse(current, "Cannot parse expression");
        
        context.appendBinaryExpressionInfo(operandStackDepth, current, exprStart, lastTokenEndPosition(), lastTokenEndPosition(), initialAssignments != m_assignmentCount);
        int precedence = isBinaryOperator(m_token.m_type);
        if (!precedence)
            break;
        m_nonTrivialExpressionCount++;
        m_nonLHSCount++;
        int operatorToken = m_token.m_type;
        next(TreeBuilder::DontBuildStrings);
        
        while (operatorStackDepth &&  context.operatorStackHasHigherPrecedence(operatorStackDepth, precedence)) {
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
template <class TreeBuilder> TreeProperty Parser<LexerType>::parseProperty(TreeBuilder& context, bool complete)
{
    bool wasIdent = false;
    switch (m_token.m_type) {
    namedProperty:
    case IDENT:
        wasIdent = true;
        FALLTHROUGH;
    case STRING: {
        const Identifier* ident = m_token.m_data.ident;
        if (complete || (wasIdent && (*ident == m_vm->propertyNames->get || *ident == m_vm->propertyNames->set)))
            nextExpectIdentifier(LexerFlagsIgnoreReservedWords);
        else
            nextExpectIdentifier(LexerFlagsIgnoreReservedWords | TreeBuilder::DontBuildKeywords);
        
        if (match(COLON)) {
            next();
            TreeExpression node = parseAssignmentExpression(context);
            failIfFalse(node, "Cannot parse expression for property declaration");
            return context.createProperty(ident, node, PropertyNode::Constant, complete);
        }
        failIfFalse(wasIdent, "Expected an identifier as property name");
        const Identifier* accessorName = 0;
        TreeFormalParameterList parameters = 0;
        TreeFunctionBody body = 0;
        unsigned openBraceOffset = 0;
        unsigned closeBraceOffset = 0;
        int bodyStartLine = 0;
        unsigned bodyStartColumn = 0;
        PropertyNode::Type type;
        if (*ident == m_vm->propertyNames->get)
            type = PropertyNode::Getter;
        else if (*ident == m_vm->propertyNames->set)
            type = PropertyNode::Setter;
        else
            failWithMessage("Expected a ':' following the property name '", ident->impl(), "'");
        const Identifier* stringPropertyName = 0;
        double numericPropertyName = 0;
        if (m_token.m_type == IDENT || m_token.m_type == STRING)
            stringPropertyName = m_token.m_data.ident;
        else if (m_token.m_type == NUMBER)
            numericPropertyName = m_token.m_data.doubleValue;
        else
            failDueToUnexpectedToken();
        JSTokenLocation location(tokenLocation());
        next();
        if (type == PropertyNode::Getter) {
            failIfFalse(match(OPENPAREN), "Expected a parameter list for getter definition");
            failIfFalse((parseFunctionInfo(context, FunctionNoRequirements, GetterMode, false, accessorName, parameters, body, openBraceOffset, closeBraceOffset, bodyStartLine, bodyStartColumn)), "Cannot parse getter definition");
        } else {
            failIfFalse(match(OPENPAREN), "Expected a parameter list for setter definition");
            failIfFalse((parseFunctionInfo(context, FunctionNoRequirements, SetterMode, false, accessorName, parameters, body, openBraceOffset, closeBraceOffset, bodyStartLine, bodyStartColumn)), "Cannot parse setter definition");
        }
        if (stringPropertyName)
            return context.createGetterOrSetterProperty(location, type, complete, stringPropertyName, parameters, body, openBraceOffset, closeBraceOffset, bodyStartLine, m_lastTokenEndPosition.line, bodyStartColumn);
        return context.createGetterOrSetterProperty(const_cast<VM*>(m_vm), location, type, complete, numericPropertyName, parameters, body, openBraceOffset, closeBraceOffset, bodyStartLine, m_lastTokenEndPosition.line, bodyStartColumn);
    }
    case NUMBER: {
        double propertyName = m_token.m_data.doubleValue;
        next();
        consumeOrFail(COLON, "Expected ':' after property name");
        TreeExpression node = parseAssignmentExpression(context);
        failIfFalse(node, "Cannot parse expression for property declaration");
        return context.createProperty(const_cast<VM*>(m_vm), propertyName, node, PropertyNode::Constant, complete);
    }
    case OPENBRACKET: {
        next();
        auto propertyName = parseExpression(context);
        failIfFalse(propertyName, "Cannot parse computed property name");
        
        handleProductionOrFail(CLOSEBRACKET, "]", "end", "computed property name");
        consumeOrFail(COLON, "Expected ':' after property name");
        TreeExpression node = parseAssignmentExpression(context);
        failIfFalse(node, "Cannot parse expression for property declaration");
        return context.createProperty(const_cast<VM*>(m_vm), propertyName, node, PropertyNode::Constant, complete);
    }
    default:
        failIfFalse(m_token.m_type & KeywordTokenFlag, "Expected a property name");
        goto namedProperty;
    }
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseObjectLiteral(TreeBuilder& context)
{
    auto savePoint = createSavePoint();
    consumeOrFailWithFlags(OPENBRACE, TreeBuilder::DontBuildStrings, "Expected opening '{' at the start of an object literal");
    JSTokenLocation location(tokenLocation());

    int oldNonLHSCount = m_nonLHSCount;
    
    if (match(CLOSEBRACE)) {
        next();
        return context.createObjectLiteral(location);
    }
    
    TreeProperty property = parseProperty(context, false);
    failIfFalse(property, "Cannot parse object literal property");
    if (!m_syntaxAlreadyValidated && context.getType(property) != PropertyNode::Constant) {
        restoreSavePoint(savePoint);
        return parseStrictObjectLiteral(context);
    }
    TreePropertyList propertyList = context.createPropertyList(location, property);
    TreePropertyList tail = propertyList;
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        // allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939
        if (match(CLOSEBRACE))
            break;
        JSTokenLocation propertyLocation(tokenLocation());
        property = parseProperty(context, false);
        failIfFalse(property, "Cannot parse object literal property");
        if (!m_syntaxAlreadyValidated && context.getType(property) != PropertyNode::Constant) {
            restoreSavePoint(savePoint);
            return parseStrictObjectLiteral(context);
        }
        tail = context.createPropertyList(propertyLocation, property, tail);
    }

    location = tokenLocation();
    handleProductionOrFail(CLOSEBRACE, "}", "end", "object literal");
    
    m_nonLHSCount = oldNonLHSCount;
    
    return context.createObjectLiteral(location, propertyList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseStrictObjectLiteral(TreeBuilder& context)
{
    consumeOrFail(OPENBRACE, "Expected opening '{' at the start of an object literal");
    
    int oldNonLHSCount = m_nonLHSCount;

    JSTokenLocation location(tokenLocation());
    if (match(CLOSEBRACE)) {
        next();
        return context.createObjectLiteral(location);
    }
    
    TreeProperty property = parseProperty(context, true);
    failIfFalse(property, "Cannot parse object literal property");
    
    typedef HashMap<RefPtr<StringImpl>, unsigned, IdentifierRepHash> ObjectValidationMap;
    ObjectValidationMap objectValidator;
    // Add the first property
    if (!m_syntaxAlreadyValidated && context.getName(property))
        objectValidator.add(context.getName(property)->impl(), context.getType(property));
    
    TreePropertyList propertyList = context.createPropertyList(location, property);
    TreePropertyList tail = propertyList;
    while (match(COMMA)) {
        next();
        // allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939
        if (match(CLOSEBRACE))
            break;
        JSTokenLocation propertyLocation(tokenLocation());
        property = parseProperty(context, true);
        failIfFalse(property, "Cannot parse object literal property");
        if (!m_syntaxAlreadyValidated && context.getName(property)) {
            ObjectValidationMap::AddResult propertyEntry = objectValidator.add(context.getName(property)->impl(), context.getType(property));
            if (!propertyEntry.isNewEntry) {
                semanticFailIfTrue(propertyEntry.iterator->value == PropertyNode::Constant, "Attempted to redefine property '", propertyEntry.iterator->key.get(), "'");
                semanticFailIfTrue(context.getType(property) == PropertyNode::Constant, "Attempted to redefine property '", propertyEntry.iterator->key.get(), "'");
                semanticFailIfTrue(context.getType(property) & propertyEntry.iterator->value, "Attempted to redefine property '", propertyEntry.iterator->key.get(), "'");
                propertyEntry.iterator->value |= context.getType(property);
            }
        }
        tail = context.createPropertyList(propertyLocation, property, tail);
    }

    location = tokenLocation();
    handleProductionOrFail(CLOSEBRACE, "}", "end", "object literal");

    m_nonLHSCount = oldNonLHSCount;

    return context.createObjectLiteral(location, propertyList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseArrayLiteral(TreeBuilder& context)
{
    consumeOrFailWithFlags(OPENBRACKET, TreeBuilder::DontBuildStrings, "Expected an opening '[' at the beginning of an array literal");
    
    int oldNonLHSCount = m_nonLHSCount;
    
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
        auto spreadExpr = parseAssignmentExpression(context);
        failIfFalse(spreadExpr, "Cannot parse subject of a spread operation");
        elem = context.createSpreadExpression(spreadLocation, spreadExpr, start, divot, m_lastTokenEndPosition);
    } else
        elem = parseAssignmentExpression(context);
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
            TreeExpression elem = parseAssignmentExpression(context);
            failIfFalse(elem, "Cannot parse subject of a spread operation");
            auto spread = context.createSpreadExpression(spreadLocation, elem, start, divot, m_lastTokenEndPosition);
            tail = context.createElementList(tail, elisions, spread);
            continue;
        }
        TreeExpression elem = parseAssignmentExpression(context);
        failIfFalse(elem, "Cannot parse array literal element");
        tail = context.createElementList(tail, elisions, elem);
    }

    JSTokenLocation location(tokenLocation());
    if (!consume(CLOSEBRACKET)) {
        failIfFalse(match(DOTDOTDOT), "Expected either a closing ']' or a ',' following an array element");
        semanticFail("The '...' operator should come before a target expression");
    }
    
    m_nonLHSCount = oldNonLHSCount;
    
    return context.createArray(location, elementList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parsePrimaryExpression(TreeBuilder& context)
{
    failIfStackOverflow();
    switch (m_token.m_type) {
    case OPENBRACE:
        if (strictMode())
            return parseStrictObjectLiteral(context);
        return parseObjectLiteral(context);
    case OPENBRACKET:
        return parseArrayLiteral(context);
    case OPENPAREN: {
        next();
        int oldNonLHSCount = m_nonLHSCount;
        TreeExpression result = parseExpression(context);
        m_nonLHSCount = oldNonLHSCount;
        handleProductionOrFail(CLOSEPAREN, ")", "end", "compound expression");
        return result;
    }
    case THISTOKEN: {
        JSTokenLocation location(tokenLocation());
        next();
        return context.thisExpr(location);
    }
    case IDENT: {
        JSTextPosition start = tokenStartPosition();
        const Identifier* ident = m_token.m_data.ident;
        JSTokenLocation location(tokenLocation());
        next();
        currentScope()->useVariable(ident, m_vm->propertyNames->eval == *ident);
        m_lastIdentifier = ident;
        return context.createResolve(location, ident, start);
    }
    case STRING: {
        const Identifier* ident = m_token.m_data.ident;
        JSTokenLocation location(tokenLocation());
        next();
        return context.createString(location, ident);
    }
    case NUMBER: {
        double d = m_token.m_data.doubleValue;
        JSTokenLocation location(tokenLocation());
        next();
        return context.createNumberExpr(location, d);
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
        const Identifier* pattern;
        const Identifier* flags;
        if (match(DIVEQUAL))
            failIfFalse(m_lexer->scanRegExp(pattern, flags, '='), "Invalid regular expression");
        else
            failIfFalse(m_lexer->scanRegExp(pattern, flags), "Invalid regular expression");
        
        JSTextPosition start = tokenStartPosition();
        JSTokenLocation location(tokenLocation());
        next();
        TreeExpression re = context.createRegExp(location, *pattern, *flags, start);
        if (!re) {
            const char* yarrErrorMsg = Yarr::checkSyntax(pattern->string());
            regexFail(yarrErrorMsg);
        }
        return re;
    }
    default:
        failDueToUnexpectedToken();
    }
}

template <typename LexerType>
template <class TreeBuilder> TreeArguments Parser<LexerType>::parseArguments(TreeBuilder& context, SpreadMode mode)
{
    consumeOrFailWithFlags(OPENPAREN, TreeBuilder::DontBuildStrings, "Expected opening '(' at start of argument list");
    JSTokenLocation location(tokenLocation());
    if (match(CLOSEPAREN)) {
        next(TreeBuilder::DontBuildStrings);
        return context.createArguments();
    }
    if (match(DOTDOTDOT) && mode == AllowSpread) {
        JSTokenLocation spreadLocation(tokenLocation());
        auto start = m_token.m_startPosition;
        auto divot = m_token.m_endPosition;
        next();
        auto spreadExpr = parseAssignmentExpression(context);
        auto end = m_lastTokenEndPosition;
        if (!spreadExpr)
            failWithMessage("Cannot parse spread expression");
        if (!consume(CLOSEPAREN)) {
            if (match(COMMA))
                semanticFail("Spread operator may only be applied to the last argument passed to a function");
            handleProductionOrFail(CLOSEPAREN, ")", "end", "argument list");
        }
        auto spread = context.createSpreadExpression(spreadLocation, spreadExpr, start, divot, end);
        TreeArgumentsList argList = context.createArgumentsList(location, spread);
        return context.createArguments(argList);
    }
    TreeExpression firstArg = parseAssignmentExpression(context);
    failIfFalse(firstArg, "Cannot parse function argument");
    
    TreeArgumentsList argList = context.createArgumentsList(location, firstArg);
    TreeArgumentsList tail = argList;
    while (match(COMMA)) {
        JSTokenLocation argumentLocation(tokenLocation());
        next(TreeBuilder::DontBuildStrings);
        TreeExpression arg = parseAssignmentExpression(context);
        failIfFalse(arg, "Cannot parse function argument");
        tail = context.createArgumentsList(argumentLocation, tail, arg);
    }
    semanticFailIfTrue(match(DOTDOTDOT), "The '...' operator should come before the target expression");
    handleProductionOrFail(CLOSEPAREN, ")", "end", "argument list");
    return context.createArguments(argList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseMemberExpression(TreeBuilder& context)
{
    TreeExpression base = 0;
    JSTextPosition expressionStart = tokenStartPosition();
    int newCount = 0;
    JSTokenLocation location;
    while (match(NEW)) {
        next();
        newCount++;
    }
    
    if (match(FUNCTION)) {
        const Identifier* name = &m_vm->propertyNames->nullIdentifier;
        TreeFormalParameterList parameters = 0;
        TreeFunctionBody body = 0;
        unsigned openBraceOffset = 0;
        unsigned closeBraceOffset = 0;
        int bodyStartLine = 0;
        unsigned bodyStartColumn = 0;
        location = tokenLocation();
        next();
        failIfFalse((parseFunctionInfo(context, FunctionNoRequirements, FunctionMode, false, name, parameters, body, openBraceOffset, closeBraceOffset, bodyStartLine, bodyStartColumn)), "Cannot parse function expression");
        base = context.createFunctionExpr(location, name, body, parameters, openBraceOffset, closeBraceOffset, bodyStartLine, m_lastTokenEndPosition.line, bodyStartColumn);
    } else
        base = parsePrimaryExpression(context);
    
    failIfFalse(base, "Cannot parse base expression");
    while (true) {
        location = tokenLocation();
        switch (m_token.m_type) {
        case OPENBRACKET: {
            m_nonTrivialExpressionCount++;
            JSTextPosition expressionEnd = lastTokenEndPosition();
            next();
            int nonLHSCount = m_nonLHSCount;
            int initialAssignments = m_assignmentCount;
            TreeExpression property = parseExpression(context);
            failIfFalse(property, "Cannot parse subscript expression");
            base = context.createBracketAccess(location, base, property, initialAssignments != m_assignmentCount, expressionStart, expressionEnd, tokenEndPosition());
            handleProductionOrFail(CLOSEBRACKET, "]", "end", "subscript expression");
            m_nonLHSCount = nonLHSCount;
            break;
        }
        case OPENPAREN: {
            m_nonTrivialExpressionCount++;
            int nonLHSCount = m_nonLHSCount;
            if (newCount) {
                newCount--;
                JSTextPosition expressionEnd = lastTokenEndPosition();
                TreeArguments arguments = parseArguments(context, AllowSpread);
                failIfFalse(arguments, "Cannot parse call arguments");
                base = context.createNewExpr(location, base, arguments, expressionStart, expressionEnd, lastTokenEndPosition());
            } else {
                JSTextPosition expressionEnd = lastTokenEndPosition();
                TreeArguments arguments = parseArguments(context, AllowSpread);
                failIfFalse(arguments, "Cannot parse call arguments");
                base = context.makeFunctionCallNode(location, base, arguments, expressionStart, expressionEnd, lastTokenEndPosition());
            }
            m_nonLHSCount = nonLHSCount;
            break;
        }
        case DOT: {
            m_nonTrivialExpressionCount++;
            JSTextPosition expressionEnd = lastTokenEndPosition();
            nextExpectIdentifier(LexerFlagsIgnoreReservedWords | TreeBuilder::DontBuildKeywords);
            matchOrFail(IDENT, "Expected a property name after '.'");
            base = context.createDotAccess(location, base, m_token.m_data.ident, expressionStart, expressionEnd, tokenEndPosition());
            next();
            break;
        }
        default:
            goto endMemberExpression;
        }
    }
endMemberExpression:
    while (newCount--)
        base = context.createNewExpr(location, base, expressionStart, lastTokenEndPosition());
    return base;
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
    bool modifiesExpr = false;
    bool requiresLExpr = false;
    unsigned lastOperator = 0;
    while (isUnaryOp(m_token.m_type)) {
        if (strictMode()) {
            switch (m_token.m_type) {
            case PLUSPLUS:
            case MINUSMINUS:
            case AUTOPLUSPLUS:
            case AUTOMINUSMINUS:
                semanticFailIfTrue(requiresLExpr, "The ", operatorString(true, lastOperator), " operator requires a reference expression");
                modifiesExpr = true;
                requiresLExpr = true;
                break;
            case DELETETOKEN:
                semanticFailIfTrue(requiresLExpr, "The ", operatorString(true, lastOperator), " operator requires a reference expression");
                requiresLExpr = true;
                break;
            default:
                semanticFailIfTrue(requiresLExpr, "The ", operatorString(true, lastOperator), " operator requires a reference expression");
                break;
            }
        }
        lastOperator = m_token.m_type;
        m_nonLHSCount++;
        context.appendUnaryToken(tokenStackDepth, m_token.m_type, tokenStartPosition());
        next();
        m_nonTrivialExpressionCount++;
    }
    JSTextPosition subExprStart = tokenStartPosition();
    ASSERT(subExprStart.offset >= subExprStart.lineStartOffset);
    JSTokenLocation location(tokenLocation());
    TreeExpression expr = parseMemberExpression(context);
    if (!expr) {
        if (lastOperator)
            failWithMessage("Cannot parse subexpression of ", operatorString(true, lastOperator), "operator");
        failWithMessage("Cannot parse member expression");
    }
    bool isEvalOrArguments = false;
    if (strictMode() && !m_syntaxAlreadyValidated) {
        if (context.isResolve(expr))
            isEvalOrArguments = *m_lastIdentifier == m_vm->propertyNames->eval || *m_lastIdentifier == m_vm->propertyNames->arguments;
    }
    failIfTrueIfStrict(isEvalOrArguments && modifiesExpr, "Cannot modify '", m_lastIdentifier->impl(), "' in strict mode");
    switch (m_token.m_type) {
    case PLUSPLUS:
        m_nonTrivialExpressionCount++;
        m_nonLHSCount++;
        expr = context.makePostfixNode(location, expr, OpPlusPlus, subExprStart, lastTokenEndPosition(), tokenEndPosition());
        m_assignmentCount++;
        failIfTrueIfStrict(isEvalOrArguments, "Cannot modify '", m_lastIdentifier->impl(), "' in strict mode");
        semanticFailIfTrue(requiresLExpr, "The ", operatorString(false, lastOperator), " operator requires a reference expression");
        lastOperator = PLUSPLUS;
        next();
        break;
    case MINUSMINUS:
        m_nonTrivialExpressionCount++;
        m_nonLHSCount++;
        expr = context.makePostfixNode(location, expr, OpMinusMinus, subExprStart, lastTokenEndPosition(), tokenEndPosition());
        m_assignmentCount++;
        failIfTrueIfStrict(isEvalOrArguments, "'", m_lastIdentifier->impl(), "' cannot be modified in strict mode");
        semanticFailIfTrue(requiresLExpr, "The ", operatorString(false, lastOperator), " operator requires a reference expression");
        lastOperator = PLUSPLUS;
        next();
        break;
    default:
        break;
    }
    
    JSTextPosition end = lastTokenEndPosition();

    if (!TreeBuilder::CreatesAST && (m_syntaxAlreadyValidated || !strictMode()))
        return expr;

    location = tokenLocation();
    location.line = m_lexer->lastLineNumber();
    while (tokenStackDepth) {
        switch (context.unaryTokenStackLastType(tokenStackDepth)) {
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
            expr = context.makePrefixNode(location, expr, OpPlusPlus, context.unaryTokenStackLastStart(tokenStackDepth), subExprStart + 1, end);
            m_assignmentCount++;
            break;
        case MINUSMINUS:
        case AUTOMINUSMINUS:
            expr = context.makePrefixNode(location, expr, OpMinusMinus, context.unaryTokenStackLastStart(tokenStackDepth), subExprStart + 1, end);
            m_assignmentCount++;
            break;
        case TYPEOF:
            expr = context.makeTypeOfNode(location, expr);
            break;
        case VOIDTOKEN:
            expr = context.createVoid(location, expr);
            break;
        case DELETETOKEN:
            failIfTrueIfStrict(context.isResolve(expr), "Cannot delete unqualified property '", m_lastIdentifier->impl(), "' in strict mode");
            expr = context.makeDeleteNode(location, expr, context.unaryTokenStackLastStart(tokenStackDepth), end, end);
            break;
        default:
            // If we get here something has gone horribly horribly wrong
            CRASH();
        }
        subExprStart = context.unaryTokenStackLastStart(tokenStackDepth);
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
    case INVALID_IDENTIFIER_UNICODE_ESCAPE_ERRORTOK:
        out.print("Invalid unicode escape in identifier: '", getToken(), "'");
        return;
    case INVALID_NUMERIC_LITERAL_ERRORTOK:
        out.print("Invalid numeric literal: '", getToken(), "'");
        return;
    case INVALID_OCTAL_NUMBER_ERRORTOK:
        out.print("Invalid use of octal: '", getToken(), "'");
        return;
    case INVALID_STRING_LITERAL_ERRORTOK:
        out.print("Invalid string literal: '", getToken(), "'");
        return;
    case ERRORTOK:
        out.print("Unrecognized token '", getToken(), "'");
        return;
    case STRING:
        out.print("Unexpected string literal ", getToken());
        return;
    case NUMBER:
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
