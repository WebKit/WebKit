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
    propertyNameIsNumber(false),
    prop(p)
{
}

Reference::Reference(ObjectImp *b, unsigned p)
  : base(b),
    propertyNameAsNumber(p),
    propertyNameIsNumber(true)
{
}

Identifier Reference::getPropertyName(ExecState *exec) const
{
  if (propertyNameIsNumber && prop.isNull())
    prop = Identifier::from(propertyNameAsNumber);
  return prop;
}

ValueImp *Reference::getValue(ExecState *exec) const 
{
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

bool Reference::deleteValue(ExecState *exec)
{
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
