/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#include <math.h>
#include <assert.h>
#ifdef KJS_DEBUG_MEM
#include <stdio.h>
#include <typeinfo>
#endif

#include "collector.h"
#include "context.h"
#include "debugger.h"
#include "function_object.h"
#include "internal.h"
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "lexer.h"
#include "operations.h"
#include "ustring.h"
#include "reference_list.h"

using namespace KJS;

#define KJS_BREAKPOINT \
  if (Debugger::debuggersPresent > 0 && !hitStatement(exec)) \
    return Completion(Normal);

#define KJS_ABORTPOINT \
  if (Debugger::debuggersPresent > 0 && \
      exec->dynamicInterpreter()->imp()->debugger() && \
      exec->dynamicInterpreter()->imp()->debugger()->imp()->aborted()) \
    return Completion(Normal);

#define KJS_CHECKEXCEPTION \
  if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    JSValue *ex = exec->exception(); \
    exec->clearException(); \
    return Completion(Throw, ex); \
  } \
  if (Collector::isOutOfMemory()) \
    return Completion(Throw, Error::create(exec, GeneralError, "Out of memory"));

#define KJS_CHECKEXCEPTIONVALUE \
  if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    return jsUndefined(); \
  } \
  if (Collector::isOutOfMemory()) \
    return jsUndefined(); // will be picked up by KJS_CHECKEXCEPTION

#define KJS_CHECKEXCEPTIONLIST \
  if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    return List(); \
  } \
  if (Collector::isOutOfMemory()) \
    return List(); // will be picked up by KJS_CHECKEXCEPTION

// ------------------------------ Node -----------------------------------------


#ifndef NDEBUG
struct NodeCounter { 
    static int count; 
    ~NodeCounter() { if (count != 0) fprintf(stderr, "LEAK: %d KJS::Node\n", count); }
};
int NodeCounter::count = 0;
static NodeCounter nodeImplCounter;
#endif NDEBUG


Node::Node()
{
#ifndef NDEBUG
    ++NodeCounter::count;
#endif
  line = Lexer::curr()->lineNo();
  sourceURL = Lexer::curr()->sourceURL();
  m_refcount = 0;
  Parser::saveNewNode(this);
}

Node::~Node()
{
#ifndef NDEBUG
    --NodeCounter::count;
#endif
}

static void substitute(UString &string, const UString &substring)
{
    int position = string.find("%s");
    assert(position != -1);
    string = string.substr(0, position) + substring + string.substr(position + 2);
}

Completion Node::createErrorCompletion(ExecState *exec, ErrorType e, const char *msg)
{
    return Completion(Throw, Error::create(exec, e, msg, lineNo(), sourceId(), &sourceURL));
}

Completion Node::createErrorCompletion(ExecState *exec, ErrorType e, const char *msg, const Identifier &ident)
{
    UString message = msg;
    substitute(message, ident.ustring());
    return Completion(Throw, Error::create(exec, e, message, lineNo(), sourceId(), &sourceURL));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg)
{
    return KJS::throwError(exec, e, msg, lineNo(), sourceId(), &sourceURL);
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *expr)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    return KJS::throwError(exec, e, message, lineNo(), sourceId(), &sourceURL);
}


JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, const Identifier &label)
{
    UString message = msg;
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), sourceId(), &sourceURL);
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *e1, Node *e2)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, e1->toString());
    substitute(message, e2->toString());
    return KJS::throwError(exec, e, message, lineNo(), sourceId(), &sourceURL);
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *expr, const Identifier &label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), sourceId(), &sourceURL);
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, const Identifier &label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), sourceId(), &sourceURL);
}

JSValue *Node::throwUndefinedVariableError(ExecState *exec, const Identifier &ident)
{
    return throwError(exec, ReferenceError, "Can't find variable: %s", ident);
}

void Node::setExceptionDetailsIfNeeded(ExecState *exec)
{
    JSValue *exceptionValue = exec->exception();
    if (exceptionValue->isObject()) {
        JSObject *exception = static_cast<JSObject *>(exceptionValue);
        if (!exception->hasProperty(exec, "line") && !exception->hasProperty(exec, "sourceURL")) {
            exception->put(exec, "line", jsNumber(line));
            exception->put(exec, "sourceURL", jsString(sourceURL));
        }
    }
}

Node *Node::nodeInsideAllParens()
{
    return this;
}

// ------------------------------ StatementNode --------------------------------

StatementNode::StatementNode() : l0(-1), l1(-1), sid(-1), breakPoint(false)
{
}

void StatementNode::setLoc(int line0, int line1, int sourceId)
{
    l0 = line0;
    l1 = line1;
    sid = sourceId;
}

// return true if the debugger wants us to stop at this point
bool StatementNode::hitStatement(ExecState *exec)
{
  Debugger *dbg = exec->dynamicInterpreter()->imp()->debugger();
  if (dbg)
    return dbg->atStatement(exec,sid,l0,l1);
  else
    return true; // continue
}

void StatementNode::processFuncDecl(ExecState *exec)
{
}

// ------------------------------ NullNode -------------------------------------

JSValue *NullNode::evaluate(ExecState *)
{
  return jsNull();
}

// ------------------------------ BooleanNode ----------------------------------

JSValue *BooleanNode::evaluate(ExecState *)
{
  return jsBoolean(value);
}

// ------------------------------ NumberNode -----------------------------------

JSValue *NumberNode::evaluate(ExecState *)
{
  return jsNumber(value);
}

// ------------------------------ StringNode -----------------------------------

JSValue *StringNode::evaluate(ExecState *)
{
  return jsString(value);
}

// ------------------------------ RegExpNode -----------------------------------

JSValue *RegExpNode::evaluate(ExecState *exec)
{
  List list;
  list.append(jsString(pattern));
  list.append(jsString(flags));

  JSObject *reg = exec->lexicalInterpreter()->imp()->builtinRegExp();
  return reg->construct(exec,list);
}

// ------------------------------ ThisNode -------------------------------------

// ECMA 11.1.1
JSValue *ThisNode::evaluate(ExecState *exec)
{
  return exec->context().imp()->thisValue();
}

