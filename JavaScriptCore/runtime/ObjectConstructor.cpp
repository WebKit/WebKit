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
#include "JSFunction.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "ObjectPrototype.h"
#include "PropertyDescriptor.h"
#include "PropertyNameArray.h"
#include "PrototypeFunction.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ObjectConstructor);

static JSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL objectConstructorKeys(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL objectConstructorDefineProperty(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL objectConstructorDefineProperties(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL objectConstructorCreate(ExecState*, JSObject*, JSValue, const ArgList&);

ObjectConstructor::ObjectConstructor(ExecState* exec, NonNullPassRefPtr<Structure> structure, ObjectPrototype* objectPrototype, Structure* prototypeFunctionStructure)
: InternalFunction(&exec->globalData(), structure, Identifier(exec, "Object"))
{
    // ECMA 15.2.3.1
    putDirectWithoutTransition(exec->propertyNames().prototype, objectPrototype, DontEnum | DontDelete | ReadOnly);
    
    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);
    
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().getPrototypeOf, objectConstructorGetPrototypeOf), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 2, exec->propertyNames().getOwnPropertyDescriptor, objectConstructorGetOwnPropertyDescriptor), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().keys, objectConstructorKeys), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 3, exec->propertyNames().defineProperty, objectConstructorDefineProperty), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 2, exec->propertyNames().defineProperties, objectConstructorDefineProperties), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 2, exec->propertyNames().create, objectConstructorCreate), DontEnum);
}

// ECMA 15.2.2
static ALWAYS_INLINE JSObject* constructObject(ExecState* exec, const ArgList& args)
{
    JSValue arg = args.at(0);
    if (arg.isUndefinedOrNull())
        return new (exec) JSObject(exec->lexicalGlobalObject()->emptyObjectStructure());
    return arg.toObject(exec);
}

static JSObject* constructWithObjectConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructObject(exec, args);
}

ConstructType ObjectConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithObjectConstructor;
    return ConstructTypeHost;
}

static JSValue JSC_HOST_CALL callObjectConstructor(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    return constructObject(exec, args);
}

CallType ObjectConstructor::getCallData(CallData& callData)
{
    callData.native.function = callObjectConstructor;
    return CallTypeHost;
}

JSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (!args.at(0).isObject())
        return throwError(exec, TypeError, "Requested prototype of a value that is not an object.");
    return asObject(args.at(0))->prototype();
}

JSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (!args.at(0).isObject())
        return throwError(exec, TypeError, "Requested property descriptor of a value that is not an object.");
    UString propertyName = args.at(1).toString(exec);
    if (exec->hadException())
        return jsNull();
    JSObject* object = asObject(args.at(0));
    PropertyDescriptor descriptor;
    if (!object->getOwnPropertyDescriptor(exec, Identifier(exec, propertyName), descriptor))
        return jsUndefined();
    if (exec->hadException())
        return jsUndefined();

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

    return description;
}

JSValue JSC_HOST_CALL objectConstructorKeys(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (!args.at(0).isObject())
        return throwError(exec, TypeError, "Requested keys of a value that is not an object.");
    PropertyNameArray properties(exec);
    asObject(args.at(0))->getOwnPropertyNames(exec, properties);
    JSArray* keys = constructEmptyArray(exec);
    size_t numProperties = properties.size();
    for (size_t i = 0; i < numProperties; i++)
        keys->push(exec, jsOwnedString(exec, properties[i].ustring()));
    return keys;
}

