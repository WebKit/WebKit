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

#include "internal.h"

#include <assert.h>
#include <stdio.h>

#include "kjs.h"
#include "object.h"
#include "types.h"
#include "operations.h"
#include "regexp.h"
#include "nodes.h"
#include "lexer.h"
#include "collector.h"
#include "debugger.h"

#define I18N_NOOP(s) s

extern int kjsyyparse();

using namespace KJS;

const TypeInfo UndefinedImp::info = { "Undefined", UndefinedType, 0, 0, 0 };
const TypeInfo NullImp::info = { "Null", NullType, 0, 0, 0 };
const TypeInfo NumberImp::info = { "Number", NumberType, 0, 0,0  };
const TypeInfo StringImp::info = { "String", StringType, 0, 0, 0 };
const TypeInfo BooleanImp::info = { "Boolean", BooleanType, 0, 0, 0 };
const TypeInfo CompletionImp::info = { "Completion", CompletionType, 0, 0, 0 };
const TypeInfo ReferenceImp::info = { "Reference", ReferenceType, 0, 0, 0 };

UndefinedImp *UndefinedImp::staticUndefined = 0;

UndefinedImp::UndefinedImp()
{
}

KJSO UndefinedImp::toPrimitive(Type) const
{
  return (Imp*)this;
}

Boolean UndefinedImp::toBoolean() const
{
  return Boolean(false);
}

Number UndefinedImp::toNumber() const
{
  return Number(NaN);
}

String UndefinedImp::toString() const
{
  return String("undefined");
}

Object UndefinedImp::toObject() const
{
  return Error::createObject(TypeError, I18N_NOOP("Undefined value"));
}

NullImp *NullImp::staticNull = 0;

NullImp::NullImp()
{
}

KJSO NullImp::toPrimitive(Type) const
{
  return (Imp*)this;
}

Boolean NullImp::toBoolean() const
{
  return Boolean(false);
}

Number NullImp::toNumber() const
{
  return Number(0);
}

String NullImp::toString() const
{
  return String("null");
}

Object NullImp::toObject() const
{
  return Error::createObject(TypeError, I18N_NOOP("Null value"));
}

BooleanImp* BooleanImp::staticTrue = 0;
BooleanImp* BooleanImp::staticFalse = 0;

KJSO BooleanImp::toPrimitive(Type) const
{
  return (Imp*)this;
}

Boolean BooleanImp::toBoolean() const
{
  return Boolean((BooleanImp*)this);
}

Number BooleanImp::toNumber() const
{
  return Number(val ? 1 : 0);
}

String BooleanImp::toString() const
{
  return String(val ? "true" : "false");
}

Object BooleanImp::toObject() const
{
  return Object::create(BooleanClass, Boolean((BooleanImp*)this));
}

NumberImp::NumberImp(double v)
  : val(v)
{
}

KJSO NumberImp::toPrimitive(Type) const
{
  return (Imp*)this;
}

Boolean NumberImp::toBoolean() const
{
  bool b = !((val == 0) /* || (iVal() == N0) */ || isNaN(val));

  return Boolean(b);
}

Number NumberImp::toNumber() const
{
  return Number((NumberImp*)this);
}

String NumberImp::toString() const
{
  return String(UString::from(val));
}

Object NumberImp::toObject() const
{
  return Object::create(NumberClass, Number((NumberImp*)this));
}

StringImp::StringImp(const UString& v)
  : val(v)
{
}

KJSO StringImp::toPrimitive(Type) const
{
  return (Imp*)this;
}

Boolean StringImp::toBoolean() const
{
  return Boolean(val.size() > 0);
}

Number StringImp::toNumber() const
{
  return Number(val.toDouble());
}

String StringImp::toString() const
{
  return String((StringImp*)this);
}

Object StringImp::toObject() const
{
  return Object::create(StringClass, String((StringImp*)this));
}

ReferenceImp::ReferenceImp(const KJSO& b, const UString& p)
  : base(b), prop(p)
{
}

void ReferenceImp::mark(Imp*)
{
  Imp::mark();
  Imp *im = base.imp();
  if (im && !im->marked())
    im->mark();
}

CompletionImp::CompletionImp(Compl c, const KJSO& v, const UString& t)
  : comp(c), val(v), tar(t)
{
}

