/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011, 2013 Apple Inc. All rights reserved.
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

#ifndef Parser_h
#define Parser_h

#include "Debugger.h"
#include "ExceptionHelpers.h"
#include "Executable.h"
#include "JSGlobalObject.h"
#include "Lexer.h"
#include "Nodes.h"
#include "ParserArena.h"
#include "ParserError.h"
#include "ParserTokens.h"
#include "SourceProvider.h"
#include "SourceProviderCache.h"
#include "SourceProviderCacheItem.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
namespace JSC {
struct Scope;
}

namespace WTF {
template <> struct VectorTraits<JSC::Scope> : SimpleClassVectorTraits {
    static const bool canInitializeWithMemset = false; // Not all Scope data members initialize to 0.
};
}

namespace JSC {

class ExecState;
class FunctionBodyNode;
class FunctionParameters;
class Identifier;
class VM;
class ProgramNode;
class SourceCode;

// Macros to make the more common TreeBuilder types a little less verbose
#define TreeStatement typename TreeBuilder::Statement
#define TreeExpression typename TreeBuilder::Expression
#define TreeFormalParameterList typename TreeBuilder::FormalParameterList
#define TreeSourceElements typename TreeBuilder::SourceElements
#define TreeClause typename TreeBuilder::Clause
#define TreeClauseList typename TreeBuilder::ClauseList
#define TreeConstDeclList typename TreeBuilder::ConstDeclList
#define TreeArguments typename TreeBuilder::Arguments
#define TreeArgumentsList typename TreeBuilder::ArgumentsList
#define TreeFunctionBody typename TreeBuilder::FunctionBody
#define TreeProperty typename TreeBuilder::Property
#define TreePropertyList typename TreeBuilder::PropertyList
#define TreeDeconstructionPattern typename TreeBuilder::DeconstructionPattern

COMPILE_ASSERT(LastUntaggedToken < 64, LessThan64UntaggedTokens);

enum SourceElementsMode { CheckForStrictMode, DontCheckForStrictMode };
enum FunctionRequirements { FunctionNoRequirements, FunctionNeedsName };
enum FunctionParseMode { FunctionMode, GetterMode, SetterMode };
enum DeconstructionKind {
    DeconstructToVariables,
    DeconstructToParameters,
    DeconstructToExpressions
};

template <typename T> inline bool isEvalNode() { return false; }
template <> inline bool isEvalNode<EvalNode>() { return true; }

struct DepthManager {
    DepthManager(int* depth)
        : m_originalDepth(*depth)
        , m_depth(depth)
    {
    }

    ~DepthManager()
    {
        *m_depth = m_originalDepth;
    }

private:
    int m_originalDepth;
    int* m_depth;
};

struct ScopeLabelInfo {
    ScopeLabelInfo(StringImpl* ident, bool isLoop)
        : m_ident(ident)
        , m_isLoop(isLoop)
    {
    }

    StringImpl* m_ident;
    bool m_isLoop;
};

struct Scope {
    Scope(const VM* vm, bool isFunction, bool strictMode)
        : m_vm(vm)
        , m_shadowsArguments(false)
        , m_usesEval(false)
        , m_needsFullActivation(false)
        , m_allowsNewDecls(true)
        , m_strictMode(strictMode)
        , m_isFunction(isFunction)
        , m_isFunctionBoundary(false)
        , m_isValidStrictMode(true)
        , m_loopDepth(0)
        , m_switchDepth(0)
    {
    }

    Scope(const Scope& rhs)
        : m_vm(rhs.m_vm)
        , m_shadowsArguments(rhs.m_shadowsArguments)
        , m_usesEval(rhs.m_usesEval)
        , m_needsFullActivation(rhs.m_needsFullActivation)
        , m_allowsNewDecls(rhs.m_allowsNewDecls)
        , m_strictMode(rhs.m_strictMode)
        , m_isFunction(rhs.m_isFunction)
        , m_isFunctionBoundary(rhs.m_isFunctionBoundary)
        , m_isValidStrictMode(rhs.m_isValidStrictMode)
        , m_loopDepth(rhs.m_loopDepth)
        , m_switchDepth(rhs.m_switchDepth)
    {
        if (rhs.m_labels) {
            m_labels = adoptPtr(new LabelStack);

            typedef LabelStack::const_iterator iterator;
            iterator end = rhs.m_labels->end();
            for (iterator it = rhs.m_labels->begin(); it != end; ++it)
                m_labels->append(ScopeLabelInfo(it->m_ident, it->m_isLoop));
        }
    }