// ES5 8.10.5 ToPropertyDescriptor
static bool toPropertyDescriptor(ExecState* exec, JSValue in, PropertyDescriptor& desc)
{
    if (!in.isObject()) {
        throwError(exec, TypeError, "Property description must be an object.");
        return false;
    }
    JSObject* description = asObject(in);

    PropertySlot enumerableSlot;
    if (description->getPropertySlot(exec, exec->propertyNames().enumerable, enumerableSlot)) {
        desc.setEnumerable(enumerableSlot.getValue(exec, exec->propertyNames().enumerable).toBoolean(exec));
        if (exec->hadException())
            return false;
    }

    PropertySlot configurableSlot;
    if (description->getPropertySlot(exec, exec->propertyNames().configurable, configurableSlot)) {
        desc.setConfigurable(configurableSlot.getValue(exec, exec->propertyNames().configurable).toBoolean(exec));
        if (exec->hadException())
            return false;
    }

    JSValue value;
    PropertySlot valueSlot;
    if (description->getPropertySlot(exec, exec->propertyNames().value, valueSlot)) {
        desc.setValue(valueSlot.getValue(exec, exec->propertyNames().value));
        if (exec->hadException())
            return false;
    }

    PropertySlot writableSlot;
    if (description->getPropertySlot(exec, exec->propertyNames().writable, writableSlot)) {
        desc.setWritable(writableSlot.getValue(exec, exec->propertyNames().writable).toBoolean(exec));
        if (exec->hadException())
            return false;
    }

    PropertySlot getSlot;
    if (description->getPropertySlot(exec, exec->propertyNames().get, getSlot)) {
        JSValue get = getSlot.getValue(exec, exec->propertyNames().get);
        if (exec->hadException())
            return false;
        if (!get.isUndefined()) {
            CallData callData;
            if (get.getCallData(callData) == CallTypeNone) {
                throwError(exec, TypeError, "Getter must be a function.");
                return false;
            }
        } else
            get = JSValue();
        desc.setGetter(get);
    }

    PropertySlot setSlot;
    if (description->getPropertySlot(exec, exec->propertyNames().set, setSlot)) {
        JSValue set = setSlot.getValue(exec, exec->propertyNames().set);
        if (exec->hadException())
            return false;
        if (!set.isUndefined()) {
            CallData callData;
            if (set.getCallData(callData) == CallTypeNone) {
                throwError(exec, TypeError, "Setter must be a function.");
                return false;
            }
        } else
            set = JSValue();

        desc.setSetter(set);
    }

    if (!desc.isAccessorDescriptor())
        return true;

    if (desc.value()) {
        throwError(exec, TypeError, "Invalid property.  'value' present on property with getter or setter.");
        return false;
    }

    if (desc.writablePresent()) {
        throwError(exec, TypeError, "Invalid property.  'writable' present on property with getter or setter.");
        return false;
    }
    return true;
}

JSValue JSC_HOST_CALL objectConstructorDefineProperty(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (!args.at(0).isObject())
        return throwError(exec, TypeError, "Properties can only be defined on Objects.");
    JSObject* O = asObject(args.at(0));
    UString propertyName = args.at(1).toString(exec);
    if (exec->hadException())
        return jsNull();
    PropertyDescriptor descriptor;
    if (!toPropertyDescriptor(exec, args.at(2), descriptor))
        return jsNull();
    ASSERT((descriptor.attributes() & (Getter | Setter)) || (!descriptor.isAccessorDescriptor()));
    ASSERT(!exec->hadException());
    O->defineOwnProperty(exec, Identifier(exec, propertyName), descriptor, true);
    return O;
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

JSValue JSC_HOST_CALL objectConstructorDefineProperties(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (!args.at(0).isObject())
        return throwError(exec, TypeError, "Properties can only be defined on Objects.");
    if (!args.at(1).isObject())
        return throwError(exec, TypeError, "Property descriptor list must be an Object.");
    return defineProperties(exec, asObject(args.at(0)), asObject(args.at(1)));
}

JSValue JSC_HOST_CALL objectConstructorCreate(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    if (!args.at(0).isObject() && !args.at(0).isNull())
        return throwError(exec, TypeError, "Object prototype may only be an Object or null.");
    JSObject* newObject = constructEmptyObject(exec);
    newObject->setPrototype(args.at(0));
    if (args.at(1).isUndefined())
        return newObject;
    if (!args.at(1).isObject())
        return throwError(exec, TypeError, "Property descriptor list must be an Object.");
    return defineProperties(exec, newObject, asObject(args.at(1)));
}

} // namespace JSC
