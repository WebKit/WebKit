/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "ObjectConstructor.h"

#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSFunction.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "ObjectPrototype.h"
#include "PropertyDescriptor.h"
#include "PropertyNameArray.h"
#include "PrototypeFunction.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ObjectConstructor);

static EncodedJSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyNames(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectConstructorKeys(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperty(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperties(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectConstructorCreate(ExecState*);

ObjectConstructor::ObjectConstructor(ExecState* exec, JSGlobalObject* globalObject, NonNullPassRefPtr<Structure> structure, ObjectPrototype* objectPrototype, Structure* prototypeFunctionStructure)
: InternalFunction(&exec->globalData(), globalObject, structure, Identifier(exec, "Object"))
{
    // ECMA 15.2.3.1
    putDirectWithoutTransition(exec->propertyNames().prototype, objectPrototype, DontEnum | DontDelete | ReadOnly);
    
    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
    
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 1, exec->propertyNames().getPrototypeOf, objectConstructorGetPrototypeOf), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 2, exec->propertyNames().getOwnPropertyDescriptor, objectConstructorGetOwnPropertyDescriptor), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 1, exec->propertyNames().getOwnPropertyNames, objectConstructorGetOwnPropertyNames), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 1, exec->propertyNames().keys, objectConstructorKeys), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 3, exec->propertyNames().defineProperty, objectConstructorDefineProperty), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 2, exec->propertyNames().defineProperties, objectConstructorDefineProperties), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 2, exec->propertyNames().create, objectConstructorCreate), DontEnum);
}

// ECMA 15.2.2
static ALWAYS_INLINE JSObject* constructObject(ExecState* exec, const ArgList& args)
{
    JSValue arg = args.at(0);
    if (arg.isUndefinedOrNull())
        return new (exec) JSObject(exec->lexicalGlobalObject()->emptyObjectStructure());
    return arg.toObject(exec);
}

static EncodedJSValue JSC_HOST_CALL constructWithObjectConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructObject(exec, args));
}

ConstructType ObjectConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithObjectConstructor;
    return ConstructTypeHost;
}

static EncodedJSValue JSC_HOST_CALL callObjectConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructObject(exec, args));
}

CallType ObjectConstructor::getCallData(CallData& callData)
{
    callData.native.function = callObjectConstructor;
    return CallTypeHost;
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, "Requested prototype of a value that is not an object."));
    return JSValue::encode(asObject(exec->argument(0))->prototype());
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, "Requested property descriptor of a value that is not an object."));
    UString propertyName = exec->argument(1).toString(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());
    JSObject* object = asObject(exec->argument(0));
    PropertyDescriptor descriptor;
    if (!object->getOwnPropertyDescriptor(exec, Identifier(exec, propertyName), descriptor))
        return JSValue::encode(jsUndefined());
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSObject* description = constructEmptyObject(exec);
    if (!descriptor.isAccessorDescriptor()) {
        description->putDirect(exec->propertyNames().value, descriptor.value() ? descriptor.value() : jsUndefined(), 0);
        description->putDirect(exec->propertyNames().writable, jsBoolean(descriptor.writable()), 0);
    } else {
        description->putDirect(exec->propertyNames().get, descriptor.getter() ? descriptor.getter() : jsUndefined(), 0);
        description->putDirect(exec->propertyNames().set, descriptor.setter() ? descriptor.setter() : jsUndefined(), 0);
    }
    
    description->putDirect(exec->propertyNames().enumerable, jsBoolean(descriptor.enumerable()), 0);
    description->putDirect(exec->propertyNames().configurable, jsBoolean(descriptor.configurable()), 0);

    return JSValue::encode(description);
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyNames(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, "Requested property names of a value that is not an object."));
    PropertyNameArray properties(exec);
    asObject(exec->argument(0))->getOwnPropertyNames(exec, properties, IncludeDontEnumProperties);
    JSArray* names = constructEmptyArray(exec);
    size_t numProperties = properties.size();
    for (size_t i = 0; i < numProperties; i++)
        names->push(exec, jsOwnedString(exec, properties[i].ustring()));
    return JSValue::encode(names);
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorKeys(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, "Requested keys of a value that is not an object."));
    PropertyNameArray properties(exec);
    asObject(exec->argument(0))->getOwnPropertyNames(exec, properties);
    JSArray* keys = constructEmptyArray(exec);
    size_t numProperties = properties.size();
    for (size_t i = 0; i < numProperties; i++)
        keys->push(exec, jsOwnedString(exec, properties[i].ustring()));
    return JSValue::encode(keys);
}

