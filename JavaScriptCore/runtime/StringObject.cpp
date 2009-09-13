/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "StringObject.h"

#include "PropertyNameArray.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(StringObject);

const ClassInfo StringObject::info = { "String", 0, 0, 0 };

StringObject::StringObject(ExecState* exec, PassRefPtr<Structure> structure)
    : JSWrapperObject(structure)
{
    setInternalValue(jsEmptyString(exec));
}

StringObject::StringObject(PassRefPtr<Structure> structure, JSString* string)
    : JSWrapperObject(structure)
{
    setInternalValue(string);
}

StringObject::StringObject(ExecState* exec, PassRefPtr<Structure> structure, const UString& string)
    : JSWrapperObject(structure)
{
    setInternalValue(jsString(exec, string));
}

bool StringObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (internalValue()->getStringPropertySlot(exec, propertyName, slot))
        return true;
    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}
    
bool StringObject::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    if (internalValue()->getStringPropertySlot(exec, propertyName, slot))
        return true;    
    return JSObject::getOwnPropertySlot(exec, Identifier::from(exec, propertyName), slot);
}

bool StringObject::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    if (internalValue()->getStringPropertyDescriptor(exec, propertyName, descriptor))
        return true;    
    return JSObject::getOwnPropertyDescriptor(exec, propertyName, descriptor);
}

void StringObject::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length)
        return;
    JSObject::put(exec, propertyName, value, slot);
}

bool StringObject::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().length)
        return false;
    return JSObject::deleteProperty(exec, propertyName);
}

void StringObject::getOwnPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    int size = internalValue()->value().size();
    for (int i = 0; i < size; ++i)
        propertyNames.add(Identifier(exec, UString::from(i)));
    return JSObject::getOwnPropertyNames(exec, propertyNames);
}

} // namespace JSC
