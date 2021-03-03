/*
 * Copyright (C) 2016-2021 Apple Inc. All Rights Reserved.
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
#include "ProxyRevoke.h"

#include "JSCInlines.h"
#include "ProxyObject.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ProxyRevoke);

const ClassInfo ProxyRevoke::s_info = { "ProxyRevoke", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ProxyRevoke) };

ProxyRevoke* ProxyRevoke::create(VM& vm, Structure* structure, ProxyObject* proxy)
{
    ProxyRevoke* revoke = new (NotNull, allocateCell<ProxyRevoke>(vm.heap)) ProxyRevoke(vm, structure);
    revoke->finishCreation(vm, proxy);
    return revoke;
}

static JSC_DECLARE_HOST_FUNCTION(performProxyRevoke);

ProxyRevoke::ProxyRevoke(VM& vm, Structure* structure)
    : Base(vm, structure, performProxyRevoke, nullptr)
{
}

void ProxyRevoke::finishCreation(VM& vm, ProxyObject* proxy)
{
    Base::finishCreation(vm, 0, emptyString());
    m_proxy.set(vm, this, proxy);
}

JSC_DEFINE_HOST_FUNCTION(performProxyRevoke, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ProxyRevoke* proxyRevoke = jsCast<ProxyRevoke*>(callFrame->jsCallee());
    JSValue proxyValue = proxyRevoke->proxy();
    if (proxyValue.isNull())
        return JSValue::encode(jsUndefined());

    ProxyObject* proxy = jsCast<ProxyObject*>(proxyValue);
    VM& vm = globalObject->vm();
    proxy->revoke(vm);
    proxyRevoke->setProxyToNull(vm);
    return JSValue::encode(jsUndefined());
}

template<typename Visitor>
void ProxyRevoke::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    ProxyRevoke* thisObject = jsCast<ProxyRevoke*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_proxy);
}

DEFINE_VISIT_CHILDREN(ProxyRevoke);

} // namespace JSC
