/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

#include "nodes.h"

#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "kjs.h"
#include "ustring.h"
#include "lexer.h"
#include "types.h"
#include "internal.h"
#include "operations.h"
#include "regexp_object.h"
#include "debugger.h"

using namespace KJS;

#ifdef KJS_DEBUGGER
#define KJS_BREAKPOINT if (!hitStatement()) return Completion(Normal);
#define KJS_ABORTPOINT if (abortStatement()) return Completion(Normal);
#else
#define KJS_BREAKPOINT
#define KJS_ABORTPOINT
#endif

int   Node::nodeCount = 0;
Node* Node::first = 0;

Node::Node()
{
  assert(Lexer::curr());
  line = Lexer::curr()->lineNo();
  nodeCount++;
  //  cout << "Node()" << endl;

  // create a list of allocated objects. Makes
  // deleting (even after a parse error) quite easy
  next = first;
  prev = 0L;
  if (first)
    first->prev = this;
  first = this;
}

Node::~Node()
{
  //  cout << "~Node()" << endl;
  if (next)
    next->prev = prev;
  if (prev)
    prev->next = next;
  nodeCount--;
}

void Node::deleteAllNodes()
{
  Node *tmp, *n = first;

  while ((tmp = n)) {
    n = n->next;
    delete tmp;
  }
  first = 0L;
  //  assert(nodeCount == 0);
}

KJSO Node::throwError(ErrorType e, const char *msg)
{
  return Error::create(e, msg, lineNo());
}

#ifdef KJS_DEBUGGER
void StatementNode::setLoc(int line0, int line1)
{
    l0 = line0;
    l1 = line1;
    sid = KJScriptImp::current()->sourceId();
}

bool StatementNode::hitStatement()
{
  if (KJScriptImp::current()->debugger())
    return KJScriptImp::current()->debugger()->hit(firstLine(), breakPoint);
  else
    return true;
}

// return true if the debugger wants us to stop at this point
bool StatementNode::abortStatement()
{
  if (KJScriptImp::current()->debugger() &&
      KJScriptImp::current()->debugger()->mode() == Debugger::Stop)
      return true;

  return false;
}

bool Node::setBreakpoint(Node *firstNode, int id, int line, bool set)
{
  while (firstNode) {
    if (firstNode->setBreakpoint(id, line, set) && line >= 0) // line<0 for all
      return true;
    firstNode = firstNode->next;
  }
  return false;
}

/**
 * Try to set or delete a breakpoint depending on the value of set.
 * The call will return true if successful, i.e. if line is inside
 * of the statement's range. Additionally, a breakpoint had to
 * be set already if you tried to delete with set=false.
 */
bool StatementNode::setBreakpoint(int id, int line, bool set)
{
  // in our source unit and line range ?
  if (id != sid || ((line < l0 || line > l1) && line >= 0))
    return false;

  if (!set && !breakPoint)
    return false;

  breakPoint = set;
  return true;
}
#endif

KJSO NullNode::evaluate()
{
  return Null();
}

KJSO BooleanNode::evaluate()
{
  return Boolean(value);
}

KJSO NumberNode::evaluate()
{
  return Number(value);
}

KJSO StringNode::evaluate()
{
  return String(value);
}

KJSO RegExpNode::evaluate()
{
  List list;
  String p(pattern);
  String f(flags);
  list.append(p);
  list.append(f);

  // very ugly
  KJSO r = Global::current().get("RegExp");
  RegExpObject *r2 = (RegExpObject*)r.imp();
  return r2->construct(list);
}

// ECMA 11.1.1
KJSO ThisNode::evaluate()
{
  return Context::current()->thisValue();
}

// ECMA 11.1.2 & 10.1.4
KJSO ResolveNode::evaluate()
{
  assert(Context::current());
  const List *chain = Context::current()->pScopeChain();
  assert(chain);
  ListIterator scope = chain->begin();

  while (scope != chain->end()) {
    if (scope->hasProperty(ident)) {
//        cout << "Resolve: found '" << ident.ascii() << "'"
// 	    << " type " << scope->get(ident).imp()->typeInfo()->name << endl;
      return Reference(*scope, ident);
    }
    scope++;
  }

  // identifier not found
//  cout << "Resolve: didn't find '" << ident.ascii() << "'" << endl;
  return Reference(Null(), ident);
}