// ES5 8.10.5 ToPropertyDescriptor
static bool toPropertyDescriptor(ExecState* exec, JSValue in, PropertyDescriptor& desc)
{
    if (!in.isObject()) {
        throwError(exec, createTypeError(exec, "Property description must be an object."));
        return false;
    }
    JSObject* description = asObject(in);

    PropertySlot enumerableSlot(description);
    if (description->getPropertySlot(exec, exec->propertyNames().enumerable, enumerableSlot)) {
        desc.setEnumerable(enumerableSlot.getValue(exec, exec->propertyNames().enumerable).toBoolean(exec));
        if (exec->hadException())
            return false;
    }

    PropertySlot configurableSlot(description);
    if (description->getPropertySlot(exec, exec->propertyNames().configurable, configurableSlot)) {
        desc.setConfigurable(configurableSlot.getValue(exec, exec->propertyNames().configurable).toBoolean(exec));
        if (exec->hadException())
            return false;
    }

    JSValue value;
    PropertySlot valueSlot(description);
    if (description->getPropertySlot(exec, exec->propertyNames().value, valueSlot)) {
        desc.setValue(valueSlot.getValue(exec, exec->propertyNames().value));
        if (exec->hadException())
            return false;
    }

    PropertySlot writableSlot(description);
    if (description->getPropertySlot(exec, exec->propertyNames().writable, writableSlot)) {
        desc.setWritable(writableSlot.getValue(exec, exec->propertyNames().writable).toBoolean(exec));
        if (exec->hadException())
            return false;
    }

    PropertySlot getSlot(description);
    if (description->getPropertySlot(exec, exec->propertyNames().get, getSlot)) {
        JSValue get = getSlot.getValue(exec, exec->propertyNames().get);
        if (exec->hadException())
            return false;
        if (!get.isUndefined()) {
            CallData callData;
            if (getCallData(get, callData) == CallTypeNone) {
                throwError(exec, createTypeError(exec, "Getter must be a function."));
                return false;
            }
        } else
            get = JSValue();
        desc.setGetter(get);
    }

    PropertySlot setSlot(description);
    if (description->getPropertySlot(exec, exec->propertyNames().set, setSlot)) {
        JSValue set = setSlot.getValue(exec, exec->propertyNames().set);
        if (exec->hadException())
            return false;
        if (!set.isUndefined()) {
            CallData callData;
            if (getCallData(set, callData) == CallTypeNone) {
                throwError(exec, createTypeError(exec, "Setter must be a function."));
                return false;
            }
        } else
            set = JSValue();

        desc.setSetter(set);
    }

    if (!desc.isAccessorDescriptor())
        return true;

    if (desc.value()) {
        throwError(exec, createTypeError(exec, "Invalid property.  'value' present on property with getter or setter."));
        return false;
    }

    if (desc.writablePresent()) {
        throwError(exec, createTypeError(exec, "Invalid property.  'writable' present on property with getter or setter."));
        return false;
    }
    return true;
}

EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperty(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, "Properties can only be defined on Objects."));
    JSObject* O = asObject(exec->argument(0));
    UString propertyName = exec->argument(1).toString(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());
    PropertyDescriptor descriptor;
    if (!toPropertyDescriptor(exec, exec->argument(2), descriptor))
        return JSValue::encode(jsNull());
    ASSERT((descriptor.attributes() & (Getter | Setter)) || (!descriptor.isAccessorDescriptor()));
    ASSERT(!exec->hadException());
    O->defineOwnProperty(exec, Identifier(exec, propertyName), descriptor, true);
    return JSValue::encode(O);
}

static JSValue defineProperties(ExecState* exec, JSObject* object, JSObject* properties)
{
    PropertyNameArray propertyNames(exec);
    asObject(properties)->getOwnPropertyNames(exec, propertyNames);
    size_t numProperties = propertyNames.size();
    Vector<PropertyDescriptor> descriptors;
    MarkedArgumentBuffer markBuffer;
    for (size_t i = 0; i < numProperties; i++) {
        PropertySlot slot;
        JSValue prop = properties->get(exec, propertyNames[i]);
        if (exec->hadException())
            return jsNull();
        PropertyDescriptor descriptor;
        if (!toPropertyDescriptor(exec, prop, descriptor))
            return jsNull();
        descriptors.append(descriptor);
        // Ensure we mark all the values that we're accumulating
        if (descriptor.isDataDescriptor() && descriptor.value())
            markBuffer.append(descriptor.value());
        if (descriptor.isAccessorDescriptor()) {
            if (descriptor.getter())
                markBuffer.append(descriptor.getter());
            if (descriptor.setter())
                markBuffer.append(descriptor.setter());
        }
    }
    for (size_t i = 0; i < numProperties; i++) {
        object->defineOwnProperty(exec, propertyNames[i], descriptors[i], true);
        if (exec->hadException())
            return jsNull();
    }
    return object;
}

EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperties(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, "Properties can only be defined on Objects."));
    if (!exec->argument(1).isObject())
        return throwVMError(exec, createTypeError(exec, "Property descriptor list must be an Object."));
    return JSValue::encode(defineProperties(exec, asObject(exec->argument(0)), asObject(exec->argument(1))));
}

EncodedJSValue JSC_HOST_CALL objectConstructorCreate(ExecState* exec)
{
    if (!exec->argument(0).isObject() && !exec->argument(0).isNull())
        return throwVMError(exec, createTypeError(exec, "Object prototype may only be an Object or null."));
    JSObject* newObject = constructEmptyObject(exec);
    newObject->setPrototype(exec->argument(0));
    if (exec->argument(1).isUndefined())
        return JSValue::encode(newObject);
    if (!exec->argument(1).isObject())
        return throwVMError(exec, createTypeError(exec, "Property descriptor list must be an Object."));
    return JSValue::encode(defineProperties(exec, newObject, asObject(exec->argument(1))));
}

} // namespace JSC
