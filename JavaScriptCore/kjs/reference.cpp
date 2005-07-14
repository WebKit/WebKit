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

Reference::Reference(const Object& b, const Identifier& p)
  : base(b),
    baseIsValue(false),
    propertyNameIsNumber(false),
    prop(p)
{
}

Reference::Reference(const Object& b, unsigned p)
  : base(b),
    propertyNameAsNumber(p),
    baseIsValue(false),
    propertyNameIsNumber(true)
{
}

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

Reference::Reference(const Null& b, const Identifier& p)
  : base(b),
    baseIsValue(false),
    propertyNameIsNumber(false),
    prop(p)
{
}

Reference::Reference(const Null& b, unsigned p)
  : base(b),
    propertyNameAsNumber(p),
    baseIsValue(false),
    propertyNameIsNumber(true)
{
}

Reference Reference::makeValueReference(const Value& v)
{
  Reference valueRef;
  valueRef.base = v;
  valueRef.baseIsValue = true;
  return valueRef;
}

Value Reference::getBase(ExecState *exec) const
{
  if (baseIsValue) {
    Object err = Error::create(exec, ReferenceError, I18N_NOOP("Invalid reference base"));
    exec->setException(err);
    return err;
  }

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

Value Reference::getValue(ExecState *exec) const 
{
  if (baseIsValue) {
    return base;
  }

  ValueImp *o = base.imp();
  Type t = o ? o->dispatchType() : NullType;

  if (t == NullType) {
    UString m = I18N_NOOP("Can't find variable: ") + getPropertyName(exec).ustring();
    Object err = Error::create(exec, ReferenceError, m.ascii());
    exec->setException(err);
    return err;
  }

  if (t != ObjectType) {
    UString m = I18N_NOOP("Base is not an object");
    Object err = Error::create(exec, ReferenceError, m.ascii());
    exec->setException(err);
    return err;
  }

  if (propertyNameIsNumber)
    return static_cast<ObjectImp*>(o)->get(exec, propertyNameAsNumber);
  return static_cast<ObjectImp*>(o)->get(exec, prop);
}

void Reference::putValue(ExecState *exec, const Value &w)
{
  if (baseIsValue) {
    Object err = Error::create(exec, ReferenceError);
    exec->setException(err);
    return;
  }

#ifdef KJS_VERBOSE
  printInfo(exec,(UString("setting property ")+getPropertyName(exec).ustring()).cstring().c_str(),w);
#endif

  ValueImp *o = base.imp();
  Type t = o ? o->dispatchType() : NullType;

  if (t == NullType)
    o = exec->lexicalInterpreter()->globalObject().imp();

  if (propertyNameIsNumber)
    return static_cast<ObjectImp*>(o)->put(exec, propertyNameAsNumber, w);
  return static_cast<ObjectImp*>(o)->put(exec, prop, w);
}

bool Reference::deleteValue(ExecState *exec)
{
  if (baseIsValue) {
    Object err = Error::create(exec,ReferenceError);
    exec->setException(err);
    return false;
  }

  ValueImp *o = base.imp();
  Type t = o ? o->dispatchType() : NullType;

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
