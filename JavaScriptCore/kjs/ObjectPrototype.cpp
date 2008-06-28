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

#include "operations.h"
#include "FunctionPrototype.h"

namespace KJS {

static JSValue* objectProtoFuncValueOf(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncHasOwnProperty(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncIsPrototypeOf(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncDefineGetter(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncDefineSetter(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncLookupGetter(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncLookupSetter(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncPropertyIsEnumerable(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* objectProtoFuncToLocaleString(ExecState*, JSObject*, JSValue*, const ArgList&);

ObjectPrototype::ObjectPrototype(ExecState* exec, FunctionPrototype* functionPrototype)
    : JSObject() // [[Prototype]] is null
{
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, objectProtoFuncToString), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toLocaleString, objectProtoFuncToLocaleString), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().valueOf, objectProtoFuncValueOf), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().hasOwnProperty, objectProtoFuncHasOwnProperty), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().propertyIsEnumerable, objectProtoFuncPropertyIsEnumerable), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().isPrototypeOf, objectProtoFuncIsPrototypeOf), DontEnum);

    // Mozilla extensions
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 2, exec->propertyNames().__defineGetter__, objectProtoFuncDefineGetter), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 2, exec->propertyNames().__defineSetter__, objectProtoFuncDefineSetter), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().__lookupGetter__, objectProtoFuncLookupGetter), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().__lookupSetter__, objectProtoFuncLookupSetter), DontEnum);
}


// ------------------------------ Functions --------------------------------

// ECMA 15.2.4.2, 15.2.4.4, 15.2.4.5, 15.2.4.7

JSValue* objectProtoFuncValueOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    return thisValue->toThisObject(exec);
}

JSValue* objectProtoFuncHasOwnProperty(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    return jsBoolean(thisValue->toThisObject(exec)->hasOwnProperty(exec, Identifier(exec, args[0]->toString(exec))));
}

JSValue* objectProtoFuncIsPrototypeOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    if (!args[0]->isObject())
        return jsBoolean(false);

    JSValue* v = static_cast<JSObject*>(args[0])->prototype();

    while (true) {
        if (!v->isObject())
            return jsBoolean(false);
        if (thisObj == v)
            return jsBoolean(true);
        v = static_cast<JSObject*>(v)->prototype();
    }
}

JSValue* objectProtoFuncDefineGetter(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    CallData callData;
    if (args[1]->getCallData(callData) == CallTypeNone)
        return throwError(exec, SyntaxError, "invalid getter usage");
    thisValue->toThisObject(exec)->defineGetter(exec, Identifier(exec, args[0]->toString(exec)), static_cast<JSObject*>(args[1]));
    return jsUndefined();
}

JSValue* objectProtoFuncDefineSetter(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    CallData callData;
    if (args[1]->getCallData(callData) == CallTypeNone)
        return throwError(exec, SyntaxError, "invalid setter usage");
    thisValue->toThisObject(exec)->defineSetter(exec, Identifier(exec, args[0]->toString(exec)), static_cast<JSObject*>(args[1]));
    return jsUndefined();
}

JSValue* objectProtoFuncLookupGetter(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    return thisValue->toThisObject(exec)->lookupGetter(exec, Identifier(exec, args[0]->toString(exec)));
}

JSValue* objectProtoFuncLookupSetter(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    return thisValue->toThisObject(exec)->lookupSetter(exec, Identifier(exec, args[0]->toString(exec)));
}

JSValue* objectProtoFuncPropertyIsEnumerable(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    return jsBoolean(thisValue->toThisObject(exec)->propertyIsEnumerable(exec, Identifier(exec, args[0]->toString(exec))));
}

JSValue* objectProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    return jsString(exec, thisValue->toThisObject(exec)->toString(exec));
}

JSValue* objectProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    return jsString(exec, "[object " + thisValue->toThisObject(exec)->className() + "]");
}

} // namespace KJS
