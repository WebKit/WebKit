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

#include "config.h"
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "bool_object.h"
#include "error_object.h"

#include <assert.h>

using namespace KJS;

// ------------------------------ BooleanInstance ---------------------------

const ClassInfo BooleanInstance::info = {"Boolean", 0, 0, 0};

BooleanInstance::BooleanInstance(JSObject *proto)
  : JSObject(proto)
{
}

// ------------------------------ BooleanPrototype --------------------------

// ECMA 15.6.4

BooleanPrototype::BooleanPrototype(ExecState *exec,
                                         ObjectPrototype *objectProto,
                                         FunctionPrototype *funcProto)
  : BooleanInstance(objectProto)
{
  // The constructor will be added later by InterpreterImp::InterpreterImp()

  putDirect(toStringPropertyName, new BooleanProtoFunc(exec,funcProto,BooleanProtoFunc::ToString,0), DontEnum);
  putDirect(valueOfPropertyName,  new BooleanProtoFunc(exec,funcProto,BooleanProtoFunc::ValueOf,0),  DontEnum);
  setInternalValue(jsBoolean(false));
}


// ------------------------------ BooleanProtoFunc --------------------------

BooleanProtoFunc::BooleanProtoFunc(ExecState *exec,
                                         FunctionPrototype *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}


bool BooleanProtoFunc::implementsCall() const
{
  return true;
}


// ECMA 15.6.4.2 + 15.6.4.3
JSValue *BooleanProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &/*args*/)
{
  // no generic function. "this" has to be a Boolean object
  if (!thisObj->inherits(&BooleanInstance::info))
    return throwError(exec, TypeError);

  // execute "toString()" or "valueOf()", respectively

  JSValue *v = thisObj->internalValue();
  assert(v);

  if (id == ToString)
    return jsString(v->toString(exec));
  return jsBoolean(v->toBoolean(exec)); /* TODO: optimize for bool case */
}

// ------------------------------ BooleanObjectImp -----------------------------


BooleanObjectImp::BooleanObjectImp(ExecState *exec, FunctionPrototype *funcProto,
                                   BooleanPrototype *booleanProto)
  : InternalFunctionImp(funcProto)
{
  putDirect(prototypePropertyName, booleanProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}


bool BooleanObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.6.2
JSObject *BooleanObjectImp::construct(ExecState *exec, const List &args)
{
  JSObject *obj(new BooleanInstance(exec->lexicalInterpreter()->builtinBooleanPrototype()));

  bool b;
  if (args.size() > 0)
    b = args.begin()->toBoolean(exec);
  else
    b = false;

  obj->setInternalValue(jsBoolean(b));

  return obj;
}

bool BooleanObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.6.1
JSValue *BooleanObjectImp::callAsFunction(ExecState *exec, JSObject */*thisObj*/, const List &args)
{
  if (args.isEmpty())
    return jsBoolean(false);
  else
    return jsBoolean(args[0]->toBoolean(exec)); /* TODO: optimize for bool case */
}

