/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "ProxyConstructor.h"

#include "JSCInlines.h"
#include "JSFunctionWithFields.h"
#include "ObjectConstructor.h"
#include "ProxyObject.h"
#include "VMInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ProxyConstructor);

const ClassInfo ProxyConstructor::s_info = { "Proxy"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ProxyConstructor) };

ProxyConstructor* ProxyConstructor::create(VM& vm, Structure* structure)
{
    ProxyConstructor* constructor = new (NotNull, allocateCell<ProxyConstructor>(vm)) ProxyConstructor(vm, structure);
    constructor->finishCreation(vm, structure->realm());
    return constructor;
}

static JSC_DECLARE_HOST_FUNCTION(callProxy);
static JSC_DECLARE_HOST_FUNCTION(constructProxyObject);
static JSC_DECLARE_HOST_FUNCTION(makeRevocableProxy);

ProxyConstructor::ProxyConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callProxy, constructProxyObject)
{
}

static constexpr PropertyOffset proxyRevocableResultProxyPropertyOffset = 0;
static constexpr PropertyOffset proxyRevocableResultRevokePropertyOffset = 1;

Structure* createProxyRevocableResultStructure(VM& vm, JSGlobalObject& globalObject)
{
    constexpr unsigned inlineCapacity = 2;
    Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), inlineCapacity);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->proxy, 0, offset);
    RELEASE_ASSERT(offset == proxyRevocableResultProxyPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->revoke, 0, offset);
    RELEASE_ASSERT(offset == proxyRevocableResultRevokePropertyOffset);
    return structure;
}

JSC_DEFINE_HOST_FUNCTION(proxyRevoke, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    JSValue proxyValue = callee->getField(JSFunctionWithFields::Field::RevocableProxy);
    if (proxyValue.isNull())
        return JSValue::encode(jsUndefined());

    ProxyObject* proxy = uncheckedDowncast<ProxyObject>(proxyValue);
    VM& vm = globalObject->vm();
    proxy->revoke(vm);
    callee->setField(vm, JSFunctionWithFields::Field::RevocableProxy, jsNull());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(makeRevocableProxy, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (callFrame->argumentCount() < 2)
        return throwVMTypeError(globalObject, scope, "Proxy.revocable needs to be called with two arguments: the target and the handler"_s);

    JSValue target = callFrame->argument(0);
    JSValue handler = callFrame->argument(1);
    ProxyObject* proxy = ProxyObject::create(globalObject, target, handler);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto* revoke = JSFunctionWithFields::create(vm, globalObject, vm.proxyRevokeExecutable());
    revoke->setField(vm, JSFunctionWithFields::Field::RevocableProxy, proxy);

    JSObject* result = constructEmptyObject(vm, globalObject->proxyRevocableResultStructure());
    result->putDirectOffset(vm, proxyRevocableResultProxyPropertyOffset, proxy);
    result->putDirectOffset(vm, proxyRevocableResultRevokePropertyOffset, revoke);

    return JSValue::encode(result);
}

void ProxyConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm, 2, "Proxy"_s, PropertyAdditionMode::WithoutStructureTransition);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("revocable"_s, makeRevocableProxy, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
}

JSC_DEFINE_HOST_FUNCTION(constructProxyObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue target = callFrame->argument(0);
    JSValue handler = callFrame->argument(1);
    return JSValue::encode(ProxyObject::create(globalObject, target, handler));
}

JSC_DEFINE_HOST_FUNCTION(callProxy, (JSGlobalObject* globalObject, CallFrame*))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Proxy"_s));
}

} // namespace JSC