    void startSwitch() { m_switchDepth++; }
    void endSwitch() { m_switchDepth--; }
    void startLoop() { m_loopDepth++; }
    void endLoop() { ASSERT(m_loopDepth); m_loopDepth--; }
    bool inLoop() { return !!m_loopDepth; }
    bool breakIsValid() { return m_loopDepth || m_switchDepth; }
    bool continueIsValid() { return m_loopDepth; }

    void pushLabel(const Identifier* label, bool isLoop)
    {
        if (!m_labels)
            m_labels = adoptPtr(new LabelStack);
        m_labels->append(ScopeLabelInfo(label->impl(), isLoop));
    }

    void popLabel()
    {
        ASSERT(m_labels);
        ASSERT(m_labels->size());
        m_labels->removeLast();
    }

    ScopeLabelInfo* getLabel(const Identifier* label)
    {
        if (!m_labels)
            return 0;
        for (int i = m_labels->size(); i > 0; i--) {
            if (m_labels->at(i - 1).m_ident == label->impl())
                return &m_labels->at(i - 1);
        }
        return 0;
    }

    void setIsFunction()
    {
        m_isFunction = true;
        m_isFunctionBoundary = true;
    }
    bool isFunction() { return m_isFunction; }
    bool isFunctionBoundary() { return m_isFunctionBoundary; }

    void declareCallee(const Identifier* ident)
    {
        m_declaredVariables.add(ident->string().impl());
    }

    bool declareVariable(const Identifier* ident)
    {
        bool isValidStrictMode = m_vm->propertyNames->eval != *ident && m_vm->propertyNames->arguments != *ident;
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        m_declaredVariables.add(ident->string().impl());
        return isValidStrictMode;
    }

    bool hasDeclaredVariable(const Identifier& ident)
    {
        return m_declaredVariables.contains(ident.impl());
    }
    
    bool hasDeclaredParameter(const Identifier& ident)
    {
        return m_declaredParameters.contains(ident.impl()) || m_declaredVariables.contains(ident.impl());
    }
    
    void declareWrite(const Identifier* ident)
    {
        ASSERT(m_strictMode);
        m_writtenVariables.add(ident->impl());
    }

    void preventNewDecls() { m_allowsNewDecls = false; }
    bool allowsNewDecls() const { return m_allowsNewDecls; }

    bool declareParameter(const Identifier* ident)
    {
        bool isArguments = m_vm->propertyNames->arguments == *ident;
        bool isValidStrictMode = m_declaredVariables.add(ident->string().impl()).isNewEntry && m_vm->propertyNames->eval != *ident && !isArguments;
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        m_declaredParameters.add(ident->string().impl());

        if (isArguments)
            m_shadowsArguments = true;
        return isValidStrictMode;
    }
    
    enum BindingResult {
        BindingFailed,
        StrictBindingFailed,
        BindingSucceeded
    };
    BindingResult declareBoundParameter(const Identifier* ident)
    {
        bool isArguments = m_vm->propertyNames->arguments == *ident;
        bool newEntry = m_declaredVariables.add(ident->string().impl()).isNewEntry;
        bool isValidStrictMode = newEntry && m_vm->propertyNames->eval != *ident && !isArguments;
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
    
        if (isArguments)
            m_shadowsArguments = true;
        if (!newEntry)
            return BindingFailed;
        return isValidStrictMode ? BindingSucceeded : StrictBindingFailed;
    }

    void getUsedVariables(IdentifierSet& usedVariables)
    {
        usedVariables.swap(m_usedVariables);
    }

    void useVariable(const Identifier* ident, bool isEval)
    {
        m_usesEval |= isEval;
        m_usedVariables.add(ident->string().impl());
    }

    void setNeedsFullActivation() { m_needsFullActivation = true; }

