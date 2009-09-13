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

ObjectConstructor::ObjectConstructor(ExecState* exec, PassRefPtr<Structure> structure, ObjectPrototype* objectPrototype, Structure* prototypeFunctionStructure)
: InternalFunction(&exec->globalData(), structure, Identifier(exec, "Object"))
{
    // ECMA 15.2.3.1
    putDirectWithoutTransition(exec->propertyNames().prototype, objectPrototype, DontEnum | DontDelete | ReadOnly);
    
    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);
    
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().getPrototypeOf, objectConstructorGetPrototypeOf), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 2, exec->propertyNames().getOwnPropertyDescriptor, objectConstructorGetOwnPropertyDescriptor), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().keys, objectConstructorKeys), DontEnum);
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
    ASSERT(descriptor.isValid());

    JSObject* description = constructEmptyObject(exec);
    if (!descriptor.hasAccessors()) {
        description->putDirect(exec->propertyNames().value, descriptor.value(), 0);
        description->putDirect(exec->propertyNames().writable, jsBoolean(descriptor.writable()), 0);
    } else {
        description->putDirect(exec->propertyNames().get, descriptor.getter(), 0);
        description->putDirect(exec->propertyNames().set, descriptor.setter(), 0);
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

} // namespace JSC
