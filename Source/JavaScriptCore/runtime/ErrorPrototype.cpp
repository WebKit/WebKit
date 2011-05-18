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
#include "StringRecursionChecker.h"
#include "UString.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ErrorPrototype);

static EncodedJSValue JSC_HOST_CALL errorProtoFuncToString(ExecState*);

}

#include "ErrorPrototype.lut.h"

namespace JSC {

const ClassInfo ErrorPrototype::s_info = { "Error", &ErrorInstance::s_info, 0, ExecState::errorPrototypeTable };

/* Source for ErrorPrototype.lut.h
@begin errorPrototypeTable
  toString          errorProtoFuncToString         DontEnum|Function 0
@end
*/

ASSERT_CLASS_FITS_IN_CELL(ErrorPrototype);

ErrorPrototype::ErrorPrototype(ExecState* exec, JSGlobalObject* globalObject, Structure* structure)
    : ErrorInstance(&exec->globalData(), structure)
{
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().name, jsNontrivialString(exec, "Error"), DontEnum);

    ASSERT(inherits(&s_info));
    putAnonymousValue(globalObject->globalData(), 0, globalObject);
}

bool ErrorPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<ErrorInstance>(exec, ExecState::errorPrototypeTable(exec), this, propertyName, slot);
}

bool ErrorPrototype::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticFunctionDescriptor<ErrorInstance>(exec, ExecState::errorPrototypeTable(exec), this, propertyName, descriptor);
}

// ------------------------------ Functions ---------------------------

EncodedJSValue JSC_HOST_CALL errorProtoFuncToString(ExecState* exec)
{
    JSObject* thisObj = exec->hostThisValue().toThisObject(exec);

    StringRecursionChecker checker(exec, thisObj);
    if (EncodedJSValue earlyReturnValue = checker.earlyReturnValue())
        return earlyReturnValue;

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