// ------------------------------ ResolveNode ----------------------------------

// ECMA 11.1.2 & 10.1.4
JSValue *ResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->context().imp()->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  assert(iter != end);

  PropertySlot slot;
  do { 
    JSObject *o = *iter;

    if (o->getPropertySlot(exec, ident, slot))
      return slot.getValue(exec, o, ident);
    
    ++iter;
  } while (iter != end);

  return throwUndefinedVariableError(exec, ident);
}

// ------------------------------ GroupNode ------------------------------------

// ECMA 11.1.6
JSValue *GroupNode::evaluate(ExecState *exec)
{
  return group->evaluate(exec);
}

Node *GroupNode::nodeInsideAllParens()
{
    Node *n = this;
    do
        n = static_cast<GroupNode *>(n)->group.get();
    while (n->isGroupNode());
    return n;
}

// ------------------------------ ElementNode ----------------------------------

// ECMA 11.1.4
JSValue *ElementNode::evaluate(ExecState *exec)
{
  JSObject *array = exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty());
  int length = 0;
  for (ElementNode *n = this; n; n = n->next.get()) {
    JSValue *val = n->node->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    length += n->elision;
    array->put(exec, length++, val);
  }
  return array;
}

void ElementNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ ArrayNode ------------------------------------

// ECMA 11.1.4
JSValue *ArrayNode::evaluate(ExecState *exec)
{
  JSObject *array;
  int length;

  if (element) {
    array = static_cast<JSObject*>(element->evaluate(exec));
    KJS_CHECKEXCEPTIONVALUE
    length = opt ? array->get(exec,lengthPropertyName)->toInt32(exec) : 0;
  } else {
    JSValue *newArr = exec->lexicalInterpreter()->builtinArray()->construct(exec,List::empty());
    array = static_cast<JSObject*>(newArr);
    length = 0;
  }

  if (opt)
    array->put(exec,lengthPropertyName, jsNumber(elision + length), DontEnum | DontDelete);

  return array;
}

// ------------------------------ ObjectLiteralNode ----------------------------

// ECMA 11.1.5
JSValue *ObjectLiteralNode::evaluate(ExecState *exec)
{
  if (list)
    return list->evaluate(exec);

  return exec->lexicalInterpreter()->builtinObject()->construct(exec,List::empty());
}

// ------------------------------ PropertyListNode -----------------------------

// ECMA 11.1.5
JSValue *PropertyListNode::evaluate(ExecState *exec)
{
  JSObject *obj = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
  
  for (PropertyListNode *p = this; p; p = p->next.get()) {
    JSValue *n = p->node->name->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    JSValue *v = p->node->assign->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    
    Identifier propertyName = Identifier(n->toString(exec));
    switch (p->node->type) {
      case PropertyNode::Getter:
        assert(v->isObject());
        obj->defineGetter(exec, propertyName, static_cast<JSObject *>(v));
        break;
      case PropertyNode::Setter:
        assert(v->isObject());
        obj->defineSetter(exec, propertyName, static_cast<JSObject *>(v));
        break;
      case PropertyNode::Constant:
        obj->put(exec, propertyName, v);
        break;
    }
  }

  return obj;
}

void PropertyListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ PropertyNode -----------------------------
// ECMA 11.1.5
JSValue *PropertyNode::evaluate(ExecState *exec)
{
  assert(false);
  return jsNull();
}

// ---------------------------- PropertyNameNode -------------------------------

// ECMA 11.1.5
JSValue *PropertyNameNode::evaluate(ExecState *)
{
  JSValue *s;

  if (str.isNull()) {
    s = jsString(UString::from(numeric));
  } else {
    s = jsString(str.ustring());
  }

  return s;
}

// ------------------------------ BracketAccessorNode --------------------------------

// ECMA 11.2.1a
JSValue *BracketAccessorNode::evaluate(ExecState *exec)
{
  JSValue *v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *o = v1->toObject(exec);
  uint32_t i;
  if (v2->getUInt32(i))
    return o->get(exec, i);
  return o->get(exec, Identifier(v2->toString(exec)));
}

// ------------------------------ DotAccessorNode --------------------------------

// ECMA 11.2.1b
JSValue *DotAccessorNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  return v->toObject(exec)->get(exec, ident);

}

// ------------------------------ ArgumentListNode -----------------------------

JSValue *ArgumentListNode::evaluate(ExecState *)
{
  assert(0);
  return 0; // dummy, see evaluateList()
}

// ECMA 11.2.4
List ArgumentListNode::evaluateList(ExecState *exec)
{
  List l;

  for (ArgumentListNode *n = this; n; n = n->next.get()) {
    JSValue *v = n->expr->evaluate(exec);
    KJS_CHECKEXCEPTIONLIST
    l.append(v);
  }

  return l;
}

void ArgumentListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ ArgumentsNode --------------------------------

JSValue *ArgumentsNode::evaluate(ExecState *)
{
  assert(0);
  return 0; // dummy, see evaluateList()
}

// ------------------------------ NewExprNode ----------------------------------

// ECMA 11.2.2

JSValue *NewExprNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  List argList;
  if (args) {
    argList = args->evaluateList(exec);
    KJS_CHECKEXCEPTIONVALUE
  }

  if (!v->isObject()) {
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v, expr.get());
  }

  JSObject *constr = static_cast<JSObject*>(v);
  if (!constr->implementsConstruct()) {
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v, expr.get());
  }

  return constr->construct(exec, argList);
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

  List argList = args->evaluateList(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *thisObj =  exec->dynamicInterpreter()->globalObject();

  return func->call(exec, thisObj, argList);
}

