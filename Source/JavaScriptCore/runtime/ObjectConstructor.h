/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "InternalFunction.h"
#include "JSGlobalObject.h"
#include "ObjectPrototype.h"

namespace JSC {

EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptors(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertySymbols(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyNames(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL objectConstructorKeys(JSGlobalObject*, CallFrame*);

class ObjectPrototype;

class ObjectConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static ObjectConstructor* create(VM& vm, JSGlobalObject* globalObject, Structure* structure, ObjectPrototype* objectPrototype)
    {
        ObjectConstructor* constructor = new (NotNull, allocateCell<ObjectConstructor>(vm.heap)) ObjectConstructor(vm, structure);
        constructor->finishCreation(vm, globalObject, objectPrototype);
        return constructor;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

private:
    ObjectConstructor(VM&, Structure*);
    void finishCreation(VM&, JSGlobalObject*, ObjectPrototype*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(ObjectConstructor, InternalFunction);

inline JSFinalObject* constructEmptyObject(VM& vm, Structure* structure)
{
    return JSFinalObject::create(vm, structure);
}

inline JSFinalObject* constructEmptyObject(JSGlobalObject* globalObject, JSObject* prototype, unsigned inlineCapacity)
{
    VM& vm = getVM(globalObject);
    StructureCache& structureCache = vm.structureCache;
    Structure* structure = structureCache.emptyObjectStructureForPrototype(globalObject, prototype, inlineCapacity);
    return constructEmptyObject(vm, structure);
}

inline JSFinalObject* constructEmptyObject(JSGlobalObject* globalObject, JSObject* prototype)
{
    return constructEmptyObject(globalObject, prototype, JSFinalObject::defaultInlineCapacity());
}

inline JSFinalObject* constructEmptyObject(JSGlobalObject* globalObject)
{
    return constructEmptyObject(getVM(globalObject), globalObject->objectStructureForObjectConstructor());
}

inline JSObject* constructObject(JSGlobalObject* globalObject, JSValue arg)
{
    if (arg.isUndefinedOrNull())
        return constructEmptyObject(globalObject, globalObject->objectPrototype());
    return arg.toObject(globalObject);
}

// https://tc39.es/ecma262/#sec-frompropertydescriptor
inline JSObject* constructObjectFromPropertyDescriptor(JSGlobalObject* globalObject, const PropertyDescriptor& descriptor)
{
    VM& vm = getVM(globalObject);
    JSObject* result = constructEmptyObject(globalObject);

    if (descriptor.value())
        result->putDirect(vm, vm.propertyNames->value, descriptor.value());
    if (descriptor.writablePresent())
        result->putDirect(vm, vm.propertyNames->writable, jsBoolean(descriptor.writable()));
    if (descriptor.getterPresent())
        result->putDirect(vm, vm.propertyNames->get, descriptor.getter());
    if (descriptor.setterPresent())
        result->putDirect(vm, vm.propertyNames->set, descriptor.setter());
    if (descriptor.enumerablePresent())
        result->putDirect(vm, vm.propertyNames->enumerable, jsBoolean(descriptor.enumerable()));
    if (descriptor.configurablePresent())
        result->putDirect(vm, vm.propertyNames->configurable, jsBoolean(descriptor.configurable()));

    return result;
}


JS_EXPORT_PRIVATE JSObject* objectConstructorFreeze(JSGlobalObject*, JSObject*);
JS_EXPORT_PRIVATE JSObject* objectConstructorSeal(JSGlobalObject*, JSObject*);
JSValue objectConstructorGetOwnPropertyDescriptor(JSGlobalObject*, JSObject*, const Identifier&);
JSValue objectConstructorGetOwnPropertyDescriptors(JSGlobalObject*, JSObject*);
JSArray* ownPropertyKeys(JSGlobalObject*, JSObject*, PropertyNameMode, DontEnumPropertiesMode);
bool toPropertyDescriptor(JSGlobalObject*, JSValue, PropertyDescriptor&);

EncodedJSValue JSC_HOST_CALL objectConstructorIs(JSGlobalObject*, CallFrame*);

} // namespace JSC