void CompletionImp::mark(Imp*)
{
  Imp::mark();
  Imp *im = val.imp();
  if (im && !im->marked())
    im->mark();
}

RegExpImp::RegExpImp()
  : ObjectImp(RegExpClass), reg(0L)
{
}

RegExpImp::~RegExpImp()
{
  delete reg;
}

// ECMA 10.2
Context::Context(CodeType type, Context *callingContext,
		 FunctionImp *func, const List *args, Imp *thisV)
{
  Global glob(Global::current());

  // create and initialize activation object (ECMA 10.1.6)
  if (type == FunctionCode || type == AnonymousCode || type == HostCode) {
    activation = new ActivationImp(func, args);
    variable = activation;
  } else {
    activation = KJSO();
    variable = glob;
  }

  // ECMA 10.2
  switch(type) {
    case EvalCode:
      if (callingContext) {
	scopeChain = callingContext->copyOfChain();
	variable = callingContext->variableObject();
	thisVal = callingContext->thisValue();
	break;
      } // else same as GlobalCode
    case GlobalCode:
      scopeChain = new List();
      scopeChain->append(glob);
      thisVal = glob.imp();
      break;
    case FunctionCode:
    case AnonymousCode:
      if (type == FunctionCode) {
	scopeChain = ((DeclaredFunctionImp*)func)->scopeChain()->copy();
	scopeChain->prepend(activation);
      } else {
	scopeChain = new List();
	scopeChain->append(activation);
	scopeChain->append(glob);
      }
      variable = activation; /* TODO: DontDelete ? (ECMA 10.2.3) */
      if (thisV->type() >= ObjectType) {
	thisVal = thisV;
      }
      else
	thisVal = glob.imp();
      break;
    case HostCode:
      if (thisV->type() >= ObjectType)
	thisVal = thisV;
      else
	thisVal = glob;
      variable = activation; /* TODO: DontDelete (ECMA 10.2.4) */
      scopeChain = new List();
      scopeChain->append(activation);
      if (func->hasAttribute(ImplicitThis))
	scopeChain->append(KJSO(thisVal));
      if (func->hasAttribute(ImplicitParents)) {
	/* TODO ??? */
      }
      scopeChain->append(glob);
      break;
    }
}

Context::~Context()
{
  delete scopeChain;
}

Context *Context::current()
{
  return KJScriptImp::curr ? KJScriptImp::curr->con : 0L;
}

void Context::setCurrent(Context *c)
{
  KJScriptImp::current()->con = c;
}

void Context::pushScope(const KJSO &s)
{
  scopeChain->prepend(s);
}

void Context::popScope()
{
  scopeChain->removeFirst();
}

List* Context::copyOfChain()
{
  return scopeChain->copy();
}


AnonymousFunction::AnonymousFunction()
  : Function(0L)
{
  /* TODO */
}

DeclaredFunctionImp::DeclaredFunctionImp(const UString &n,
					 FunctionBodyNode *b, const List *sc)
  : ConstructorImp(n), body(b), scopes(sc->copy())
{
}

DeclaredFunctionImp::~DeclaredFunctionImp()
{
  delete scopes;
}

// step 2 of ECMA 13.2.1. rest in FunctionImp::executeCall()
Completion DeclaredFunctionImp::execute(const List &)
{
 /* TODO */

#ifdef KJS_DEBUGGER
  Debugger *dbg = KJScriptImp::current()->debugger();
  int oldSourceId = -1;
  if (dbg) {
    oldSourceId = dbg->sourceId();
    dbg->setSourceId(body->sourceId());
  }
#endif

  Completion result = body->execute();

#ifdef KJS_DEBUGGER
  if (dbg) {
    dbg->setSourceId(oldSourceId);
  }
#endif

  if (result.complType() == Throw || result.complType() == ReturnValue)
      return result;
  return Completion(Normal, Undefined()); /* TODO: or ReturnValue ? */
}

// ECMA 13.2.2 [[Construct]]
Object DeclaredFunctionImp::construct(const List &args)
{
  Object obj(ObjectClass);
  KJSO p = get("prototype");
  if (p.isObject())
    obj.setPrototype(p);
  else
    obj.setPrototype(Global::current().objectPrototype());

  KJSO res = executeCall(obj.imp(), &args);

  Object v = Object::dynamicCast(res);
  if (v.isNull())
    return obj;
  else
    return v;
}

