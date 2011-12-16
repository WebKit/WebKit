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
ASSERT_HAS_TRIVIAL_DESTRUCTOR(StringObject);

const ClassInfo StringObject::s_info = { "String", &JSWrapperObject::s_info, 0, 0, CREATE_METHOD_TABLE(StringObject) };

StringObject::StringObject(JSGlobalData& globalData, Structure* structure)
    : JSWrapperObject(globalData, structure)
{
}

void StringObject::finishCreation(JSGlobalData& globalData, JSString* string)
{
    Base::finishCreation(globalData);
    ASSERT(inherits(&s_info));
    setInternalValue(globalData, string);
}

bool StringObject::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->getStringPropertySlot(exec, propertyName, slot))
        return true;
    return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}
    
bool StringObject::getOwnPropertySlotByIndex(JSCell* cell, ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->getStringPropertySlot(exec, propertyName, slot))
        return true;    
    return JSObject::getOwnPropertySlot(thisObject, exec, Identifier::from(exec, propertyName), slot);
}

bool StringObject::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    StringObject* thisObject = jsCast<StringObject*>(object);
    if (thisObject->internalValue()->getStringPropertyDescriptor(exec, propertyName, descriptor))
        return true;    
    return JSObject::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
}

void StringObject::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length)
        return;
    JSObject::put(cell, exec, propertyName, value, slot);
}

bool StringObject::deleteProperty(JSCell* cell, ExecState* exec, const Identifier& propertyName)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (propertyName == exec->propertyNames().length)
        return false;
    bool isStrictUInt32;
    unsigned i = propertyName.toUInt32(isStrictUInt32);
    if (isStrictUInt32 && thisObject->internalValue()->canGetIndex(i))
        return false;
    return JSObject::deleteProperty(thisObject, exec, propertyName);
}

void StringObject::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    StringObject* thisObject = jsCast<StringObject*>(object);
    int size = thisObject->internalValue()->length();
    for (int i = 0; i < size; ++i)
        propertyNames.add(Identifier(exec, UString::number(i)));
    if (mode == IncludeDontEnumProperties)
        propertyNames.add(exec->propertyNames().length);
    return JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

} // namespace JSC