// ECMA 11.2.3
JSValue *FunctionCallResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->context().imp()->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  assert(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, ident, slot)) {
      JSValue *v = slot.getValue(exec, base, ident);
      KJS_CHECKEXCEPTIONVALUE
        
      if (!v->isObject()) {
        return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident);
      }
      
      JSObject *func = static_cast<JSObject*>(v);
      
      if (!func->implementsCall()) {
        return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident);
      }
      
      List argList = args->evaluateList(exec);
      KJS_CHECKEXCEPTIONVALUE
        
      JSObject *thisObj = base;
      // ECMA 11.2.3 says that in this situation the this value should be null.
      // However, section 10.2.3 says that in the case where the value provided
      // by the caller is null, the global object should be used. It also says
      // that the section does not apply to interal functions, but for simplicity
      // of implementation we use the global object anyway here. This guarantees
      // that in host objects you always get a valid object for this.
      if (thisObj->isActivation())
        thisObj = exec->dynamicInterpreter()->globalObject();

      return func->call(exec, thisObj, argList);
    }
    ++iter;
  } while (iter != end);
  
  return throwUndefinedVariableError(exec, ident);
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

  List argList = args->evaluateList(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *thisObj = baseObj;
  assert(thisObj);
  assert(thisObj->isObject());
  assert(!thisObj->isActivation());

  return func->call(exec, thisObj, argList);
}

static const char *dotExprNotAnObjectString()
{
  return "Value %s (result of expression %s.%s) is not object.";
}

static const char *dotExprDoesNotAllowCallsString() 
{
  return "Object %s (result of expression %s.%s) does not allow calls.";
}

// ECMA 11.2.3
JSValue *FunctionCallDotNode::evaluate(ExecState *exec)
{
  JSValue *baseVal = base->evaluate(exec);

  JSObject *baseObj = baseVal->toObject(exec);
  PropertySlot slot;
  JSValue *funcVal = baseObj->getPropertySlot(exec, ident, slot) ? slot.getValue(exec, baseObj, ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  if (!funcVal->isObject())
    return throwError(exec, TypeError, dotExprNotAnObjectString(), funcVal, base.get(), ident);
  
  JSObject *func = static_cast<JSObject*>(funcVal);

  if (!func->implementsCall())
    return throwError(exec, TypeError, dotExprDoesNotAllowCallsString(), funcVal, base.get(), ident);

  List argList = args->evaluateList(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *thisObj = baseObj;
  assert(thisObj);
  assert(thisObj->isObject());
  assert(!thisObj->isActivation());

  return func->call(exec, thisObj, argList);
}

// ECMA 11.3

// ------------------------------ PostfixResolveNode ----------------------------------

JSValue *PostfixResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->context().imp()->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  assert(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
        JSValue *v = slot.getValue(exec, base, m_ident);

        double n = v->toNumber(exec);
        
        double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
        base->put(exec, m_ident, jsNumber(newValue));
        
        return jsNumber(n);
    }

    ++iter;
  } while (iter != end);

  return throwUndefinedVariableError(exec, m_ident);
}

// ------------------------------ PostfixBracketNode ----------------------------------

JSValue *PostfixBracketNode::evaluate(ExecState *exec)
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

    double n = v->toNumber(exec);

    double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
    base->put(exec, propertyIndex, jsNumber(newValue));
        
    return jsNumber(n);
  }

  Identifier propertyName(subscript->toString(exec));
  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  double n = v->toNumber(exec);
  
  double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
  base->put(exec, propertyName, jsNumber(newValue));
        
  return jsNumber(n);
}

// ------------------------------ PostfixDotNode ----------------------------------

JSValue *PostfixDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  double n = v->toNumber(exec);
  
  double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
  base->put(exec, m_ident, jsNumber(newValue));
        
  return jsNumber(n);
}

// ECMA 11.4.1

// ------------------------------ DeleteResolveNode -----------------------------------
JSValue *DeleteResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->context().imp()->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  assert(iter != end);

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

// ------------------------------ DeleteBracketNode -----------------------------------
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
JSValue *DeleteDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  JSObject *base = baseValue->toObject(exec);
  KJS_CHECKEXCEPTIONVALUE

  return jsBoolean(base->deleteProperty(exec, m_ident));
}

// ------------------------------ DeleteValueNode -----------------------------------
JSValue *DeleteValueNode::evaluate(ExecState *exec)
{
  m_expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  // delete on a non-location expression ignores the value and returns true
  return jsBoolean(true);
}

// ------------------------------ VoidNode -------------------------------------

// ECMA 11.4.2
JSValue *VoidNode::evaluate(ExecState *exec)
{
  expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return jsUndefined();
}

// ECMA 11.4.3

// ------------------------------ TypeOfValueNode -----------------------------------

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

JSValue *TypeOfResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->context().imp()->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  assert(iter != end);

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

JSValue *PrefixResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->context().imp()->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  assert(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot)) {
        JSValue *v = slot.getValue(exec, base, m_ident);

        double n = v->toNumber(exec);
        
        double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
        JSValue *n2 = jsNumber(newValue);
        base->put(exec, m_ident, n2);

        return n2;
    }

    ++iter;
  } while (iter != end);

  return throwUndefinedVariableError(exec, m_ident);
}

// ------------------------------ PrefixBracketNode ----------------------------------

JSValue *PrefixBracketNode::evaluate(ExecState *exec)
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

    double n = v->toNumber(exec);

    double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
    JSValue *n2 = jsNumber(newValue);
    base->put(exec, propertyIndex, n2);

    return n2;
  }

  Identifier propertyName(subscript->toString(exec));
  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  double n = v->toNumber(exec);
  
  double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
  JSValue *n2 = jsNumber(newValue);
  base->put(exec, propertyName, n2);

  return n2;
}

// ------------------------------ PrefixDotNode ----------------------------------

JSValue *PrefixDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  PropertySlot slot;
  JSValue *v = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
  KJS_CHECKEXCEPTIONVALUE

  double n = v->toNumber(exec);
  
  double newValue = (m_oper == OpPlusPlus) ? n + 1 : n - 1;
  JSValue *n2 = jsNumber(newValue);
  base->put(exec, m_ident, n2);

  return n2;
}

// ------------------------------ UnaryPlusNode --------------------------------

// ECMA 11.4.6
JSValue *UnaryPlusNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return jsNumber(v->toNumber(exec));
}

// ------------------------------ NegateNode -----------------------------------