Completion AnonymousFunction::execute(const List &)
{
 /* TODO */
  return Completion(Normal, Null());
}

// ECMA 10.1.8
class ArgumentsObject : public ObjectImp {
public:
  ArgumentsObject(FunctionImp *func, const List *args);
};

ArgumentsObject::ArgumentsObject(FunctionImp *func, const List *args)
  : ObjectImp(UndefClass)
{
  put("callee", Function(func), DontEnum);
  if (args) {
    put("length", Number(args->size()), DontEnum);
    ListIterator arg = args->begin();
    for (int i = 0; arg != args->end(); arg++, i++) {
      put(UString::from(i), *arg, DontEnum);
    }
  }
}

const TypeInfo ActivationImp::info = { "Activation", ActivationType, 0, 0, 0 };

// ECMA 10.1.6
ActivationImp::ActivationImp(FunctionImp *f, const List *args)
{
  KJSO aobj(new ArgumentsObject(f, args));
  put("arguments", aobj, DontDelete);
  /* TODO: this is here to get myFunc.arguments and myFunc.a1 going.
     I can't see this described in the spec but it's possible in browsers. */
  if (!f->name().isNull())
    f->put("arguments", aobj);
}

ExecutionStack::ExecutionStack()
  : progNode(0L), firstNode(0L), prev(0)
{
}

ExecutionStack* ExecutionStack::push()
{
  ExecutionStack *s = new ExecutionStack();
  s->prev = this;

  return s;
}

ExecutionStack* ExecutionStack::pop()
{
  ExecutionStack *s = prev;
  delete this;

  return s;
}

KJScriptImp* KJScriptImp::curr = 0L;
KJScriptImp* KJScriptImp::hook = 0L;
int          KJScriptImp::instances = 0;
int          KJScriptImp::running = 0;

KJScriptImp::KJScriptImp(KJScript *s)
  : scr(s),
    initialized(false),
    glob(0L),
#ifdef KJS_DEBUGGER
    dbg(0L),
#endif
    retVal(0L)
{
  instances++;
  KJScriptImp::curr = this;
  // are we the first interpreter instance ? Initialize some stuff
  if (instances == 1)
    globalInit();
  stack = new ExecutionStack();
  clearException();
  lex = new Lexer();
}

KJScriptImp::~KJScriptImp()
{
  KJScriptImp::curr = this;

#ifdef KJS_DEBUGGER
  attachDebugger(0L);
#endif

  clear();

  delete lex;
  lex = 0L;

  delete stack;
  stack = 0L;

  KJScriptImp::curr = 0L;
  // are we the last of our kind ? Free global stuff.
  if (instances == 1)
    globalClear();
  instances--;
}

void KJScriptImp::globalInit()
{
  UndefinedImp::staticUndefined = new UndefinedImp();
  UndefinedImp::staticUndefined->ref();
  NullImp::staticNull = new NullImp();
  NullImp::staticNull->ref();
  BooleanImp::staticTrue = new BooleanImp(true);
  BooleanImp::staticTrue->ref();
  BooleanImp::staticFalse = new BooleanImp(false);
  BooleanImp::staticFalse->ref();
}

void KJScriptImp::globalClear()
{
  UndefinedImp::staticUndefined->deref();
  UndefinedImp::staticUndefined = 0L;
  NullImp::staticNull->deref();
  NullImp::staticNull = 0L;
  BooleanImp::staticTrue->deref();
  BooleanImp::staticTrue = 0L;
  BooleanImp::staticFalse->deref();
  BooleanImp::staticFalse = 0L;
}

void KJScriptImp::mark()
{
  if (exVal && !exVal->marked())
    exVal->mark();
  if (retVal && !retVal->marked())
    retVal->mark();
  UndefinedImp::staticUndefined->mark();
  NullImp::staticNull->mark();
  BooleanImp::staticTrue->mark();
  BooleanImp::staticFalse->mark();
}

