/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "ArrayConstructor.h"

#include "ArrayPrototype.h"
#include "Error.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Lookup.h"
#include "PrototypeFunction.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ArrayConstructor);
    
static JSValue JSC_HOST_CALL arrayConstructorIsArray(ExecState*, JSObject*, JSValue, const ArgList&);

ArrayConstructor::ArrayConstructor(ExecState* exec, NonNullPassRefPtr<Structure> structure, ArrayPrototype* arrayPrototype, Structure* prototypeFunctionStructure)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, arrayPrototype->classInfo()->className))
{
    // ECMA 15.4.3.1 Array.prototype
    putDirectWithoutTransition(exec->propertyNames().prototype, arrayPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);

    // ES5
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().isArray, arrayConstructorIsArray), DontEnum);
}

static JSObject* constructArrayWithSizeQuirk(ExecState* exec, const ArgList& args)
{
    // a single numeric argument denotes the array size (!)
    if (args.size() == 1 && args.at(0).isNumber()) {
        uint32_t n = args.at(0).toUInt32(exec);
        if (n != args.at(0).toNumber(exec))
            return throwError(exec, RangeError, "Array size is not a small enough positive integer.");
        return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure(), n);
    }

    // otherwise the array is constructed with the arguments in it
    return new (exec) JSArray(exec->lexicalGlobalObject()->arrayStructure(), args);
}

static JSObject* constructWithArrayConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructArrayWithSizeQuirk(exec, args);
}

// ECMA 15.4.2
ConstructType ArrayConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithArrayConstructor;
    return ConstructTypeHost;
}

static JSValue JSC_HOST_CALL callArrayConstructor(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    return constructArrayWithSizeQuirk(exec, args);
}

// ECMA 15.6.1
CallType ArrayConstructor::getCallData(CallData& callData)
{
    // equivalent to 'new Array(....)'
    callData.native.function = callArrayConstructor;
    return CallTypeHost;
}

JSValue JSC_HOST_CALL arrayConstructorIsArray(ExecState*, JSObject*, JSValue, const ArgList& args)
{
    return jsBoolean(args.at(0).inherits(&JSArray::info));
}

} // namespace JSC
