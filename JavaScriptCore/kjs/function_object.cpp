// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
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
  putDirect(lengthPropertyName,   NumberImp::zero(),                                                       DontDelete|ReadOnly|DontEnum);
  putDirect(toStringPropertyName, new FunctionProtoFuncImp(exec, this, FunctionProtoFuncImp::ToString, 0), DontEnum);
  static const Identifier applyPropertyName("apply");
  putDirect(applyPropertyName,    new FunctionProtoFuncImp(exec, this, FunctionProtoFuncImp::Apply,    2), DontEnum);
  static const Identifier callPropertyName("call");
  putDirect(callPropertyName,     new FunctionProtoFuncImp(exec, this, FunctionProtoFuncImp::Call,     1), DontEnum);
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
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
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
       return String("function " + fi->name().ustring() + "(" +
         fi->parameterString() + ") " + fi->body->toString());
    } else if (thisObj.inherits(&FunctionImp::info) &&
        !static_cast<FunctionImp*>(thisObj.imp())->name().isNull()) {
      result = String("function " + static_cast<FunctionImp*>(thisObj.imp())->name().ustring() + "()");
    }
    else {
      result = String("(Internal Function)");
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
      applyThis = exec->dynamicInterpreter()->globalObject();
    else
      applyThis = thisArg.toObject(exec);

    List applyArgs;
    if (!argArray.isA(NullType) && !argArray.isA(UndefinedType)) {
      if (argArray.isA(ObjectType) &&
           (Object::dynamicCast(argArray).inherits(&ArrayInstanceImp::info) ||
            Object::dynamicCast(argArray).inherits(&ArgumentsImp::info))) {

        Object argArrayObj = Object::dynamicCast(argArray);
        unsigned int length = argArrayObj.get(exec,lengthPropertyName).toUInt32(exec);
        for (unsigned int i = 0; i < length; i++)
          applyArgs.append(argArrayObj.get(exec,i));
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
      callThis = exec->dynamicInterpreter()->globalObject();
    else
      callThis = thisArg.toObject(exec);

    result = func.call(exec,callThis,args.copyTail());
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
  putDirect(prototypePropertyName, funcProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, NumberImp::one(), ReadOnly|DontDelete|DontEnum);
}

FunctionObjectImp::~FunctionObjectImp()
{
}

bool FunctionObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.3.2 The Function Constructor
Object FunctionObjectImp::construct(ExecState *exec, const List &args, const UString &sourceURL, int lineNumber)
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
  ProgramNode *progNode = Parser::parse(sourceURL, lineNumber, body.data(),body.size(),&sid,&errLine,&errMsg);

  // notify debugger that source has been parsed
  Debugger *dbg = exec->dynamicInterpreter()->imp()->debugger();
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

  ScopeChain scopeChain;
  scopeChain.push(exec->dynamicInterpreter()->globalObject().imp());
  FunctionBodyNode *bodyNode = progNode;

  FunctionImp *fimp = new DeclaredFunctionImp(exec, Identifier::null(), bodyNode,
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
      if (Lexer::isIdentLetter(c->uc)) {  // else error
	  param = UString(c, 1);
	  c++, i++;
	  while (i < len && (Lexer::isIdentLetter(c->uc) ||
			     Lexer::isDecimalDigit(c->uc))) {
	      param += UString(c, 1);
	      c++, i++;
	  }
	  while (i < len && *c == ' ')
	      c++, i++;
	  if (i == len) {
	      fimp->addParameter(Identifier(param));
	      params++;
	      break;
	  } else if (*c == ',') {
	      fimp->addParameter(Identifier(param));
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

  List consArgs;

  Object objCons = exec->lexicalInterpreter()->builtinObject();
  Object prototype = objCons.construct(exec,List::empty());
  prototype.put(exec, constructorPropertyName, Value(fimp), DontEnum|DontDelete|ReadOnly);
  fimp->put(exec, prototypePropertyName, prototype, DontEnum|DontDelete|ReadOnly);
  return ret;
}

// ECMA 15.3.2 The Function Constructor
Object FunctionObjectImp::construct(ExecState *exec, const List &args)
{
  return FunctionObjectImp::construct(exec, args, UString(), 0);
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