void KJScriptImp::init()
{
  KJScriptImp::curr = this;

  clearException();
  retVal = 0L;

  if (!initialized) {
    // add this interpreter to the global chain
    // as a root set for garbage collection
    if (hook) {
      prev = hook;
      next = hook->next;
      hook->next->prev = this;
      hook->next = this;
    } else {
      hook = next = prev = this;
    }

    glob.init();
    con = new Context();
    firstN = 0L;
    progN = 0L;
    recursion = 0;
    errMsg = "";
    initialized = true;
#ifdef KJS_DEBUGGER
    sid = -1;
#endif
  }
}

void KJScriptImp::clear()
{
  if ( recursion ) {
#ifndef NDEBUG
      fprintf(stderr, "KJS: ignoring clear() while running\n");
#endif
      return;
  }
  KJScriptImp *old = curr;
  if (initialized) {
    KJScriptImp::curr = this;

    Node::setFirstNode(firstNode());
    Node::deleteAllNodes();
    setFirstNode(0L);
    setProgNode(0L);

    clearException();
    retVal = 0L;

    delete con; con = 0L;
    glob.clear();

    Collector::collect();

    // remove from global chain (see init())
    next->prev = prev;
    prev->next = next;
    hook = next;
    if (hook == this)
      hook = 0L;

#ifdef KJS_DEBUGGER
    sid = -1;
#endif

    initialized = false;
  }
  if (old != this)
      KJScriptImp::curr = old;
}

bool KJScriptImp::evaluate(const UChar *code, unsigned int length, const KJSO &thisV,
			   bool onlyCheckSyntax)
{
  init();

#ifdef KJS_DEBUGGER
  sid++;
  if (debugger())
    debugger()->setSourceId(sid);
#endif
  if (recursion > 7) {
    fprintf(stderr, "KJS: breaking out of recursion\n");
    return true;
  } else if (recursion > 0) {
#ifndef NDEBUG
    fprintf(stderr, "KJS: entering recursion level %d\n", recursion);
#endif
    pushStack();
  }

  assert(Lexer::curr());
  Lexer::curr()->setCode(code, length);
  Node::setFirstNode(firstNode());
  int parseError = kjsyyparse();
  setFirstNode(Node::firstNode());

  if (parseError) {
    errType = 99; /* TODO */
    errLine = Lexer::curr()->lineNo();
    errMsg = "Parse error at line " + UString::from(errLine);
#ifndef NDEBUG
    fprintf(stderr, "JavaScript parse error at line %d.\n", errLine);
#endif
    /* TODO: either clear everything or keep previously
       parsed function definitions */
    //    Node::deleteAllNodes();
    return false;
  }

  if (onlyCheckSyntax)
      return true;

  clearException();

  KJSO oldVar;
  if (!thisV.isNull()) {
    context()->setThisValue(thisV);
    context()->pushScope(thisV);
    oldVar = context()->variableObject();
    context()->setVariableObject(thisV);
  }

  running++;
  recursion++;
  assert(progNode());
  Completion res = progNode()->execute();
  recursion--;
  running--;

  if (hadException()) {
    KJSO err = exception();
    errType = 99; /* TODO */
    errLine = err.get("line").toInt32();
    errMsg = err.get("name").toString().value() + ". ";
    errMsg += err.get("message").toString().value();
#ifdef KJS_DEBUGGER
    if (dbg)
      dbg->setSourceId(err.get("sid").toInt32());
#endif
    clearException();
  } else {
    errType = 0;
    errLine = -1;
    errMsg = "";

    // catch return value
    retVal = 0L;
    if (res.complType() == ReturnValue || !thisV.isNull())
	retVal = res.value().imp();
  }

  if (!thisV.isNull()) {
    context()->popScope();
    context()->setVariableObject(oldVar);
  }

  if (progNode())
    progNode()->deleteGlobalStatements();

  if (recursion > 0) {
    popStack();
  }

  return !errType;
}

void KJScriptImp::pushStack()
{
    stack = stack->push();
}

void KJScriptImp::popStack()
{
    stack = stack->pop();
    assert(stack);
}

