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
#include "object.h"
#include "operations.h"
#include "types.h"
#include "value.h"

namespace KJS {

// ------------------------------ ErrorInstance ----------------------------

const ClassInfo ErrorInstance::info = { "Error", 0, 0 };

ErrorInstance::ErrorInstance(JSObject* prototype)
    : JSObject(prototype)
{
}

// ------------------------------ ErrorPrototype ----------------------------

// ECMA 15.9.4
ErrorPrototype::ErrorPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : ErrorInstance(objectPrototype)
{
    // The constructor will be added later in ErrorObjectImp's constructor

    putDirect(exec->propertyNames().name, jsString("Error"), DontEnum);
    putDirect(exec->propertyNames().message, jsString("Unknown error"), DontEnum);

    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, errorProtoFuncToString), DontEnum);
}

JSValue* errorProtoFuncToString(ExecState* exec, JSObject* thisObj, const List&)
{
    UString s = "Error";

    JSValue* v = thisObj->get(exec, exec->propertyNames().name);
    if (!v->isUndefined())
        s = v->toString(exec);

    v = thisObj->get(exec, exec->propertyNames().message);
    if (!v->isUndefined())
        // Mozilla compatible format
        s += ": " + v->toString(exec);

    return jsString(s);
}

// ------------------------------ ErrorObjectImp -------------------------------

ErrorObjectImp::ErrorObjectImp(ExecState* exec, FunctionPrototype* funcProto, ErrorPrototype* errorProto)
    : InternalFunctionImp(funcProto, errorProto->classInfo()->className)
{
    // ECMA 15.11.3.1 Error.prototype
    putDirect(exec->propertyNames().prototype, errorProto, DontEnum|DontDelete|ReadOnly);
    putDirect(exec->propertyNames().length, jsNumber(1), DontDelete|ReadOnly|DontEnum);
}

bool ErrorObjectImp::implementsConstruct() const
{
    return true;
}

// ECMA 15.9.3
JSObject* ErrorObjectImp::construct(ExecState* exec, const List& args)
{
    JSObject* proto = static_cast<JSObject*>(exec->lexicalGlobalObject()->errorPrototype());
    JSObject* imp = new ErrorInstance(proto);
    JSObject* obj(imp);

    if (!args[0]->isUndefined())
        imp->putDirect(exec->propertyNames().message, jsString(args[0]->toString(exec)));

    return obj;
}

// ECMA 15.9.2
JSValue* ErrorObjectImp::callAsFunction(ExecState* exec, JSObject* /*thisObj*/, const List& args)
{
    // "Error()" gives the sames result as "new Error()"
    return construct(exec, args);
}

// ------------------------------ NativeErrorPrototype ----------------------

NativeErrorPrototype::NativeErrorPrototype(ExecState* exec, ErrorPrototype* errorProto, const UString& name, const UString& message)
    : JSObject(errorProto)
{
    putDirect(exec->propertyNames().name, jsString(name), 0);
    putDirect(exec->propertyNames().message, jsString(message), 0);
}

// ------------------------------ NativeErrorImp -------------------------------

const ClassInfo NativeErrorImp::info = { "Function", &InternalFunctionImp::info, 0 };

NativeErrorImp::NativeErrorImp(ExecState* exec, FunctionPrototype* funcProto, NativeErrorPrototype* prot)
    : InternalFunctionImp(funcProto, Identifier(prot->getDirect(exec->propertyNames().name)->getString()))
    , proto(prot)
{
    putDirect(exec->propertyNames().length, jsNumber(1), DontDelete|ReadOnly|DontEnum); // ECMA 15.11.7.5
    putDirect(exec->propertyNames().prototype, proto, DontDelete|ReadOnly|DontEnum);
}

bool NativeErrorImp::implementsConstruct() const
{
    return true;
}

JSObject* NativeErrorImp::construct(ExecState* exec, const List& args)
{
    JSObject* imp = new ErrorInstance(proto);
    JSObject* obj(imp);
    if (!args[0]->isUndefined())
        imp->putDirect(exec->propertyNames().message, jsString(args[0]->toString(exec)));
    return obj;
}

JSValue* NativeErrorImp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    return construct(exec, args);
}

void NativeErrorImp::mark()
{
    JSObject::mark();
    if (proto && !proto->marked())
        proto->mark();
}

} // namespace KJS
