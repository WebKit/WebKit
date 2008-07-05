/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Arguments.h"

#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSActivation.h"

namespace KJS {

const ClassInfo Arguments::info = { "Arguments", 0, 0, 0 };

// ECMA 10.1.8
Arguments::Arguments(ExecState* exec, JSFunction* func, const ArgList& args, JSActivation* act)
    : JSObject(exec->lexicalGlobalObject()->objectPrototype())
    , _activationObject(act)
    , indexToNameMap(func, args)
{
    putDirect(exec->propertyNames().callee, func, DontEnum);
    putDirect(exec, exec->propertyNames().length, args.size(), DontEnum);
  
    int i = 0;
    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++it, ++i) {
        Identifier name = Identifier::from(exec, i);
        if (!indexToNameMap.isMapped(name))
            putDirect(name, *it, DontEnum);
    }
}

void Arguments::mark() 
{
  JSObject::mark();
  if (_activationObject && !_activationObject->marked())
    _activationObject->mark();
}

JSValue* Arguments::mappedIndexGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
  Arguments* thisObj = static_cast<Arguments*>(slot.slotBase());
  return thisObj->_activationObject->get(exec, thisObj->indexToNameMap[propertyName]);
}

bool Arguments::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (indexToNameMap.isMapped(propertyName)) {
    slot.setCustom(this, mappedIndexGetter);
    return true;
  }

  return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void Arguments::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    if (indexToNameMap.isMapped(propertyName))
        _activationObject->put(exec, indexToNameMap[propertyName], value);
    else
        JSObject::put(exec, propertyName, value);
}

bool Arguments::deleteProperty(ExecState* exec, const Identifier& propertyName) 
{
  if (indexToNameMap.isMapped(propertyName)) {
    indexToNameMap.unMap(exec, propertyName);
    return true;
  } else {
    return JSObject::deleteProperty(exec, propertyName);
  }
}

} // namespace KJS
