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

// ------------------------------ ObjectPrototypeImp --------------------------------

ObjectPrototypeImp::ObjectPrototypeImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto)
  : ObjectImp() // [[Prototype]] is Null()
{
    putDirect(toStringPropertyName, new ObjectProtoFuncImp(exec,funcProto,ObjectProtoFuncImp::ToString,            0), DontEnum);
    putDirect(toLocaleStringPropertyName, new ObjectProtoFuncImp(exec,funcProto,ObjectProtoFuncImp::ToLocaleString,0), DontEnum);
    putDirect(valueOfPropertyName,  new ObjectProtoFuncImp(exec,funcProto,ObjectProtoFuncImp::ValueOf,             0), DontEnum);
    putDirect("hasOwnProperty", new ObjectProtoFuncImp(exec,funcProto,ObjectProtoFuncImp::HasOwnProperty,          1), DontEnum);
}


// ------------------------------ ObjectProtoFuncImp --------------------------------

ObjectProtoFuncImp::ObjectProtoFuncImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto,
                                       int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}


bool ObjectProtoFuncImp::implementsCall() const
{
  return true;
}

// ECMA 15.2.4.2, 15.2.4.4, 15.2.4.5

ValueImp *ObjectProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    switch (id) {
        case ValueOf:
            return thisObj;
        case HasOwnProperty:
            // Same as the in operator but without checking the prototype
            return jsBoolean(thisObj->hasOwnProperty(exec, Identifier(args[0]->toString(exec))));
        case ToLocaleString:
            return jsString(thisObj->toString(exec));
        case ToString:
        default:
            return String("[object " + thisObj->className() + "]");
    }
}

// ------------------------------ ObjectObjectImp --------------------------------

ObjectObjectImp::ObjectObjectImp(ExecState *exec,
                                 ObjectPrototypeImp *objProto,
                                 FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.2.3.1
  putDirect(prototypePropertyName, objProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsOne(), ReadOnly|DontDelete|DontEnum);
}


bool ObjectObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.2.2
ObjectImp *ObjectObjectImp::construct(ExecState *exec, const List &args)
{
  // if no arguments have been passed ...
  if (args.isEmpty()) {
    ObjectImp *proto = exec->lexicalInterpreter()->builtinObjectPrototype();
    ObjectImp *result(new ObjectImp(proto));
    return result;
  }

  ValueImp *arg = *(args.begin());
  if (ObjectImp *obj = arg->getObject())
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
    return new ObjectImp(exec->lexicalInterpreter()->builtinObjectPrototype());
  }
}

bool ObjectObjectImp::implementsCall() const
{
  return true;
}

ValueImp *ObjectObjectImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  ValueImp *result;

  List argList;
  // Construct a new Object
  if (args.isEmpty()) {
    result = construct(exec,argList);
  } else {
    ValueImp *arg = args[0];
    if (arg->isUndefinedOrNull()) {
      argList.append(arg);
      result = construct(exec,argList);
    } else
      result = arg->toObject(exec);
  }
  return result;
}

