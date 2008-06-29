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
#include "NativeErrorConstructor.h"

#include "ErrorInstance.h"
#include "FunctionPrototype.h"
#include "NativeErrorPrototype.h"

namespace KJS {

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
