/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 */

#include "function_object.h"

#include "lexer.h"
#include "nodes.h"
#include "error_object.h"

extern int kjsyyparse();

using namespace KJS;

FunctionObject::FunctionObject(const Object& funcProto)
  : ConstructorImp(funcProto, 1)
{
  // ECMA 15.3.3.1 Function.prototype
  setPrototypeProperty(funcProto);
}

// ECMA 15.3.1 The Function Constructor Called as a Function
Completion FunctionObject::execute(const List &args)
{
  return Completion(ReturnValue, construct(args));
}

// ECMA 15.3.2 The Function Constructor
Object FunctionObject::construct(const List &args)
{
  UString p("");
  UString body;
  int argsSize = args.size();
  if (argsSize == 0) {
    body = "";
  } else if (argsSize == 1) {
    body = args[0].toString().value();
  } else {
    p = args[0].toString().value();
    for (int k = 1; k < argsSize - 1; k++)
      p += "," + args[k].toString().value();
    body = args[argsSize-1].toString().value();
  }

  Lexer::curr()->setCode(body.data(), body.size());

  KJScriptImp::current()->pushStack();
  int yp = kjsyyparse();
  ProgramNode *progNode = KJScriptImp::current()->progNode();
  KJScriptImp::current()->popStack();
  if (yp) {
    /* TODO: free nodes */
    return ErrorObject::create(SyntaxError,
			       I18N_NOOP("Syntax error in function body"), -1);
  }

  List scopeChain;
  scopeChain.append(Global::current());
  FunctionBodyNode *bodyNode = progNode;
  FunctionImp *fimp = new DeclaredFunctionImp(UString::null, bodyNode,
					      &scopeChain);
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
      return ErrorObject::create(SyntaxError,
				 I18N_NOOP("Syntax error in parameter list"),
				 -1);
  }

  fimp->setLength(params);
  fimp->setPrototypeProperty(Global::current().functionPrototype());
  return ret;
}

FunctionPrototype::FunctionPrototype(const Object &p)
    : ObjectImp(FunctionClass, Null(), p)
{
}
