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

#include "kjs.h"
#include "operations.h"
#include "types.h"
#include "internal.h"
#include "error_object.h"
#include "debugger.h"

using namespace KJS;

const char *ErrorObject::errName[] = {
  "No Error",
  "Error",
  "EvalError",
  "RangeError",
  "ReferenceError",
  "SyntaxError",
  "TypeError",
  "URIError"
};

ErrorObject::ErrorObject(const Object &funcProto, const Object &errProto,
			 ErrorType t)
  : ConstructorImp(funcProto, 1), errType(t)
{
  // ECMA 15.11.3.1 Error.prototype
  setPrototypeProperty(errProto);
  const char *n = errName[errType];

  put("name", String(n));
}

ErrorObject::ErrorObject(const Object& proto, ErrorType t,
			 const char *m, int l)
  : ConstructorImp(proto, 1), errType(t)
{
  const char *n = errName[errType];

  put("name", String(n));
  put("message", String(m));
  put("line", Number(l));
#ifdef KJS_DEBUGGER
  Debugger *dbg = KJScriptImp::current()->debugger();
  if (dbg)
    put("sid", Number(dbg->sourceId()));
#endif
}

// ECMA 15.9.2
Completion ErrorObject::execute(const List &args)
{
  // "Error()" gives the sames result as "new Error()"
  return Completion(ReturnValue, construct(args));
}

// ECMA 15.9.3
Object ErrorObject::construct(const List &args)
{
  if (args.isEmpty() == 1 || !args[0].isDefined())
    return Object::create(ErrorClass, Undefined());

  String message = args[0].toString();
  return Object::create(ErrorClass, message);
}

Object ErrorObject::create(ErrorType e, const char *m, int l)
{
  Global global(Global::current());
  KJSO prot = Global::current().get("[[Error.prototype]]");
  assert(prot.isObject());
  Imp *d = new ErrorObject(Object(prot.imp()), e, m, l);

  return Object(d);
}

// ECMA 15.9.4
ErrorPrototype::ErrorPrototype(const Object& proto)
  : ObjectImp(ErrorClass, Undefined(), proto)
{
  // The constructor will be added later in ErrorObject's constructor
}

KJSO ErrorPrototype::get(const UString &p) const
{
  const char *s;

  /* TODO: are these properties dynamic, i.e. should we put() them ? */
  if (p == "name")
    s = "Error";
  else if (p == "message")
    s = "Error message.";
  else if (p == "toString")
    return Function(new ErrorProtoFunc());
  else
    return Imp::get(p);

  return String(s);
}

Completion ErrorProtoFunc::execute(const List &)
{
  // toString()
  const char *s = "Error message.";

  return Completion(ReturnValue, String(s));
}

