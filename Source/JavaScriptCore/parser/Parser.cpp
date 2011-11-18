/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSGlobalData.h"
#include "Lexer.h"
#include "NodeInfo.h"
#include "SourceProvider.h"
#include "SourceProviderCacheItem.h"
#include <utility>
#include <wtf/HashFunctions.h>
#include <wtf/OwnPtr.h>
#include <wtf/WTFThreadData.h>

using namespace std;

namespace JSC {

template <typename LexerType>
Parser<LexerType>::Parser(JSGlobalData* globalData, const SourceCode& source, FunctionParameters* parameters, JSParserStrictness strictness, JSParserMode parserMode)
    : m_globalData(globalData)
    , m_source(&source)
    , m_stack(globalData->stack())
    , m_error(false)
    , m_errorMessage("Parse error")
    , m_allowsIn(true)
    , m_lastLine(0)
    , m_lastTokenEnd(0)
    , m_assignmentCount(0)
    , m_nonLHSCount(0)
    , m_syntaxAlreadyValidated(source.provider()->isValid())
    , m_statementDepth(0)
    , m_nonTrivialExpressionCount(0)
    , m_lastIdentifier(0)
    , m_sourceElements(0)
{
    m_lexer = adoptPtr(new LexerType(globalData));
    m_arena = m_globalData->parserArena.get();
    m_lexer->setCode(source, m_arena);

    m_functionCache = source.provider()->cache();
    ScopeRef scope = pushScope();
    if (parserMode == JSParseFunctionCode)
        scope->setIsFunction();
    if (strictness == JSParseStrict)
        scope->setStrictMode();
    if (parameters) {
        for (unsigned i = 0; i < parameters->size(); i++)
            scope->declareParameter(&parameters->at(i));
    }
    next();
    m_lexer->setLastLineNumber(tokenLine());
}

template <typename LexerType>
Parser<LexerType>::~Parser()
{
}

template <typename LexerType>
UString Parser<LexerType>::parseInner()
{
    UString parseError = UString();
    
    unsigned oldFunctionCacheSize = m_functionCache ? m_functionCache->byteSize() : 0;
    ASTBuilder context(const_cast<JSGlobalData*>(m_globalData), const_cast<SourceCode*>(m_source));
    if (m_lexer->isReparsing())
        m_statementDepth--;
    ScopeRef scope = currentScope();
    SourceElements* sourceElements = parseSourceElements<CheckForStrictMode>(context);
    if (!sourceElements || !consume(EOFTOK))
        parseError = m_errorMessage;

    IdentifierSet capturedVariables;
    scope->getCapturedVariables(capturedVariables);
    CodeFeatures features = context.features();
    if (scope->strictMode())
        features |= StrictModeFeature;
    if (scope->shadowsArguments())
        features |= ShadowsArgumentsFeature;
    unsigned functionCacheSize = m_functionCache ? m_functionCache->byteSize() : 0;
    if (functionCacheSize != oldFunctionCacheSize)
        m_lexer->sourceProvider()->notifyCacheSizeChanged(functionCacheSize - oldFunctionCacheSize);

    didFinishParsing(sourceElements, context.varDeclarations(), context.funcDeclarations(), features,
                     m_lastLine, context.numConstants(), capturedVariables);

    return parseError;
}

template <typename LexerType>
void Parser<LexerType>::didFinishParsing(SourceElements* sourceElements, ParserArenaData<DeclarationStacks::VarStack>* varStack, 
                              ParserArenaData<DeclarationStacks::FunctionStack>* funcStack, CodeFeatures features, int lastLine, int numConstants, IdentifierSet& capturedVars)
{
    m_sourceElements = sourceElements;
    m_varDeclarations = varStack;
    m_funcDeclarations = funcStack;
    m_capturedVariables.swap(capturedVars);
    m_features = features;
    m_lastLine = lastLine;
    m_numConstants = numConstants;
}

template <typename LexerType>
bool Parser<LexerType>::allowAutomaticSemicolon()
{
    return match(CLOSEBRACE) || match(EOFTOK) || m_lexer->prevTerminator();
}

template <typename LexerType>
template <SourceElementsMode mode, class TreeBuilder> TreeSourceElements Parser<LexerType>::parseSourceElements(TreeBuilder& context)
{
    const unsigned lengthOfUseStrictLiteral = 12; // "use strict".length
    TreeSourceElements sourceElements = context.createSourceElements();
    bool seenNonDirective = false;
    const Identifier* directive = 0;
    unsigned directiveLiteralLength = 0;
    unsigned startOffset = m_token.m_info.startOffset;
    unsigned oldLastLineNumber = m_lexer->lastLineNumber();
    unsigned oldLineNumber = m_lexer->lineNumber();
    bool hasSetStrict = false;
    while (TreeStatement statement = parseStatement(context, directive, &directiveLiteralLength)) {
        if (mode == CheckForStrictMode && !seenNonDirective) {
            if (directive) {
                // "use strict" must be the exact literal without escape sequences or line continuation.
                if (!hasSetStrict && directiveLiteralLength == lengthOfUseStrictLiteral && m_globalData->propertyNames->useStrictIdentifier == *directive) {
                    setStrictMode();
                    hasSetStrict = true;
                    failIfFalse(isValidStrictMode());
                    m_lexer->setOffset(startOffset);
                    next();
                    m_lexer->setLastLineNumber(oldLastLineNumber);
                    m_lexer->setLineNumber(oldLineNumber);
                    failIfTrue(m_error);
                    continue;
                }
            } else
                seenNonDirective = true;
        }
        context.appendStatement(sourceElements, statement);
    }
    
    if (m_error)
        fail();
    return sourceElements;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseVarDeclaration(TreeBuilder& context)
{
    ASSERT(match(VAR));
    int start = tokenLine();
    int end = 0;
    int scratch;
    const Identifier* scratch1 = 0;
    TreeExpression scratch2 = 0;
    int scratch3 = 0;
    TreeExpression varDecls = parseVarDeclarationList(context, scratch, scratch1, scratch2, scratch3, scratch3, scratch3);
    failIfTrue(m_error);
    failIfFalse(autoSemiColon());
    
    return context.createVarStatement(m_lexer->lastLineNumber(), varDecls, start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseConstDeclaration(TreeBuilder& context)
{
    ASSERT(match(CONSTTOKEN));
    int start = tokenLine();
    int end = 0;
    TreeConstDeclList constDecls = parseConstDeclarationList(context);
    failIfTrue(m_error);
    failIfFalse(autoSemiColon());
    
    return context.createConstStatement(m_lexer->lastLineNumber(), constDecls, start, end);
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
    failIfFalse(statement);
    int endLine = tokenLine();
    consumeOrFail(WHILE);
    consumeOrFail(OPENPAREN);
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr);
    consumeOrFail(CLOSEPAREN);
    if (match(SEMICOLON))
        next(); // Always performs automatic semicolon insertion.
    return context.createDoWhileStatement(m_lexer->lastLineNumber(), statement, expr, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseWhileStatement(TreeBuilder& context)
{
    ASSERT(match(WHILE));
    int startLine = tokenLine();
    next();
    consumeOrFail(OPENPAREN);
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr);
    int endLine = tokenLine();
    consumeOrFail(CLOSEPAREN);
    const Identifier* unused = 0;
    startLoop();
    TreeStatement statement = parseStatement(context, unused);
    endLoop();
    failIfFalse(statement);
    return context.createWhileStatement(m_lexer->lastLineNumber(), expr, statement, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseVarDeclarationList(TreeBuilder& context, int& declarations, const Identifier*& lastIdent, TreeExpression& lastInitializer, int& identStart, int& initStart, int& initEnd)
{
    TreeExpression varDecls = 0;
    do {
        declarations++;
        next();
        matchOrFail(IDENT);
        
        int varStart = tokenStart();
        identStart = varStart;
        const Identifier* name = m_token.m_data.ident;
        lastIdent = name;
        next();
        bool hasInitializer = match(EQUAL);
        failIfFalseIfStrictWithNameAndMessage(declareVariable(name), "Cannot declare a variable named", name->impl(), "in strict mode.");
        context.addVar(name, (hasInitializer || (!m_allowsIn && match(INTOKEN))) ? DeclarationStacks::HasInitializer : 0);
        if (hasInitializer) {
            int varDivot = tokenStart() + 1;
            initStart = tokenStart();
            next(TreeBuilder::DontBuildStrings); // consume '='
            int initialAssignments = m_assignmentCount;
            TreeExpression initializer = parseAssignmentExpression(context);
            initEnd = lastTokenEnd();
            lastInitializer = initializer;
            failIfFalse(initializer);
            
            TreeExpression node = context.createAssignResolve(m_lexer->lastLineNumber(), *name, initializer, initialAssignments != m_assignmentCount, varStart, varDivot, lastTokenEnd());
            if (!varDecls)
                varDecls = node;
            else
                varDecls = context.combineCommaNodes(m_lexer->lastLineNumber(), varDecls, node);
        }
    } while (match(COMMA));
    return varDecls;
}

template <typename LexerType>
template <class TreeBuilder> TreeConstDeclList Parser<LexerType>::parseConstDeclarationList(TreeBuilder& context)
{
    failIfTrue(strictMode());
    TreeConstDeclList constDecls = 0;
    TreeConstDeclList tail = 0;
    do {
        next();
        matchOrFail(IDENT);
        const Identifier* name = m_token.m_data.ident;
        next();
        bool hasInitializer = match(EQUAL);
        declareVariable(name);
        context.addVar(name, DeclarationStacks::IsConstant | (hasInitializer ? DeclarationStacks::HasInitializer : 0));
        TreeExpression initializer = 0;
        if (hasInitializer) {
            next(TreeBuilder::DontBuildStrings); // consume '='
            initializer = parseAssignmentExpression(context);
        }
        tail = context.appendConstDecl(m_lexer->lastLineNumber(), tail, name, initializer);
        if (!constDecls)
            constDecls = tail;
    } while (match(COMMA));
    return constDecls;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseForStatement(TreeBuilder& context)
{
    ASSERT(match(FOR));
    int startLine = tokenLine();
    next();
    consumeOrFail(OPENPAREN);
    int nonLHSCount = m_nonLHSCount;
    int declarations = 0;
    int declsStart = 0;
    int declsEnd = 0;
    TreeExpression decls = 0;
    bool hasDeclaration = false;
    if (match(VAR)) {
        /*
         for (var IDENT in expression) statement
         for (var IDENT = expression in expression) statement
         for (var varDeclarationList; expressionOpt; expressionOpt)
         */
        hasDeclaration = true;
        const Identifier* forInTarget = 0;
        TreeExpression forInInitializer = 0;
        m_allowsIn = false;
        int initStart = 0;
        int initEnd = 0;
        decls = parseVarDeclarationList(context, declarations, forInTarget, forInInitializer, declsStart, initStart, initEnd);
        m_allowsIn = true;
        if (m_error)
            fail();
        
        // Remainder of a standard for loop is handled identically
        if (match(SEMICOLON))
            goto standardForLoop;
        
        failIfFalse(declarations == 1);
        
        // Handle for-in with var declaration
        int inLocation = tokenStart();
        consumeOrFail(INTOKEN);
        
        TreeExpression expr = parseExpression(context);
        failIfFalse(expr);
        int exprEnd = lastTokenEnd();
        
        int endLine = tokenLine();
        consumeOrFail(CLOSEPAREN);
        
        const Identifier* unused = 0;
        startLoop();
        TreeStatement statement = parseStatement(context, unused);
        endLoop();
        failIfFalse(statement);
        
        return context.createForInLoop(m_lexer->lastLineNumber(), forInTarget, forInInitializer, expr, statement, declsStart, inLocation, exprEnd, initStart, initEnd, startLine, endLine);
    }
    
    if (!match(SEMICOLON)) {
        m_allowsIn = false;
        declsStart = tokenStart();
        decls = parseExpression(context);
        declsEnd = lastTokenEnd();
        m_allowsIn = true;
        failIfFalse(decls);
    }
    
    if (match(SEMICOLON)) {
    standardForLoop:
        // Standard for loop
        next();
        TreeExpression condition = 0;
        
        if (!match(SEMICOLON)) {
            condition = parseExpression(context);
            failIfFalse(condition);
        }
        consumeOrFail(SEMICOLON);
        
        TreeExpression increment = 0;
        if (!match(CLOSEPAREN)) {
            increment = parseExpression(context);
            failIfFalse(increment);
        }
        int endLine = tokenLine();
        consumeOrFail(CLOSEPAREN);
        const Identifier* unused = 0;
        startLoop();
        TreeStatement statement = parseStatement(context, unused);
        endLoop();
        failIfFalse(statement);
        return context.createForLoop(m_lexer->lastLineNumber(), decls, condition, increment, statement, hasDeclaration, startLine, endLine);
    }
    
    // For-in loop
    failIfFalse(nonLHSCount == m_nonLHSCount);
    consumeOrFail(INTOKEN);
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr);
    int exprEnd = lastTokenEnd();
    int endLine = tokenLine();
    consumeOrFail(CLOSEPAREN);
    const Identifier* unused = 0;
    startLoop();
    TreeStatement statement = parseStatement(context, unused);
    endLoop();
    failIfFalse(statement);
    
    return context.createForInLoop(m_lexer->lastLineNumber(), decls, expr, statement, declsStart, declsEnd, exprEnd, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseBreakStatement(TreeBuilder& context)
{
    ASSERT(match(BREAK));
    int startCol = tokenStart();
    int endCol = tokenEnd();
    int startLine = tokenLine();
    int endLine = tokenLine();
    next();
    
    if (autoSemiColon()) {
        failIfFalseWithMessage(breakIsValid(), "'break' is only valid inside a switch or loop statement");
        return context.createBreakStatement(m_lexer->lastLineNumber(), startCol, endCol, startLine, endLine);
    }
    matchOrFail(IDENT);
    const Identifier* ident = m_token.m_data.ident;
    failIfFalseWithNameAndMessage(getLabel(ident), "Label", ident->impl(), "is not defined");
    endCol = tokenEnd();
    endLine = tokenLine();
    next();
    failIfFalse(autoSemiColon());
    return context.createBreakStatement(m_lexer->lastLineNumber(), ident, startCol, endCol, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseContinueStatement(TreeBuilder& context)
{
    ASSERT(match(CONTINUE));
    int startCol = tokenStart();
    int endCol = tokenEnd();
    int startLine = tokenLine();
    int endLine = tokenLine();
    next();
    
    if (autoSemiColon()) {
        failIfFalseWithMessage(continueIsValid(), "'continue' is only valid inside a loop statement");
        return context.createContinueStatement(m_lexer->lastLineNumber(), startCol, endCol, startLine, endLine);
    }
    matchOrFail(IDENT);
    const Identifier* ident = m_token.m_data.ident;
    ScopeLabelInfo* label = getLabel(ident);
    failIfFalseWithNameAndMessage(label, "Label", ident->impl(), "is not defined");
    failIfFalseWithMessage(label->m_isLoop, "'continue' is only valid inside a loop statement");
    endCol = tokenEnd();
    endLine = tokenLine();
    next();
    failIfFalse(autoSemiColon());
    return context.createContinueStatement(m_lexer->lastLineNumber(), ident, startCol, endCol, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseReturnStatement(TreeBuilder& context)
{
    ASSERT(match(RETURN));
    failIfFalse(currentScope()->isFunction());
    int startLine = tokenLine();
    int endLine = startLine;
    int start = tokenStart();
    int end = tokenEnd();
    next();
    // We do the auto semicolon check before attempting to parse an expression
    // as we need to ensure the a line break after the return correctly terminates
    // the statement
    if (match(SEMICOLON))
        endLine  = tokenLine();
    if (autoSemiColon())
        return context.createReturnStatement(m_lexer->lastLineNumber(), 0, start, end, startLine, endLine);
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr);
    end = lastTokenEnd();
    if (match(SEMICOLON))
        endLine  = tokenLine();
    failIfFalse(autoSemiColon());
    return context.createReturnStatement(m_lexer->lastLineNumber(), expr, start, end, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseThrowStatement(TreeBuilder& context)
{
    ASSERT(match(THROW));
    int eStart = tokenStart();
    int startLine = tokenLine();
    next();
    
    failIfTrue(autoSemiColon());
    
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr);
    int eEnd = lastTokenEnd();
    int endLine = tokenLine();
    failIfFalse(autoSemiColon());
    
    return context.createThrowStatement(m_lexer->lastLineNumber(), expr, eStart, eEnd, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseWithStatement(TreeBuilder& context)
{
    ASSERT(match(WITH));
    failIfTrueWithMessage(strictMode(), "'with' statements are not valid in strict mode");
    currentScope()->setNeedsFullActivation();
    int startLine = tokenLine();
    next();
    consumeOrFail(OPENPAREN);
    int start = tokenStart();
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr);
    int end = lastTokenEnd();
    
    int endLine = tokenLine();
    consumeOrFail(CLOSEPAREN);
    const Identifier* unused = 0;
    TreeStatement statement = parseStatement(context, unused);
    failIfFalse(statement);
    
    return context.createWithStatement(m_lexer->lastLineNumber(), expr, statement, start, end, startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseSwitchStatement(TreeBuilder& context)
{
    ASSERT(match(SWITCH));
    int startLine = tokenLine();
    next();
    consumeOrFail(OPENPAREN);
    TreeExpression expr = parseExpression(context);
    failIfFalse(expr);
    int endLine = tokenLine();
    consumeOrFail(CLOSEPAREN);
    consumeOrFail(OPENBRACE);
    startSwitch();
    TreeClauseList firstClauses = parseSwitchClauses(context);
    failIfTrue(m_error);
    
    TreeClause defaultClause = parseSwitchDefaultClause(context);
    failIfTrue(m_error);
    
    TreeClauseList secondClauses = parseSwitchClauses(context);
    failIfTrue(m_error);
    endSwitch();
    consumeOrFail(CLOSEBRACE);
    
    return context.createSwitchStatement(m_lexer->lastLineNumber(), expr, firstClauses, defaultClause, secondClauses, startLine, endLine);
    
}

template <typename LexerType>
template <class TreeBuilder> TreeClauseList Parser<LexerType>::parseSwitchClauses(TreeBuilder& context)
{
    if (!match(CASE))
        return 0;
    next();
    TreeExpression condition = parseExpression(context);
    failIfFalse(condition);
    consumeOrFail(COLON);
    TreeSourceElements statements = parseSourceElements<DontCheckForStrictMode>(context);
    failIfFalse(statements);
    TreeClause clause = context.createClause(condition, statements);
    TreeClauseList clauseList = context.createClauseList(clause);
    TreeClauseList tail = clauseList;
    
    while (match(CASE)) {
        next();
        TreeExpression condition = parseExpression(context);
        failIfFalse(condition);
        consumeOrFail(COLON);
        TreeSourceElements statements = parseSourceElements<DontCheckForStrictMode>(context);
        failIfFalse(statements);
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
    consumeOrFail(COLON);
    TreeSourceElements statements = parseSourceElements<DontCheckForStrictMode>(context);
    failIfFalse(statements);
    return context.createClause(0, statements);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseTryStatement(TreeBuilder& context)
{
    ASSERT(match(TRY));
    TreeStatement tryBlock = 0;
    const Identifier* ident = &m_globalData->propertyNames->nullIdentifier;
    bool catchHasEval = false;
    TreeStatement catchBlock = 0;
    TreeStatement finallyBlock = 0;
    int firstLine = tokenLine();
    next();
    matchOrFail(OPENBRACE);
    
    tryBlock = parseBlockStatement(context);
    failIfFalse(tryBlock);
    int lastLine = m_lastLine;
    
    if (match(CATCH)) {
        currentScope()->setNeedsFullActivation();
        next();
        consumeOrFail(OPENPAREN);
        matchOrFail(IDENT);
        ident = m_token.m_data.ident;
        next();
        AutoPopScopeRef catchScope(this, pushScope());
        failIfFalseIfStrictWithNameAndMessage(catchScope->declareVariable(ident), "Cannot declare a variable named", ident->impl(), "in strict mode");
        catchScope->preventNewDecls();
        consumeOrFail(CLOSEPAREN);
        matchOrFail(OPENBRACE);
        int initialEvalCount = context.evalCount();
        catchBlock = parseBlockStatement(context);
        failIfFalseWithMessage(catchBlock, "'try' must have a catch or finally block");
        catchHasEval = initialEvalCount != context.evalCount();
        failIfFalse(popScope(catchScope, TreeBuilder::NeedsFreeVariableInfo));
    }
    
    if (match(FINALLY)) {
        next();
        matchOrFail(OPENBRACE);
        finallyBlock = parseBlockStatement(context);
        failIfFalse(finallyBlock);
    }
    failIfFalse(catchBlock || finallyBlock);
    return context.createTryStatement(m_lexer->lastLineNumber(), tryBlock, ident, catchHasEval, catchBlock, finallyBlock, firstLine, lastLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseDebuggerStatement(TreeBuilder& context)
{
    ASSERT(match(DEBUGGER));
    int startLine = tokenLine();
    int endLine = startLine;
    next();
    if (match(SEMICOLON))
        startLine = tokenLine();
    failIfFalse(autoSemiColon());
    return context.createDebugger(m_lexer->lastLineNumber(), startLine, endLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseBlockStatement(TreeBuilder& context)
{
    ASSERT(match(OPENBRACE));
    int start = tokenLine();
    next();
    if (match(CLOSEBRACE)) {
        next();
        return context.createBlockStatement(m_lexer->lastLineNumber(), 0, start, m_lastLine);
    }
    TreeSourceElements subtree = parseSourceElements<DontCheckForStrictMode>(context);
    failIfFalse(subtree);
    matchOrFail(CLOSEBRACE);
    next();
    return context.createBlockStatement(m_lexer->lastLineNumber(), subtree, start, m_lastLine);
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
        failIfFalseIfStrictWithMessage(m_statementDepth == 1, "Functions cannot be declared in a nested block in strict mode");
        return parseFunctionDeclaration(context);
    case SEMICOLON:
        next();
        return context.createEmptyStatement(m_lexer->lastLineNumber());
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
            *directiveLiteralLength = m_token.m_info.endOffset - m_token.m_info.startOffset;
        nonTrivialExpressionCount = m_nonTrivialExpressionCount;
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
    matchOrFail(IDENT);
    failIfFalseIfStrictWithNameAndMessage(declareParameter(m_token.m_data.ident), "Cannot declare a parameter named", m_token.m_data.ident->impl(), " in strict mode");
    TreeFormalParameterList list = context.createFormalParameterList(*m_token.m_data.ident);
    TreeFormalParameterList tail = list;
    next();
    while (match(COMMA)) {
        next();
        matchOrFail(IDENT);
        const Identifier* ident = m_token.m_data.ident;
        failIfFalseIfStrictWithNameAndMessage(declareParameter(ident), "Cannot declare a parameter named", ident->impl(), "in strict mode");
        next();
        tail = context.createFormalParameterList(tail, *ident);
    }
    return list;
}

template <typename LexerType>
template <class TreeBuilder> TreeFunctionBody Parser<LexerType>::parseFunctionBody(TreeBuilder& context)
{
    if (match(CLOSEBRACE))
        return context.createFunctionBody(m_lexer->lastLineNumber(), strictMode());
    DepthManager statementDepth(&m_statementDepth);
    m_statementDepth = 0;
    typename TreeBuilder::FunctionBodyBuilder bodyBuilder(const_cast<JSGlobalData*>(m_globalData), m_lexer.get());
    failIfFalse(parseSourceElements<CheckForStrictMode>(bodyBuilder));
    return context.createFunctionBody(m_lexer->lastLineNumber(), strictMode());
}

template <typename LexerType>
template <FunctionRequirements requirements, bool nameIsInContainingScope, class TreeBuilder> bool Parser<LexerType>::parseFunctionInfo(TreeBuilder& context, const Identifier*& name, TreeFormalParameterList& parameters, TreeFunctionBody& body, int& openBracePos, int& closeBracePos, int& bodyStartLine)
{
    AutoPopScopeRef functionScope(this, pushScope());
    functionScope->setIsFunction();
    if (match(IDENT)) {
        name = m_token.m_data.ident;
        failIfTrueWithMessage(*name == m_globalData->propertyNames->underscoreProto, "Cannot name a function __proto__");
        next();
        if (!nameIsInContainingScope)
            failIfFalseIfStrict(functionScope->declareVariable(name));
    } else if (requirements == FunctionNeedsName)
        return false;
    consumeOrFail(OPENPAREN);
    if (!match(CLOSEPAREN)) {
        parameters = parseFormalParameters(context);
        failIfFalse(parameters);
    }
    consumeOrFail(CLOSEPAREN);
    matchOrFail(OPENBRACE);
    
    openBracePos = m_token.m_data.intValue;
    bodyStartLine = tokenLine();
    
    // If we know about this function already, we can use the cached info and skip the parser to the end of the function.
    if (const SourceProviderCacheItem* cachedInfo = TreeBuilder::CanUseFunctionCache ? findCachedFunctionInfo(openBracePos) : 0) {
        // If we're in a strict context, the cached function info must say it was strict too.
        ASSERT(!strictMode() || cachedInfo->strictMode);
        body = context.createFunctionBody(m_lexer->lastLineNumber(), cachedInfo->strictMode);
        
        functionScope->restoreFunctionInfo(cachedInfo);
        failIfFalse(popScope(functionScope, TreeBuilder::NeedsFreeVariableInfo));
        
        closeBracePos = cachedInfo->closeBracePos;
        m_token = cachedInfo->closeBraceToken();
        m_lexer->setOffset(m_token.m_info.endOffset);
        m_lexer->setLineNumber(m_token.m_info.line);
        
        next();
        return true;
    }
    
    next();
    
    body = parseFunctionBody(context);
    failIfFalse(body);
    if (functionScope->strictMode() && name) {
        failIfTrueWithNameAndMessage(m_globalData->propertyNames->arguments == *name, "Function name", name->impl(), "is not valid in strict mode");
        failIfTrueWithNameAndMessage(m_globalData->propertyNames->eval == *name, "Function name", name->impl(), "is not valid in strict mode");
    }
    closeBracePos = m_token.m_data.intValue;
    
    // Cache the tokenizer state and the function scope the first time the function is parsed.
    // Any future reparsing can then skip the function.
    static const int minimumFunctionLengthToCache = 64;
    OwnPtr<SourceProviderCacheItem> newInfo;
    int functionLength = closeBracePos - openBracePos;
    if (TreeBuilder::CanUseFunctionCache && m_functionCache && functionLength > minimumFunctionLengthToCache) {
        newInfo = adoptPtr(new SourceProviderCacheItem(m_token.m_info.line, closeBracePos));
        functionScope->saveFunctionInfo(newInfo.get());
    }
    
    failIfFalse(popScope(functionScope, TreeBuilder::NeedsFreeVariableInfo));
    matchOrFail(CLOSEBRACE);
    
    if (newInfo) {
        unsigned approximateByteSize = newInfo->approximateByteSize();
        m_functionCache->add(openBracePos, newInfo.release(), approximateByteSize);
    }
    
    next();
    return true;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseFunctionDeclaration(TreeBuilder& context)
{
    ASSERT(match(FUNCTION));
    next();
    const Identifier* name = 0;
    TreeFormalParameterList parameters = 0;
    TreeFunctionBody body = 0;
    int openBracePos = 0;
    int closeBracePos = 0;
    int bodyStartLine = 0;
    failIfFalse((parseFunctionInfo<FunctionNeedsName, true>(context, name, parameters, body, openBracePos, closeBracePos, bodyStartLine)));
    failIfFalse(name);
    failIfFalseIfStrict(declareVariable(name));
    return context.createFuncDeclStatement(m_lexer->lastLineNumber(), name, body, parameters, openBracePos, closeBracePos, bodyStartLine, m_lastLine);
}

struct LabelInfo {
    LabelInfo(const Identifier* ident, int start, int end)
    : m_ident(ident)
    , m_start(start)
    , m_end(end)
    {
    }
    
    const Identifier* m_ident;
    int m_start;
    int m_end;
};

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseExpressionOrLabelStatement(TreeBuilder& context)
{
    
    /* Expression and Label statements are ambiguous at LL(1), so we have a
     * special case that looks for a colon as the next character in the input.
     */
    Vector<LabelInfo> labels;
    
    do {
        int start = tokenStart();
        int startLine = tokenLine();
        if (!nextTokenIsColon()) {
            // If we hit this path we're making a expression statement, which
            // by definition can't make use of continue/break so we can just
            // ignore any labels we might have accumulated.
            TreeExpression expression = parseExpression(context);
            failIfFalse(expression);
            failIfFalse(autoSemiColon());
            return context.createExprStatement(m_lexer->lastLineNumber(), expression, startLine, m_lastLine);
        }
        const Identifier* ident = m_token.m_data.ident;
        int end = tokenEnd();
        next();
        consumeOrFail(COLON);
        if (!m_syntaxAlreadyValidated) {
            // This is O(N^2) over the current list of consecutive labels, but I
            // have never seen more than one label in a row in the real world.
            for (size_t i = 0; i < labels.size(); i++)
                failIfTrue(ident->impl() == labels[i].m_ident->impl());
            failIfTrue(getLabel(ident));
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
    failIfFalse(statement);
    for (size_t i = 0; i < labels.size(); i++) {
        const LabelInfo& info = labels[labels.size() - i - 1];
        statement = context.createLabelStatement(m_lexer->lastLineNumber(), info.m_ident, statement, info.m_start, info.m_end);
    }
    return statement;
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseExpressionStatement(TreeBuilder& context)
{
    int startLine = tokenLine();
    TreeExpression expression = parseExpression(context);
    failIfFalse(expression);
    failIfFalse(autoSemiColon());
    return context.createExprStatement(m_lexer->lastLineNumber(), expression, startLine, m_lastLine);
}

template <typename LexerType>
template <class TreeBuilder> TreeStatement Parser<LexerType>::parseIfStatement(TreeBuilder& context)
{
    ASSERT(match(IF));
    
    int start = tokenLine();
    next();
    
    consumeOrFail(OPENPAREN);
    
    TreeExpression condition = parseExpression(context);
    failIfFalse(condition);
    int end = tokenLine();
    consumeOrFail(CLOSEPAREN);
    
    const Identifier* unused = 0;
    TreeStatement trueBlock = parseStatement(context, unused);
    failIfFalse(trueBlock);
    
    if (!match(ELSE))
        return context.createIfStatement(m_lexer->lastLineNumber(), condition, trueBlock, start, end);
    
    Vector<TreeExpression> exprStack;
    Vector<pair<int, int> > posStack;
    Vector<TreeStatement> statementStack;
    bool trailingElse = false;
    do {
        next();
        if (!match(IF)) {
            const Identifier* unused = 0;
            TreeStatement block = parseStatement(context, unused);
            failIfFalse(block);
            statementStack.append(block);
            trailingElse = true;
            break;
        }
        int innerStart = tokenLine();
        next();
        
        consumeOrFail(OPENPAREN);
        
        TreeExpression innerCondition = parseExpression(context);
        failIfFalse(innerCondition);
        int innerEnd = tokenLine();
        consumeOrFail(CLOSEPAREN);
        const Identifier* unused = 0;
        TreeStatement innerTrueBlock = parseStatement(context, unused);
        failIfFalse(innerTrueBlock);     
        exprStack.append(innerCondition);
        posStack.append(make_pair(innerStart, innerEnd));
        statementStack.append(innerTrueBlock);
    } while (match(ELSE));
    
    if (!trailingElse) {
        TreeExpression condition = exprStack.last();
        exprStack.removeLast();
        TreeStatement trueBlock = statementStack.last();
        statementStack.removeLast();
        pair<int, int> pos = posStack.last();
        posStack.removeLast();
        statementStack.append(context.createIfStatement(m_lexer->lastLineNumber(), condition, trueBlock, pos.first, pos.second));
    }
    
    while (!exprStack.isEmpty()) {
        TreeExpression condition = exprStack.last();
        exprStack.removeLast();
        TreeStatement falseBlock = statementStack.last();
        statementStack.removeLast();
        TreeStatement trueBlock = statementStack.last();
        statementStack.removeLast();
        pair<int, int> pos = posStack.last();
        posStack.removeLast();
        statementStack.append(context.createIfStatement(m_lexer->lastLineNumber(), condition, trueBlock, falseBlock, pos.first, pos.second));
    }
    
    return context.createIfStatement(m_lexer->lastLineNumber(), condition, trueBlock, statementStack.last(), start, end);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseExpression(TreeBuilder& context)
{
    failIfStackOverflow();
    TreeExpression node = parseAssignmentExpression(context);
    failIfFalse(node);
    if (!match(COMMA))
        return node;
    next();
    m_nonTrivialExpressionCount++;
    m_nonLHSCount++;
    TreeExpression right = parseAssignmentExpression(context);
    failIfFalse(right);
    typename TreeBuilder::Comma commaNode = context.createCommaExpr(m_lexer->lastLineNumber(), node, right);
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        right = parseAssignmentExpression(context);
        failIfFalse(right);
        context.appendToComma(commaNode, right);
    }
    return commaNode;
}

template <typename LexerType>
template <typename TreeBuilder> TreeExpression Parser<LexerType>::parseAssignmentExpression(TreeBuilder& context)
{
    failIfStackOverflow();
    int start = tokenStart();
    int initialAssignmentCount = m_assignmentCount;
    int initialNonLHSCount = m_nonLHSCount;
    TreeExpression lhs = parseConditionalExpression(context);
    failIfFalse(lhs);
    if (initialNonLHSCount != m_nonLHSCount)
        return lhs;
    
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
        context.assignmentStackAppend(assignmentStack, lhs, start, tokenStart(), m_assignmentCount, op);
        start = tokenStart();
        m_assignmentCount++;
        next(TreeBuilder::DontBuildStrings);
        if (strictMode() && m_lastIdentifier && context.isResolve(lhs)) {
            failIfTrueIfStrictWithMessage(m_globalData->propertyNames->eval == *m_lastIdentifier, "'eval' cannot be modified in strict mode");
            failIfTrueIfStrictWithMessage(m_globalData->propertyNames->arguments == *m_lastIdentifier, "'arguments' cannot be modified in strict mode");
            declareWrite(m_lastIdentifier);
            m_lastIdentifier = 0;
        }
        lhs = parseConditionalExpression(context);
        failIfFalse(lhs);
        if (initialNonLHSCount != m_nonLHSCount)
            break;
    }
end:
    if (hadAssignment)
        m_nonLHSCount++;
    
    if (!TreeBuilder::CreatesAST)
        return lhs;
    
    while (assignmentStack)
        lhs = context.createAssignment(m_lexer->lastLineNumber(), assignmentStack, lhs, initialAssignmentCount, m_assignmentCount, lastTokenEnd());
    
    return lhs;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseConditionalExpression(TreeBuilder& context)
{
    TreeExpression cond = parseBinaryExpression(context);
    failIfFalse(cond);
    if (!match(QUESTION))
        return cond;
    m_nonTrivialExpressionCount++;
    m_nonLHSCount++;
    next(TreeBuilder::DontBuildStrings);
    TreeExpression lhs = parseAssignmentExpression(context);
    consumeOrFailWithFlags(COLON, TreeBuilder::DontBuildStrings);
    
    TreeExpression rhs = parseAssignmentExpression(context);
    failIfFalse(rhs);
    return context.createConditionalExpr(m_lexer->lastLineNumber(), cond, lhs, rhs);
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
    while (true) {
        int exprStart = tokenStart();
        int initialAssignments = m_assignmentCount;
        TreeExpression current = parseUnaryExpression(context);
        failIfFalse(current);
        
        context.appendBinaryExpressionInfo(operandStackDepth, current, exprStart, lastTokenEnd(), lastTokenEnd(), initialAssignments != m_assignmentCount);
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
            context.appendBinaryOperation(m_lexer->lastLineNumber(), operandStackDepth, operatorStackDepth, lhs, rhs);
            context.operatorStackPop(operatorStackDepth);
        }
        context.operatorStackAppend(operatorStackDepth, operatorToken, precedence);
    }
    while (operatorStackDepth) {
        ASSERT(operandStackDepth > 1);
        
        typename TreeBuilder::BinaryOperand rhs = context.getFromOperandStack(-1);
        typename TreeBuilder::BinaryOperand lhs = context.getFromOperandStack(-2);
        context.shrinkOperandStackBy(operandStackDepth, 2);
        context.appendBinaryOperation(m_lexer->lastLineNumber(), operandStackDepth, operatorStackDepth, lhs, rhs);
        context.operatorStackPop(operatorStackDepth);
    }
    return context.popOperandStack(operandStackDepth);
}

template <typename LexerType>
template <bool complete, class TreeBuilder> TreeProperty Parser<LexerType>::parseProperty(TreeBuilder& context)
{
    bool wasIdent = false;
    switch (m_token.m_type) {
    namedProperty:
    case IDENT:
        wasIdent = true;
    case STRING: {
        const Identifier* ident = m_token.m_data.ident;
        if (complete || (wasIdent && (*ident == m_globalData->propertyNames->get || *ident == m_globalData->propertyNames->set)))
            nextExpectIdentifier(LexerFlagsIgnoreReservedWords);
        else
            nextExpectIdentifier(LexerFlagsIgnoreReservedWords | TreeBuilder::DontBuildKeywords);
        
        if (match(COLON)) {
            next();
            TreeExpression node = parseAssignmentExpression(context);
            failIfFalse(node);
            return context.template createProperty<complete>(ident, node, PropertyNode::Constant);
        }
        failIfFalse(wasIdent);
        matchOrFail(IDENT);
        const Identifier* accessorName = 0;
        TreeFormalParameterList parameters = 0;
        TreeFunctionBody body = 0;
        int openBracePos = 0;
        int closeBracePos = 0;
        int bodyStartLine = 0;
        PropertyNode::Type type;
        if (*ident == m_globalData->propertyNames->get)
            type = PropertyNode::Getter;
        else if (*ident == m_globalData->propertyNames->set)
            type = PropertyNode::Setter;
        else
            fail();
        failIfFalse((parseFunctionInfo<FunctionNeedsName, false>(context, accessorName, parameters, body, openBracePos, closeBracePos, bodyStartLine)));
        return context.template createGetterOrSetterProperty<complete>(m_lexer->lastLineNumber(), type, accessorName, parameters, body, openBracePos, closeBracePos, bodyStartLine, m_lastLine);
    }
    case NUMBER: {
        double propertyName = m_token.m_data.doubleValue;
        next();
        consumeOrFail(COLON);
        TreeExpression node = parseAssignmentExpression(context);
        failIfFalse(node);
        return context.template createProperty<complete>(const_cast<JSGlobalData*>(m_globalData), propertyName, node, PropertyNode::Constant);
    }
    default:
        failIfFalse(m_token.m_type & KeywordTokenFlag);
        goto namedProperty;
    }
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseObjectLiteral(TreeBuilder& context)
{
    int startOffset = m_token.m_data.intValue;
    consumeOrFailWithFlags(OPENBRACE, TreeBuilder::DontBuildStrings);
    
    if (match(CLOSEBRACE)) {
        next();
        return context.createObjectLiteral(m_lexer->lastLineNumber());
    }
    
    TreeProperty property = parseProperty<false>(context);
    failIfFalse(property);
    if (!m_syntaxAlreadyValidated && context.getType(property) != PropertyNode::Constant) {
        m_lexer->setOffset(startOffset);
        next();
        return parseStrictObjectLiteral(context);
    }
    TreePropertyList propertyList = context.createPropertyList(m_lexer->lastLineNumber(), property);
    TreePropertyList tail = propertyList;
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        // allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939
        if (match(CLOSEBRACE))
            break;
        property = parseProperty<false>(context);
        failIfFalse(property);
        if (!m_syntaxAlreadyValidated && context.getType(property) != PropertyNode::Constant) {
            m_lexer->setOffset(startOffset);
            next();
            return parseStrictObjectLiteral(context);
        }
        tail = context.createPropertyList(m_lexer->lastLineNumber(), property, tail);
    }
    
    consumeOrFail(CLOSEBRACE);
    
    return context.createObjectLiteral(m_lexer->lastLineNumber(), propertyList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseStrictObjectLiteral(TreeBuilder& context)
{
    consumeOrFail(OPENBRACE);
    
    if (match(CLOSEBRACE)) {
        next();
        return context.createObjectLiteral(m_lexer->lastLineNumber());
    }
    
    TreeProperty property = parseProperty<true>(context);
    failIfFalse(property);
    
    typedef HashMap<RefPtr<StringImpl>, unsigned, IdentifierRepHash> ObjectValidationMap;
    ObjectValidationMap objectValidator;
    // Add the first property
    if (!m_syntaxAlreadyValidated)
        objectValidator.add(context.getName(property).impl(), context.getType(property));
    
    TreePropertyList propertyList = context.createPropertyList(m_lexer->lastLineNumber(), property);
    TreePropertyList tail = propertyList;
    while (match(COMMA)) {
        next();
        // allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939
        if (match(CLOSEBRACE))
            break;
        property = parseProperty<true>(context);
        failIfFalse(property);
        if (!m_syntaxAlreadyValidated) {
            std::pair<ObjectValidationMap::iterator, bool> propertyEntryIter = objectValidator.add(context.getName(property).impl(), context.getType(property));
            if (!propertyEntryIter.second) {
                failIfTrue(propertyEntryIter.first->second == PropertyNode::Constant);
                failIfTrue(context.getType(property) == PropertyNode::Constant);
                failIfTrue(context.getType(property) & propertyEntryIter.first->second);
                propertyEntryIter.first->second |= context.getType(property);
            }
        }
        tail = context.createPropertyList(m_lexer->lastLineNumber(), property, tail);
    }
    
    consumeOrFail(CLOSEBRACE);
    
    return context.createObjectLiteral(m_lexer->lastLineNumber(), propertyList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseArrayLiteral(TreeBuilder& context)
{
    consumeOrFailWithFlags(OPENBRACKET, TreeBuilder::DontBuildStrings);
    
    int elisions = 0;
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        elisions++;
    }
    if (match(CLOSEBRACKET)) {
        next(TreeBuilder::DontBuildStrings);
        return context.createArray(m_lexer->lastLineNumber(), elisions);
    }
    
    TreeExpression elem = parseAssignmentExpression(context);
    failIfFalse(elem);
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
            next(TreeBuilder::DontBuildStrings);
            return context.createArray(m_lexer->lastLineNumber(), elisions, elementList);
        }
        TreeExpression elem = parseAssignmentExpression(context);
        failIfFalse(elem);
        tail = context.createElementList(tail, elisions, elem);
    }
    
    consumeOrFail(CLOSEBRACKET);
    
    return context.createArray(m_lexer->lastLineNumber(), elementList);
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
        consumeOrFail(CLOSEPAREN);
        
        return result;
    }
    case THISTOKEN: {
        next();
        return context.thisExpr(m_lexer->lastLineNumber());
    }
    case IDENT: {
        int start = tokenStart();
        const Identifier* ident = m_token.m_data.ident;
        next();
        currentScope()->useVariable(ident, m_globalData->propertyNames->eval == *ident);
        m_lastIdentifier = ident;
        return context.createResolve(m_lexer->lastLineNumber(), ident, start);
    }
    case STRING: {
        const Identifier* ident = m_token.m_data.ident;
        next();
        return context.createString(m_lexer->lastLineNumber(), ident);
    }
    case NUMBER: {
        double d = m_token.m_data.doubleValue;
        next();
        return context.createNumberExpr(m_lexer->lastLineNumber(), d);
    }
    case NULLTOKEN: {
        next();
        return context.createNull(m_lexer->lastLineNumber());
    }
    case TRUETOKEN: {
        next();
        return context.createBoolean(m_lexer->lastLineNumber(), true);
    }
    case FALSETOKEN: {
        next();
        return context.createBoolean(m_lexer->lastLineNumber(), false);
    }
    case DIVEQUAL:
    case DIVIDE: {
        /* regexp */
        const Identifier* pattern;
        const Identifier* flags;
        if (match(DIVEQUAL))
            failIfFalse(m_lexer->scanRegExp(pattern, flags, '='));
        else
            failIfFalse(m_lexer->scanRegExp(pattern, flags));
        
        int start = tokenStart();
        next();
        TreeExpression re = context.createRegExp(m_lexer->lastLineNumber(), *pattern, *flags, start);
        if (!re) {
            const char* yarrErrorMsg = Yarr::checkSyntax(pattern->ustring());
            ASSERT(!m_errorMessage.isNull());
            failWithMessage(yarrErrorMsg);
        }
        return re;
    }
    default:
        fail();
    }
}

template <typename LexerType>
template <class TreeBuilder> TreeArguments Parser<LexerType>::parseArguments(TreeBuilder& context)
{
    consumeOrFailWithFlags(OPENPAREN, TreeBuilder::DontBuildStrings);
    if (match(CLOSEPAREN)) {
        next(TreeBuilder::DontBuildStrings);
        return context.createArguments();
    }
    TreeExpression firstArg = parseAssignmentExpression(context);
    failIfFalse(firstArg);
    
    TreeArgumentsList argList = context.createArgumentsList(m_lexer->lastLineNumber(), firstArg);
    TreeArgumentsList tail = argList;
    while (match(COMMA)) {
        next(TreeBuilder::DontBuildStrings);
        TreeExpression arg = parseAssignmentExpression(context);
        failIfFalse(arg);
        tail = context.createArgumentsList(m_lexer->lastLineNumber(), tail, arg);
    }
    consumeOrFail(CLOSEPAREN);
    return context.createArguments(argList);
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseMemberExpression(TreeBuilder& context)
{
    TreeExpression base = 0;
    int start = tokenStart();
    int expressionStart = start;
    int newCount = 0;
    while (match(NEW)) {
        next();
        newCount++;
    }
    
    if (match(FUNCTION)) {
        const Identifier* name = &m_globalData->propertyNames->nullIdentifier;
        TreeFormalParameterList parameters = 0;
        TreeFunctionBody body = 0;
        int openBracePos = 0;
        int closeBracePos = 0;
        int bodyStartLine = 0;
        next();
        failIfFalse((parseFunctionInfo<FunctionNoRequirements, false>(context, name, parameters, body, openBracePos, closeBracePos, bodyStartLine)));
        base = context.createFunctionExpr(m_lexer->lastLineNumber(), name, body, parameters, openBracePos, closeBracePos, bodyStartLine, m_lastLine);
    } else
        base = parsePrimaryExpression(context);
    
    failIfFalse(base);
    while (true) {
        switch (m_token.m_type) {
        case OPENBRACKET: {
            m_nonTrivialExpressionCount++;
            int expressionEnd = lastTokenEnd();
            next();
            int nonLHSCount = m_nonLHSCount;
            int initialAssignments = m_assignmentCount;
            TreeExpression property = parseExpression(context);
            failIfFalse(property);
            base = context.createBracketAccess(m_lexer->lastLineNumber(), base, property, initialAssignments != m_assignmentCount, expressionStart, expressionEnd, tokenEnd());
            consumeOrFail(CLOSEBRACKET);
            m_nonLHSCount = nonLHSCount;
            break;
        }
        case OPENPAREN: {
            m_nonTrivialExpressionCount++;
            if (newCount) {
                newCount--;
                if (match(OPENPAREN)) {
                    int exprEnd = lastTokenEnd();
                    TreeArguments arguments = parseArguments(context);
                    failIfFalse(arguments);
                    base = context.createNewExpr(m_lexer->lastLineNumber(), base, arguments, start, exprEnd, lastTokenEnd());
                } else
                    base = context.createNewExpr(m_lexer->lastLineNumber(), base, start, lastTokenEnd());               
            } else {
                int nonLHSCount = m_nonLHSCount;
                int expressionEnd = lastTokenEnd();
                TreeArguments arguments = parseArguments(context);
                failIfFalse(arguments);
                base = context.makeFunctionCallNode(m_lexer->lastLineNumber(), base, arguments, expressionStart, expressionEnd, lastTokenEnd());
                m_nonLHSCount = nonLHSCount;
            }
            break;
        }
        case DOT: {
            m_nonTrivialExpressionCount++;
            int expressionEnd = lastTokenEnd();
            nextExpectIdentifier(LexerFlagsIgnoreReservedWords | TreeBuilder::DontBuildKeywords);
            matchOrFail(IDENT);
            base = context.createDotAccess(m_lexer->lastLineNumber(), base, m_token.m_data.ident, expressionStart, expressionEnd, tokenEnd());
            next();
            break;
        }
        default:
            goto endMemberExpression;
        }
    }
endMemberExpression:
    while (newCount--)
        base = context.createNewExpr(m_lexer->lastLineNumber(), base, start, lastTokenEnd());
    return base;
}

template <typename LexerType>
template <class TreeBuilder> TreeExpression Parser<LexerType>::parseUnaryExpression(TreeBuilder& context)
{
    typename TreeBuilder::UnaryExprContext unaryExprContext(context);
    AllowInOverride allowInOverride(this);
    int tokenStackDepth = 0;
    bool modifiesExpr = false;
    bool requiresLExpr = false;
    while (isUnaryOp(m_token.m_type)) {
        if (strictMode()) {
            switch (m_token.m_type) {
            case PLUSPLUS:
            case MINUSMINUS:
            case AUTOPLUSPLUS:
            case AUTOMINUSMINUS:
                failIfTrue(requiresLExpr);
                modifiesExpr = true;
                requiresLExpr = true;
                break;
            case DELETETOKEN:
                failIfTrue(requiresLExpr);
                requiresLExpr = true;
                break;
            default:
                failIfTrue(requiresLExpr);
                break;
            }
        }
        m_nonLHSCount++;
        context.appendUnaryToken(tokenStackDepth, m_token.m_type, tokenStart());
        next();
        m_nonTrivialExpressionCount++;
    }
    int subExprStart = tokenStart();
    TreeExpression expr = parseMemberExpression(context);
    failIfFalse(expr);
    bool isEvalOrArguments = false;
    if (strictMode() && !m_syntaxAlreadyValidated) {
        if (context.isResolve(expr))
            isEvalOrArguments = *m_lastIdentifier == m_globalData->propertyNames->eval || *m_lastIdentifier == m_globalData->propertyNames->arguments;
    }
    failIfTrueIfStrictWithNameAndMessage(isEvalOrArguments && modifiesExpr, "'", m_lastIdentifier->impl(), "' cannot be modified in strict mode");
    switch (m_token.m_type) {
    case PLUSPLUS:
        m_nonTrivialExpressionCount++;
        m_nonLHSCount++;
        expr = context.makePostfixNode(m_lexer->lastLineNumber(), expr, OpPlusPlus, subExprStart, lastTokenEnd(), tokenEnd());
        m_assignmentCount++;
        failIfTrueIfStrictWithNameAndMessage(isEvalOrArguments, "'", m_lastIdentifier->impl(), "' cannot be modified in strict mode");
        failIfTrueIfStrict(requiresLExpr);
        next();
        break;
    case MINUSMINUS:
        m_nonTrivialExpressionCount++;
        m_nonLHSCount++;
        expr = context.makePostfixNode(m_lexer->lastLineNumber(), expr, OpMinusMinus, subExprStart, lastTokenEnd(), tokenEnd());
        m_assignmentCount++;
        failIfTrueIfStrictWithNameAndMessage(isEvalOrArguments, "'", m_lastIdentifier->impl(), "' cannot be modified in strict mode");
        failIfTrueIfStrict(requiresLExpr);
        next();
        break;
    default:
        break;
    }
    
    int end = lastTokenEnd();
    
    if (!TreeBuilder::CreatesAST && (m_syntaxAlreadyValidated || !strictMode()))
        return expr;
    
    while (tokenStackDepth) {
        switch (context.unaryTokenStackLastType(tokenStackDepth)) {
        case EXCLAMATION:
            expr = context.createLogicalNot(m_lexer->lastLineNumber(), expr);
            break;
        case TILDE:
            expr = context.makeBitwiseNotNode(m_lexer->lastLineNumber(), expr);
            break;
        case MINUS:
            expr = context.makeNegateNode(m_lexer->lastLineNumber(), expr);
            break;
        case PLUS:
            expr = context.createUnaryPlus(m_lexer->lastLineNumber(), expr);
            break;
        case PLUSPLUS:
        case AUTOPLUSPLUS:
            expr = context.makePrefixNode(m_lexer->lastLineNumber(), expr, OpPlusPlus, context.unaryTokenStackLastStart(tokenStackDepth), subExprStart + 1, end);
            m_assignmentCount++;
            break;
        case MINUSMINUS:
        case AUTOMINUSMINUS:
            expr = context.makePrefixNode(m_lexer->lastLineNumber(), expr, OpMinusMinus, context.unaryTokenStackLastStart(tokenStackDepth), subExprStart + 1, end);
            m_assignmentCount++;
            break;
        case TYPEOF:
            expr = context.makeTypeOfNode(m_lexer->lastLineNumber(), expr);
            break;
        case VOIDTOKEN:
            expr = context.createVoid(m_lexer->lastLineNumber(), expr);
            break;
        case DELETETOKEN:
            failIfTrueIfStrictWithNameAndMessage(context.isResolve(expr), "Cannot delete unqualified property", m_lastIdentifier->impl(), "in strict mode");
            expr = context.makeDeleteNode(m_lexer->lastLineNumber(), expr, context.unaryTokenStackLastStart(tokenStackDepth), end, end);
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

// Instantiate the two flavors of Parser we need instead of putting most of this file in Parser.h
template class Parser< Lexer<LChar> >;
template class Parser< Lexer<UChar> >;

} // namespace JSC
