// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "function_object.h"
#include "internal.h"
#include "function.h"
#include "array_object.h"
#include "nodes.h"
#include "lexer.h"
#include "debugger.h"
#include "object.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace KJS;

// ------------------------------ FunctionPrototypeImp -------------------------

FunctionPrototypeImp::FunctionPrototypeImp(ExecState *exec)
  : InternalFunctionImp(0)
{
  Value protect(this);
  put(exec, "toString", Object(new FunctionProtoFuncImp(exec, this, FunctionProtoFuncImp::ToString, 0)), DontEnum);
  put(exec, "apply",    Object(new FunctionProtoFuncImp(exec, this, FunctionProtoFuncImp::Apply,    2)), DontEnum);
  put(exec, "call",     Object(new FunctionProtoFuncImp(exec, this, FunctionProtoFuncImp::Call,     1)), DontEnum);
}

FunctionPrototypeImp::~FunctionPrototypeImp()
{
}

bool FunctionPrototypeImp::implementsCall() const
{
  return true;
}

// ECMA 15.3.4
Value FunctionPrototypeImp::call(ExecState */*exec*/, Object &/*thisObj*/, const List &/*args*/)
{
  return Undefined();
}

// ------------------------------ FunctionProtoFuncImp -------------------------

FunctionProtoFuncImp::FunctionProtoFuncImp(ExecState *exec,
                                         FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}


bool FunctionProtoFuncImp::implementsCall() const
{
  return true;
}

Value FunctionProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  Value result;

  switch (id) {
  case ToString: {
    // ### also make this work for internal functions
    if (thisObj.isNull() || !thisObj.inherits(&InternalFunctionImp::info)) {
#ifndef NDEBUG
      fprintf(stderr,"attempted toString() call on null or non-function object\n");
#endif
      Object err = Error::create(exec,TypeError);
      exec->setException(err);
      return err;
    }
    if (thisObj.inherits(&DeclaredFunctionImp::info)) {
       DeclaredFunctionImp *fi = static_cast<DeclaredFunctionImp*>
                                 (thisObj.imp());
       return String("function " + fi->name() + "(" +
         fi->parameterString() + ") " + fi->body->toString());
    } else if (thisObj.inherits(&FunctionImp::info) &&
        !static_cast<FunctionImp*>(thisObj.imp())->name().isNull()) {
      result = String("function " + static_cast<FunctionImp*>(thisObj.imp())->name() + "()");
    }
    else {
      result = String("(Internal function)");
    }
    }
    break;
  case Apply: {
    Value thisArg = args[0];
    Value argArray = args[1];
    Object func = thisObj;

    if (!func.implementsCall()) {
      Object err = Error::create(exec,TypeError);
      exec->setException(err);
      return err;
    }

    Object applyThis;
    if (thisArg.isA(NullType) || thisArg.isA(UndefinedType))
      applyThis = exec->interpreter()->globalObject();
    else
      applyThis = thisArg.toObject(exec);

    List applyArgs;
    if (!argArray.isA(NullType) && !argArray.isA(UndefinedType)) {
      if ((argArray.isA(ObjectType) &&
           Object::dynamicCast(argArray).inherits(&ArrayInstanceImp::info)) ||
           Object::dynamicCast(argArray).inherits(&ArgumentsImp::info)) {

        Object argArrayObj = Object::dynamicCast(argArray);
        unsigned int length = argArrayObj.get(exec,"length").toUInt32(exec);
        for (unsigned int i = 0; i < length; i++)
          applyArgs.append(argArrayObj.get(exec,UString::from(i)));
      }
      else {
        Object err = Error::create(exec,TypeError);
        exec->setException(err);
        return err;
      }
    }
    result = func.call(exec,applyThis,applyArgs);
    }
    break;
  case Call: {
    Value thisArg = args[0];
    Object func = thisObj;

    if (!func.implementsCall()) {
      Object err = Error::create(exec,TypeError);
      exec->setException(err);
      return err;
    }

    Object callThis;
    if (thisArg.isA(NullType) || thisArg.isA(UndefinedType))
      callThis = exec->interpreter()->globalObject();
    else
      callThis = thisArg.toObject(exec);

    List callArgs = args.copy();
    callArgs.removeFirst();
    result = func.call(exec,callThis,callArgs);
    }
    break;
  }

  return result;
}

