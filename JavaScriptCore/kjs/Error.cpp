/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
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
#include "Error.h"

#include "ConstructData.h"
#include "ErrorConstructor.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "NativeErrorConstructor.h"

namespace KJS {

JSObject* Error::create(ExecState* exec, ErrorType errtype, const UString& message,
    int lineno, int sourceId, const UString& sourceURL)
{
  JSObject* cons;
  const char* name;
  switch (errtype) {
  case EvalError:
    cons = exec->lexicalGlobalObject()->evalErrorConstructor();
    name = "Evaluation error";
    break;
  case RangeError:
    cons = exec->lexicalGlobalObject()->rangeErrorConstructor();
    name = "Range error";
    break;
  case ReferenceError:
    cons = exec->lexicalGlobalObject()->referenceErrorConstructor();
    name = "Reference error";
    break;
  case SyntaxError:
    cons = exec->lexicalGlobalObject()->syntaxErrorConstructor();
    name = "Syntax error";
    break;
  case TypeError:
    cons = exec->lexicalGlobalObject()->typeErrorConstructor();
    name = "Type error";
    break;
  case URIError:
    cons = exec->lexicalGlobalObject()->URIErrorConstructor();
    name = "URI error";
    break;
  default:
    cons = exec->lexicalGlobalObject()->errorConstructor();
    name = "Error";
    break;
  }

  ArgList args;
  if (message.isEmpty())
    args.append(jsString(exec, name));
  else
    args.append(jsString(exec, message));
  ConstructData constructData;
  ConstructType constructType = cons->getConstructData(constructData);
  JSObject* err = construct(exec, cons, constructType, constructData, args);

  if (lineno != -1)
    err->put(exec, Identifier(exec, "line"), jsNumber(exec, lineno));
  if (sourceId != -1)
    err->put(exec, Identifier(exec, "sourceId"), jsNumber(exec, sourceId));

  if(!sourceURL.isNull())
    err->put(exec, Identifier(exec, "sourceURL"), jsString(exec, sourceURL));
 
  return err;
}

JSObject *Error::create(ExecState *exec, ErrorType type, const char *message)
{
    return create(exec, type, message, -1, -1, NULL);
}

JSObject *throwError(ExecState *exec, ErrorType type)
{
    JSObject *error = Error::create(exec, type, UString(), -1, -1, NULL);
    exec->setException(error);
    return error;
}

JSObject *throwError(ExecState *exec, ErrorType type, const UString &message)
{
    JSObject *error = Error::create(exec, type, message, -1, -1, NULL);
    exec->setException(error);
    return error;
}

JSObject *throwError(ExecState *exec, ErrorType type, const char *message)
{
    JSObject *error = Error::create(exec, type, message, -1, -1, NULL);
    exec->setException(error);
    return error;
}

JSObject *throwError(ExecState *exec, ErrorType type, const UString &message, int line, int sourceId, const UString &sourceURL)
{
    JSObject *error = Error::create(exec, type, message, line, sourceId, sourceURL);
    exec->setException(error);
    return error;
}

} // namespace KJS
