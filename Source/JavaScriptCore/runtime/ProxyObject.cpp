/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

#include "Error.h"
#include "IdentifierInlines.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "ObjectConstructor.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ProxyObject);

const ClassInfo ProxyObject::s_info = { "ProxyObject", &Base::s_info, 0, CREATE_METHOD_TABLE(ProxyObject) };

ProxyObject::ProxyObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

Structure* ProxyObject::structureForTarget(JSGlobalObject* globalObject, JSValue target)
{
    if (!target.isObject())
        return globalObject->proxyObjectStructure();

    JSObject* targetAsObject = jsCast<JSObject*>(target);
    CallData ignoredCallData;
    bool isCallable = targetAsObject->methodTable()->getCallData(targetAsObject, ignoredCallData) != CallType::None;
    return isCallable ? globalObject->callableProxyObjectStructure() : globalObject->proxyObjectStructure();
}

void ProxyObject::finishCreation(VM& vm, ExecState* exec, JSValue target, JSValue handler)
{
    Base::finishCreation(vm);
    if (!target.isObject()) {
        throwTypeError(exec, ASCIILiteral("A Proxy's 'target' should be an Object"));
        return;
    }
    if (ProxyObject* targetAsProxy = jsDynamicCast<ProxyObject*>(target)) {
        if (targetAsProxy->handler().isNull()) {
            throwTypeError(exec, ASCIILiteral("If a Proxy's handler is another Proxy object, the other Proxy should not have been revoked."));
            return;
        }
    }
    if (!handler.isObject()) {
        throwTypeError(exec, ASCIILiteral("A Proxy's 'handler' should be an Object"));
        return;
    }

    JSObject* targetAsObject = jsCast<JSObject*>(target);

    CallData ignoredCallData;
    m_isCallable = targetAsObject->methodTable(vm)->getCallData(targetAsObject, ignoredCallData) != CallType::None;
    if (m_isCallable) {
        TypeInfo info = structure(vm)->typeInfo();
        RELEASE_ASSERT(info.implementsHasInstance() && info.implementsDefaultHasInstance());
    }

    ConstructData ignoredConstructData;
    m_isConstructible = jsCast<JSObject*>(target)->methodTable(vm)->getConstructData(jsCast<JSObject*>(target), ignoredConstructData) != ConstructType::None;

    m_target.set(vm, this, targetAsObject);
    m_handler.set(vm, this, handler);
}

static const char* s_proxyAlreadyRevokedErrorMessage = "Proxy has already been revoked. No more operations are allowed to be performed on it.";

static EncodedJSValue performProxyGet(ExecState* exec, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = exec->vm();
    if (!vm.isSafeToRecurse()) {
        throwStackOverflowError(exec);
        return JSValue::encode(JSValue());
    }

    JSObject* thisObject = jsCast<JSObject*>(JSValue::decode(thisValue)); // This might be a value where somewhere in __proto__ chain lives a ProxyObject.
    JSObject* proxyObjectAsObject = thisObject;
    // FIXME: make it so that custom getters take both the |this| value and the slotBase (property holder).
    // https://bugs.webkit.org/show_bug.cgi?id=154320
    while (true) {
        if (LIKELY(proxyObjectAsObject->type() == ProxyObjectType))
            break;

        JSValue prototype = proxyObjectAsObject->getPrototypeDirect();
        RELEASE_ASSERT(prototype.isObject());
        proxyObjectAsObject = asObject(prototype);
    }

    ProxyObject* proxyObject = jsCast<ProxyObject*>(proxyObjectAsObject);
    JSObject* target = proxyObject->target();

    if (propertyName == vm.propertyNames->underscoreProto)
        return JSValue::encode(proxyObject->performGetPrototype(exec));

    auto performDefaultGet = [&] {
        return JSValue::encode(target->get(exec, propertyName));
    };

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultGet();

    JSValue handlerValue = proxyObject->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue getHandler = handler->getMethod(exec, callData, callType, vm.propertyNames->get, ASCIILiteral("'get' property of a Proxy's handler object should be callable."));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    if (getHandler.isUndefined())
        return performDefaultGet();

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    arguments.append(thisObject);
    JSValue trapResult = call(exec, getHandler, callType, callData, handler, arguments);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    PropertyDescriptor descriptor;
    if (target->getOwnPropertyDescriptor(exec, propertyName, descriptor)) {
        if (descriptor.isDataDescriptor() && !descriptor.configurable() && !descriptor.writable()) {
            if (!sameValue(exec, descriptor.value(), trapResult))
                return throwVMTypeError(exec, ASCIILiteral("Proxy handler's 'get' result of a non-configurable and non-writable property should be the same value as the target's property."));
        } else if (descriptor.isAccessorDescriptor() && !descriptor.configurable() && descriptor.getter().isUndefined()) {
            if (!trapResult.isUndefined())
                return throwVMTypeError(exec, ASCIILiteral("Proxy handler's 'get' result of a non-configurable accessor property without a getter should be undefined."));
        }
    }

    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    return JSValue::encode(trapResult);
}