// ECMA 11.1.4
KJSO ArrayNode::evaluate()
{
  KJSO array;
  int length;
  int elisionLen = elision ? elision->evaluate().toInt32() : 0;

  if (element) {
    array = element->evaluate();
    length = opt ? array.get("length").toInt32() : 0;
  } else {
    array = Object::create(ArrayClass);
    length = 0;
  }

  if (opt)
    array.put("length", Number(elisionLen + length), DontEnum | DontDelete);

  return array;
}

// ECMA 11.1.4
KJSO ElementNode::evaluate()
{
  KJSO array, val;
  int length = 0;
  int elisionLen = elision ? elision->evaluate().toInt32() : 0;

  if (list) {
    array = list->evaluate();
    val = node->evaluate().getValue();
    length = array.get("length").toInt32();
  } else {
    array = Object::create(ArrayClass);
    val = node->evaluate().getValue();
  }

  array.putArrayElement(UString::from(elisionLen + length), val);

  return array;
}

// ECMA 11.1.4
KJSO ElisionNode::evaluate()
{
  if (elision)
    return Number(elision->evaluate().toNumber().value() + 1);
  else
    return Number(1);
}

// ECMA 11.1.5
KJSO ObjectLiteralNode::evaluate()
{
  if (list)
    return list->evaluate();

  return Object::create(ObjectClass);
}

// ECMA 11.1.5
KJSO PropertyValueNode::evaluate()
{
  KJSO obj;
  if (list)
    obj = list->evaluate();
  else
    obj = Object::create(ObjectClass);
  KJSO n = name->evaluate();
  KJSO a = assign->evaluate();
  KJSO v = a.getValue();

  obj.put(n.toString().value(), v);

  return obj;
}

// ECMA 11.1.5
KJSO PropertyNode::evaluate()
{
  KJSO s;

  if (str.isNull()) {
    s = String(UString::from(numeric));
  } else
    s = String(str);

  return s;
}

// ECMA 11.1.6
KJSO GroupNode::evaluate()
{
  return group->evaluate();
}

// ECMA 11.2.1a
KJSO AccessorNode1::evaluate()
{
  KJSO e1 = expr1->evaluate();
  KJSO v1 = e1.getValue();
  KJSO e2 = expr2->evaluate();
  KJSO v2 = e2.getValue();
  Object o = v1.toObject();
  String s = v2.toString();
  return Reference(o, s.value());
}

// ECMA 11.2.1b
KJSO AccessorNode2::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  KJSO o = v.toObject();
  return Reference(o, ident);
}

// ECMA 11.2.2
KJSO NewExprNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();

  List *argList = args ? args->evaluateList() : 0;

  if (!v.isObject()) {
    delete argList;
    return throwError(TypeError, "Expression is no object. Cannot be new'ed");
  }

  Constructor constr = Constructor::dynamicCast(v);
  if (constr.isNull()) {
    delete argList;
    return throwError(TypeError, "Expression is no constructor.");
  }

  if (!argList)
    argList = new List;

  KJSO res = constr.construct(*argList);

  delete argList;

  return res;
}

// ECMA 11.2.3
KJSO FunctionCallNode::evaluate()
{
  KJSO e = expr->evaluate();

  List *argList = args->evaluateList();

  KJSO v = e.getValue();

  if (!v.isObject()) {
#ifndef NDEBUG
    printInfo("Failed function call attempt on", e);
#endif
    delete argList;
    return throwError(TypeError, "Expression is no object. Cannot be called.");
  }

  if (!v.implementsCall()) {
#ifndef NDEBUG
    printInfo("Failed function call attempt on", e);
#endif
    delete argList;
    return throwError(TypeError, "Expression does not allow calls.");
  }

  KJSO o;
  if (e.isA(ReferenceType))
    o = e.getBase();
  else
    o = Null();

  if (o.isA(ActivationType))
    o = Null();

#ifdef KJS_DEBUGGER
  steppingInto(true);
#endif

  KJSO result = v.executeCall(o, argList);

#ifdef KJS_DEBUGGER
  steppingInto(false);
#endif

  delete argList;

  return result;
}

