/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#define KJS_BREAKPOINT \
  if (Debugger::debuggersPresent > 0 && !hitStatement(exec)) \
    return Completion(Normal);

#define KJS_CHECKEXCEPTION \
  if (exec->hadException()) \
    return rethrowException(exec);

#define KJS_CHECKEXCEPTIONVALUE \
  if (exec->hadException()) { \
    handleException(exec); \
    return jsUndefined(); \
  }

#define KJS_CHECKEXCEPTIONNUMBER \
  if (exec->hadException()) { \
    handleException(exec); \
    return 0; \
  }

#define KJS_CHECKEXCEPTIONBOOLEAN \
  if (exec->hadException()) { \
    handleException(exec); \
    return false; \
  }

#define KJS_CHECKEXCEPTIONLIST \
  if (exec->hadException()) { \
    handleException(exec); \
    return; \
  }

#define KJS_CHECKEXCEPTIONVOID \
  if (exec->hadException()) { \
    handleException(exec); \
    return; \
  }

#if !ASSERT_DISABLED
static inline bool canSkipLookup(ExecState* exec, const Identifier& ident)
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

double ExpressionNode::evaluateToNumber(ExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return value->toNumber(exec);
}

bool ExpressionNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return value->toBoolean(exec);
}

int32_t ExpressionNode::evaluateToInt32(ExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return value->toInt32(exec);
}

uint32_t ExpressionNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* value = evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return value->toUInt32(exec);
}

static void substitute(UString &string, const UString &substring) KJS_FAST_CALL;
static void substitute(UString &string, const UString &substring)
{
    int position = string.find("%s");
    ASSERT(position != -1);
    UString newString = string.substr(0, position);
    newString.append(substring);
    newString.append(string.substr(position + 2));
    string = newString;
}

static inline int currentSourceId(ExecState* exec) KJS_FAST_CALL;
static inline int currentSourceId(ExecState* exec)
{
    return exec->scopeNode()->sourceId();
}

static inline const UString& currentSourceURL(ExecState* exec) KJS_FAST_CALL;
static inline const UString& currentSourceURL(ExecState* exec)
{
    return exec->scopeNode()->sourceURL();
}

Completion Node::createErrorCompletion(ExecState* exec, ErrorType e, const char *msg)
{
    return Completion(Throw, Error::create(exec, e, msg, lineNo(), currentSourceId(exec), currentSourceURL(exec)));
}

Completion Node::createErrorCompletion(ExecState *exec, ErrorType e, const char *msg, const Identifier &ident)
{
    UString message = msg;
    substitute(message, ident.ustring());
    return Completion(Throw, Error::create(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec)));
}

JSValue *Node::throwError(ExecState* exec, ErrorType e, const char *msg)
{
    return KJS::throwError(exec, e, msg, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState* exec, ErrorType e, const char* msg, const char* string)
{
    UString message = msg;
    substitute(message, string);
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *expr)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}


JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, const Identifier &label)
{
    UString message = msg;
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *e1, Node *e2)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, e1->toString());
    substitute(message, e2->toString());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *expr, const Identifier &label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, const Identifier &label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), currentSourceURL(exec));
}

JSValue *Node::throwUndefinedVariableError(ExecState *exec, const Identifier &ident)
{
    return throwError(exec, ReferenceError, "Can't find variable: %s", ident);
}

void Node::handleException(ExecState* exec)
{
    handleException(exec, exec->exception());
}

void Node::handleException(ExecState* exec, JSValue* exceptionValue)
{
    if (exceptionValue->isObject()) {
        JSObject* exception = static_cast<JSObject*>(exceptionValue);
        if (!exception->hasProperty(exec, "line") && !exception->hasProperty(exec, "sourceURL")) {
            exception->put(exec, "line", jsNumber(m_line));
            exception->put(exec, "sourceURL", jsString(currentSourceURL(exec)));
        }
    }
    Debugger* dbg = exec->dynamicGlobalObject()->debugger();
    if (dbg && !dbg->hasHandledException(exec, exceptionValue)) {
        bool cont = dbg->exception(exec, currentSourceId(exec), m_line, exceptionValue);
        if (!cont)
            dbg->imp()->abort();
    }
}

Completion Node::rethrowException(ExecState* exec)
{
    JSValue* exception = exec->exception();
    exec->clearException();
    handleException(exec, exception);
    return Completion(Throw, exception);
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

// return true if the debugger wants us to stop at this point
bool StatementNode::hitStatement(ExecState* exec)
{
  Debugger *dbg = exec->dynamicGlobalObject()->debugger();
  if (dbg)
    return dbg->atStatement(exec, currentSourceId(exec), firstLine(), lastLine());
  else
    return true; // continue
}

// ------------------------------ NullNode -------------------------------------

JSValue *NullNode::evaluate(ExecState *)
{
  return jsNull();
}

// ------------------------------ FalseNode ----------------------------------

JSValue* FalseNode::evaluate(ExecState*)
{
    return jsBoolean(false);
}

// ------------------------------ TrueNode ----------------------------------

JSValue* TrueNode::evaluate(ExecState*)
{
    return jsBoolean(true);
}

// ------------------------------ NumberNode -----------------------------------

JSValue* NumberNode::evaluate(ExecState*)
{
    // Number nodes are only created when the number can't fit in a JSImmediate, so no need to check again.
    return jsNumberCell(m_double);
}
    
double NumberNode::evaluateToNumber(ExecState*)
{
    return m_double;
}

bool NumberNode::evaluateToBoolean(ExecState*)
{
    return m_double < 0.0 || m_double > 0.0; // false for NaN as well as 0
}

int32_t NumberNode::evaluateToInt32(ExecState*)
{
    return JSValue::toInt32(m_double);
}

uint32_t NumberNode::evaluateToUInt32(ExecState*)
{
    return JSValue::toUInt32(m_double);
}

// ------------------------------ ImmediateNumberNode -----------------------------------

JSValue* ImmediateNumberNode::evaluate(ExecState*)
{
    return m_value;
}

int32_t ImmediateNumberNode::evaluateToInt32(ExecState*)
{
    return JSImmediate::getTruncatedInt32(m_value);
}

uint32_t ImmediateNumberNode::evaluateToUInt32(ExecState*)
{
    uint32_t i;
    if (JSImmediate::getTruncatedUInt32(m_value, i))
        return i;
    bool ok;
    return JSValue::toUInt32SlowCase(m_double, ok);
}

// ------------------------------ StringNode -----------------------------------

JSValue* StringNode::evaluate(ExecState*)
{
  return jsOwnedString(m_value);
}

double StringNode::evaluateToNumber(ExecState*)
{
    return m_value.toDouble();
}

bool StringNode::evaluateToBoolean(ExecState*)
{
    return !m_value.isEmpty();
}
    
// ------------------------------ RegExpNode -----------------------------------

JSValue* RegExpNode::evaluate(ExecState* exec)
{
    return exec->lexicalGlobalObject()->regExpConstructor()->createRegExpImp(exec, m_regExp);
}

// ------------------------------ ThisNode -------------------------------------

// ECMA 11.1.1
JSValue *ThisNode::evaluate(ExecState *exec)
{
  return exec->thisValue();
}

// ------------------------------ ResolveNode ----------------------------------

// ECMA 11.1.2 & 10.1.4
JSValue* ResolveNode::inlineEvaluate(ExecState* exec)
{
  // Check for missed optimization opportunity.
  ASSERT(!canSkipLookup(exec, ident));

  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  do { 
    JSObject *o = *iter;

    if (o->getPropertySlot(exec, ident, slot))
      return slot.getValue(exec, o, ident);
    
    ++iter;
  } while (iter != end);

  return throwUndefinedVariableError(exec, ident);
}

JSValue* ResolveNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double ResolveNode::evaluateToNumber(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool ResolveNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t ResolveNode::evaluateToInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t ResolveNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

void ResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack&)
{
    size_t index = symbolTable.get(ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) LocalVarAccessNode(index);
}

JSValue* LocalVarAccessNode::inlineEvaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    return exec->localStorage()[index].value;
}

JSValue* LocalVarAccessNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double LocalVarAccessNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluate(exec)->toNumber(exec);
}

bool LocalVarAccessNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluate(exec)->toBoolean(exec);
}

int32_t LocalVarAccessNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluate(exec)->toInt32(exec);
}

uint32_t LocalVarAccessNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluate(exec)->toUInt32(exec);
}

// ------------------------------ ElementNode ----------------------------------

void ElementNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (next)
        nodeStack.append(next.get());
    ASSERT(node);
    nodeStack.append(node.get());
}

// ECMA 11.1.4
JSValue *ElementNode::evaluate(ExecState *exec)
{
  JSObject *array = exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, List::empty());
  int length = 0;
  for (ElementNode *n = this; n; n = n->next.get()) {
    JSValue *val = n->node->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    length += n->elision;
    array->put(exec, length++, val);
  }
  return array;
}

// ------------------------------ ArrayNode ------------------------------------

void ArrayNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (element)
        nodeStack.append(element.get());
}


// ECMA 11.1.4
JSValue *ArrayNode::evaluate(ExecState *exec)
{
  JSObject *array;
  int length;

  if (element) {
    array = static_cast<JSObject*>(element->evaluate(exec));
    KJS_CHECKEXCEPTIONVALUE
    length = opt ? array->get(exec, exec->propertyNames().length)->toInt32(exec) : 0;
  } else {
    JSValue *newArr = exec->lexicalGlobalObject()->arrayConstructor()->construct(exec,List::empty());
    array = static_cast<JSObject*>(newArr);
    length = 0;
  }

  if (opt)
    array->put(exec, exec->propertyNames().length, jsNumber(elision + length), DontEnum | DontDelete);

  return array;
}

// ------------------------------ ObjectLiteralNode ----------------------------

void ObjectLiteralNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (list)
        nodeStack.append(list.get());
}

