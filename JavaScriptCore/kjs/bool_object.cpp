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
#include "bool_object.h"
#include "error_object.h"

#include <assert.h>

using namespace KJS;

// ------------------------------ BooleanInstanceImp ---------------------------

const ClassInfo BooleanInstanceImp::info = {"Boolean", 0, 0, 0};

BooleanInstanceImp::BooleanInstanceImp(const Object &proto)
  : ObjectImp(proto)
{
}

// ------------------------------ BooleanPrototypeImp --------------------------

// ECMA 15.6.4

BooleanPrototypeImp::BooleanPrototypeImp(ExecState *exec,
                                         ObjectPrototypeImp *objectProto,
                                         FunctionPrototypeImp *funcProto)
  : BooleanInstanceImp(Object(objectProto))
{
  Value protect(this);
  // The constructor will be added later by InterpreterImp::InterpreterImp()

  put(exec,"toString", Object(new BooleanProtoFuncImp(exec,funcProto,BooleanProtoFuncImp::ToString,0)), DontEnum);
  put(exec,"valueOf",  Object(new BooleanProtoFuncImp(exec,funcProto,BooleanProtoFuncImp::ValueOf,0)),  DontEnum);
  setInternalValue(Boolean(false));
}


// ------------------------------ BooleanProtoFuncImp --------------------------

BooleanProtoFuncImp::BooleanProtoFuncImp(ExecState *exec,
                                         FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}


bool BooleanProtoFuncImp::implementsCall() const
{
  return true;
}


// ECMA 15.6.4.2 + 15.6.4.3
Value BooleanProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &/*args*/)
{
  // no generic function. "this" has to be a Boolean object
  if (!thisObj.inherits(&BooleanInstanceImp::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }

  // execute "toString()" or "valueOf()", respectively

  Value v = thisObj.internalValue();
  assert(!v.isNull());

  if (id == ToString)
    return String(v.toString(exec));
  else
    return Boolean(v.toBoolean(exec)); /* TODO: optimize for bool case */
}

// ------------------------------ BooleanObjectImp -----------------------------


BooleanObjectImp::BooleanObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                                   BooleanPrototypeImp *booleanProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  put(exec,"prototype", Object(booleanProto),DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec,"length", Number(1), ReadOnly|DontDelete|DontEnum);
}


bool BooleanObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.6.2
Object BooleanObjectImp::construct(ExecState *exec, const List &args)
{
  Object proto = exec->interpreter()->builtinBooleanPrototype();
  Object obj(new BooleanInstanceImp(proto));

  Boolean b;
  if (args.size() > 0)
    b = args.begin()->toBoolean(exec);
  else
    b = Boolean(false);

  obj.setInternalValue(b);

  return obj;
}

bool BooleanObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.6.1
Value BooleanObjectImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  if (args.isEmpty())
    return Boolean(false);
  else
    return Boolean(args[0].toBoolean(exec)); /* TODO: optimize for bool case */
}