#ifdef KJS_DEBUGGER
void FunctionCallNode::steppingInto(bool in)
{
  Debugger *dbg = KJScriptImp::current()->debugger();
  if (!dbg)
    return;
  if (in) {
    // before entering function. Don't step inside if 'Next' is chosen.
    previousMode = dbg->mode();
    if (previousMode == Debugger::Next)
      dbg->setMode(Debugger::Continue);
  } else {
    // restore mode after leaving function
    dbg->setMode(previousMode);
  }
}
#endif

KJSO ArgumentsNode::evaluate()
{
  assert(0);
  return KJSO(); // dummy, see evaluateList()
}

// ECMA 11.2.4
List* ArgumentsNode::evaluateList()
{
  if (!list)
    return new List();

  return list->evaluateList();
}

KJSO ArgumentListNode::evaluate()
{
  assert(0);
  return KJSO(); // dummy, see evaluateList()
}

// ECMA 11.2.4
List* ArgumentListNode::evaluateList()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();

  if (!list) {
    List *l = new List();
    l->append(v);
    return l;
  }

  List *l = list->evaluateList();
  l->append(v);

  return l;
}

// ECMA 11.8
KJSO RelationalNode::evaluate()
{
  KJSO e1 = expr1->evaluate();
  KJSO v1 = e1.getValue();
  KJSO e2 = expr2->evaluate();
  KJSO v2 = e2.getValue();

  bool b;
  if (oper == OpLess || oper == OpGreaterEq) {
    int r = relation(v1, v2);
    if (r < 0)
      b = false;
    else
      b = (oper == OpLess) ? (r == 1) : (r == 0);
  } else if (oper == OpGreater || oper == OpLessEq) {
    int r = relation(v2, v1);
    if (r < 0)
      b = false;
    else
      b = (oper == OpGreater) ? (r == 1) : (r == 0);
  } else if (oper == OpIn) {
      /* Is all of this OK for host objects? */
      if (!v2.isObject())
          return throwError( TypeError,
                             "Shift expression not an object into IN expression." );
      b = v2.hasProperty(v1.toString().value());
  } else {
    /* TODO: should apply to Function _objects_ only */
    if (!v2.derivedFrom("Function"))
      return throwError(TypeError,
			"Called instanceof operator on non-function object." );
    return hasInstance(v2, v1);	/* TODO: make object member function */
  }

  return Boolean(b);
}

// ECMA 11.9
KJSO EqualNode::evaluate()
{
  KJSO e1 = expr1->evaluate();
  KJSO e2 = expr2->evaluate();
  KJSO v1 = e1.getValue();
  KJSO v2 = e2.getValue();

  bool result;
  if (oper == OpEqEq || oper == OpNotEq) {
    // == and !=
    bool eq = equal(v1, v2);
    result = oper == OpEqEq ? eq : !eq;
  } else {
    // === and !==
    bool eq = strictEqual(v1, v2);
    result = oper == OpStrEq ? eq : !eq;
  }
  return Boolean(result);
}

// ECMA 11.10
KJSO BitOperNode::evaluate()
{
  KJSO e1 = expr1->evaluate();
  KJSO v1 = e1.getValue();
  KJSO e2 = expr2->evaluate();
  KJSO v2 = e2.getValue();
  int i1 = v1.toInt32();
  int i2 = v2.toInt32();
  int result;
  if (oper == OpBitAnd)
    result = i1 & i2;
  else if (oper == OpBitXOr)
    result = i1 ^ i2;
  else
    result = i1 | i2;

  return Number(result);
}

