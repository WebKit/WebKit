// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *
 */

#include "function.h"

#include "internal.h"
#include "function_object.h"
#include "lexer.h"
#include "nodes.h"
#include "operations.h"
#include "debugger.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

using namespace KJS;

// ----------------------------- FunctionImp ----------------------------------

const ClassInfo FunctionImp::info = {"Function", &InternalFunctionImp::info, 0, 0};

namespace KJS {
  class Parameter {
  public:
    Parameter(const UString &n) : name(n), next(0L) { }
    ~Parameter() { delete next; }
    UString name;
    Parameter *next;
  };
};

FunctionImp::FunctionImp(ExecState *exec, const UString &n)
  : InternalFunctionImp(
      static_cast<FunctionPrototypeImp*>(exec->interpreter()->builtinFunctionPrototype().imp())
      ), param(0L), ident(n), argStack(0)
{
  Value protect(this);
  argStack = new ListImp();
  Value protectArgStack( argStack ); // this also calls setGcAllowed on argStack
  //fprintf(stderr,"FunctionImp::FunctionImp this=%p argStack=%p\n");
  put(exec,"arguments",Null(),ReadOnly|DontDelete|DontEnum);
}

FunctionImp::~FunctionImp()
{
  // The function shouldn't be deleted while it is still executed; argStack
  // should be set to 0 by the last call to popArgs()
  //assert(argStack->isEmpty());
  // Accessing argStack from here is a problem though.
  // When the function isn't used anymore, it's not marked, and neither is the
  // argStack, so both can be deleted - in any order!
  delete param;
}

void FunctionImp::mark()
{
  InternalFunctionImp::mark();
  if (argStack && !argStack->marked())
    argStack->mark();
}

bool FunctionImp::implementsCall() const
{
  return true;
}

Value FunctionImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  Object globalObj = exec->interpreter()->globalObject();

  Debugger *dbg = exec->interpreter()->imp()->debugger();
  int sid = -1;
  int lineno = -1;
  if (dbg) {
    if (inherits(&DeclaredFunctionImp::info)) {
      sid = static_cast<DeclaredFunctionImp*>(this)->body->sourceId();
      lineno = static_cast<DeclaredFunctionImp*>(this)->body->firstLine();
    }

    Object func(this);
    bool cont = dbg->callEvent(exec,sid,lineno,func,args);
    if (!cont) {
      dbg->imp()->abort();
      return Undefined();
    }
  }

  // enter a new execution context
  ContextImp ctx(globalObj, exec, thisObj, codeType(),
                 exec->context().imp(), this, args);
  ExecState newExec(exec->interpreter(), &ctx);
  newExec.setException(exec->exception()); // could be null

  // In order to maintain our "arguments" property, we maintain a list of arguments
  // properties from earlier in the execution stack. Upon return, we restore the
  // previous arguments object using popArgs().
  // Note: this does not appear to be part of the spec
  if (codeType() == FunctionCode) {
    assert(ctx.activationObject().inherits(&ActivationImp::info));
    Object argsObj = static_cast<ActivationImp*>(ctx.activationObject().imp())->argumentsObject();
    put(&newExec, "arguments", argsObj, DontDelete|DontEnum|ReadOnly);
    pushArgs(&newExec, argsObj);
  }

  // assign user supplied arguments to parameters
  processParameters(&newExec, args);
  // add variable declarations (initialized to undefined)
  processVarDecls(&newExec);

  Completion comp = execute(&newExec);

  // if an exception occured, propogate it back to the previous execution object
  if (newExec.hadException())
    exec->setException(newExec.exception());
  if (codeType() == FunctionCode)
    popArgs(&newExec);

#ifdef KJS_VERBOSE
  if (comp.complType() == Throw)
    printInfo(exec,"throwing", comp.value());
  else if (comp.complType() == ReturnValue)
    printInfo(exec,"returning", comp.value());
  else
    fprintf(stderr, "returning: undefined\n");
#endif

  if (dbg) {
    Object func(this);
    int cont = dbg->returnEvent(exec,sid,lineno,func);
    if (!cont) {
      dbg->imp()->abort();
      return Undefined();
    }
  }

  if (comp.complType() == Throw) {
    exec->setException(comp.value());
    return comp.value();
  }
  else if (comp.complType() == ReturnValue)
    return comp.value();
  else
    return Undefined();
}

void FunctionImp::addParameter(const UString &n)
{
  Parameter **p = &param;
  while (*p)
    p = &(*p)->next;

  *p = new Parameter(n);
}

