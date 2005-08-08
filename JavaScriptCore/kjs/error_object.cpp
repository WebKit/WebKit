// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "error_object.h"
//#include "debugger.h"

using namespace KJS;

// ------------------------------ ErrorInstanceImp ----------------------------

const ClassInfo ErrorInstanceImp::info = {"Error", 0, 0, 0};

ErrorInstanceImp::ErrorInstanceImp(ObjectImp *proto)
: ObjectImp(proto)
{
}

// ------------------------------ ErrorPrototypeImp ----------------------------

// ECMA 15.9.4
ErrorPrototypeImp::ErrorPrototypeImp(ExecState *exec,
                                     ObjectPrototypeImp *objectProto,
                                     FunctionPrototypeImp *funcProto)
  : ObjectImp(objectProto)
{
  setInternalValue(Undefined());
  // The constructor will be added later in ErrorObjectImp's constructor

  put(exec, namePropertyName,     String("Error"), DontEnum);
  put(exec, messagePropertyName,  String("Unknown error"), DontEnum);
  putDirect(toStringPropertyName, new ErrorProtoFuncImp(exec,funcProto), DontEnum);
}

// ------------------------------ ErrorProtoFuncImp ----------------------------

ErrorProtoFuncImp::ErrorProtoFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  putDirect(lengthPropertyName, jsZero(), DontDelete|ReadOnly|DontEnum);
}

bool ErrorProtoFuncImp::implementsCall() const
{
  return true;
}

ValueImp *ErrorProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &/*args*/)
{
  // toString()
  UString s = "Error";

  ValueImp *v = thisObj->get(exec, namePropertyName);
  if (!v->isUndefined()) {
    s = v->toString(exec);
  }

  v = thisObj->get(exec, messagePropertyName);
  if (!v->isUndefined()) {
    s += ": " + v->toString(exec); // Mozilla compatible format
  }

  return String(s);
}

// ------------------------------ ErrorObjectImp -------------------------------

ErrorObjectImp::ErrorObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                               ErrorPrototypeImp *errorProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.11.3.1 Error.prototype
  putDirect(prototypePropertyName, errorProto, DontEnum|DontDelete|ReadOnly);
  putDirect(lengthPropertyName, jsOne(), DontDelete|ReadOnly|DontEnum);
  //putDirect(namePropertyName, String(n));
}

bool ErrorObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.9.3
ObjectImp *ErrorObjectImp::construct(ExecState *exec, const List &args)
{
  ObjectImp *proto = static_cast<ObjectImp *>(exec->lexicalInterpreter()->builtinErrorPrototype());
  ObjectImp *imp = new ErrorInstanceImp(proto);
  ObjectImp *obj(imp);

  if (!args[0]->isUndefined())
    imp->putDirect(messagePropertyName, jsString(args[0]->toString(exec)));

  return obj;
}

bool ErrorObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.9.2
ValueImp *ErrorObjectImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  // "Error()" gives the sames result as "new Error()"
  return construct(exec,args);
}

// ------------------------------ NativeErrorPrototypeImp ----------------------

NativeErrorPrototypeImp::NativeErrorPrototypeImp(ExecState *exec, ErrorPrototypeImp *errorProto,
                                                 ErrorType et, UString name, UString message)
  : ObjectImp(errorProto)
{
  errType = et;
  putDirect(namePropertyName, jsString(name), 0);
  putDirect(messagePropertyName, jsString(message), 0);
}

// ------------------------------ NativeErrorImp -------------------------------

const ClassInfo NativeErrorImp::info = {"Function", &InternalFunctionImp::info, 0, 0};

NativeErrorImp::NativeErrorImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                               ObjectImp *prot)
  : InternalFunctionImp(funcProto), proto(0)
{
  proto = static_cast<ObjectImp*>(prot);

  putDirect(lengthPropertyName, jsOne(), DontDelete|ReadOnly|DontEnum); // ECMA 15.11.7.5
  putDirect(prototypePropertyName, proto, DontDelete|ReadOnly|DontEnum);
}

bool NativeErrorImp::implementsConstruct() const
{
  return true;
}

ObjectImp *NativeErrorImp::construct(ExecState *exec, const List &args)
{
  ObjectImp *imp = new ErrorInstanceImp(proto);
  ObjectImp *obj(imp);
  if (!args[0]->isUndefined())
    imp->putDirect(messagePropertyName, jsString(args[0]->toString(exec)));
  return obj;
}

bool NativeErrorImp::implementsCall() const
{
  return true;
}

ValueImp *NativeErrorImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  return construct(exec,args);
}

void NativeErrorImp::mark()
{
  ObjectImp::mark();
  if (proto && !proto->marked())
    proto->mark();
}
