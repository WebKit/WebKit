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

ASSERT_HAS_TRIVIAL_DESTRUCTOR(ObjectPrototype);

const ClassInfo ObjectPrototype::s_info = { "Object", &JSNonFinalObject::s_info, 0, ExecState::objectPrototypeTable, CREATE_METHOD_TABLE(ObjectPrototype) };

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

ObjectPrototype::ObjectPrototype(ExecState* exec, Structure* stucture)
    : JSNonFinalObject(exec->globalData(), stucture)
    , m_hasNoPropertiesWithUInt32Names(true)
{
}

void ObjectPrototype::finishCreation(JSGlobalData& globalData, JSGlobalObject*)
{
    Base::finishCreation(globalData);
    ASSERT(inherits(&s_info));
}

void ObjectPrototype::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    ObjectPrototype* thisObject = jsCast<ObjectPrototype*>(cell);
    Base::put(cell, exec, propertyName, value, slot);

    if (thisObject->m_hasNoPropertiesWithUInt32Names) {
        bool isUInt32;
        propertyName.toUInt32(isUInt32);
        thisObject->m_hasNoPropertiesWithUInt32Names = !isUInt32;
    }
}

bool ObjectPrototype::defineOwnProperty(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor, bool shouldThrow)
{
    ObjectPrototype* thisObject = jsCast<ObjectPrototype*>(object);
    bool result = Base::defineOwnProperty(object, exec, propertyName, descriptor, shouldThrow);

    if (thisObject->m_hasNoPropertiesWithUInt32Names) {
        bool isUInt32;
        propertyName.toUInt32(isUInt32);
        thisObject->m_hasNoPropertiesWithUInt32Names = !isUInt32;
    }

    return result;
}

bool ObjectPrototype::getOwnPropertySlotByIndex(JSCell* cell, ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    ObjectPrototype* thisObject = jsCast<ObjectPrototype*>(cell);
    if (thisObject->m_hasNoPropertiesWithUInt32Names)
        return false;
    return Base::getOwnPropertySlotByIndex(thisObject, exec, propertyName, slot);
}

bool ObjectPrototype::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<JSNonFinalObject>(exec, ExecState::objectPrototypeTable(exec), jsCast<ObjectPrototype*>(cell), propertyName, slot);
}

bool ObjectPrototype::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticFunctionDescriptor<JSNonFinalObject>(exec, ExecState::objectPrototypeTable(exec), jsCast<ObjectPrototype*>(object), propertyName, descriptor);
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
    return JSValue::encode(jsBoolean(thisValue.toObject(exec)->hasOwnProperty(exec, Identifier(exec, exec->argument(0).toString(exec)->value(exec)))));
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
    JSObject* thisObject = exec->hostThisValue().toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue get = exec->argument(1);
    CallData callData;
    if (getCallData(get, callData) == CallTypeNone)
        return throwVMError(exec, createSyntaxError(exec, "invalid getter usage"));

    PropertyDescriptor descriptor;
    descriptor.setGetter(get);
    descriptor.setEnumerable(true);
    descriptor.setConfigurable(true);
    thisObject->methodTable()->defineOwnProperty(thisObject, exec, Identifier(exec, exec->argument(0).toString(exec)->value(exec)), descriptor, false);

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineSetter(ExecState* exec)
{
    JSObject* thisObject = exec->hostThisValue().toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue set = exec->argument(1);
    CallData callData;
    if (getCallData(set, callData) == CallTypeNone)
        return throwVMError(exec, createSyntaxError(exec, "invalid setter usage"));

    PropertyDescriptor descriptor;
    descriptor.setSetter(set);
    descriptor.setEnumerable(true);
    descriptor.setConfigurable(true);
    thisObject->methodTable()->defineOwnProperty(thisObject, exec, Identifier(exec, exec->argument(0).toString(exec)->value(exec)), descriptor, false);

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupGetter(ExecState* exec)
{
    JSObject* thisObject = exec->hostThisValue().toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    PropertyDescriptor descriptor;
    if (thisObject->getPropertyDescriptor(exec, Identifier(exec, exec->argument(0).toString(exec)->value(exec)), descriptor)
        && descriptor.getterPresent())
        return JSValue::encode(descriptor.getter());

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupSetter(ExecState* exec)
{
    JSObject* thisObject = exec->hostThisValue().toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    PropertyDescriptor descriptor;
    if (thisObject->getPropertyDescriptor(exec, Identifier(exec, exec->argument(0).toString(exec)->value(exec)), descriptor)
        && descriptor.setterPresent())
        return JSValue::encode(descriptor.setter());

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncPropertyIsEnumerable(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    return JSValue::encode(jsBoolean(thisValue.toObject(exec)->propertyIsEnumerable(exec, Identifier(exec, exec->argument(0).toString(exec)->value(exec)))));
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
    JSObject* thisObject = thisValue.toObject(exec);

    JSString* result = thisObject->structure()->objectToStringValue();
    if (!result) {
        RefPtr<StringImpl> newString = WTF::tryMakeString("[object ", thisObject->methodTable()->className(thisObject), "]");
        if (!newString)
            return JSValue::encode(throwOutOfMemoryError(exec));

        result = jsNontrivialString(exec, newString.release());
        thisObject->structure()->setObjectToStringValue(exec->globalData(), thisObject, result);
    }
    return JSValue::encode(result);
}

} // namespace JSC
