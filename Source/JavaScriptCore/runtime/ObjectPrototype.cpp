/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "GetterSetter.h"
#include "HasOwnPropertyCache.h"
#include "JSFunction.h"
#include "JSString.h"
#include "JSCInlines.h"
#include "PropertySlot.h"
#include "StructureInlines.h"
#include "StructureRareDataInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL objectProtoFuncValueOf(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncHasOwnProperty(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncIsPrototypeOf(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineGetter(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineSetter(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupGetter(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupSetter(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncPropertyIsEnumerable(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL objectProtoFuncToLocaleString(JSGlobalObject*, CallFrame*);

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ObjectPrototype);

const ClassInfo ObjectPrototype::s_info = { "Object", &JSNonFinalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ObjectPrototype) };

ObjectPrototype::ObjectPrototype(VM& vm, Structure* stucture)
    : JSNonFinalObject(vm, stucture)
{
}

void ObjectPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, objectProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, objectProtoFuncToLocaleString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->valueOf, objectProtoFuncValueOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->hasOwnProperty, objectProtoFuncHasOwnProperty, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, HasOwnPropertyIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->propertyIsEnumerable, objectProtoFuncPropertyIsEnumerable, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->isPrototypeOf, objectProtoFuncIsPrototypeOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->__defineGetter__, objectProtoFuncDefineGetter, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->__defineSetter__, objectProtoFuncDefineSetter, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->__lookupGetter__, objectProtoFuncLookupGetter, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->__lookupSetter__, objectProtoFuncLookupSetter, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
}

ObjectPrototype* ObjectPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    ObjectPrototype* prototype = new (NotNull, allocateCell<ObjectPrototype>(vm.heap)) ObjectPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

// ------------------------------ Functions --------------------------------

EncodedJSValue JSC_HOST_CALL objectProtoFuncValueOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);
    JSObject* valueObj = thisValue.toObject(globalObject);
    if (UNLIKELY(!valueObj))
        return encodedJSValue();
    return JSValue::encode(valueObj);
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncHasOwnProperty(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);
    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSObject* thisObject = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (UNLIKELY(!thisObject))
        return encodedJSValue();

    Structure* structure = thisObject->structure(vm);
    HasOwnPropertyCache* hasOwnPropertyCache = vm.ensureHasOwnPropertyCache();
    if (Optional<bool> result = hasOwnPropertyCache->get(structure, propertyName)) {
        ASSERT(*result == thisObject->hasOwnProperty(globalObject, propertyName));
        scope.assertNoException();
        return JSValue::encode(jsBoolean(*result));
    }

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);
    bool result = thisObject->hasOwnProperty(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    hasOwnPropertyCache->tryAdd(vm, slot, thisObject, propertyName, result);
    return JSValue::encode(jsBoolean(result));
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncIsPrototypeOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->argument(0).isObject())
        return JSValue::encode(jsBoolean(false));

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);
    JSObject* thisObj = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return encodedJSValue();

    JSValue v = asObject(callFrame->argument(0))->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    while (true) {
        if (!v.isObject())
            return JSValue::encode(jsBoolean(false));
        if (v == thisObj)
            return JSValue::encode(jsBoolean(true));
        v = asObject(v)->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineGetter(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue get = callFrame->argument(1);
    CallData callData;
    if (getCallData(vm, get, callData) == CallType::None)
        return throwVMTypeError(globalObject, scope, "invalid getter usage"_s);

    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    PropertyDescriptor descriptor;
    descriptor.setGetter(get);
    descriptor.setEnumerable(true);
    descriptor.setConfigurable(true);

    bool shouldThrow = true;
    scope.release();
    thisObject->methodTable(vm)->defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow);

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncDefineSetter(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue set = callFrame->argument(1);
    CallData callData;
    if (getCallData(vm, set, callData) == CallType::None)
        return throwVMTypeError(globalObject, scope, "invalid setter usage"_s);

    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    PropertyDescriptor descriptor;
    descriptor.setSetter(set);
    descriptor.setEnumerable(true);
    descriptor.setConfigurable(true);

    bool shouldThrow = true;
    scope.release();
    thisObject->methodTable(vm)->defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow);

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupGetter(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);
    bool hasProperty = thisObject->getPropertySlot(globalObject, propertyName, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        if (slot.isAccessor()) {
            GetterSetter* getterSetter = slot.getterSetter();
            return getterSetter->isGetterNull() ? JSValue::encode(jsUndefined()) : JSValue::encode(getterSetter->getter());
        }
        if (slot.attributes() & PropertyAttribute::CustomAccessor) {
            PropertyDescriptor descriptor;
            ASSERT(slot.slotBase());
            if (slot.slotBase()->getOwnPropertyDescriptor(globalObject, propertyName, descriptor))
                return descriptor.getterPresent() ? JSValue::encode(descriptor.getter()) : JSValue::encode(jsUndefined());
        }
    }

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncLookupSetter(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);
    bool hasProperty = thisObject->getPropertySlot(globalObject, propertyName, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        if (slot.isAccessor()) {
            GetterSetter* getterSetter = slot.getterSetter();
            return getterSetter->isSetterNull() ? JSValue::encode(jsUndefined()) : JSValue::encode(getterSetter->setter());
        }
        if (slot.attributes() & PropertyAttribute::CustomAccessor) {
            PropertyDescriptor descriptor;
            ASSERT(slot.slotBase());
            if (slot.slotBase()->getOwnPropertyDescriptor(globalObject, propertyName, descriptor))
                return descriptor.setterPresent() ? JSValue::encode(descriptor.setter()) : JSValue::encode(jsUndefined());
        }
    }

    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncPropertyIsEnumerable(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    scope.release();
    PropertyDescriptor descriptor;
    bool enumerable = thisObject->getOwnPropertyDescriptor(globalObject, propertyName, descriptor) && descriptor.enumerable();
    return JSValue::encode(jsBoolean(enumerable));
}

// 15.2.4.3 Object.prototype.toLocaleString()
EncodedJSValue JSC_HOST_CALL objectProtoFuncToLocaleString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let V be the this value.
    JSValue thisValue = callFrame->thisValue();

    // 2. Invoke(V, "toString")

    // Let O be the result of calling ToObject passing the this value as the argument.
    JSObject* object = thisValue.toThis(globalObject, StrictMode).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // Let toString be the O.[[Get]]("toString", V)
    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);
    bool hasProperty = object->getPropertySlot(globalObject, vm.propertyNames->toString, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    JSValue toString = hasProperty ? slot.getValue(globalObject, vm.propertyNames->toString) : jsUndefined();
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // If IsCallable(toString) is false, throw a TypeError exception.
    CallData callData;
    CallType callType = getCallData(vm, toString, callData);
    if (callType == CallType::None)
        return throwVMTypeError(globalObject, scope);

    // Return the result of calling the [[Call]] internal method of toString passing the this value and no arguments.
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, toString, callType, callData, thisValue, *vm.emptyList)));
}