UString FunctionImp::parameterString() const
{
  UString s;
  const Parameter * const *p = &param;
  while (*p) {
    if (!s.isEmpty())
        s += ", ";
    s += (*p)->name;
    p = &(*p)->next;
  }

  return s;
}


// ECMA 10.1.3q
void FunctionImp::processParameters(ExecState *exec, const List &args)
{
  Object variable = exec->context().imp()->variableObject();

#ifdef KJS_VERBOSE
  fprintf(stderr, "---------------------------------------------------\n"
	  "processing parameters for %s call\n",
	  name().isEmpty() ? "(internal)" : name().ascii());
#endif

  if (param) {
    ListIterator it = args.begin();
    Parameter **p = &param;
    while (*p) {
      if (it != args.end()) {
#ifdef KJS_VERBOSE
	fprintf(stderr, "setting parameter %s ", (*p)->name.ascii());
	printInfo(exec,"to", *it);
#endif
	variable.put(exec,(*p)->name, *it);
	it++;
      } else
	variable.put(exec,(*p)->name, Undefined());
      p = &(*p)->next;
    }
  }
#ifdef KJS_VERBOSE
  else {
    for (int i = 0; i < args.size(); i++)
      printInfo(exec,"setting argument", args[i]);
  }
#endif
}

void FunctionImp::processVarDecls(ExecState */*exec*/)
{
}

void FunctionImp::pushArgs(ExecState *exec, const Object &args)
{
  argStack->append(args);
  put(exec,"arguments",args,ReadOnly|DontDelete|DontEnum);
}

void FunctionImp::popArgs(ExecState *exec)
{
  argStack->removeLast();
  if (argStack->isEmpty()) {
    put(exec,"arguments",Null(),ReadOnly|DontDelete|DontEnum);
  }
  else
    put(exec,"arguments",argStack->at(argStack->size()-1),ReadOnly|DontDelete|DontEnum);
}

// ------------------------------ DeclaredFunctionImp --------------------------

// ### is "Function" correct here?
const ClassInfo DeclaredFunctionImp::info = {"Function", &FunctionImp::info, 0, 0};

DeclaredFunctionImp::DeclaredFunctionImp(ExecState *exec, const UString &n,
					 FunctionBodyNode *b, const List &sc)
  : FunctionImp(exec,n), body(b)
{
  Value protect(this);
  body->ref();
  setScope(sc.copy());
}

DeclaredFunctionImp::~DeclaredFunctionImp()
{
  if ( body->deref() )
    delete body;
}

bool DeclaredFunctionImp::implementsConstruct() const
{
  return true;
}

// ECMA 13.2.2 [[Construct]]
Object DeclaredFunctionImp::construct(ExecState *exec, const List &args)
{
  Object proto;
  Value p = get(exec,"prototype");
  if (p.type() == ObjectType)
    proto = Object(static_cast<ObjectImp*>(p.imp()));
  else
    proto = exec->interpreter()->builtinObjectPrototype();

  Object obj(new ObjectImp(proto));

  Value res = call(exec,obj,args);

  if (res.type() == ObjectType)
    return Object::dynamicCast(res);
  else
    return obj;
}

Completion DeclaredFunctionImp::execute(ExecState *exec)
{
  Completion result = body->execute(exec);

  if (result.complType() == Throw || result.complType() == ReturnValue)
      return result;
  return Completion(Normal, Undefined()); // TODO: or ReturnValue ?
}

void DeclaredFunctionImp::processVarDecls(ExecState *exec)
{
  body->processVarDecls(exec);
}

// ------------------------------ ArgumentsImp ---------------------------------

const ClassInfo ArgumentsImp::info = {"Arguments", 0, 0, 0};

// ECMA 10.1.8
ArgumentsImp::ArgumentsImp(ExecState *exec, FunctionImp *func, const List &args)
  : ObjectImp(exec->interpreter()->builtinObjectPrototype())
{
  Value protect(this);
  put(exec,"callee", Object(func), DontEnum);
  put(exec,"length", Number(args.size()), DontEnum);
  if (!args.isEmpty()) {
    ListIterator arg = args.begin();
    for (int i = 0; arg != args.end(); arg++, i++) {
      put(exec,UString::from(i), *arg, DontEnum);
    }
  }
}

// ------------------------------ ActivationImp --------------------------------

const ClassInfo ActivationImp::info = {"Activation", 0, 0, 0};

