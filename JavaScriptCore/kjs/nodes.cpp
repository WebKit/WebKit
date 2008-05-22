/*
*  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
*  Copyright (C) 2001 Peter Kelly (pmk@post.com)
*  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
*  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
*  Copyright (C) 2007 Maks Orlovich
*  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "nodes.h"

#include "CodeGenerator.h"
#include "ExecState.h"
#include "JSGlobalObject.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "array_object.h"
#include "debugger.h"
#include "function_object.h"
#include "lexer.h"
#include "operations.h"
#include "regexp_object.h"
#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/MathExtras.h>

namespace KJS {

class FunctionBodyNodeWithDebuggerHooks : public FunctionBodyNode {
public:
    FunctionBodyNodeWithDebuggerHooks(SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;
    virtual JSValue* execute(OldInterpreterExecState*) KJS_FAST_CALL;
};

#if COMPILER(GCC)
#define UNLIKELY(x) \
  __builtin_expect ((x), 0)
#else
#define UNLIKELY(x) x
#endif
    
#define KJS_CHECKEXCEPTION \
if (UNLIKELY(exec->hadException())) \
    return rethrowException(exec);

#define KJS_CHECKEXCEPTIONVALUE \
if (UNLIKELY(exec->hadException())) { \
    handleException(exec); \
    return jsUndefined(); \
}

#define KJS_CHECKEXCEPTIONNUMBER \
if (UNLIKELY(exec->hadException())) { \
    handleException(exec); \
    return 0; \
}

#define KJS_CHECKEXCEPTIONBOOLEAN \
if (UNLIKELY(exec->hadException())) { \
    handleException(exec); \
    return false; \
}

#define KJS_CHECKEXCEPTIONVOID \
if (UNLIKELY(exec->hadException())) { \
    handleException(exec); \
    return; \
}

#if !ASSERT_DISABLED
static inline bool canSkipLookup(OldInterpreterExecState* exec, const Identifier& ident)
{
    // Static lookup in EvalCode is impossible because variables aren't DontDelete.
    // Static lookup in GlobalCode may be possible, but we haven't implemented support for it yet.
    if (exec->codeType() != FunctionCode)
        return false;

    // Static lookup is impossible when something dynamic has been added to the front of the scope chain.
    if (exec->variableObject() != exec->scopeChain().top())
        return false;

    // Static lookup is impossible if the symbol isn't statically declared.
    if (!exec->variableObject()->symbolTable().contains(ident.ustring().rep()))
        return false;

    return true;
}
#endif

static inline bool isConstant(const LocalStorage& localStorage, size_t index)
{
    ASSERT(index < localStorage.size());
    return localStorage[index].attributes & ReadOnly;
}

static inline UString::Rep* rep(const Identifier& ident)
{
    return ident.ustring().rep();
}

// ------------------------------ Node -----------------------------------------

#ifndef NDEBUG
#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX Log
#endif
static WTFLogChannel LogKJSNodeLeaks = { 0x00000000, "", WTFLogChannelOn };

struct ParserRefCountedCounter {
    static unsigned count;
    ParserRefCountedCounter()
    {
        if (count)
            LOG(KJSNodeLeaks, "LEAK: %u KJS::Node\n", count);
    }
};
unsigned ParserRefCountedCounter::count = 0;
static ParserRefCountedCounter parserRefCountedCounter;
#endif

static HashSet<ParserRefCounted*>* newTrackedObjects;
static HashCountedSet<ParserRefCounted*>* trackedObjectExtraRefCounts;

ParserRefCounted::ParserRefCounted()
{
#ifndef NDEBUG
    ++ParserRefCountedCounter::count;
#endif
    if (!newTrackedObjects)
        newTrackedObjects = new HashSet<ParserRefCounted*>;
    newTrackedObjects->add(this);
    ASSERT(newTrackedObjects->contains(this));
}

ParserRefCounted::~ParserRefCounted()
{
#ifndef NDEBUG
    --ParserRefCountedCounter::count;
#endif
}

void ParserRefCounted::ref()
{
    // bumping from 0 to 1 is just removing from the new nodes set
    if (newTrackedObjects) {
        HashSet<ParserRefCounted*>::iterator it = newTrackedObjects->find(this);
        if (it != newTrackedObjects->end()) {
            newTrackedObjects->remove(it);
            ASSERT(!trackedObjectExtraRefCounts || !trackedObjectExtraRefCounts->contains(this));
            return;
        }
    }

    ASSERT(!newTrackedObjects || !newTrackedObjects->contains(this));

    if (!trackedObjectExtraRefCounts)
        trackedObjectExtraRefCounts = new HashCountedSet<ParserRefCounted*>;
    trackedObjectExtraRefCounts->add(this);
}

void ParserRefCounted::deref()
{
    ASSERT(!newTrackedObjects || !newTrackedObjects->contains(this));

    if (!trackedObjectExtraRefCounts) {
        delete this;
        return;
    }

    HashCountedSet<ParserRefCounted*>::iterator it = trackedObjectExtraRefCounts->find(this);
    if (it == trackedObjectExtraRefCounts->end())
        delete this;
    else
        trackedObjectExtraRefCounts->remove(it);
}

unsigned ParserRefCounted::refcount()
{
    if (newTrackedObjects && newTrackedObjects->contains(this)) {
        ASSERT(!trackedObjectExtraRefCounts || !trackedObjectExtraRefCounts->contains(this));
        return 0;
    }

    ASSERT(!newTrackedObjects || !newTrackedObjects->contains(this));

    if (!trackedObjectExtraRefCounts)
        return 1;

    return 1 + trackedObjectExtraRefCounts->count(this);
}

void ParserRefCounted::deleteNewObjects()
{
    if (!newTrackedObjects)
        return;

#ifndef NDEBUG
    HashSet<ParserRefCounted*>::iterator end = newTrackedObjects->end();
    for (HashSet<ParserRefCounted*>::iterator it = newTrackedObjects->begin(); it != end; ++it)
        ASSERT(!trackedObjectExtraRefCounts || !trackedObjectExtraRefCounts->contains(*it));
#endif
    deleteAllValues(*newTrackedObjects);
    delete newTrackedObjects;
    newTrackedObjects = 0;
}

Node::Node()
    : m_expectedReturnType(ObjectType)
{
    m_line = lexer().lineNo();
}

Node::Node(JSType expectedReturn)
    : m_expectedReturnType(expectedReturn)
{
    m_line = lexer().lineNo();
}

double ExpressionNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return value->toNumber(exec);
}

bool ExpressionNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return value->toBoolean(exec);
}

int32_t ExpressionNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return value->toInt32(exec);
}

uint32_t ExpressionNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return value->toUInt32(exec);
}

static void substitute(UString& string, const UString& substring) KJS_FAST_CALL;
static void substitute(UString& string, const UString& substring)
{
    int position = string.find("%s");
    ASSERT(position != -1);
    UString newString = string.substr(0, position);
    newString.append(substring);
    newString.append(string.substr(position + 2));
    string = newString;
}

static inline int currentSourceId(ExecState* exec) KJS_FAST_CALL;
static inline int currentSourceId(ExecState*)
{
    ASSERT_NOT_REACHED();
    return 0;
}

static inline const UString currentSourceURL(ExecState* exec) KJS_FAST_CALL;
static inline const UString currentSourceURL(ExecState*)
{
    ASSERT_NOT_REACHED();
    return UString();
}

JSValue* Node::setErrorCompletion(OldInterpreterExecState* exec, ErrorType e, const char* msg)
{
    return exec->setThrowCompletion(Error::create(exec, e, msg, lineNo(), currentSourceId(exec), currentSourceURL(exec)));
}

JSValue* Node::setErrorCompletion(OldInterpreterExecState* exec, ErrorType e, const char* msg, const Identifier& ident)
{
    UString message = msg;
    substitute(message, ident.ustring());
    return exec->setThrowCompletion(Error::create(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec)));
}

JSValue* Node::throwError(OldInterpreterExecState* exec, ErrorType e, const char* msg)
{
    return KJS::throwError(exec, e, msg, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue* Node::throwError(OldInterpreterExecState* exec, ErrorType e, const char* msg, const char* string)
{
    UString message = msg;
    substitute(message, string);
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue* Node::throwError(OldInterpreterExecState* exec, ErrorType e, const char* msg, JSValue* v, Node* expr)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue* Node::throwError(OldInterpreterExecState* exec, ErrorType e, const char* msg, const Identifier& label)
{
    UString message = msg;
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue* Node::throwError(OldInterpreterExecState* exec, ErrorType e, const char* msg, JSValue* v, Node* e1, Node* e2)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, e1->toString());
    substitute(message, e2->toString());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue* Node::throwError(OldInterpreterExecState* exec, ErrorType e, const char* msg, JSValue* v, Node* expr, const Identifier& label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue* Node::throwError(OldInterpreterExecState* exec, ErrorType e, const char* msg, JSValue* v, const Identifier& label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue* Node::throwUndefinedVariableError(OldInterpreterExecState* exec, const Identifier& ident)
{
    return throwError(exec, ReferenceError, "Can't find variable: %s", ident);
}

void Node::handleException(OldInterpreterExecState* exec)
{
    handleException(exec, exec->exception());
}

void Node::handleException(OldInterpreterExecState* exec, JSValue* exceptionValue)
{
    if (exceptionValue->isObject()) {
        JSObject* exception = static_cast<JSObject*>(exceptionValue);
        if (!exception->hasProperty(exec, "line") && !exception->hasProperty(exec, "sourceURL")) {
            exception->put(exec, "line", jsNumber(m_line));
            exception->put(exec, "sourceURL", jsString(currentSourceURL(exec)));
        }
    }
#if 0
    Debugger* dbg = exec->dynamicGlobalObject()->debugger();
    if (dbg && !dbg->hasHandledException(exec, exceptionValue))
        dbg->exception(exec, currentSourceId(exec), m_line, exceptionValue);
#endif
}

NEVER_INLINE JSValue* Node::rethrowException(OldInterpreterExecState* exec)
{
    JSValue* exception = exec->exception();
    exec->clearException();
    handleException(exec, exception);
    return exec->setThrowCompletion(exception);
}

RegisterID* Node::emitThrowError(CodeGenerator& generator, ErrorType e, const char* msg)
{
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(msg));
    generator.emitThrow(exception);
    return exception;
}

RegisterID* Node::emitThrowError(CodeGenerator& generator, ErrorType e, const char* msg, const Identifier& label)
{
    UString message = msg;
    substitute(message, label.ustring());
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(message));
    generator.emitThrow(exception);
    return exception;
}
    
// ------------------------------ StatementNode --------------------------------

StatementNode::StatementNode()
    : m_lastLine(-1)
{
    m_line = -1;
}

void StatementNode::setLoc(int firstLine, int lastLine)
{
    m_line = firstLine;
    m_lastLine = lastLine;
}

// ------------------------------ SourceElements --------------------------------

void SourceElements::append(PassRefPtr<StatementNode> statement)
{
    if (statement->isEmptyStatement())
        return;

    m_statements.append(statement);
}

// ------------------------------ BreakpointCheckStatement --------------------------------

BreakpointCheckStatement::BreakpointCheckStatement(PassRefPtr<StatementNode> statement)
    : m_statement(statement)
{
    ASSERT(m_statement);
}

JSValue* BreakpointCheckStatement::execute(OldInterpreterExecState* exec)
{
    return m_statement->execute(exec);
}

void BreakpointCheckStatement::streamTo(SourceStream& stream) const
{
    m_statement->streamTo(stream);
}

void BreakpointCheckStatement::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_statement.get());
}

// ------------------------------ NullNode -------------------------------------

RegisterID* NullNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitLoad(generator.finalDestination(dst), jsNull());
}

JSValue* NullNode::evaluate(OldInterpreterExecState* )
{
    return jsNull();
}

// ------------------------------ FalseNode ----------------------------------

RegisterID* FalseNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitLoad(generator.finalDestination(dst), false);
}

JSValue* FalseNode::evaluate(OldInterpreterExecState*)
{
    return jsBoolean(false);
}

// ------------------------------ TrueNode ----------------------------------

RegisterID* TrueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitLoad(generator.finalDestination(dst), true);
}

JSValue* TrueNode::evaluate(OldInterpreterExecState*)
{
    return jsBoolean(true);
}

// ------------------------------ NumberNode -----------------------------------

RegisterID* NumberNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitLoad(generator.finalDestination(dst), m_double);
}

JSValue* NumberNode::evaluate(OldInterpreterExecState*)
{
    // Number nodes are only created when the number can't fit in a JSImmediate, so no need to check again.
    return jsNumberCell(m_double);
}

double NumberNode::evaluateToNumber(OldInterpreterExecState*)
{
    return m_double;
}

bool NumberNode::evaluateToBoolean(OldInterpreterExecState*)
{
    return m_double < 0.0 || m_double > 0.0; // false for NaN as well as 0
}

int32_t NumberNode::evaluateToInt32(OldInterpreterExecState*)
{
    return JSValue::toInt32(m_double);
}

uint32_t NumberNode::evaluateToUInt32(OldInterpreterExecState*)
{
    return JSValue::toUInt32(m_double);
}

// ------------------------------ ImmediateNumberNode -----------------------------------

JSValue* ImmediateNumberNode::evaluate(OldInterpreterExecState*)
{
    return m_value;
}

int32_t ImmediateNumberNode::evaluateToInt32(OldInterpreterExecState*)
{
    return JSImmediate::getTruncatedInt32(m_value);
}

uint32_t ImmediateNumberNode::evaluateToUInt32(OldInterpreterExecState*)
{
    uint32_t i;
    if (JSImmediate::getTruncatedUInt32(m_value, i))
        return i;
    bool ok;
    return JSValue::toUInt32SlowCase(m_double, ok);
}

// ------------------------------ StringNode -----------------------------------

RegisterID* StringNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    // FIXME: should we try to atomize constant strings?
    return generator.emitLoad(generator.finalDestination(dst), jsOwnedString(m_value));
}

JSValue* StringNode::evaluate(OldInterpreterExecState*)
{
    return jsOwnedString(m_value);
}

double StringNode::evaluateToNumber(OldInterpreterExecState*)
{
    return m_value.toDouble();
}

bool StringNode::evaluateToBoolean(OldInterpreterExecState*)
{
    return !m_value.isEmpty();
}

// ------------------------------ RegExpNode -----------------------------------

RegisterID* RegExpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (!m_regExp->isValid())
        return emitThrowError(generator, SyntaxError, "Invalid regular expression: %s", m_regExp->errorMessage());
    return generator.emitNewRegExp(generator.finalDestination(dst), m_regExp.get());
}

JSValue* RegExpNode::evaluate(OldInterpreterExecState*)
{
    ASSERT_NOT_REACHED();
    return 0;
}

// ------------------------------ ThisNode -------------------------------------

RegisterID* ThisNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.moveToDestinationIfNeeded(dst, generator.thisRegister());
}

// ECMA 11.1.1
JSValue* ThisNode::evaluate(OldInterpreterExecState* exec)
{
    return exec->thisValue();
}

// ------------------------------ ResolveNode ----------------------------------

RegisterID* ResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident))
        return generator.moveToDestinationIfNeeded(dst, local);

    return generator.emitResolve(generator.finalDestination(dst), m_ident);
}

// ECMA 11.1.2 & 10.1.4
JSValue* ResolveNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    // Check for missed optimization opportunity.
    ASSERT(!canSkipLookup(exec, m_ident));

    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    do {
        JSObject* o = *iter;

        if (o->getPropertySlot(exec, m_ident, slot))
            return slot.getValue(exec, o, m_ident);

        ++iter;
    } while (iter != end);

    return throwUndefinedVariableError(exec, m_ident);
}

JSValue* ResolveNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double ResolveNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool ResolveNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t ResolveNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t ResolveNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

static size_t getSymbolTableEntry(OldInterpreterExecState* exec, const Identifier& ident, const SymbolTable& symbolTable, size_t& stackDepth) 
{
    int index = symbolTable.get(ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker()) {
        stackDepth = 0;
        return index;
    }
    
    if (ident == exec->propertyNames().arguments) {
        stackDepth = 0;
        return missingSymbolMarker();
    }
    
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();
    size_t depth = 0;
    for (; iter != end; ++iter, ++depth) {
        JSObject* currentScope = *iter;
        if (!currentScope->isVariableObject()) 
            break;
        JSVariableObject* currentVariableObject = static_cast<JSVariableObject*>(currentScope);
        index = currentVariableObject->symbolTable().get(ident.ustring().rep()).getIndex();
        if (index != missingSymbolMarker()) {
            stackDepth = depth;
            return index;
        }
        if (currentVariableObject->isDynamicScope())
            break;
    }
    stackDepth = depth;
    return missingSymbolMarker();
}

void ResolveNode::optimizeVariableAccess(OldInterpreterExecState* exec, const SymbolTable& symbolTable, const LocalStorage&, NodeStack&)
{
    size_t depth = 0;
    int index = getSymbolTableEntry(exec, m_ident, symbolTable, depth);
    if (index != missingSymbolMarker()) {
        if (!depth)
            new (this) LocalVarAccessNode(index);
        else
            new (this) ScopedVarAccessNode(index, depth);
        return;
    }

    if (!depth)
        return;

    new (this) NonLocalVarAccessNode(depth);
}

JSValue* LocalVarAccessNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    return exec->localStorage()[m_index].value;
}

JSValue* LocalVarAccessNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double LocalVarAccessNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toNumber(exec);
}

bool LocalVarAccessNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toBoolean(exec);
}

int32_t LocalVarAccessNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toInt32(exec);
}

uint32_t LocalVarAccessNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toUInt32(exec);
}

static inline JSValue* getNonLocalSymbol(OldInterpreterExecState* exec, size_t, size_t scopeDepth)
{
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    for (size_t i = 0; i < scopeDepth; ++iter, ++i)
        ASSERT(iter != chain.end());
#ifndef NDEBUG
    JSObject* scope = *iter;
#endif
    ASSERT(scope->isVariableObject());
    ASSERT_NOT_REACHED();
    return 0;
}

JSValue* ScopedVarAccessNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    return getNonLocalSymbol(exec, m_index, m_scopeDepth);
}

JSValue* ScopedVarAccessNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double ScopedVarAccessNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toNumber(exec);
}

bool ScopedVarAccessNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toBoolean(exec);
}

int32_t ScopedVarAccessNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toInt32(exec);
}

uint32_t ScopedVarAccessNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toUInt32(exec);
}

JSValue* NonLocalVarAccessNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    // Check for missed optimization opportunity.
    ASSERT(!canSkipLookup(exec, m_ident));
    
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();
    for (size_t i = 0; i < m_scopeDepth; ++i, ++iter)
        ASSERT(iter != end);

    // we must always have something in the scope chain
    ASSERT(iter != end);
    
    PropertySlot slot;
    do {
        JSObject* o = *iter;
        
        if (o->getPropertySlot(exec, m_ident, slot))
            return slot.getValue(exec, o, m_ident);
        
        ++iter;
    } while (iter != end);
    
    return throwUndefinedVariableError(exec, m_ident);
}

JSValue* NonLocalVarAccessNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double NonLocalVarAccessNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toNumber(exec);
}

bool NonLocalVarAccessNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toBoolean(exec);
}

int32_t NonLocalVarAccessNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toInt32(exec);
}

uint32_t NonLocalVarAccessNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec)->toUInt32(exec);
}

// ------------------------------ ElementNode ----------------------------------

void ElementNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_next)
        nodeStack.append(m_next.get());
    ASSERT(m_node);
    nodeStack.append(m_node.get());
}

// ECMA 11.1.4
JSValue* ElementNode::evaluate(OldInterpreterExecState* exec)
{
    JSObject* array = exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, exec->emptyList());
    int length = 0;
    for (ElementNode* n = this; n; n = n->m_next.get()) {
        JSValue* val = n->m_node->evaluate(exec);
        KJS_CHECKEXCEPTIONVALUE
        length += n->m_elision;
        array->put(exec, length++, val);
    }
    return array;
}

// ------------------------------ ArrayNode ------------------------------------


RegisterID* ArrayNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newArray = generator.emitNewArray(generator.tempDestination(dst));
    unsigned length = 0;

    RegisterID* value;
    for (ElementNode* n = m_element.get(); n; n = n->m_next.get()) {
        value = generator.emitNode(n->m_node.get());
        length += n->m_elision;
        generator.emitPutByIndex(newArray.get(), length++, value);
    }

    value = generator.emitLoad(generator.newTemporary(), jsNumber(m_elision + length));
    generator.emitPutById(newArray.get(), generator.propertyNames().length, value);

    return generator.moveToDestinationIfNeeded(dst, newArray.get());
}

void ArrayNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_element)
        nodeStack.append(m_element.get());
}

// ECMA 11.1.4
JSValue* ArrayNode::evaluate(OldInterpreterExecState* exec)
{
    JSObject* array;
    int length;

    if (m_element) {
        array = static_cast<JSObject*>(m_element->evaluate(exec));
        KJS_CHECKEXCEPTIONVALUE
        length = m_optional ? array->get(exec, exec->propertyNames().length)->toInt32(exec) : 0;
    } else {
        JSValue* newArr = exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, exec->emptyList());
        array = static_cast<JSObject*>(newArr);
        length = 0;
    }

    if (m_optional)
        array->put(exec, exec->propertyNames().length, jsNumber(m_elision + length));

    return array;
}

// ------------------------------ ObjectLiteralNode ----------------------------

RegisterID* ObjectLiteralNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (m_list)
        return generator.emitNode(dst, m_list.get());
    else
        return generator.emitNewObject(generator.finalDestination(dst));
}

void ObjectLiteralNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_list)
        nodeStack.append(m_list.get());
}

// ECMA 11.1.5
JSValue* ObjectLiteralNode::evaluate(OldInterpreterExecState* exec)
{
    if (m_list)
        return m_list->evaluate(exec);

    return exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());
}

// ------------------------------ PropertyListNode -----------------------------

RegisterID* PropertyListNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newObj = generator.tempDestination(dst);
    
    generator.emitNewObject(newObj.get());
    
    for (PropertyListNode* p = this; p; p = p->m_next.get()) {
        RegisterID* value = generator.emitNode(p->m_node->m_assign.get());
        
        switch (p->m_node->m_type) {
            case PropertyNode::Constant: {
                generator.emitPutById(newObj.get(), p->m_node->name(), value);
                break;
            }
            case PropertyNode::Getter: {
                generator.emitPutGetter(newObj.get(), p->m_node->name(), value);
                break;
            }
            case PropertyNode::Setter: {
                generator.emitPutSetter(newObj.get(), p->m_node->name(), value);
                break;
            }
            default:
                ASSERT_NOT_REACHED();
        }
    }
    
    return generator.moveToDestinationIfNeeded(dst, newObj.get());
}

void PropertyListNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_next)
        nodeStack.append(m_next.get());
    nodeStack.append(m_node.get());
}

// ECMA 11.1.5
JSValue* PropertyListNode::evaluate(OldInterpreterExecState* exec)
{
    JSObject* obj = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());

    for (PropertyListNode* p = this; p; p = p->m_next.get()) {
        JSValue* v = p->m_node->m_assign->evaluate(exec);
        KJS_CHECKEXCEPTIONVALUE

        switch (p->m_node->m_type) {
            case PropertyNode::Getter:
                ASSERT(v->isObject());
                obj->defineGetter(exec, p->m_node->name(), static_cast<JSObject* >(v));
                break;
            case PropertyNode::Setter:
                ASSERT(v->isObject());
                obj->defineSetter(exec, p->m_node->name(), static_cast<JSObject* >(v));
                break;
            case PropertyNode::Constant:
                obj->put(exec, p->m_node->name(), v);
                break;
        }
    }

    return obj;
}

// ------------------------------ PropertyNode -----------------------------

void PropertyNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_assign.get());
}

// ECMA 11.1.5
JSValue* PropertyNode::evaluate(OldInterpreterExecState*)
{
    ASSERT(false);
    return jsNull();
}

// ------------------------------ BracketAccessorNode --------------------------------

RegisterID* BracketAccessorNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RegisterID* property = generator.emitNode(m_subscript.get());

    return generator.emitGetByVal(generator.finalDestination(dst), base.get(), property);
}

void BracketAccessorNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

// ECMA 11.2.1a
JSValue* BracketAccessorNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSObject* o = v1->toObject(exec);
    uint32_t i;
    if (v2->getUInt32(i))
        return o->get(exec, i);
    return o->get(exec, Identifier(v2->toString(exec)));
}

JSValue* BracketAccessorNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double BracketAccessorNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool BracketAccessorNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t BracketAccessorNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t BracketAccessorNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

// ------------------------------ DotAccessorNode --------------------------------

RegisterID* DotAccessorNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* base = generator.emitNode(m_base.get());
    return generator.emitGetById(generator.finalDestination(dst), base, m_ident);
}

void DotAccessorNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_base.get());
}

// ECMA 11.2.1b
JSValue* DotAccessorNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    JSValue* v = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return v->toObject(exec)->get(exec, m_ident);
}

JSValue* DotAccessorNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double DotAccessorNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool DotAccessorNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t DotAccessorNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t DotAccessorNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

// ------------------------------ ArgumentListNode -----------------------------

RegisterID* ArgumentListNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    return generator.emitNode(dst, m_expr.get());
}

void ArgumentListNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_next)
        nodeStack.append(m_next.get());
    ASSERT(m_expr);
    nodeStack.append(m_expr.get());
}

// ECMA 11.2.4
void ArgumentListNode::evaluateList(OldInterpreterExecState* exec, List& list)
{
    for (ArgumentListNode* n = this; n; n = n->m_next.get()) {
        JSValue* v = n->m_expr->evaluate(exec);
        KJS_CHECKEXCEPTIONVOID
        list.append(v);
    }
}

// ------------------------------ ArgumentsNode --------------------------------

void ArgumentsNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_listNode)
        nodeStack.append(m_listNode.get());
}

// ------------------------------ NewExprNode ----------------------------------

RegisterID* NewExprNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    return generator.emitConstruct(generator.finalDestination(dst), r0.get(), m_args.get());
}

void NewExprNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_args)
        nodeStack.append(m_args.get());
    nodeStack.append(m_expr.get());
}

// ECMA 11.2.2

JSValue* NewExprNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    JSValue* v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    List argList;
    if (m_args) {
        m_args->evaluateList(exec, argList);
        KJS_CHECKEXCEPTIONVALUE
    }

    if (!v->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v, m_expr.get());

    ConstructData constructData;
    if (v->getConstructData(constructData) == ConstructTypeNone)
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v, m_expr.get());

    return static_cast<JSObject*>(v)->construct(exec, argList);
}

JSValue* NewExprNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double NewExprNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool NewExprNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t NewExprNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t NewExprNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

template <ExpressionNode::CallerType callerType, bool scopeDepthIsZero> 
inline JSValue* ExpressionNode::resolveAndCall(OldInterpreterExecState* exec, const Identifier& ident, ArgumentsNode* args, size_t scopeDepth)
{
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    if (!scopeDepthIsZero) {
        for (size_t i = 0; i < scopeDepth; ++iter, ++i)
            ASSERT(iter != chain.end());
    }
    
    // we must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot)) {
            JSValue* v = slot.getValue(exec, base, ident);
            KJS_CHECKEXCEPTIONVALUE

            if (!v->isObject())
                return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident);

            JSObject* func = static_cast<JSObject*>(v);

            if (!func->implementsCall())
                return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident);

            List argList;
            args->evaluateList(exec, argList);
            KJS_CHECKEXCEPTIONVALUE

            if (callerType == EvalOperator) {
                if (base == exec->lexicalGlobalObject() && func == exec->lexicalGlobalObject()->evalFunction()) {
                    ASSERT_NOT_REACHED();
                }
            }

            JSObject* thisObj = base->toThisObject(exec);
            return func->call(exec, thisObj, argList);
        }
        ++iter;
    } while (iter != end);

    return throwUndefinedVariableError(exec, ident);
}

RegisterID* EvalFunctionCallNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.tempDestination(dst);
    RegisterID* func = generator.newTemporary();
    generator.emitResolveWithBase(base.get(), func, CommonIdentifiers::shared()->eval);
    return generator.emitCallEval(generator.finalDestination(dst, base.get()), func, base.get(), m_args.get());
}

void EvalFunctionCallNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_args.get());
}

JSValue* EvalFunctionCallNode::evaluate(OldInterpreterExecState* exec)
{
    return resolveAndCall<EvalOperator, true>(exec, exec->propertyNames().eval, m_args.get());
}

RegisterID* FunctionCallValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* r0 = generator.emitNode(m_expr.get());
    return generator.emitCall(generator.finalDestination(dst), r0, 0, m_args.get());
}

void FunctionCallValueNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_args.get());
    nodeStack.append(m_expr.get());
}

// ECMA 11.2.3
JSValue* FunctionCallValueNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    if (!v->isObject()) {
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, m_expr.get());
    }

    JSObject* func = static_cast<JSObject*>(v);

    if (!func->implementsCall()) {
        return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, m_expr.get());
    }

    List argList;
    m_args->evaluateList(exec, argList);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* thisObj = exec->globalThisValue();
    return func->call(exec, thisObj, argList);
}

RegisterID* FunctionCallResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident))
        return generator.emitCall(generator.finalDestination(dst), local, 0, m_args.get());

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RegisterID* func = generator.emitGetScopedVar(generator.newTemporary(), depth, index);
        return generator.emitCall(generator.finalDestination(dst), func, 0, m_args.get());
    }

    RefPtr<RegisterID> base = generator.tempDestination(dst);
    RegisterID* func = generator.newTemporary();
    generator.emitResolveFunction(base.get(), func, m_ident);
    return generator.emitCall(generator.finalDestination(dst, base.get()), func, base.get(), m_args.get());
}

void FunctionCallResolveNode::optimizeVariableAccess(OldInterpreterExecState* exec, const SymbolTable& symbolTable, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_args.get());

    size_t depth = 0;
    int index = getSymbolTableEntry(exec, m_ident, symbolTable, depth);
    if (index != missingSymbolMarker()) {
        if (!depth)
            new (this) LocalVarFunctionCallNode(index);
        else
            new (this) ScopedVarFunctionCallNode(index, depth);
        return;
    }
    
    if (!depth)
        return;
    
    new (this) NonLocalVarFunctionCallNode(depth);
}

// ECMA 11.2.3
JSValue* FunctionCallResolveNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    // Check for missed optimization opportunity.
    ASSERT(!canSkipLookup(exec, m_ident));

    return resolveAndCall<FunctionCall, true>(exec, m_ident, m_args.get());
}

JSValue* FunctionCallResolveNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double FunctionCallResolveNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool FunctionCallResolveNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t FunctionCallResolveNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t FunctionCallResolveNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

JSValue* LocalVarFunctionCallNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    JSValue* v = exec->localStorage()[m_index].value;

    if (!v->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, m_ident);

    JSObject* func = static_cast<JSObject*>(v);
    if (!func->implementsCall())
        return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, m_ident);

    List argList;
    m_args->evaluateList(exec, argList);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* thisObj = exec->globalThisValue();
    return func->call(exec, thisObj, argList);
}

JSValue* LocalVarFunctionCallNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double LocalVarFunctionCallNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool LocalVarFunctionCallNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t LocalVarFunctionCallNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t LocalVarFunctionCallNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

JSValue* ScopedVarFunctionCallNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    
    JSValue* v = getNonLocalSymbol(exec, m_index, m_scopeDepth);
    
    if (!v->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, m_ident);
    
    JSObject* func = static_cast<JSObject*>(v);
    if (!func->implementsCall())
        return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, m_ident);
    
    List argList;
    m_args->evaluateList(exec, argList);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* thisObj = exec->globalThisValue();
    return func->call(exec, thisObj, argList);
}

JSValue* ScopedVarFunctionCallNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double ScopedVarFunctionCallNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool ScopedVarFunctionCallNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t ScopedVarFunctionCallNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t ScopedVarFunctionCallNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

JSValue* NonLocalVarFunctionCallNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    // Check for missed optimization opportunity.
    ASSERT(!canSkipLookup(exec, m_ident));
    
    return resolveAndCall<FunctionCall, false>(exec, m_ident, m_args.get(), m_scopeDepth);
}

JSValue* NonLocalVarFunctionCallNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double NonLocalVarFunctionCallNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool NonLocalVarFunctionCallNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t NonLocalVarFunctionCallNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t NonLocalVarFunctionCallNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

RegisterID* FunctionCallBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RegisterID* property = generator.emitNode(m_subscript.get());
    RegisterID* function = generator.emitGetByVal(generator.newTemporary(), base.get(), property);
    return generator.emitCall(generator.finalDestination(dst, base.get()), function, base.get(), m_args.get());
}

void FunctionCallBracketNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_args.get());
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

// ECMA 11.2.3
JSValue* FunctionCallBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseVal = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSValue* subscriptVal = m_subscript->evaluate(exec);

    JSObject* baseObj = baseVal->toObject(exec);
    uint32_t i;
    PropertySlot slot;

    JSValue* funcVal;
    if (subscriptVal->getUInt32(i)) {
        if (baseObj->getPropertySlot(exec, i, slot))
            funcVal = slot.getValue(exec, baseObj, i);
        else
            funcVal = jsUndefined();
    } else {
        Identifier ident(subscriptVal->toString(exec));
        if (baseObj->getPropertySlot(exec, ident, slot))
            funcVal = baseObj->get(exec, ident);
        else
            funcVal = jsUndefined();
    }

    KJS_CHECKEXCEPTIONVALUE

    if (!funcVal->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s[%s]) is not object.", funcVal, m_base.get(), m_subscript.get());

    JSObject* func = static_cast<JSObject*>(funcVal);

    if (!func->implementsCall())
        return throwError(exec, TypeError, "Object %s (result of expression %s[%s]) does not allow calls.", funcVal, m_base.get(), m_subscript.get());

    List argList;
    m_args->evaluateList(exec, argList);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* thisObj = baseObj;
    ASSERT(thisObj);
    ASSERT(thisObj->isObject());
    ASSERT(!thisObj->isActivationObject());

    // No need to call toThisObject() on the thisObj as it is known not to be the GlobalObject or ActivationObject
    return func->call(exec, thisObj, argList);
}

static const char* dotExprNotAnObjectString() KJS_FAST_CALL;
static const char* dotExprNotAnObjectString()
{
    return "Value %s (result of expression %s.%s) is not object.";
}

static const char* dotExprDoesNotAllowCallsString() KJS_FAST_CALL;
static const char* dotExprDoesNotAllowCallsString()
{
    return "Object %s (result of expression %s.%s) does not allow calls.";
}

RegisterID* FunctionCallDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RegisterID* function = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    return generator.emitCall(generator.finalDestination(dst, base.get()), function, base.get(), m_args.get());
}

void FunctionCallDotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_args.get());
    nodeStack.append(m_base.get());
}

// ECMA 11.2.3
JSValue* FunctionCallDotNode::inlineEvaluate(OldInterpreterExecState* exec)
{
    JSValue* baseVal = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* baseObj = baseVal->toObject(exec);
    PropertySlot slot;
    JSValue* funcVal = baseObj->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, baseObj, m_ident) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    if (!funcVal->isObject())
        return throwError(exec, TypeError, dotExprNotAnObjectString(), funcVal, m_base.get(), m_ident);

    JSObject* func = static_cast<JSObject*>(funcVal);

    if (!func->implementsCall())
        return throwError(exec, TypeError, dotExprDoesNotAllowCallsString(), funcVal, m_base.get(), m_ident);

    List argList;
    m_args->evaluateList(exec, argList);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* thisObj = baseObj;
    ASSERT(thisObj);
    ASSERT(thisObj->isObject());
    ASSERT(!thisObj->isActivationObject());

    // No need to call toThisObject() on the thisObj as it is known not to be the GlobalObject or ActivationObject
    return func->call(exec, thisObj, argList);
}

JSValue* FunctionCallDotNode::evaluate(OldInterpreterExecState* exec)
{
    return inlineEvaluate(exec);
}

double FunctionCallDotNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool FunctionCallDotNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t FunctionCallDotNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t FunctionCallDotNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

// ECMA 11.3

// ------------------------------ PostfixResolveNode ----------------------------------

RegisterID* PostIncResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    // FIXME: I think we can detect the absense of dependent expressions here, 
    // and emit a PreInc instead of a PostInc. A post-pass to eliminate dead
    // code would work, too.
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident))
            return generator.emitToJSNumber(generator.finalDestination(dst), local);
        
        return generator.emitPostInc(generator.finalDestination(dst), local);
    }

    RefPtr<RegisterID> value = generator.newTemporary();
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), value.get(), m_ident);
    RegisterID* oldValue = generator.emitPostInc(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

// Increment
void PostIncResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage& localStorage, NodeStack&)
{
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker()) {
        if (isConstant(localStorage, index))
            new (this) PostIncConstNode(index);
        else
            new (this) PostIncLocalVarNode(index);
    }
}

JSValue* PostIncResolveNode::evaluate(OldInterpreterExecState* exec)
{
    // Check for missed optimization opportunity.
    ASSERT(!canSkipLookup(exec, m_ident));

    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    do {
        if ((*iter)->getPropertySlot(exec, m_ident, slot)) {
            // If m_ident is 'arguments', the base->getPropertySlot() may cause 
            // base (which must be an ActivationImp in such this case) to be torn
            // off from the activation stack, in which case we need to get it again
            // from the ScopeChainIterator.
            
            JSObject* base = *iter;
            JSValue* v = slot.getValue(exec, base, m_ident)->toJSNumber(exec);
            base->put(exec, m_ident, jsNumber(v->toNumber(exec) + 1));
            return v;
        }

        ++iter;
    } while (iter != end);

    return throwUndefinedVariableError(exec, m_ident);
}

void PostIncResolveNode::optimizeForUnnecessaryResult()
{
    new (this) PreIncResolveNode(PlacementNewAdopt);
}

JSValue* PostIncLocalVarNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    JSValue** slot = &exec->localStorage()[m_index].value;
    JSValue* v = (*slot)->toJSNumber(exec);
    *slot = jsNumber(v->toNumber(exec) + 1);
    return v;
}

void PostIncLocalVarNode::optimizeForUnnecessaryResult()
{
    new (this) PreIncLocalVarNode(m_index);
}

// Decrement
RegisterID* PostDecResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    // FIXME: I think we can detect the absense of dependent expressions here, 
    // and emit a PreDec instead of a PostDec. A post-pass to eliminate dead
    // code would work, too.
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident))
            return generator.emitToJSNumber(generator.finalDestination(dst), local);
        
        return generator.emitPostDec(generator.finalDestination(dst), local);
    }

    RefPtr<RegisterID> value = generator.newTemporary();
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), value.get(), m_ident);
    RegisterID* oldValue = generator.emitPostDec(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

void PostDecResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage& localStorage, NodeStack&)
{
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker()) {
        if (isConstant(localStorage, index))
            new (this) PostDecConstNode(index);
        else
            new (this) PostDecLocalVarNode(index);
    }
}

JSValue* PostDecResolveNode::evaluate(OldInterpreterExecState* exec)
{
    // Check for missed optimization opportunity.
    ASSERT(!canSkipLookup(exec, m_ident));

    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    do {
        if ((*iter)->getPropertySlot(exec, m_ident, slot)) {
            // See the comment in PostIncResolveNode::evaluate().

            JSObject* base = *iter;
            JSValue* v = slot.getValue(exec, base, m_ident)->toJSNumber(exec);
            base->put(exec, m_ident, jsNumber(v->toNumber(exec) - 1));
            return v;
        }

        ++iter;
    } while (iter != end);

    return throwUndefinedVariableError(exec, m_ident);
}

void PostDecResolveNode::optimizeForUnnecessaryResult()
{
    new (this) PreDecResolveNode(PlacementNewAdopt);
}

JSValue* PostDecLocalVarNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    JSValue** slot = &exec->localStorage()[m_index].value;
    JSValue* v = (*slot)->toJSNumber(exec);
    *slot = jsNumber(v->toNumber(exec) - 1);
    return v;
}

double PostDecLocalVarNode::inlineEvaluateToNumber(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    JSValue** slot = &exec->localStorage()[m_index].value;
    double n = (*slot)->toNumber(exec);
    *slot = jsNumber(n - 1);
    return n;
}

double PostDecLocalVarNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

bool PostDecLocalVarNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    double result = inlineEvaluateToNumber(exec);
    return  result > 0.0 || 0.0 > result; // NaN produces false as well
}

int32_t PostDecLocalVarNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t PostDecLocalVarNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

void PostDecLocalVarNode::optimizeForUnnecessaryResult()
{
    new (this) PreDecLocalVarNode(m_index);
}

// ------------------------------ PostfixBracketNode ----------------------------------

void PostfixBracketNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

RegisterID* PostIncBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue = generator.emitPostInc(generator.finalDestination(dst), value.get());
    generator.emitPutByVal(base.get(), property.get(), value.get());
    return oldValue;
}

JSValue* PostIncBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* subscript = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* base = baseValue->toObject(exec);

    uint32_t propertyIndex;
    if (subscript->getUInt32(propertyIndex)) {
        PropertySlot slot;
        JSValue* v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
        KJS_CHECKEXCEPTIONVALUE

        JSValue* v2 = v->toJSNumber(exec);
        base->put(exec, propertyIndex, jsNumber(v2->toNumber(exec) + 1));

        return v2;
    }

    Identifier propertyName(subscript->toString(exec));
    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = v->toJSNumber(exec);
    base->put(exec, propertyName, jsNumber(v2->toNumber(exec) + 1));
    return v2;
}

RegisterID* PostDecBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue = generator.emitPostDec(generator.finalDestination(dst), value.get());
    generator.emitPutByVal(base.get(), property.get(), value.get());
    return oldValue;
}

JSValue* PostDecBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* subscript = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* base = baseValue->toObject(exec);

    uint32_t propertyIndex;
    if (subscript->getUInt32(propertyIndex)) {
        PropertySlot slot;
        JSValue* v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
        KJS_CHECKEXCEPTIONVALUE

        JSValue* v2 = v->toJSNumber(exec);
        base->put(exec, propertyIndex, jsNumber(v2->toNumber(exec) - 1));
        return v2;
    }

    Identifier propertyName(subscript->toString(exec));
    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = v->toJSNumber(exec);
    base->put(exec, propertyName, jsNumber(v2->toNumber(exec) - 1));
    return v2;
}

// ------------------------------ PostfixDotNode ----------------------------------

void PostfixDotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_base.get());
}

RegisterID* PostIncDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> value = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    RegisterID* oldValue = generator.emitPostInc(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

JSValue* PostIncDotNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSObject* base = baseValue->toObject(exec);

    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = v->toJSNumber(exec);
    base->put(exec, m_ident, jsNumber(v2->toNumber(exec) + 1));
    return v2;
}

RegisterID* PostDecDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> value = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    RegisterID* oldValue = generator.emitPostDec(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

JSValue* PostDecDotNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSObject* base = baseValue->toObject(exec);

    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = v->toJSNumber(exec);
    base->put(exec, m_ident, jsNumber(v2->toNumber(exec) - 1));
    return v2;
}

// ------------------------------ PostfixErrorNode -----------------------------------

RegisterID* PostfixErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, m_operator == OpPlusPlus ? "Postfix ++ operator applied to value that is not a reference." : "Postfix -- operator applied to value that is not a reference.");
}

JSValue* PostfixErrorNode::evaluate(OldInterpreterExecState* exec)
{
    throwError(exec, ReferenceError, "Postfix %s operator applied to value that is not a reference.",
               m_operator == OpPlusPlus ? "++" : "--");
    handleException(exec);
    return jsUndefined();
}

// ------------------------------ DeleteResolveNode -----------------------------------

RegisterID* DeleteResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.registerForLocal(m_ident))
        return generator.emitLoad(generator.finalDestination(dst), false);

    RegisterID* base = generator.emitResolveBase(generator.tempDestination(dst), m_ident);
    return generator.emitDeleteById(generator.finalDestination(dst, base), base, m_ident);
}

void DeleteResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage&, NodeStack&)
{
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker())
        new (this) LocalVarDeleteNode();
}

// ECMA 11.4.1

JSValue* DeleteResolveNode::evaluate(OldInterpreterExecState* exec)
{
    // Check for missed optimization opportunity.
    ASSERT(!canSkipLookup(exec, m_ident));

    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // We must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, m_ident, slot))
            return jsBoolean(base->deleteProperty(exec, m_ident));

        ++iter;
    } while (iter != end);

    return jsBoolean(true);
}

JSValue* LocalVarDeleteNode::evaluate(OldInterpreterExecState*)
{
    return jsBoolean(false);
}

// ------------------------------ DeleteBracketNode -----------------------------------

RegisterID* DeleteBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_base.get());
    RefPtr<RegisterID> r1 = generator.emitNode(m_subscript.get());
    return generator.emitDeleteByVal(generator.finalDestination(dst), r0.get(), r1.get());
}

void DeleteBracketNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue* DeleteBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* subscript = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* base = baseValue->toObject(exec);

    uint32_t propertyIndex;
    if (subscript->getUInt32(propertyIndex))
        return jsBoolean(base->deleteProperty(exec, propertyIndex));

    Identifier propertyName(subscript->toString(exec));
    return jsBoolean(base->deleteProperty(exec, propertyName));
}

// ------------------------------ DeleteDotNode -----------------------------------

RegisterID* DeleteDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* r0 = generator.emitNode(m_base.get());
    return generator.emitDeleteById(generator.finalDestination(dst), r0, m_ident);
}

void DeleteDotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_base.get());
}

JSValue* DeleteDotNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    JSObject* base = baseValue->toObject(exec);
    KJS_CHECKEXCEPTIONVALUE

    return jsBoolean(base->deleteProperty(exec, m_ident));
}

// ------------------------------ DeleteValueNode -----------------------------------

RegisterID* DeleteValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(m_expr.get());

    // delete on a non-location expression ignores the value and returns true
    return generator.emitLoad(generator.finalDestination(dst), true);
}

void DeleteValueNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

JSValue* DeleteValueNode::evaluate(OldInterpreterExecState* exec)
{
    m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    // delete on a non-location expression ignores the value and returns true
    return jsBoolean(true);
}

// ------------------------------ VoidNode -------------------------------------

RegisterID* VoidNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    return generator.emitLoad(generator.finalDestination(dst, r0.get()), jsUndefined());
}

void VoidNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

// ECMA 11.4.2
JSValue* VoidNode::evaluate(OldInterpreterExecState* exec)
{
    m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    return jsUndefined();
}

// ECMA 11.4.3

// ------------------------------ TypeOfValueNode -----------------------------------

void TypeOfValueNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

static JSValue* typeStringForValue(JSValue* v) KJS_FAST_CALL;
static JSValue* typeStringForValue(JSValue* v)
{
    switch (v->type()) {
        case UndefinedType:
            return jsString("undefined");
        case NullType:
            return jsString("object");
        case BooleanType:
            return jsString("boolean");
        case NumberType:
            return jsString("number");
        case StringType:
            return jsString("string");
        default:
            if (v->isObject()) {
                // Return "undefined" for objects that should be treated
                // as null when doing comparisons.
                if (static_cast<JSObject*>(v)->masqueradeAsUndefined())
                    return jsString("undefined");
                else if (static_cast<JSObject*>(v)->implementsCall())
                    return jsString("function");
            }

            return jsString("object");
    }
}

void TypeOfResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage&, NodeStack&)
{
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker())
        new (this) LocalVarTypeOfNode(index);
}

JSValue* LocalVarTypeOfNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    return typeStringForValue(exec->localStorage()[m_index].value);
}

RegisterID* TypeOfResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident))
        return generator.emitTypeOf(generator.finalDestination(dst), local);

    RefPtr<RegisterID> scratch = generator.emitResolveBase(generator.tempDestination(dst), m_ident);
    generator.emitGetById(scratch.get(), scratch.get(), m_ident);
    return generator.emitTypeOf(generator.finalDestination(dst, scratch.get()), scratch.get());
}

JSValue* TypeOfResolveNode::evaluate(OldInterpreterExecState* exec)
{
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // We must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, m_ident, slot)) {
            JSValue* v = slot.getValue(exec, base, m_ident);
            return typeStringForValue(v);
        }

        ++iter;
    } while (iter != end);

    return jsString("undefined");
}

// ------------------------------ TypeOfValueNode -----------------------------------

RegisterID* TypeOfValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src = generator.emitNode(m_expr.get());
    return generator.emitTypeOf(generator.finalDestination(dst), src.get());
}

JSValue* TypeOfValueNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    return typeStringForValue(v);
}

// ECMA 11.4.4 and 11.4.5

// ------------------------------ PrefixResolveNode ----------------------------------

RegisterID* PreIncResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RefPtr<RegisterID> r0 = generator.emitLoad(generator.finalDestination(dst), 1.0);
            return generator.emitAdd(r0.get(), local, r0.get());
        }
        
        generator.emitPreInc(local);
        return generator.moveToDestinationIfNeeded(dst, local);
    }
    
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), propDst.get(), m_ident);
    generator.emitPreInc(propDst.get());
    generator.emitPutById(base.get(), m_ident, propDst.get());
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

void PreIncResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage& localStorage, NodeStack&)
{
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker()) {
        if (isConstant(localStorage, index))
            new (this) PreIncConstNode(index);
        else
            new (this) PreIncLocalVarNode(index);
    }
}

JSValue* PreIncLocalVarNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue** slot = &exec->localStorage()[m_index].value;

    double n = (*slot)->toNumber(exec);
    JSValue* n2 = jsNumber(n + 1);
    *slot = n2;
    return n2;
}

JSValue* PreIncResolveNode::evaluate(OldInterpreterExecState* exec)
{
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    do {
        if ((*iter)->getPropertySlot(exec, m_ident, slot)) {
            // See the comment in PostIncResolveNode::evaluate().

            JSObject* base = *iter;
            JSValue* v = slot.getValue(exec, base, m_ident);

            double n = v->toNumber(exec);
            JSValue* n2 = jsNumber(n + 1);
            base->put(exec, m_ident, n2);

            return n2;
        }

        ++iter;
    } while (iter != end);

    return throwUndefinedVariableError(exec, m_ident);
}

RegisterID* PreDecResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RefPtr<RegisterID> r0 = generator.emitLoad(generator.finalDestination(dst), -1.0);
            return generator.emitAdd(r0.get(), local, r0.get());
        }
        
        generator.emitPreDec(local);
        return generator.moveToDestinationIfNeeded(dst, local);
    }

    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), propDst.get(), m_ident);
    generator.emitPreDec(propDst.get());
    generator.emitPutById(base.get(), m_ident, propDst.get());
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

void PreDecResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage& localStorage, NodeStack&)
{
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker()) {
        if (isConstant(localStorage, index))
            new (this) PreDecConstNode(index);
        else
            new (this) PreDecLocalVarNode(index);
    }
}

JSValue* PreDecLocalVarNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue** slot = &exec->localStorage()[m_index].value;

    double n = (*slot)->toNumber(exec);
    JSValue* n2 = jsNumber(n - 1);
    *slot = n2;
    return n2;
}

JSValue* PreDecResolveNode::evaluate(OldInterpreterExecState* exec)
{
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    do {
        if ((*iter)->getPropertySlot(exec, m_ident, slot)) {
            // See the comment in PostIncResolveNode::evaluate().

            JSObject* base = *iter;
            JSValue* v = slot.getValue(exec, base, m_ident);

            double n = v->toNumber(exec);
            JSValue* n2 = jsNumber(n - 1);
            base->put(exec, m_ident, n2);

            return n2;
        }

        ++iter;
    } while (iter != end);

    return throwUndefinedVariableError(exec, m_ident);
}

// ------------------------------ PreIncConstNode ----------------------------------

JSValue* PreIncConstNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    return jsNumber(exec->localStorage()[m_index].value->toNumber(exec) + 1);
}

// ------------------------------ PreDecConstNode ----------------------------------

JSValue* PreDecConstNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    return jsNumber(exec->localStorage()[m_index].value->toNumber(exec) - 1);
}

// ------------------------------ PostIncConstNode ----------------------------------

JSValue* PostIncConstNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    return jsNumber(exec->localStorage()[m_index].value->toNumber(exec));
}

// ------------------------------ PostDecConstNode ----------------------------------

JSValue* PostDecConstNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    return jsNumber(exec->localStorage()[m_index].value->toNumber(exec));
}

// ------------------------------ PrefixBracketNode ----------------------------------

void PrefixBracketNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

RegisterID* PreIncBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetByVal(propDst.get(), base.get(), property.get());
    generator.emitPreInc(value);
    generator.emitPutByVal(base.get(), property.get(), value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

JSValue* PreIncBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* subscript = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* base = baseValue->toObject(exec);

    uint32_t propertyIndex;
    if (subscript->getUInt32(propertyIndex)) {
        PropertySlot slot;
        JSValue* v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
        KJS_CHECKEXCEPTIONVALUE

        JSValue* n2 = jsNumber(v->toNumber(exec) + 1);
        base->put(exec, propertyIndex, n2);

        return n2;
    }

    Identifier propertyName(subscript->toString(exec));
    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* n2 = jsNumber(v->toNumber(exec) + 1);
    base->put(exec, propertyName, n2);

    return n2;
}

RegisterID* PreDecBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetByVal(propDst.get(), base.get(), property.get());
    generator.emitPreDec(value);
    generator.emitPutByVal(base.get(), property.get(), value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

JSValue* PreDecBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* subscript = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* base = baseValue->toObject(exec);

    uint32_t propertyIndex;
    if (subscript->getUInt32(propertyIndex)) {
        PropertySlot slot;
        JSValue* v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
        KJS_CHECKEXCEPTIONVALUE

        JSValue* n2 = jsNumber(v->toNumber(exec) - 1);
        base->put(exec, propertyIndex, n2);

        return n2;
    }

    Identifier propertyName(subscript->toString(exec));
    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* n2 = jsNumber(v->toNumber(exec) - 1);
    base->put(exec, propertyName, n2);

    return n2;
}

// ------------------------------ PrefixDotNode ----------------------------------

void PrefixDotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_base.get());
}

RegisterID* PreIncDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetById(propDst.get(), base.get(), m_ident);
    generator.emitPreInc(value);
    generator.emitPutById(base.get(), m_ident, value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

JSValue* PreIncDotNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSObject* base = baseValue->toObject(exec);

    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    double n = v->toNumber(exec);
    JSValue* n2 = jsNumber(n + 1);
    base->put(exec, m_ident, n2);

    return n2;
}

RegisterID* PreDecDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetById(propDst.get(), base.get(), m_ident);
    generator.emitPreDec(value);
    generator.emitPutById(base.get(), m_ident, value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

JSValue* PreDecDotNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSObject* base = baseValue->toObject(exec);

    PropertySlot slot;
    JSValue* v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    double n = v->toNumber(exec);
    JSValue* n2 = jsNumber(n - 1);
    base->put(exec, m_ident, n2);

    return n2;
}

// ------------------------------ PrefixErrorNode -----------------------------------

RegisterID* PrefixErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, m_operator == OpPlusPlus ? "Prefix ++ operator applied to value that is not a reference." : "Prefix -- operator applied to value that is not a reference.");
}

JSValue* PrefixErrorNode::evaluate(OldInterpreterExecState* exec)
{
    throwError(exec, ReferenceError, "Prefix %s operator applied to value that is not a reference.",
               m_operator == OpPlusPlus ? "++" : "--");
    handleException(exec);
    return jsUndefined();
}

// ------------------------------ UnaryPlusNode --------------------------------

RegisterID* UnaryPlusNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitToJSNumber(generator.finalDestination(dst), src);
}

void UnaryPlusNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

// ECMA 11.4.6
JSValue* UnaryPlusNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return v->toJSNumber(exec);
}

bool UnaryPlusNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return m_expr->evaluateToBoolean(exec);
}

double UnaryPlusNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return m_expr->evaluateToNumber(exec);
}

int32_t UnaryPlusNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return m_expr->evaluateToInt32(exec);
}

uint32_t UnaryPlusNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return m_expr->evaluateToInt32(exec);
}

// ------------------------------ NegateNode -----------------------------------

RegisterID* NegateNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitNegate(generator.finalDestination(dst), src);
}

void NegateNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

// ECMA 11.4.7
JSValue* NegateNode::evaluate(OldInterpreterExecState* exec)
{
    // No need to check exception, caller will do so right after evaluate()
    return jsNumber(-m_expr->evaluateToNumber(exec));
}

double NegateNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    // No need to check exception, caller will do so right after evaluateToNumber()
    return -m_expr->evaluateToNumber(exec);
}

// ------------------------------ BitwiseNotNode -------------------------------

RegisterID* BitwiseNotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitBitNot(generator.finalDestination(dst), src);
}

void BitwiseNotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

// ECMA 11.4.8
int32_t BitwiseNotNode::inlineEvaluateToInt32(OldInterpreterExecState* exec)
{
    return ~m_expr->evaluateToInt32(exec);
}

JSValue* BitwiseNotNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double BitwiseNotNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitwiseNotNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitwiseNotNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitwiseNotNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

// ------------------------------ LogicalNotNode -------------------------------

RegisterID* LogicalNotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitNot(generator.finalDestination(dst), src);
}

void LogicalNotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

// ECMA 11.4.9
JSValue* LogicalNotNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(!m_expr->evaluateToBoolean(exec));
}

bool LogicalNotNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return !m_expr->evaluateToBoolean(exec);
}

// ------------------------------ Multiplicative Nodes -----------------------------------

RegisterID* MultNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_term1.get());
    RegisterID* src2 = generator.emitNode(m_term2.get());
    return generator.emitMul(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void MultNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.5.1
double MultNode::inlineEvaluateToNumber(OldInterpreterExecState* exec)
{
    double n1 = m_term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = m_term2->evaluateToNumber(exec);
    return n1 * n2;
}

JSValue* MultNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double MultNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

bool MultNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    double result = inlineEvaluateToNumber(exec);
    return  result > 0.0 || 0.0 > result; // NaN produces false as well
}

int32_t MultNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t MultNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

RegisterID* DivNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> dividend = generator.emitNode(m_term1.get());
    RegisterID* divisor = generator.emitNode(m_term2.get());
    return generator.emitDiv(generator.finalDestination(dst, dividend.get()), dividend.get(), divisor);
}

void DivNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.5.2
double DivNode::inlineEvaluateToNumber(OldInterpreterExecState* exec)
{
    double n1 = m_term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = m_term2->evaluateToNumber(exec);
    return n1 / n2;
}

JSValue* DivNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double DivNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t DivNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t DivNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

RegisterID* ModNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> dividend = generator.emitNode(m_term1.get());
    RegisterID* divisor = generator.emitNode(m_term2.get());
    return generator.emitMod(generator.finalDestination(dst, dividend.get()), dividend.get(), divisor);
}

void ModNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.5.3
double ModNode::inlineEvaluateToNumber(OldInterpreterExecState* exec)
{
    double n1 = m_term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = m_term2->evaluateToNumber(exec);
    return fmod(n1, n2);
}

JSValue* ModNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double ModNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

bool ModNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    double result = inlineEvaluateToNumber(exec);
    return  result > 0.0 || 0.0 > result; // NaN produces false as well
}

int32_t ModNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t ModNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

// ------------------------------ Additive Nodes --------------------------------------

static double throwOutOfMemoryErrorToNumber(OldInterpreterExecState* exec)
{
    JSObject* error = Error::create(exec, GeneralError, "Out of memory");
    exec->setException(error);
    return 0.0;
}

// ECMA 11.6
static JSValue* addSlowCase(OldInterpreterExecState* exec, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue* p1 = v1->toPrimitive(exec, UnspecifiedType);
    JSValue* p2 = v2->toPrimitive(exec, UnspecifiedType);

    if (p1->isString() || p2->isString()) {
        UString value = p1->toString(exec) + p2->toString(exec);
        if (value.isNull())
            return throwOutOfMemoryError(exec);
        return jsString(value);
    }

    return jsNumber(p1->toNumber(exec) + p2->toNumber(exec));
}

static double addSlowCaseToNumber(OldInterpreterExecState* exec, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue* p1 = v1->toPrimitive(exec, UnspecifiedType);
    JSValue* p2 = v2->toPrimitive(exec, UnspecifiedType);

    if (p1->isString() || p2->isString()) {
        UString value = p1->toString(exec) + p2->toString(exec);
        if (value.isNull())
            return throwOutOfMemoryErrorToNumber(exec);
        return value.toDouble();
    }

    return p1->toNumber(exec) + p2->toNumber(exec);
}

// Fast-path choices here are based on frequency data from SunSpider:
//    <times> Add case: <t1> <t2>
//    ---------------------------
//    5627160 Add case: 1 1
//    247427  Add case: 5 5
//    20901   Add case: 5 6
//    13978   Add case: 5 1
//    4000    Add case: 1 5
//    1       Add case: 3 5

static inline JSValue* add(OldInterpreterExecState* exec, JSValue* v1, JSValue* v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    const unsigned bothTypes = (t1 << 3) | t2;

    if (bothTypes == ((NumberType << 3) | NumberType))
        return jsNumber(v1->toNumber(exec) + v2->toNumber(exec));
    if (bothTypes == ((StringType << 3) | StringType)) {
        UString value = static_cast<StringImp*>(v1)->value() + static_cast<StringImp*>(v2)->value();
        if (value.isNull())
            return throwOutOfMemoryError(exec);
        return jsString(value);
    }

    // All other cases are pretty uncommon
    return addSlowCase(exec, v1, v2);
}

static inline double addToNumber(OldInterpreterExecState* exec, JSValue* v1, JSValue* v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    const unsigned bothTypes = (t1 << 3) | t2;

    if (bothTypes == ((NumberType << 3) | NumberType))
        return v1->toNumber(exec) + v2->toNumber(exec);
    if (bothTypes == ((StringType << 3) | StringType)) {
        UString value = static_cast<StringImp*>(v1)->value() + static_cast<StringImp*>(v2)->value();
        if (value.isNull())
            return throwOutOfMemoryErrorToNumber(exec);
        return value.toDouble();
    }

    // All other cases are pretty uncommon
    return addSlowCaseToNumber(exec, v1, v2);
}

RegisterID* AddNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_term1.get());
    RegisterID* src2 = generator.emitNode(m_term2.get());
    return generator.emitAdd(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void AddNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.6.1
JSValue* AddNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_term1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = m_term2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    return add(exec, v1, v2);
}

double AddNode::inlineEvaluateToNumber(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_term1->evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER

    JSValue* v2 = m_term2->evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER

    return addToNumber(exec, v1, v2);
}

double AddNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t AddNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t AddNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

double AddNumbersNode::inlineEvaluateToNumber(OldInterpreterExecState* exec)
{
    double n1 = m_term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = m_term2->evaluateToNumber(exec);
    return n1 + n2;
}

JSValue* AddNumbersNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double AddNumbersNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t AddNumbersNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t AddNumbersNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

JSValue* AddStringsNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_term1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = m_term2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    return jsString(static_cast<StringImp*>(v1)->value() + static_cast<StringImp*>(v2)->value());
}

JSValue* AddStringLeftNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_term1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = m_term2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSValue* p2 = v2->toPrimitive(exec, UnspecifiedType);
    return jsString(static_cast<StringImp*>(v1)->value() + p2->toString(exec));
}

JSValue* AddStringRightNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_term1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = m_term2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSValue* p1 = v1->toPrimitive(exec, UnspecifiedType);
    return jsString(p1->toString(exec) + static_cast<StringImp*>(v2)->value());
}

RegisterID* SubNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_term1.get());
    RegisterID* src2 = generator.emitNode(m_term2.get());
    return generator.emitSub(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void SubNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.6.2
double SubNode::inlineEvaluateToNumber(OldInterpreterExecState* exec)
{
    double n1 = m_term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = m_term2->evaluateToNumber(exec);
    return n1 - n2;
}

JSValue* SubNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double SubNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t SubNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t SubNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

// ------------------------------ Shift Nodes ------------------------------------

RegisterID* LeftShiftNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> val = generator.emitNode(m_term1.get());
    RegisterID* shift = generator.emitNode(m_term2.get());
    return generator.emitLeftShift(generator.finalDestination(dst, val.get()), val.get(), shift);
}

void LeftShiftNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.7.1
int32_t LeftShiftNode::inlineEvaluateToInt32(OldInterpreterExecState* exec)
{
    int i1 = m_term1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    unsigned int i2 = m_term2->evaluateToUInt32(exec) & 0x1f;
    return (i1 << i2);
}

JSValue* LeftShiftNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double LeftShiftNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t LeftShiftNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t LeftShiftNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

RegisterID* RightShiftNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> val = generator.emitNode(m_term1.get());
    RegisterID* shift = generator.emitNode(m_term2.get());
    return generator.emitRightShift(generator.finalDestination(dst, val.get()), val.get(), shift);
}

void RightShiftNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.7.2
int32_t RightShiftNode::inlineEvaluateToInt32(OldInterpreterExecState* exec)
{
    int i1 = m_term1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    unsigned int i2 = m_term2->evaluateToUInt32(exec) & 0x1f;
    return (i1 >> i2);
}

JSValue* RightShiftNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double RightShiftNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t RightShiftNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t RightShiftNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

RegisterID* UnsignedRightShiftNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> val = generator.emitNode(m_term1.get());
    RegisterID* shift = generator.emitNode(m_term2.get());
    return generator.emitUnsignedRightShift(generator.finalDestination(dst, val.get()), val.get(), shift);
}

void UnsignedRightShiftNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_term1.get());
    nodeStack.append(m_term2.get());
}

// ECMA 11.7.3
uint32_t UnsignedRightShiftNode::inlineEvaluateToUInt32(OldInterpreterExecState* exec)
{
    unsigned int i1 = m_term1->evaluateToUInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    unsigned int i2 = m_term2->evaluateToUInt32(exec) & 0x1f;
    return (i1 >> i2);
}

JSValue* UnsignedRightShiftNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToUInt32(exec));
}

double UnsignedRightShiftNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToUInt32(exec);
}

int32_t UnsignedRightShiftNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToUInt32(exec);
}

uint32_t UnsignedRightShiftNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToUInt32(exec);
}

// ------------------------------ Relational Nodes -------------------------------

static inline bool lessThan(OldInterpreterExecState* exec, JSValue* v1, JSValue* v2)
{
    double n1;
    double n2;
    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 < n2;

    return static_cast<const StringImp*>(p1)->value() < static_cast<const StringImp*>(p2)->value();
}

static inline bool lessThanEq(OldInterpreterExecState* exec, JSValue* v1, JSValue* v2)
{
    double n1;
    double n2;
    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;

    return !(static_cast<const StringImp*>(p2)->value() < static_cast<const StringImp*>(p1)->value());
}

RegisterID* LessNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitLess(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void LessNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.8.1
bool LessNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThan(exec, v1, v2);
}

JSValue* LessNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

bool LessNumbersNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    double n1 = m_expr1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONVALUE
    double n2 = m_expr2->evaluateToNumber(exec);
    return n1 < n2;
}

JSValue* LessNumbersNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessNumbersNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

bool LessStringsNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = m_expr2->evaluate(exec);
    return static_cast<StringImp*>(v1)->value() < static_cast<StringImp*>(v2)->value();
}

JSValue* LessStringsNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessStringsNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

RegisterID* GreaterNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr2.get());
    RegisterID* src2 = generator.emitNode(m_expr1.get());
    return generator.emitLess(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void GreaterNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.8.2
bool GreaterNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThan(exec, v2, v1);
}

JSValue* GreaterNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool GreaterNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

RegisterID* LessEqNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitLessEq(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void LessEqNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.8.3
bool LessEqNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThanEq(exec, v1, v2);
}

JSValue* LessEqNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessEqNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

RegisterID* GreaterEqNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr2.get());
    RegisterID* src2 = generator.emitNode(m_expr1.get());
    return generator.emitLessEq(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void GreaterEqNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.8.4
bool GreaterEqNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThanEq(exec, v2, v1);
}

JSValue* GreaterEqNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool GreaterEqNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

RegisterID* InstanceOfNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> value = generator.emitNode(m_expr1.get());
    RegisterID* base = generator.emitNode(m_expr2.get());
    return generator.emitInstanceOf(generator.finalDestination(dst, value.get()), value.get(), base);
}

void InstanceOfNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.8.6
JSValue* InstanceOfNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    if (!v2->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with instanceof operator.", v2, m_expr2.get());

    JSObject* o2 = static_cast<JSObject*>(v2);

    // According to the spec, only some types of objects "implement" the [[HasInstance]] property.
    // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
    // property. It seems that all objects have the property, but not all implement it, so in this
    // case we return false (consistent with Mozilla).
    if (!o2->implementsHasInstance())
        return jsBoolean(false);

    return jsBoolean(o2->hasInstance(exec, v1));
}

bool InstanceOfNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    if (!v2->isObject()) {
        throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with 'instanceof' operator.", v2, m_expr2.get());
        return false;
    }

    JSObject* o2 = static_cast<JSObject*>(v2);

    // According to the spec, only some types of objects "implement" the [[HasInstance]] property.
    // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
    // property. It seems that all objects have the property, but not all implement it, so in this
    // case we return false (consistent with Mozilla).
    if (!o2->implementsHasInstance())
        return false;

    return o2->hasInstance(exec, v1);
}

RegisterID* InNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> property = generator.emitNode(m_expr1.get());
    RegisterID* base = generator.emitNode(m_expr2.get());
    return generator.emitIn(generator.finalDestination(dst, property.get()), property.get(), base);
}

void InNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.8.7
JSValue* InNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    if (!v2->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with 'in' operator.", v2, m_expr2.get());

    return jsBoolean(static_cast<JSObject*>(v2)->hasProperty(exec, Identifier(v1->toString(exec))));
}

bool InNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    if (!v2->isObject()) {
        throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with 'in' operator.", v2, m_expr2.get());
        return false;
    }

    return static_cast<JSObject*>(v2)->hasProperty(exec, Identifier(v1->toString(exec)));
}

// ------------------------------ Equality Nodes ------------------------------------

RegisterID* EqualNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitEqual(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void EqualNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.9.1
bool EqualNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    return equal(exec, v1, v2);
}

JSValue* EqualNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool EqualNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

RegisterID* NotEqualNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitNotEqual(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void NotEqualNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.9.2
bool NotEqualNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    return !equal(exec,v1, v2);
}

JSValue* NotEqualNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool NotEqualNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

RegisterID* StrictEqualNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitStrictEqual(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void StrictEqualNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.9.4
bool StrictEqualNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    return strictEqual(v1, v2);
}

JSValue* StrictEqualNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool StrictEqualNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

RegisterID* NotStrictEqualNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitNotStrictEqual(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void NotStrictEqualNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.9.5
bool NotStrictEqualNode::inlineEvaluateToBoolean(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    return !strictEqual(v1, v2);
}

JSValue* NotStrictEqualNode::evaluate(OldInterpreterExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool NotStrictEqualNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

// ------------------------------ Bit Operation Nodes ----------------------------------

RegisterID* BitAndNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitBitAnd(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void BitAndNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.10
JSValue* BitAndNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    return jsNumberFromAnd(exec, v1, v2);
}

int32_t BitAndNode::inlineEvaluateToInt32(OldInterpreterExecState* exec)
{
    int32_t i1 = m_expr1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    int32_t i2 = m_expr2->evaluateToInt32(exec);
    return (i1 & i2);
}

double BitAndNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitAndNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitAndNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitAndNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

RegisterID* BitXOrNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitBitXOr(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void BitXOrNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

int32_t BitXOrNode::inlineEvaluateToInt32(OldInterpreterExecState* exec)
{
    int i1 = m_expr1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    int i2 = m_expr2->evaluateToInt32(exec);
    return (i1 ^ i2);
}

JSValue* BitXOrNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double BitXOrNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitXOrNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitXOrNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitXOrNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

RegisterID* BitOrNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr1.get());
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitBitOr(generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

void BitOrNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

int32_t BitOrNode::inlineEvaluateToInt32(OldInterpreterExecState* exec)
{
    int i1 = m_expr1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    int i2 = m_expr2->evaluateToInt32(exec);
    return (i1 | i2);
}

JSValue* BitOrNode::evaluate(OldInterpreterExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double BitOrNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitOrNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitOrNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitOrNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

// ------------------------------ Binary Logical Nodes ----------------------------

RegisterID* LogicalAndNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    RefPtr<LabelID> target = generator.newLabel();
    
    generator.emitNode(temp.get(), m_expr1.get());
    generator.emitJumpIfFalse(temp.get(), target.get());
    generator.emitNode(temp.get(), m_expr2.get());
    generator.emitLabel(target.get());

    return generator.moveToDestinationIfNeeded(dst, temp.get());
}

void LogicalAndNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.11
JSValue* LogicalAndNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    bool b1 = v1->toBoolean(exec);
    KJS_CHECKEXCEPTIONVALUE
    if (!b1)
        return v1;
    JSValue* v2 = m_expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return v2;
}

bool LogicalAndNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    bool b = m_expr1->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return b && m_expr2->evaluateToBoolean(exec);
}

RegisterID* LogicalOrNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    RefPtr<LabelID> target = generator.newLabel();
    
    generator.emitNode(temp.get(), m_expr1.get());
    generator.emitJumpIfTrue(temp.get(), target.get());
    generator.emitNode(temp.get(), m_expr2.get());
    generator.emitLabel(target.get());

    return generator.moveToDestinationIfNeeded(dst, temp.get());
}

void LogicalOrNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

JSValue* LogicalOrNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v1 = m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    if (v1->toBoolean(exec))
        return v1;
    return m_expr2->evaluate(exec);
}

bool LogicalOrNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    bool b = m_expr1->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return b || m_expr2->evaluateToBoolean(exec);
}

// ------------------------------ ConditionalNode ------------------------------

RegisterID* ConditionalNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newDst = generator.finalDestination(dst);
    RefPtr<LabelID> beforeElse = generator.newLabel();
    RefPtr<LabelID> afterElse = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_logical.get());
    generator.emitJumpIfFalse(cond, beforeElse.get());

    generator.emitNode(newDst.get(), m_expr1.get());
    generator.emitJump(afterElse.get());

    generator.emitLabel(beforeElse.get());
    generator.emitNode(newDst.get(), m_expr2.get());

    generator.emitLabel(afterElse.get());

    return newDst.get();
}

void ConditionalNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
    nodeStack.append(m_logical.get());
}

// ECMA 11.12
JSValue* ConditionalNode::evaluate(OldInterpreterExecState* exec)
{
    bool b = m_logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONVALUE
    return b ? m_expr1->evaluate(exec) : m_expr2->evaluate(exec);
}

bool ConditionalNode::evaluateToBoolean(OldInterpreterExecState* exec)
{
    bool b = m_logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return b ? m_expr1->evaluateToBoolean(exec) : m_expr2->evaluateToBoolean(exec);
}

double ConditionalNode::evaluateToNumber(OldInterpreterExecState* exec)
{
    bool b = m_logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return b ? m_expr1->evaluateToNumber(exec) : m_expr2->evaluateToNumber(exec);
}

int32_t ConditionalNode::evaluateToInt32(OldInterpreterExecState* exec)
{
    bool b = m_logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return b ? m_expr1->evaluateToInt32(exec) : m_expr2->evaluateToInt32(exec);
}

uint32_t ConditionalNode::evaluateToUInt32(OldInterpreterExecState* exec)
{
    bool b = m_logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return b ? m_expr1->evaluateToUInt32(exec) : m_expr2->evaluateToUInt32(exec);
}

// ECMA 11.13

static ALWAYS_INLINE JSValue* valueForReadModifyAssignment(OldInterpreterExecState* exec, JSValue* current, ExpressionNode* right, Operator oper) KJS_FAST_CALL;
static ALWAYS_INLINE JSValue* valueForReadModifyAssignment(OldInterpreterExecState* exec, JSValue* current, ExpressionNode* right, Operator oper)
{
    JSValue* v;
    int i1;
    int i2;
    unsigned int ui;
    switch (oper) {
        case OpMultEq:
            v = jsNumber(current->toNumber(exec) * right->evaluateToNumber(exec));
            break;
        case OpDivEq:
            v = jsNumber(current->toNumber(exec) / right->evaluateToNumber(exec));
            break;
        case OpPlusEq:
            v = add(exec, current, right->evaluate(exec));
            break;
        case OpMinusEq:
            v = jsNumber(current->toNumber(exec) - right->evaluateToNumber(exec));
            break;
        case OpLShift:
            i1 = current->toInt32(exec);
            i2 = right->evaluateToInt32(exec);
            v = jsNumber(i1 << i2);
            break;
        case OpRShift:
            i1 = current->toInt32(exec);
            i2 = right->evaluateToInt32(exec);
            v = jsNumber(i1 >> i2);
            break;
        case OpURShift:
            ui = current->toUInt32(exec);
            i2 = right->evaluateToInt32(exec);
            v = jsNumber(ui >> i2);
            break;
        case OpAndEq:
            i1 = current->toInt32(exec);
            i2 = right->evaluateToInt32(exec);
            v = jsNumber(i1 & i2);
            break;
        case OpXOrEq:
            i1 = current->toInt32(exec);
            i2 = right->evaluateToInt32(exec);
            v = jsNumber(i1 ^ i2);
            break;
        case OpOrEq:
            i1 = current->toInt32(exec);
            i2 = right->evaluateToInt32(exec);
            v = jsNumber(i1 | i2);
            break;
        case OpModEq: {
            double d1 = current->toNumber(exec);
            double d2 = right->evaluateToNumber(exec);
            v = jsNumber(fmod(d1, d2));
        }
            break;
        default:
            ASSERT_NOT_REACHED();
            v = jsUndefined();
    }

    return v;
}

// ------------------------------ ReadModifyResolveNode -----------------------------------

// FIXME: should this be moved to be a method on CodeGenerator?
static ALWAYS_INLINE RegisterID* emitReadModifyAssignment(CodeGenerator& generator, RegisterID* dst, RegisterID* src1, RegisterID* src2, Operator oper)
{
    switch (oper) {
        case OpMultEq:
            return generator.emitMul(dst, src1, src2);
        case OpDivEq:
            return generator.emitDiv(dst, src1, src2);
        case OpPlusEq:
            return generator.emitAdd(dst, src1, src2);
        case OpMinusEq:
            return generator.emitSub(dst, src1, src2);
        case OpLShift:
            return generator.emitLeftShift(dst, src1, src2);
        case OpRShift:
            return generator.emitRightShift(dst, src1, src2);
        case OpURShift:
            return generator.emitUnsignedRightShift(dst, src1, src2);
        case OpAndEq:
            return generator.emitBitAnd(dst, src1, src2);
        case OpXOrEq:
            return generator.emitBitXOr(dst, src1, src2);
        case OpOrEq:
            return generator.emitBitOr(dst, src1, src2);
        case OpModEq:
            return generator.emitMod(dst, src1, src2);
        default:
            ASSERT_NOT_REACHED();
    }

    return dst;
}

RegisterID* ReadModifyResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RegisterID* src2 = generator.emitNode(m_right.get());
            return emitReadModifyAssignment(generator, generator.finalDestination(dst), local, src2, m_operator);
        }
        
        if (generator.leftHandSideNeedsCopy(m_rightHasAssignments) && !m_right.get()->isNumber()) {
            RefPtr<RegisterID> result = generator.newTemporary();
            generator.emitMove(result.get(), local);
            RegisterID* src2 = generator.emitNode(m_right.get());
            emitReadModifyAssignment(generator, result.get(), result.get(), src2, m_operator);
            generator.emitMove(local, result.get());
            return generator.moveToDestinationIfNeeded(dst, result.get());
        }
        
        RegisterID* src2 = generator.emitNode(m_right.get());
        RegisterID* result = emitReadModifyAssignment(generator, local, local, src2, m_operator);
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    RefPtr<RegisterID> src1 = generator.tempDestination(dst);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), src1.get(), m_ident);
    RegisterID* src2 = generator.emitNode(m_right.get());
    RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst, src1.get()), src1.get(), src2, m_operator);
    return generator.emitPutById(base.get(), m_ident, result);
}

void ReadModifyResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage& localStorage, NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker()) {
        if (isConstant(localStorage, index))
            new (this) ReadModifyConstNode(index);
        else
            new (this) ReadModifyLocalVarNode(index);
    }
}

// ------------------------------ AssignResolveNode -----------------------------------

RegisterID* AssignResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident))
            return generator.emitNode(dst, m_right.get());
        
        RegisterID* result = generator.emitNode(local, m_right.get());
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RegisterID* value = generator.emitNode(dst, m_right.get());
        generator.emitPutScopedVar(depth, index, value);
        return value;
    }

    RefPtr<RegisterID> base = generator.emitResolveBase(generator.newTemporary(), m_ident);
    RegisterID* value = generator.emitNode(dst, m_right.get());
    return generator.emitPutById(base.get(), m_ident, value);
}

void AssignResolveNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable& symbolTable, const LocalStorage& localStorage, NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    int index = symbolTable.get(m_ident.ustring().rep()).getIndex();
    if (index != missingSymbolMarker()) {
        if (isConstant(localStorage, index))
            new (this) AssignConstNode;
        else
            new (this) AssignLocalVarNode(index);
    }
}

// ------------------------------ ReadModifyLocalVarNode -----------------------------------

JSValue* ReadModifyLocalVarNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    ASSERT(m_operator != OpEqual);
    JSValue* v = valueForReadModifyAssignment(exec, exec->localStorage()[m_index].value, m_right.get(), m_operator);

    KJS_CHECKEXCEPTIONVALUE
    
    // We can't store a pointer into localStorage() and use it throughout the function
    // body, because valueForReadModifyAssignment() might cause an ActivationImp tear-off,
    // changing the value of localStorage().
    
    exec->localStorage()[m_index].value = v;
    return v;
}

// ------------------------------ AssignLocalVarNode -----------------------------------

JSValue* AssignLocalVarNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue* v = m_right->evaluate(exec);

    KJS_CHECKEXCEPTIONVALUE

    exec->localStorage()[m_index].value = v;

    return v;
}

// ------------------------------ ReadModifyConstNode -----------------------------------

JSValue* ReadModifyConstNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue* left = exec->localStorage()[m_index].value;
    ASSERT(m_operator != OpEqual);
    JSValue* result = valueForReadModifyAssignment(exec, left, m_right.get(), m_operator);
    KJS_CHECKEXCEPTIONVALUE
    return result;
}

// ------------------------------ AssignConstNode -----------------------------------

JSValue* AssignConstNode::evaluate(OldInterpreterExecState* exec)
{
    return m_right->evaluate(exec);
}

JSValue* ReadModifyResolveNode::evaluate(OldInterpreterExecState* exec)
{
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // We must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, m_ident, slot)) {
            // See the comment in PostIncResolveNode::evaluate().

            base = *iter;
            goto found;
        }

        ++iter;
    } while (iter != end);

    ASSERT(m_operator != OpEqual);
    return throwUndefinedVariableError(exec, m_ident);

found:
    JSValue* v;

    ASSERT(m_operator != OpEqual);
    JSValue* v1 = slot.getValue(exec, base, m_ident);
    KJS_CHECKEXCEPTIONVALUE
    v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_operator);

    KJS_CHECKEXCEPTIONVALUE
    
    // Since valueForReadModifyAssignment() might cause an ActivationImp tear-off,
    // we need to get the base from the ScopeChainIterator again.
    
    (*iter)->put(exec, m_ident, v);
    return v;
}

JSValue* AssignResolveNode::evaluate(OldInterpreterExecState* exec)
{
    const ScopeChain& chain = exec->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    PropertySlot slot;
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, m_ident, slot)) {
            // See the comment in PostIncResolveNode::evaluate().

            base = *iter;
            goto found;
        }

        ++iter;
    } while (iter != end);

found:
    JSValue* v = m_right->evaluate(exec);

    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, m_ident, v);
    return v;
}

// ------------------------------ ReadModifyDotNode -----------------------------------

RegisterID* AssignDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_rightHasAssignments);
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RegisterID* result = generator.emitNode(value.get(), m_right.get());
    generator.emitPutById(base.get(), m_ident, result);
    return generator.moveToDestinationIfNeeded(dst, result);
}

void AssignDotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_base.get());
}

JSValue* AssignDotNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSObject* base = baseValue->toObject(exec);

    JSValue* v = m_right->evaluate(exec);

    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, m_ident, v);
    return v;
}

RegisterID* ReadModifyDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_rightHasAssignments);
    RefPtr<RegisterID> value = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator);
    return generator.emitPutById(base.get(), m_ident, updatedValue);
}

void ReadModifyDotNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_base.get());
}

JSValue* ReadModifyDotNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSObject* base = baseValue->toObject(exec);

    JSValue* v;

    ASSERT(m_operator != OpEqual);
    PropertySlot slot;
    JSValue* v1 = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE
    v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_operator);

    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, m_ident, v);
    return v;
}

// ------------------------------ AssignErrorNode -----------------------------------

RegisterID* AssignErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, "Left side of assignment is not a reference.");
}

JSValue* AssignErrorNode::evaluate(OldInterpreterExecState* exec)
{
    throwError(exec, ReferenceError, "Left side of assignment is not a reference.");
    handleException(exec);
    return jsUndefined();
}

// ------------------------------ AssignBracketNode -----------------------------------

RegisterID* AssignBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments || m_rightHasAssignments);
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript.get(), m_rightHasAssignments);
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RegisterID* result = generator.emitNode(value.get(), m_right.get());
    generator.emitPutByVal(base.get(), property.get(), result);
    return generator.moveToDestinationIfNeeded(dst, result);
}

void AssignBracketNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue* AssignBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* subscript = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* base = baseValue->toObject(exec);

    uint32_t propertyIndex;
    if (subscript->getUInt32(propertyIndex)) {
        JSValue* v = m_right->evaluate(exec);
        KJS_CHECKEXCEPTIONVALUE

        base->put(exec, propertyIndex, v);
        return v;
    }

    Identifier propertyName(subscript->toString(exec));
    JSValue* v = m_right->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, propertyName, v);
    return v;
}

RegisterID* ReadModifyBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments || m_rightHasAssignments);
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript.get(), m_rightHasAssignments);

    RefPtr<RegisterID> value = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator);

    generator.emitPutByVal(base.get(), property.get(), updatedValue);

    return updatedValue;
}

void ReadModifyBracketNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue* ReadModifyBracketNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* baseValue = m_base->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* subscript = m_subscript->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    JSObject* base = baseValue->toObject(exec);

    uint32_t propertyIndex;
    if (subscript->getUInt32(propertyIndex)) {
        JSValue* v;
        ASSERT(m_operator != OpEqual);
        PropertySlot slot;
        JSValue* v1 = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
        KJS_CHECKEXCEPTIONVALUE
        v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_operator);

        KJS_CHECKEXCEPTIONVALUE

        base->put(exec, propertyIndex, v);
        return v;
    }

    Identifier propertyName(subscript->toString(exec));
    JSValue* v;

    ASSERT(m_operator != OpEqual);
    PropertySlot slot;
    JSValue* v1 = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE
    v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_operator);

    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, propertyName, v);
    return v;
}

// ------------------------------ CommaNode ------------------------------------

RegisterID* CommaNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(m_expr1.get());
    return generator.emitNode(dst, m_expr2.get());
}

void CommaNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 11.14
JSValue* CommaNode::evaluate(OldInterpreterExecState* exec)
{
    m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return m_expr2->evaluate(exec);
}

// ------------------------------ ConstDeclNode ----------------------------------

ConstDeclNode::ConstDeclNode(const Identifier& ident, ExpressionNode* init)
    : m_ident(ident)
    , m_init(init)
{
}

void ConstDeclNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_next)
        nodeStack.append(m_next.get());
    if (m_init)
        nodeStack.append(m_init.get());
}

void ConstDeclNode::handleSlowCase(OldInterpreterExecState* exec, const ScopeChain& chain, JSValue* val)
{
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // We must always have something in the scope chain
    ASSERT(iter != end);

    JSObject* base;

    do {
        base = *iter;
        if (base->isVariableObject())
            break;
        ++iter;
    } while (iter != end);

    ASSERT(base && base->isVariableObject());

    static_cast<JSVariableObject*>(base)->putWithAttributes(exec, m_ident, val, ReadOnly);
}

// ECMA 12.2
inline void ConstDeclNode::evaluateSingle(OldInterpreterExecState* exec)
{
    ASSERT(exec->variableObject()->hasOwnProperty(exec, m_ident) || exec->codeType() == EvalCode); // Guaranteed by processDeclarations.
    const ScopeChain& chain = exec->scopeChain();
    JSVariableObject* variableObject = exec->variableObject();

    bool inGlobalScope = ++chain.begin() == chain.end();

    if (m_init) {
        if (inGlobalScope) {
            JSValue* val = m_init->evaluate(exec);
            unsigned attributes = ReadOnly;
            if (exec->codeType() != EvalCode)
                attributes |= DontDelete;
            variableObject->putWithAttributes(exec, m_ident, val, attributes);
        } else {
            JSValue* val = m_init->evaluate(exec);
            KJS_CHECKEXCEPTIONVOID

            // if the variable object is the top of the scope chain, then that must
            // be where this variable is declared, processVarDecls would have put
            // it there. Don't search the scope chain, to optimize this very common case.
            if (chain.top() != variableObject)
                return handleSlowCase(exec, chain, val);

            variableObject->putWithAttributes(exec, m_ident, val, ReadOnly);
        }
    }
}

RegisterID* ConstDeclNode::emitCodeSingle(CodeGenerator& generator)
{
    if (RegisterID* local = generator.registerForLocalConstInit(m_ident))
        return generator.emitNode(local, m_init.get());

    // FIXME: While this code should only be hit in eval code, it will potentially
    // assign to the wrong base if m_ident exists in an intervening dynamic scope.
    RefPtr<RegisterID> base = generator.emitResolveBase(generator.newTemporary(), m_ident);
    RegisterID* value = generator.emitNode(m_init.get());
    return generator.emitPutById(base.get(), m_ident, value);
}

RegisterID* ConstDeclNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    RegisterID* result = 0;
    for (ConstDeclNode* n = this; n; n = n->m_next.get())
        result = n->emitCodeSingle(generator);

    return result;
}

JSValue* ConstDeclNode::evaluate(OldInterpreterExecState* exec)
{
    evaluateSingle(exec);

    if (ConstDeclNode* n = m_next.get()) {
        do {
            n->evaluateSingle(exec);
            KJS_CHECKEXCEPTIONVALUE
            n = n->m_next.get();
        } while (n);
    }
    return jsUndefined();
}

// ------------------------------ ConstStatementNode -----------------------------

void ConstStatementNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    ASSERT(m_next);
    nodeStack.append(m_next.get());
}

RegisterID* ConstStatementNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return generator.emitNode(m_next.get());
}

// ECMA 12.2
JSValue* ConstStatementNode::execute(OldInterpreterExecState* exec)
{
    m_next->evaluate(exec);
    KJS_CHECKEXCEPTION

    return exec->setNormalCompletion();
}

// ------------------------------ Helper functions for handling Vectors of StatementNode -------------------------------

static inline RegisterID* statementListEmitCode(StatementVector& statements, CodeGenerator& generator, RegisterID* dst = 0)
{
    RefPtr<RegisterID> r0 = dst;

    StatementVector::iterator end = statements.end();
    for (StatementVector::iterator it = statements.begin(); it != end; ++it) {
        StatementNode* n = it->get();
        generator.emitDebugHook(WillExecuteStatement, n->firstLine(), n->lastLine());
        if (RegisterID* r1 = generator.emitNode(dst, n))
            r0 = r1;
    }
    
    return r0.get();
}

static inline void statementListPushFIFO(StatementVector& statements, DeclarationStacks::NodeStack& stack)
{
    StatementVector::iterator it = statements.end();
    StatementVector::iterator begin = statements.begin();
    while (it != begin) {
        --it;
        stack.append((*it).get());
    }
}

static inline Node* statementListInitializeVariableAccessStack(StatementVector& statements, DeclarationStacks::NodeStack& stack)
{
    if (statements.isEmpty())
        return 0;

    StatementVector::iterator it = statements.end();
    StatementVector::iterator begin = statements.begin();
    StatementVector::iterator beginPlusOne = begin + 1;

    while (it != beginPlusOne) {
        --it;
        stack.append((*it).get());
    }

    return (*begin).get();
}

static inline JSValue* statementListExecute(StatementVector& statements, OldInterpreterExecState* exec)
{
    JSValue* value = 0;
    size_t size = statements.size();
    for (size_t i = 0; i != size; ++i) {
        JSValue* statementValue = statements[i]->execute(exec);
        if (statementValue)
            value = statementValue;
        if (exec->completionType() != Normal)
            return value;
    }
    return exec->setNormalCompletion(value);
}

// ------------------------------ BlockNode ------------------------------------

BlockNode::BlockNode(SourceElements* children)
{
    if (children)
        children->releaseContentsIntoVector(m_children);
}

RegisterID* BlockNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return statementListEmitCode(m_children, generator, dst);
}

void BlockNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    statementListPushFIFO(m_children, nodeStack);
}

// ECMA 12.1
JSValue* BlockNode::execute(OldInterpreterExecState* exec)
{
    return statementListExecute(m_children, exec);
}

// ------------------------------ EmptyStatementNode ---------------------------

RegisterID* EmptyStatementNode::emitCode(CodeGenerator&, RegisterID* dst)
{
    return dst;
}

// ECMA 12.3
JSValue* EmptyStatementNode::execute(OldInterpreterExecState* exec)
{
    return exec->setNormalCompletion();
}

// ------------------------------ ExprStatementNode ----------------------------

RegisterID* ExprStatementNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    return generator.emitNode(dst, m_expr.get());
}

void ExprStatementNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    ASSERT(m_expr);
    nodeStack.append(m_expr.get());
}

// ECMA 12.4
JSValue* ExprStatementNode::execute(OldInterpreterExecState* exec)
{
    JSValue* value = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTION

    return exec->setNormalCompletion(value);
}

// ------------------------------ VarStatementNode ----------------------------

RegisterID* VarStatementNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    ASSERT(m_expr);
    return generator.emitNode(m_expr.get());
}

void VarStatementNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    ASSERT(m_expr);
    nodeStack.append(m_expr.get());
}

JSValue* VarStatementNode::execute(OldInterpreterExecState* exec)
{
    m_expr->evaluate(exec);
    KJS_CHECKEXCEPTION

    return exec->setNormalCompletion();
}

// ------------------------------ IfNode ---------------------------------------

RegisterID* IfNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> afterThen = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, afterThen.get());

    generator.emitNode(dst, m_ifBlock.get());
    generator.emitLabel(afterThen.get());

    // FIXME: This should return the last statement exectuted so that it can be returned as a Completion
    return 0;
}

void IfNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_ifBlock.get());
    nodeStack.append(m_condition.get());
}

// ECMA 12.5
JSValue* IfNode::execute(OldInterpreterExecState* exec)
{
    bool b = m_condition->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTION

    if (b)
        return m_ifBlock->execute(exec);
    return exec->setNormalCompletion();
}

RegisterID* IfElseNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> beforeElse = generator.newLabel();
    RefPtr<LabelID> afterElse = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, beforeElse.get());

    generator.emitNode(dst, m_ifBlock.get());
    generator.emitJump(afterElse.get());

    generator.emitLabel(beforeElse.get());
    generator.emitNode(dst, m_elseBlock.get());

    generator.emitLabel(afterElse.get());

    // FIXME: This should return the last statement exectuted so that it can be returned as a Completion
    return 0;
}

void IfElseNode::optimizeVariableAccess(OldInterpreterExecState* exec, const SymbolTable& symbolTable, const LocalStorage& localStorage, NodeStack& nodeStack)
{
    nodeStack.append(m_elseBlock.get());
    IfNode::optimizeVariableAccess(exec, symbolTable, localStorage, nodeStack);
}

// ECMA 12.5
JSValue* IfElseNode::execute(OldInterpreterExecState* exec)
{
    bool b = m_condition->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTION

    if (b)
        return m_ifBlock->execute(exec);
    return m_elseBlock->execute(exec);
}

// ------------------------------ DoWhileNode ----------------------------------

RegisterID* DoWhileNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());

    RefPtr<LabelID> continueTarget = generator.newLabel();
    RefPtr<LabelID> breakTarget = generator.newLabel();
    
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    RefPtr<RegisterID> result = generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();
    
    generator.emitLabel(continueTarget.get());
    RegisterID* cond = generator.emitNode(m_expr.get());
    generator.emitJumpIfTrue(cond, topOfLoop.get());
    generator.emitLabel(breakTarget.get());
    return result.get();
}

void DoWhileNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_statement.get());
    nodeStack.append(m_expr.get());
}

// ECMA 12.6.1
JSValue* DoWhileNode::execute(OldInterpreterExecState* exec)
{
    JSValue* value = 0;

    while (1) {
        exec->pushIteration();
        JSValue* statementValue = m_statement->execute(exec);
        exec->popIteration();

        if (exec->dynamicGlobalObject()->timedOut())
            return exec->setInterruptedCompletion();

        if (statementValue)
            value = statementValue;

        if (exec->completionType() != Normal) {
            if (exec->completionType() == Continue && m_labelStack.contains(exec->breakOrContinueTarget()))
                goto continueDoWhileLoop;
            if (exec->completionType() == Break && m_labelStack.contains(exec->breakOrContinueTarget()))
                break;
            return statementValue;
        }

    continueDoWhileLoop:
        bool b = m_expr->evaluateToBoolean(exec);
        KJS_CHECKEXCEPTION
        if (!b)
            break;
    }

    return exec->setNormalCompletion(value);
}

// ------------------------------ WhileNode ------------------------------------

RegisterID* WhileNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> topOfLoop = generator.newLabel();
    RefPtr<LabelID> continueTarget = generator.newLabel();
    RefPtr<LabelID> breakTarget = generator.newLabel();

    generator.emitJump(continueTarget.get());
    generator.emitLabel(topOfLoop.get());
    
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();

    generator.emitLabel(continueTarget.get());
    RegisterID* cond = generator.emitNode(m_expr.get());
    generator.emitJumpIfTrue(cond, topOfLoop.get());

    generator.emitLabel(breakTarget.get());
    
    // FIXME: This should return the last statement executed so that it can be returned as a Completion
    return 0;
}

void WhileNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_statement.get());
    nodeStack.append(m_expr.get());
}

// ECMA 12.6.2
JSValue* WhileNode::execute(OldInterpreterExecState* exec)
{
    JSValue* value = 0;

    while (1) {
        bool b = m_expr->evaluateToBoolean(exec);
        KJS_CHECKEXCEPTION
        if (!b)
            break;

        exec->pushIteration();
        JSValue* statementValue = m_statement->execute(exec);
        exec->popIteration();

        if (exec->dynamicGlobalObject()->timedOut())
            return exec->setInterruptedCompletion();

        if (statementValue)
            value = statementValue;

        if (exec->completionType() != Normal) {
            if (exec->completionType() == Continue && m_labelStack.contains(exec->breakOrContinueTarget()))
                continue;
            if (exec->completionType() == Break && m_labelStack.contains(exec->breakOrContinueTarget()))
                break;
            return statementValue;
        }
    }

    return exec->setNormalCompletion(value);
}

// ------------------------------ ForNode --------------------------------------

RegisterID* ForNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(m_expr1.get());
    
    RefPtr<LabelID> topOfLoop = generator.newLabel();
    RefPtr<LabelID> beforeCondition = generator.newLabel();
    RefPtr<LabelID> continueTarget = generator.newLabel(); 
    RefPtr<LabelID> breakTarget = generator.newLabel(); 
    generator.emitJump(beforeCondition.get());

    generator.emitLabel(topOfLoop.get());
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    RefPtr<RegisterID> result = generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();
    generator.emitLabel(continueTarget.get());  
    generator.emitNode(m_expr3.get());

    generator.emitLabel(beforeCondition.get());
    RegisterID* cond = generator.emitNode(m_expr2.get());
    generator.emitJumpIfTrue(cond, topOfLoop.get());
    generator.emitLabel(breakTarget.get());
    return result.get();
}

void ForNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_statement.get());
    nodeStack.append(m_expr3.get());
    nodeStack.append(m_expr2.get());
    nodeStack.append(m_expr1.get());
}

// ECMA 12.6.3
JSValue* ForNode::execute(OldInterpreterExecState* exec)
{
    JSValue* value = 0;

    m_expr1->evaluate(exec);
    KJS_CHECKEXCEPTION

    while (1) {
        bool b = m_expr2->evaluateToBoolean(exec);
        KJS_CHECKEXCEPTION
        if (!b)
            break;

        exec->pushIteration();
        JSValue* statementValue = m_statement->execute(exec);
        exec->popIteration();
        if (statementValue)
            value = statementValue;

        if (exec->dynamicGlobalObject()->timedOut())
            return exec->setInterruptedCompletion();

        if (exec->completionType() != Normal) {
            if (exec->completionType() == Continue && m_labelStack.contains(exec->breakOrContinueTarget()))
                goto continueForLoop;
            if (exec->completionType() == Break && m_labelStack.contains(exec->breakOrContinueTarget()))
                break;
            return statementValue;
        }

    continueForLoop:
        m_expr3->evaluate(exec);
        KJS_CHECKEXCEPTION
    }

    return exec->setNormalCompletion(value);
}

// ------------------------------ ForInNode ------------------------------------

ForInNode::ForInNode(ExpressionNode* l, ExpressionNode* expr, StatementNode* statement)
    : m_init(0L)
    , m_lexpr(l)
    , m_expr(expr)
    , m_statement(statement)
    , m_identIsVarDecl(false)
{
}

ForInNode::ForInNode(const Identifier& ident, ExpressionNode* in, ExpressionNode* expr, StatementNode* statement)
    : m_ident(ident)
    , m_lexpr(new ResolveNode(ident))
    , m_expr(expr)
    , m_statement(statement)
    , m_identIsVarDecl(true)
{
    if (in)
        m_init = new AssignResolveNode(ident, in, true);
    // for( var foo = bar in baz )
}

void ForInNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_statement.get());
    nodeStack.append(m_expr.get());
    nodeStack.append(m_lexpr.get());
    if (m_init)
        nodeStack.append(m_init.get());
}

RegisterID* ForInNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> loopStart = generator.newLabel();
    RefPtr<LabelID> continueTarget = generator.newLabel(); 
    RefPtr<LabelID> breakTarget = generator.newLabel(); 

    if (m_init)
        generator.emitNode(m_init.get());
    RegisterID* forInBase = generator.emitNode(m_expr.get());
    RefPtr<RegisterID> iter = generator.emitGetPropertyNames(generator.newTemporary(), forInBase);
    generator.emitJump(continueTarget.get());
    generator.emitLabel(loopStart.get());
    RegisterID* propertyName;
    if (m_lexpr->isResolveNode()) {
        const Identifier& ident = static_cast<ResolveNode*>(m_lexpr.get())->identifier();
        propertyName = generator.registerForLocal(ident);
        if (!propertyName) {
            propertyName = generator.newTemporary();
            RefPtr<RegisterID> protect = propertyName;
            RegisterID* base = generator.emitResolveBase(generator.newTemporary(), ident);
            generator.emitPutById(base, ident, propertyName);
        }
    } else if (m_lexpr->isDotAccessorNode()) {
        DotAccessorNode* assignNode = static_cast<DotAccessorNode*>(m_lexpr.get());
        const Identifier& ident = assignNode->identifier();
        propertyName = generator.newTemporary();
        RefPtr<RegisterID> protect = propertyName;
        RegisterID* base = generator.emitNode(assignNode->base());
        generator.emitPutById(base, ident, propertyName);
    } else {
        ASSERT(m_lexpr->isBracketAccessorNode());
        BracketAccessorNode* assignNode = static_cast<BracketAccessorNode*>(m_lexpr.get());
        propertyName = generator.newTemporary();
        RefPtr<RegisterID> protect = propertyName;
        RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
        RegisterID* subscript = generator.emitNode(assignNode->subscript());
        generator.emitPutByVal(base.get(), subscript, propertyName);
    }   
    
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();

    generator.emitLabel(continueTarget.get());
    generator.emitNextPropertyName(propertyName, iter.get(), loopStart.get());
    generator.emitLabel(breakTarget.get());
    return dst;
}

// ECMA 12.6.4
JSValue* ForInNode::execute(OldInterpreterExecState* exec)
{
    JSValue* value = 0;

    if (m_init) {
        m_init->evaluate(exec);
        KJS_CHECKEXCEPTION
    }

    JSValue* e = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTION

    // For Null and Undefined, we want to make sure not to go through
    // the loop at all, because toObject will throw an exception.
    if (e->isUndefinedOrNull())
        return exec->setNormalCompletion();

    JSObject* v = e->toObject(exec);
    PropertyNameArray propertyNames;
    v->getPropertyNames(exec, propertyNames);

    PropertyNameArray::const_iterator end = propertyNames.end();
    for (PropertyNameArray::const_iterator it = propertyNames.begin(); it != end; ++it) {
        const Identifier& name = *it;
        if (!v->hasProperty(exec, name))
            continue;

        JSValue* str = jsOwnedString(name.ustring());

        if (m_lexpr->isResolveNode()) {
            const Identifier& ident = static_cast<ResolveNode*>(m_lexpr.get())->identifier();

            const ScopeChain& chain = exec->scopeChain();
            ScopeChainIterator iter = chain.begin();
            ScopeChainIterator end = chain.end();

            // we must always have something in the scope chain
            ASSERT(iter != end);

            PropertySlot slot;
            JSObject* o;
            do {
                o = *iter;
                if (o->getPropertySlot(exec, ident, slot)) {
                    o->put(exec, ident, str);
                    break;
                }
                ++iter;
            } while (iter != end);

            if (iter == end)
                o->put(exec, ident, str);
        } else if (m_lexpr->isDotAccessorNode()) {
            const Identifier& ident = static_cast<DotAccessorNode*>(m_lexpr.get())->identifier();
            JSValue* v = static_cast<DotAccessorNode*>(m_lexpr.get())->base()->evaluate(exec);
            KJS_CHECKEXCEPTION
            JSObject* o = v->toObject(exec);

            o->put(exec, ident, str);
        } else {
            ASSERT(m_lexpr->isBracketAccessorNode());
            JSValue* v = static_cast<BracketAccessorNode*>(m_lexpr.get())->base()->evaluate(exec);
            KJS_CHECKEXCEPTION
            JSValue* v2 = static_cast<BracketAccessorNode*>(m_lexpr.get())->subscript()->evaluate(exec);
            KJS_CHECKEXCEPTION
            JSObject* o = v->toObject(exec);

            uint32_t i;
            if (v2->getUInt32(i))
                o->put(exec, i, str);
            o->put(exec, Identifier(v2->toString(exec)), str);
        }

        KJS_CHECKEXCEPTION

        exec->pushIteration();
        JSValue* statementValue = m_statement->execute(exec);
        exec->popIteration();
        if (statementValue)
            value = statementValue;
        
        if (exec->dynamicGlobalObject()->timedOut())
            return exec->setInterruptedCompletion();

        if (exec->completionType() != Normal) {
            if (exec->completionType() == Continue && m_labelStack.contains(exec->breakOrContinueTarget()))
                continue;
            if (exec->completionType() == Break && m_labelStack.contains(exec->breakOrContinueTarget()))
                break;
            return statementValue;
        }
    }

    return exec->setNormalCompletion(value);
}

// ------------------------------ ContinueNode ---------------------------------

// ECMA 12.7
RegisterID* ContinueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (!generator.inContinueContext())
        return emitThrowError(generator, SyntaxError, "Invalid continue statement.");

    JumpContext* targetContext = generator.jumpContextForContinue(m_ident);

    if (!targetContext) {
        if (m_ident.isEmpty())
            return emitThrowError(generator, SyntaxError, "Invalid continue statement.");
        else
            return emitThrowError(generator, SyntaxError, "Label %s not found.", m_ident);
    }

    if (!targetContext->continueTarget)
        return emitThrowError(generator, SyntaxError, "Invalid continue statement.");        

    generator.emitJumpScopes(targetContext->continueTarget, targetContext->scopeDepth);
    
    return dst;
}

JSValue* ContinueNode::execute(OldInterpreterExecState* exec)
{
    if (m_ident.isEmpty() && !exec->inIteration())
        return setErrorCompletion(exec, SyntaxError, "Invalid continue statement.");
    if (!m_ident.isEmpty() && !exec->seenLabels().contains(m_ident))
        return setErrorCompletion(exec, SyntaxError, "Label %s not found.", m_ident);
    return exec->setContinueCompletion(&m_ident);
}

// ------------------------------ BreakNode ------------------------------------

// ECMA 12.8
RegisterID* BreakNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (!generator.inJumpContext())
        return emitThrowError(generator, SyntaxError, "Invalid break statement.");
    
    JumpContext* targetContext = generator.jumpContextForBreak(m_ident);
    
    if (!targetContext) {
        if (m_ident.isEmpty())
            return emitThrowError(generator, SyntaxError, "Invalid break statement.");
        else
            return emitThrowError(generator, SyntaxError, "Label %s not found.", m_ident);
    }

    ASSERT(targetContext->breakTarget);

    generator.emitJumpScopes(targetContext->breakTarget, targetContext->scopeDepth);

    return dst;
}

JSValue* BreakNode::execute(OldInterpreterExecState* exec)
{
    if (m_ident.isEmpty() && !exec->inIteration() && !exec->inSwitch())
        return setErrorCompletion(exec, SyntaxError, "Invalid break statement.");
    if (!m_ident.isEmpty() && !exec->seenLabels().contains(m_ident))
        return setErrorCompletion(exec, SyntaxError, "Label %s not found.");
    return exec->setBreakCompletion(&m_ident);
}

// ------------------------------ ReturnNode -----------------------------------

RegisterID* ReturnNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.codeType() != FunctionCode)
        return emitThrowError(generator, SyntaxError, "Invalid return statement.");
        
    RegisterID* r0 = m_value ? generator.emitNode(dst, m_value.get()) : generator.emitLoad(generator.finalDestination(dst), jsUndefined());
    if (generator.scopeDepth()) {
        RefPtr<LabelID> l0 = generator.newLabel();
        generator.emitJumpScopes(l0.get(), 0);
        generator.emitLabel(l0.get());
    }
    generator.emitDebugHook(WillLeaveCallFrame, firstLine(), lastLine());
    return generator.emitReturn(r0);
}

void ReturnNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_value)
        nodeStack.append(m_value.get());
}

// ECMA 12.9
JSValue* ReturnNode::execute(OldInterpreterExecState* exec)
{
    CodeType codeType = exec->codeType();
    if (codeType != FunctionCode)
        return setErrorCompletion(exec, SyntaxError, "Invalid return statement.");

    if (!m_value)
        return exec->setReturnValueCompletion(jsUndefined());

    JSValue* v = m_value->evaluate(exec);
    KJS_CHECKEXCEPTION

    return exec->setReturnValueCompletion(v);
}

// ------------------------------ WithNode -------------------------------------

RegisterID* WithNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> scope = generator.emitNode(m_expr.get()); // scope must be protected until popped
    generator.emitPushScope(scope.get());
    RegisterID* result = generator.emitNode(dst, m_statement.get());
    generator.emitPopScope();
    return result;
}

void WithNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    // Can't optimize within statement because "with" introduces a dynamic scope.
    nodeStack.append(m_expr.get());
}

// ECMA 12.10
JSValue* WithNode::execute(OldInterpreterExecState*)
{
    ASSERT_NOT_REACHED();
    return 0;
}

// ------------------------------ CaseClauseNode -------------------------------

void CaseClauseNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_expr)
        nodeStack.append(m_expr.get());
    statementListPushFIFO(m_children, nodeStack);
}

// ECMA 12.11
JSValue* CaseClauseNode::evaluate(OldInterpreterExecState* exec)
{
    JSValue* v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    return v;
}

// ECMA 12.11
JSValue* CaseClauseNode::executeStatements(OldInterpreterExecState* exec)
{
    return statementListExecute(m_children, exec);
}

// ------------------------------ ClauseListNode -------------------------------

void ClauseListNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_next)
        nodeStack.append(m_next.get());
    nodeStack.append(m_clause.get());
}

// ------------------------------ CaseBlockNode --------------------------------

RegisterID* CaseBlockNode::emitCodeForBlock(CodeGenerator& generator, RegisterID* switchExpression, RegisterID* dst)
{
    Vector<RefPtr<LabelID>, 8> labelVector;

    // Setup jumps
    for (ClauseListNode* list = m_list1.get(); list; list = list->getNext()) {
        RegisterID* clauseVal = generator.emitNode(list->getClause()->expr());
        generator.emitStrictEqual(clauseVal, clauseVal, switchExpression);
        labelVector.append(generator.newLabel());
        generator.emitJumpIfTrue(clauseVal, labelVector[labelVector.size() - 1].get());
    }

    for (ClauseListNode* list = m_list2.get(); list; list = list->getNext()) {
        RegisterID* clauseVal = generator.emitNode(list->getClause()->expr());
        generator.emitStrictEqual(clauseVal, clauseVal, switchExpression);
        labelVector.append(generator.newLabel());
        generator.emitJumpIfTrue(clauseVal, labelVector[labelVector.size() - 1].get());
    }

    RefPtr<LabelID> defaultLabel;
    defaultLabel = generator.newLabel();
    generator.emitJump(defaultLabel.get());

    RegisterID* result = 0;

    size_t i = 0;
    for (ClauseListNode* list = m_list1.get(); list; list = list->getNext()) {
        generator.emitLabel(labelVector[i++].get());
        result = statementListEmitCode(list->getClause()->children(), generator, dst);
    }

    if (m_defaultClause) {
        generator.emitLabel(defaultLabel.get());
        result = statementListEmitCode(m_defaultClause->children(), generator, dst);
    }

    for (ClauseListNode* list = m_list2.get(); list; list = list->getNext()) {
        generator.emitLabel(labelVector[i++].get());
        result = statementListEmitCode(list->getClause()->children(), generator, dst);
    }
    if (!m_defaultClause)
        generator.emitLabel(defaultLabel.get());

    ASSERT(i == labelVector.size());

    return result;
}

void CaseBlockNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    if (m_list2)
        nodeStack.append(m_list2.get());
    if (m_defaultClause)
        nodeStack.append(m_defaultClause.get());
    if (m_list1)
        nodeStack.append(m_list1.get());
}

// ECMA 12.11
JSValue* CaseBlockNode::executeBlock(OldInterpreterExecState* exec, JSValue* input)
{
    ClauseListNode* a = m_list1.get();
    while (a) {
        CaseClauseNode* clause = a->getClause();
        a = a->getNext();
        JSValue* v = clause->evaluate(exec);
        KJS_CHECKEXCEPTION
        if (strictEqual(input, v)) {
            JSValue* res = clause->executeStatements(exec);
            if (exec->completionType() != Normal)
                return res;
            for (; a; a = a->getNext()) {
                JSValue* res = a->getClause()->executeStatements(exec);
                if (exec->completionType() != Normal)
                    return res;
            }
            break;
        }
    }

    ClauseListNode* b = m_list2.get();
    while (b) {
        CaseClauseNode* clause = b->getClause();
        b = b->getNext();
        JSValue* v = clause->evaluate(exec);
        KJS_CHECKEXCEPTION
        if (strictEqual(input, v)) {
            JSValue* res = clause->executeStatements(exec);
            if (exec->completionType() != Normal)
                return res;
            goto step18;
        }
    }

    // default clause
    if (m_defaultClause) {
        JSValue* res = m_defaultClause->executeStatements(exec);
        if (exec->completionType() != Normal)
            return res;
    }
    b = m_list2.get();
step18:
    while (b) {
        CaseClauseNode* clause = b->getClause();
        JSValue* res = clause->executeStatements(exec);
        if (exec->completionType() != Normal)
            return res;
        b = b->getNext();
    }

    // bail out on error
    KJS_CHECKEXCEPTION

    return exec->setNormalCompletion();
}

// ------------------------------ SwitchNode -----------------------------------

RegisterID* SwitchNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> breakTarget = generator.newLabel();

    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    generator.pushJumpContext(&m_labelStack, 0, breakTarget.get(), true);
    RegisterID* r1 = m_block->emitCodeForBlock(generator, r0.get(), dst);
    generator.popJumpContext();

    generator.emitLabel(breakTarget.get());

    return r1;
}

void SwitchNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_block.get());
    nodeStack.append(m_expr.get());
}

// ECMA 12.11
JSValue* SwitchNode::execute(OldInterpreterExecState* exec)
{
    JSValue* v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTION

    exec->pushSwitch();
    JSValue* result = m_block->executeBlock(exec, v);
    exec->popSwitch();

    if (exec->completionType() == Break && m_labelStack.contains(exec->breakOrContinueTarget()))
        exec->setCompletionType(Normal);
    return result;
}

// ------------------------------ LabelNode ------------------------------------
RegisterID* LabelNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.jumpContextForBreak(m_label))
        return emitThrowError(generator, SyntaxError, "Duplicated label %s found.", m_label);
    
    RefPtr<LabelID> l0 = generator.newLabel();
    m_labelStack.push(m_label);
    generator.pushJumpContext(&m_labelStack, 0, l0.get(), false);
    
    RegisterID* r0 = generator.emitNode(dst, m_statement.get());
    
    generator.popJumpContext();
    m_labelStack.pop();
    
    generator.emitLabel(l0.get());
    return r0;
}

void LabelNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_statement.get());
}

// ECMA 12.12
JSValue* LabelNode::execute(OldInterpreterExecState* exec)
{
    if (!exec->seenLabels().push(m_label))
        return setErrorCompletion(exec, SyntaxError, "Duplicated label %s found.", m_label);
    JSValue* result = m_statement->execute(exec);
    exec->seenLabels().pop();

    if (exec->completionType() == Break && exec->breakOrContinueTarget() == m_label)
        exec->setCompletionType(Normal);
    return result;
}

// ------------------------------ ThrowNode ------------------------------------

RegisterID* ThrowNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitThrow(generator.emitNode(dst, m_expr.get()));
    return dst;
}

void ThrowNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

// ECMA 12.13
JSValue* ThrowNode::execute(OldInterpreterExecState* exec)
{
    JSValue* v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTION

    handleException(exec, v);
    return exec->setThrowCompletion(v);
}

// ------------------------------ TryNode --------------------------------------

RegisterID* TryNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> tryStartLabel = generator.newLabel();
    RefPtr<LabelID> tryEndLabel = generator.newLabel();
    RefPtr<LabelID> finallyStart;
    RefPtr<RegisterID> finallyReturnAddr;
    if (m_finallyBlock) {
        finallyStart = generator.newLabel();
        finallyReturnAddr = generator.newTemporary();
        generator.pushFinallyContext(finallyStart.get(), finallyReturnAddr.get());
    }
    generator.emitLabel(tryStartLabel.get());
    generator.emitNode(dst, m_tryBlock.get());
    generator.emitLabel(tryEndLabel.get());

    if (m_catchBlock) {
        RefPtr<LabelID> handlerEndLabel = generator.newLabel();
        generator.emitJump(handlerEndLabel.get());
        RefPtr<RegisterID> exceptionRegister = generator.emitCatch(generator.newTemporary(), tryStartLabel.get(), tryEndLabel.get());
        RefPtr<RegisterID> newScope = generator.emitNewObject(generator.newTemporary()); // scope must be protected until popped
        generator.emitPutById(newScope.get(), m_exceptionIdent, exceptionRegister.get());
        exceptionRegister = 0; // Release register used for temporaries
        generator.emitPushScope(newScope.get());
        generator.emitNode(dst, m_catchBlock.get());
        generator.emitPopScope();
        generator.emitLabel(handlerEndLabel.get());
    }

    if (m_finallyBlock) {
        generator.popFinallyContext();
        RefPtr<LabelID> finallyEndLabel = generator.newLabel();
        generator.emitJumpSubroutine(finallyReturnAddr.get(), finallyStart.get());
        generator.emitJump(finallyEndLabel.get());

        // Finally block for exception path
        RefPtr<RegisterID> tempExceptionRegister = generator.emitCatch(generator.newTemporary(), tryStartLabel.get(), generator.emitLabel(generator.newLabel().get()).get());
        generator.emitJumpSubroutine(finallyReturnAddr.get(), finallyStart.get());
        generator.emitThrow(tempExceptionRegister.get());

        // emit the finally block itself
        generator.emitLabel(finallyStart.get());
        generator.emitNode(dst, m_finallyBlock.get());
        generator.emitSubroutineReturn(finallyReturnAddr.get());

        generator.emitLabel(finallyEndLabel.get());
    }

    return dst;
}


void TryNode::optimizeVariableAccess(OldInterpreterExecState*, const SymbolTable&, const LocalStorage&, NodeStack& nodeStack)
{
    // Can't optimize within catchBlock because "catch" introduces a dynamic scope.
    if (m_finallyBlock)
        nodeStack.append(m_finallyBlock.get());
    nodeStack.append(m_tryBlock.get());
}

// ECMA 12.14
JSValue* TryNode::execute(OldInterpreterExecState*)
{
    ASSERT_NOT_REACHED();
    return 0;
}

// ------------------------------ FunctionBodyNode -----------------------------

ScopeNode::ScopeNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : BlockNode(children)
    , m_sourceURL(parser().sourceURL())
    , m_sourceId(parser().sourceId())
    , m_usesEval(usesEval)
    , m_needsClosure(needsClosure)
{
    if (varStack)
        m_varStack = *varStack;
    if (funcStack)
        m_functionStack = *funcStack;
}

// ------------------------------ ProgramNode -----------------------------

ProgramNode::ProgramNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : ScopeNode(children, varStack, funcStack, usesEval, needsClosure)
{
}

ProgramNode::~ProgramNode()
{
}

ProgramNode* ProgramNode::create(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
{
    return new ProgramNode(children, varStack, funcStack, usesEval, needsClosure);
}

// ------------------------------ EvalNode -----------------------------

EvalNode::EvalNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : ScopeNode(children, varStack, funcStack, usesEval, needsClosure)
{
}

EvalNode::~EvalNode()
{
}

RegisterID* EvalNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    statementListEmitCode(m_children, generator, dstRegister.get());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void EvalNode::generateCode(ScopeChainNode* sc)
{
    ScopeChain scopeChain(sc);
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(scopeChain.bottom());
    ASSERT(globalObject->isGlobalObject());
    
    m_code.set(new EvalCodeBlock(this, globalObject));
    SymbolTable symbolTable;
    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &symbolTable, m_code.get(), m_varStack, m_functionStack);
    generator.generate();

    m_children.shrinkCapacity(0);
}

EvalNode* EvalNode::create(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
{
    return new EvalNode(children, varStack, funcStack, usesEval, needsClosure);
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionBodyNode::FunctionBodyNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : ScopeNode(children, varStack, funcStack, usesEval, needsClosure)
{
}

FunctionBodyNode::~FunctionBodyNode()
{
}

void FunctionBodyNode::mark()
{
    if (m_code)
        m_code->mark();
}

FunctionBodyNode* FunctionBodyNode::create(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
{
    return new FunctionBodyNode(children, varStack, funcStack, usesEval, needsClosure);
}

void FunctionBodyNode::generateCode(ScopeChainNode* sc)
{
    m_code.set(new CodeBlock(this));

    ScopeChain scopeChain(sc);
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(scopeChain.bottom());
    ASSERT(globalObject->isGlobalObject());

    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_symbolTable, m_code.get(), m_varStack, m_functionStack, m_parameters);
    generator.generate();

    m_children.shrinkCapacity(0);
}

RegisterID* FunctionBodyNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(DidEnterCallFrame, firstLine(), lastLine());
    statementListEmitCode(m_children, generator);
    if (!m_children.size() || !m_children.last()->isReturnNode()) {
        RegisterID* r0 = generator.emitLoad(generator.newTemporary(), jsUndefined());
        generator.emitDebugHook(WillLeaveCallFrame, firstLine(), lastLine());
        generator.emitReturn(r0);
    }
    return 0;
}

RegisterID* ProgramNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    statementListEmitCode(m_children, generator, dstRegister.get());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void ProgramNode::generateCode(ScopeChainNode* sc, bool canCreateGlobals)
{
    ScopeChain scopeChain(sc);
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(scopeChain.bottom());
    ASSERT(globalObject->isGlobalObject());
    
    m_code.set(new ProgramCodeBlock(this, globalObject));
    
    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_code.get(), m_varStack, m_functionStack, canCreateGlobals);
    generator.generate();

    m_children.shrinkCapacity(0);
}

void ProgramNode::initializeSymbolTable(OldInterpreterExecState* exec)
{
    // If a previous script defined a symbol with the same name as one of our
    // symbols, to avoid breaking previously optimized nodes, we need to reuse
    // the symbol's existing storage index. So, we can't be as efficient as
    // FunctionBodyNode::initializeSymbolTable, which knows that no bindings
    // have yet been made.

    JSVariableObject* variableObject = exec->variableObject();
    SymbolTable& symbolTable = variableObject->symbolTable();

    size_t localStorageIndex = symbolTable.size();
    size_t size;

    // Order must match the order in processDeclarations.

    size = m_functionStack.size();
    m_functionIndexes.resize(size);
    for (size_t i = 0; i < size; ++i) {
        UString::Rep* rep = m_functionStack[i]->m_ident.ustring().rep();
        pair<SymbolTable::iterator, bool> result = symbolTable.add(rep, localStorageIndex);
        m_functionIndexes[i] = result.first->second.getIndex();
        if (result.second)
            ++localStorageIndex;
    }

    size = m_varStack.size();
    m_varIndexes.resize(size);
    for (size_t i = 0; i < size; ++i) {
        const Identifier& ident = m_varStack[i].first;
        if (variableObject->hasProperty(exec, ident)) {
            m_varIndexes[i] = missingSymbolMarker(); // Signal not to initialize this declaration.
            continue;
        }

        UString::Rep* rep = ident.ustring().rep();
        pair<SymbolTable::iterator, bool> result = symbolTable.add(rep, localStorageIndex);
        if (!result.second) {
            m_varIndexes[i] = missingSymbolMarker(); // Signal not to initialize this declaration.
            continue;
        }

        m_varIndexes[i] = result.first->second.getIndex();
        ++localStorageIndex;
    }
}

void ScopeNode::optimizeVariableAccess(OldInterpreterExecState* exec)
{
    NodeStack nodeStack;
    Node* node = statementListInitializeVariableAccessStack(m_children, nodeStack);
    if (!node)
        return;

    const SymbolTable& symbolTable = exec->variableObject()->symbolTable();
    ASSERT_NOT_REACHED();
    const LocalStorage localStorage;
    while (true) {
        node->optimizeVariableAccess(exec, symbolTable, localStorage, nodeStack);

        size_t size = nodeStack.size();
        if (!size)
            break;
        --size;
        node = nodeStack[size];
        nodeStack.shrink(size);
    }
}

static void gccIsCrazy() KJS_FAST_CALL;
static void gccIsCrazy()
{
}

void ProgramNode::processDeclarations(OldInterpreterExecState* exec)
{
    // If you remove this call, some SunSpider tests, including
    // bitops-nsieve-bits.js, will regress substantially on Mac, due to a ~40%
    // increase in L2 cache misses. FIXME: <rdar://problem/5657439> WTF?
    gccIsCrazy();

    initializeSymbolTable(exec);

    ASSERT_NOT_REACHED();
    LocalStorage localStorage;

    // We can't just resize localStorage here because that would temporarily
    // leave uninitialized entries, which would crash GC during the mark phase.
    localStorage.reserveCapacity(localStorage.size() + m_varStack.size() + m_functionStack.size());

    int minAttributes = DontDelete;

    // In order for our localStorage indexes to be correct, we must match the
    // order of addition in initializeSymbolTable().

    for (size_t i = 0, size = m_functionStack.size(); i < size; ++i) {
        FuncDeclNode* node = m_functionStack[i];
        LocalStorageEntry entry = LocalStorageEntry(node->makeFunction(exec, exec->scopeChain().node()), minAttributes);
        size_t index = m_functionIndexes[i];

        if (index == localStorage.size())
            localStorage.uncheckedAppend(entry);
        else {
            ASSERT(index < localStorage.size());
            localStorage[index] = entry;
        }
    }

    for (size_t i = 0, size = m_varStack.size(); i < size; ++i) {
        int index = m_varIndexes[i];
        if (index == missingSymbolMarker())
            continue;

        int attributes = minAttributes;
        if (m_varStack[i].second & DeclarationStacks::IsConstant)
            attributes |= ReadOnly;
        LocalStorageEntry entry = LocalStorageEntry(jsUndefined(), attributes);

        ASSERT(static_cast<unsigned>(index) == localStorage.size());
        localStorage.uncheckedAppend(entry);
    }

    optimizeVariableAccess(exec);
}

void EvalNode::processDeclarations(OldInterpreterExecState* exec)
{
    // We could optimize access to pre-existing symbols here, but SunSpider
    // reports that to be a net loss.

    size_t i;
    size_t size;

    JSVariableObject* variableObject = exec->variableObject();

    for (i = 0, size = m_varStack.size(); i < size; ++i) {
        Identifier& ident = m_varStack[i].first;
        if (variableObject->hasProperty(exec, ident))
            continue;
        int attributes = 0;
        if (m_varStack[i].second & DeclarationStacks::IsConstant)
            attributes = ReadOnly;
        variableObject->putWithAttributes(exec, ident, jsUndefined(), attributes);
    }

    for (i = 0, size = m_functionStack.size(); i < size; ++i) {
        FuncDeclNode* funcDecl = m_functionStack[i];
        variableObject->putWithAttributes(exec, funcDecl->m_ident, funcDecl->makeFunction(exec, exec->scopeChain().node()), 0);
    }
}

UString FunctionBodyNode::paramString() const
{
    UString s("");
    size_t count = m_parameters.size();
    for (size_t pos = 0; pos < count; ++pos) {
        if (!s.isEmpty())
            s += ", ";
        s += m_parameters[pos].ustring();
    }

    return s;
}

JSValue* ProgramNode::execute(OldInterpreterExecState* exec)
{
    processDeclarations(exec);
    return ScopeNode::execute(exec);
}

JSValue* EvalNode::execute(OldInterpreterExecState* exec)
{
    processDeclarations(exec);
    return ScopeNode::execute(exec);
}

// ------------------------------ FunctionBodyNodeWithDebuggerHooks ---------------------------------

FunctionBodyNodeWithDebuggerHooks::FunctionBodyNodeWithDebuggerHooks(SourceElements* children, DeclarationStacks::VarStack* varStack, DeclarationStacks::FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : FunctionBodyNode(children, varStack, funcStack, usesEval, needsClosure)
{
}

JSValue* FunctionBodyNodeWithDebuggerHooks::execute(OldInterpreterExecState* exec)
{
    JSValue* result = FunctionBodyNode::execute(exec);

    return result;
}

// ------------------------------ FuncDeclNode ---------------------------------

void FuncDeclNode::addParams()
{
    for (ParameterNode* p = m_parameter.get(); p; p = p->nextParam())
        m_body->parameters().append(p->ident());
}

FunctionImp* FuncDeclNode::makeFunction(ExecState* exec, ScopeChainNode* scopeChain)
{
    FunctionImp* func = new FunctionImp(exec, m_ident, m_body.get(), scopeChain);

    JSObject* proto = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());
    proto->putDirect(exec->propertyNames().constructor, func, DontEnum);
    func->putDirect(exec->propertyNames().prototype, proto, DontDelete);
    func->putDirect(exec->propertyNames().length, jsNumber(m_body->parameters().size()), ReadOnly | DontDelete | DontEnum);
    return func;
}

RegisterID* FuncDeclNode::emitCode(CodeGenerator&, RegisterID* dst)
{
    return dst;
}

JSValue* FuncDeclNode::execute(OldInterpreterExecState* exec)
{
    return exec->setNormalCompletion();
}

// ------------------------------ FuncExprNode ---------------------------------

RegisterID* FuncExprNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitNewFunctionExpression(generator.finalDestination(dst), this);
}

FunctionImp* FuncExprNode::makeFunction(ExecState* exec, ScopeChainNode* scopeChain)
{
    FunctionImp* func = new FunctionImp(exec, m_ident, m_body.get(), scopeChain);
    JSObject* proto = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());
    proto->putDirect(exec->propertyNames().constructor, func, DontEnum);
    func->putDirect(exec->propertyNames().prototype, proto, DontDelete);

    /* 
        The Identifier in a FunctionExpression can be referenced from inside
        the FunctionExpression's FunctionBody to allow the function to call
        itself recursively. However, unlike in a FunctionDeclaration, the
        Identifier in a FunctionExpression cannot be referenced from and
        does not affect the scope enclosing the FunctionExpression.
     */

    if (!m_ident.isNull()) {
        JSObject* functionScopeObject = new JSObject;
        functionScopeObject->putDirect(m_ident, func, ReadOnly | DontDelete);
        func->scope().push(functionScopeObject);
    }

    return func;
}

// ECMA 13
void FuncExprNode::addParams()
{
    for (ParameterNode* p = m_parameter.get(); p; p = p->nextParam())
        m_body->parameters().append(p->ident());
}

JSValue* FuncExprNode::evaluate(OldInterpreterExecState* exec)
{
    ASSERT_NOT_REACHED();

    bool named = !m_ident.isNull();
    JSObject* functionScopeObject = 0;

    if (named) {
        // named FunctionExpressions can recursively call themselves,
        // but they won't register with the current scope chain and should
        // be contained as single property in an anonymous object.
        functionScopeObject = new JSObject;
        exec->pushScope(functionScopeObject);
    }

    FunctionImp* func = new FunctionImp(exec, m_ident, m_body.get(), exec->scopeChain().node());
    JSObject* proto = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());
    proto->putDirect(exec->propertyNames().constructor, func, DontEnum);
    func->putDirect(exec->propertyNames().prototype, proto, DontDelete);

    if (named) {
        functionScopeObject->putDirect(m_ident, func, ReadOnly | (exec->codeType() == EvalCode ? 0 : DontDelete));
        exec->popScope();
    }

    return func;
}

} // namespace KJS