// ------------------------------ FunctionObjectImp ----------------------------

FunctionObjectImp::FunctionObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  put(exec,"prototype", Object(funcProto), DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec,"length", Number(1), ReadOnly|DontDelete|DontEnum);
}

FunctionObjectImp::~FunctionObjectImp()
{
}

bool FunctionObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.3.2 The Function Constructor
Object FunctionObjectImp::construct(ExecState *exec, const List &args)
{
  UString p("");
  UString body;
  int argsSize = args.size();
  if (argsSize == 0) {
    body = "";
  } else if (argsSize == 1) {
    body = args[0].toString(exec);
  } else {
    p = args[0].toString(exec);
    for (int k = 1; k < argsSize - 1; k++)
      p += "," + args[k].toString(exec);
    body = args[argsSize-1].toString(exec);
  }

  // parse the source code
  int sid;
  int errLine;
  UString errMsg;
  ProgramNode *progNode = Parser::parse(body.data(),body.size(),&sid,&errLine,&errMsg);

  // notify debugger that source has been parsed
  Debugger *dbg = exec->interpreter()->imp()->debugger();
  if (dbg) {
    bool cont = dbg->sourceParsed(exec,sid,body,errLine);
    if (!cont) {
      dbg->imp()->abort();
      return Object(new ObjectImp());
    }
  }

  // no program node == syntax error - throw a syntax error
  if (!progNode) {
    Object err = Error::create(exec,SyntaxError,errMsg.ascii(),errLine);
    // we can't return a Completion(Throw) here, so just set the exception
    // and return it
    exec->setException(err);
    return err;
  }

  List scopeChain;
  scopeChain.append(exec->interpreter()->globalObject());
  FunctionBodyNode *bodyNode = progNode;

  FunctionImp *fimp = new DeclaredFunctionImp(exec, UString::null, bodyNode,
					      scopeChain);
  Object ret(fimp); // protect from GC

  // parse parameter list. throw syntax error on illegal identifiers
  int len = p.size();
  const UChar *c = p.data();
  int i = 0, params = 0;
  UString param;
  while (i < len) {
      while (*c == ' ' && i < len)
	  c++, i++;
      if (Lexer::isIdentLetter(c->unicode())) {  // else error
	  param = UString(c, 1);
	  c++, i++;
	  while (i < len && (Lexer::isIdentLetter(c->unicode()) ||
			     Lexer::isDecimalDigit(c->unicode()))) {
	      param += UString(c, 1);
	      c++, i++;
	  }
	  while (i < len && *c == ' ')
	      c++, i++;
	  if (i == len) {
	      fimp->addParameter(param);
	      params++;
	      break;
	  } else if (*c == ',') {
	      fimp->addParameter(param);
	      params++;
	      c++, i++;
	      continue;
	  } // else error
      }
      Object err = Error::create(exec,SyntaxError,
				 I18N_NOOP("Syntax error in parameter list"),
				 -1);
      exec->setException(err);
      return err;
  }

  fimp->put(exec,"length", Number(params),ReadOnly|DontDelete|DontEnum);
  List consArgs;

  Object objCons = exec->interpreter()->builtinObject();
  Object prototype = objCons.construct(exec,List::empty());
  prototype.put(exec, "constructor",
		Object(fimp), DontEnum|DontDelete|ReadOnly);
  fimp->put(exec,"prototype",prototype,DontEnum|DontDelete|ReadOnly);
  fimp->put(exec,"arguments",Null(),DontEnum|DontDelete|ReadOnly);
  return ret;
}

bool FunctionObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.3.1 The Function Constructor Called as a Function
Value FunctionObjectImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  return construct(exec,args);
}