// ECMA 11.4.7
JSValue *NegateNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  double n = v->toNumber(exec);
  return jsNumber(-n);
}

// ------------------------------ BitwiseNotNode -------------------------------

// ECMA 11.4.8
JSValue *BitwiseNotNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  return jsNumber(~v->toInt32(exec));
}

// ------------------------------ LogicalNotNode -------------------------------

// ECMA 11.4.9
JSValue *LogicalNotNode::evaluate(ExecState *exec)
{
  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  return jsBoolean(!v->toBoolean(exec));
}

// ------------------------------ MultNode -------------------------------------

// ECMA 11.5
JSValue *MultNode::evaluate(ExecState *exec)
{
  JSValue *v1 = term1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSValue *v2 = term2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return mult(exec, v1, v2, oper);
}

// ------------------------------ AddNode --------------------------------------

// ECMA 11.6
JSValue *AddNode::evaluate(ExecState *exec)
{
  JSValue *v1 = term1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSValue *v2 = term2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return add(exec, v1, v2, oper);
}

// ------------------------------ ShiftNode ------------------------------------

// ECMA 11.7
JSValue *ShiftNode::evaluate(ExecState *exec)
{
  JSValue *v1 = term1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *v2 = term2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  unsigned int i2 = v2->toUInt32(exec);
  i2 &= 0x1f;

  switch (oper) {
  case OpLShift:
    return jsNumber(v1->toInt32(exec) << i2);
  case OpRShift:
    return jsNumber(v1->toInt32(exec) >> i2);
  case OpURShift:
    return jsNumber(v1->toUInt32(exec) >> i2);
  default:
    assert(!"ShiftNode: unhandled switch case");
    return jsUndefined();
  }
}

// ------------------------------ RelationalNode -------------------------------

// ECMA 11.8
JSValue *RelationalNode::evaluate(ExecState *exec)
{
  JSValue *v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  bool b;
  if (oper == OpLess || oper == OpGreaterEq) {
    int r = relation(exec, v1, v2);
    if (r < 0)
      b = false;
    else
      b = (oper == OpLess) ? (r == 1) : (r == 0);
  } else if (oper == OpGreater || oper == OpLessEq) {
    int r = relation(exec, v2, v1);
    if (r < 0)
      b = false;
    else
      b = (oper == OpGreater) ? (r == 1) : (r == 0);
  } else if (oper == OpIn) {
      // Is all of this OK for host objects?
      if (!v2->isObject())
          return throwError(exec,  TypeError,
                             "Value %s (result of expression %s) is not an object. Cannot be used with IN expression.", v2, expr2.get());
      JSObject *o2(static_cast<JSObject*>(v2));
      b = o2->hasProperty(exec, Identifier(v1->toString(exec)));
  } else {
    if (!v2->isObject())
        return throwError(exec,  TypeError,
                           "Value %s (result of expression %s) is not an object. Cannot be used with instanceof operator.", v2, expr2.get());

    JSObject *o2(static_cast<JSObject*>(v2));
    if (!o2->implementsHasInstance()) {
      // According to the spec, only some types of objects "implement" the [[HasInstance]] property.
      // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
      // property. It seems that all object have the property, but not all implement it, so in this
      // case we return false (consistent with mozilla)
      return jsBoolean(false);
      //      return throwError(exec, TypeError,
      //			"Object does not implement the [[HasInstance]] method." );
    }
    return jsBoolean(o2->hasInstance(exec, v1));
  }

  return jsBoolean(b);
}

// ------------------------------ EqualNode ------------------------------------

// ECMA 11.9
JSValue *EqualNode::evaluate(ExecState *exec)
{
  JSValue *v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  bool result;
  if (oper == OpEqEq || oper == OpNotEq) {
    // == and !=
    bool eq = equal(exec,v1, v2);
    result = oper == OpEqEq ? eq : !eq;
  } else {
    // === and !==
    bool eq = strictEqual(exec,v1, v2);
    result = oper == OpStrEq ? eq : !eq;
  }
  return jsBoolean(result);
}

// ------------------------------ BitOperNode ----------------------------------

// ECMA 11.10
JSValue *BitOperNode::evaluate(ExecState *exec)
{
  JSValue *v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  int i1 = v1->toInt32(exec);
  int i2 = v2->toInt32(exec);
  int result;
  if (oper == OpBitAnd)
    result = i1 & i2;
  else if (oper == OpBitXOr)
    result = i1 ^ i2;
  else
    result = i1 | i2;

  return jsNumber(result);
}

// ------------------------------ BinaryLogicalNode ----------------------------

// ECMA 11.11
JSValue *BinaryLogicalNode::evaluate(ExecState *exec)
{
  JSValue *v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  bool b1 = v1->toBoolean(exec);
  if ((!b1 && oper == OpAnd) || (b1 && oper == OpOr))
    return v1;

  JSValue *v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v2;
}

// ------------------------------ ConditionalNode ------------------------------

// ECMA 11.12
JSValue *ConditionalNode::evaluate(ExecState *exec)
{
  JSValue *v = logical->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  bool b = v->toBoolean(exec);

  if (b)
    v = expr1->evaluate(exec);
  else
    v = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v;
}

// ECMA 11.13

static ALWAYS_INLINE JSValue *valueForReadModifyAssignment(ExecState * exec, JSValue *v1, JSValue *v2, Operator oper)
{
  JSValue *v;
  int i1;
  int i2;
  unsigned int ui;
  switch (oper) {
  case OpMultEq:
    v = mult(exec, v1, v2, '*');
    break;
  case OpDivEq:
    v = mult(exec, v1, v2, '/');
    break;
  case OpPlusEq:
    v = add(exec, v1, v2, '+');
    break;
  case OpMinusEq:
    v = add(exec, v1, v2, '-');
    break;
  case OpLShift:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 << i2);
    break;
  case OpRShift:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 >> i2);
    break;
  case OpURShift:
    ui = v1->toUInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(ui >> i2);
    break;
  case OpAndEq:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 & i2);
    break;
  case OpXOrEq:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 ^ i2);
    break;
  case OpOrEq:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 | i2);
    break;
  case OpModEq: {
    double d1 = v1->toNumber(exec);
    double d2 = v2->toNumber(exec);
    v = jsNumber(fmod(d1, d2));
  }
    break;
  default:
    assert(0);
    v = jsUndefined();
  }
  
  return v;
}