// ECMA 11.11
KJSO BinaryLogicalNode::evaluate()
{
  KJSO e1 = expr1->evaluate();
  KJSO v1 = e1.getValue();
  Boolean b1 = v1.toBoolean();
  if ((!b1.value() && oper == OpAnd) || (b1.value() && oper == OpOr))
    return v1;

  KJSO e2 = expr2->evaluate();
  KJSO v2 = e2.getValue();

  return v2;
}

// ECMA 11.12
KJSO ConditionalNode::evaluate()
{
  KJSO e = logical->evaluate();
  KJSO v = e.getValue();
  Boolean b = v.toBoolean();

  if (b.value())
    e = expr1->evaluate();
  else
    e = expr2->evaluate();

  return e.getValue();
}

// ECMA 11.13
KJSO AssignNode::evaluate()
{
  KJSO l, e, v;
  ErrorType err;
  if (oper == OpEqual) {
    l = left->evaluate();
    e = expr->evaluate();
    v = e.getValue();
  } else {
    l = left->evaluate();
    KJSO v1 = l.getValue();
    e = expr->evaluate();
    KJSO v2 = e.getValue();
    int i1 = v1.toInt32();
    int i2 = v2.toInt32();
    switch (oper) {
    case OpMultEq:
      v = mult(v1, v2, '*');
      break;
    case OpDivEq:
      v = mult(v1, v2, '/');
      break;
    case OpPlusEq:
      v = add(v1, v2, '+');
      break;
    case OpMinusEq:
      v = add(v1, v2, '-');
      break;
    case OpLShift:
      v = Number(i1 <<= i2);
      break;
    case OpRShift:
      v = Number(i1 >>= i2);
      break;
    case OpURShift:
      i1 = v1.toUInt32();
      v = Number(i1 >>= i2);
      break;
    case OpAndEq:
      v = Number(i1 &= i2);
      break;
    case OpXOrEq:
      v = Number(i1 ^= i2);
      break;
    case OpOrEq:
      v = Number(i1 |= i2);
      break;
    case OpModEq:
      v = Number(i1 %= i2);
      break;
    default:
      v = Undefined();
    }
    err = l.putValue(v);
  };

  err = l.putValue(v);
  if (err == NoError)
    return v;
  else
    return throwError(err, "Invalid reference.");
}

// ECMA 11.3
KJSO PostfixNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  Number n = v.toNumber();

  double newValue = (oper == OpPlusPlus) ? n.value() + 1 : n.value() - 1;
  KJSO n2 = Number(newValue);

  e.putValue(n2);

  return n;
}

// ECMA 11.4.1
KJSO DeleteNode::evaluate()
{
  KJSO e = expr->evaluate();
  if (!e.isA(ReferenceType))
    return Boolean(true);
  KJSO b = e.getBase();
  UString n = e.getPropertyName();
  bool ret = b.deleteProperty(n);

  return Boolean(ret);
}

// ECMA 11.4.2
KJSO VoidNode::evaluate()
{
  KJSO dummy1 = expr->evaluate();
  KJSO dummy2 = dummy1.getValue();

  return Undefined();
}

// ECMA 11.4.3
KJSO TypeOfNode::evaluate()
{
  const char *s = 0L;
  KJSO e = expr->evaluate();
  if (e.isA(ReferenceType)) {
    KJSO b = e.getBase();
    if (b.isA(NullType))
      return String("undefined");
  }
  KJSO v = e.getValue();
  switch (v.type())
    {
    case UndefinedType:
      s = "undefined";
      break;
    case NullType:
      s = "object";
      break;
    case BooleanType:
      s = "boolean";
      break;
    case NumberType:
      s = "number";
      break;
    case StringType:
      s = "string";
      break;
    default:
      if (v.implementsCall())
	s = "function";
      else
	s = "object";
      break;
    }

  return String(s);
}

// ECMA 11.4.4 and 11.4.5
KJSO PrefixNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  Number n = v.toNumber();

  double newValue = (oper == OpPlusPlus) ? n.value() + 1 : n.value() - 1;
  KJSO n2 = Number(newValue);

  e.putValue(n2);

  return n2;
}

// ECMA 11.4.6
KJSO UnaryPlusNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();

  return v.toNumber();
}