// ECMA 11.1.5
JSValue *ObjectLiteralNode::evaluate(ExecState *exec)
{
  if (list)
    return list->evaluate(exec);

  return exec->lexicalGlobalObject()->objectConstructor()->construct(exec,List::empty());
}

// ------------------------------ PropertyListNode -----------------------------

void PropertyListNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (next)
        nodeStack.append(next.get());
    nodeStack.append(node.get());
}

// ECMA 11.1.5
JSValue *PropertyListNode::evaluate(ExecState *exec)
{
  JSObject *obj = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, List::empty());
  
  for (PropertyListNode *p = this; p; p = p->next.get()) {
    JSValue *v = p->node->assign->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    switch (p->node->type) {
      case PropertyNode::Getter:
        ASSERT(v->isObject());
        obj->defineGetter(exec, p->node->name(), static_cast<JSObject *>(v));
        break;
      case PropertyNode::Setter:
        ASSERT(v->isObject());
        obj->defineSetter(exec, p->node->name(), static_cast<JSObject *>(v));
        break;
      case PropertyNode::Constant:
        obj->put(exec, p->node->name(), v);
        break;
    }
  }

  return obj;
}

// ------------------------------ PropertyNode -----------------------------

void PropertyNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(assign.get());
}

// ECMA 11.1.5
JSValue *PropertyNode::evaluate(ExecState*)
{
  ASSERT(false);
  return jsNull();
}

// ------------------------------ BracketAccessorNode --------------------------------

void BracketAccessorNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.2.1a
JSValue* BracketAccessorNode::inlineEvaluate(ExecState* exec)
{
  JSValue* v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue* v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject* o = v1->toObject(exec);
  uint32_t i;
  if (v2->getUInt32(i))
    return o->get(exec, i);
  return o->get(exec, Identifier(v2->toString(exec)));
}

JSValue* BracketAccessorNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double BracketAccessorNode::evaluateToNumber(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool BracketAccessorNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t BracketAccessorNode::evaluateToInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t BracketAccessorNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}    

// ------------------------------ DotAccessorNode --------------------------------

void DotAccessorNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr.get());
}

// ECMA 11.2.1b
JSValue* DotAccessorNode::inlineEvaluate(ExecState* exec)
{
    JSValue* v = expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return v->toObject(exec)->get(exec, ident);
}

JSValue* DotAccessorNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double DotAccessorNode::evaluateToNumber(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool DotAccessorNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t DotAccessorNode::evaluateToInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t DotAccessorNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}    

// ------------------------------ ArgumentListNode -----------------------------

void ArgumentListNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (next)
        nodeStack.append(next.get());
    ASSERT(expr);
    nodeStack.append(expr.get());
}

// ECMA 11.2.4
void ArgumentListNode::evaluateList(ExecState* exec, List& list)
{
  for (ArgumentListNode *n = this; n; n = n->next.get()) {
    JSValue *v = n->expr->evaluate(exec);
    KJS_CHECKEXCEPTIONLIST
    list.append(v);
  }
}

// ------------------------------ ArgumentsNode --------------------------------

void ArgumentsNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (listNode)
        nodeStack.append(listNode.get());
}

// ------------------------------ NewExprNode ----------------------------------

void NewExprNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (args)
        nodeStack.append(args.get());
    nodeStack.append(expr.get());
}

// ECMA 11.2.2

JSValue* NewExprNode::inlineEvaluate(ExecState* exec)
{
  JSValue* v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  List argList;
  if (args) {
    args->evaluateList(exec, argList);
    KJS_CHECKEXCEPTIONVALUE
  }

  if (!v->isObject())
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v, expr.get());

  JSObject *constr = static_cast<JSObject*>(v);
  if (!constr->implementsConstruct())
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v, expr.get());

  return constr->construct(exec, argList);
}

JSValue* NewExprNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double NewExprNode::evaluateToNumber(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool NewExprNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t NewExprNode::evaluateToInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t NewExprNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}    

void FunctionCallValueNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(args.get());
    nodeStack.append(expr.get());
}

// ECMA 11.2.3
JSValue *FunctionCallValueNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  if (!v->isObject()) {
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, expr.get());
  }
  
  JSObject *func = static_cast<JSObject*>(v);

  if (!func->implementsCall()) {
    return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, expr.get());
  }

  List argList;
  args->evaluateList(exec, argList);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *thisObj =  exec->dynamicGlobalObject();

  return func->call(exec, thisObj, argList);
}

void FunctionCallResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(args.get());

    size_t index = symbolTable.get(ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) LocalVarFunctionCallNode(index);
}

// ECMA 11.2.3
JSValue* FunctionCallResolveNode::inlineEvaluate(ExecState* exec)
{
  // Check for missed optimization opportunity.
  ASSERT(!canSkipLookup(exec, ident));

  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, ident, slot)) {
      JSValue *v = slot.getValue(exec, base, ident);
      KJS_CHECKEXCEPTIONVALUE
        
      if (!v->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident);
      
      JSObject *func = static_cast<JSObject*>(v);
      
      if (!func->implementsCall())
        return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident);
      
      List argList;
      args->evaluateList(exec, argList);
      KJS_CHECKEXCEPTIONVALUE
        
      JSObject *thisObj = base;
      // ECMA 11.2.3 says that in this situation the this value should be null.
      // However, section 10.2.3 says that in the case where the value provided
      // by the caller is null, the global object should be used. It also says
      // that the section does not apply to interal functions, but for simplicity
      // of implementation we use the global object anyway here. This guarantees
      // that in host objects you always get a valid object for this.
      if (thisObj->isActivationObject())
        thisObj = exec->dynamicGlobalObject();

      return func->call(exec, thisObj, argList);
    }
    ++iter;
  } while (iter != end);
  
  return throwUndefinedVariableError(exec, ident);
}

JSValue* FunctionCallResolveNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double FunctionCallResolveNode::evaluateToNumber(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool FunctionCallResolveNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t FunctionCallResolveNode::evaluateToInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t FunctionCallResolveNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

JSValue* LocalVarFunctionCallNode::inlineEvaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    JSValue* v = exec->localStorage()[index].value;

    if (!v->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident);
      
    JSObject* func = static_cast<JSObject*>(v);
    if (!func->implementsCall())
        return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident);
      
    List argList;
    args->evaluateList(exec, argList);
    KJS_CHECKEXCEPTIONVALUE

    return func->call(exec, exec->dynamicGlobalObject(), argList);
}

JSValue* LocalVarFunctionCallNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double LocalVarFunctionCallNode::evaluateToNumber(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool LocalVarFunctionCallNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t LocalVarFunctionCallNode::evaluateToInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t LocalVarFunctionCallNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

void FunctionCallBracketNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(args.get());
    nodeStack.append(subscript.get());
    nodeStack.append(base.get());
}

// ECMA 11.2.3
JSValue *FunctionCallBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseVal = base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSValue *subscriptVal = subscript->evaluate(exec);

  JSObject *baseObj = baseVal->toObject(exec);
  uint32_t i;
  PropertySlot slot;

  JSValue *funcVal;
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
  
  if (!funcVal->isObject()) {
    return throwError(exec, TypeError, "Value %s (result of expression %s[%s]) is not object.", funcVal, base.get(), subscript.get());
  }
  
  JSObject *func = static_cast<JSObject*>(funcVal);

  if (!func->implementsCall()) {
    return throwError(exec, TypeError, "Object %s (result of expression %s[%s]) does not allow calls.", funcVal, base.get(), subscript.get());
  }

  List argList;
  args->evaluateList(exec, argList);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *thisObj = baseObj;
  ASSERT(thisObj);
  ASSERT(thisObj->isObject());
  ASSERT(!thisObj->isActivationObject());

  return func->call(exec, thisObj, argList);
}

static const char *dotExprNotAnObjectString() KJS_FAST_CALL;
static const char *dotExprNotAnObjectString()
{
  return "Value %s (result of expression %s.%s) is not object.";
}

static const char *dotExprDoesNotAllowCallsString() KJS_FAST_CALL;
static const char *dotExprDoesNotAllowCallsString()
{
  return "Object %s (result of expression %s.%s) does not allow calls.";
}

void FunctionCallDotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(args.get());
    nodeStack.append(base.get());
}

// ECMA 11.2.3
JSValue* FunctionCallDotNode::inlineEvaluate(ExecState* exec)
{
  JSValue *baseVal = base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *baseObj = baseVal->toObject(exec);
  PropertySlot slot;
  JSValue *funcVal = baseObj->getPropertySlot(exec, ident, slot) ? slot.getValue(exec, baseObj, ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  if (!funcVal->isObject())
    return throwError(exec, TypeError, dotExprNotAnObjectString(), funcVal, base.get(), ident);
  
  JSObject *func = static_cast<JSObject*>(funcVal);

  if (!func->implementsCall())
    return throwError(exec, TypeError, dotExprDoesNotAllowCallsString(), funcVal, base.get(), ident);

  List argList;
  args->evaluateList(exec, argList);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *thisObj = baseObj;
  ASSERT(thisObj);
  ASSERT(thisObj->isObject());
  ASSERT(!thisObj->isActivationObject());

  return func->call(exec, thisObj, argList);
}

JSValue* FunctionCallDotNode::evaluate(ExecState* exec)
{
    return inlineEvaluate(exec);
}

double FunctionCallDotNode::evaluateToNumber(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toNumber(exec);
}

bool FunctionCallDotNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return v->toBoolean(exec);
}

int32_t FunctionCallDotNode::evaluateToInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toInt32(exec);
}

uint32_t FunctionCallDotNode::evaluateToUInt32(ExecState* exec)
{
    JSValue* v = inlineEvaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return v->toUInt32(exec);
}

// ECMA 11.3

// ------------------------------ PostfixResolveNode ----------------------------------

// Increment
void PostIncResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack&)
{
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) PostIncLocalVarNode(index);
}