// ------------------------------ AssignResolveNode -----------------------------------

JSValue *AssignResolveNode::evaluate(ExecState *exec)
{
  const ScopeChain& chain = exec->context().imp()->scopeChain();
  ScopeChainIterator iter = chain.begin();
  ScopeChainIterator end = chain.end();
  
  // we must always have something in the scope chain
  assert(iter != end);

  PropertySlot slot;
  JSObject *base;
  do { 
    base = *iter;
    if (base->getPropertySlot(exec, m_ident, slot))
      goto found;

    ++iter;
  } while (iter != end);

  if (m_oper != OpEqual)
    return throwUndefinedVariableError(exec, m_ident);

 found:
  JSValue *v;

  if (m_oper == OpEqual) {
    v = m_right->evaluate(exec);
  } else {
    JSValue *v1 = slot.getValue(exec, base, m_ident);
    KJS_CHECKEXCEPTIONVALUE
    JSValue *v2 = m_right->evaluate(exec);
    v = valueForReadModifyAssignment(exec, v1, v2, m_oper);
  }

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, m_ident, v);
  return v;
}

// ------------------------------ AssignDotNode -----------------------------------

JSValue *AssignDotNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSObject *base = baseValue->toObject(exec);

  JSValue *v;

  if (m_oper == OpEqual) {
    v = m_right->evaluate(exec);
  } else {
    PropertySlot slot;
    JSValue *v1 = base->getPropertySlot(exec, m_ident, slot) ? slot.getValue(exec, base, m_ident) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE
    JSValue *v2 = m_right->evaluate(exec);
    v = valueForReadModifyAssignment(exec, v1, v2, m_oper);
  }

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, m_ident, v);
  return v;
}

// ------------------------------ AssignBracketNode -----------------------------------

JSValue *AssignBracketNode::evaluate(ExecState *exec)
{
  JSValue *baseValue = m_base->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *subscript = m_subscript->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  JSObject *base = baseValue->toObject(exec);

  uint32_t propertyIndex;
  if (subscript->getUInt32(propertyIndex)) {
    JSValue *v;
    if (m_oper == OpEqual) {
      v = m_right->evaluate(exec);
    } else {
      PropertySlot slot;
      JSValue *v1 = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
      KJS_CHECKEXCEPTIONVALUE
      JSValue *v2 = m_right->evaluate(exec);
      v = valueForReadModifyAssignment(exec, v1, v2, m_oper);
    }

    KJS_CHECKEXCEPTIONVALUE

    base->put(exec, propertyIndex, v);
    return v;
  }

  Identifier propertyName(subscript->toString(exec));
  JSValue *v;

  if (m_oper == OpEqual) {
    v = m_right->evaluate(exec);
  } else {
    PropertySlot slot;
    JSValue *v1 = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
    KJS_CHECKEXCEPTIONVALUE
    JSValue *v2 = m_right->evaluate(exec);
    v = valueForReadModifyAssignment(exec, v1, v2, m_oper);
  }

  KJS_CHECKEXCEPTIONVALUE

  base->put(exec, propertyName, v);
  return v;
}

// ------------------------------ CommaNode ------------------------------------

// ECMA 11.14
JSValue *CommaNode::evaluate(ExecState *exec)
{
  expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  JSValue *v = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v;
}

// ------------------------------ StatListNode ---------------------------------

StatListNode::StatListNode(StatementNode *s)
  : statement(s), next(this)
{
    Parser::noteNodeCycle(this);
    setLoc(s->firstLine(), s->lastLine(), s->sourceId());
}
 
StatListNode::StatListNode(StatListNode *l, StatementNode *s)
  : statement(s), next(l->next)
{
  l->next = this;
  setLoc(l->firstLine(), s->lastLine(), l->sourceId());
}

// ECMA 12.1
Completion StatListNode::execute(ExecState *exec)
{
  Completion c = statement->execute(exec);
  KJS_ABORTPOINT
  if (c.complType() != Normal)
    return c;
  
  JSValue *v = c.value();
  
  for (StatListNode *n = next.get(); n; n = n->next.get()) {
    Completion c2 = n->statement->execute(exec);
    KJS_ABORTPOINT
    if (c2.complType() != Normal)
      return c2;

    if (c2.isValueCompletion())
      v = c2.value();
    c = c2;
  }

  return Completion(c.complType(), v, c.target());
}

void StatListNode::processVarDecls(ExecState *exec)
{
  for (StatListNode *n = this; n; n = n->next.get())
    n->statement->processVarDecls(exec);
}

void StatListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ AssignExprNode -------------------------------

// ECMA 12.2
JSValue *AssignExprNode::evaluate(ExecState *exec)
{
  return expr->evaluate(exec);
}

// ------------------------------ VarDeclNode ----------------------------------

    
VarDeclNode::VarDeclNode(const Identifier &id, AssignExprNode *in, Type t)
    : varType(t), ident(id), init(in)
{
}

// ECMA 12.2
JSValue *VarDeclNode::evaluate(ExecState *exec)
{
  JSObject *variable = exec->context().imp()->variableObject();

  JSValue *val;
  if (init) {
      val = init->evaluate(exec);
      KJS_CHECKEXCEPTIONVALUE
  } else {
      // already declared? - check with getDirect so you can override
      // built-in properties of the global object with var declarations.
      if (variable->getDirect(ident)) 
          return 0;
      val = jsUndefined();
  }

#ifdef KJS_VERBOSE
  printInfo(exec,(UString("new variable ")+ident.ustring()).cstring().c_str(),val);
#endif
  // We use Internal to bypass all checks in derived objects, e.g. so that
  // "var location" creates a dynamic property instead of activating window.location.
  int flags = Internal;
  if (exec->context().imp()->codeType() != EvalCode)
    flags |= DontDelete;
  if (varType == VarDeclNode::Constant)
    flags |= ReadOnly;
  variable->put(exec, ident, val, flags);

  return jsString(ident.ustring());
}

