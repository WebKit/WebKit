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

#include "Error.h"
#include "JSGlobalObject.h"
#include "Operations.h"
#include "PropertyNameArray.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(StringObject);

const ClassInfo StringObject::s_info = { "String", &JSWrapperObject::s_info, 0, 0, CREATE_METHOD_TABLE(StringObject) };

StringObject::StringObject(VM& vm, Structure* structure)
    : JSWrapperObject(vm, structure)
{
}

void StringObject::finishCreation(VM& vm, JSString* string)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    setInternalValue(vm, string);
}

bool StringObject::getOwnPropertySlot(JSObject* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->getStringPropertySlot(exec, propertyName, slot))
        return true;
    return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}
    
bool StringObject::getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    StringObject* thisObject = jsCast<StringObject*>(object);
    if (thisObject->internalValue()->getStringPropertySlot(exec, propertyName, slot))
        return true;    
    return JSObject::getOwnPropertySlot(thisObject, exec, Identifier::from(exec, propertyName), slot);
}

void StringObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
        if (slot.isStrictMode())
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return;
    }
    JSObject::put(cell, exec, propertyName, value, slot);
}

void StringObject::putByIndex(JSCell* cell, ExecState* exec, unsigned propertyName, JSValue value, bool shouldThrow)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->canGetIndex(propertyName)) {
        if (shouldThrow)
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return;
    }
    JSObject::putByIndex(cell, exec, propertyName, value, shouldThrow);
}

bool StringObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    StringObject* thisObject = jsCast<StringObject*>(object);

    if (propertyName == exec->propertyNames().length) {
        if (!object->isExtensible()) {
            if (throwException)
                exec->vm().throwException(exec, createTypeError(exec, ASCIILiteral("Attempting to define property on object that is not extensible.")));
            return false;
        }
        if (descriptor.configurablePresent() && descriptor.configurable()) {
            if (throwException)
                exec->vm().throwException(exec, createTypeError(exec, ASCIILiteral("Attempting to configurable attribute of unconfigurable property.")));
            return false;
        }
        if (descriptor.enumerablePresent() && descriptor.enumerable()) {
            if (throwException)
                exec->vm().throwException(exec, createTypeError(exec, ASCIILiteral("Attempting to change enumerable attribute of unconfigurable property.")));
            return false;
        }
        if (descriptor.isAccessorDescriptor()) {
            if (throwException)
                exec->vm().throwException(exec, createTypeError(exec, ASCIILiteral("Attempting to change access mechanism for an unconfigurable property.")));
            return false;
        }
        if (descriptor.writablePresent() && descriptor.writable()) {
            if (throwException)
                exec->vm().throwException(exec, createTypeError(exec, ASCIILiteral("Attempting to change writable attribute of unconfigurable property.")));
            return false;
        }
        if (!descriptor.value())
            return true;
        if (propertyName == exec->propertyNames().length && sameValue(exec, descriptor.value(), jsNumber(thisObject->internalValue()->length())))
            return true;
        if (throwException)
            exec->vm().throwException(exec, createTypeError(exec, ASCIILiteral("Attempting to change value of a readonly property.")));
        return false;
    }

    return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
}

bool StringObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (propertyName == exec->propertyNames().length)
        return false;
    unsigned i = propertyName.asIndex();
    if (thisObject->internalValue()->canGetIndex(i)) {
        ASSERT(i != PropertyName::NotAnIndex); // No need for an explicit check, the above test would always fail!
        return false;
    }
    return JSObject::deleteProperty(thisObject, exec, propertyName);
}

bool StringObject::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned i)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->canGetIndex(i))
        return false;
    return JSObject::deletePropertyByIndex(thisObject, exec, i);
}

void StringObject::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    StringObject* thisObject = jsCast<StringObject*>(object);
    int size = thisObject->internalValue()->length();
    for (int i = 0; i < size; ++i)
        propertyNames.add(Identifier::from(exec, i));
    if (mode == IncludeDontEnumProperties)
        propertyNames.add(exec->propertyNames().length);
    return JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

StringObject* constructString(VM& vm, JSGlobalObject* globalObject, JSValue string)
{
    StringObject* object = StringObject::create(vm, globalObject->stringObjectStructure());
    object->setInternalValue(vm, string);
    return object;
}

} // namespace JSC
