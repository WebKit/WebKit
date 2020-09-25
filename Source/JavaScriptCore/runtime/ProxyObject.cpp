/*
 * Copyright (C) 2016-2020 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProxyObject.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "VMInlines.h"
#include <wtf/NoTailCalls.h>

// Note that we use NO_TAIL_CALLS() throughout this file because we rely on the machine stack
// growing larger for throwing OOM errors for when we have an effectively cyclic prototype chain.

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ProxyObject);

const ClassInfo ProxyObject::s_info = { "ProxyObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ProxyObject) };

static JSC_DECLARE_HOST_FUNCTION(performProxyCall);
static JSC_DECLARE_HOST_FUNCTION(performProxyConstruct);

ProxyObject::ProxyObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

Structure* ProxyObject::structureForTarget(JSGlobalObject* globalObject, JSValue target)
{
    VM& vm = globalObject->vm();
    return target.isCallable(vm) ? globalObject->callableProxyObjectStructure() : globalObject->proxyObjectStructure();
}

void ProxyObject::finishCreation(VM& vm, JSGlobalObject* globalObject, JSValue target, JSValue handler)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    Base::finishCreation(vm);
    ASSERT(type() == ProxyObjectType);
    if (!target.isObject()) {
        throwTypeError(globalObject, scope, "A Proxy's 'target' should be an Object"_s);
        return;
    }
    if (!handler.isObject()) {
        throwTypeError(globalObject, scope, "A Proxy's 'handler' should be an Object"_s);
        return;
    }

    JSObject* targetAsObject = jsCast<JSObject*>(target);

    m_isCallable = targetAsObject->isCallable(vm);
    if (m_isCallable) {
        TypeInfo info = structure(vm)->typeInfo();
        RELEASE_ASSERT(info.implementsHasInstance() && info.implementsDefaultHasInstance());
    }

    m_isConstructible = targetAsObject->isConstructor(vm);

    m_target.set(vm, this, targetAsObject);
    m_handler.set(vm, this, handler);
}

static const ASCIILiteral s_proxyAlreadyRevokedErrorMessage { "Proxy has already been revoked. No more operations are allowed to be performed on it"_s };

static JSValue performProxyGet(JSGlobalObject* globalObject, ProxyObject* proxyObject, JSValue receiver, PropertyName propertyName)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return { };
    }

    JSObject* target = proxyObject->target();

    auto performDefaultGet = [&] {
        scope.release();
        PropertySlot slot(receiver, PropertySlot::InternalMethodType::Get);
        bool hasProperty = target->getPropertySlot(globalObject, propertyName, slot);
        EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
        if (hasProperty)
            RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));

        return jsUndefined();
    };

    if (propertyName.isPrivateName())
        return jsUndefined();

    JSValue handlerValue = proxyObject->handler();
    if (handlerValue.isNull())
        return throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue getHandler = handler->getMethod(globalObject, callData, vm.propertyNames->get, "'get' property of a Proxy's handler object should be callable"_s);
    RETURN_IF_EXCEPTION(scope, { });

    if (getHandler.isUndefined())
        return performDefaultGet();

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(vm, propertyName.uid())));
    arguments.append(receiver);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, getHandler, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, { });

    PropertyDescriptor descriptor;
    bool result = target->getOwnPropertyDescriptor(globalObject, propertyName, descriptor);
    EXCEPTION_ASSERT(!scope.exception() || !result);
    if (result) {
        if (descriptor.isDataDescriptor() && !descriptor.configurable() && !descriptor.writable()) {
            bool isSame = sameValue(globalObject, descriptor.value(), trapResult);
            RETURN_IF_EXCEPTION(scope, { });
            if (!isSame)
                return throwTypeError(globalObject, scope, "Proxy handler's 'get' result of a non-configurable and non-writable property should be the same value as the target's property"_s);
        } else if (descriptor.isAccessorDescriptor() && !descriptor.configurable() && descriptor.getter().isUndefined()) {
            if (!trapResult.isUndefined())
                return throwTypeError(globalObject, scope, "Proxy handler's 'get' result of a non-configurable accessor property without a getter should be undefined"_s);
        }
    }

    RETURN_IF_EXCEPTION(scope, { });

    return trapResult;
}

bool ProxyObject::performGet(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue result = performProxyGet(globalObject, this, slot.thisValue(), propertyName);
    RETURN_IF_EXCEPTION(scope, false);
    unsigned ignoredAttributes = 0;
    slot.setValue(this, ignoredAttributes, result);
    return true;
}

bool ProxyObject::performInternalMethodGetOwnProperty(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }
    JSObject* target = this->target();

    auto performDefaultGetOwnProperty = [&] {
        return target->methodTable(vm)->getOwnPropertySlot(target, globalObject, propertyName, slot);
    };

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue getOwnPropertyDescriptorMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "getOwnPropertyDescriptor"), "'getOwnPropertyDescriptor' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    if (getOwnPropertyDescriptorMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultGetOwnProperty());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(vm, propertyName.uid())));
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, getOwnPropertyDescriptorMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResult.isUndefined() && !trapResult.isObject()) {
        throwTypeError(globalObject, scope, "result of 'getOwnPropertyDescriptor' call should either be an Object or undefined"_s);
        return false;
    }

    PropertyDescriptor targetPropertyDescriptor;
    bool isTargetPropertyDescriptorDefined = target->getOwnPropertyDescriptor(globalObject, propertyName, targetPropertyDescriptor);
    RETURN_IF_EXCEPTION(scope, false);

    if (trapResult.isUndefined()) {
        if (!isTargetPropertyDescriptorDefined)
            return false;
        if (!targetPropertyDescriptor.configurable()) {
            throwTypeError(globalObject, scope, "When the result of 'getOwnPropertyDescriptor' is undefined the target must be configurable"_s);
            return false;
        }
        bool isExtensible = target->isExtensible(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (!isExtensible) {
            throwTypeError(globalObject, scope, "When 'getOwnPropertyDescriptor' returns undefined, the 'target' of a Proxy should be extensible"_s);
            return false;
        }

        return false;
    }

    bool isExtensible = target->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    PropertyDescriptor trapResultAsDescriptor;
    toPropertyDescriptor(globalObject, trapResult, trapResultAsDescriptor);
    RETURN_IF_EXCEPTION(scope, false);
    bool throwException = false;
    bool valid = validateAndApplyPropertyDescriptor(globalObject, nullptr, propertyName, isExtensible,
        trapResultAsDescriptor, isTargetPropertyDescriptorDefined, targetPropertyDescriptor, throwException);
    RETURN_IF_EXCEPTION(scope, false);
    if (!valid) {
        throwTypeError(globalObject, scope, "Result from 'getOwnPropertyDescriptor' fails the IsCompatiblePropertyDescriptor test"_s);
        return false;
    }

    if (!trapResultAsDescriptor.configurable()) {
        if (!isTargetPropertyDescriptorDefined || targetPropertyDescriptor.configurable()) {
            throwTypeError(globalObject, scope, "Result from 'getOwnPropertyDescriptor' can't be non-configurable when the 'target' doesn't have it as an own property or if it is a configurable own property on 'target'"_s);
            return false;
        }
        if (trapResultAsDescriptor.writablePresent() && !trapResultAsDescriptor.writable() && targetPropertyDescriptor.writable()) {
            throwTypeError(globalObject, scope, "Result from 'getOwnPropertyDescriptor' can't be non-configurable and non-writable when the target's property is writable"_s);
            return false;
        }
    }

    if (trapResultAsDescriptor.isAccessorDescriptor()) {
        GetterSetter* getterSetter = trapResultAsDescriptor.slowGetterSetter(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        slot.setGetterSlot(this, trapResultAsDescriptor.attributes(), getterSetter);
    } else if (trapResultAsDescriptor.isDataDescriptor() && !trapResultAsDescriptor.value().isEmpty())
        slot.setValue(this, trapResultAsDescriptor.attributes(), trapResultAsDescriptor.value());
    else
        slot.setValue(this, trapResultAsDescriptor.attributes(), jsUndefined()); // We use undefined because it's the default value in object properties.

    return true;
}

bool ProxyObject::performHasProperty(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }
    JSObject* target = this->target();
    slot.setValue(this, static_cast<unsigned>(PropertyAttribute::None), jsUndefined()); // Nobody should rely on our value, but be safe and protect against any bad actors reading our value.

    auto performDefaultHasProperty = [&] {
        return target->getPropertySlot(globalObject, propertyName, slot);
    };

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue hasMethod = handler->getMethod(globalObject, callData, vm.propertyNames->has, "'has' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    if (hasMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultHasProperty());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(vm, propertyName.uid())));
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, hasMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResultAsBool) {
        PropertyDescriptor descriptor;
        bool isPropertyDescriptorDefined = target->getOwnPropertyDescriptor(globalObject, propertyName, descriptor); 
        RETURN_IF_EXCEPTION(scope, false);
        if (isPropertyDescriptorDefined) {
            if (!descriptor.configurable()) {
                throwTypeError(globalObject, scope, "Proxy 'has' must return 'true' for non-configurable properties"_s);
                return false;
            }
            bool isExtensible = target->isExtensible(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
            if (!isExtensible) {
                throwTypeError(globalObject, scope, "Proxy 'has' must return 'true' for a non-extensible 'target' object with a configurable property"_s);
                return false;
            }
        }
    }

    return trapResultAsBool;
}

bool ProxyObject::getOwnPropertySlotCommon(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    slot.disableCaching();
    slot.setIsTaintedByOpaqueObject();

    if (slot.isVMInquiry()) {
        slot.setValue(this, static_cast<unsigned>(JSC::PropertyAttribute::None), jsUndefined());
        return false;
    }

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }
    switch (slot.internalMethodType()) {
    case PropertySlot::InternalMethodType::Get:
        RELEASE_AND_RETURN(scope, performGet(globalObject, propertyName, slot));
    case PropertySlot::InternalMethodType::GetOwnProperty:
        RELEASE_AND_RETURN(scope, performInternalMethodGetOwnProperty(globalObject, propertyName, slot));
    case PropertySlot::InternalMethodType::HasProperty:
        RELEASE_AND_RETURN(scope, performHasProperty(globalObject, propertyName, slot));
    default:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool ProxyObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->getOwnPropertySlotCommon(globalObject, propertyName, slot);
}

bool ProxyObject::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    Identifier ident = Identifier::from(vm, propertyName);
    return thisObject->getOwnPropertySlotCommon(globalObject, ident.impl(), slot);
}

template <typename PerformDefaultPutFunction>
bool ProxyObject::performPut(JSGlobalObject* globalObject, JSValue putValue, JSValue thisValue, PropertyName propertyName, PerformDefaultPutFunction performDefaultPut, bool shouldThrow)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue setMethod = handler->getMethod(globalObject, callData, vm.propertyNames->set, "'set' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    JSObject* target = this->target();
    if (setMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultPut());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(vm, propertyName.uid())));
    arguments.append(putValue);
    arguments.append(thisValue);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, setMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);
    bool trapResultAsBool = trapResult.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (!trapResultAsBool) {
        if (shouldThrow)
            throwTypeError(globalObject, scope, makeString("Proxy object's 'set' trap returned falsy value for property '", String(propertyName.uid()), "'"));
        return false;
    }

    PropertyDescriptor descriptor;
    bool hasProperty = target->getOwnPropertyDescriptor(globalObject, propertyName, descriptor);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        if (descriptor.isDataDescriptor() && !descriptor.configurable() && !descriptor.writable()) {
            bool isSame = sameValue(globalObject, descriptor.value(), putValue);
            RETURN_IF_EXCEPTION(scope, false);
            if (!isSame) {
                throwTypeError(globalObject, scope, "Proxy handler's 'set' on a non-configurable and non-writable property on 'target' should either return false or be the same value already on the 'target'"_s);
                return false;
            }
        } else if (descriptor.isAccessorDescriptor() && !descriptor.configurable() && descriptor.setter().isUndefined()) {
            throwTypeError(globalObject, scope, "Proxy handler's 'set' method on a non-configurable accessor property without a setter should return false"_s);
            return false;
        }
    }
    return true;
}

bool ProxyObject::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    slot.disableCaching();

    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    auto performDefaultPut = [&] () {
        JSObject* target = jsCast<JSObject*>(thisObject->target());
        return target->methodTable(vm)->put(target, globalObject, propertyName, value, slot);
    };
    return thisObject->performPut(globalObject, value, slot.thisValue(), propertyName, performDefaultPut, slot.isStrictMode());
}

bool ProxyObject::putByIndexCommon(JSGlobalObject* globalObject, JSValue thisValue, unsigned propertyName, JSValue putValue, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Identifier ident = Identifier::from(vm, propertyName);
    RETURN_IF_EXCEPTION(scope, false);
    auto performDefaultPut = [&] () {
        JSObject* target = this->target();
        bool isStrictMode = shouldThrow;
        PutPropertySlot slot(thisValue, isStrictMode); // We must preserve the "this" target of the putByIndex.
        return target->methodTable(vm)->put(target, globalObject, ident.impl(), putValue, slot);
    };
    RELEASE_AND_RETURN(scope, performPut(globalObject, putValue, thisValue, ident.impl(), performDefaultPut, shouldThrow));
}

bool ProxyObject::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    return thisObject->putByIndexCommon(globalObject, thisObject, propertyName, value, shouldThrow);
}

JSC_DEFINE_HOST_FUNCTION(performProxyCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return encodedJSValue();
    }
    ProxyObject* proxy = jsCast<ProxyObject*>(callFrame->jsCallee());
    JSValue handlerValue = proxy->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue applyMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "apply"), "'apply' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSObject* target = proxy->target();
    if (applyMethod.isUndefined()) {
        auto callData = getCallData(vm, target);
        RELEASE_ASSERT(callData.type != CallData::Type::None);
        RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, target, callData, callFrame->thisValue(), ArgList(callFrame))));
    }

    JSArray* argArray = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), ArgList(callFrame));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
    arguments.append(argArray);
    ASSERT(!arguments.hasOverflowed());
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, applyMethod, callData, handler, arguments)));
}

CallData ProxyObject::getCallData(JSCell* cell)
{
    CallData callData;
    ProxyObject* proxy = jsCast<ProxyObject*>(cell);
    if (proxy->m_isCallable) {
        callData.type = CallData::Type::Native;
        callData.native.function = performProxyCall;
    }
    return callData;
}

JSC_DEFINE_HOST_FUNCTION(performProxyConstruct, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return encodedJSValue();
    }
    ProxyObject* proxy = jsCast<ProxyObject*>(callFrame->jsCallee());
    JSValue handlerValue = proxy->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue constructMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "construct"), "'construct' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSObject* target = proxy->target();
    if (constructMethod.isUndefined()) {
        auto constructData = getConstructData(vm, target);
        RELEASE_ASSERT(constructData.type != CallData::Type::None);
        RELEASE_AND_RETURN(scope, JSValue::encode(construct(globalObject, target, constructData, ArgList(callFrame), callFrame->newTarget())));
    }

    JSArray* argArray = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), ArgList(callFrame));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(argArray);
    arguments.append(callFrame->newTarget());
    ASSERT(!arguments.hasOverflowed());
    JSValue result = call(globalObject, constructMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!result.isObject())
        return throwVMTypeError(globalObject, scope, "Result from Proxy handler's 'construct' method should be an object"_s);
    return JSValue::encode(result);
}

CallData ProxyObject::getConstructData(JSCell* cell)
{
    CallData constructData;
    ProxyObject* proxy = jsCast<ProxyObject*>(cell);
    if (proxy->m_isConstructible) {
        constructData.type = CallData::Type::Native;
        constructData.native.function = performProxyConstruct;
    }
    return constructData;
}

template <typename DefaultDeleteFunction>
bool ProxyObject::performDelete(JSGlobalObject* globalObject, PropertyName propertyName, DefaultDeleteFunction performDefaultDelete)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue deletePropertyMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "deleteProperty"), "'deleteProperty' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    JSObject* target = this->target();
    if (deletePropertyMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultDelete());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(vm, propertyName.uid())));
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, deletePropertyMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResultAsBool)
        return false;

    PropertyDescriptor descriptor;
    bool result = target->getOwnPropertyDescriptor(globalObject, propertyName, descriptor);
    EXCEPTION_ASSERT(!scope.exception() || !result);
    if (result) {
        if (!descriptor.configurable()) {
            throwTypeError(globalObject, scope, "Proxy handler's 'deleteProperty' method should return false when the target's property is not configurable"_s);
            return false;
        }
        bool targetIsExtensible = target->isExtensible(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (!targetIsExtensible) {
            throwTypeError(globalObject, scope, "Proxy handler's 'deleteProperty' method should return false when the target has property and is not extensible"_s);
            return false;
        }
    }

    RETURN_IF_EXCEPTION(scope, false);

    return true;
}

bool ProxyObject::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    auto performDefaultDelete = [&] () -> bool {
        JSObject* target = thisObject->target();
        return target->methodTable(globalObject->vm())->deleteProperty(target, globalObject, propertyName, slot);
    };
    return thisObject->performDelete(globalObject, propertyName, performDefaultDelete);
}

bool ProxyObject::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName)
{
    VM& vm = globalObject->vm();
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    Identifier ident = Identifier::from(vm, propertyName);
    auto performDefaultDelete = [&] () -> bool {
        JSObject* target = thisObject->target();
        return target->methodTable(vm)->deletePropertyByIndex(target, globalObject, propertyName);
    };
    return thisObject->performDelete(globalObject, ident.impl(), performDefaultDelete);
}

bool ProxyObject::performPreventExtensions(JSGlobalObject* globalObject)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue preventExtensionsMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "preventExtensions"), "'preventExtensions' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    JSObject* target = this->target();
    if (preventExtensionsMethod.isUndefined())
        RELEASE_AND_RETURN(scope, target->methodTable(vm)->preventExtensions(target, globalObject));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, preventExtensionsMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    if (trapResultAsBool) {
        bool targetIsExtensible = target->isExtensible(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (targetIsExtensible) {
            throwTypeError(globalObject, scope, "Proxy's 'preventExtensions' trap returned true even though its target is extensible. It should have returned false"_s);
            return false;
        }
    }

    return trapResultAsBool;
}

bool ProxyObject::preventExtensions(JSObject* object, JSGlobalObject* globalObject)
{
    return jsCast<ProxyObject*>(object)->performPreventExtensions(globalObject);
}

bool ProxyObject::performIsExtensible(JSGlobalObject* globalObject)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue isExtensibleMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "isExtensible"), "'isExtensible' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);

    JSObject* target = this->target();
    if (isExtensibleMethod.isUndefined())
        RELEASE_AND_RETURN(scope, target->isExtensible(globalObject));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, isExtensibleMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    bool isTargetExtensible = target->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    if (trapResultAsBool != isTargetExtensible) {
        if (isTargetExtensible) {
            ASSERT(!trapResultAsBool);
            throwTypeError(globalObject, scope, "Proxy object's 'isExtensible' trap returned false when the target is extensible. It should have returned true"_s);
        } else {
            ASSERT(!isTargetExtensible);
            ASSERT(trapResultAsBool);
            throwTypeError(globalObject, scope, "Proxy object's 'isExtensible' trap returned true when the target is non-extensible. It should have returned false"_s);
        }
    }
    
    return trapResultAsBool;
}

bool ProxyObject::isExtensible(JSObject* object, JSGlobalObject* globalObject)
{
    return jsCast<ProxyObject*>(object)->performIsExtensible(globalObject);
}

bool ProxyObject::performDefineOwnProperty(JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }

    JSObject* target = this->target();
    auto performDefaultDefineOwnProperty = [&] {
        RELEASE_AND_RETURN(scope, target->methodTable(vm)->defineOwnProperty(target, globalObject, propertyName, descriptor, shouldThrow));
    };

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue definePropertyMethod = handler->getMethod(globalObject, callData, vm.propertyNames->defineProperty, "'defineProperty' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);

    if (definePropertyMethod.isUndefined())
        return performDefaultDefineOwnProperty();

    JSObject* descriptorObject = constructObjectFromPropertyDescriptor(globalObject, descriptor);
    scope.assertNoException();
    ASSERT(descriptorObject);

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(vm, propertyName.uid())));
    arguments.append(descriptorObject);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, definePropertyMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResultAsBool) {
        if (shouldThrow)
            throwTypeError(globalObject, scope, makeString("Proxy's 'defineProperty' trap returned falsy value for property '", String(propertyName.uid()), "'"));
        return false;
    }

    PropertyDescriptor targetDescriptor;
    bool isTargetDescriptorDefined = target->getOwnPropertyDescriptor(globalObject, propertyName, targetDescriptor);
    RETURN_IF_EXCEPTION(scope, false);

    bool targetIsExtensible = target->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    bool settingConfigurableToFalse = descriptor.configurablePresent() && !descriptor.configurable();

    if (!isTargetDescriptorDefined) {
        if (!targetIsExtensible) {
            throwTypeError(globalObject, scope, "Proxy's 'defineProperty' trap returned true even though getOwnPropertyDescriptor of the Proxy's target returned undefined and the target is non-extensible"_s);
            return false;
        }
        if (settingConfigurableToFalse) {
            throwTypeError(globalObject, scope, "Proxy's 'defineProperty' trap returned true for a non-configurable field even though getOwnPropertyDescriptor of the Proxy's target returned undefined"_s);
            return false;
        }

        return true;
    } 

    ASSERT(isTargetDescriptorDefined);
    bool isCurrentDefined = isTargetDescriptorDefined;
    const PropertyDescriptor& current = targetDescriptor;
    bool throwException = false;
    bool isCompatibleDescriptor = validateAndApplyPropertyDescriptor(globalObject, nullptr, propertyName, targetIsExtensible, descriptor, isCurrentDefined, current, throwException);
    RETURN_IF_EXCEPTION(scope, false);    
    if (!isCompatibleDescriptor) {
        throwTypeError(globalObject, scope, "Proxy's 'defineProperty' trap did not define a property on its target that is compatible with the trap's input descriptor"_s);
        return false;
    }
    if (settingConfigurableToFalse && targetDescriptor.configurable()) {
        throwTypeError(globalObject, scope, "Proxy's 'defineProperty' trap did not define a non-configurable property on its target even though the input descriptor to the trap said it must do so"_s);
        return false;
    }
    if (targetDescriptor.isDataDescriptor() && !targetDescriptor.configurable() && targetDescriptor.writable()) {
        if (descriptor.writablePresent() && !descriptor.writable()) {
            throwTypeError(globalObject, scope, "Proxy's 'defineProperty' trap returned true for a non-writable input descriptor when the target's property is non-configurable and writable"_s);
            return false;
        }
    }
    
    return true;
}

bool ProxyObject::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->performDefineOwnProperty(globalObject, propertyName, descriptor, shouldThrow);
}

void ProxyObject::performGetOwnPropertyNames(JSGlobalObject* globalObject, PropertyNameArray& propertyNames)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return;
    }
    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue ownKeysMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "ownKeys"), "'ownKeys' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, void());
    JSObject* target = this->target();
    EnumerationMode enumerationMode(DontEnumPropertiesMode::Include);
    if (ownKeysMethod.isUndefined()) {
        scope.release();
        target->methodTable(vm)->getOwnPropertyNames(target, globalObject, propertyNames, enumerationMode);
        return;
    }

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, ownKeysMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, void());

    HashSet<UniquedStringImpl*> uncheckedResultKeys;
    {
        RuntimeTypeMask resultFilter = 0;
        switch (propertyNames.propertyNameMode()) {
        case PropertyNameMode::Symbols:
            resultFilter = TypeSymbol;
            break;
        case PropertyNameMode::Strings:
            resultFilter = TypeString;
            break;
        case PropertyNameMode::StringsAndSymbols:
            resultFilter = TypeSymbol | TypeString;
            break;
        }
        ASSERT(resultFilter);

        auto addPropName = [&] (JSValue value, RuntimeType type) -> bool {
            static constexpr bool doExitEarly = true;
            static constexpr bool dontExitEarly = false;

            Identifier ident = value.toPropertyKey(globalObject);
            RETURN_IF_EXCEPTION(scope, doExitEarly);

            if (!uncheckedResultKeys.add(ident.impl()).isNewEntry) {
                throwTypeError(globalObject, scope, "Proxy handler's 'ownKeys' trap result must not contain any duplicate names"_s);
                return doExitEarly;
            }

            if (type & resultFilter)
                propertyNames.add(ident.impl());

            return dontExitEarly;
        };

        RuntimeTypeMask dontThrowAnExceptionTypeFilter = TypeString | TypeSymbol;
        createListFromArrayLike(globalObject, trapResult, dontThrowAnExceptionTypeFilter, "Proxy handler's 'ownKeys' method must return an object"_s, "Proxy handler's 'ownKeys' method must return an array-like object containing only Strings and Symbols"_s, addPropName);
        RETURN_IF_EXCEPTION(scope, void());
    }

    bool targetIsExensible = target->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    PropertyNameArray targetKeys(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    target->methodTable(vm)->getOwnPropertyNames(target, globalObject, targetKeys, enumerationMode);
    RETURN_IF_EXCEPTION(scope, void());
    Vector<UniquedStringImpl*> targetConfigurableKeys;
    Vector<UniquedStringImpl*> targetNonConfigurableKeys;
    for (const Identifier& ident : targetKeys) {
        PropertyDescriptor descriptor;
        bool isPropertyDefined = target->getOwnPropertyDescriptor(globalObject, ident.impl(), descriptor); 
        RETURN_IF_EXCEPTION(scope, void());
        if (isPropertyDefined && !descriptor.configurable())
            targetNonConfigurableKeys.append(ident.impl());
        else
            targetConfigurableKeys.append(ident.impl());
    }

    enum ContainedIn { IsContainedIn, IsNotContainedIn };
    auto removeIfContainedInUncheckedResultKeys = [&] (UniquedStringImpl* impl) -> ContainedIn {
        auto iter = uncheckedResultKeys.find(impl);
        if (iter == uncheckedResultKeys.end())
            return IsNotContainedIn;

        uncheckedResultKeys.remove(iter);
        return IsContainedIn;
    };

    for (UniquedStringImpl* impl : targetNonConfigurableKeys) {
        if (removeIfContainedInUncheckedResultKeys(impl) == IsNotContainedIn) {
            throwTypeError(globalObject, scope, makeString("Proxy object's 'target' has the non-configurable property '", String(impl), "' that was not in the result from the 'ownKeys' trap"));
            return;
        }
    }

    if (!targetIsExensible) {
        for (UniquedStringImpl* impl : targetConfigurableKeys) {
            if (removeIfContainedInUncheckedResultKeys(impl) == IsNotContainedIn) {
                throwTypeError(globalObject, scope, makeString("Proxy object's non-extensible 'target' has configurable property '", String(impl), "' that was not in the result from the 'ownKeys' trap"));
                return;
            }
        }

        if (uncheckedResultKeys.size()) {
            throwTypeError(globalObject, scope, "Proxy handler's 'ownKeys' method returned a key that was not present in its non-extensible target"_s);
            return;
        }
    }
}

void ProxyObject::performGetOwnEnumerablePropertyNames(JSGlobalObject* globalObject, PropertyNameArray& propertyNames)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertyNameArray unfilteredNames(vm, propertyNames.propertyNameMode(), propertyNames.privateSymbolMode());
    performGetOwnPropertyNames(globalObject, unfilteredNames);
    RETURN_IF_EXCEPTION(scope, void());
    // Filtering DontEnum properties is observable in proxies and must occur after the invariant checks pass.
    for (const auto& propertyName : unfilteredNames) {
        PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
        auto isPropertyDefined = getOwnPropertySlotCommon(globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, void());
        if (!isPropertyDefined)
            continue;
        if (slot.attributes() & PropertyAttribute::DontEnum)
            continue;
        propertyNames.add(propertyName.impl());
    }
}

void ProxyObject::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNameArray, EnumerationMode enumerationMode)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    if (enumerationMode.includeDontEnumProperties())
        thisObject->performGetOwnPropertyNames(globalObject, propertyNameArray);
    else
        thisObject->performGetOwnEnumerablePropertyNames(globalObject, propertyNameArray);
}

void ProxyObject::getPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNameArray, EnumerationMode enumerationMode)
{
    NO_TAIL_CALLS();
    JSObject::getPropertyNames(object, globalObject, propertyNameArray, enumerationMode);
}

void ProxyObject::getOwnNonIndexPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void ProxyObject::getStructurePropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    // We should always go down the getOwnPropertyNames path.
    RELEASE_ASSERT_NOT_REACHED();
}

void ProxyObject::getGenericPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool ProxyObject::performSetPrototype(JSGlobalObject* globalObject, JSValue prototype, bool shouldThrowIfCantSet)
{
    NO_TAIL_CALLS();

    ASSERT(prototype.isObject() || prototype.isNull());

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return false;
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue setPrototypeOfMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "setPrototypeOf"), "'setPrototypeOf' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);

    JSObject* target = this->target();
    if (setPrototypeOfMethod.isUndefined())
        RELEASE_AND_RETURN(scope, target->setPrototype(vm, globalObject, prototype, shouldThrowIfCantSet));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(prototype);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, setPrototypeOfMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    
    if (!trapResultAsBool) {
        if (shouldThrowIfCantSet)
            throwTypeError(globalObject, scope, "Proxy 'setPrototypeOf' returned false indicating it could not set the prototype value. The operation was expected to succeed"_s);
        return false;
    }

    bool targetIsExtensible = target->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (targetIsExtensible)
        return true;

    JSValue targetPrototype = target->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    bool isSame = sameValue(globalObject, prototype, targetPrototype);
    RETURN_IF_EXCEPTION(scope, false);
    if (!isSame) {
        throwTypeError(globalObject, scope, "Proxy 'setPrototypeOf' trap returned true when its target is non-extensible and the new prototype value is not the same as the current prototype value. It should have returned false"_s);
        return false;
    }

    return true;
}

bool ProxyObject::setPrototype(JSObject* object, JSGlobalObject* globalObject, JSValue prototype, bool shouldThrowIfCantSet)
{
    return jsCast<ProxyObject*>(object)->performSetPrototype(globalObject, prototype, shouldThrowIfCantSet);
}

JSValue ProxyObject::performGetPrototype(JSGlobalObject* globalObject)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(globalObject, scope);
        return { };
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwTypeError(globalObject, scope, s_proxyAlreadyRevokedErrorMessage);
        return { };
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    JSValue getPrototypeOfMethod = handler->getMethod(globalObject, callData, makeIdentifier(vm, "getPrototypeOf"), "'getPrototypeOf' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* target = this->target();
    if (getPrototypeOfMethod.isUndefined()) 
        RELEASE_AND_RETURN(scope, target->getPrototype(vm, globalObject));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(globalObject, getPrototypeOfMethod, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, { });

    if (!trapResult.isObject() && !trapResult.isNull()) {
        throwTypeError(globalObject, scope, "Proxy handler's 'getPrototypeOf' trap should either return an object or null"_s);
        return { };
    }

    bool targetIsExtensible = target->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (targetIsExtensible)
        return trapResult;

    JSValue targetPrototype = target->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    bool isSame = sameValue(globalObject, targetPrototype, trapResult);
    RETURN_IF_EXCEPTION(scope, { });
    if (!isSame) {
        throwTypeError(globalObject, scope, "Proxy's 'getPrototypeOf' trap for a non-extensible target should return the same value as the target's prototype"_s);
        return { };
    }

    return trapResult;
}

JSValue ProxyObject::getPrototype(JSObject* object, JSGlobalObject* globalObject)
{
    return jsCast<ProxyObject*>(object)->performGetPrototype(globalObject);
}

void ProxyObject::revoke(VM& vm)
{ 
    // This should only ever be called once and we should strictly transition from Object to null.
    RELEASE_ASSERT(!m_handler.get().isNull() && m_handler.get().isObject());
    m_handler.set(vm, this, jsNull());
}

bool ProxyObject::isRevoked() const
{
    return handler().isNull();
}

void ProxyObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_target);
    visitor.append(thisObject->m_handler);
}

} // namespace JSC