void VarDeclNode::processVarDecls(ExecState *exec)
{
  JSObject *variable = exec->context().imp()->variableObject();

  // If a variable by this name already exists, don't clobber it -
  // it might be a function parameter
  if (!variable->hasProperty(exec, ident)) {
    int flags = Internal;
    if (exec->context().imp()->codeType() != EvalCode)
      flags |= DontDelete;
    if (varType == VarDeclNode::Constant)
      flags |= ReadOnly;
    variable->put(exec, ident, jsUndefined(), flags);
  }
}

// ------------------------------ VarDeclListNode ------------------------------

// ECMA 12.2
JSValue *VarDeclListNode::evaluate(ExecState *exec)
{
  for (VarDeclListNode *n = this; n; n = n->next.get()) {
    n->var->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
  }
  return jsUndefined();
}

void VarDeclListNode::processVarDecls(ExecState *exec)
{
  for (VarDeclListNode *n = this; n; n = n->next.get())
    n->var->processVarDecls(exec);
}

void VarDeclListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ VarStatementNode -----------------------------

// ECMA 12.2
Completion VarStatementNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  (void) next->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(Normal);
}

void VarStatementNode::processVarDecls(ExecState *exec)
{
  next->processVarDecls(exec);
}

// ------------------------------ BlockNode ------------------------------------

BlockNode::BlockNode(SourceElementsNode *s)
{
  if (s) {
    source = s->next;
    Parser::removeNodeCycle(source.get());
    s->next = 0;
    setLoc(s->firstLine(), s->lastLine(), s->sourceId());
  } else {
    source = 0;
  }
}

// ECMA 12.1
Completion BlockNode::execute(ExecState *exec)
{
  if (!source)
    return Completion(Normal);

  source->processFuncDecl(exec);

  return source->execute(exec);
}

void BlockNode::processVarDecls(ExecState *exec)
{
  if (source)
    source->processVarDecls(exec);
}

// ------------------------------ EmptyStatementNode ---------------------------

// ECMA 12.3
Completion EmptyStatementNode::execute(ExecState *)
{
  return Completion(Normal);
}

// ------------------------------ ExprStatementNode ----------------------------

// ECMA 12.4
Completion ExprStatementNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(Normal, v);
}

// ------------------------------ IfNode ---------------------------------------

// ECMA 12.5
Completion IfNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION
  bool b = v->toBoolean(exec);

  // if ... then
  if (b)
    return statement1->execute(exec);

  // no else
  if (!statement2)
    return Completion(Normal);

  // else
  return statement2->execute(exec);
}

void IfNode::processVarDecls(ExecState *exec)
{
  statement1->processVarDecls(exec);

  if (statement2)
    statement2->processVarDecls(exec);
}

// ------------------------------ DoWhileNode ----------------------------------

// ECMA 12.6.1
Completion DoWhileNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *bv;
  Completion c;

  do {
    // bail out on error
    KJS_CHECKEXCEPTION

    exec->context().imp()->seenLabels()->pushIteration();
    c = statement->execute(exec);
    exec->context().imp()->seenLabels()->popIteration();
    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        return Completion(Normal, 0);
      if (c.complType() != Normal)
        return c;
    }
    bv = expr->evaluate(exec);
    KJS_CHECKEXCEPTION
  } while (bv->toBoolean(exec));

  return Completion(Normal, 0);
}

void DoWhileNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ WhileNode ------------------------------------

// ECMA 12.6.2
Completion WhileNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *bv;
  Completion c;
  bool b(false);
  JSValue *value = 0;

  while (1) {
    bv = expr->evaluate(exec);
    KJS_CHECKEXCEPTION
    b = bv->toBoolean(exec);

    // bail out on error
    KJS_CHECKEXCEPTION

    if (!b)
      return Completion(Normal, value);

    exec->context().imp()->seenLabels()->pushIteration();
    c = statement->execute(exec);
    exec->context().imp()->seenLabels()->popIteration();
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

void WhileNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ ForNode --------------------------------------

// ECMA 12.6.3
Completion ForNode::execute(ExecState *exec)
{
  JSValue *v, *cval = 0;

  if (expr1) {
    v = expr1->evaluate(exec);
    KJS_CHECKEXCEPTION
  }
  while (1) {
    if (expr2) {
      v = expr2->evaluate(exec);
      KJS_CHECKEXCEPTION
      if (!v->toBoolean(exec))
	return Completion(Normal, cval);
    }
    // bail out on error
    KJS_CHECKEXCEPTION

    exec->context().imp()->seenLabels()->pushIteration();
    Completion c = statement->execute(exec);
    exec->context().imp()->seenLabels()->popIteration();
    if (c.isValueCompletion())
      cval = c.value();
    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        return Completion(Normal, cval);
      if (c.complType() != Normal)
      return c;
    }
    if (expr3) {
      v = expr3->evaluate(exec);
      KJS_CHECKEXCEPTION
    }
  }
  
  return Completion(); // work around gcc 4.0 bug
}

void ForNode::processVarDecls(ExecState *exec)
{
  if (expr1)
    expr1->processVarDecls(exec);

  statement->processVarDecls(exec);
}

// ------------------------------ ForInNode ------------------------------------

ForInNode::ForInNode(Node *l, Node *e, StatementNode *s)
  : init(0L), lexpr(l), expr(e), varDecl(0L), statement(s)
{
}

ForInNode::ForInNode(const Identifier &i, AssignExprNode *in, Node *e, StatementNode *s)
  : ident(i), init(in), expr(e), statement(s)
{
  // for( var foo = bar in baz )
  varDecl = new VarDeclNode(ident, init.get(), VarDeclNode::Variable);
  lexpr = new ResolveNode(ident);
}

