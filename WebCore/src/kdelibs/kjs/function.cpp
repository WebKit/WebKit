/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#include "function.h"

#include "kjs.h"
#include "types.h"
#include "internal.h"
#include "operations.h"
#include "nodes.h"
#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace KJS;

const TypeInfo FunctionImp::info = { "Function", FunctionType,
				      &ObjectImp::info, 0, 0 };
const TypeInfo InternalFunctionImp::info = { "InternalFunction",
					      InternalFunctionType,
					      &FunctionImp::info, 0, 0 };
const TypeInfo ConstructorImp::info = { "Function", ConstructorType,
					 &InternalFunctionImp::info, 0, 0 };

namespace KJS {

  class Parameter {
  public:
    Parameter(const UString &n) : name(n), next(0L) { }
    ~Parameter() { delete next; }
    UString name;
    Parameter *next;
  };

};

FunctionImp::FunctionImp()
  : ObjectImp(/*TODO*/BooleanClass), param(0L)
{
}

FunctionImp::FunctionImp(const UString &n)
  : ObjectImp(/*TODO*/BooleanClass), ident(n), param(0L)
{
}

FunctionImp::~FunctionImp()
{
  delete param;
}

KJSO FunctionImp::thisValue() const
{
  return KJSO(Context::current()->thisValue());
}

void FunctionImp::addParameter(const UString &n)
{
  Parameter **p = &param;
  while (*p)
    p = &(*p)->next;

  *p = new Parameter(n);
}

void FunctionImp::setLength(int l)
{
  put("length", Number(l), ReadOnly|DontDelete|DontEnum);
}

// ECMA 10.1.3
void FunctionImp::processParameters(const List *args)
{
  KJSO variable = Context::current()->variableObject();

  assert(args);

#ifdef KJS_VERBOSE
  fprintf(stderr, "---------------------------------------------------\n"
	  "processing parameters for %s call\n",
	  name().isEmpty() ? "(internal)" : name().ascii());
#endif

  if (param) {
    ListIterator it = args->begin();
    Parameter **p = &param;
    while (*p) {
      if (it != args->end()) {
#ifdef KJS_VERBOSE
	fprintf(stderr, "setting parameter %s ", (*p)->name.ascii());
	printInfo("to", *it);
#endif
	variable.put((*p)->name, *it);
	it++;
      } else
	variable.put((*p)->name, Undefined());
      p = &(*p)->next;
    }
  }
#ifdef KJS_VERBOSE
  else {
    for (int i = 0; i < args->size(); i++)
      printInfo("setting argument", (*args)[i]);
  }
#endif
#ifdef KJS_DEBUGGER
  if (KJScriptImp::current()->debugger()) {
    UString argStr;
    for (int i = 0; i < args->size(); i++) {
      if (i > 0)
	argStr += ", ";
      Imp *a = (*args)[i].imp();
      argStr += a->toString().value() + " : " +	UString(a->typeInfo()->name);
    }
    UString n = name().isEmpty() ? UString( "(internal)" ) : name();
    KJScriptImp::current()->debugger()->callEvent(n, argStr);
  }
#endif
}

// ECMA 13.2.1
KJSO FunctionImp::executeCall(Imp *thisV, const List *args)
{
  return executeCall(thisV,args,0);
}

KJSO FunctionImp::executeCall(Imp *thisV, const List *args, const List *extraScope)
{
  bool dummyList = false;
  if (!args) {
    args = new List();
    dummyList = true;
  }

  KJScriptImp *curr = KJScriptImp::current();
  Context *save = curr->context();

  Context *ctx = new Context(codeType(), save, this, args, thisV);
  curr->setContext(ctx);

  int numScopes = 0;
  if (extraScope) {
    ListIterator scopeIt = extraScope->begin();
    for (; scopeIt != extraScope->end(); scopeIt++) {
      KJSO obj(*scopeIt);
      ctx->pushScope(obj);
      numScopes++;
    }
  }

  // assign user supplied arguments to parameters
  processParameters(args);

  Completion comp = execute(*args);

  if (dummyList)
    delete args;

  int i;
  for (i = 0; i < numScopes; i++)
    ctx->popScope();

  put("arguments", Null());
  delete ctx;
  curr->setContext(save);

#ifdef KJS_VERBOSE
  if (comp.complType() == Throw)
    printInfo("throwing", comp.value());
  else if (comp.complType() == ReturnValue)
    printInfo("returning", comp.value());
  else
    fprintf(stderr, "returning: undefined\n");
#endif
#ifdef KJS_DEBUGGER
  if (KJScriptImp::current()->debugger())
    KJScriptImp::current()->debugger()->returnEvent();
#endif

  if (comp.complType() == Throw)
    return comp.value();
  else if (comp.complType() == ReturnValue)
    return comp.value();
  else
    return Undefined();
}