bool ProxyObject::performInternalMethodGetOwnProperty(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    JSObject* target = this->target();

    auto performDefaultGetOwnProperty = [&] {
        return target->methodTable(vm)->getOwnPropertySlot(target, exec, propertyName, slot);
    };

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultGetOwnProperty();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue getOwnPropertyDescriptorMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "getOwnPropertyDescriptor"), ASCIILiteral("'getOwnPropertyDescriptor' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return false;
    if (getOwnPropertyDescriptorMethod.isUndefined())
        return performDefaultGetOwnProperty();

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    JSValue trapResult = call(exec, getOwnPropertyDescriptorMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return false;

    if (!trapResult.isUndefined() && !trapResult.isObject()) {
        throwVMTypeError(exec, ASCIILiteral("result of 'getOwnPropertyDescriptor' call should either be an Object or undefined."));
        return false;
    }

    PropertyDescriptor targetPropertyDescriptor;
    bool isTargetPropertyDescriptorDefined = target->getOwnPropertyDescriptor(exec, propertyName, targetPropertyDescriptor);
    if (exec->hadException())
        return false;

    if (trapResult.isUndefined()) {
        if (!isTargetPropertyDescriptorDefined)
            return false;
        if (!targetPropertyDescriptor.configurable()) {
            throwVMTypeError(exec, ASCIILiteral("When the result of 'getOwnPropertyDescriptor' is undefined the target must be configurable."));
            return false;
        }
        // FIXME: this doesn't work if 'target' is another Proxy. We don't have isExtensible implemented in a way that fits w/ Proxys.
        // https://bugs.webkit.org/show_bug.cgi?id=154375
        bool isExtensible = target->isExtensible(exec);
        if (exec->hadException())
            return false;
        if (!isExtensible) {
            // FIXME: Come up with a test for this error. I'm not sure how to because
            // Object.seal(o) will make all fields [[Configurable]] false.
            // https://bugs.webkit.org/show_bug.cgi?id=154376
            throwVMTypeError(exec, ASCIILiteral("When 'getOwnPropertyDescriptor' returns undefined, the 'target' of a Proxy should be extensible."));
            return false;
        }

        return false;
    }

    bool isExtensible = target->isExtensible(exec);
    if (exec->hadException())
        return false;
    PropertyDescriptor trapResultAsDescriptor;
    toPropertyDescriptor(exec, trapResult, trapResultAsDescriptor);
    if (exec->hadException())
        return false;
    bool throwException = false;
    bool valid = validateAndApplyPropertyDescriptor(exec, nullptr, propertyName, isExtensible,
        trapResultAsDescriptor, isTargetPropertyDescriptorDefined, targetPropertyDescriptor, throwException);
    if (!valid) {
        throwVMTypeError(exec, ASCIILiteral("Result from 'getOwnPropertyDescriptor' fails the IsCompatiblePropertyDescriptor test."));
        return false;
    }

    if (!trapResultAsDescriptor.configurable()) {
        if (!isTargetPropertyDescriptorDefined || targetPropertyDescriptor.configurable()) {
            throwVMTypeError(exec, ASCIILiteral("Result from 'getOwnPropertyDescriptor' can't be non-configurable when the 'target' doesn't have it as an own property or if it is a configurable own property on 'target'."));
            return false;
        }
    }

    if (trapResultAsDescriptor.isAccessorDescriptor()) {
        GetterSetter* getterSetter = trapResultAsDescriptor.slowGetterSetter(exec);
        if (exec->hadException())
            return false;
        slot.setGetterSlot(this, trapResultAsDescriptor.attributes(), getterSetter);
    } else if (trapResultAsDescriptor.isDataDescriptor())
        slot.setValue(this, trapResultAsDescriptor.attributes(), trapResultAsDescriptor.value());
    else
        slot.setValue(this, trapResultAsDescriptor.attributes(), jsUndefined()); // We use undefined because it's the default value in object properties.

    return true;
}

bool ProxyObject::performHasProperty(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    JSObject* target = this->target();
    slot.setValue(this, None, jsUndefined()); // Nobody should rely on our value, but be safe and protect against any bad actors reading our value.

    auto performDefaultHasProperty = [&] {
        return target->methodTable(vm)->getOwnPropertySlot(target, exec, propertyName, slot);
    };

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultHasProperty();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue hasMethod = handler->getMethod(exec, callData, callType, vm.propertyNames->has, ASCIILiteral("'has' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return false;
    if (hasMethod.isUndefined())
        return performDefaultHasProperty();

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    JSValue trapResult = call(exec, hasMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return false;

    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (exec->hadException())
        return false;

    if (!trapResultAsBool) {
        PropertyDescriptor descriptor;
        bool isPropertyDescriptorDefined = target->getOwnPropertyDescriptor(exec, propertyName, descriptor); 
        if (exec->hadException())
            return false;
        if (isPropertyDescriptorDefined) {
            if (!descriptor.configurable()) {
                throwVMTypeError(exec, ASCIILiteral("Proxy 'has' must return 'true' for non-configurable properties."));
                return false;
            }
            bool isExtensible = target->isExtensible(exec);
            if (exec->hadException())
                return false;
            if (!isExtensible) {
                throwVMTypeError(exec, ASCIILiteral("Proxy 'has' must return 'true' for a non-extensible 'target' object with a configurable property."));
                return false;
            }
        }
    }

    return trapResultAsBool;
}

bool ProxyObject::getOwnPropertySlotCommon(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    slot.disableCaching();
    switch (slot.internalMethodType()) {
    case PropertySlot::InternalMethodType::Get:
        slot.setCustom(this, CustomAccessor, performProxyGet);
        return true;
    case PropertySlot::InternalMethodType::GetOwnProperty:
        return performInternalMethodGetOwnProperty(exec, propertyName, slot);
    case PropertySlot::InternalMethodType::HasProperty:
        return performHasProperty(exec, propertyName, slot);
    default:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool ProxyObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->getOwnPropertySlotCommon(exec, propertyName, slot);
}

bool ProxyObject::getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    Identifier ident = Identifier::from(exec, propertyName); 
    return thisObject->getOwnPropertySlotCommon(exec, ident.impl(), slot);
}

template <typename PerformDefaultPutFunction>
void ProxyObject::performPut(ExecState* exec, JSValue putValue, JSValue thisValue, PropertyName propertyName, PerformDefaultPutFunction performDefaultPut)
{
    VM& vm = exec->vm();
    if (!vm.isSafeToRecurse()) {
        throwStackOverflowError(exec);
        return;
    }

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultPut();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue setMethod = handler->getMethod(exec, callData, callType, vm.propertyNames->set, ASCIILiteral("'set' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return;
    JSObject* target = this->target();
    if (setMethod.isUndefined()) {
        performDefaultPut();
        return;
    }

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    arguments.append(putValue);
    arguments.append(thisValue);
    JSValue trapResult = call(exec, setMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return;
    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (exec->hadException())
        return;
    if (!trapResultAsBool)
        return;

    PropertyDescriptor descriptor;
    if (target->getOwnPropertyDescriptor(exec, propertyName, descriptor)) {
        if (descriptor.isDataDescriptor() && !descriptor.configurable() && !descriptor.writable()) {
            if (!sameValue(exec, descriptor.value(), putValue)) {
                throwVMTypeError(exec, ASCIILiteral("Proxy handler's 'set' on a non-configurable and non-writable property on 'target' should either return false or be the same value already on the 'target'."));
                return;
            }
        } else if (descriptor.isAccessorDescriptor() && !descriptor.configurable() && descriptor.setter().isUndefined()) {
            throwVMTypeError(exec, ASCIILiteral("Proxy handler's 'set' method on a non-configurable accessor property without a setter should return false."));
            return;
        }
    }
}

void ProxyObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    if (propertyName == vm.propertyNames->underscoreProto) {
        Base::put(cell, exec, propertyName, value, slot);
        return;
    }

    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    auto performDefaultPut = [&] () {
        JSObject* target = jsCast<JSObject*>(thisObject->target());
        target->methodTable(vm)->put(target, exec, propertyName, value, slot);
    };
    thisObject->performPut(exec, value, slot.thisValue(), propertyName, performDefaultPut);
}

void ProxyObject::putByIndexCommon(ExecState* exec, JSValue thisValue, unsigned propertyName, JSValue putValue, bool shouldThrow)
{
    VM& vm = exec->vm();
    Identifier ident = Identifier::from(exec, propertyName); 
    if (exec->hadException())
        return;
    auto performDefaultPut = [&] () {
        JSObject* target = this->target();
        bool isStrictMode = shouldThrow;
        PutPropertySlot slot(thisValue, isStrictMode); // We must preserve the "this" target of the putByIndex.
        target->methodTable(vm)->put(target, exec, ident.impl(), putValue, slot);
    };
    performPut(exec, putValue, thisValue, ident.impl(), performDefaultPut);
}

void ProxyObject::putByIndex(JSCell* cell, ExecState* exec, unsigned propertyName, JSValue value, bool shouldThrow)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    thisObject->putByIndexCommon(exec, thisObject, propertyName, value, shouldThrow);
}

static EncodedJSValue JSC_HOST_CALL performProxyCall(ExecState* exec)
{
    VM& vm = exec->vm();
    ProxyObject* proxy = jsCast<ProxyObject*>(exec->callee());
    JSValue handlerValue = proxy->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue applyMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "apply"), ASCIILiteral("'apply' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    JSObject* target = proxy->target();
    if (applyMethod.isUndefined()) {
        CallData callData;
        CallType callType = target->methodTable(vm)->getCallData(target, callData);
        RELEASE_ASSERT(callType != CallType::None);
        return JSValue::encode(call(exec, target, callType, callData, exec->thisValue(), ArgList(exec)));
    }

    JSArray* argArray = constructArray(exec, static_cast<ArrayAllocationProfile*>(nullptr), ArgList(exec));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(exec->thisValue());
    arguments.append(argArray);
    return JSValue::encode(call(exec, applyMethod, callType, callData, handler, arguments));
}

CallType ProxyObject::getCallData(JSCell* cell, CallData& callData)
{
    ProxyObject* proxy = jsCast<ProxyObject*>(cell);
    if (!proxy->m_isCallable) {
        callData.js.functionExecutable = nullptr;
        callData.js.scope = nullptr;
        return CallType::None;
    }

    callData.native.function = performProxyCall;
    return CallType::Host;
}

static EncodedJSValue JSC_HOST_CALL performProxyConstruct(ExecState* exec)
{
    VM& vm = exec->vm();
    ProxyObject* proxy = jsCast<ProxyObject*>(exec->callee());
    JSValue handlerValue = proxy->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue constructMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "construct"), ASCIILiteral("'construct' property of a Proxy's handler should be constructible."));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    JSObject* target = proxy->target();
    if (constructMethod.isUndefined()) {
        ConstructData constructData;
        ConstructType constructType = target->methodTable(vm)->getConstructData(target, constructData);
        RELEASE_ASSERT(constructType != ConstructType::None);
        return JSValue::encode(construct(exec, target, constructType, constructData, ArgList(exec), exec->newTarget()));
    }

    JSArray* argArray = constructArray(exec, static_cast<ArrayAllocationProfile*>(nullptr), ArgList(exec));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(argArray);
    arguments.append(exec->newTarget());
    JSValue result = call(exec, constructMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    if (!result.isObject())
        return throwVMTypeError(exec, ASCIILiteral("Result from Proxy handler's 'construct' method should be an object."));
    return JSValue::encode(result);
}

ConstructType ProxyObject::getConstructData(JSCell* cell, ConstructData& constructData)
{
    ProxyObject* proxy = jsCast<ProxyObject*>(cell);
    if (!proxy->m_isConstructible) {
        constructData.js.functionExecutable = nullptr;
        constructData.js.scope = nullptr;
        return ConstructType::None;
    }

    constructData.native.function = performProxyConstruct;
    return ConstructType::Host;
}

template <typename DefaultDeleteFunction>
bool ProxyObject::performDelete(ExecState* exec, PropertyName propertyName, DefaultDeleteFunction performDefaultDelete)
{
    VM& vm = exec->vm();

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultDelete();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue deletePropertyMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "deleteProperty"), ASCIILiteral("'deleteProperty' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return false;
    JSObject* target = this->target();
    if (deletePropertyMethod.isUndefined())
        return performDefaultDelete();

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    JSValue trapResult = call(exec, deletePropertyMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return false;

    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (exec->hadException())
        return false;

    if (!trapResultAsBool)
        return false;

    PropertyDescriptor descriptor;
    if (target->getOwnPropertyDescriptor(exec, propertyName, descriptor)) {
        if (!descriptor.configurable()) {
            throwVMTypeError(exec, ASCIILiteral("Proxy handler's 'deleteProperty' method should return false when the target's property is not configurable."));
            return false;
        }
    }

    if (exec->hadException())
        return false;

    return true;
}

bool ProxyObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    auto performDefaultDelete = [&] () -> bool {
        JSObject* target = thisObject->target();
        return target->methodTable(exec->vm())->deleteProperty(target, exec, propertyName);
    };
    return thisObject->performDelete(exec, propertyName, performDefaultDelete);
}

bool ProxyObject::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    Identifier ident = Identifier::from(exec, propertyName); 
    auto performDefaultDelete = [&] () -> bool {
        JSObject* target = thisObject->target();
        return target->methodTable(exec->vm())->deletePropertyByIndex(target, exec, propertyName);
    };
    return thisObject->performDelete(exec, ident.impl(), performDefaultDelete);
}

bool ProxyObject::performPreventExtensions(ExecState* exec)
{
    VM& vm = exec->vm();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue preventExtensionsMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "preventExtensions"), ASCIILiteral("'preventExtensions' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return false;
    JSObject* target = this->target();
    if (preventExtensionsMethod.isUndefined())
        return target->methodTable(vm)->preventExtensions(target, exec);

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    JSValue trapResult = call(exec, preventExtensionsMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return false;

    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (exec->hadException())
        return false;

    if (trapResultAsBool) {
        bool targetIsExtensible = target->isExtensible(exec);
        if (exec->hadException())
            return false;
        if (targetIsExtensible) {
            throwVMTypeError(exec, ASCIILiteral("Proxy's 'preventExtensions' trap returned true even though its target is extensible. It should have returned false."));
            return false;
        }
    }

    return trapResultAsBool;
}

bool ProxyObject::preventExtensions(JSObject* object, ExecState* exec)
{
    return jsCast<ProxyObject*>(object)->performPreventExtensions(exec);
}

bool ProxyObject::performIsExtensible(ExecState* exec)
{
    VM& vm = exec->vm();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue isExtensibleMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "isExtensible"), ASCIILiteral("'isExtensible' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return false;

    JSObject* target = this->target();
    if (isExtensibleMethod.isUndefined())
        return target->isExtensible(exec);

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    JSValue trapResult = call(exec, isExtensibleMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return false;

    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (exec->hadException())
        return false;

    bool isTargetExtensible = target->isExtensible(exec);
    if (exec->hadException())
        return false;

    if (trapResultAsBool != isTargetExtensible) {
        if (isTargetExtensible) {
            ASSERT(!trapResultAsBool);
            throwVMTypeError(exec, ASCIILiteral("Proxy object's 'isExtensible' trap returned false when the target is extensible. It should have returned true."));
        } else {
            ASSERT(!isTargetExtensible);
            ASSERT(trapResultAsBool);
            throwVMTypeError(exec, ASCIILiteral("Proxy object's 'isExtensible' trap returned true when the target is non-extensible. It should have returned false."));
        }
    }
    
    return trapResultAsBool;
}

bool ProxyObject::isExtensible(JSObject* object, ExecState* exec)
{
    return jsCast<ProxyObject*>(object)->performIsExtensible(exec);
}

bool ProxyObject::performDefineOwnProperty(ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = exec->vm();

    JSObject* target = this->target();
    auto performDefaultDefineOwnProperty = [&] {
        return target->methodTable(vm)->defineOwnProperty(target, exec, propertyName, descriptor, shouldThrow);
    };

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultDefineOwnProperty();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue definePropertyMethod = handler->getMethod(exec, callData, callType, vm.propertyNames->defineProperty, ASCIILiteral("'defineProperty' property of a Proxy's handler should be callable."));
    if (vm.exception())
        return false;

    if (definePropertyMethod.isUndefined())
        return performDefaultDefineOwnProperty();

    JSObject* descriptorObject = constructObjectFromPropertyDescriptor(exec, descriptor);
    if (vm.exception())
        return false;

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    arguments.append(descriptorObject);
    JSValue trapResult = call(exec, definePropertyMethod, callType, callData, handler, arguments);
    if (vm.exception())
        return false;

    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (vm.exception())
        return false;

    if (!trapResultAsBool)
        return false;

    PropertyDescriptor targetDescriptor;
    bool isTargetDescriptorDefined = target->getOwnPropertyDescriptor(exec, propertyName, targetDescriptor);
    if (vm.exception())
        return false;

    bool targetIsExtensible = target->isExtensible(exec);
    if (vm.exception())
        return false;
    bool settingConfigurableToFalse = descriptor.configurablePresent() && !descriptor.configurable();

    if (!isTargetDescriptorDefined) {
        if (!targetIsExtensible) {
            throwVMTypeError(exec, ASCIILiteral("Proxy's 'defineProperty' trap returned true even though getOwnPropertyDescriptor of the Proxy's target returned undefined and the target is non-extensible."));
            return false;
        }
        if (settingConfigurableToFalse) {
            throwVMTypeError(exec, ASCIILiteral("Proxy's 'defineProperty' trap returned true for a non-configurable field even though getOwnPropertyDescriptor of the Proxy's target returned undefined."));
            return false;
        }

        return true;
    } 

    ASSERT(isTargetDescriptorDefined);
    bool isCurrentDefined = isTargetDescriptorDefined;
    const PropertyDescriptor& current = targetDescriptor;
    bool throwException = false;
    bool isCompatibleDescriptor = validateAndApplyPropertyDescriptor(exec, nullptr, propertyName, targetIsExtensible, descriptor, isCurrentDefined, current, throwException);
    if (!isCompatibleDescriptor) {
        throwVMTypeError(exec, ASCIILiteral("Proxy's 'defineProperty' trap did not define a property on its target that is compatible with the trap's input descriptor."));
        return false;
    }
    if (settingConfigurableToFalse && targetDescriptor.configurable()) {
        throwVMTypeError(exec, ASCIILiteral("Proxy's 'defineProperty' trap did not define a non-configurable property on its target even though the input descriptor to the trap said it must do so."));
        return false;
    }
    
    return true;
}

bool ProxyObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->performDefineOwnProperty(exec, propertyName, descriptor, shouldThrow);
}

void ProxyObject::performGetOwnPropertyNames(ExecState* exec, PropertyNameArray& trapResult, EnumerationMode enumerationMode)
{
    VM& vm = exec->vm();
    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue ownKeysMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "ownKeys"), ASCIILiteral("'ownKeys' property of a Proxy's handler should be callable."));
    if (exec->hadException())
        return;
    JSObject* target = this->target();
    if (ownKeysMethod.isUndefined()) {
        target->methodTable(exec->vm())->getOwnPropertyNames(target, exec, trapResult, enumerationMode);
        return;
    }

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    JSValue arrayLikeObject = call(exec, ownKeysMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return;

    PropertyNameMode propertyNameMode = trapResult.mode();
    RuntimeTypeMask resultFilter = 0;
    switch (propertyNameMode) {
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
    RuntimeTypeMask dontThrowAnExceptionTypeFilter = TypeString | TypeSymbol;
    HashMap<UniquedStringImpl*, unsigned> uncheckedResultKeys;
    unsigned totalSize = 0;

    auto addPropName = [&] (JSValue value, RuntimeType type) -> bool {
        static const bool doExitEarly = true;
        static const bool dontExitEarly = false;

        if (!(type & resultFilter))
            return dontExitEarly;

        Identifier ident = value.toPropertyKey(exec);
        if (exec->hadException())
            return doExitEarly;

        ++uncheckedResultKeys.add(ident.impl(), 0).iterator->value;
        ++totalSize;

        trapResult.add(ident.impl());

        return dontExitEarly;
    };

    createListFromArrayLike(exec, arrayLikeObject, dontThrowAnExceptionTypeFilter, ASCIILiteral("Proxy handler's 'ownKeys' method must return a array-like object containing only Strings and Symbols."), addPropName);
    if (exec->hadException())
        return;

    bool targetIsExensible = target->isExtensible(exec);

    PropertyNameArray targetKeys(&vm, propertyNameMode);
    target->methodTable(vm)->getOwnPropertyNames(target, exec, targetKeys, enumerationMode);
    if (exec->hadException())
        return;
    Vector<UniquedStringImpl*> targetConfigurableKeys;
    Vector<UniquedStringImpl*> targetNonConfigurableKeys;
    for (const Identifier& ident : targetKeys) {
        PropertyDescriptor descriptor;
        bool isPropertyDefined = target->getOwnPropertyDescriptor(exec, ident.impl(), descriptor); 
        if (exec->hadException())
            return;
        if (isPropertyDefined && !descriptor.configurable())
            targetNonConfigurableKeys.append(ident.impl());
        else
            targetConfigurableKeys.append(ident.impl());
    }

    auto removeIfContainedInUncheckedResultKeys = [&] (UniquedStringImpl* impl) -> bool {
        static const bool isContainedIn = true;
        static const bool isNotContainedIn = false;

        auto iter = uncheckedResultKeys.find(impl);
        if (iter == uncheckedResultKeys.end())
            return isNotContainedIn;

        unsigned& count = iter->value;
        if (count == 0)
            return isNotContainedIn;

        --count;
        --totalSize;
        return isContainedIn;
    };

    for (UniquedStringImpl* impl : targetNonConfigurableKeys) {
        bool contains = removeIfContainedInUncheckedResultKeys(impl);
        if (!contains) {
            throwVMTypeError(exec, makeString("Proxy object's 'target' has the non-configurable property '", String(impl), "' that was not in the result from the 'ownKeys' trap."));
            return;
        }
    }

    if (targetIsExensible)
        return;

    for (UniquedStringImpl* impl : targetConfigurableKeys) {
        bool contains = removeIfContainedInUncheckedResultKeys(impl);
        if (!contains) {
            throwVMTypeError(exec, makeString("Proxy object's non-extensible 'target' has configurable property '", String(impl), "' that was not in the result from the 'ownKeys' trap."));
            return;
        }
    }

#ifndef NDEBUG
    unsigned sum = 0;
    for (unsigned keyCount : uncheckedResultKeys.values())
        sum += keyCount;
    ASSERT(sum == totalSize);
#endif

    if (totalSize) {
        throwVMTypeError(exec, ASCIILiteral("Proxy handler's 'ownKeys' method returned a key that was not present in its target or it returned duplicate keys."));
        return;
    }
}

void ProxyObject::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNameArray, EnumerationMode enumerationMode)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    thisObject->performGetOwnPropertyNames(exec, propertyNameArray, enumerationMode);
}

void ProxyObject::getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void ProxyObject::getStructurePropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    // We should always go down the getOwnPropertyNames path.
    RELEASE_ASSERT_NOT_REACHED();
}

void ProxyObject::getGenericPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool ProxyObject::performSetPrototype(ExecState* exec, JSValue prototype, bool shouldThrowIfCantSet)
{
    ASSERT(prototype.isObject() || prototype.isNull());

    VM& vm = exec->vm();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue setPrototypeOfMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "setPrototypeOf"), ASCIILiteral("'setPrototypeOf' property of a Proxy's handler should be callable."));
    if (vm.exception())
        return false;

    JSObject* target = this->target();
    if (setPrototypeOfMethod.isUndefined())
        return target->setPrototype(vm, exec, prototype, shouldThrowIfCantSet);

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(prototype);
    JSValue trapResult = call(exec, setPrototypeOfMethod, callType, callData, handler, arguments);
    if (vm.exception())
        return false;

    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (vm.exception())
        return false;
    
    if (!trapResultAsBool) {
        if (shouldThrowIfCantSet)
            throwVMTypeError(exec, ASCIILiteral("Proxy 'setPrototypeOf' returned false indicating it could not set the prototype value. The operation was expected to succeed."));
        return false;
    }

    bool targetIsExtensible = target->isExtensible(exec);
    if (vm.exception())
        return false;
    if (targetIsExtensible)
        return true;

    JSValue targetPrototype = target->getPrototype(vm, exec);
    if (vm.exception())
        return false;
    if (!sameValue(exec, prototype, targetPrototype)) {
        throwVMTypeError(exec, ASCIILiteral("Proxy 'setPrototypeOf' trap returned true when its target is non-extensible and the new prototype value is not the same as the current prototype value. It should have returned false."));
        return false;
    }

    return true;
}

