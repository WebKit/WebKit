/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008, 2011 Apple Inc. All rights reserved.
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
#include "JSFunction.h"
#include "JSString.h"
#include "JSStringBuilder.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL objectProtoFuncValueOf(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncHasOwnProperty(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncIsPrototypeOf(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineGetter(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineSetter(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupGetter(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupSetter(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncPropertyIsEnumerable(ExecState*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncToLocaleString(ExecState*);

}

#include "ObjectPrototype.lut.h"

namespace JSC {

const ClassInfo ObjectPrototype::s_info = { "Object", &JSNonFinalObject::s_info, 0, ExecState::objectPrototypeTable };

/* Source for ObjectPrototype.lut.h
@begin objectPrototypeTable
  toString              objectProtoFuncToString                 DontEnum|Function 0
  toLocaleString        objectProtoFuncToLocaleString           DontEnum|Function 0
  valueOf               objectProtoFuncValueOf                  DontEnum|Function 0
  hasOwnProperty        objectProtoFuncHasOwnProperty           DontEnum|Function 1
  propertyIsEnumerable  objectProtoFuncPropertyIsEnumerable     DontEnum|Function 1
  isPrototypeOf         objectProtoFuncIsPrototypeOf            DontEnum|Function 1
  __defineGetter__      objectProtoFuncDefineGetter             DontEnum|Function 2
  __defineSetter__      objectProtoFuncDefineSetter             DontEnum|Function 2
  __lookupGetter__      objectProtoFuncLookupGetter             DontEnum|Function 1
  __lookupSetter__      objectProtoFuncLookupSetter             DontEnum|Function 1
@end
*/

ASSERT_CLASS_FITS_IN_CELL(ObjectPrototype);

ObjectPrototype::ObjectPrototype(ExecState* exec, JSGlobalObject* globalObject, Structure* stucture)
    : JSNonFinalObject(exec->globalData(), stucture)
    , m_hasNoPropertiesWithUInt32Names(true)
{
    ASSERT(inherits(&s_info));
    putAnonymousValue(globalObject->globalData(), 0, globalObject);
}

void ObjectPrototype::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    JSNonFinalObject::put(exec, propertyName, value, slot);

    if (m_hasNoPropertiesWithUInt32Names) {
        bool isUInt32;
        propertyName.toUInt32(isUInt32);
        m_hasNoPropertiesWithUInt32Names = !isUInt32;
    }
}

bool ObjectPrototype::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    if (m_hasNoPropertiesWithUInt32Names)
        return false;
    return JSNonFinalObject::getOwnPropertySlot(exec, propertyName, slot);
}

bool ObjectPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<JSNonFinalObject>(exec, ExecState::objectPrototypeTable(exec), this, propertyName, slot);
}

bool ObjectPrototype::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticFunctionDescriptor<JSNonFinalObject>(exec, ExecState::objectPrototypeTable(exec), this, propertyName, descriptor);
}

// ------------------------------ Functions --------------------------------

EncodedJSValue JSC_HOST_CALL objectProtoFuncValueOf(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    return JSValue::encode(thisValue.toObject(exec));
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncHasOwnProperty(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    return JSValue::encode(jsBoolean(thisValue.toObject(exec)->hasOwnProperty(exec, Identifier(exec, exec->argument(0).toString(exec)))));
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncIsPrototypeOf(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSObject* thisObj = thisValue.toObject(exec);

    if (!exec->argument(0).isObject())
        return JSValue::encode(jsBoolean(false));

    JSValue v = asObject(exec->argument(0))->prototype();

    while (true) {
        if (!v.isObject())
            return JSValue::encode(jsBoolean(false));
        if (v == thisObj)
            return JSValue::encode(jsBoolean(true));
        v = asObject(v)->prototype();
    }
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineGetter(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    CallData callData;
    if (getCallData(exec->argument(1), callData) == CallTypeNone)
        return throwVMError(exec, createSyntaxError(exec, "invalid getter usage"));
    thisValue.toThisObject(exec)->defineGetter(exec, Identifier(exec, exec->argument(0).toString(exec)), asObject(exec->argument(1)));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineSetter(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    CallData callData;
    if (getCallData(exec->argument(1), callData) == CallTypeNone)
        return throwVMError(exec, createSyntaxError(exec, "invalid setter usage"));
    thisValue.toThisObject(exec)->defineSetter(exec, Identifier(exec, exec->argument(0).toString(exec)), asObject(exec->argument(1)));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupGetter(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    return JSValue::encode(thisValue.toThisObject(exec)->lookupGetter(exec, Identifier(exec, exec->argument(0).toString(exec))));
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupSetter(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    return JSValue::encode(thisValue.toThisObject(exec)->lookupSetter(exec, Identifier(exec, exec->argument(0).toString(exec))));
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncPropertyIsEnumerable(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    return JSValue::encode(jsBoolean(thisValue.toObject(exec)->propertyIsEnumerable(exec, Identifier(exec, exec->argument(0).toString(exec)))));
}

// 15.2.4.3 Object.prototype.toLocaleString()
EncodedJSValue JSC_HOST_CALL objectProtoFuncToLocaleString(ExecState* exec)
{
    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    JSObject* object = exec->hostThisValue().toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    // 2. Let toString be the result of calling the [[Get]] internal method of O passing "toString" as the argument.
    JSValue toString = object->get(exec, exec->propertyNames().toString);

    // 3. If IsCallable(toString) is false, throw a TypeError exception.
    CallData callData;
    CallType callType = getCallData(toString, callData);
    if (callType == CallTypeNone)
        return JSValue::encode(jsUndefined());

    // 4. Return the result of calling the [[Call]] internal method of toString passing O as the this value and no arguments.
    return JSValue::encode(call(exec, toString, callType, callData, object, exec->emptyList()));
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncToString(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    if (thisValue.isUndefinedOrNull())
        return JSValue::encode(jsNontrivialString(exec, thisValue.isUndefined() ? "[object Undefined]" : "[object Null]"));
    return JSValue::encode(jsMakeNontrivialString(exec, "[object ", thisValue.toObject(exec)->className(), "]"));
}

} // namespace JSC