JSValue *PostIncResolveNode::evaluate(ExecState *exec)
{
  // Check for missed optimization opportunity.
  ASSERT(!canSkipLookup(exec, m_ident));

  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
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
    
JSValue* PostIncLocalVarNode::evaluate(ExecState* exec)
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
void PostDecResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack&)
{
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) PostDecLocalVarNode(index);
}

JSValue *PostDecResolveNode::evaluate(ExecState *exec)
{
  // Check for missed optimization opportunity.
  ASSERT(!canSkipLookup(exec, m_ident));

  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
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
    
JSValue* PostDecLocalVarNode::evaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    JSValue** slot = &exec->localStorage()[m_index].value;
    JSValue* v = (*slot)->toJSNumber(exec);
    *slot = jsNumber(v->toNumber(exec) - 1);
    return v;
}

double PostDecLocalVarNode::inlineEvaluateToNumber(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    
    JSValue** slot = &exec->localStorage()[m_index].value;
    double n = (*slot)->toNumber(exec);
    *slot = jsNumber(n - 1);
    return n;
}

double PostDecLocalVarNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

bool PostDecLocalVarNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t PostDecLocalVarNode::evaluateToInt32(ExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t PostDecLocalVarNode::evaluateToUInt32(ExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

void PostDecLocalVarNode::optimizeForUnnecessaryResult()
{
    new (this) PreDecLocalVarNode(m_index);
}
    
// ------------------------------ PostfixBracketNode ----------------------------------

void PostfixBracketNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue *PostIncBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex)) {
    PropertySlot slot;
    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = v->toJSNumber(exec);
    base->put(exec, propertyIndex, jsNumber(v2->toNumber(exec) + 1));
        
    return v2;
  }

  Identifier propertyName(subscript->toString(exec));
  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  JSValue* v2 = v->toJSNumber(exec);
  base->put(exec, propertyName, jsNumber(v2->toNumber(exec) + 1));
  return v2;
}

JSValue *PostDecBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex)) {
    PropertySlot slot;
    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue* v2 = v->toJSNumber(exec);
    base->put(exec, propertyIndex, jsNumber(v2->toNumber(exec) - 1));
    return v2;
  }

  Identifier propertyName(subscript->toString(exec));
  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  JSValue* v2 = v->toJSNumber(exec);
  base->put(exec, propertyName, jsNumber(v2->toNumber(exec) - 1));
  return v2;
}

// ------------------------------ PostfixDotNode ----------------------------------

void PostfixDotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_base.get());
}

JSValue *PostIncDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  JSValue* v2 = v->toJSNumber(exec);
  base->put(exec, m_ident, jsNumber(v2->toNumber(exec) + 1));
  return v2;
}

JSValue *PostDecDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  JSValue* v2 = v->toJSNumber(exec);
  base->put(exec, m_ident, jsNumber(v2->toNumber(exec) - 1));
  return v2;
}

// ------------------------------ PostfixErrorNode -----------------------------------

JSValue* PostfixErrorNode::evaluate(ExecState* exec)
{
    throwError(exec, ReferenceError, "Postfix %s operator applied to value that is not a reference.",
        m_oper == OpPlusPlus ? "++" : "--");
    handleException(exec);
    return jsUndefined();
}

// ------------------------------ DeleteResolveNode -----------------------------------

void DeleteResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack&)
{
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) LocalVarDeleteNode();
}

// ECMA 11.4.1

JSValue *DeleteResolveNode::evaluate(ExecState *exec)
{
  // Check for missed optimization opportunity.
  ASSERT(!canSkipLookup(exec, m_ident));

  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
        return jsBoolean(base->deleteProperty(exec, m_ident));
    }

    ++iter;
  } while (iter != end);

  return jsBoolean(true);
}

JSValue* LocalVarDeleteNode::evaluate(ExecState*)
{
    return jsBoolean(false);
}

// ------------------------------ DeleteBracketNode -----------------------------------

void DeleteBracketNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue *DeleteBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex))
      return jsBoolean(base->deleteProperty(exec, propertyIndex));

  Identifier propertyName(subscript->toString(exec));
  return jsBoolean(base->deleteProperty(exec, propertyName));
}

// ------------------------------ DeleteDotNode -----------------------------------

void DeleteDotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_base.get());
}

JSValue *DeleteDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  JSObject *base = baseValue->toObject(exec);
  KJS_CHECKEXCEPTIONVALUE

  return jsBoolean(base->deleteProperty(exec, m_ident));
}

// ------------------------------ DeleteValueNode -----------------------------------

void DeleteValueNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

JSValue *DeleteValueNode::evaluate(ExecState *exec)
{
  m_expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  // delete on a non-location expression ignores the value and returns true
  return jsBoolean(true);
}

// ------------------------------ VoidNode -------------------------------------

void VoidNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr.get());
}

// ECMA 11.4.2
JSValue *VoidNode::evaluate(ExecState *exec)
{
  expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return jsUndefined();
}

// ECMA 11.4.3

// ------------------------------ TypeOfValueNode -----------------------------------

void TypeOfValueNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

static JSValue *typeStringForValue(JSValue *v) KJS_FAST_CALL;
static JSValue *typeStringForValue(JSValue *v)
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

void TypeOfResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack&)
{
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) LocalVarTypeOfNode(index);
}

JSValue* LocalVarTypeOfNode::evaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());

    return typeStringForValue(exec->localStorage()[m_index].value);
}

JSValue *TypeOfResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
        JSValue *v = slot.getValue(exec, base, m_ident);
        return typeStringForValue(v);
    }

    ++iter;
  } while (iter != end);

  return jsString("undefined");
}

// ------------------------------ TypeOfValueNode -----------------------------------

JSValue *TypeOfValueNode::evaluate(ExecState *exec)
{
  JSValue *v = m_expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return typeStringForValue(v);
}

// ECMA 11.4.4 and 11.4.5

// ------------------------------ PrefixResolveNode ----------------------------------

void PreIncResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack&)
{
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) PreIncLocalVarNode(index);
}

JSValue* PreIncLocalVarNode::evaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue** slot = &exec->localStorage()[m_index].value;

    double n = (*slot)->toNumber(exec);
    JSValue* n2 = jsNumber(n + 1);
    *slot = n2;
    return n2;
}

JSValue *PreIncResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
        JSValue *v = slot.getValue(exec, base, m_ident);

        double n = v->toNumber(exec);
        JSValue *n2 = jsNumber(n + 1);
        base->put(exec, m_ident, n2);

        return n2;
    }

    ++iter;
  } while (iter != end);

  return throwUndefinedVariableError(exec, m_ident);
}

void PreDecResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack&)
{
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) PreDecLocalVarNode(index);
}

JSValue* PreDecLocalVarNode::evaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue** slot = &exec->localStorage()[m_index].value;

    double n = (*slot)->toNumber(exec);
    JSValue* n2 = jsNumber(n - 1);
    *slot = n2;
    return n2;
}

JSValue *PreDecResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
        JSValue *v = slot.getValue(exec, base, m_ident);

        double n = v->toNumber(exec);
        JSValue *n2 = jsNumber(n - 1);
        base->put(exec, m_ident, n2);

        return n2;
    }

    ++iter;
  } while (iter != end);

  return throwUndefinedVariableError(exec, m_ident);
}

// ------------------------------ PrefixBracketNode ----------------------------------

void PrefixBracketNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue *PreIncBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex)) {
    PropertySlot slot;
    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue *n2 = jsNumber(v->toNumber(exec) + 1);
    base->put(exec, propertyIndex, n2);

    return n2;
  }

  Identifier propertyName(subscript->toString(exec));
  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  JSValue *n2 = jsNumber(v->toNumber(exec) + 1);
  base->put(exec, propertyName, n2);

  return n2;
}

JSValue *PreDecBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex)) {
    PropertySlot slot;
    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE

    JSValue *n2 = jsNumber(v->toNumber(exec) - 1);
    base->put(exec, propertyIndex, n2);

    return n2;
  }

  Identifier propertyName(subscript->toString(exec));
  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  JSValue *n2 = jsNumber(v->toNumber(exec) - 1);
  base->put(exec, propertyName, n2);

  return n2;
}

// ------------------------------ PrefixDotNode ----------------------------------

void PrefixDotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_base.get());
}

JSValue *PreIncDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  double n = v->toNumber(exec);
  JSValue *n2 = jsNumber(n + 1);
  base->put(exec, m_ident, n2);

  return n2;
}

JSValue *PreDecDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  double n = v->toNumber(exec);
  JSValue *n2 = jsNumber(n - 1);
  base->put(exec, m_ident, n2);

  return n2;
}

// ------------------------------ PrefixErrorNode -----------------------------------

JSValue* PrefixErrorNode::evaluate(ExecState* exec)
{
    throwError(exec, ReferenceError, "Prefix %s operator applied to value that is not a reference.",
        m_oper == OpPlusPlus ? "++" : "--");
    handleException(exec);
    return jsUndefined();
}

// ------------------------------ UnaryPlusNode --------------------------------

void UnaryPlusNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_expr.get());
}

// ECMA 11.4.6
JSValue* UnaryPlusNode::evaluate(ExecState* exec)
{
    JSValue *v = m_expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return v->toJSNumber(exec);
}

bool UnaryPlusNode::evaluateToBoolean(ExecState* exec)
{
    return m_expr->evaluateToBoolean(exec);
}

double UnaryPlusNode::evaluateToNumber(ExecState* exec)
{
    return m_expr->evaluateToNumber(exec);
}

int32_t UnaryPlusNode::evaluateToInt32(ExecState* exec)
{
    return m_expr->evaluateToInt32(exec);
}

uint32_t UnaryPlusNode::evaluateToUInt32(ExecState* exec)
{
    return m_expr->evaluateToInt32(exec);
}

// ------------------------------ NegateNode -----------------------------------

void NegateNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr.get());
}

// ECMA 11.4.7
JSValue *NegateNode::evaluate(ExecState *exec)
{
    // No need to check exception, caller will do so right after evaluate()
    return jsNumber(-expr->evaluateToNumber(exec));
}

