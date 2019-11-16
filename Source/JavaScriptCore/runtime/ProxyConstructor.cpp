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
#include "ProxyConstructor.h"

#include "Error.h"
#include "IdentifierInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "ProxyObject.h"
#include "ProxyRevoke.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ProxyConstructor);

const ClassInfo ProxyConstructor::s_info = { "Proxy", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ProxyConstructor) };

ProxyConstructor* ProxyConstructor::create(VM& vm, Structure* structure)
{
    ProxyConstructor* constructor = new (NotNull, allocateCell<ProxyConstructor>(vm.heap)) ProxyConstructor(vm, structure);
    constructor->finishCreation(vm, "Proxy", structure->globalObject());
    return constructor;
}

static EncodedJSValue JSC_HOST_CALL callProxy(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructProxyObject(JSGlobalObject*, CallFrame*);

ProxyConstructor::ProxyConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callProxy, constructProxyObject)
{
}

static EncodedJSValue JSC_HOST_CALL makeRevocableProxy(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (callFrame->argumentCount() < 2)
        return throwVMTypeError(globalObject, scope, "Proxy.revocable needs to be called with two arguments: the target and the handler"_s);

    ArgList args(callFrame);
    JSValue target = args.at(0);
    JSValue handler = args.at(1);
    ProxyObject* proxy = ProxyObject::create(globalObject, target, handler);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    ProxyRevoke* revoke = ProxyRevoke::create(vm, globalObject->proxyRevokeStructure(), proxy);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSObject* result = constructEmptyObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    result->putDirect(vm, makeIdentifier(vm, "proxy"), proxy, static_cast<unsigned>(PropertyAttribute::None));
    result->putDirect(vm, makeIdentifier(vm, "revoke"), revoke, static_cast<unsigned>(PropertyAttribute::None));

    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL proxyRevocableConstructorThrowError(JSGlobalObject* globalObject, CallFrame*)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return throwVMTypeError(globalObject, scope, "Proxy.revocable cannot be constructed. It can only be called"_s);
}

void ProxyConstructor::finishCreation(VM& vm, const char* name, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm, name, NameAdditionMode::WithStructureTransition);
    putDirect(vm, vm.propertyNames->length, jsNumber(2), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirect(vm, makeIdentifier(vm, "revocable"), JSFunction::create(vm, globalObject, 2, "revocable"_s, makeRevocableProxy, NoIntrinsic, proxyRevocableConstructorThrowError));
}

static EncodedJSValue JSC_HOST_CALL constructProxyObject(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    if (callFrame->newTarget().isUndefined())
        return throwVMTypeError(globalObject, scope, "new.target of Proxy construct should not be undefined"_s);

    ArgList args(callFrame);
    JSValue target = args.at(0);
    JSValue handler = args.at(1);
    RELEASE_AND_RETURN(scope, JSValue::encode(ProxyObject::create(globalObject, target, handler)));
}

static EncodedJSValue JSC_HOST_CALL callProxy(JSGlobalObject* globalObject, CallFrame*)
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Proxy"));
}

} // namespace JSC