    bool collectFreeVariables(Scope* nestedScope, bool shouldTrackClosedVariables)
    {
        if (nestedScope->m_usesEval)
            m_usesEval = true;
        IdentifierSet::iterator end = nestedScope->m_usedVariables.end();
        for (IdentifierSet::iterator ptr = nestedScope->m_usedVariables.begin(); ptr != end; ++ptr) {
            if (nestedScope->m_declaredVariables.contains(*ptr))
                continue;
            m_usedVariables.add(*ptr);
            if (shouldTrackClosedVariables)
                m_closedVariables.add(*ptr);
        }
        if (nestedScope->m_writtenVariables.size()) {
            IdentifierSet::iterator end = nestedScope->m_writtenVariables.end();
            for (IdentifierSet::iterator ptr = nestedScope->m_writtenVariables.begin(); ptr != end; ++ptr) {
                if (nestedScope->m_declaredVariables.contains(*ptr))
                    continue;
                m_writtenVariables.add(*ptr);
            }
        }

        return true;
    }

    void getCapturedVariables(IdentifierSet& capturedVariables, bool& modifiedParameter)
    {
        if (m_needsFullActivation || m_usesEval) {
            modifiedParameter = true;
            capturedVariables.swap(m_declaredVariables);
            return;
        }
        for (IdentifierSet::iterator ptr = m_closedVariables.begin(); ptr != m_closedVariables.end(); ++ptr) {
            if (!m_declaredVariables.contains(*ptr))
                continue;
            capturedVariables.add(*ptr);
        }
        modifiedParameter = false;
        if (m_declaredParameters.size()) {
            IdentifierSet::iterator end = m_writtenVariables.end();
            for (IdentifierSet::iterator ptr = m_writtenVariables.begin(); ptr != end; ++ptr) {
                if (!m_declaredParameters.contains(*ptr))
                    continue;
                modifiedParameter = true;
                break;
            }
        }
    }
    void setStrictMode() { m_strictMode = true; }
    bool strictMode() const { return m_strictMode; }
    bool isValidStrictMode() const { return m_isValidStrictMode; }
    bool shadowsArguments() const { return m_shadowsArguments; }

    void copyCapturedVariablesToVector(const IdentifierSet& capturedVariables, Vector<RefPtr<StringImpl>>& vector)
    {
        IdentifierSet::iterator end = capturedVariables.end();
        for (IdentifierSet::iterator it = capturedVariables.begin(); it != end; ++it) {
            if (m_declaredVariables.contains(*it))
                continue;
            vector.append(*it);
        }
    }

    void fillParametersForSourceProviderCache(SourceProviderCacheItemCreationParameters& parameters)
    {
        ASSERT(m_isFunction);
        parameters.usesEval = m_usesEval;
        parameters.strictMode = m_strictMode;
        parameters.needsFullActivation = m_needsFullActivation;
        copyCapturedVariablesToVector(m_writtenVariables, parameters.writtenVariables);
        copyCapturedVariablesToVector(m_usedVariables, parameters.usedVariables);
    }

    void restoreFromSourceProviderCache(const SourceProviderCacheItem* info)
    {
        ASSERT(m_isFunction);
        m_usesEval = info->usesEval;
        m_strictMode = info->strictMode;
        m_needsFullActivation = info->needsFullActivation;
        for (unsigned i = 0; i < info->usedVariablesCount; ++i)
            m_usedVariables.add(info->usedVariables()[i]);
        for (unsigned i = 0; i < info->writtenVariablesCount; ++i)
            m_writtenVariables.add(info->writtenVariables()[i]);
    }

private:
    const VM* m_vm;
    bool m_shadowsArguments : 1;
    bool m_usesEval : 1;
    bool m_needsFullActivation : 1;
    bool m_allowsNewDecls : 1;
    bool m_strictMode : 1;
    bool m_isFunction : 1;
    bool m_isFunctionBoundary : 1;
    bool m_isValidStrictMode : 1;
    int m_loopDepth;
    int m_switchDepth;

    typedef Vector<ScopeLabelInfo, 2> LabelStack;
    OwnPtr<LabelStack> m_labels;
    IdentifierSet m_declaredParameters;
    IdentifierSet m_declaredVariables;
    IdentifierSet m_usedVariables;
    IdentifierSet m_closedVariables;
    IdentifierSet m_writtenVariables;
};

typedef Vector<Scope, 10> ScopeStack;

struct ScopeRef {
    ScopeRef(ScopeStack* scopeStack, unsigned index)
        : m_scopeStack(scopeStack)
        , m_index(index)
    {
    }
    Scope* operator->() { return &m_scopeStack->at(m_index); }
    unsigned index() const { return m_index; }