// ECMA 11.4.7
KJSO NegateNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  Number n = v.toNumber();

  double d = -n.value();

  return Number(d);
}

// ECMA 11.4.8
KJSO BitwiseNotNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  int i32 = v.toInt32();

  return Number(~i32);
}

// ECMA 11.4.9
KJSO LogicalNotNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  Boolean b = v.toBoolean();

  return Boolean(!b.value());
}

// ECMA 11.5
KJSO MultNode::evaluate()
{
  KJSO t1 = term1->evaluate();
  KJSO v1 = t1.getValue();

  KJSO t2 = term2->evaluate();
  KJSO v2 = t2.getValue();

  return mult(v1, v2, oper);
}

// ECMA 11.7
KJSO ShiftNode::evaluate()
{
  KJSO t1 = term1->evaluate();
  KJSO v1 = t1.getValue();
  KJSO t2 = term2->evaluate();
  KJSO v2 = t2.getValue();
  unsigned int i2 = v2.toUInt32();
  i2 &= 0x1f;

  long result;
  switch (oper) {
  case OpLShift:
    result = v1.toInt32() << i2;
    break;
  case OpRShift:
    result = v1.toInt32() >> i2;
    break;
  case OpURShift:
    result = v1.toUInt32() >> i2;
    break;
  default:
    assert(!"ShiftNode: unhandled switch case");
    result = 0L;
  }

  return Number(static_cast<double>(result));
}

// ECMA 11.6
KJSO AddNode::evaluate()
{
  KJSO t1 = term1->evaluate();
  KJSO v1 = t1.getValue();

  KJSO t2 = term2->evaluate();
  KJSO v2 = t2.getValue();

  return add(v1, v2, oper);
}

// ECMA 11.14
KJSO CommaNode::evaluate()
{
  KJSO e = expr1->evaluate();
  KJSO dummy = e.getValue(); // ignore return value
  e = expr2->evaluate();

  return e.getValue();
}

// ECMA 12.1
Completion BlockNode::execute()
{
  if (!statlist)
    return Completion(Normal);

  return statlist->execute();
}

// ECMA 12.1
Completion StatListNode::execute()
{
  if (!list) {
    Completion c = statement->execute();
    KJS_ABORTPOINT
    if (KJScriptImp::hadException()) {
      KJSO ex = KJScriptImp::exception();
      KJScriptImp::clearException();
      return Completion(Throw, ex);
    } else
      return c;
  }

  Completion l = list->execute();
  KJS_ABORTPOINT
  if (l.complType() != Normal)
    return l;
  Completion e = statement->execute();
  KJS_ABORTPOINT;
  if (KJScriptImp::hadException()) {
    KJSO ex = KJScriptImp::exception();
    KJScriptImp::clearException();
    return Completion(Throw, ex);
  }

  KJSO v = e.isValueCompletion() ? e.value() : l.value();

  return Completion(e.complType(), v, e.target() );
}

// ECMA 12.2
Completion VarStatementNode::execute()
{
  KJS_BREAKPOINT;

  (void) list->evaluate(); // returns 0L

  return Completion(Normal);
}

// ECMA 12.2
KJSO VarDeclNode::evaluate()
{
  KJSO variable = Context::current()->variableObject();

  KJSO val, tmp;
  if (init) {
      tmp = init->evaluate();
      val = tmp.getValue();
  } else {
      if ( variable.hasProperty( ident ) ) // already declared ?
          return KJSO();
      val = Undefined();
  }
  variable.put(ident, val, DontDelete);

  // spec wants to return ident. But what for ? Will be ignored above.
  return KJSO();
}

// ECMA 12.2
KJSO VarDeclListNode::evaluate()
{
  if (list)
    (void) list->evaluate();

  (void) var->evaluate();

  return KJSO();
}

// ECMA 12.2
KJSO AssignExprNode::evaluate()
{
  return expr->evaluate();
}

// ECMA 12.3
Completion EmptyStatementNode::execute()
{
  return Completion(Normal);
}

