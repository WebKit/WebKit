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
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
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

    m_target.set(vm, this, jsCast<JSObject*>(target));
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
    JSValue handlerValue = proxyObject->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, ASCIILiteral("Proxy 'handler' is null. It should be an Object."));

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    JSObject* target = proxyObject->target();
    JSValue getHandler = handler->get(exec, vm.propertyNames->get);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    if (getHandler.isUndefined())
        return JSValue::encode(target->get(exec, propertyName));

    if (!getHandler.isCell())
        return throwVMTypeError(exec, ASCIILiteral("'get' property of Proxy's handler should be callable."));

    CallData callData;
    CallType callType = getHandler.asCell()->methodTable()->getCallData(getHandler.asCell(), callData);
    if (callType == CallTypeNone)
        return throwVMTypeError(exec, ASCIILiteral("'get' property of Proxy's handler should be callable."));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(jsString(exec, propertyName.uid()));
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

    // FIXME: when implementing Proxy.[[GetOwnProperty]] it would be a good test to
    // have handler be another Proxy where its handler throws on [[GetOwnProperty]]
    // right here.
    // https://bugs.webkit.org/show_bug.cgi?id=154314
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    return JSValue::encode(trapResult);
}

bool ProxyObject::getOwnPropertySlotCommon(ExecState*, PropertySlot& slot)
{
    if (slot.internalMethodType() == PropertySlot::InternalMethodType::Get) {
        slot.setCustom(this, CustomAccessor, performProxyGet);
        slot.disableCaching();
        return true;
    }

    // FIXME: Implement Proxy.[[HasProperty]] and Proxy.[[GetOwnProperty]]
    // https://bugs.webkit.org/show_bug.cgi?id=154313
    // https://bugs.webkit.org/show_bug.cgi?id=154314
    return false;
}
bool ProxyObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName, PropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->getOwnPropertySlotCommon(exec, slot);
}

bool ProxyObject::getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned, PropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->getOwnPropertySlotCommon(exec, slot);
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
