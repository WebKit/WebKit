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

#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "ObjectPrototype.h"
#include "PrototypeFunction.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ObjectConstructor);

static JSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState*, JSObject*, JSValue, const ArgList&);

ObjectConstructor::ObjectConstructor(ExecState* exec, PassRefPtr<Structure> structure, ObjectPrototype* objectPrototype, Structure* prototypeFunctionStructure)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, "Object"))
{
    // ECMA 15.2.3.1
    putDirectWithoutTransition(exec->propertyNames().prototype, objectPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);

    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().getPrototypeOf, objectConstructorGetPrototypeOf), DontEnum);
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

} // namespace JSC