// ECMA 12.6.3
Completion ForNode::execute()
{
  KJSO e, v, cval;
  Boolean b;

  if (expr1) {
    e = expr1->evaluate();
    v = e.getValue();
  }
  while (1) {
    if (expr2) {
      e = expr2->evaluate();
      v = e.getValue();
      b = v.toBoolean();
      if (b.value() == false)
	return Completion(Normal, cval);
    }
    // bail out on error
    if (KJScriptImp::hadException())
      return Completion(Throw, KJScriptImp::exception());

    Completion c = stat->execute();
    if (c.isValueCompletion())
      cval = c.value();
    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        return Completion(Normal, cval);
      if (c.complType() != Normal)
      return c;
    }
    if (expr3) {
      e = expr3->evaluate();
      v = e.getValue();
    }
  }
}

// ECMA 12.6.4
Completion ForInNode::execute()
{
  KJSO e, v, retval;
  Completion c;
  VarDeclNode *vd = 0;
  const PropList *lst, *curr;

  // This should be done in the constructor
  if (!lexpr) { // for( var foo = bar in baz )
    vd = new VarDeclNode(&ident, init);
    vd->evaluate();

    lexpr = new ResolveNode(&ident);
  }

  e = expr->evaluate();
  v = e.getValue().toObject();
  curr = lst = v.imp()->propList();

  while (curr) {
    if (!v.hasProperty(curr->name)) {
      curr = curr->next;
      continue;
    }

    e = lexpr->evaluate();
    e.putValue(String(curr->name));

    c = stat->execute();
    if (c.isValueCompletion())
      retval = c.value();

    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        break;
      if (c.complType() != Normal) {
        delete lst;
        return c;
      }
    }

    curr = curr->next;
  }

  delete lst;
  return Completion(Normal, retval);
}

// ECMA 12.4
Completion ExprStatementNode::execute()
{
  KJS_BREAKPOINT;

  KJSO e = expr->evaluate();
  KJSO v = e.getValue();

  return Completion(Normal, v);
}

// ECMA 12.5
Completion IfNode::execute()
{
  KJS_BREAKPOINT;

  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  Boolean b = v.toBoolean();

  // if ... then
  if (b.value())
    return statement1->execute();

  // no else
  if (!statement2)
    return Completion(Normal);

  // else
  return statement2->execute();
}

// ECMA 12.6.1
Completion DoWhileNode::execute()
{
  KJS_BREAKPOINT;

  KJSO be, bv;
  Completion c;
  KJSO value;

  do {
    // bail out on error
    if (KJScriptImp::hadException())
      return Completion(Throw, KJScriptImp::exception());

    c = statement->execute();
    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        return Completion(Normal, value);
      if (c.complType() != Normal)
        return c;
    }
    be = expr->evaluate();
    bv = be.getValue();
  } while (bv.toBoolean().value());

  return Completion(Normal, value);
}

// ECMA 12.6.2
Completion WhileNode::execute()
{
  KJS_BREAKPOINT;

  KJSO be, bv;
  Completion c;
  Boolean b(false);
  KJSO value;

  while (1) {
    be = expr->evaluate();
    bv = be.getValue();
    b = bv.toBoolean();

    // bail out on error
    if (KJScriptImp::hadException())
      return Completion(Throw, KJScriptImp::exception());

    if (!b.value())
      return Completion(Normal, value);

    c = statement->execute();
    if (c.isValueCompletion())
      value = c.value();

    if ((c.complType() == Continue) && ls.contains(c.target()))
      continue;
    if ((c.complType() == Break) && ls.contains(c.target()))
      return Completion(Normal, value);
    if (c.complType() != Normal)
      return c;
  }
}

// ECMA 12.7
Completion ContinueNode::execute()
{
  KJS_BREAKPOINT;

  KJSO dummy;
  return Context::current()->seenLabels()->contains(ident) ?
    Completion(Continue, dummy, ident) :
    Completion(Throw,
	       throwError(SyntaxError, "Label not found in containing block"));
}

