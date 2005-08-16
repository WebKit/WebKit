// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "reference.h"
#include "internal.h"

namespace KJS {

// ------------------------------ Reference ------------------------------------

Reference::Reference(ObjectImp *b, const Identifier& p)
  : base(b),
    baseIsValue(false),
    propertyNameIsNumber(false),
    prop(p)
{
}

Reference::Reference(ObjectImp *b, unsigned p)
  : base(b),
    propertyNameAsNumber(p),
    baseIsValue(false),
    propertyNameIsNumber(true)
{
}

Reference::Reference(const Identifier& p)
  : base(jsNull()),
    baseIsValue(false),
    propertyNameIsNumber(false),
    prop(p)
{
}

Reference::Reference(unsigned p)
  : base(jsNull()),
    propertyNameAsNumber(p),
    baseIsValue(false),
    propertyNameIsNumber(true)
{
}

Reference Reference::makeValueReference(ValueImp *v)
{
  Reference valueRef;
  valueRef.base = v;
  valueRef.baseIsValue = true;
  return valueRef;
}

ValueImp *Reference::getBase(ExecState *exec) const
{
  if (baseIsValue)
    return throwError(exec, ReferenceError, "Invalid reference base");
  return base;
}

Identifier Reference::getPropertyName(ExecState *exec) const
{
  if (baseIsValue) {
    // the spec wants a runtime error here. But getValue() and putValue()
    // will catch this case on their own earlier. When returning a Null
    // string we should be on the safe side.
    return Identifier();
  }

  if (propertyNameIsNumber && prop.isNull())
    prop = Identifier::from(propertyNameAsNumber);
  return prop;
}

ValueImp *Reference::getValue(ExecState *exec) const 
{
  if (baseIsValue)
    return base;

  ValueImp *o = base;
  if (!o || !o->isObject()) {
    if (!o || o->isNull())
      return throwError(exec, ReferenceError, "Can't find variable: " + getPropertyName(exec).ustring());
    return throwError(exec, ReferenceError, "Base is not an object");
  }

  if (propertyNameIsNumber)
    return static_cast<ObjectImp*>(o)->get(exec, propertyNameAsNumber);
  return static_cast<ObjectImp*>(o)->get(exec, prop);
}

void Reference::putValue(ExecState *exec, ValueImp *w)
{
  if (baseIsValue) {
    throwError(exec, ReferenceError);
    return;
  }

#ifdef KJS_VERBOSE
  printInfo(exec,(UString("setting property ")+getPropertyName(exec).ustring()).cstring().c_str(),w);
#endif

  ValueImp *o = base;
  Type t = o ? o->type() : NullType;

  if (t == NullType)
    o = exec->lexicalInterpreter()->globalObject();

  if (propertyNameIsNumber)
    return static_cast<ObjectImp*>(o)->put(exec, propertyNameAsNumber, w);
  return static_cast<ObjectImp*>(o)->put(exec, prop, w);
}

bool Reference::deleteValue(ExecState *exec)
{
  if (baseIsValue) {
    throwError(exec, ReferenceError);
    return false;
  }

  ValueImp *o = base;
  Type t = o ? o->type() : NullType;

  // The spec doesn't mention what to do if the base is null... just return true
  if (t != ObjectType) {
    assert(t == NullType);
    return true;
  }

  if (propertyNameIsNumber)
    return static_cast<ObjectImp*>(o)->deleteProperty(exec,propertyNameAsNumber);
  return static_cast<ObjectImp*>(o)->deleteProperty(exec,prop);
}

}
