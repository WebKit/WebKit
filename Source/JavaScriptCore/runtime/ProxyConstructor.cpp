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
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "ProxyObject.h"
#include "ProxyRevoke.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ProxyConstructor);

const ClassInfo ProxyConstructor::s_info = { "Proxy", &Base::s_info, 0, CREATE_METHOD_TABLE(ProxyConstructor) };

ProxyConstructor* ProxyConstructor::create(VM& vm, Structure* structure)
{
    ProxyConstructor* constructor = new (NotNull, allocateCell<ProxyConstructor>(vm.heap)) ProxyConstructor(vm, structure);
    constructor->finishCreation(vm, "Proxy", structure->globalObject());
    return constructor;
}

ProxyConstructor::ProxyConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

static EncodedJSValue JSC_HOST_CALL makeRevocableProxy(ExecState* exec)
{
    if (exec->argumentCount() < 2)
        return throwVMTypeError(exec, ASCIILiteral("Proxy.revocable needs to be called with two arguments: the target and the handler"));

    VM& vm = exec->vm();
    ArgList args(exec);
    JSValue target = args.at(0);
    JSValue handler = args.at(1);
    ProxyObject* proxy = ProxyObject::create(exec, exec->lexicalGlobalObject(), target, handler);
    if (vm.exception())
        return JSValue::encode(JSValue());
    ProxyRevoke* revoke = ProxyRevoke::create(vm, exec->lexicalGlobalObject()->proxyRevokeStructure(), proxy);
    if (vm.exception())
        return JSValue::encode(JSValue());

    JSObject* result = constructEmptyObject(exec);
    if (vm.exception())
        return JSValue::encode(JSValue());
    result->putDirect(vm, makeIdentifier(vm, "proxy"), proxy, None);
    result->putDirect(vm, makeIdentifier(vm, "revoke"), revoke, None);

    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL proxyRevocableConstructorThrowError(ExecState* exec)
{
    return throwVMTypeError(exec, ASCIILiteral("Proxy.revocable cannot be constructed. It can only be called"));
}

void ProxyConstructor::finishCreation(VM& vm, const char* name, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm, name);

    putDirect(vm, vm.propertyNames->length, jsNumber(2), ReadOnly | DontDelete | DontEnum);
    putDirect(vm, makeIdentifier(vm, "revocable"), JSFunction::create(vm, globalObject, 2, ASCIILiteral("revocable"), makeRevocableProxy, NoIntrinsic, proxyRevocableConstructorThrowError));
}

static EncodedJSValue JSC_HOST_CALL constructProxyObject(ExecState* exec)
{
    if (exec->newTarget().isUndefined())
        return throwVMTypeError(exec, ASCIILiteral("new.target of Proxy construct should not be undefined"));

    ArgList args(exec);
    JSValue target = args.at(0);
    JSValue handler = args.at(1);
    return JSValue::encode(ProxyObject::create(exec, exec->lexicalGlobalObject(), target, handler));
}

ConstructType ProxyConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructProxyObject;
    return ConstructType::Host;
}

static EncodedJSValue JSC_HOST_CALL callProxy(ExecState* exec)
{
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(exec, "Proxy"));
}

CallType ProxyConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callProxy;
    return CallType::Host;
}

} // namespace JSC