// ECMA 12.8
Completion BreakNode::execute()
{
  KJS_BREAKPOINT;

  KJSO dummy;
  return Context::current()->seenLabels()->contains(ident) ?
    Completion(Break, dummy, ident) :
    Completion(Throw,
	       throwError(SyntaxError, "Label not found in containing block"));
}

// ECMA 12.9
Completion ReturnNode::execute()
{
  KJS_BREAKPOINT;

  if (!value)
    return Completion(ReturnValue, Undefined());

  KJSO e = value->evaluate();
  KJSO v = e.getValue();

  return Completion(ReturnValue, v);
}

// ECMA 12.10
Completion WithNode::execute()
{
  KJS_BREAKPOINT;

  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  Object o = v.toObject();
  Context::current()->pushScope(o);
  Completion res = stat->execute();
  Context::current()->popScope();

  return res;
}

// ECMA 12.11
ClauseListNode* ClauseListNode::append(CaseClauseNode *c)
{
  ClauseListNode *l = this;
  while (l->nx)
    l = l->nx;
  l->nx = new ClauseListNode(c);

  return this;
}

// ECMA 12.11
Completion SwitchNode::execute()
{
  KJS_BREAKPOINT;

  KJSO e = expr->evaluate();
  KJSO v = e.getValue();
  Completion res = block->evalBlock(v);

  if ((res.complType() == Break) && ls.contains(res.target()))
    return Completion(Normal, res.value());
  else
    return res;
}

// ECMA 12.11
Completion CaseBlockNode::evalBlock(const KJSO& input)
{
  KJSO v;
  Completion res;
  ClauseListNode *a = list1, *b = list2;
  CaseClauseNode *clause;

  if (a) {
    while (a) {
      clause = a->clause();
      a = a->next();
      v = clause->evaluate();
      if (strictEqual(input, v)) {
	res = clause->evalStatements();
	if (res.complType() != Normal)
	  return res;
	while (a) {
	  res = a->clause()->evalStatements();
	  if (res.complType() != Normal)
	    return res;
	  a = a->next();
	}
	break;
      }
    }
  }

  while (b) {
    clause = b->clause();
    b = b->next();
    v = clause->evaluate();
    if (strictEqual(input, v)) {
      res = clause->evalStatements();
      if (res.complType() != Normal)
	return res;
      goto step18;
    }
  }

  // default clause
  if (def) {
    res = def->evalStatements();
    if (res.complType() != Normal)
      return res;
  }
  b = list2;
 step18:
  while (b) {
    clause = b->clause();
    res = clause->evalStatements();
    if (res.complType() != Normal)
      return res;
    b = b->next();
  }

  return Completion(Normal);
}

// ECMA 12.11
KJSO CaseClauseNode::evaluate()
{
  KJSO e = expr->evaluate();
  KJSO v = e.getValue();

  return v;
}

// ECMA 12.11
Completion CaseClauseNode::evalStatements()
{
  if (list)
    return list->execute();
  else
    return Completion(Normal, Undefined());
}

// ECMA 12.12
Completion LabelNode::execute()
{
  Completion e;

  if (!Context::current()->seenLabels()->push(label)) {
    return Completion( Throw,
		       throwError(SyntaxError, "Duplicated label found" ));
  };
  e = stat->execute();
  Context::current()->seenLabels()->pop();

  if ((e.complType() == Break) && (e.target() == label))
    return Completion(Normal, e.value());
  else
    return e;
}

// ECMA 12.13
Completion ThrowNode::execute()
{
  KJS_BREAKPOINT;

  KJSO v = expr->evaluate().getValue();

  return Completion(Throw, v);
}

// ECMA 12.14
Completion TryNode::execute()
{
  KJS_BREAKPOINT;

  Completion c, c2;

  c = block->execute();

  if (!_final) {
    if (c.complType() != Throw)
      return c;
    return _catch->execute(c.value());
  }

  if (!_catch) {
    c2 = _final->execute();
    return (c2.complType() == Normal) ? c : c2;
  }

  if (c.complType() == Throw)
    c = _catch->execute(c.value());

  c2 = _final->execute();
  return (c2.complType() == Normal) ? c : c2;
}

