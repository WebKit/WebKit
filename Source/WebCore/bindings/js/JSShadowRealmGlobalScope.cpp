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

#include "config.h"
#include "JSShadowRealmGlobalScope.h"
#include <JavaScriptCore/HeapAnalyzer.h>
#include <wtf/Language.h>

using namespace JSC;

namespace WebCore {


const ClassInfo JSShadowRealmGlobalScope::s_info = { "ShadowRealmGlobalScope", &JSDOMGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSShadowRealmGlobalScope) };

const GlobalObjectMethodTable JSShadowRealmGlobalScope::s_globalObjectMethodTable = {
    &JSGlobalObject::supportsRichSourceInfo,
    &JSGlobalObject::shouldInterruptScript,
    &JSGlobalObject::javaScriptRuntimeFlags,
    nullptr,
    &JSGlobalObject::shouldInterruptScriptBeforeTimeout,
    &moduleLoaderImportModule,
    &moduleLoaderResolve,
    &moduleLoaderFetch,
    &moduleLoaderCreateImportMetaProperties,
    &moduleLoaderEvaluate,
    &promiseRejectionTracker,
    &reportUncaughtExceptionAtEventLoop,
    &JSGlobalObject::currentScriptExecutionOwner,
    &JSGlobalObject::scriptExecutionStatus,
    &JSGlobalObject::reportViolationForUnsafeEval,
    [] { return defaultLanguage(); },
    nullptr,
    nullptr,
    &deriveShadowRealmGlobalObject
};

JSShadowRealmGlobalScope::JSShadowRealmGlobalScope(VM& vm, Structure* structure, JSDOMGlobalObject* parentGlobal)
    : JSDOMGlobalObject(vm, structure, parentGlobal->world(), &s_globalObjectMethodTable)
{
    m_parent = parentGlobal;
}

void JSShadowRealmGlobalScope::destroy(JSCell* cell)
{
    static_cast<JSShadowRealmGlobalScope*>(cell)->JSShadowRealmGlobalScope::~JSShadowRealmGlobalScope();
}


JSObject* JSShadowRealmGlobalScope::createPrototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSShadowRealmGlobalScopePrototype::create(vm, &globalObject, JSShadowRealmGlobalScopePrototype::createStructure(vm, &globalObject, JSEventTarget::prototype(vm, globalObject)));
}

JSObject* JSShadowRealmGlobalScope::prototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return getDOMPrototype<JSShadowRealmGlobalScope>(vm, globalObject);
}

template<typename Visitor>
void JSShadowRealmGlobalScope::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSShadowRealmGlobalScope*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    /* thisObject->visitAdditionalChildren(visitor); */
}

DEFINE_VISIT_CHILDREN(JSShadowRealmGlobalScope);

template<typename Visitor>
void JSShadowRealmGlobalScope::visitOutputConstraints(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSShadowRealmGlobalScope*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitOutputConstraints(thisObject, visitor);
    /* thisObject->visitAdditionalChildren(visitor); */
}

template void JSShadowRealmGlobalScope::visitOutputConstraints(JSCell*, AbstractSlotVisitor&);
template void JSShadowRealmGlobalScope::visitOutputConstraints(JSCell*, SlotVisitor&);
void JSShadowRealmGlobalScope::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSShadowRealmGlobalScope*>(cell);
    analyzer.setWrappedObjectForCell(cell, thisObject->parent());
    if (thisObject->scriptExecutionContext())
        analyzer.setLabelForCell(cell, "url " + thisObject->parent()->scriptExecutionContext()->url().string());
    Base::analyzeHeap(cell, analyzer);
}

const ClassInfo JSShadowRealmGlobalScopePrototype::s_info = { "ShadowRealmGlobalScope", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSShadowRealmGlobalScopePrototype) };

} // namespace WebCore
