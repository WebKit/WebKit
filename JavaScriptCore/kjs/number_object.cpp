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
#include "number_object.h"
#include "error_object.h"

#include "number_object.lut.h"

using namespace KJS;


// ------------------------------ NumberInstanceImp ----------------------------

const ClassInfo NumberInstanceImp::info = {"Number", 0, 0, 0};

NumberInstanceImp::NumberInstanceImp(const Object &proto)
  : ObjectImp(proto)
{
}
// ------------------------------ NumberPrototypeImp ---------------------------

// ECMA 15.7.4

NumberPrototypeImp::NumberPrototypeImp(ExecState *exec,
                                       ObjectPrototypeImp *objProto,
                                       FunctionPrototypeImp *funcProto)
  : NumberInstanceImp(Object(objProto))
{
  Value protect(this);
  setInternalValue(Number(0));

  // The constructor will be added later, after NumberObjectImp has been constructed

  put(exec,"toString",       Object(new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ToString,       1)), DontEnum);
  put(exec,"toLocaleString", Object(new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ToLocaleString, 0)), DontEnum);
  put(exec,"valueOf",        Object(new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ValueOf,        0)), DontEnum);
}


// ------------------------------ NumberProtoFuncImp ---------------------------

NumberProtoFuncImp::NumberProtoFuncImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}


bool NumberProtoFuncImp::implementsCall() const
{
  return true;
}

// ECMA 15.7.4.2 - 15.7.4.7
Value NumberProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &/*args*/)
{
  Value result;

  // no generic function. "this" has to be a Number object
  if (!thisObj.inherits(&NumberInstanceImp::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }

  // execute "toString()" or "valueOf()", respectively
  Value v = thisObj.internalValue();
  switch (id) {
  case ToString:
  case ToLocaleString: /* TODO */
    result = String(v.toString(exec));
    break;
  case ValueOf:
    result = Number(v.toNumber(exec));
    break;
  }

  return result;
}

// ------------------------------ NumberObjectImp ------------------------------

const ClassInfo NumberObjectImp::info = {"Number", &InternalFunctionImp::info, &numberTable, 0};
//const ClassInfo NumberObjectImp::info = {"Number", 0, &numberTable, 0};

/* Source for number_object.lut.h
@begin numberTable 5
  NaN			NumberObjectImp::NaNValue	DontEnum
  NEGATIVE_INFINITY	NumberObjectImp::NegInfinity	DontEnum
  POSITIVE_INFINITY	NumberObjectImp::PosInfinity	DontEnum
  MAX_VALUE		NumberObjectImp::MaxValue	DontEnum
  MIN_VALUE		NumberObjectImp::MinValue	DontEnum
@end
*/
NumberObjectImp::NumberObjectImp(ExecState *exec,
                                 FunctionPrototypeImp *funcProto,
                                 NumberPrototypeImp *numberProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  // Number.Prototype
  put(exec,"prototype", Value(numberProto),DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec,"length", Number(1), ReadOnly|DontDelete|DontEnum);
}

Value NumberObjectImp::get(ExecState *exec, const UString &propertyName) const
{
  return lookupGetValue<NumberObjectImp, InternalFunctionImp>( exec, propertyName, &numberTable, this );
}

Value NumberObjectImp::getValueProperty(ExecState *, int token) const
{
  // ECMA 15.7.3
  switch(token) {
  case NaNValue:
    return Number(NaN);
  case NegInfinity:
    return Number(-Inf);
  case PosInfinity:
    return Number(Inf);
  case MaxValue:
    return Number(1.7976931348623157E+308);
  case MinValue:
    return Number(5E-324);
  }
  return Null();
}

bool NumberObjectImp::implementsConstruct() const
{
  return true;
}


// ECMA 15.7.1
Object NumberObjectImp::construct(ExecState *exec, const List &args)
{
  Object proto = exec->interpreter()->builtinNumberPrototype();
  Object obj(new NumberInstanceImp(proto));

  Number n;
  if (args.isEmpty())
    n = Number(0);
  else
    n = args[0].toNumber(exec);

  obj.setInternalValue(n);

  return obj;
}

bool NumberObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.7.2
Value NumberObjectImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  if (args.isEmpty())
    return Number(0);
  else
    return Number(args[0].toNumber(exec));
}