Completion CatchNode::execute()
{
  // should never be reached. execute(const KJS &arg) is used instead
  assert(0L);
  return Completion();
}

// ECMA 12.14
Completion CatchNode::execute(const KJSO &arg)
{
  /* TODO: correct ? Not part of the spec */
  KJScriptImp::clearException();

  Object obj;
  obj.put(ident, arg, DontDelete);
  Context::current()->pushScope(obj);
  Completion c = block->execute();
  Context::current()->popScope();

  return c;
}

// ECMA 12.14
Completion FinallyNode::execute()
{
  return block->execute();
}

FunctionBodyNode::FunctionBodyNode(SourceElementsNode *s)
  : source(s)
{
#ifdef KJS_DEBUGGER
  setLoc(-1, -1);
#endif
}

// ECMA 13 + 14 for ProgramNode
Completion FunctionBodyNode::execute()
{
  /* TODO: workaround for empty body which I don't see covered by the spec */
  if (!source)
    return Completion(ReturnValue, Undefined());

  source->processFuncDecl();

  return source->execute();
}

// ECMA 13
void FuncDeclNode::processFuncDecl()
{
  const List *sc = Context::current()->pScopeChain();
  /* TODO: let this be an object with [[Class]] property "Function" */
  FunctionImp *fimp = new DeclaredFunctionImp(ident, body, sc);
  Function func(fimp); // protect from GC
  fimp->put("prototype", Object::create(ObjectClass), DontDelete);

  int plen = 0;
  for(ParameterNode *p = param; p != 0L; p = p->nextParam(), plen++)
    fimp->addParameter(p->ident());

  fimp->setLength(plen);

  Context::current()->variableObject().put(ident, func);
}

// ECMA 13
KJSO FuncExprNode::evaluate()
{
  const List *sc = Context::current()->pScopeChain();
  FunctionImp *fimp = new DeclaredFunctionImp(UString::null, body, sc->copy());
  Function ret(fimp);

  int plen = 0;
  for(ParameterNode *p = param; p != 0L; p = p->nextParam(), plen++)
    fimp->addParameter(p->ident());
  fimp->setLength(plen);

  return ret;
}

ParameterNode* ParameterNode::append(const UString *i)
{
  ParameterNode *p = this;
  while (p->next)
    p = p->next;

  p->next = new ParameterNode(i);

  return this;
}

// ECMA 13
KJSO ParameterNode::evaluate()
{
  return Undefined();
}

void ProgramNode::deleteGlobalStatements()
{
  source->deleteStatements();
}

// ECMA 14
Completion SourceElementsNode::execute()
{
  if (KJScriptImp::hadException())
    return Completion(Throw, KJScriptImp::exception());

  if (!elements)
    return element->execute();

  Completion c1 = elements->execute();
  if (KJScriptImp::hadException())
    return Completion(Throw, KJScriptImp::exception());
  if (c1.complType() != Normal)
    return c1;

  Completion c2 = element->execute();
  if (KJScriptImp::hadException())
    return Completion(Throw, KJScriptImp::exception());

  return c2;
}

// ECMA 14
void SourceElementsNode::processFuncDecl()
{
  if (elements)
    elements->processFuncDecl();

  element->processFuncDecl();
}

void SourceElementsNode::deleteStatements()
{
  element->deleteStatements();

  if (elements)
    elements->deleteStatements();
}

// ECMA 14
Completion SourceElementNode::execute()
{
  if (statement)
    return statement->execute();

  return Completion(Normal);
}

// ECMA 14
void SourceElementNode::processFuncDecl()
{
  if (function)
    function->processFuncDecl();
}

void SourceElementNode::deleteStatements()
{
  delete statement;
}

ArgumentListNode::ArgumentListNode(Node *e) : list(0L), expr(e) {}

VarDeclNode::VarDeclNode(const UString *id, AssignExprNode *in)
    : ident(*id), init(in) { }

ArgumentListNode::ArgumentListNode(ArgumentListNode *l, Node *e) :
    list(l), expr(e) {}

ArgumentsNode::ArgumentsNode(ArgumentListNode *l) : list(l) {}