double NegateNode::evaluateToNumber(ExecState* exec)
{
    // No need to check exception, caller will do so right after evaluateToNumber()
    return -expr->evaluateToNumber(exec);
}

// ------------------------------ BitwiseNotNode -------------------------------

void BitwiseNotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr.get());
}

// ECMA 11.4.8
int32_t BitwiseNotNode::inlineEvaluateToInt32(ExecState* exec)
{
    return ~expr->evaluateToInt32(exec);
}

JSValue* BitwiseNotNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double BitwiseNotNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitwiseNotNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitwiseNotNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitwiseNotNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

// ------------------------------ LogicalNotNode -------------------------------

void LogicalNotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr.get());
}

// ECMA 11.4.9
JSValue* LogicalNotNode::evaluate(ExecState* exec)
{
    return jsBoolean(!expr->evaluateToBoolean(exec));
}

bool LogicalNotNode::evaluateToBoolean(ExecState* exec)
{
    return !expr->evaluateToBoolean(exec);
}

// ------------------------------ Multiplicative Nodes -----------------------------------

void MultNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.5.1
double MultNode::inlineEvaluateToNumber(ExecState* exec)
{
    double n1 = term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = term2->evaluateToNumber(exec);
    return n1 * n2;
}

JSValue *MultNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}
    
double MultNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

bool MultNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t MultNode::evaluateToInt32(ExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t MultNode::evaluateToUInt32(ExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

void DivNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.5.2
double DivNode::inlineEvaluateToNumber(ExecState* exec)
{
    double n1 = term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = term2->evaluateToNumber(exec);
    return n1 / n2;
}

JSValue* DivNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double DivNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t DivNode::evaluateToInt32(ExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t DivNode::evaluateToUInt32(ExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

void ModNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.5.3
double ModNode::inlineEvaluateToNumber(ExecState* exec)
{
    double n1 = term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = term2->evaluateToNumber(exec);
    return fmod(n1, n2);
}

JSValue* ModNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double ModNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

bool ModNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t ModNode::evaluateToInt32(ExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t ModNode::evaluateToUInt32(ExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

// ------------------------------ Additive Nodes --------------------------------------

static JSValue* throwOutOfMemoryError(ExecState* exec)
{
    JSObject* error = Error::create(exec, GeneralError, "Out of memory");
    exec->setException(error);
    return error;
}

static double throwOutOfMemoryErrorToNumber(ExecState* exec)
{
    JSObject* error = Error::create(exec, GeneralError, "Out of memory");
    exec->setException(error);
    return 0.0;
}    

// ECMA 11.6
static JSValue* addSlowCase(ExecState* exec, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue *p1 = v1->toPrimitive(exec, UnspecifiedType);
    JSValue *p2 = v2->toPrimitive(exec, UnspecifiedType);
    
    if (p1->isString() || p2->isString()) {
        UString value = p1->toString(exec) + p2->toString(exec);
        if (value.isNull())
            return throwOutOfMemoryError(exec);
        return jsString(value);
    }
    
    return jsNumber(p1->toNumber(exec) + p2->toNumber(exec));
}

static double addSlowCaseToNumber(ExecState* exec, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue *p1 = v1->toPrimitive(exec, UnspecifiedType);
    JSValue *p2 = v2->toPrimitive(exec, UnspecifiedType);
    
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

static inline JSValue* add(ExecState* exec, JSValue* v1, JSValue *v2)
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

static inline double addToNumber(ExecState* exec, JSValue* v1, JSValue *v2)
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

void AddNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.6.1
JSValue* AddNode::evaluate(ExecState* exec)
{
  JSValue* v1 = term1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSValue* v2 = term2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return add(exec, v1, v2);
}

double AddNode::inlineEvaluateToNumber(ExecState* exec)
{
    JSValue* v1 = term1->evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    
    JSValue* v2 = term2->evaluate(exec);
    KJS_CHECKEXCEPTIONNUMBER
    
    return addToNumber(exec, v1, v2);
}

double AddNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t AddNode::evaluateToInt32(ExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t AddNode::evaluateToUInt32(ExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}    

double AddNumbersNode::inlineEvaluateToNumber(ExecState* exec)
{
    double n1 = term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = term2->evaluateToNumber(exec);
    return n1 + n2;
}

JSValue* AddNumbersNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double AddNumbersNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t AddNumbersNode::evaluateToInt32(ExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t AddNumbersNode::evaluateToUInt32(ExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}

JSValue* AddStringsNode::evaluate(ExecState* exec)
{
    JSValue* v1 = term1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    JSValue* v2 = term2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    return jsString(static_cast<StringImp*>(v1)->value() + static_cast<StringImp*>(v2)->value());
}

JSValue* AddStringLeftNode::evaluate(ExecState* exec)
{
    JSValue* v1 = term1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    JSValue* v2 = term2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    JSValue* p2 = v2->toPrimitive(exec, UnspecifiedType);
    return jsString(static_cast<StringImp*>(v1)->value() + p2->toString(exec));
}

JSValue* AddStringRightNode::evaluate(ExecState* exec)
{
    JSValue* v1 = term1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    JSValue* v2 = term2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    JSValue* p1 = v1->toPrimitive(exec, UnspecifiedType);
    return jsString(p1->toString(exec) + static_cast<StringImp*>(v2)->value());
}

void SubNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.6.2
double SubNode::inlineEvaluateToNumber(ExecState* exec)
{
    double n1 = term1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONNUMBER
    double n2 = term2->evaluateToNumber(exec);
    return n1 - n2;
}

JSValue* SubNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToNumber(exec));
}

double SubNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToNumber(exec);
}

int32_t SubNode::evaluateToInt32(ExecState* exec)
{
    return JSValue::toInt32(inlineEvaluateToNumber(exec));
}

uint32_t SubNode::evaluateToUInt32(ExecState* exec)
{
    return JSValue::toUInt32(inlineEvaluateToNumber(exec));
}    

// ------------------------------ Shift Nodes ------------------------------------

void LeftShiftNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.7.1
int32_t LeftShiftNode::inlineEvaluateToInt32(ExecState* exec)
{
    int i1 = term1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    unsigned int i2 = term2->evaluateToUInt32(exec) & 0x1f;
    return (i1 << i2);
}

JSValue* LeftShiftNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double LeftShiftNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t LeftShiftNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t LeftShiftNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

void RightShiftNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.7.2
int32_t RightShiftNode::inlineEvaluateToInt32(ExecState* exec)
{
    int i1 = term1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    unsigned int i2 = term2->evaluateToUInt32(exec) & 0x1f;
    return (i1 >> i2);
}

JSValue* RightShiftNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double RightShiftNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t RightShiftNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t RightShiftNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

void UnsignedRightShiftNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(term1.get());
    nodeStack.append(term2.get());
}

// ECMA 11.7.3
uint32_t UnsignedRightShiftNode::inlineEvaluateToUInt32(ExecState* exec)
{
    unsigned int i1 = term1->evaluateToUInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    unsigned int i2 = term2->evaluateToUInt32(exec) & 0x1f;
    return (i1 >> i2);
}

JSValue* UnsignedRightShiftNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToUInt32(exec));
}

double UnsignedRightShiftNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToUInt32(exec);
}

int32_t UnsignedRightShiftNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluateToUInt32(exec);
}

uint32_t UnsignedRightShiftNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluateToUInt32(exec);
}

// ------------------------------ Relational Nodes -------------------------------

static inline bool lessThan(ExecState *exec, JSValue* v1, JSValue* v2) 
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

static inline bool lessThanEq(ExecState *exec, JSValue* v1, JSValue* v2) 
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

void LessNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.8.1
bool LessNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThan(exec, v1, v2);
}

JSValue* LessNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

bool LessNumbersNode::inlineEvaluateToBoolean(ExecState* exec)
{
    double n1 = expr1->evaluateToNumber(exec);
    KJS_CHECKEXCEPTIONVALUE
    double n2 = expr2->evaluateToNumber(exec);
    return n1 < n2;
}

JSValue* LessNumbersNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessNumbersNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

bool LessStringsNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = expr2->evaluate(exec);
    return static_cast<StringImp*>(v1)->value() < static_cast<StringImp*>(v2)->value();
}

JSValue* LessStringsNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessStringsNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

void GreaterNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.8.2
bool GreaterNode::inlineEvaluateToBoolean(ExecState *exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThan(exec, v2, v1);
}

JSValue* GreaterNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool GreaterNode::evaluateToBoolean(ExecState *exec)
{
    return inlineEvaluateToBoolean(exec);
}

void LessEqNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.8.3
bool LessEqNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThanEq(exec, v1, v2);
}

JSValue* LessEqNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool LessEqNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

void GreaterEqNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.8.4
bool GreaterEqNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return lessThanEq(exec, v2, v1);
}

JSValue* GreaterEqNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool GreaterEqNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

void InstanceOfNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.8.6
JSValue* InstanceOfNode::evaluate(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    if (!v2->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with instanceof operator.", v2, expr2.get());

    JSObject* o2 = static_cast<JSObject*>(v2);

    // According to the spec, only some types of objects "implement" the [[HasInstance]] property.
    // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
    // property. It seems that all objects have the property, but not all implement it, so in this
    // case we return false (consistent with Mozilla).
    if (!o2->implementsHasInstance())
        return jsBoolean(false);

    return jsBoolean(o2->hasInstance(exec, v1));
}

bool InstanceOfNode::evaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    if (!v2->isObject()) {
        throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with 'instanceof' operator.", v2, expr2.get());
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

void InNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.8.7
JSValue* InNode::evaluate(ExecState *exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    if (!v2->isObject())
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with 'in' operator.", v2, expr2.get());

    return jsBoolean(static_cast<JSObject*>(v2)->hasProperty(exec, Identifier(v1->toString(exec))));
}

bool InNode::evaluateToBoolean(ExecState *exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    if (!v2->isObject()) {
        throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with 'in' operator.", v2, expr2.get());
        return false;
    }

    return static_cast<JSObject*>(v2)->hasProperty(exec, Identifier(v1->toString(exec)));
}

// ------------------------------ Equality Nodes ------------------------------------

void EqualNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.9.1
bool EqualNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    
    return equal(exec, v1, v2);
}

JSValue* EqualNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool EqualNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

void NotEqualNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.9.2
bool NotEqualNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    return !equal(exec,v1, v2);
}

