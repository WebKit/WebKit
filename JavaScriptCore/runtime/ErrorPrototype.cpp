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
#include "ErrorPrototype.h"

#include "JSFunction.h"
#include "JSString.h"
#include "JSStringBuilder.h"
#include "ObjectPrototype.h"
#include "PrototypeFunction.h"
#include "UString.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ErrorPrototype);

static EncodedJSValue JSC_HOST_CALL errorProtoFuncToString(ExecState*);

// ECMA 15.9.4
ErrorPrototype::ErrorPrototype(ExecState* exec, JSGlobalObject* globalObject, NonNullPassRefPtr<Structure> structure, Structure* prototypeFunctionStructure)
    : ErrorInstance(&exec->globalData(), structure)
{
    // The constructor will be added later in ErrorConstructor's constructor

    putDirectWithoutTransition(exec->propertyNames().name, jsNontrivialString(exec, "Error"), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, globalObject, prototypeFunctionStructure, 0, exec->propertyNames().toString, errorProtoFuncToString), DontEnum);
}

EncodedJSValue JSC_HOST_CALL errorProtoFuncToString(ExecState* exec)
{
    JSObject* thisObj = exec->hostThisValue().toThisObject(exec);
    JSValue name = thisObj->get(exec, exec->propertyNames().name);
    JSValue message = thisObj->get(exec, exec->propertyNames().message);

    // Mozilla-compatible format.

    if (!name.isUndefined()) {
        if (!message.isUndefined())
            return JSValue::encode(jsMakeNontrivialString(exec, name.toString(exec), ": ", message.toString(exec)));
        return JSValue::encode(jsNontrivialString(exec, name.toString(exec)));
    }
    if (!message.isUndefined())
        return JSValue::encode(jsMakeNontrivialString(exec, "Error: ", message.toString(exec)));
    return JSValue::encode(jsNontrivialString(exec, "Error"));
}

} // namespace JSC
