// -*- c-basic-offset: 2 -*-
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
 *
 */

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "error_object.h"
//#include "debugger.h"

using namespace KJS;

// ------------------------------ ErrorPrototypeImp ----------------------------

// ECMA 15.9.4
ErrorPrototypeImp::ErrorPrototypeImp(ExecState *exec,
                                     ObjectPrototypeImp *objectProto,
                                     FunctionPrototypeImp *funcProto)
  : ObjectImp(Object(objectProto))
{
  Value protect(this);
  setInternalValue(Undefined());
  // The constructor will be added later in ErrorObjectImp's constructor

  put(exec, "name",     String("Error"), DontEnum);
  put(exec, "message",  String("Unknown error"), DontEnum);
  put(exec, "toString", Object(new ErrorProtoFuncImp(exec,funcProto)), DontEnum);
}

// ------------------------------ ErrorProtoFuncImp ----------------------------

ErrorProtoFuncImp::ErrorProtoFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  put(exec,"length",Number(0),DontDelete|ReadOnly|DontEnum);
}

bool ErrorProtoFuncImp::implementsCall() const
{
  return true;
}

Value ErrorProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &/*args*/)
{
  // toString()
  UString s = "Error";

  Value v = thisObj.get(exec,"name");
  if (v.type() != UndefinedType) {
    s = v.toString(exec);
  }

  v = thisObj.get(exec,"message");
  if (v.type() != UndefinedType) {
    s += ": "+v.toString(exec);
  }

  return String(s);
}

// ------------------------------ ErrorObjectImp -------------------------------

ErrorObjectImp::ErrorObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                               ErrorPrototypeImp *errorProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  // ECMA 15.11.3.1 Error.prototype
  put(exec, "prototype", Object(errorProto), DontEnum|DontDelete|ReadOnly);
  //put(exec, "name", String(n));
}

bool ErrorObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.9.3
Object ErrorObjectImp::construct(ExecState *exec, const List &args)
{
  Object proto = Object::dynamicCast(exec->interpreter()->builtinErrorPrototype());
  Object obj(new ObjectImp(proto));

  if (!args.isEmpty() && args[0].type() != UndefinedType) {
    obj.put(exec,"message", String(args[0].toString(exec)));
  }

  return obj;
}

bool ErrorObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.9.2
Value ErrorObjectImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  // "Error()" gives the sames result as "new Error()"
  return construct(exec,args);
}

// ------------------------------ NativeErrorPrototypeImp ----------------------

NativeErrorPrototypeImp::NativeErrorPrototypeImp(ExecState *exec, ErrorPrototypeImp *errorProto,
                                                 ErrorType et, UString name, UString message)
  : ObjectImp(Object(errorProto))
{
  Value protect(this);
  errType = et;
  put(exec,"name",String(name));
  put(exec,"message",String(message));
}

// ------------------------------ NativeErrorImp -------------------------------

const ClassInfo NativeErrorImp::info = {"Error", &InternalFunctionImp::info, 0, 0};

NativeErrorImp::NativeErrorImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                               const Object &prot)
  : InternalFunctionImp(funcProto), proto(0)
{
  Value protect(this);
  proto = static_cast<ObjectImp*>(prot.imp());

  put(exec,"length",Number(1),DontDelete|ReadOnly|DontEnum); // ECMA 15.11.7.5
  put(exec,"prototype",prot);
}

bool NativeErrorImp::implementsConstruct() const
{
  return true;
}

Object NativeErrorImp::construct(ExecState *exec, const List &args)
{
  Object obj(new ObjectImp(Object(proto)));
  if (args[0].type() != UndefinedType)
    obj.put(exec, "message", String(args[0].toString(exec)));
  return obj;
}

bool NativeErrorImp::implementsCall() const
{
  return true;
}

Value NativeErrorImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  return construct(exec,args);
}

void NativeErrorImp::mark()
{
  ObjectImp::mark();
  if (proto && !proto->marked())
    proto->mark();
}