// ECMA 10.1.6
ActivationImp::ActivationImp(ExecState *exec, FunctionImp *f, const List &args)
  : ObjectImp()
{
  Value protect(this);
  arguments = new ArgumentsImp(exec,f, args);
  put(exec, "arguments", Object(arguments), Internal|DontDelete);
}

ActivationImp::~ActivationImp()
{
  arguments->setGcAllowed();
}

// ------------------------------ GlobalFunc -----------------------------------


GlobalFuncImp::GlobalFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}

CodeType GlobalFuncImp::codeType() const
{
  return id == Eval ? EvalCode : codeType();
}

bool GlobalFuncImp::implementsCall() const
{
  return true;
}

Value GlobalFuncImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  Value res;

  static const char non_escape[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				   "abcdefghijklmnopqrstuvwxyz"
				   "0123456789@*_+-./";

  switch (id) {
  case Eval: { // eval()
    Value x = args[0];
    if (x.type() != StringType)
      return x;
    else {
      UString s = x.toString(exec);

      int sid;
      int errLine;
      UString errMsg;
      ProgramNode *progNode = Parser::parse(s.data(),s.size(),&sid,&errLine,&errMsg);

      // no program node means a syntax occurred
      if (!progNode) {
	Object err = Error::create(exec,SyntaxError,errMsg.ascii(),errLine);
        err.put(exec,"sid",Number(sid));
        exec->setException(err);
        return err;
      }

      progNode->ref();

      // enter a new execution context
      Object glob(exec->interpreter()->globalObject());
      Object thisVal(Object::dynamicCast(exec->context().thisValue()));
      ContextImp *ctx = new ContextImp(glob,
                                       exec,
                                       thisVal,
                                       EvalCode,
                                       exec->context().imp());

      ExecState *newExec = new ExecState(exec->interpreter(),ctx);
      newExec->setException(exec->exception()); // could be null

      // execute the code
      Completion c = progNode->execute(newExec);

      // if an exception occured, propogate it back to the previous execution object
      if (newExec->hadException())
        exec->setException(newExec->exception());
      delete newExec;
      delete ctx;

      if ( progNode->deref() )
          delete progNode;
      if (c.complType() == ReturnValue)
	  return c;
      // ### setException() on throw?
      else if (c.complType() == Normal) {
	  if (c.isValueCompletion())
	      return c.value();
	  else
	      return Undefined();
      } else {
	  return c;
      }
    }
    break;
  }
  case ParseInt: {
    CString cstr = args[0].toString(exec).cstring();
    int radix = args[1].toInt32(exec);

    char* endptr;
    errno = 0;
    long value = strtol(cstr.c_str(), &endptr, radix);
    if (errno != 0 || endptr == cstr.c_str())
      res = Number(NaN);
    else
      res = Number(value);
    break;
  }
  case ParseFloat:
    res = Number(args[0].toString(exec).toDouble( true /*tolerant*/ ));
    break;
  case IsNaN:
    res = Boolean(isNaN(args[0].toNumber(exec)));
    break;
  case IsFinite: {
    double n = args[0].toNumber(exec);
    res = Boolean(!isNaN(n) && !isInf(n));
    break;
  }
  case Escape: {
    UString r = "", s, str = args[0].toString(exec);
    const UChar *c = str.data();
    for (int k = 0; k < str.size(); k++, c++) {
      int u = c->unicode();
      if (u > 255) {
	char tmp[7];
	sprintf(tmp, "%%u%04X", u);
	s = UString(tmp);
      } else if (strchr(non_escape, (char)u)) {
	s = UString(c, 1);
      } else {
	char tmp[4];
	sprintf(tmp, "%%%02X", u);
	s = UString(tmp);
      }
      r += s;
    }
    res = String(r);
    break;
  }
  case UnEscape: {
    UString s, str = args[0].toString(exec);
    int k = 0, len = str.size();
    UChar u;
    while (k < len) {
      const UChar *c = str.data() + k;
      if (*c == UChar('%') && k <= len - 6 && *(c+1) == UChar('u')) {
	u = Lexer::convertUnicode((c+2)->unicode(), (c+3)->unicode(),
				  (c+4)->unicode(), (c+5)->unicode());
	c = &u;
	k += 5;
      } else if (*c == UChar('%') && k <= len - 3) {
	u = UChar(Lexer::convertHex((c+1)->unicode(), (c+2)->unicode()));
	c = &u;
	k += 2;
      }
      k++;
      s += UString(c, 1);
    }
    res = String(s);
    break;
  }
  }

  return res;
}