    bool hasContainingScope()
    {
        return m_index && !m_scopeStack->at(m_index).isFunctionBoundary();
    }

    ScopeRef containingScope()
    {
        ASSERT(hasContainingScope());
        return ScopeRef(m_scopeStack, m_index - 1);
    }

private:
    ScopeStack* m_scopeStack;
    unsigned m_index;
};

template <typename LexerType>
class Parser {
    WTF_MAKE_NONCOPYABLE(Parser);
    WTF_MAKE_FAST_ALLOCATED;

public:
    Parser(VM*, const SourceCode&, FunctionParameters*, const Identifier&, JSParserStrictness, JSParserMode);
    ~Parser();

    template <class ParsedNode>
    PassRefPtr<ParsedNode> parse(ParserError&);

    JSTextPosition positionBeforeLastNewline() const { return m_lexer->positionBeforeLastNewline(); }
    const Vector<RefPtr<StringImpl>>&& closedVariables() { return std::move(m_closedVariables); }

private:
    struct AllowInOverride {
        AllowInOverride(Parser* parser)
            : m_parser(parser)
            , m_oldAllowsIn(parser->m_allowsIn)
        {
            parser->m_allowsIn = true;
        }
        ~AllowInOverride()
        {
            m_parser->m_allowsIn = m_oldAllowsIn;
        }
        Parser* m_parser;
        bool m_oldAllowsIn;
    };

    struct AutoPopScopeRef : public ScopeRef {
        AutoPopScopeRef(Parser* parser, ScopeRef scope)
        : ScopeRef(scope)
        , m_parser(parser)
        {
        }
        
        ~AutoPopScopeRef()
        {
            if (m_parser)
                m_parser->popScope(*this, false);
        }
        
        void setPopped()
        {
            m_parser = 0;
        }
        
    private:
        Parser* m_parser;
    };

    ScopeRef currentScope()
    {
        return ScopeRef(&m_scopeStack, m_scopeStack.size() - 1);
    }
    
    ScopeRef pushScope()
    {
        bool isFunction = false;
        bool isStrict = false;
        if (!m_scopeStack.isEmpty()) {
            isStrict = m_scopeStack.last().strictMode();
            isFunction = m_scopeStack.last().isFunction();
        }
        m_scopeStack.append(Scope(m_vm, isFunction, isStrict));
        return currentScope();
    }
    
    bool popScopeInternal(ScopeRef& scope, bool shouldTrackClosedVariables)
    {
        ASSERT_UNUSED(scope, scope.index() == m_scopeStack.size() - 1);
        ASSERT(m_scopeStack.size() > 1);
        bool result = m_scopeStack[m_scopeStack.size() - 2].collectFreeVariables(&m_scopeStack.last(), shouldTrackClosedVariables);
        m_scopeStack.removeLast();
        return result;
    }
    
    bool popScope(ScopeRef& scope, bool shouldTrackClosedVariables)
    {
        return popScopeInternal(scope, shouldTrackClosedVariables);
    }
    
    bool popScope(AutoPopScopeRef& scope, bool shouldTrackClosedVariables)
    {
        scope.setPopped();
        return popScopeInternal(scope, shouldTrackClosedVariables);
    }
    
    bool declareVariable(const Identifier* ident)
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsNewDecls()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return m_scopeStack[i].declareVariable(ident);
    }
    
