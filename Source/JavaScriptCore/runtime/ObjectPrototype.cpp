/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#include "ArrayConstructor.h"
#include "GetterSetter.h"
#include "HasOwnPropertyCache.h"
#include "IntegrityInlines.h"
#include "JSCInlines.h"
#include "PropertySlot.h"

#if PLATFORM(IOS)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncValueOf);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncHasOwnProperty);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncIsPrototypeOf);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncDefineGetter);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncDefineSetter);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncLookupGetter);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncLookupSetter);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncPropertyIsEnumerable);
static JSC_DECLARE_HOST_FUNCTION(objectProtoFuncToLocaleString);

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
    
    putDirectWithoutTransition(vm, vm.propertyNames->toString, globalObject->objectProtoToStringFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
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
    ObjectPrototype* prototype = new (NotNull, allocateCell<ObjectPrototype>(vm)) ObjectPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

// ------------------------------ Functions --------------------------------

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncValueOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    JSObject* valueObj = thisValue.toObject(globalObject);
    if (UNLIKELY(!valueObj))
        return encodedJSValue();
    Integrity::auditStructureID(valueObj->structureID());
    return JSValue::encode(valueObj);
}

bool objectPrototypeHasOwnProperty(JSGlobalObject* globalObject, JSObject* thisObject, const Identifier& propertyName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = thisObject->structure(vm);
    HasOwnPropertyCache* hasOwnPropertyCache = vm.ensureHasOwnPropertyCache();
    if (std::optional<bool> result = hasOwnPropertyCache->get(structure, propertyName)) {
        ASSERT(*result == thisObject->hasOwnProperty(globalObject, propertyName) || vm.hasPendingTerminationException());
        scope.assertNoExceptionExceptTermination();
        return *result;
    }

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);
    bool result = thisObject->hasOwnProperty(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, false);

    hasOwnPropertyCache->tryAdd(vm, slot, thisObject, propertyName, result);
    return result;
}

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncHasOwnProperty, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = callFrame->thisValue();
    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSObject* thisObject = base.toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(objectPrototypeHasOwnProperty(globalObject, thisObject, propertyName))));
}

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncIsPrototypeOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->argument(0).isObject())
        return JSValue::encode(jsBoolean(false));

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
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

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncDefineGetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue get = callFrame->argument(1);
    if (!get.isCallable(vm))
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

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncDefineSetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue set = callFrame->argument(1);
    if (!set.isCallable(vm))
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

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncLookupGetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
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

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncLookupSetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
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

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncPropertyIsEnumerable, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    scope.release();
    PropertyDescriptor descriptor;
    bool enumerable = thisObject->getOwnPropertyDescriptor(globalObject, propertyName, descriptor) && descriptor.enumerable();
    return JSValue::encode(jsBoolean(enumerable));
}

// 15.2.4.3 Object.prototype.toLocaleString()
JSC_DEFINE_HOST_FUNCTION(objectProtoFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let V be the this value.
    JSValue thisValue = callFrame->thisValue();

    // 2. Invoke(V, "toString")

    // Let O be the result of calling ToObject passing the this value as the argument.
    JSObject* object = thisValue.toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // Let toString be the O.[[Get]]("toString", V)
    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);
    bool hasProperty = object->getPropertySlot(globalObject, vm.propertyNames->toString, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    JSValue toString = hasProperty ? slot.getValue(globalObject, vm.propertyNames->toString) : jsUndefined();
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // If IsCallable(toString) is false, throw a TypeError exception.
    auto callData = getCallData(vm, toString);
    if (callData.type == CallData::Type::None)
        return throwVMTypeError(globalObject, scope);

    // Return the result of calling the [[Call]] internal method of toString passing the this value and no arguments.
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, toString, callData, thisValue, *vm.emptyList)));
}

#if PLATFORM(IOS)
inline static bool isPokerBros()
{
    auto bundleID = CFBundleGetIdentifier(CFBundleGetMainBundle());
    return bundleID
        && CFEqual(bundleID, CFSTR("com.kpgame.PokerBros"))
        && !linkedOnOrAfter(SDKVersion::FirstWithoutPokerBrosBuiltInTagQuirk);
}
#endif

inline const char* inferBuiltinTag(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
#if PLATFORM(IOS)
    static bool needsOldBuiltinTag = isPokerBros();
    if (UNLIKELY(needsOldBuiltinTag))
        return object->className(vm);
#endif
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool objectIsArray = isArray(globalObject, object);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (objectIsArray)
        return "Array";
    if (object->isCallable(vm))
        return "Function";
    JSType type = object->type();
    if (TypeInfo::isArgumentsType(type)
        || type == ErrorInstanceType
        || type == BooleanObjectType
        || type == NumberObjectType
        || type == StringObjectType
        || type == DerivedStringObjectType
        || type == JSDateType
        || type == RegExpObjectType)
        return object->className(vm);
    return "Object";
}

JSString* objectPrototypeToString(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (thisValue.isUndefined())
        return vm.smallStrings.undefinedObjectString();
    if (thisValue.isNull())
        return vm.smallStrings.nullObjectString();
    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    Integrity::auditStructureID(thisObject->structureID());
    auto result = thisObject->structure(vm)->cachedSpecialProperty(CachedSpecialPropertyKey::ToStringTag);
    if (result)
        return asString(result);

    const char* tag = inferBuiltinTag(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSString* jsTag = nullptr;

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::Get);
    bool hasProperty = thisObject->getPropertySlot(globalObject, vm.propertyNames->toStringTagSymbol, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue tagValue = slot.getValue(globalObject, vm.propertyNames->toStringTagSymbol);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (tagValue.isString())
            jsTag = asString(tagValue);
    }

    if (!jsTag)
        jsTag = jsString(vm, AtomStringImpl::add(tag).releaseNonNull());

    JSString* jsResult = jsString(globalObject, vm.smallStrings.objectStringStart(), jsTag, vm.smallStrings.singleCharacterString(']'));
    RETURN_IF_EXCEPTION(scope, nullptr);
    thisObject->structure(vm)->cacheSpecialProperty(globalObject, vm, jsResult, CachedSpecialPropertyKey::ToStringTag, slot);
    return jsResult;
}

JSC_DEFINE_HOST_FUNCTION(objectProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    return JSValue::encode(objectPrototypeToString(globalObject, thisValue));
}

} // namespace JSC