JSValue* NotEqualNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool NotEqualNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

void StrictEqualNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.9.4
bool StrictEqualNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    return strictEqual(exec,v1, v2);
}

JSValue* StrictEqualNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool StrictEqualNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

void NotStrictEqualNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.9.5
bool NotStrictEqualNode::inlineEvaluateToBoolean(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONBOOLEAN

    return !strictEqual(exec,v1, v2);
}

JSValue* NotStrictEqualNode::evaluate(ExecState* exec)
{
    return jsBoolean(inlineEvaluateToBoolean(exec));
}

bool NotStrictEqualNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToBoolean(exec);
}

// ------------------------------ Bit Operation Nodes ----------------------------------

void BitAndNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.10
JSValue* BitAndNode::evaluate(ExecState* exec)
{    
    JSValue *v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue *v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    return jsNumberFromAnd(exec, v1, v2);
}

int32_t BitAndNode::inlineEvaluateToInt32(ExecState* exec)
{
    int32_t i1 = expr1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    int32_t i2 = expr2->evaluateToInt32(exec);
    return (i1 & i2);
}

double BitAndNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitAndNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitAndNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitAndNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

void BitXOrNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

int32_t BitXOrNode::inlineEvaluateToInt32(ExecState* exec)
{
    int i1 = expr1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    int i2 = expr2->evaluateToInt32(exec);
    return (i1 ^ i2);
}

JSValue* BitXOrNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double BitXOrNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitXOrNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitXOrNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitXOrNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

void BitOrNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

int32_t BitOrNode::inlineEvaluateToInt32(ExecState* exec)
{
    int i1 = expr1->evaluateToInt32(exec);
    KJS_CHECKEXCEPTIONNUMBER
    int i2 = expr2->evaluateToInt32(exec);
    return (i1 | i2);
}

JSValue* BitOrNode::evaluate(ExecState* exec)
{
    return jsNumber(inlineEvaluateToInt32(exec));
}

double BitOrNode::evaluateToNumber(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

bool BitOrNode::evaluateToBoolean(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

int32_t BitOrNode::evaluateToInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

uint32_t BitOrNode::evaluateToUInt32(ExecState* exec)
{
    return inlineEvaluateToInt32(exec);
}

// ------------------------------ Binary Logical Nodes ----------------------------

void LogicalAndNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.11
JSValue* LogicalAndNode::evaluate(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    bool b1 = v1->toBoolean(exec);
    KJS_CHECKEXCEPTIONVALUE
    if (!b1)
        return v1;
    JSValue* v2 = expr2->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return v2;
}

bool LogicalAndNode::evaluateToBoolean(ExecState* exec)
{
    bool b = expr1->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return b && expr2->evaluateToBoolean(exec);
}

void LogicalOrNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

JSValue* LogicalOrNode::evaluate(ExecState* exec)
{
    JSValue* v1 = expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    if (v1->toBoolean(exec))
        return v1;
    return expr2->evaluate(exec);
}

bool LogicalOrNode::evaluateToBoolean(ExecState* exec)
{
    bool b = expr1->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return b || expr2->evaluateToBoolean(exec);
}

// ------------------------------ ConditionalNode ------------------------------

void ConditionalNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
    nodeStack.append(logical.get());
}

// ECMA 11.12
JSValue* ConditionalNode::evaluate(ExecState* exec)
{
    bool b = logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONVALUE
    return b ? expr1->evaluate(exec) : expr2->evaluate(exec);
}

bool ConditionalNode::evaluateToBoolean(ExecState* exec)
{
    bool b = logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONBOOLEAN
    return b ? expr1->evaluateToBoolean(exec) : expr2->evaluateToBoolean(exec);
}

double ConditionalNode::evaluateToNumber(ExecState* exec)
{
    bool b = logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return b ? expr1->evaluateToNumber(exec) : expr2->evaluateToNumber(exec);
}

int32_t ConditionalNode::evaluateToInt32(ExecState* exec)
{
    bool b = logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return b ? expr1->evaluateToInt32(exec) : expr2->evaluateToInt32(exec);
}

uint32_t ConditionalNode::evaluateToUInt32(ExecState* exec)
{
    bool b = logical->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTIONNUMBER
    return b ? expr1->evaluateToUInt32(exec) : expr2->evaluateToUInt32(exec);
}

// ECMA 11.13

static ALWAYS_INLINE JSValue* valueForReadModifyAssignment(ExecState* exec, JSValue* current, ExpressionNode* right, Operator oper) KJS_FAST_CALL;
static ALWAYS_INLINE JSValue* valueForReadModifyAssignment(ExecState* exec, JSValue* current, ExpressionNode* right, Operator oper)
{
  JSValue *v;
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

void ReadModifyResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) ReadModifyLocalVarNode(index);
}

void AssignResolveNode::optimizeVariableAccess(SymbolTable& symbolTable, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    size_t index = symbolTable.get(m_ident.ustring().rep());
    if (index != missingSymbolMarker())
        new (this) AssignLocalVarNode(index);
}

JSValue* ReadModifyLocalVarNode::evaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue** slot = &exec->localStorage()[m_index].value;

    ASSERT(m_oper != OpEqual);
    JSValue* v = valueForReadModifyAssignment(exec, *slot, m_right.get(), m_oper);

    KJS_CHECKEXCEPTIONVALUE

    *slot = v;
    return v;
}

JSValue* AssignLocalVarNode::evaluate(ExecState* exec)
{
    ASSERT(exec->variableObject() == exec->scopeChain().top());
    JSValue* v = m_right->evaluate(exec);

    KJS_CHECKEXCEPTIONVALUE

    exec->localStorage()[m_index].value = v;
    
    return v;
}

JSValue *ReadModifyResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot))
      goto found;

    ++iter;
  } while (iter != end);

  ASSERT(m_oper != OpEqual);
  return throwUndefinedVariableError(exec, m_ident);

 found:
  JSValue *v;

  
  ASSERT(m_oper != OpEqual);
  JSValue *v1 = slot.getValue(exec, base, m_ident);
  KJS_CHECKEXCEPTIONVALUE
  v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_oper);

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, m_ident, v);
  return v;
}

JSValue *AssignResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  ASSERT(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot))
      goto found;

    ++iter;
  } while (iter != end);

 found:
  JSValue *v = m_right->evaluate(exec);

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, m_ident, v);
  return v;
}

// ------------------------------ ReadModifyDotNode -----------------------------------

void AssignDotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_base.get());
}

JSValue *AssignDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  JSValue *v = m_right->evaluate(exec);

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, m_ident, v);
  return v;
}

void ReadModifyDotNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_base.get());
}

JSValue *ReadModifyDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  JSValue *v;

  ASSERT(m_oper != OpEqual);
  PropertySlot slot;
  JSValue *v1 = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE
  v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_oper);

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, m_ident, v);
  return v;
}

// ------------------------------ AssignErrorNode -----------------------------------

JSValue* AssignErrorNode::evaluate(ExecState* exec)
{
    throwError(exec, ReferenceError, "Left side of assignment is not a reference.");
    handleException(exec);
    return jsUndefined();
}

// ------------------------------ AssignBracketNode -----------------------------------

void AssignBracketNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue *AssignBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex)) {
    JSValue *v = m_right->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, propertyIndex, v);
    return v;
  }

  Identifier propertyName(subscript->toString(exec));
  JSValue *v = m_right->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, propertyName, v);
  return v;
}
void ReadModifyBracketNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(m_right.get());
    nodeStack.append(m_subscript.get());
    nodeStack.append(m_base.get());
}

JSValue *ReadModifyBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex)) {
    JSValue *v;
    ASSERT(m_oper != OpEqual);
    PropertySlot slot;
    JSValue *v1 = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE
    v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_oper);

    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, propertyIndex, v);
    return v;
  }

  Identifier propertyName(subscript->toString(exec));
  JSValue *v;

  ASSERT(m_oper != OpEqual);
  PropertySlot slot;
  JSValue *v1 = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE
  v = valueForReadModifyAssignment(exec, v1, m_right.get(), m_oper);

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, propertyName, v);
  return v;
}

// ------------------------------ CommaNode ------------------------------------

void CommaNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr2.get());
    nodeStack.append(expr1.get());
}

// ECMA 11.14
JSValue* CommaNode::evaluate(ExecState *exec)
{
    expr1->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    return expr2->evaluate(exec);
}

// ------------------------------ AssignExprNode -------------------------------

void AssignExprNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr.get());
}

// ECMA 12.2
JSValue* AssignExprNode::evaluate(ExecState* exec)
{
    return expr->evaluate(exec);
}

bool AssignExprNode::evaluateToBoolean(ExecState* exec)
{
    return expr->evaluateToBoolean(exec);
}

double AssignExprNode::evaluateToNumber(ExecState* exec)
{
    return expr->evaluateToNumber(exec);
}

int32_t AssignExprNode::evaluateToInt32(ExecState* exec)
{
    return expr->evaluateToInt32(exec);
}

uint32_t AssignExprNode::evaluateToUInt32(ExecState* exec)
{
    return expr->evaluateToInt32(exec);
}

// ------------------------------ VarDeclNode ----------------------------------

VarDeclNode::VarDeclNode(const Identifier &id, AssignExprNode *in, Type t)
    : varType(t)
    , ident(id)
    , init(in)
{
}

void VarDeclNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (next)
        nodeStack.append(next.get());
    if (init)
        nodeStack.append(init.get());
}