    NEVER_INLINE bool hasDeclaredVariable(const Identifier& ident)
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsNewDecls()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return m_scopeStack[i].hasDeclaredVariable(ident);
    }
    
    NEVER_INLINE bool hasDeclaredParameter(const Identifier& ident)
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsNewDecls()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return m_scopeStack[i].hasDeclaredParameter(ident);
    }
    
    void declareWrite(const Identifier* ident)
    {
        if (!m_syntaxAlreadyValidated || strictMode())
            m_scopeStack.last().declareWrite(ident);
    }
    
    ScopeStack m_scopeStack;
    
    const SourceProviderCacheItem* findCachedFunctionInfo(int openBracePos) 
    {
        return m_functionCache ? m_functionCache->get(openBracePos) : 0;
    }

    Parser();
    String parseInner();

    void didFinishParsing(SourceElements*, ParserArenaData<DeclarationStacks::VarStack>*, 
        ParserArenaData<DeclarationStacks::FunctionStack>*, CodeFeatures, int, IdentifierSet&, const Vector<RefPtr<StringImpl>>&&);

    // Used to determine type of error to report.
    bool isFunctionBodyNode(ScopeNode*) { return false; }
    bool isFunctionBodyNode(FunctionBodyNode*) { return true; }

    ALWAYS_INLINE void next(unsigned lexerFlags = 0)
    {
        int lastLine = m_token.m_location.line;
        int lastTokenEnd = m_token.m_location.endOffset;
        int lastTokenLineStart = m_token.m_location.lineStartOffset;
        m_lastTokenEndPosition = JSTextPosition(lastLine, lastTokenEnd, lastTokenLineStart);
        m_lexer->setLastLineNumber(lastLine);
        m_token.m_type = m_lexer->lex(&m_token, lexerFlags, strictMode());
    }

    ALWAYS_INLINE void nextExpectIdentifier(unsigned lexerFlags = 0)
    {
        int lastLine = m_token.m_location.line;
        int lastTokenEnd = m_token.m_location.endOffset;
        int lastTokenLineStart = m_token.m_location.lineStartOffset;
        m_lastTokenEndPosition = JSTextPosition(lastLine, lastTokenEnd, lastTokenLineStart);
        m_lexer->setLastLineNumber(lastLine);
        m_token.m_type = m_lexer->lexExpectIdentifier(&m_token, lexerFlags, strictMode());
    }

    ALWAYS_INLINE bool nextTokenIsColon()
    {
        return m_lexer->nextTokenIsColon();
    }

    ALWAYS_INLINE bool consume(JSTokenType expected, unsigned flags = 0)
    {
        bool result = m_token.m_type == expected;
        if (result)
            next(flags);
        return result;
    }

    void printUnexpectedTokenText(WTF::PrintStream&);
    ALWAYS_INLINE String getToken() {
        SourceProvider* sourceProvider = m_source->provider();
        return sourceProvider->getRange(tokenStart(), tokenEndPosition().offset);
    }
    
    ALWAYS_INLINE bool match(JSTokenType expected)
    {
        return m_token.m_type == expected;
    }
    
    ALWAYS_INLINE bool isofToken()
    {
        return m_token.m_type == IDENT && *m_token.m_data.ident == m_vm->propertyNames->of;
    }
    
    ALWAYS_INLINE unsigned tokenStart()
    {
        return m_token.m_location.startOffset;
    }
    
    ALWAYS_INLINE const JSTextPosition& tokenStartPosition()
    {
        return m_token.m_startPosition;
    }

    ALWAYS_INLINE int tokenLine()
    {
        return m_token.m_location.line;
    }
    
    ALWAYS_INLINE int tokenColumn()
    {
        return tokenStart() - tokenLineStart();
    }

    ALWAYS_INLINE const JSTextPosition& tokenEndPosition()
    {
        return m_token.m_endPosition;
    }
    
    ALWAYS_INLINE unsigned tokenLineStart()
    {
        return m_token.m_location.lineStartOffset;
    }
    
    ALWAYS_INLINE const JSTokenLocation& tokenLocation()
    {
        return m_token.m_location;
    }

    void setErrorMessage(String msg)
    {
        m_errorMessage = msg;
    }
    
    NEVER_INLINE void logError(bool);
    template <typename A> NEVER_INLINE void logError(bool, const A&);
    template <typename A, typename B> NEVER_INLINE void logError(bool, const A&, const B&);
    template <typename A, typename B, typename C> NEVER_INLINE void logError(bool, const A&, const B&, const C&);
    template <typename A, typename B, typename C, typename D> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&);
    template <typename A, typename B, typename C, typename D, typename E> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&, const E&);
    template <typename A, typename B, typename C, typename D, typename E, typename F> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&, const E&, const F&);
    template <typename A, typename B, typename C, typename D, typename E, typename F, typename G> NEVER_INLINE void logError(bool, const A&, const B&, const C&, const D&, const E&, const F&, const G&);
    
    NEVER_INLINE void updateErrorWithNameAndMessage(const char* beforeMsg, String name, const char* afterMsg)
    {
        m_errorMessage = makeString(beforeMsg, " '", name, "' ", afterMsg);
    }
    
    NEVER_INLINE void updateErrorMessage(const char* msg)
    {
        ASSERT(msg);
        m_errorMessage = String(msg);
        ASSERT(!m_errorMessage.isNull());
    }
    
    void startLoop() { currentScope()->startLoop(); }
    void endLoop() { currentScope()->endLoop(); }
    void startSwitch() { currentScope()->startSwitch(); }
    void endSwitch() { currentScope()->endSwitch(); }
    void setStrictMode() { currentScope()->setStrictMode(); }
    bool strictMode() { return currentScope()->strictMode(); }
    bool isValidStrictMode() { return currentScope()->isValidStrictMode(); }
    bool declareParameter(const Identifier* ident) { return currentScope()->declareParameter(ident); }
    Scope::BindingResult declareBoundParameter(const Identifier* ident) { return currentScope()->declareBoundParameter(ident); }
    bool breakIsValid()
    {
        ScopeRef current = currentScope();
        while (!current->breakIsValid()) {
            if (!current.hasContainingScope())
                return false;
            current = current.containingScope();
        }
        return true;
    }
    bool continueIsValid()
    {
        ScopeRef current = currentScope();
        while (!current->continueIsValid()) {
            if (!current.hasContainingScope())
                return false;
            current = current.containingScope();
        }
        return true;
    }
    void pushLabel(const Identifier* label, bool isLoop) { currentScope()->pushLabel(label, isLoop); }
    void popLabel() { currentScope()->popLabel(); }
    ScopeLabelInfo* getLabel(const Identifier* label)
    {
        ScopeRef current = currentScope();
        ScopeLabelInfo* result = 0;
        while (!(result = current->getLabel(label))) {
            if (!current.hasContainingScope())
                return 0;
            current = current.containingScope();
        }
        return result;
    }
    
    template <class TreeBuilder> TreeSourceElements parseSourceElements(TreeBuilder&, SourceElementsMode);
    template <class TreeBuilder> TreeStatement parseStatement(TreeBuilder&, const Identifier*& directive, unsigned* directiveLiteralLength = 0);
    template <class TreeBuilder> TreeStatement parseFunctionDeclaration(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseVarDeclaration(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseConstDeclaration(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseDoWhileStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseWhileStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseForStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseBreakStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseContinueStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseReturnStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseThrowStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseWithStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseSwitchStatement(TreeBuilder&);
    template <class TreeBuilder> TreeClauseList parseSwitchClauses(TreeBuilder&);
    template <class TreeBuilder> TreeClause parseSwitchDefaultClause(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseTryStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseDebuggerStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseExpressionStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseExpressionOrLabelStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseIfStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseBlockStatement(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseConditionalExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseBinaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseUnaryExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseMemberExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parsePrimaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseArrayLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseObjectLiteral(TreeBuilder&);
    template <class TreeBuilder> NEVER_INLINE TreeExpression parseStrictObjectLiteral(TreeBuilder&);
    enum SpreadMode { AllowSpread, DontAllowSpread };
    template <class TreeBuilder> ALWAYS_INLINE TreeArguments parseArguments(TreeBuilder&, SpreadMode);
    template <class TreeBuilder> TreeProperty parseProperty(TreeBuilder&, bool strict);
    template <class TreeBuilder> ALWAYS_INLINE TreeFunctionBody parseFunctionBody(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeFormalParameterList parseFormalParameters(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseVarDeclarationList(TreeBuilder&, int& declarations, TreeDeconstructionPattern& lastPattern, TreeExpression& lastInitializer, JSTextPosition& identStart, JSTextPosition& initStart, JSTextPosition& initEnd);
    template <class TreeBuilder> NEVER_INLINE TreeConstDeclList parseConstDeclarationList(TreeBuilder&);

    template <class TreeBuilder> NEVER_INLINE TreeDeconstructionPattern createBindingPattern(TreeBuilder&, DeconstructionKind, const Identifier&, int depth);
    template <class TreeBuilder> NEVER_INLINE TreeDeconstructionPattern parseDeconstructionPattern(TreeBuilder&, DeconstructionKind, int depth = 0);
    template <class TreeBuilder> NEVER_INLINE TreeDeconstructionPattern tryParseDeconstructionPatternExpression(TreeBuilder&);
    template <class TreeBuilder> NEVER_INLINE bool parseFunctionInfo(TreeBuilder&, FunctionRequirements, FunctionParseMode, bool nameIsInContainingScope, const Identifier*&, TreeFormalParameterList&, TreeFunctionBody&, unsigned& openBraceOffset, unsigned& closeBraceOffset, int& bodyStartLine, unsigned& bodyStartColumn);
    ALWAYS_INLINE int isBinaryOperator(JSTokenType);
    bool allowAutomaticSemicolon();
    
    bool autoSemiColon()
    {
        if (m_token.m_type == SEMICOLON) {
            next();
            return true;
        }
        return allowAutomaticSemicolon();
    }
    
    bool canRecurse()
    {
        return m_vm->isSafeToRecurse();
    }
    
    const JSTextPosition& lastTokenEndPosition() const
    {
        return m_lastTokenEndPosition;
    }

    bool hasError() const
    {
        return !m_errorMessage.isNull();
    }

    struct SavePoint {
        int startOffset;
        unsigned oldLineStartOffset;
        unsigned oldLastLineNumber;
        unsigned oldLineNumber;
    };
    
    ALWAYS_INLINE SavePoint createSavePoint()
    {
        ASSERT(!hasError());
        SavePoint result;
        result.startOffset = m_token.m_location.startOffset;
        result.oldLineStartOffset = m_token.m_location.lineStartOffset;
        result.oldLastLineNumber = m_lexer->lastLineNumber();
        result.oldLineNumber = m_lexer->lineNumber();
        return result;
    }
    
    ALWAYS_INLINE void restoreSavePoint(const SavePoint& savePoint)
    {
        m_errorMessage = String();
        m_lexer->setOffset(savePoint.startOffset, savePoint.oldLineStartOffset);
        next();
        m_lexer->setLastLineNumber(savePoint.oldLastLineNumber);
        m_lexer->setLineNumber(savePoint.oldLineNumber);
    }

    struct ParserState {
        int assignmentCount;
        int nonLHSCount;
        int nonTrivialExpressionCount;
    };

    ALWAYS_INLINE ParserState saveState()
    {
        ParserState result;
        result.assignmentCount = m_assignmentCount;
        result.nonLHSCount = m_nonLHSCount;
        result.nonTrivialExpressionCount = m_nonTrivialExpressionCount;
        return result;
    }
    
    ALWAYS_INLINE void restoreState(const ParserState& state)
    {
        m_assignmentCount = state.assignmentCount;
        m_nonLHSCount = state.nonLHSCount;
        m_nonTrivialExpressionCount = state.nonTrivialExpressionCount;
        
    }
    

    VM* m_vm;
    const SourceCode* m_source;
    ParserArena* m_arena;
    OwnPtr<LexerType> m_lexer;
    
    bool m_hasStackOverflow;
    String m_errorMessage;
    JSToken m_token;
    bool m_allowsIn;
    JSTextPosition m_lastTokenEndPosition;
    int m_assignmentCount;
    int m_nonLHSCount;
    bool m_syntaxAlreadyValidated;
    int m_statementDepth;
    int m_nonTrivialExpressionCount;
    const Identifier* m_lastIdentifier;
    const Identifier* m_lastFunctionName;
    RefPtr<SourceProviderCache> m_functionCache;
    SourceElements* m_sourceElements;
    bool m_parsingBuiltin;
    ParserArenaData<DeclarationStacks::VarStack>* m_varDeclarations;
    ParserArenaData<DeclarationStacks::FunctionStack>* m_funcDeclarations;
    IdentifierSet m_capturedVariables;
    Vector<RefPtr<StringImpl>> m_closedVariables;
    CodeFeatures m_features;
    int m_numConstants;
    
    struct DepthManager {
        DepthManager(int* depth)
        : m_originalDepth(*depth)
        , m_depth(depth)
        {
        }
        
        ~DepthManager()
        {
            *m_depth = m_originalDepth;
        }
        
    private:
        int m_originalDepth;
        int* m_depth;
    };
};


template <typename LexerType>
template <class ParsedNode>
PassRefPtr<ParsedNode> Parser<LexerType>::parse(ParserError& error)
{
    int errLine;
    String errMsg;

    if (ParsedNode::scopeIsFunction)
        m_lexer->setIsReparsing();

    m_sourceElements = 0;

    errLine = -1;
    errMsg = String();

    JSTokenLocation startLocation(tokenLocation());
    ASSERT(m_source->startColumn() > 0);
    unsigned startColumn = m_source->startColumn() - 1;

    String parseError = parseInner();

    int lineNumber = m_lexer->lineNumber();
    bool lexError = m_lexer->sawError();
    String lexErrorMessage = lexError ? m_lexer->getErrorMessage() : String();
    ASSERT(lexErrorMessage.isNull() != lexError);
    m_lexer->clear();

    if (!parseError.isNull() || lexError) {
        errLine = lineNumber;
        errMsg = !lexErrorMessage.isNull() ? lexErrorMessage : parseError;
        m_sourceElements = 0;
    }

    RefPtr<ParsedNode> result;
    if (m_sourceElements) {
        JSTokenLocation endLocation;
        endLocation.line = m_lexer->lineNumber();
        endLocation.lineStartOffset = m_lexer->currentLineStartOffset();
        endLocation.startOffset = m_lexer->currentOffset();
        unsigned endColumn = endLocation.startOffset - endLocation.lineStartOffset;
        result = ParsedNode::create(m_vm,
                                    startLocation,
                                    endLocation,
                                    startColumn,
                                    endColumn,
                                    m_sourceElements,
                                    m_varDeclarations ? &m_varDeclarations->data : 0,
                                    m_funcDeclarations ? &m_funcDeclarations->data : 0,
                                    m_capturedVariables,
                                    *m_source,
                                    m_features,
                                    m_numConstants);
        result->setLoc(m_source->firstLine(), m_lexer->lineNumber(), m_lexer->currentOffset(), m_lexer->currentLineStartOffset());
    } else {
        // We can never see a syntax error when reparsing a function, since we should have
        // reported the error when parsing the containing program or eval code. So if we're
        // parsing a function body node, we assume that what actually happened here is that
        // we ran out of stack while parsing. If we see an error while parsing eval or program
        // code we assume that it was a syntax error since running out of stack is much less
        // likely, and we are currently unable to distinguish between the two cases.
        if (isFunctionBodyNode(static_cast<ParsedNode*>(0)) || m_hasStackOverflow)
            error = ParserError(ParserError::StackOverflow, ParserError::SyntaxErrorNone, m_token);
        else {
            ParserError::SyntaxErrorType errorType = ParserError::SyntaxErrorIrrecoverable;
            if (m_token.m_type == EOFTOK)
                errorType = ParserError::SyntaxErrorRecoverable;
            else if (m_token.m_type & UnterminatedErrorTokenFlag)
                errorType = ParserError::SyntaxErrorUnterminatedLiteral;
            
            if (isEvalNode<ParsedNode>())
                error = ParserError(ParserError::EvalError, errorType, m_token, errMsg, errLine);
            else
                error = ParserError(ParserError::SyntaxError, errorType, m_token, errMsg, errLine);
        }
    }

    m_arena->reset();

    return result.release();
}

template <class ParsedNode>
PassRefPtr<ParsedNode> parse(VM* vm, const SourceCode& source, FunctionParameters* parameters, const Identifier& name, JSParserStrictness strictness, JSParserMode parserMode, ParserError& error, JSTextPosition* positionBeforeLastNewline = 0)
{
    SamplingRegion samplingRegion("Parsing");

    ASSERT(!source.provider()->source().isNull());
    if (source.provider()->source().is8Bit()) {
        Parser<Lexer<LChar>> parser(vm, source, parameters, name, strictness, parserMode);
        RefPtr<ParsedNode> result = parser.parse<ParsedNode>(error);
        if (positionBeforeLastNewline)
            *positionBeforeLastNewline = parser.positionBeforeLastNewline();
        if (strictness == JSParseBuiltin) {
            if (!result)
                WTF::dataLog("Error compiling builtin: ", error.m_message, "\n");
            RELEASE_ASSERT(result);
            result->setClosedVariables(std::move(parser.closedVariables()));
        }
        return result.release();
    }
    Parser<Lexer<UChar>> parser(vm, source, parameters, name, strictness, parserMode);
    RefPtr<ParsedNode> result = parser.parse<ParsedNode>(error);
    if (positionBeforeLastNewline)
        *positionBeforeLastNewline = parser.positionBeforeLastNewline();
    return result.release();
}

} // namespace
#endif
