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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "object_object.h"
#include "function_object.h"
#include <stdio.h>
#include <assert.h>

using namespace KJS;

// ------------------------------ ObjectPrototype --------------------------------

ObjectPrototype::ObjectPrototype(ExecState *exec,
                                       FunctionPrototype *funcProto)
  : JSObject() // [[Prototype]] is null
{
    putDirect(toStringPropertyName, new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::ToString,               0), DontEnum);
    putDirect(toLocaleStringPropertyName, new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::ToLocaleString,   0), DontEnum);
    putDirect(valueOfPropertyName, new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::ValueOf,                 0), DontEnum);
    putDirect("hasOwnProperty", new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::HasOwnProperty,             1), DontEnum);
    putDirect("propertyIsEnumerable", new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::PropertyIsEnumerable, 1), DontEnum);
}


// ------------------------------ ObjectProtoFunc --------------------------------

ObjectProtoFunc::ObjectProtoFunc(ExecState *exec,
                                       FunctionPrototype *funcProto,
                                       int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}


bool ObjectProtoFunc::implementsCall() const
{
  return true;
}

// ECMA 15.2.4.2, 15.2.4.4, 15.2.4.5, 15.2.4.7

JSValue *ObjectProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    switch (id) {
        case ValueOf:
            return thisObj;
        case HasOwnProperty: {
            PropertySlot slot;
            return jsBoolean(thisObj->getOwnPropertySlot(exec, Identifier(args[0]->toString(exec)), slot));
        }
        case PropertyIsEnumerable:
            return jsBoolean(thisObj->propertyIsEnumerable(exec, Identifier(args[0]->toString(exec))));
        case ToLocaleString:
            return jsString(thisObj->toString(exec));
        case ToString:
        default:
            return jsString("[object " + thisObj->className() + "]");
    }
}

// ------------------------------ ObjectObjectImp --------------------------------

ObjectObjectImp::ObjectObjectImp(ExecState *exec,
                                 ObjectPrototype *objProto,
                                 FunctionPrototype *funcProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.2.3.1
  putDirect(prototypePropertyName, objProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}


bool ObjectObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.2.2
JSObject *ObjectObjectImp::construct(ExecState *exec, const List &args)
{
  // if no arguments have been passed ...
  if (args.isEmpty()) {
    JSObject *proto = exec->lexicalInterpreter()->builtinObjectPrototype();
    JSObject *result(new JSObject(proto));
    return result;
  }

  JSValue *arg = *(args.begin());
  if (JSObject *obj = arg->getObject())
    return obj;

  switch (arg->type()) {
  case StringType:
  case BooleanType:
  case NumberType:
    return arg->toObject(exec);
  default:
    assert(!"unhandled switch case in ObjectConstructor");
  case NullType:
  case UndefinedType:
    return new JSObject(exec->lexicalInterpreter()->builtinObjectPrototype());
  }
}

bool ObjectObjectImp::implementsCall() const
{
  return true;
}

JSValue *ObjectObjectImp::callAsFunction(ExecState *exec, JSObject */*thisObj*/, const List &args)
{
  JSValue *result;

  List argList;
  // Construct a new Object
  if (args.isEmpty()) {
    result = construct(exec,argList);
  } else {
    JSValue *arg = args[0];
    if (arg->isUndefinedOrNull()) {
      argList.append(arg);
      result = construct(exec,argList);
    } else
      result = arg->toObject(exec);
  }
  return result;
}

