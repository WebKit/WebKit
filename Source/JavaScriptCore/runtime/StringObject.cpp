/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "PropertyNameArray.h"
#include "TypeError.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(StringObject);

const ClassInfo StringObject::s_info = { "String", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(StringObject) };

StringObject::StringObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void StringObject::finishCreation(VM& vm, JSString* string)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    setInternalValue(vm, string);
    ASSERT_WITH_MESSAGE(type() == StringObjectType || type() == DerivedStringObjectType, "Instance inheriting StringObject should have DerivedStringObjectType");
}

bool StringObject::getOwnPropertySlot(JSObject* cell, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->getStringPropertySlot(globalObject, propertyName, slot))
        return true;
    return JSObject::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}
    
bool StringObject::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    StringObject* thisObject = jsCast<StringObject*>(object);
    if (thisObject->internalValue()->getStringPropertySlot(globalObject, propertyName, slot))
        return true;    
    VM& vm = globalObject->vm();
    return JSObject::getOwnPropertySlot(thisObject, globalObject, Identifier::from(vm, propertyName), slot);
}

bool StringObject::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StringObject* thisObject = jsCast<StringObject*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject))) 
        RELEASE_AND_RETURN(scope, ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode()));

    if (propertyName == vm.propertyNames->length)
        return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
    if (Optional<uint32_t> index = parseIndex(propertyName)) 
        RELEASE_AND_RETURN(scope, putByIndex(cell, globalObject, index.value(), value, slot.isStrictMode()));
    RELEASE_AND_RETURN(scope, JSObject::put(cell, globalObject, propertyName, value, slot));
}

bool StringObject::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->canGetIndex(propertyName))
        return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);
    RELEASE_AND_RETURN(scope, JSObject::putByIndex(cell, globalObject, propertyName, value, shouldThrow));
}

static bool isStringOwnProperty(JSGlobalObject* globalObject, StringObject* object, PropertyName propertyName)
{
    VM& vm = globalObject->vm();
    if (propertyName == vm.propertyNames->length)
        return true;
    if (Optional<uint32_t> index = parseIndex(propertyName)) {
        if (object->internalValue()->canGetIndex(index.value()))
            return true;
    }
    return false;
}

bool StringObject::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    StringObject* thisObject = jsCast<StringObject*>(object);

    if (isStringOwnProperty(globalObject, thisObject, propertyName)) {
        // The current PropertyDescriptor is always
        // PropertyDescriptor{[[Value]]: value, [[Writable]]: false, [[Enumerable]]: true, [[Configurable]]: false}.
        // This ensures that any property descriptor cannot change the existing one.
        // Here, simply return the result of validateAndApplyPropertyDescriptor.
        // https://tc39.github.io/ecma262/#sec-string-exotic-objects-getownproperty-p
        PropertyDescriptor current;
        bool isCurrentDefined = thisObject->getOwnPropertyDescriptor(globalObject, propertyName, current);
        EXCEPTION_ASSERT(!scope.exception() == isCurrentDefined);
        RETURN_IF_EXCEPTION(scope, false);
        bool isExtensible = thisObject->isExtensible(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        RELEASE_AND_RETURN(scope, validateAndApplyPropertyDescriptor(globalObject, nullptr, propertyName, isExtensible, descriptor, isCurrentDefined, current, throwException));
    }

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, throwException));
}

bool StringObject::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (propertyName == vm.propertyNames->length)
        return false;
    Optional<uint32_t> index = parseIndex(propertyName);
    if (index && thisObject->internalValue()->canGetIndex(index.value()))
        return false;
    return JSObject::deleteProperty(thisObject, globalObject, propertyName, slot);
}

bool StringObject::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned i)
{
    StringObject* thisObject = jsCast<StringObject*>(cell);
    if (thisObject->internalValue()->canGetIndex(i))
        return false;
    return JSObject::deletePropertyByIndex(thisObject, globalObject, i);
}

void StringObject::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    StringObject* thisObject = jsCast<StringObject*>(object);
    if (propertyNames.includeStringProperties()) {
        int size = thisObject->internalValue()->length();
        for (int i = 0; i < size; ++i)
            propertyNames.add(Identifier::from(vm, i));
    }
    return JSObject::getOwnPropertyNames(thisObject, globalObject, propertyNames, mode);
}

void StringObject::getOwnNonIndexPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    StringObject* thisObject = jsCast<StringObject*>(object);
    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->length);
    return JSObject::getOwnNonIndexPropertyNames(thisObject, globalObject, propertyNames, mode);
}

String StringObject::toStringName(const JSObject*, JSGlobalObject*)
{
    return "String"_s;
}

StringObject* constructString(VM& vm, JSGlobalObject* globalObject, JSValue string)
{
    StringObject* object = StringObject::create(vm, globalObject->stringObjectStructure());
    object->setInternalValue(vm, string);
    return object;
}

} // namespace JSC
