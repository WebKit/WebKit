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
#include "number_object.h"

using namespace KJS;

NumberObject::NumberObject(const Object& funcProto, const Object &numProto)
  : ConstructorImp(funcProto, 1)
{
  // Number.Prototype
  setPrototypeProperty(numProto);
}

// ECMA 15.7.3
KJSO NumberObject::get(const UString &p) const
{
  double d;

  if (p == "NaN")
    d = NaN;
  else if (p == "NEGATIVE_INFINITY")
    d = -Inf;
  else if (p == "POSITIVE_INFINITY")
    d = Inf;
  else
    return Imp::get(p);

  return Number(d);
}

// ECMA 15.7.1
Completion NumberObject::execute(const List &args)
{
  Number n;
  if (args.isEmpty())
    n = Number(0);
  else
    n = args[0].toNumber();

  return Completion(ReturnValue, n);
}

// ECMA 15.7.2
Object NumberObject::construct(const List &args)
{
  Number n;
  if (args.isEmpty())
    n = Number(0);
  else
    n = args[0].toNumber();

  return Object::create(NumberClass, n);
}

class NumberProtoFunc : public InternalFunctionImp {
public:
  NumberProtoFunc(int i) : id (i) { }
  Completion execute(const List &);
  enum { ToString, ToLocaleString, ValueOf };
private:
  int id;
};

// ECMA 15.7.4.2 - 15.7.4.7
Completion NumberProtoFunc::execute(const List &)
{
  KJSO result;

  Object thisObj = Object::dynamicCast(thisValue());

  // no generic function. "this" has to be a Number object
  if (thisObj.isNull() || thisObj.getClass() != NumberClass) {
    result = Error::create(TypeError);
    return Completion(ReturnValue, result);
  }

  // execute "toString()" or "valueOf()", respectively
  KJSO v = thisObj.internalValue();
  switch (id) {
  case ToString:
  case ToLocaleString: /* TODO */
    result = v.toString();
    break;
  case ValueOf:
    result = v.toNumber();
    break;
  }

  return Completion(ReturnValue, result);
}

// ECMA 15.7.4
NumberPrototype::NumberPrototype(const Object& proto)
  : ObjectImp(NumberClass, Number(0), proto)
{
  // The constructor will be added later in NumberObject's constructor
}

KJSO NumberPrototype::get(const UString &p) const
{
  int t;
  if (p == "toString")
    t = NumberProtoFunc::ToString;
  else if (p == "toLocaleString")
    t = NumberProtoFunc::ToLocaleString;
  else if (p == "valueOf")
    t = NumberProtoFunc::ValueOf;
  else
    return Imp::get(p);

  return Function(new NumberProtoFunc(t));
}