bool ProxyObject::setPrototype(JSObject* object, ExecState* exec, JSValue prototype, bool shouldThrowIfCantSet)
{
    return jsCast<ProxyObject*>(object)->performSetPrototype(exec, prototype, shouldThrowIfCantSet);
}

JSValue ProxyObject::performGetPrototype(ExecState* exec)
{
    VM& vm = exec->vm();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral(s_proxyAlreadyRevokedErrorMessage));
        return JSValue();
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue getPrototypeOfMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "getPrototypeOf"), ASCIILiteral("'getPrototypeOf' property of a Proxy's handler should be callable."));
    if (vm.exception())
        return JSValue();

    JSObject* target = this->target();
    if (getPrototypeOfMethod.isUndefined())
        return target->getPrototype(vm, exec);

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    JSValue trapResult = call(exec, getPrototypeOfMethod, callType, callData, handler, arguments);
    if (vm.exception())
        return JSValue();

    if (!trapResult.isObject() && !trapResult.isNull()) {
        throwVMTypeError(exec, ASCIILiteral("Proxy handler's 'getPrototypeOf' trap should either return an object or null."));
        return JSValue();
    }

    bool targetIsExtensible = target->isExtensible(exec);
    if (vm.exception())
        return JSValue();
    if (targetIsExtensible)
        return trapResult;

    JSValue targetPrototype = target->getPrototype(vm, exec);
    if (vm.exception())
        return JSValue();
    if (!sameValue(exec, targetPrototype, trapResult)) {
        throwVMTypeError(exec, ASCIILiteral("Proxy's 'getPrototypeOf' trap for a non-extensible target should return the same value as the target's prototype."));
        return JSValue();
    }

    return trapResult;
}

JSValue ProxyObject::getPrototype(JSObject* object, ExecState* exec)
{
    return jsCast<ProxyObject*>(object)->performGetPrototype(exec);
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

    visitor.append(&thisObject->m_target);
    visitor.append(&thisObject->m_handler);
}

} // namespace JSC
