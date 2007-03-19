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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "bool_object.h"

#include "operations.h"
#include "error_object.h"

using namespace KJS;

// ------------------------------ BooleanInstance ---------------------------

const ClassInfo BooleanInstance::info = {"Boolean", 0, 0, 0};

BooleanInstance::BooleanInstance(JSObject *proto)
  : JSWrapperObject(proto)
{
}

// ------------------------------ BooleanPrototype --------------------------

// ECMA 15.6.4

BooleanPrototype::BooleanPrototype(ExecState* exec, ObjectPrototype* objectProto, FunctionPrototype* funcProto)
  : BooleanInstance(objectProto)
{
  // The constructor will be added later by Interpreter::Interpreter()

  putDirectFunction(new BooleanProtoFunc(exec, funcProto, BooleanProtoFunc::ToString, 0, exec->propertyNames().toString), DontEnum);
  putDirectFunction(new BooleanProtoFunc(exec, funcProto, BooleanProtoFunc::ValueOf, 0, exec->propertyNames().valueOf),  DontEnum);
  setInternalValue(jsBoolean(false));
}


// ------------------------------ BooleanProtoFunc --------------------------

BooleanProtoFunc::BooleanProtoFunc(ExecState* exec, FunctionPrototype* funcProto, int i, int len, const Identifier& name)
  : InternalFunctionImp(funcProto, name)
  , id(i)
{
  putDirect(exec->propertyNames().length, len, DontDelete|ReadOnly|DontEnum);
}


// ECMA 15.6.4.2 + 15.6.4.3
JSValue *BooleanProtoFunc::callAsFunction(ExecState* exec, JSObject *thisObj, const List &/*args*/)
{
  // no generic function. "this" has to be a Boolean object
  if (!thisObj->inherits(&BooleanInstance::info))
    return throwError(exec, TypeError);

  // execute "toString()" or "valueOf()", respectively

  JSValue *v = static_cast<BooleanInstance*>(thisObj)->internalValue();
  assert(v);

  if (id == ToString)
    return jsString(v->toString(exec));
  return jsBoolean(v->toBoolean(exec)); /* TODO: optimize for bool case */
}

// ------------------------------ BooleanObjectImp -----------------------------


BooleanObjectImp::BooleanObjectImp(ExecState* exec, FunctionPrototype* funcProto, BooleanPrototype* booleanProto)
  : InternalFunctionImp(funcProto)
{
  putDirect(exec->propertyNames().prototype, booleanProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}


bool BooleanObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.6.2
JSObject *BooleanObjectImp::construct(ExecState *exec, const List &args)
{
  BooleanInstance *obj(new BooleanInstance(exec->lexicalInterpreter()->builtinBooleanPrototype()));

  bool b;
  if (args.size() > 0)
    b = args.begin()->toBoolean(exec);
  else
    b = false;

  obj->setInternalValue(jsBoolean(b));

  return obj;
}

// ECMA 15.6.1
JSValue *BooleanObjectImp::callAsFunction(ExecState *exec, JSObject *, const List &args)
{
  if (args.isEmpty())
    return jsBoolean(false);
  else
    return jsBoolean(args[0]->toBoolean(exec)); /* TODO: optimize for bool case */
}