void VarDeclNode::handleSlowCase(ExecState* exec, const ScopeChain& chain, JSValue* val)
{
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();
    
    // we must always have something in the scope chain
    ASSERT(iter != end);
    
    PropertySlot slot;
    JSObject* base;
    
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot))
            break;
        
        ++iter;
    } while (iter != end);
    
    unsigned flags = 0;
    base->getPropertyAttributes(ident, flags);
    if (varType == VarDeclNode::Constant)
        flags |= ReadOnly;
    
    base->put(exec, ident, val, flags);
}

// ECMA 12.2
inline void VarDeclNode::evaluateSingle(ExecState* exec)
{
    ASSERT(exec->variableObject()->hasOwnProperty(exec, ident) || exec->codeType() == EvalCode); // Guaranteed by processDeclarations.
    const ScopeChain& chain = exec->scopeChain();
    JSObject* variableObject = exec->variableObject();

    ASSERT(!chain.isEmpty());

    bool inGlobalScope = ++chain.begin() == chain.end();

    if (init) {
        if (inGlobalScope) {
            JSValue* val = init->evaluate(exec);
            int flags = Internal;
            if (exec->codeType() != EvalCode)
                flags |= DontDelete;
            if (varType == VarDeclNode::Constant)
                flags |= ReadOnly;
            variableObject->put(exec, ident, val, flags);
        } else {
            JSValue* val = init->evaluate(exec);
            KJS_CHECKEXCEPTIONVOID

            // if the variable object is the top of the scope chain, then that must
            // be where this variable is declared, processVarDecls would have put 
            // it there. Don't search the scope chain, to optimize this very common case.
            if (chain.top() != variableObject)
                return handleSlowCase(exec, chain, val);

            unsigned flags = 0;
            variableObject->getPropertyAttributes(ident, flags);
            if (varType == VarDeclNode::Constant)
                flags |= ReadOnly;
            
            variableObject->put(exec, ident, val, flags);
        }
    }
}

JSValue* VarDeclNode::evaluate(ExecState* exec)
{
    evaluateSingle(exec);

    if (VarDeclNode* n = next.get()) {
        do {
            n->evaluateSingle(exec);
            KJS_CHECKEXCEPTIONVALUE
            n = n->next.get();
        } while (n);
    }
    return jsUndefined();
}

// ------------------------------ VarStatementNode -----------------------------

void VarStatementNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    ASSERT(next);
    nodeStack.append(next.get());
}

// ECMA 12.2
Completion VarStatementNode::execute(ExecState *exec)
{
    KJS_BREAKPOINT;

    next->evaluate(exec);
    KJS_CHECKEXCEPTION

    return Completion(Normal);
}

// ------------------------------ Helper functions for handling Vectors of StatementNode -------------------------------

static inline void statementListPushFIFO(SourceElements& statements, DeclarationStacks::NodeStack& stack)
{
    SourceElements::iterator it = statements.end();
    SourceElements::iterator begin = statements.begin();
    while (it != begin) {
        --it;
        stack.append((*it).get());
    }
}

static inline Node* statementListInitializeVariableAccessStack(SourceElements& statements, DeclarationStacks::NodeStack& stack)
{
    if (!statements.size())
        return 0;

    SourceElements::iterator it = statements.end();
    SourceElements::iterator begin = statements.begin();
    SourceElements::iterator beginPlusOne = begin + 1;
    
    while (it != beginPlusOne) {
        --it;
        stack.append((*it).get());
    }

    return (*begin).get();
}

static inline Completion statementListExecute(SourceElements& statements, ExecState* exec)
{
    JSValue* v = 0;
    Completion c(Normal);
    const SourceElements::iterator end = statements.end();
    for (SourceElements::iterator ptr = statements.begin(); ptr != end; ++ptr) {
        c = (*ptr)->execute(exec);
        
        if (JSValue* v2 = c.value())
            v = v2;
        c.setValue(v);
        
        if (c.complType() != Normal)
            return c;
    }
    return c;
}
    
// ------------------------------ BlockNode ------------------------------------

BlockNode::BlockNode(SourceElements* children)
    : m_children(children)
{
    ASSERT(m_children);
}

void BlockNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    statementListPushFIFO(*m_children, nodeStack);
}

// ECMA 12.1
Completion BlockNode::execute(ExecState *exec)
{
    return statementListExecute(*m_children, exec);
}

// ------------------------------ EmptyStatementNode ---------------------------

// ECMA 12.3
Completion EmptyStatementNode::execute(ExecState *)
{
  return Completion(Normal);
}

// ------------------------------ ExprStatementNode ----------------------------

void ExprStatementNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    ASSERT(expr);
    nodeStack.append(expr.get());
}

// ECMA 12.4
Completion ExprStatementNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(Normal, v);
}

// ------------------------------ IfNode ---------------------------------------

void IfNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (statement2)
        nodeStack.append(statement2.get());
    ASSERT(statement1);
    nodeStack.append(statement1.get());
    ASSERT(expr);
    nodeStack.append(expr.get());
}

// ECMA 12.5
Completion IfNode::execute(ExecState* exec)
{
    KJS_BREAKPOINT;

    bool b = expr->evaluateToBoolean(exec);
    KJS_CHECKEXCEPTION

    // if ... then
    if (b)
        return statement1->execute(exec);

    // no else
    if (!statement2)
        return Completion(Normal);

    // else
    return statement2->execute(exec);
}

// ------------------------------ DoWhileNode ----------------------------------

void DoWhileNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(statement.get());
    nodeStack.append(expr.get());
}

// ECMA 12.6.1
Completion DoWhileNode::execute(ExecState *exec)
{
    KJS_BREAKPOINT;

    JSValue* value = 0;

    while (1) {
        exec->pushIteration();
        Completion c = statement->execute(exec);
        exec->popIteration();

        if (exec->dynamicGlobalObject()->timedOut())
            return Completion(Interrupted);

        if (c.isValueCompletion())
            value = c.value();

        if (!((c.complType() == Continue) && ls.contains(c.target()))) {
            if ((c.complType() == Break) && ls.contains(c.target()))
                return Completion(Normal, value);
            if (c.complType() != Normal)
                return c;
        }

        bool b = expr->evaluateToBoolean(exec);
        KJS_CHECKEXCEPTION
        if (!b)
            return Completion(Normal, value);
    }

    return Completion(); // work around gcc 4.0 bug
}

// ------------------------------ WhileNode ------------------------------------

void WhileNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(statement.get());
    nodeStack.append(expr.get());
}

// ECMA 12.6.2
Completion WhileNode::execute(ExecState *exec)
{
    KJS_BREAKPOINT;

    JSValue* value = 0;

    while (1) {
        bool b = expr->evaluateToBoolean(exec);
        KJS_CHECKEXCEPTION
        if (!b)
            return Completion(Normal, value);

        exec->pushIteration();
        Completion c = statement->execute(exec);
        exec->popIteration();

        if (exec->dynamicGlobalObject()->timedOut())
            return Completion(Interrupted);
    
        if (c.isValueCompletion())
            value = c.value();

        if ((c.complType() == Continue) && ls.contains(c.target()))
            continue;
        if ((c.complType() == Break) && ls.contains(c.target()))
            return Completion(Normal, value);
        if (c.complType() != Normal)
            return c;
    }

    return Completion(); // work around gcc 4.0 bug
}

// ------------------------------ ForNode --------------------------------------

void ForNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(statement.get());
    if (expr3)
        nodeStack.append(expr3.get());
    if (expr2)
        nodeStack.append(expr2.get());
    if (expr1)
        nodeStack.append(expr1.get());
}

// ECMA 12.6.3
Completion ForNode::execute(ExecState *exec)
{
    JSValue* cval = 0;

    if (expr1) {
        expr1->evaluate(exec);
        KJS_CHECKEXCEPTION
    }

    while (1) {
        if (expr2) {
            bool b = expr2->evaluateToBoolean(exec);
            KJS_CHECKEXCEPTION
            if (!b)
                return Completion(Normal, cval);
        }

        exec->pushIteration();
        Completion c = statement->execute(exec);
        exec->popIteration();
        if (c.isValueCompletion())
            cval = c.value();
        if (!((c.complType() == Continue) && ls.contains(c.target()))) {
            if ((c.complType() == Break) && ls.contains(c.target()))
                return Completion(Normal, cval);
            if (c.complType() != Normal)
                return c;
        }

        if (exec->dynamicGlobalObject()->timedOut())
            return Completion(Interrupted);

        if (expr3) {
            expr3->evaluate(exec);
            KJS_CHECKEXCEPTION
        }
    }
  
    return Completion(); // work around gcc 4.0 bug
}

// ------------------------------ ForInNode ------------------------------------

ForInNode::ForInNode(ExpressionNode* l, ExpressionNode* e, StatementNode* s)
  : init(0L), lexpr(l), expr(e), varDecl(0L), statement(s)
{
}

ForInNode::ForInNode(const Identifier& i, AssignExprNode* in, ExpressionNode* e, StatementNode* s)
  : ident(i), init(in), expr(e), statement(s)
{
  // for( var foo = bar in baz )
  varDecl = new VarDeclNode(ident, init.get(), VarDeclNode::Variable);
  lexpr = new ResolveNode(ident);
}

void ForInNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(statement.get());
    nodeStack.append(expr.get());
    nodeStack.append(lexpr.get());
    if (varDecl)
        nodeStack.append(varDecl.get());
}

