/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
#include "error_object.h"

#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSValue.h"
#include "ObjectPrototype.h"
#include "operations.h"

namespace KJS {

// ------------------------------ ErrorInstance ----------------------------

const ClassInfo ErrorInstance::info = { "Error", 0, 0, 0 };

ErrorInstance::ErrorInstance(JSObject* prototype)
    : JSObject(prototype)
{
}

// ------------------------------ ErrorPrototype ----------------------------

static JSValue* errorProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);

// ECMA 15.9.4
ErrorPrototype::ErrorPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : ErrorInstance(objectPrototype)
{
    // The constructor will be added later in ErrorConstructor's constructor

    putDirect(exec->propertyNames().name, jsString(exec, "Error"), DontEnum);
    putDirect(exec->propertyNames().message, jsString(exec, "Unknown error"), DontEnum);

    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, errorProtoFuncToString), DontEnum);
}

JSValue* errorProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    UString s = "Error";

    JSValue* v = thisObj->get(exec, exec->propertyNames().name);
    if (!v->isUndefined())
        s = v->toString(exec);

    v = thisObj->get(exec, exec->propertyNames().message);
    if (!v->isUndefined()) {
        // Mozilla-compatible format.
        s += ": ";
        s += v->toString(exec);
    }

    return jsString(exec, s);
}

// ------------------------------ ErrorConstructor -------------------------------

ErrorConstructor::ErrorConstructor(ExecState* exec, FunctionPrototype* funcProto, ErrorPrototype* errorProto)
    : InternalFunction(funcProto, Identifier(exec, errorProto->classInfo()->className))
{
    // ECMA 15.11.3.1 Error.prototype
    putDirect(exec->propertyNames().prototype, errorProto, DontEnum|DontDelete|ReadOnly);
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), DontDelete|ReadOnly|DontEnum);
}

// ECMA 15.9.3
static ErrorInstance* constructError(ExecState* exec, const ArgList& args)
{
    ErrorInstance* obj = new (exec) ErrorInstance(exec->lexicalGlobalObject()->errorPrototype());
    if (!args[0]->isUndefined())
        obj->putDirect(exec->propertyNames().message, jsString(exec, args[0]->toString(exec)));
    return obj;
}

static JSObject* constructWithErrorConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructError(exec, args);
}

ConstructType ErrorConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithErrorConstructor;
    return ConstructTypeNative;
}

// ECMA 15.9.2
static JSValue* callErrorConstructor(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    // "Error()" gives the sames result as "new Error()"
    return constructError(exec, args);
}

CallType ErrorConstructor::getCallData(CallData& callData)
{
    callData.native.function = callErrorConstructor;
    return CallTypeNative;
}

// ------------------------------ NativeErrorPrototype ----------------------

NativeErrorPrototype::NativeErrorPrototype(ExecState* exec, ErrorPrototype* errorProto, const UString& name, const UString& message)
    : JSObject(errorProto)
{
    putDirect(exec->propertyNames().name, jsString(exec, name), 0);
    putDirect(exec->propertyNames().message, jsString(exec, message), 0);
}

// ------------------------------ NativeErrorConstructor -------------------------------

const ClassInfo NativeErrorConstructor::info = { "Function", &InternalFunction::info, 0, 0 };

NativeErrorConstructor::NativeErrorConstructor(ExecState* exec, FunctionPrototype* funcProto, NativeErrorPrototype* prot)
    : InternalFunction(funcProto, Identifier(exec, prot->getDirect(exec->propertyNames().name)->getString()))
    , proto(prot)
{
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), DontDelete|ReadOnly|DontEnum); // ECMA 15.11.7.5
    putDirect(exec->propertyNames().prototype, proto, DontDelete|ReadOnly|DontEnum);
}

ErrorInstance* NativeErrorConstructor::construct(ExecState* exec, const ArgList& args)
{
    ErrorInstance* object = new (exec) ErrorInstance(proto);
    if (!args[0]->isUndefined())
        object->putDirect(exec->propertyNames().message, jsString(exec, args[0]->toString(exec)));
    return object;
}

static JSObject* constructWithNativeErrorConstructor(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    return static_cast<NativeErrorConstructor*>(constructor)->construct(exec, args);
}

ConstructType NativeErrorConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithNativeErrorConstructor;
    return ConstructTypeNative;
}

static JSValue* callNativeErrorConstructor(ExecState* exec, JSObject* constructor, JSValue*, const ArgList& args)
{
    return static_cast<NativeErrorConstructor*>(constructor)->construct(exec, args);
}

CallType NativeErrorConstructor::getCallData(CallData& callData)
{
    callData.native.function = callNativeErrorConstructor;
    return CallTypeNative;
}

void NativeErrorConstructor::mark()
{
    JSObject::mark();
    if (proto && !proto->marked())
        proto->mark();
}

} // namespace KJS