EncodedJSValue JSC_HOST_CALL objectProtoFuncToString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);
    if (thisValue.isUndefinedOrNull())
        return JSValue::encode(thisValue.isUndefined() ? vm.smallStrings.undefinedObjectString() : vm.smallStrings.nullObjectString());
    JSObject* thisObject = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (!thisObject)
        return JSValue::encode(jsUndefined());

    auto result = thisObject->structure(vm)->objectToStringValue();
    if (result)
        return JSValue::encode(result);

    PropertyName toStringTagSymbol = vm.propertyNames->toStringTagSymbol;
    RELEASE_AND_RETURN(scope, JSValue::encode(thisObject->getPropertySlot(globalObject, toStringTagSymbol, [&] (bool found, PropertySlot& toStringTagSlot) -> JSValue {
        if (found) {
            JSValue stringTag = toStringTagSlot.getValue(globalObject, toStringTagSymbol);
            RETURN_IF_EXCEPTION(scope, { });
            if (stringTag.isString()) {
                JSString* result = jsString(globalObject, vm.smallStrings.objectStringStart(), asString(stringTag), vm.smallStrings.singleCharacterString(']'));
                RETURN_IF_EXCEPTION(scope, { });
                thisObject->structure(vm)->setObjectToStringValue(globalObject, vm, result, toStringTagSlot);
                return result;
            }
        }

        String tag = thisObject->methodTable(vm)->toStringName(thisObject, globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        String newString = tryMakeString("[object ", WTFMove(tag), "]");
        if (!newString)
            return throwOutOfMemoryError(globalObject, scope);

        auto result = jsNontrivialString(vm, newString);
        thisObject->structure(vm)->setObjectToStringValue(globalObject, vm, result, toStringTagSlot);
        return result;
    })));
}

} // namespace JSC
