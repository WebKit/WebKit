/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include "ParserTokens.h"
#include "SourceProvider.h"
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
class JSGlobalData;
class ProgramNode;
class SourceCode;
class UString;

#define fail() do { if (!m_error) updateErrorMessage(); return 0; } while (0)
#define failWithToken(tok) do { if (!m_error) updateErrorMessage(tok); return 0; } while (0)
#define failWithMessage(msg) do { if (!m_error) updateErrorMessage(msg); return 0; } while (0)
#define failWithNameAndMessage(before, name, after) do { if (!m_error) updateErrorWithNameAndMessage(before, name, after); return 0; } while (0)
#define failIfFalse(cond) do { if (!(cond)) fail(); } while (0)
#define failIfFalseWithMessage(cond, msg) do { if (!(cond)) failWithMessage(msg); } while (0)
#define failIfFalseWithNameAndMessage(cond, before, name, msg) do { if (!(cond)) failWithNameAndMessage(before, name, msg); } while (0)
#define failIfTrue(cond) do { if ((cond)) fail(); } while (0)
#define failIfTrueWithMessage(cond, msg) do { if ((cond)) failWithMessage(msg); } while (0)
#define failIfTrueWithNameAndMessage(cond, before, name, msg) do { if ((cond)) failWithNameAndMessage(before, name, msg); } while (0)
#define failIfTrueIfStrict(cond) do { if ((cond) && strictMode()) fail(); } while (0)
#define failIfTrueIfStrictWithMessage(cond, msg) do { if ((cond) && strictMode()) failWithMessage(msg); } while (0)
#define failIfTrueIfStrictWithNameAndMessage(cond, before, name, after) do { if ((cond) && strictMode()) failWithNameAndMessage(before, name, after); } while (0)
#define failIfFalseIfStrict(cond) do { if ((!(cond)) && strictMode()) fail(); } while (0)
#define failIfFalseIfStrictWithMessage(cond, msg) do { if ((!(cond)) && strictMode()) failWithMessage(msg); } while (0)
#define failIfFalseIfStrictWithNameAndMessage(cond, before, name, after) do { if ((!(cond)) && strictMode()) failWithNameAndMessage(before, name, after); } while (0)
#define consumeOrFail(tokenType) do { if (!consume(tokenType)) failWithToken(tokenType); } while (0)
#define consumeOrFailWithFlags(tokenType, flags) do { if (!consume(tokenType, flags)) failWithToken(tokenType); } while (0)
#define matchOrFail(tokenType) do { if (!match(tokenType)) failWithToken(tokenType); } while (0)
#define failIfStackOverflow() do { failIfFalseWithMessage(canRecurse(), "Code nested too deeply."); } while (0)

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

COMPILE_ASSERT(LastUntaggedToken < 64, LessThan64UntaggedTokens);