// ECMA 12.6.4
Completion ForInNode::execute(ExecState *exec)
{
  JSValue *e;
  JSValue *retval = 0;
  JSObject *v;
  Completion c;
  ReferenceList propList;

  if (varDecl) {
    varDecl->evaluate(exec);
    KJS_CHECKEXCEPTION
  }

  e = expr->evaluate(exec);

  // for Null and Undefined, we want to make sure not to go through
  // the loop at all, because their object wrappers will have a
  // property list but will throw an exception if you attempt to
  // access any property.
  if (e->isUndefinedOrNull()) {
    return Completion(Normal, 0);
  }

  KJS_CHECKEXCEPTION
  v = e->toObject(exec);
  propList = v->propList(exec);

  ReferenceListIterator propIt = propList.begin();

  while (propIt != propList.end()) {
    Identifier name = propIt->getPropertyName(exec);
    if (!v->hasProperty(exec, name)) {
      propIt++;
      continue;
    }

    JSValue *str = jsString(name.ustring());

    if (lexpr->isResolveNode()) {
        const Identifier &ident = static_cast<ResolveNode *>(lexpr.get())->identifier();

        const ScopeChain& chain = exec->context().imp()->scopeChain();
        ScopeChainIterator iter = chain.begin();
        ScopeChainIterator end = chain.end();
  
        // we must always have something in the scope chain
        assert(iter != end);

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
        assert(lexpr->isBracketAccessorNode());
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

    exec->context().imp()->seenLabels()->pushIteration();
    c = statement->execute(exec);
    exec->context().imp()->seenLabels()->popIteration();
    if (c.isValueCompletion())
      retval = c.value();

    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        break;
      if (c.complType() != Normal) {
        return c;
      }
    }

    propIt++;
  }

  // bail out on error
  KJS_CHECKEXCEPTION

  return Completion(Normal, retval);
}

void ForInNode::processVarDecls(ExecState *exec)
{
  if (varDecl)
    varDecl->processVarDecls(exec);
  statement->processVarDecls(exec);
}

// ------------------------------ ContinueNode ---------------------------------

// ECMA 12.7
Completion ContinueNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  if (ident.isEmpty() && !exec->context().imp()->seenLabels()->inIteration())
    return createErrorCompletion(exec, SyntaxError, "Invalid continue statement.");
  else if (!ident.isEmpty() && !exec->context().imp()->seenLabels()->contains(ident))
    return createErrorCompletion(exec, SyntaxError, "Label %s not found.", ident);
  else
    return Completion(Continue, 0, ident);
}

// ------------------------------ BreakNode ------------------------------------

// ECMA 12.8
Completion BreakNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  if (ident.isEmpty() && !exec->context().imp()->seenLabels()->inIteration() &&
      !exec->context().imp()->seenLabels()->inSwitch())
    return createErrorCompletion(exec, SyntaxError, "Invalid break statement.");
  else if (!ident.isEmpty() && !exec->context().imp()->seenLabels()->contains(ident))
    return createErrorCompletion(exec, SyntaxError, "Label %s not found.");
  else
    return Completion(Break, 0, ident);
}

// ------------------------------ ReturnNode -----------------------------------

// ECMA 12.9
Completion ReturnNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  CodeType codeType = exec->context().imp()->codeType();
  if (codeType != FunctionCode && codeType != AnonymousCode ) {
    return createErrorCompletion(exec, SyntaxError, "Invalid return statement.");
  }

  if (!value)
    return Completion(ReturnValue, jsUndefined());

  JSValue *v = value->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(ReturnValue, v);
}

// ------------------------------ WithNode -------------------------------------

// ECMA 12.10
Completion WithNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION
  JSObject *o = v->toObject(exec);
  KJS_CHECKEXCEPTION
  exec->context().imp()->pushScope(o);
  Completion res = statement->execute(exec);
  exec->context().imp()->popScope();

  return res;
}

void WithNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ CaseClauseNode -------------------------------

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
  if (next)
    return next->execute(exec);
  else
    return Completion(Normal, jsUndefined());
}

void CaseClauseNode::processVarDecls(ExecState *exec)
{
  if (next)
    next->processVarDecls(exec);
}

// ------------------------------ ClauseListNode -------------------------------

JSValue *ClauseListNode::evaluate(ExecState *)
{
  // should never be called
  assert(false);
  return 0;
}

// ECMA 12.11
void ClauseListNode::processVarDecls(ExecState *exec)
{
  for (ClauseListNode *n = this; n; n = n->next.get())
    if (n->clause)
      n->clause->processVarDecls(exec);
}

void ClauseListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ CaseBlockNode --------------------------------

CaseBlockNode::CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d,
                             ClauseListNode *l2)
{
  if (l1) {
    list1 = l1->next;
    Parser::removeNodeCycle(list1.get());
    l1->next = 0;
  } else {
    list1 = 0;
  }

  def = d;

  if (l2) {
    list2 = l2->next;
    Parser::removeNodeCycle(list2.get());
    l2->next = 0;
  } else {
    list2 = 0;
  }
}
 
JSValue *CaseBlockNode::evaluate(ExecState *)
{
  // should never be called
  assert(false);
  return 0;
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

void CaseBlockNode::processVarDecls(ExecState *exec)
{
  if (list1)
    list1->processVarDecls(exec);
  if (def)
    def->processVarDecls(exec);
  if (list2)
    list2->processVarDecls(exec);
}

// ------------------------------ SwitchNode -----------------------------------

// ECMA 12.11
Completion SwitchNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  exec->context().imp()->seenLabels()->pushSwitch();
  Completion res = block->evalBlock(exec,v);
  exec->context().imp()->seenLabels()->popSwitch();

  if ((res.complType() == Break) && ls.contains(res.target()))
    return Completion(Normal, res.value());
  return res;
}

void SwitchNode::processVarDecls(ExecState *exec)
{
  block->processVarDecls(exec);
}

// ------------------------------ LabelNode ------------------------------------

// ECMA 12.12
Completion LabelNode::execute(ExecState *exec)
{
  if (!exec->context().imp()->seenLabels()->push(label))
    return createErrorCompletion(exec, SyntaxError, "Duplicated label %s found.", label);
  Completion e = statement->execute(exec);
  exec->context().imp()->seenLabels()->pop();

  if ((e.complType() == Break) && (e.target() == label))
    return Completion(Normal, e.value());
  return e;
}

void LabelNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ ThrowNode ------------------------------------

// ECMA 12.13
Completion ThrowNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  JSValue *v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(Throw, v);
}

// ------------------------------ TryNode --------------------------------------

// ECMA 12.14
Completion TryNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Completion c = tryBlock->execute(exec);

  if (catchBlock && c.complType() == Throw) {
    JSObject *obj = new JSObject;
    obj->put(exec, exceptionIdent, c.value(), DontDelete);
    exec->context().imp()->pushScope(obj);
    c = catchBlock->execute(exec);
    exec->context().imp()->popScope();
  }

  if (finallyBlock) {
    Completion c2 = finallyBlock->execute(exec);
    if (c2.complType() != Normal)
      c = c2;
  }

  return c;
}

void TryNode::processVarDecls(ExecState *exec)
{
  tryBlock->processVarDecls(exec);
  if (catchBlock)
    catchBlock->processVarDecls(exec);
  if (finallyBlock)
    finallyBlock->processVarDecls(exec);
}

// ------------------------------ ParameterNode --------------------------------

// ECMA 13
JSValue *ParameterNode::evaluate(ExecState *)
{
  return jsUndefined();
}

void ParameterNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionBodyNode::FunctionBodyNode(SourceElementsNode *s)
  : BlockNode(s)
{
  setLoc(-1, -1, -1);
}

void FunctionBodyNode::processFuncDecl(ExecState *exec)
{
  if (source)
    source->processFuncDecl(exec);
}

// ------------------------------ FuncDeclNode ---------------------------------

// ECMA 13
void FuncDeclNode::processFuncDecl(ExecState *exec)
{
  ContextImp *context = exec->context().imp();

  // TODO: let this be an object with [[Class]] property "Function"
  FunctionImp *func = new DeclaredFunctionImp(exec, ident, body.get(), context->scopeChain());

  JSObject *proto = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
  proto->put(exec, constructorPropertyName, func, ReadOnly|DontDelete|DontEnum);
  func->put(exec, prototypePropertyName, proto, Internal|DontDelete);

  int plen = 0;
  for(ParameterNode *p = param.get(); p != 0L; p = p->nextParam(), plen++)
    func->addParameter(p->ident());

  func->put(exec, lengthPropertyName, jsNumber(plen), ReadOnly|DontDelete|DontEnum);

  // ECMA 10.2.2
  context->variableObject()->put(exec, ident, func, Internal | (context->codeType() == EvalCode ? 0 : DontDelete));

  if (body) {
    // hack the scope so that the function gets put as a property of func, and it's scope
    // contains the func as well as our current scope
    JSObject *oldVar = context->variableObject();
    context->setVariableObject(func);
    context->pushScope(func);
    body->processFuncDecl(exec);
    context->popScope();
    context->setVariableObject(oldVar);
  }
}

Completion FuncDeclNode::execute(ExecState *)
{
    return Completion(Normal);
}

// ------------------------------ FuncExprNode ---------------------------------

// ECMA 13
JSValue *FuncExprNode::evaluate(ExecState *exec)
{
  ContextImp *context = exec->context().imp();
  bool named = !ident.isNull();
  JSObject *functionScopeObject = 0;

  if (named) {
    // named FunctionExpressions can recursively call themselves,
    // but they won't register with the current scope chain and should
    // be contained as single property in an anonymous object.
    functionScopeObject = new JSObject;
    context->pushScope(functionScopeObject);
  }

  FunctionImp *func = new DeclaredFunctionImp(exec, ident, body.get(), context->scopeChain());
  JSObject *proto = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
  proto->put(exec, constructorPropertyName, func, ReadOnly|DontDelete|DontEnum);
  func->put(exec, prototypePropertyName, proto, Internal|DontDelete);

  int plen = 0;
  for(ParameterNode *p = param.get(); p != 0L; p = p->nextParam(), plen++)
    func->addParameter(p->ident());

  if (named) {
    functionScopeObject->put(exec, ident, func, Internal | ReadOnly | (context->codeType() == EvalCode ? 0 : DontDelete));
    context->popScope();
  }

  return func;
}

// ------------------------------ SourceElementsNode ---------------------------

int SourceElementsNode::count = 0;

SourceElementsNode::SourceElementsNode(StatementNode *s1)
  : node(s1), next(this)
{
    Parser::noteNodeCycle(this);
    setLoc(s1->firstLine(), s1->lastLine(), s1->sourceId());
}

SourceElementsNode::SourceElementsNode(SourceElementsNode *s1, StatementNode *s2)
  : node(s2), next(s1->next)
{
  s1->next = this;
  setLoc(s1->firstLine(), s2->lastLine(), s1->sourceId());
}

// ECMA 14
Completion SourceElementsNode::execute(ExecState *exec)
{
  KJS_CHECKEXCEPTION

  Completion c1 = node->execute(exec);
  KJS_CHECKEXCEPTION;
  if (c1.complType() != Normal)
    return c1;
  
  for (SourceElementsNode *n = next.get(); n; n = n->next.get()) {
    Completion c2 = n->node->execute(exec);
    if (c2.complType() != Normal)
      return c2;
    // The spec says to return c2 here, but it seems that mozilla returns c1 if
    // c2 doesn't have a value
    if (c2.value())
      c1 = c2;
  }
  
  return c1;
}

// ECMA 14
void SourceElementsNode::processFuncDecl(ExecState *exec)
{
  for (SourceElementsNode *n = this; n; n = n->next.get())
    n->node->processFuncDecl(exec);
}

void SourceElementsNode::processVarDecls(ExecState *exec)
{
  for (SourceElementsNode *n = this; n; n = n->next.get())
    n->node->processVarDecls(exec);
}

void SourceElementsNode::breakCycle() 
{ 
    next = 0;
}

ProgramNode::ProgramNode(SourceElementsNode *s) : FunctionBodyNode(s)
{
}