// ECMA 12.6.4
Completion ForInNode::execute(ExecState *exec)
{
  JSValue *e;
  JSValue *retval = 0;
  JSObject *v;
  Completion c;
  PropertyNameArray propertyNames;

  if (varDecl) {
    varDecl->evaluate(exec);
    KJS_CHECKEXCEPTION
  }

  e = expr->evaluate(exec);

  // for Null and Undefined, we want to make sure not to go through
  // the loop at all, because their object wrappers will have a
  // property list but will throw an exception if you attempt to
  // access any property.
  if (e->isUndefinedOrNull())
    return Completion(Normal);

  KJS_CHECKEXCEPTION
  v = e->toObject(exec);
  v->getPropertyNames(exec, propertyNames);
  
  PropertyNameArray::const_iterator end = propertyNames.end();
  for (PropertyNameArray::const_iterator it = propertyNames.begin(); it != end; ++it) {
      const Identifier &name = *it;
      if (!v->hasProperty(exec, name))
          continue;

      JSValue *str = jsOwnedString(name.ustring());

      if (lexpr->isResolveNode()) {
        const Identifier &ident = static_cast<ResolveNode *>(lexpr.get())->identifier();

        const ScopeChain& chain = exec->scopeChain();
        ScopeChainIterator iter = chain.begin();
        ScopeChainIterator end = chain.end();
  
        // we must always have something in the scope chain
        ASSERT(iter != end);

        PropertySlot slot;
        JSObject *o;
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
    } else if (lexpr->isDotAccessorNode()) {
        const Identifier& ident = static_cast<DotAccessorNode *>(lexpr.get())->identifier();
        JSValue *v = static_cast<DotAccessorNode *>(lexpr.get())->base()->evaluate(exec);
        KJS_CHECKEXCEPTION
        JSObject *o = v->toObject(exec);

        o->put(exec, ident, str);
    } else {
        ASSERT(lexpr->isBracketAccessorNode());
        JSValue *v = static_cast<BracketAccessorNode *>(lexpr.get())->base()->evaluate(exec);
        KJS_CHECKEXCEPTION
        JSValue *v2 = static_cast<BracketAccessorNode *>(lexpr.get())->subscript()->evaluate(exec);
        KJS_CHECKEXCEPTION
        JSObject *o = v->toObject(exec);

        uint32_t i;
        if (v2->getUInt32(i))
            o->put(exec, i, str);
        o->put(exec, Identifier(v2->toString(exec)), str);
    }

    KJS_CHECKEXCEPTION

    exec->pushIteration();
    c = statement->execute(exec);
    exec->popIteration();
    if (c.isValueCompletion())
      retval = c.value();

    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        break;
      if (c.complType() != Normal) {
        return c;
      }
    }
  }

  return Completion(Normal, retval);
}

// ------------------------------ ContinueNode ---------------------------------

// ECMA 12.7
Completion ContinueNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  if (ident.isEmpty() && !exec->inIteration())
    return createErrorCompletion(exec, SyntaxError, "Invalid continue statement.");
  if (!ident.isEmpty() && !exec->seenLabels()->contains(ident))
    return createErrorCompletion(exec, SyntaxError, "Label %s not found.", ident);
  return Completion(Continue, &ident);
}

// ------------------------------ BreakNode ------------------------------------

// ECMA 12.8
Completion BreakNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  if (ident.isEmpty() && !exec->inIteration() &&
      !exec->inSwitch())
    return createErrorCompletion(exec, SyntaxError, "Invalid break statement.");
  if (!ident.isEmpty() && !exec->seenLabels()->contains(ident))
    return createErrorCompletion(exec, SyntaxError, "Label %s not found.");
  return Completion(Break, &ident);
}

// ------------------------------ ReturnNode -----------------------------------

void ReturnNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (value)
        nodeStack.append(value.get());
}

// ECMA 12.9
Completion ReturnNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  CodeType codeType = exec->codeType();
  if (codeType != FunctionCode)
    return createErrorCompletion(exec, SyntaxError, "Invalid return statement.");

  if (!value)
    return Completion(ReturnValue, jsUndefined());

  JSValue *v = value->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(ReturnValue, v);
}

// ------------------------------ WithNode -------------------------------------

void WithNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    // Can't optimize within statement because "with" introduces a dynamic scope.
    nodeStack.append(expr.get());
}

// ECMA 12.10
Completion WithNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION
  JSObject *o = v->toObject(exec);
  KJS_CHECKEXCEPTION
  exec->pushScope(o);
  Completion res = statement->execute(exec);
  exec->popScope();

  return res;
}
    
// ------------------------------ CaseClauseNode -------------------------------

void CaseClauseNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (expr)
        nodeStack.append(expr.get());
    if (m_children)
        statementListPushFIFO(*m_children, nodeStack);
}

// ECMA 12.11
JSValue *CaseClauseNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v;
}

// ECMA 12.11
Completion CaseClauseNode::evalStatements(ExecState *exec)
{
  if (m_children)
    return statementListExecute(*m_children, exec);
  else
    return Completion(Normal, jsUndefined());
}

// ------------------------------ ClauseListNode -------------------------------

void ClauseListNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (next)
        nodeStack.append(next.get());
    nodeStack.append(clause.get());
}

// ------------------------------ CaseBlockNode --------------------------------

CaseBlockNode::CaseBlockNode(ClauseListNode* l1, CaseClauseNode* d, ClauseListNode* l2)
    : list1(l1)
    , def(d)
    , list2(l2)
{
}
 
void CaseBlockNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    if (list2)
        nodeStack.append(list2.get());
    if (def)
        nodeStack.append(def.get());
    if (list1)
        nodeStack.append(list1.get());
}

// ECMA 12.11
Completion CaseBlockNode::evalBlock(ExecState *exec, JSValue *input)
{
  JSValue *v;
  Completion res;
  ClauseListNode *a = list1.get();
  ClauseListNode *b = list2.get();
  CaseClauseNode *clause;

    while (a) {
      clause = a->getClause();
      a = a->getNext();
      v = clause->evaluate(exec);
      KJS_CHECKEXCEPTION
      if (strictEqual(exec, input, v)) {
        res = clause->evalStatements(exec);
        if (res.complType() != Normal)
          return res;
        while (a) {
          res = a->getClause()->evalStatements(exec);
          if (res.complType() != Normal)
            return res;
          a = a->getNext();
        }
        break;
      }
    }

  while (b) {
    clause = b->getClause();
    b = b->getNext();
    v = clause->evaluate(exec);
    KJS_CHECKEXCEPTION
    if (strictEqual(exec, input, v)) {
      res = clause->evalStatements(exec);
      if (res.complType() != Normal)
        return res;
      goto step18;
    }
  }

  // default clause
  if (def) {
    res = def->evalStatements(exec);
    if (res.complType() != Normal)
      return res;
  }
  b = list2.get();
 step18:
  while (b) {
    clause = b->getClause();
    res = clause->evalStatements(exec);
    if (res.complType() != Normal)
      return res;
    b = b->getNext();
  }

  // bail out on error
  KJS_CHECKEXCEPTION

  return Completion(Normal);
}

// ------------------------------ SwitchNode -----------------------------------

void SwitchNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(block.get());
    nodeStack.append(expr.get());
}

// ECMA 12.11
Completion SwitchNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  exec->pushSwitch();
  Completion res = block->evalBlock(exec,v);
  exec->popSwitch();

  if ((res.complType() == Break) && ls.contains(res.target()))
    return Completion(Normal, res.value());
  return res;
}

// ------------------------------ LabelNode ------------------------------------

void LabelNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(statement.get());
}

// ECMA 12.12
Completion LabelNode::execute(ExecState *exec)
{
  if (!exec->seenLabels()->push(label))
    return createErrorCompletion(exec, SyntaxError, "Duplicated label %s found.", label);
  Completion e = statement->execute(exec);
  exec->seenLabels()->pop();

  if ((e.complType() == Break) && (e.target() == label))
    return Completion(Normal, e.value());
  return e;
}

// ------------------------------ ThrowNode ------------------------------------

void ThrowNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    nodeStack.append(expr.get());
}

// ECMA 12.13
Completion ThrowNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  handleException(exec, v);
  return Completion(Throw, v);
}

// ------------------------------ TryNode --------------------------------------

void TryNode::optimizeVariableAccess(SymbolTable&, DeclarationStacks::NodeStack& nodeStack)
{
    // Can't optimize within catchBlock because "catch" introduces a dynamic scope.
    if (finallyBlock)
        nodeStack.append(finallyBlock.get());
    nodeStack.append(tryBlock.get());
}

// ECMA 12.14
Completion TryNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Completion c = tryBlock->execute(exec);

  if (Collector::isOutOfMemory())
      return c; // don't try to catch an out of memory exception thrown by the collector
  
  if (catchBlock && c.complType() == Throw) {
    JSObject *obj = new JSObject;
    obj->put(exec, exceptionIdent, c.value(), DontDelete);
    exec->pushScope(obj);
    c = catchBlock->execute(exec);
    exec->popScope();
  }

  if (finallyBlock) {
    Completion c2 = finallyBlock->execute(exec);
    if (c2.complType() != Normal)
      c = c2;
  }

  return c;
}

// ------------------------------ FunctionBodyNode -----------------------------

ScopeNode::ScopeNode(SourceElements* children, DeclarationStacks::VarStack* varStack, DeclarationStacks::FunctionStack* funcStack)
    : BlockNode(children)
    , m_sourceURL(parser().sourceURL())
    , m_sourceId(parser().sourceId())
{
    if (varStack)
        m_varStack = *varStack;

    if (funcStack)
        m_functionStack = *funcStack;
}

ProgramNode::ProgramNode(SourceElements* children, DeclarationStacks::VarStack* varStack, DeclarationStacks::FunctionStack* funcStack)
    : ScopeNode(children, varStack, funcStack)
{
}

EvalNode::EvalNode(SourceElements* children, DeclarationStacks::VarStack* varStack, DeclarationStacks::FunctionStack* funcStack)
    : ScopeNode(children, varStack, funcStack)
{
}

FunctionBodyNode::FunctionBodyNode(SourceElements* children, DeclarationStacks::VarStack* varStack, DeclarationStacks::FunctionStack* funcStack)
    : ScopeNode(children, varStack, funcStack)
    , m_initialized(false)
{
}