UString FunctionImp::name() const
{
  return ident;
}

InternalFunctionImp::InternalFunctionImp()
{
}

InternalFunctionImp::InternalFunctionImp(int l)
{
  if (l >= 0)
    setLength(l);
}

InternalFunctionImp::InternalFunctionImp(const UString &n)
  : FunctionImp(n)
{
}

String InternalFunctionImp::toString() const
{
  if (name().isNull())
    return UString("(Internal function)");
  else
    return UString("function " + name() + "()");
}

Completion InternalFunctionImp::execute(const List &)
{
  return Completion(ReturnValue, Undefined());
}

ConstructorImp::ConstructorImp() {
  setPrototype(Global::current().functionPrototype());
  // TODO ???  put("constructor", this);
  setLength(1);
}

ConstructorImp::ConstructorImp(const UString &n)
  : InternalFunctionImp(n)
{
}

ConstructorImp::ConstructorImp(const KJSO &p, int len)
{
  setPrototype(p);
  // TODO ???  put("constructor", *this);
  setLength(len);
}

ConstructorImp::ConstructorImp(const UString &n, const KJSO &p, int len)
  : InternalFunctionImp(n)
{
  setPrototype(p);
  // TODO ???  put("constructor", *this);
  setLength(len);
}

ConstructorImp::~ConstructorImp() { }

Completion ConstructorImp::execute(const List &)
{
  /* TODO */
  return Completion(ReturnValue, Null());
}

Function::Function(Imp *d)
  : KJSO(d)
{
  if (d) {
    static_cast<FunctionImp*>(rep)->attr = ImplicitNone;
    assert(Global::current().hasProperty("[[Function.prototype]]"));
    setPrototype(Global::current().functionPrototype());
  }
}

Completion Function::execute(const List &args)
{
  assert(rep);
  return static_cast<FunctionImp*>(rep)->execute(args);
}

bool Function::hasAttribute(FunctionAttribute a) const
{
  assert(rep);
  return static_cast<FunctionImp*>(rep)->hasAttribute(a);
}

#if 0
InternalFunction::InternalFunction(Imp *d)
  : Function(d)
{
  param = 0L;
}

InternalFunction::~InternalFunction()
{
}
#endif

Constructor::Constructor(Imp *d)
  : Function(d)
{
  if (d) {
    assert(Global::current().hasProperty("[[Function.prototype]]"));
    setPrototype(Global::current().get("[[Function.prototype]]"));
    put("constructor", *this);
    KJSO tmp(d); // protect from GC
    ((FunctionImp*)d)->setLength(1);
  }
}

#if 0
Constructor::Constructor(const Object& proto, int len)
{
  setPrototype(proto);
  put("constructor", *this);
  put("length", len, DontEnum);
}
#endif

Constructor::~Constructor()
{
}

Completion Constructor::execute(const List &)
{
  /* TODO: call construct instead ? */
  return Completion(ReturnValue, Undefined());
}

Object Constructor::construct(const List &args)
{
  assert(rep && rep->type() == ConstructorType);
  return ((ConstructorImp*)rep)->construct(args);
}

Constructor Constructor::dynamicCast(const KJSO &obj)
{
  // return null object on type mismatch
  if (!obj.isA(ConstructorType))
    return Constructor(0L);

  return Constructor(obj.imp());
}

KJSO Function::thisValue() const
{
  return KJSO(Context::current()->thisValue());
}
