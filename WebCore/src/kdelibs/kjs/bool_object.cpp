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
#include "bool_object.h"
#include "error_object.h"

using namespace KJS;

BooleanObject::BooleanObject(const KJSO& funcProto, const KJSO &booleanProto)
  : ConstructorImp(funcProto, 1)
{
  // Boolean.prototype
  setPrototypeProperty(booleanProto);
}

// ECMA 15.6.1
Completion BooleanObject::execute(const List &args)
{
  Boolean b;

  if (args.isEmpty())
    b = Boolean(false);
  else
    b = args[0].toBoolean();

  return Completion(ReturnValue, b);
}

// ECMA 15.6.2
Object BooleanObject::construct(const List &args)
{
  Boolean b;
  if (args.size() > 0)
    b = args.begin()->toBoolean();
  else
    b = Boolean(false);

  return Object::create(BooleanClass, b);
}

// ECMA 15.6.4
BooleanPrototype::BooleanPrototype(const Object& proto)
  : ObjectImp(BooleanClass, Boolean(false), proto)
{
  // The constructor will be added later in BooleanObject's constructor
}

KJSO BooleanPrototype::get(const UString &p) const
{
  if (p == "toString")
    return Function(new BooleanProtoFunc(ToString));
  else if (p == "valueOf")
    return Function(new BooleanProtoFunc(ValueOf));
  else
    return Imp::get(p);
}

BooleanProtoFunc::BooleanProtoFunc(int i)
  : id(i)
{
}

// ECMA 15.6.4.2 + 15.6.4.3
Completion BooleanProtoFunc::execute(const List &)
{
  KJSO result;

  Object thisObj = Object::dynamicCast(thisValue());

  // no generic function. "this" has to be a Boolean object
  if (thisObj.isNull() || thisObj.getClass() != BooleanClass) {
    result = Error::create(TypeError);
    return Completion(ReturnValue, result);
  }

  // execute "toString()" or "valueOf()", respectively
  KJSO v = thisObj.internalValue();
  if (id == BooleanPrototype::ToString)
    result = v.toString();
  else
    result = v.toBoolean();

  return Completion(ReturnValue, result);
}
