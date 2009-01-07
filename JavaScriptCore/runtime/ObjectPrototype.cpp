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
#include "ObjectPrototype.h"

#include "Error.h"
#include "JSString.h"
#include "PrototypeFunction.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ObjectPrototype);

static JSValuePtr objectProtoFuncValueOf(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncHasOwnProperty(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncIsPrototypeOf(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncDefineGetter(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncDefineSetter(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncLookupGetter(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncLookupSetter(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncPropertyIsEnumerable(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr objectProtoFuncToLocaleString(ExecState*, JSObject*, JSValuePtr, const ArgList&);

ObjectPrototype::ObjectPrototype(ExecState* exec, PassRefPtr<Structure> stucture, Structure* prototypeFunctionStructure)
    : JSObject(stucture)
{
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 0, exec->propertyNames().toString, objectProtoFuncToString), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 0, exec->propertyNames().toLocaleString, objectProtoFuncToLocaleString), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 0, exec->propertyNames().valueOf, objectProtoFuncValueOf), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 1, exec->propertyNames().hasOwnProperty, objectProtoFuncHasOwnProperty), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 1, exec->propertyNames().propertyIsEnumerable, objectProtoFuncPropertyIsEnumerable), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 1, exec->propertyNames().isPrototypeOf, objectProtoFuncIsPrototypeOf), DontEnum);

    // Mozilla extensions
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 2, exec->propertyNames().__defineGetter__, objectProtoFuncDefineGetter), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 2, exec->propertyNames().__defineSetter__, objectProtoFuncDefineSetter), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 1, exec->propertyNames().__lookupGetter__, objectProtoFuncLookupGetter), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 1, exec->propertyNames().__lookupSetter__, objectProtoFuncLookupSetter), DontEnum);
}

// ------------------------------ Functions --------------------------------

// ECMA 15.2.4.2, 15.2.4.4, 15.2.4.5, 15.2.4.7

JSValuePtr objectProtoFuncValueOf(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    return thisValue->toThisObject(exec);
}

JSValuePtr objectProtoFuncHasOwnProperty(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    return jsBoolean(thisValue->toThisObject(exec)->hasOwnProperty(exec, Identifier(exec, args.at(exec, 0)->toString(exec))));
}

JSValuePtr objectProtoFuncIsPrototypeOf(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    if (!args.at(exec, 0)->isObject())
        return jsBoolean(false);

    JSValuePtr v = asObject(args.at(exec, 0))->prototype();

    while (true) {
        if (!v->isObject())
            return jsBoolean(false);
        if (v == thisObj)
            return jsBoolean(true);
        v = asObject(v)->prototype();
    }
}

JSValuePtr objectProtoFuncDefineGetter(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    CallData callData;
    if (args.at(exec, 1)->getCallData(callData) == CallTypeNone)
        return throwError(exec, SyntaxError, "invalid getter usage");
    thisValue->toThisObject(exec)->defineGetter(exec, Identifier(exec, args.at(exec, 0)->toString(exec)), asObject(args.at(exec, 1)));
    return jsUndefined();
}

JSValuePtr objectProtoFuncDefineSetter(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    CallData callData;
    if (args.at(exec, 1)->getCallData(callData) == CallTypeNone)
        return throwError(exec, SyntaxError, "invalid setter usage");
    thisValue->toThisObject(exec)->defineSetter(exec, Identifier(exec, args.at(exec, 0)->toString(exec)), asObject(args.at(exec, 1)));
    return jsUndefined();
}

JSValuePtr objectProtoFuncLookupGetter(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    return thisValue->toThisObject(exec)->lookupGetter(exec, Identifier(exec, args.at(exec, 0)->toString(exec)));
}

JSValuePtr objectProtoFuncLookupSetter(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    return thisValue->toThisObject(exec)->lookupSetter(exec, Identifier(exec, args.at(exec, 0)->toString(exec)));
}

JSValuePtr objectProtoFuncPropertyIsEnumerable(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    return jsBoolean(thisValue->toThisObject(exec)->propertyIsEnumerable(exec, Identifier(exec, args.at(exec, 0)->toString(exec))));
}

JSValuePtr objectProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    return thisValue->toThisJSString(exec);
}

JSValuePtr objectProtoFuncToString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    return jsNontrivialString(exec, "[object " + thisValue->toThisObject(exec)->className() + "]");
}

} // namespace JSC