enum SourceElementsMode { CheckForStrictMode, DontCheckForStrictMode };
enum FunctionRequirements { FunctionNoRequirements, FunctionNeedsName };

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
    Scope(const JSGlobalData* globalData, bool isFunction, bool strictMode)
        : m_globalData(globalData)
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
        : m_globalData(rhs.m_globalData)
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

    bool declareVariable(const Identifier* ident)
    {
        bool isValidStrictMode = m_globalData->propertyNames->eval != *ident && m_globalData->propertyNames->arguments != *ident;
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        m_declaredVariables.add(ident->ustring().impl());
        return isValidStrictMode;
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
        bool isArguments = m_globalData->propertyNames->arguments == *ident;
        bool isValidStrictMode = m_declaredVariables.add(ident->ustring().impl()).second && m_globalData->propertyNames->eval != *ident && !isArguments;
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        if (isArguments)
            m_shadowsArguments = true;
        return isValidStrictMode;
    }

    void useVariable(const Identifier* ident, bool isEval)
    {
        m_usesEval |= isEval;
        m_usedVariables.add(ident->ustring().impl());
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

    void getUncapturedWrittenVariables(IdentifierSet& writtenVariables)
    {
        IdentifierSet::iterator end = m_writtenVariables.end();
        for (IdentifierSet::iterator ptr = m_writtenVariables.begin(); ptr != end; ++ptr) {
            if (!m_declaredVariables.contains(*ptr))
                writtenVariables.add(*ptr);
        }
    }

    void getCapturedVariables(IdentifierSet& capturedVariables)
    {
        if (m_needsFullActivation || m_usesEval) {
            capturedVariables.swap(m_declaredVariables);
            return;
        }
        for (IdentifierSet::iterator ptr = m_closedVariables.begin(); ptr != m_closedVariables.end(); ++ptr) {
            if (!m_declaredVariables.contains(*ptr))
                continue;
            capturedVariables.add(*ptr);
        }
    }
    void setStrictMode() { m_strictMode = true; }
    bool strictMode() const { return m_strictMode; }
    bool isValidStrictMode() const { return m_isValidStrictMode; }
    bool shadowsArguments() const { return m_shadowsArguments; }

    void copyCapturedVariablesToVector(const IdentifierSet& capturedVariables, Vector<RefPtr<StringImpl> >& vector)
    {
        IdentifierSet::iterator end = capturedVariables.end();
        for (IdentifierSet::iterator it = capturedVariables.begin(); it != end; ++it) {
            if (m_declaredVariables.contains(*it))
                continue;
            vector.append(*it);
        }
        vector.shrinkToFit();
    }

    void saveFunctionInfo(SourceProviderCacheItem* info)
    {
        ASSERT(m_isFunction);
        info->usesEval = m_usesEval;
        info->strictMode = m_strictMode;
        info->needsFullActivation = m_needsFullActivation;
        copyCapturedVariablesToVector(m_writtenVariables, info->writtenVariables);
        copyCapturedVariablesToVector(m_usedVariables, info->usedVariables);
    }

    void restoreFunctionInfo(const SourceProviderCacheItem* info)
    {
        ASSERT(m_isFunction);
        m_usesEval = info->usesEval;
        m_strictMode = info->strictMode;
        m_needsFullActivation = info->needsFullActivation;
        unsigned size = info->usedVariables.size();
        for (unsigned i = 0; i < size; ++i)
            m_usedVariables.add(info->usedVariables[i]);
        size = info->writtenVariables.size();
        for (unsigned i = 0; i < size; ++i)
            m_writtenVariables.add(info->writtenVariables[i]);
    }

private:
    const JSGlobalData* m_globalData;
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
    Parser(JSGlobalData*, const SourceCode&, FunctionParameters*, JSParserStrictness, JSParserMode);
    ~Parser();

    template <class ParsedNode>
    PassRefPtr<ParsedNode> parse(JSGlobalObject* lexicalGlobalObject, Debugger*, ExecState*, JSObject**);

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
        m_scopeStack.append(Scope(m_globalData, isFunction, isStrict));
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
    
    void declareWrite(const Identifier* ident)
    {
        if (!m_syntaxAlreadyValidated)
            m_scopeStack.last().declareWrite(ident);
    }
    
    ScopeStack m_scopeStack;
    
    const SourceProviderCacheItem* findCachedFunctionInfo(int openBracePos) 
    {
        return m_functionCache ? m_functionCache->get(openBracePos) : 0;
    }

    Parser();
    UString parseInner();

    void didFinishParsing(SourceElements*, ParserArenaData<DeclarationStacks::VarStack>*, 
                          ParserArenaData<DeclarationStacks::FunctionStack>*, CodeFeatures,
                          int, int, IdentifierSet&);

    // Used to determine type of error to report.
    bool isFunctionBodyNode(ScopeNode*) { return false; }
    bool isFunctionBodyNode(FunctionBodyNode*) { return true; }


    ALWAYS_INLINE void next(unsigned lexerFlags = 0)
    {
        m_lastLine = m_token.m_info.line;
        m_lastTokenEnd = m_token.m_info.endOffset;
        m_lexer->setLastLineNumber(m_lastLine);
        m_token.m_type = m_lexer->lex(&m_token.m_data, &m_token.m_info, lexerFlags, strictMode());
    }

    ALWAYS_INLINE void nextExpectIdentifier(unsigned lexerFlags = 0)
    {
        m_lastLine = m_token.m_info.line;
        m_lastTokenEnd = m_token.m_info.endOffset;
        m_lexer->setLastLineNumber(m_lastLine);
        m_token.m_type = m_lexer->lexExpectIdentifier(&m_token.m_data, &m_token.m_info, lexerFlags, strictMode());
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
    
    ALWAYS_INLINE UString getToken() {
        SourceProvider* sourceProvider = m_source->provider();
        return UString(sourceProvider->getRange(tokenStart(), tokenEnd()).impl());
    }
    
    ALWAYS_INLINE bool match(JSTokenType expected)
    {
        return m_token.m_type == expected;
    }
    
    ALWAYS_INLINE int tokenStart()
    {
        return m_token.m_info.startOffset;
    }
    
    ALWAYS_INLINE int tokenLine()
    {
        return m_token.m_info.line;
    }
    
    ALWAYS_INLINE int tokenEnd()
    {
        return m_token.m_info.endOffset;
    }
    
    const char* getTokenName(JSTokenType tok) 
    {
        switch (tok) {
        case NULLTOKEN: 
            return "null";
        case TRUETOKEN:
            return "true";
        case FALSETOKEN: 
            return "false";
        case BREAK: 
            return "break";
        case CASE: 
            return "case";
        case DEFAULT: 
            return "defualt";
        case FOR: 
            return "for";
        case NEW: 
            return "new";
        case VAR: 
            return "var";
        case CONSTTOKEN: 
            return "const";
        case CONTINUE: 
            return "continue";
        case FUNCTION: 
            return "function";
        case IF: 
            return "if";
        case THISTOKEN: 
            return "this";
        case DO: 
            return "do";
        case WHILE: 
            return "while";
        case SWITCH: 
            return "switch";
        case WITH: 
            return "with";
        case THROW: 
            return "throw";
        case TRY: 
            return "try";
        case CATCH: 
            return "catch";
        case FINALLY: 
            return "finally";
        case DEBUGGER: 
            return "debugger";
        case ELSE: 
            return "else";
        case OPENBRACE: 
            return "{";
        case CLOSEBRACE: 
            return "}";
        case OPENPAREN: 
            return "(";
        case CLOSEPAREN: 
            return ")";
        case OPENBRACKET: 
            return "[";
        case CLOSEBRACKET: 
            return "]";
        case COMMA: 
            return ",";
        case QUESTION: 
            return "?";
        case SEMICOLON: 
            return ";";
        case COLON: 
            return ":";
        case DOT: 
            return ".";
        case EQUAL: 
            return "=";
        case PLUSEQUAL: 
            return "+=";
        case MINUSEQUAL: 
            return "-=";
        case MULTEQUAL: 
            return "*=";
        case DIVEQUAL: 
            return "/=";
        case LSHIFTEQUAL: 
            return "<<=";
        case RSHIFTEQUAL: 
            return ">>=";
        case URSHIFTEQUAL: 
            return ">>>=";
        case ANDEQUAL: 
            return "&=";
        case MODEQUAL: 
            return "%=";
        case XOREQUAL: 
            return "^=";
        case OREQUAL: 
            return "|=";
        case AUTOPLUSPLUS: 
        case PLUSPLUS: 
            return "++";
        case AUTOMINUSMINUS: 
        case MINUSMINUS: 
            return "--";
        case EXCLAMATION: 
            return "!";
        case TILDE: 
            return "~";
        case TYPEOF: 
            return "typeof";
        case VOIDTOKEN: 
            return "void";
        case DELETETOKEN: 
            return "delete";
        case OR: 
            return "||";
        case AND: 
            return "&&";
        case BITOR: 
            return "|";
        case BITXOR: 
            return "^";
        case BITAND: 
            return "&";
        case EQEQ: 
            return "==";
        case NE: 
            return "!=";
        case STREQ: 
            return "===";
        case STRNEQ: 
            return "!==";
        case LT: 
            return "<";
        case GT: 
            return ">";
        case LE: 
            return "<=";
        case GE: 
            return ">=";
        case INSTANCEOF: 
            return "instanceof";
        case INTOKEN: 
            return "in";
        case LSHIFT: 
            return "<<";
        case RSHIFT: 
            return ">>";
        case URSHIFT: 
            return ">>>";
        case PLUS: 
            return "+";
        case MINUS: 
            return "-";
        case TIMES: 
            return "*";
        case DIVIDE: 
            return "/";
        case MOD: 
            return "%";
        case RETURN: 
        case RESERVED_IF_STRICT:
        case RESERVED: 
        case NUMBER:
        case IDENT: 
        case STRING: 
        case ERRORTOK:
        case EOFTOK: 
            return 0;
        case LastUntaggedToken: 
            break;
        }
        ASSERT_NOT_REACHED();
        return "internal error";
    }
    
    ALWAYS_INLINE void updateErrorMessageSpecialCase(JSTokenType expectedToken) 
    {
        String errorMessage;
        switch (expectedToken) {
        case RESERVED_IF_STRICT:
            errorMessage = "Use of reserved word '";
            errorMessage += getToken().impl();
            errorMessage += "' in strict mode";
            m_errorMessage = errorMessage.impl();
            return;
        case RESERVED:
            errorMessage = "Use of reserved word '";
            errorMessage += getToken().impl();
            errorMessage += "'";
            m_errorMessage = errorMessage.impl();
            return;
        case NUMBER: 
            errorMessage = "Unexpected number '";
            errorMessage += getToken().impl();
            errorMessage += "'";
            m_errorMessage = errorMessage.impl();
            return;
        case IDENT: 
            errorMessage = "Expected an identifier but found '";
            errorMessage += getToken().impl();
            errorMessage += "' instead";
            m_errorMessage = errorMessage.impl();
            return;
        case STRING: 
            errorMessage = "Unexpected string ";
            errorMessage += getToken().impl();
            m_errorMessage = errorMessage.impl();
            return;
        case ERRORTOK: 
            errorMessage = "Unrecognized token '";
            errorMessage += getToken().impl();
            errorMessage += "'";
            m_errorMessage = errorMessage.impl();
            return;
        case EOFTOK:  
            m_errorMessage = "Unexpected EOF";
            return;
        case RETURN:
            m_errorMessage = "Return statements are only valid inside functions";
            return;
        default:
            ASSERT_NOT_REACHED();
            m_errorMessage = "internal error";
            return;
        }
    }
    
    NEVER_INLINE void updateErrorMessage() 
    {
        m_error = true;
        const char* name = getTokenName(m_token.m_type);
        if (!name) 
            updateErrorMessageSpecialCase(m_token.m_type);
        else 
            m_errorMessage = UString(String::format("Unexpected token '%s'", name).impl());
    }
    
    NEVER_INLINE void updateErrorMessage(JSTokenType expectedToken) 
    {
        m_error = true;
        const char* name = getTokenName(expectedToken);
        if (name)
            m_errorMessage = UString(String::format("Expected token '%s'", name).impl());
        else {
            if (!getTokenName(m_token.m_type))
                updateErrorMessageSpecialCase(m_token.m_type);
            else
                updateErrorMessageSpecialCase(expectedToken);
        } 
    }
    
    NEVER_INLINE void updateErrorWithNameAndMessage(const char* beforeMsg, UString name, const char* afterMsg) 
    {
        m_error = true;
        String prefix(beforeMsg);
        String postfix(afterMsg);
        prefix += " '";
        prefix += name.impl();
        prefix += "' ";
        prefix += postfix;
        m_errorMessage = prefix.impl();
    }
    
    NEVER_INLINE void updateErrorMessage(const char* msg) 
    {   
        m_error = true;
        m_errorMessage = UString(msg);
    }
    
    void startLoop() { currentScope()->startLoop(); }
    void endLoop() { currentScope()->endLoop(); }
    void startSwitch() { currentScope()->startSwitch(); }
    void endSwitch() { currentScope()->endSwitch(); }
    void setStrictMode() { currentScope()->setStrictMode(); }
    bool strictMode() { return currentScope()->strictMode(); }
    bool isValidStrictMode() { return currentScope()->isValidStrictMode(); }
    bool declareParameter(const Identifier* ident) { return currentScope()->declareParameter(ident); }
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
    
    template <SourceElementsMode mode, class TreeBuilder> TreeSourceElements parseSourceElements(TreeBuilder&);
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
    template <class TreeBuilder> ALWAYS_INLINE TreeStatement parseBlockStatement(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseConditionalExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseBinaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseUnaryExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseMemberExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parsePrimaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseArrayLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseObjectLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseStrictObjectLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeArguments parseArguments(TreeBuilder&);
    template <bool strict, class TreeBuilder> ALWAYS_INLINE TreeProperty parseProperty(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeFunctionBody parseFunctionBody(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeFormalParameterList parseFormalParameters(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseVarDeclarationList(TreeBuilder&, int& declarations, const Identifier*& lastIdent, TreeExpression& lastInitializer, int& identStart, int& initStart, int& initEnd);
    template <class TreeBuilder> ALWAYS_INLINE TreeConstDeclList parseConstDeclarationList(TreeBuilder& context);
    template <FunctionRequirements, bool nameIsInContainingScope, class TreeBuilder> bool parseFunctionInfo(TreeBuilder&, const Identifier*&, TreeFormalParameterList&, TreeFunctionBody&, int& openBrace, int& closeBrace, int& bodyStartLine);
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
        return m_stack.recursionCheck();
    }
    
    int lastTokenEnd() const
    {
        return m_lastTokenEnd;
    }

    mutable const JSGlobalData* m_globalData;
    const SourceCode* m_source;
    ParserArena* m_arena;
    OwnPtr<LexerType> m_lexer;
    
    StackBounds m_stack;
    bool m_error;
    UString m_errorMessage;
    JSToken m_token;
    bool m_allowsIn;
    int m_lastLine;
    int m_lastTokenEnd;
    int m_assignmentCount;
    int m_nonLHSCount;
    bool m_syntaxAlreadyValidated;
    int m_statementDepth;
    int m_nonTrivialExpressionCount;
    const Identifier* m_lastIdentifier;
    SourceProviderCache* m_functionCache;
    SourceElements* m_sourceElements;
    ParserArenaData<DeclarationStacks::VarStack>* m_varDeclarations;
    ParserArenaData<DeclarationStacks::FunctionStack>* m_funcDeclarations;
    IdentifierSet m_capturedVariables;
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
PassRefPtr<ParsedNode> Parser<LexerType>::parse(JSGlobalObject* lexicalGlobalObject, Debugger* debugger, ExecState* debuggerExecState, JSObject** exception)
{
    ASSERT(lexicalGlobalObject);
    ASSERT(exception && !*exception);
    int errLine;
    UString errMsg;

    if (ParsedNode::scopeIsFunction)
        m_lexer->setIsReparsing();

    m_sourceElements = 0;

    errLine = -1;
    errMsg = UString();

    UString parseError = parseInner();

    int lineNumber = m_lexer->lineNumber();
    bool lexError = m_lexer->sawError();
    UString lexErrorMessage = lexError ? m_lexer->getErrorMessage() : UString();
    ASSERT(lexErrorMessage.isNull() != lexError);
    m_lexer->clear();

    if (!parseError.isNull() || lexError) {
        errLine = lineNumber;
        errMsg = !lexErrorMessage.isNull() ? lexErrorMessage : parseError;
        m_sourceElements = 0;
    }

    RefPtr<ParsedNode> result;
    if (m_sourceElements) {
        result = ParsedNode::create(&lexicalGlobalObject->globalData(),
                                    m_lexer->lastLineNumber(),
                                    m_sourceElements,
                                    m_varDeclarations ? &m_varDeclarations->data : 0,
                                    m_funcDeclarations ? &m_funcDeclarations->data : 0,
                                    m_capturedVariables,
                                    *m_source,
                                    m_features,
                                    m_numConstants);
        result->setLoc(m_source->firstLine(), m_lastLine);
    } else if (lexicalGlobalObject) {
        // We can never see a syntax error when reparsing a function, since we should have
        // reported the error when parsing the containing program or eval code. So if we're
        // parsing a function body node, we assume that what actually happened here is that
        // we ran out of stack while parsing. If we see an error while parsing eval or program
        // code we assume that it was a syntax error since running out of stack is much less
        // likely, and we are currently unable to distinguish between the two cases.
        if (isFunctionBodyNode(static_cast<ParsedNode*>(0)))
            *exception = createStackOverflowError(lexicalGlobalObject);
        else if (isEvalNode<ParsedNode>())
            *exception = createSyntaxError(lexicalGlobalObject, errMsg);
        else
            *exception = addErrorInfo(&lexicalGlobalObject->globalData(), createSyntaxError(lexicalGlobalObject, errMsg), errLine, *m_source);
    }

    if (debugger && !ParsedNode::scopeIsFunction)
        debugger->sourceParsed(debuggerExecState, m_source->provider(), errLine, errMsg);

    m_arena->reset();

    return result.release();
}

template <class ParsedNode>
PassRefPtr<ParsedNode> parse(JSGlobalData* globalData, JSGlobalObject* lexicalGlobalObject, const SourceCode& source, FunctionParameters* parameters, JSParserStrictness strictness, JSParserMode parserMode, Debugger* debugger, ExecState* execState, JSObject** exception)
{
    SamplingRegion samplingRegion("Parsing");

    ASSERT(source.provider()->data());

    if (source.provider()->data()->is8Bit()) {
        Parser< Lexer<LChar> > parser(globalData, source, parameters, strictness, parserMode);
        return parser.parse<ParsedNode>(lexicalGlobalObject, debugger, execState, exception);
    }
    Parser< Lexer<UChar> > parser(globalData, source, parameters, strictness, parserMode);
    return parser.parse<ParsedNode>(lexicalGlobalObject, debugger, execState, exception);
}

} // namespace 
#endif
