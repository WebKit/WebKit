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

void ProxyObject::finishCreation(VM& vm, ExecState* exec, JSValue target, JSValue handler)
{
    Base::finishCreation(vm);
    if (!target.isObject()) {
        throwTypeError(exec, ASCIILiteral("A Proxy's 'target' should be an Object"));
        return;
    }
    if (ProxyObject* targetAsProxy = jsDynamicCast<ProxyObject*>(target)) {
        // FIXME: Add tests for this once we implement Proxy.revoke(.).
        // https://bugs.webkit.org/show_bug.cgi?id=154321
        if (targetAsProxy->handler().isNull()) {
            throwTypeError(exec, ASCIILiteral("If a Proxy's handler is another Proxy object, the other Proxy object must have a non-null handler."));
            return;
        }
    }
    if (!handler.isObject()) {
        throwTypeError(exec, ASCIILiteral("A Proxy's 'handler' should be an Object"));
        return;
    }

    JSObject* targetAsObject = jsCast<JSObject*>(target);

    CallData ignoredCallData;
    m_isCallable = targetAsObject->methodTable(vm)->getCallData(targetAsObject, ignoredCallData) != CallTypeNone;

    ConstructData ignoredConstructData;
    m_isConstructible = jsCast<JSObject*>(target)->methodTable(vm)->getConstructData(jsCast<JSObject*>(target), ignoredConstructData) != ConstructTypeNone;

    m_target.set(vm, this, targetAsObject);
    m_handler.set(vm, this, handler);
}

static EncodedJSValue performProxyGet(ExecState* exec, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = exec->vm();
    JSObject* thisObject = jsCast<JSObject*>(JSValue::decode(thisValue)); // This might be a value where somewhere in __proto__ chain lives a ProxyObject.
    JSObject* proxyObjectAsObject = thisObject;
    // FIXME: make it so that custom getters take both the |this| value and the slotBase (property holder).
    // https://bugs.webkit.org/show_bug.cgi?id=154320
    while (true) {
        if (LIKELY(proxyObjectAsObject->inherits(ProxyObject::info())))
            break;

        Structure& structure = *vm.heap.structureIDTable().get(proxyObjectAsObject->structureID());
        JSValue prototype = structure.storedPrototype();
        RELEASE_ASSERT(prototype.isObject());
        proxyObjectAsObject = asObject(prototype);
    }

    ProxyObject* proxyObject = jsCast<ProxyObject*>(proxyObjectAsObject);
    JSObject* target = proxyObject->target();

    auto performDefaultGet = [&] {
        return JSValue::encode(target->get(exec, propertyName));
    };

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultGet();

    JSValue handlerValue = proxyObject->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));

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
        throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));
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
        bool isExtensible = target->isExtensibleInline(exec);
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

    bool isExtensible = target->isExtensibleInline(exec);
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
        throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));
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
            bool isExtensible = target->isExtensibleInline(exec);
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
    if (exec->hadException())
        return false;
    return thisObject->getOwnPropertySlotCommon(exec, ident.impl(), slot);
}

template <typename PerformDefaultPutFunction>
void ProxyObject::performPut(ExecState* exec, JSValue putValue, JSValue thisValue, PropertyName propertyName, PerformDefaultPutFunction performDefaultPut)
{
    VM& vm = exec->vm();

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultPut();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));
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
        return throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));

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
        RELEASE_ASSERT(callType != CallTypeNone);
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
        return CallTypeNone;
    }

    callData.native.function = performProxyCall;
    return CallTypeHost;
}

static EncodedJSValue JSC_HOST_CALL performProxyConstruct(ExecState* exec)
{
    VM& vm = exec->vm();
    ProxyObject* proxy = jsCast<ProxyObject*>(exec->callee());
    JSValue handlerValue = proxy->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));

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
        RELEASE_ASSERT(constructType != ConstructTypeNone);
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
        return ConstructTypeNone;
    }

    constructData.native.function = performProxyConstruct;
    return ConstructTypeHost;
}

template <typename DefaultDeleteFunction>
bool ProxyObject::performDelete(ExecState* exec, PropertyName propertyName, DefaultDeleteFunction performDefaultDelete)
{
    VM& vm = exec->vm();

    if (vm.propertyNames->isPrivateName(Identifier::fromUid(&vm, propertyName.uid())))
        return performDefaultDelete();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));
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
        JSObject* target = jsCast<JSObject*>(thisObject->target());
        return target->methodTable(exec->vm())->deleteProperty(target, exec, propertyName);
    };
    return thisObject->performDelete(exec, propertyName, performDefaultDelete);
}

bool ProxyObject::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    Identifier ident = Identifier::from(exec, propertyName); 
    if (exec->hadException())
        return false;
    auto performDefaultDelete = [&] () -> bool {
        JSObject* target = jsCast<JSObject*>(thisObject->target());
        return target->methodTable(exec->vm())->deletePropertyByIndex(target, exec, propertyName);
    };
    return thisObject->performDelete(exec, ident.impl(), performDefaultDelete);
}

bool ProxyObject::performPreventExtensions(ExecState* exec)
{
    VM& vm = exec->vm();

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));
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
        bool targetIsExtensible = target->isExtensibleInline(exec);
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
        throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));
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
        return target->isExtensibleInline(exec);

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    JSValue trapResult = call(exec, isExtensibleMethod, callType, callData, handler, arguments);
    if (exec->hadException())
        return false;

    bool trapResultAsBool = trapResult.toBoolean(exec);
    if (exec->hadException())
        return false;

    bool isTargetExtensible = target->isExtensibleInline(exec);
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

void ProxyObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_target);
    visitor.append(&thisObject->m_handler);
}

} // namespace JSC