void FunctionBodyNode::initializeSymbolTable(ExecState* exec)
{
    SymbolTable& symbolTable = exec->variableObject()->symbolTable();
    ASSERT(!symbolTable.size());

    size_t localStorageIndex = 0;

    for (size_t i = 0, size = m_parameters.size(); i < size; ++i, ++localStorageIndex) {
        UString::Rep* rep = m_parameters[i].ustring().rep();
        symbolTable.set(rep, localStorageIndex);
    }

    for (size_t i = 0, size = m_functionStack.size(); i < size; ++i, ++localStorageIndex) {
        UString::Rep* rep = m_functionStack[i]->ident.ustring().rep();
        symbolTable.set(rep, localStorageIndex);
    }

    for (size_t i = 0, size = m_varStack.size(); i < size; ++i, ++localStorageIndex) {
        Identifier& ident = m_varStack[i]->ident;
        if (ident == exec->propertyNames().arguments)
            continue;
        symbolTable.add(ident.ustring().rep(), localStorageIndex);
    }
}

void ProgramNode::initializeSymbolTable(ExecState* exec)
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
    
    size = m_functionStack.size();
    m_functionIndexes.resize(size);
    for (size_t i = 0; i < size; ++i) {
        UString::Rep* rep = m_functionStack[i]->ident.ustring().rep();
        pair<SymbolTable::iterator, bool> result = symbolTable.add(rep, localStorageIndex);
        m_functionIndexes[i] = result.first->second;
        if (result.second)
            ++localStorageIndex;
    }

    size = m_varStack.size();
    m_varIndexes.resize(size);
    for (size_t i = 0; i < size; ++i) {
        const Identifier& ident = m_varStack[i]->ident;
        if (variableObject->getDirect(ident)) {
            m_varIndexes[i] = missingSymbolMarker(); // Signal not to initialize this declaration.
            continue;
        }

        UString::Rep* rep = ident.ustring().rep();
        pair<SymbolTable::iterator, bool> result = symbolTable.add(rep, localStorageIndex);
        if (!result.second) {
            m_varIndexes[i] = missingSymbolMarker(); // Signal not to initialize this declaration.
            continue;
        }
        m_varIndexes[i] = result.first->second;
        ++localStorageIndex;
    }
}

void ScopeNode::optimizeVariableAccess(ExecState* exec)
{
    DeclarationStacks::NodeStack nodeStack;
    Node* node = statementListInitializeVariableAccessStack(*m_children, nodeStack);
    if (!node)
        return;
    
    SymbolTable& symbolTable = exec->variableObject()->symbolTable();
    while (true) {
        node->optimizeVariableAccess(symbolTable, nodeStack);
        
        size_t size = nodeStack.size();
        if (!size)
            break;
        --size;
        node = nodeStack[size];
        nodeStack.shrink(size);
    }
}

void FunctionBodyNode::processDeclarations(ExecState* exec)
{
    if (!m_initialized) {
        initializeSymbolTable(exec);
        optimizeVariableAccess(exec);
        
        m_initialized = true;
    }

    LocalStorage& localStorage = exec->variableObject()->localStorage();
    
    // We can't just resize localStorage here because that would temporarily
    // leave uninitialized entries, which would crash GC during the mark phase.
    localStorage.reserveCapacity(m_varStack.size() + m_parameters.size() + m_functionStack.size());
    
    int minAttributes = Internal | DontDelete;
    
    // In order for our localStorage indexes to be correct, we must match the 
    // order of addition in initializeSymbolTable().

    const List& args = *exec->arguments();
    for (size_t i = 0, size = m_parameters.size(); i < size; ++i)
        localStorage.uncheckedAppend(LocalStorageEntry(args[i], DontDelete));

    for (size_t i = 0, size = m_functionStack.size(); i < size; ++i) {
        FuncDeclNode* node = m_functionStack[i];
        localStorage.uncheckedAppend(LocalStorageEntry(node->makeFunction(exec), minAttributes));
    }

    for (size_t i = 0, size = m_varStack.size(); i < size; ++i) {
        VarDeclNode* node = m_varStack[i];
        int attributes = minAttributes;
        if (node->varType == VarDeclNode::Constant)
            attributes |= ReadOnly;
        localStorage.uncheckedAppend(LocalStorageEntry(jsUndefined(), attributes));
    }
}

static void gccIsCrazy() KJS_FAST_CALL;
static void gccIsCrazy()
{
}

void ProgramNode::processDeclarations(ExecState* exec)
{
    // If you remove this call, some SunSpider tests, including
    // bitops-nsieve-bits.js, will regress substantially on Mac, due to a ~40%
    // increase in L2 cache misses. FIXME: WTF?
    gccIsCrazy();
    
    initializeSymbolTable(exec);
    optimizeVariableAccess(exec);

    LocalStorage& localStorage = exec->variableObject()->localStorage();

    // We can't just resize localStorage here because that would temporarily
    // leave uninitialized entries, which would crash GC during the mark phase.
    localStorage.reserveCapacity(localStorage.size() + m_varStack.size() + m_functionStack.size());

    int minAttributes = Internal | DontDelete;

    // In order for our localStorage indexes to be correct, we must match the
    // order of addition in initializeSymbolTable().

    for (size_t i = 0, size = m_functionStack.size(); i < size; ++i) {
        FuncDeclNode* node = m_functionStack[i];
        LocalStorageEntry entry = LocalStorageEntry(node->makeFunction(exec), minAttributes);
        size_t index = m_functionIndexes[i];

        if (index == localStorage.size())
            localStorage.uncheckedAppend(entry);
        else {
            ASSERT(index < localStorage.size());
            localStorage[index] = entry;
        }
    }

    for (size_t i = 0, size = m_varStack.size(); i < size; ++i) {
        size_t index = m_varIndexes[i];
        if (index == missingSymbolMarker())
            continue;

        VarDeclNode* node = m_varStack[i];
        int attributes = minAttributes;
        if (node->varType == VarDeclNode::Constant)
            attributes |= ReadOnly;
        LocalStorageEntry entry = LocalStorageEntry(jsUndefined(), attributes);
            
        ASSERT(index == localStorage.size());
        localStorage.uncheckedAppend(entry);
    }
}

void EvalNode::processDeclarations(ExecState* exec)
{
    // We could optimize access to pre-existing symbols here, but SunSpider
    // reports that to be a net loss.

    size_t i, size;

    JSVariableObject* variableObject = exec->variableObject();
    
    int minAttributes = Internal;

    for (i = 0, size = m_varStack.size(); i < size; ++i) {
        VarDeclNode* node = m_varStack[i];
        if (variableObject->hasOwnProperty(exec, node->ident))
            continue;
        int attributes = minAttributes;
        if (node->varType == VarDeclNode::Constant)
            attributes |= ReadOnly;
        variableObject->put(exec, node->ident, jsUndefined(), attributes);
    }

    for (i = 0, size = m_functionStack.size(); i < size; ++i) {
        FuncDeclNode* node = m_functionStack[i];
        variableObject->put(exec, node->ident, node->makeFunction(exec), minAttributes);
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

Completion ProgramNode::execute(ExecState* exec)
{
    processDeclarations(exec);
    return ScopeNode::execute(exec);
}

Completion EvalNode::execute(ExecState* exec)
{
    processDeclarations(exec);
    return ScopeNode::execute(exec);
}

Completion FunctionBodyNode::execute(ExecState* exec)
{
    processDeclarations(exec);

    if (Debugger* dbg = exec->dynamicGlobalObject()->debugger()) {
        if (!dbg->callEvent(exec, sourceId(), lineNo(), exec->function(), *exec->arguments())) {
            dbg->imp()->abort();
            return Completion(Interrupted, jsUndefined());
        }
    }    
    
    Completion completion = ScopeNode::execute(exec);
    
    if (Debugger* dbg = exec->dynamicGlobalObject()->debugger()) {
        if (completion.complType() == Throw)
            exec->setException(completion.value());

        if (!dbg->returnEvent(exec, sourceId(), lineNo(), exec->function())) {
            dbg->imp()->abort();
            return Completion(Interrupted, jsUndefined());
        }
    }

    
    return completion;
}

// ------------------------------ FuncDeclNode ---------------------------------

void FuncDeclNode::addParams() 
{
  for (ParameterNode *p = param.get(); p != 0L; p = p->nextParam())
    body->parameters().append(p->ident());
}

FunctionImp* FuncDeclNode::makeFunction(ExecState* exec)
{
  FunctionImp *func = new FunctionImp(exec, ident, body.get(), exec->scopeChain());

  JSObject *proto = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, List::empty());
  proto->put(exec, exec->propertyNames().constructor, func, ReadOnly | DontDelete | DontEnum);
  func->put(exec, exec->propertyNames().prototype, proto, Internal|DontDelete);

  func->put(exec, exec->propertyNames().length, jsNumber(body->parameters().size()), ReadOnly|DontDelete|DontEnum);
  return func;
}

Completion FuncDeclNode::execute(ExecState *)
{
    return Completion(Normal);
}

// ------------------------------ FuncExprNode ---------------------------------

// ECMA 13
void FuncExprNode::addParams()
{
  for (ParameterNode *p = param.get(); p != 0L; p = p->nextParam())
    body->parameters().append(p->ident());
}

JSValue *FuncExprNode::evaluate(ExecState *exec)
{
  bool named = !ident.isNull();
  JSObject *functionScopeObject = 0;

  if (named) {
    // named FunctionExpressions can recursively call themselves,
    // but they won't register with the current scope chain and should
    // be contained as single property in an anonymous object.
    functionScopeObject = new JSObject;
    exec->pushScope(functionScopeObject);
  }

  FunctionImp* func = new FunctionImp(exec, ident, body.get(), exec->scopeChain());
  JSObject* proto = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, List::empty());
  proto->put(exec, exec->propertyNames().constructor, func, ReadOnly | DontDelete | DontEnum);
  func->put(exec, exec->propertyNames().prototype, proto, Internal | DontDelete);

  if (named) {
    functionScopeObject->put(exec, ident, func, Internal | ReadOnly | (exec->codeType() == EvalCode ? 0 : DontDelete));
    exec->popScope();
  }

  return func;
}

} // namespace KJS
