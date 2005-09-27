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
{
  putDirect(lengthPropertyName,   jsZero(),                                                       DontDelete|ReadOnly|DontEnum);
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
ValueImp *FunctionPrototypeImp::callAsFunction(ExecState */*exec*/, ObjectImp */*thisObj*/, const List &/*args*/)
{
  return Undefined();
}

// ------------------------------ FunctionProtoFuncImp -------------------------

FunctionProtoFuncImp::FunctionProtoFuncImp(ExecState *exec,
                                         FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}


bool FunctionProtoFuncImp::implementsCall() const
{
  return true;
}

ValueImp *FunctionProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  ValueImp *result = NULL;

  switch (id) {
  case ToString: {
    // ### also make this work for internal functions
    if (!thisObj || !thisObj->inherits(&InternalFunctionImp::info)) {
#ifndef NDEBUG
      fprintf(stderr,"attempted toString() call on null or non-function object\n");
#endif
      return throwError(exec, TypeError);
    }
    if (thisObj->inherits(&DeclaredFunctionImp::info)) {
       DeclaredFunctionImp *fi = static_cast<DeclaredFunctionImp*>
                                 (thisObj);
       return String("function " + fi->name().ustring() + "(" +
         fi->parameterString() + ") " + fi->body->toString());
    } else if (thisObj->inherits(&FunctionImp::info) &&
        !static_cast<FunctionImp*>(thisObj)->name().isNull()) {
      result = String("function " + static_cast<FunctionImp*>(thisObj)->name().ustring() + "()");
    }
    else {
      result = String("(Internal Function)");
    }
    }
    break;
  case Apply: {
    ValueImp *thisArg = args[0];
    ValueImp *argArray = args[1];
    ObjectImp *func = thisObj;

    if (!func->implementsCall())
      return throwError(exec, TypeError);

    ObjectImp *applyThis;
    if (thisArg->isUndefinedOrNull())
      applyThis = exec->dynamicInterpreter()->globalObject();
    else
      applyThis = thisArg->toObject(exec);

    List applyArgs;
    if (!argArray->isUndefinedOrNull()) {
      if (argArray->isObject() &&
           (static_cast<ObjectImp *>(argArray)->inherits(&ArrayInstanceImp::info) ||
            static_cast<ObjectImp *>(argArray)->inherits(&ArgumentsImp::info))) {

        ObjectImp *argArrayObj = static_cast<ObjectImp *>(argArray);
        unsigned int length = argArrayObj->get(exec,lengthPropertyName)->toUInt32(exec);
        for (unsigned int i = 0; i < length; i++)
          applyArgs.append(argArrayObj->get(exec,i));
      }
      else
        return throwError(exec, TypeError);
    }
    result = func->call(exec,applyThis,applyArgs);
    }
    break;
  case Call: {
    ValueImp *thisArg = args[0];
    ObjectImp *func = thisObj;

    if (!func->implementsCall())
      return throwError(exec, TypeError);

    ObjectImp *callThis;
    if (thisArg->isUndefinedOrNull())
      callThis = exec->dynamicInterpreter()->globalObject();
    else
      callThis = thisArg->toObject(exec);

    result = func->call(exec,callThis,args.copyTail());
    }
    break;
  }

  return result;
}

// ------------------------------ FunctionObjectImp ----------------------------

FunctionObjectImp::FunctionObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  putDirect(prototypePropertyName, funcProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsOne(), ReadOnly|DontDelete|DontEnum);
}

FunctionObjectImp::~FunctionObjectImp()
{
}

bool FunctionObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.3.2 The Function Constructor
ObjectImp *FunctionObjectImp::construct(ExecState *exec, const List &args, const UString &sourceURL, int lineNumber)
{
  UString p("");
  UString body;
  int argsSize = args.size();
  if (argsSize == 0) {
    body = "";
  } else if (argsSize == 1) {
    body = args[0]->toString(exec);
  } else {
    p = args[0]->toString(exec);
    for (int k = 1; k < argsSize - 1; k++)
      p += "," + args[k]->toString(exec);
    body = args[argsSize-1]->toString(exec);
  }

  // parse the source code
  int sid;
  int errLine;
  UString errMsg;
  SharedPtr<ProgramNode> progNode = Parser::parse(sourceURL, lineNumber, body.data(),body.size(),&sid,&errLine,&errMsg);

  // notify debugger that source has been parsed
  Debugger *dbg = exec->dynamicInterpreter()->imp()->debugger();
  if (dbg) {
    // send empty sourceURL to indicate constructed code
    bool cont = dbg->sourceParsed(exec,sid,UString(),body,errLine);
    if (!cont) {
      dbg->imp()->abort();
      return new ObjectImp();
    }
  }

  // no program node == syntax error - throw a syntax error
  if (!progNode)
    // we can't return a Completion(Throw) here, so just set the exception
    // and return it
    return throwError(exec, SyntaxError, errMsg, errLine, sid, &sourceURL);

  ScopeChain scopeChain;
  scopeChain.push(exec->dynamicInterpreter()->globalObject());
  FunctionBodyNode *bodyNode = progNode.get();

  FunctionImp *fimp = new DeclaredFunctionImp(exec, Identifier::null(), bodyNode,
					      scopeChain);

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
      return throwError(exec, SyntaxError, "Syntax error in parameter list");
  }

  List consArgs;

  ObjectImp *objCons = exec->lexicalInterpreter()->builtinObject();
  ObjectImp *prototype = objCons->construct(exec,List::empty());
  prototype->put(exec, constructorPropertyName, fimp, DontEnum|DontDelete|ReadOnly);
  fimp->put(exec, prototypePropertyName, prototype, DontEnum|DontDelete|ReadOnly);
  return fimp;
}

// ECMA 15.3.2 The Function Constructor
ObjectImp *FunctionObjectImp::construct(ExecState *exec, const List &args)
{
  return FunctionObjectImp::construct(exec, args, UString(), 0);
}


bool FunctionObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.3.1 The Function Constructor Called as a Function
ValueImp *FunctionObjectImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  return construct(exec,args);
}

