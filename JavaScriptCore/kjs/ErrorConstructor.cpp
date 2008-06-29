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
#include "ErrorConstructor.h"

#include "ErrorInstance.h"
#include "ErrorPrototype.h"
#include "FunctionPrototype.h"
#include "JSGlobalObject.h"
#include "JSString.h"

namespace KJS {

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

} // namespace KJS
