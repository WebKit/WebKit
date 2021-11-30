/*
 * Copyright (C) 2021 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ShadowRealmScriptController.h"

#include "JSShadowRealmGlobalScope.h"

#include <JavaScriptCore/JSModuleRecord.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/Structure.h>
#include <JavaScriptCore/JSProxy.h>

namespace WebCore {

using JSC::VM;
using JSC::Structure;
using JSC::JSProxy;

ShadowRealmScriptController::ShadowRealmScriptController(Ref<JSC::VM>&& vm,
                                                         ShadowRealmGlobalScope* scope)
    : m_vm(WTFMove(vm))
    , m_scope(scope) {}

JSC::JSValue ShadowRealmScriptController::evaluateModule(JSC::JSModuleRecord& moduleRecord, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    return moduleRecord.evaluate(globalScopeWrapper(), awaitedValue, resumeMode);
}

void ShadowRealmScriptController::initScope() {
    ASSERT(!m_scopeWrapper);

    auto& vm = m_vm;

    Structure* contextProtoStructure = JSShadowRealmGlobalScopePrototype::createStructure(vm, nullptr, JSC::jsNull());
    auto contextProto = JSShadowRealmGlobalScopePrototype::create(vm, nullptr, contextProtoStructure);
    Structure* structure = JSShadowRealmGlobalScope::createStructure(vm, nullptr, contextProto);

    auto proxyStructure = JSProxy::createStructure(vm, nullptr, JSC::jsNull());
    auto proxy = JSProxy::create(vm, proxyStructure);

    auto wrapper = JSShadowRealmGlobalScope::create(vm, structure, Ref(*m_scope), proxy);
    m_scopeWrapper = JSC::Strong<JSShadowRealmGlobalScope>(vm, wrapper);
}


}

