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
 *  $Id$
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
  Value protect(this);
  put(exec,"toString", Object(new ObjectProtoFuncImp(exec,funcProto,ObjectProtoFuncImp::ToString, 0)), DontEnum);
  put(exec,"valueOf",  Object(new ObjectProtoFuncImp(exec,funcProto,ObjectProtoFuncImp::ValueOf,  0)), DontEnum);
}


// ------------------------------ ObjectProtoFuncImp --------------------------------

ObjectProtoFuncImp::ObjectProtoFuncImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto,
                                       int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}


bool ObjectProtoFuncImp::implementsCall() const
{
  return true;
}

// ECMA 15.2.4.2 + 15.2.4.3

Value ObjectProtoFuncImp::call(ExecState */*exec*/, Object &thisObj, const List &/*args*/)
{
  if (id == ValueOf)
    return thisObj;
  else /* ToString */
    return String("[object "+thisObj.className()+"]");
}

// ------------------------------ ObjectObjectImp --------------------------------

ObjectObjectImp::ObjectObjectImp(ExecState *exec,
                                 ObjectPrototypeImp *objProto,
                                 FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  // ECMA 15.2.3.1
  put(exec,"prototype", Object(objProto), DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec,"length", Number(1), ReadOnly|DontDelete|DontEnum);
}


bool ObjectObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.2.2
Object ObjectObjectImp::construct(ExecState *exec, const List &args)
{
  // if no arguments have been passed ...
  if (args.isEmpty()) {
    Object proto = exec->interpreter()->builtinObjectPrototype();
    Object result(new ObjectImp(proto));
    return result;
  }

  Value arg = *(args.begin());
  Object obj = Object::dynamicCast(arg);
  if (!obj.isNull()) {
    return obj;
  }

  switch (arg.type()) {
  case StringType:
  case BooleanType:
  case NumberType:
    return arg.toObject(exec);
  default:
    assert(!"unhandled switch case in ObjectConstructor");
  case NullType:
  case UndefinedType:
    Object proto = exec->interpreter()->builtinObjectPrototype();
    return Object(new ObjectImp(proto));
  }
}

bool ObjectObjectImp::implementsCall() const
{
  return true;
}

Value ObjectObjectImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  Value result;

  List argList;
  // Construct a new Object
  if (args.isEmpty()) {
    result = construct(exec,argList);
  } else {
    Value arg = args[0];
    if (arg.type() == NullType || arg.type() == UndefinedType) {
      argList.append(arg);
      result = construct(exec,argList);
    } else
      result = arg.toObject(exec);
  }
  return result;
}