bool KJScriptImp::call(const KJSO &scope, const UString &func, const List &args)
{
  init();
  KJSO callScope(scope);
  if (callScope.isNull())
    callScope = Global::current().imp();
  if (!callScope.hasProperty(func)) {
#ifndef NDEBUG
      fprintf(stderr, "couldn't resolve function name %s. call() failed\n",
	      func.ascii());
#endif
      return false;
  }
  KJSO v = callScope.get(func);
  if (!v.isA(ConstructorType)) {
#ifndef NDEBUG
      fprintf(stderr, "%s is not a function. call() failed.\n", func.ascii());
#endif
      return false;
  }
  running++;
  recursion++;
  static_cast<ConstructorImp*>(v.imp())->executeCall(scope.imp(), &args);
  recursion--;
  running--;
  return !hadException();
}

bool KJScriptImp::call(const KJSO &func, const KJSO &thisV,
		       const List &args, const List &extraScope)
{
  init();
  if(!func.implementsCall())
    return false;

  running++;
  recursion++;
  retVal = func.executeCall(thisV, &args, &extraScope).imp();
  recursion--;
  running--;

  return !hadException();
}

void KJScriptImp::setException(Imp *e)
{
  assert(curr);
  curr->exVal = e;
  curr->exMsg = "Exception"; // not very meaningful but we use !0L to test
}

void KJScriptImp::setException(const char *msg)
{
  assert(curr);
  curr->exVal = 0L;		// will be created later on exception()
  curr->exMsg = msg;
}

KJSO KJScriptImp::exception()
{
  assert(curr);
  if (!curr->exMsg)
    return Undefined();
  if (curr->exVal)
    return curr->exVal;
  return Error::create(GeneralError, curr->exMsg);
}

void KJScriptImp::clearException()
{
  assert(curr);
  curr->exMsg = 0L;
  curr->exVal = 0L;
}

#ifdef KJS_DEBUGGER
void KJScriptImp::attachDebugger(Debugger *d)
{
  static bool detaching = false;
  if (detaching) // break circular detaching
    return;

  if (dbg) {
    detaching = true;
    dbg->detach();
    detaching = false;
  }

  dbg = d;
}

bool KJScriptImp::setBreakpoint(int id, int line, bool set)
{
  init();
  return Node::setBreakpoint(firstNode(), id, line, set);
}

#endif

bool PropList::contains(const UString &name)
{
  PropList *p = this;
  while (p) {
    if (name == p->name)
      return true;
    p = p->next;
  }
  return false;
}

bool LabelStack::push(const UString &id)
{
  if (id.isEmpty() || contains(id))
    return false;

  StackElm *newtos = new StackElm;
  newtos->id = id;
  newtos->prev = tos;
  tos = newtos;
  return true;
}

bool LabelStack::contains(const UString &id) const
{
  if (id.isEmpty())
    return true;

  for (StackElm *curr = tos; curr; curr = curr->prev)
    if (curr->id == id)
      return true;

  return false;
}

void LabelStack::pop()
{
  if (tos) {
    StackElm *prev = tos->prev;
    delete tos;
    tos = prev;
  }
}

LabelStack::~LabelStack()
{
  StackElm *prev;

  while (tos) {
    prev = tos->prev;
    delete tos;
    tos = prev;
  }
}

// ECMA 15.3.5.3 [[HasInstance]]
// see comment in header file
KJSO KJS::hasInstance(const KJSO &F, const KJSO &V)
{
  if (V.isObject()) {
    KJSO prot = F.get("prototype");
    if (!prot.isObject())
      return Error::create(TypeError, "Invalid prototype encountered "
			   "in instanceof operation.");
    Imp *v = V.imp();
    while ((v = v->prototype())) {
      if (v == prot.imp())
	return Boolean(true);
    }
  }
  return Boolean(false);
}

#ifndef NDEBUG
#include <stdio.h>
void KJS::printInfo( const char *s, const KJSO &o )
{
    if (o.isNull())
      fprintf(stderr, "%s: (null)\n", s);
    else {
      KJSO v = o;
      if (o.isA(ReferenceType))
	  v = o.getValue();
      fprintf(stderr, "JS: %s: %s : %s (%p)\n",
	      s,
	      v.toString().value().ascii(),
	      v.imp()->typeInfo()->name,
	      (void*)v.imp());
      if (o.isA(ReferenceType)) {
	  fprintf(stderr, "JS: Was property '%s'\n", o.getPropertyName().ascii());
	  printInfo("of", o.getBase());
      }
    }
}
#endif
