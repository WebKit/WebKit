/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "JSDOMGlobalObject.h"
#include "JSDOMWrapper.h"

#if ENABLE(SERVICE_WORKER)
#include "ServiceWorkerGlobalScope.h"
#endif

namespace JSC {
class JSProxy;
}

namespace WebCore {

class JSDedicatedWorkerGlobalScope;
class JSWorkerGlobalScope;
class WorkerGlobalScope;

#if ENABLE(SERVICE_WORKER)
class JSServiceWorkerGlobalScope;
#endif

class JSWorkerGlobalScopeBase : public JSDOMGlobalObject {
public:
    using Base = JSDOMGlobalObject;

    template<typename, JSC::SubspaceAccess>
    static void subspaceFor(JSC::VM&) { RELEASE_ASSERT_NOT_REACHED(); }

    static void destroy(JSC::JSCell*);

    DECLARE_INFO;

    WorkerGlobalScope& wrapped() const { return *m_wrapped; }
    JSC::JSProxy* proxy() const { ASSERT(m_proxy); return m_proxy.get(); }
    ScriptExecutionContext* scriptExecutionContext() const;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::GlobalObjectType, StructureFlags), info());
    }

    static const JSC::GlobalObjectMethodTable s_globalObjectMethodTable;

    static bool supportsRichSourceInfo(const JSC::JSGlobalObject*);
    static bool shouldInterruptScript(const JSC::JSGlobalObject*);
    static bool shouldInterruptScriptBeforeTimeout(const JSC::JSGlobalObject*);
    static JSC::RuntimeFlags javaScriptRuntimeFlags(const JSC::JSGlobalObject*);
    static void queueMicrotaskToEventLoop(JSC::JSGlobalObject&, Ref<JSC::Microtask>&&);

    void clearDOMGuardedObjects();

protected:
    JSWorkerGlobalScopeBase(JSC::VM&, JSC::Structure*, RefPtr<WorkerGlobalScope>&&);
    void finishCreation(JSC::VM&, JSC::JSProxy*);

    static void visitChildren(JSC::JSCell*, JSC::SlotVisitor&);

private:
    RefPtr<WorkerGlobalScope> m_wrapped;
    JSC::WriteBarrier<JSC::JSProxy> m_proxy;
};

// Returns a JSWorkerGlobalScope or jsNull()
// Always ignores the execState and passed globalObject, WorkerGlobalScope is itself a globalObject and will always use its own prototype chain.
JSC::JSValue toJS(JSC::JSGlobalObject*, JSDOMGlobalObject*, WorkerGlobalScope&);
inline JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, WorkerGlobalScope* scope) { return scope ? toJS(lexicalGlobalObject, globalObject, *scope) : JSC::jsNull(); }
JSC::JSValue toJS(JSC::JSGlobalObject*, WorkerGlobalScope&);
inline JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, WorkerGlobalScope* scope) { return scope ? toJS(lexicalGlobalObject, *scope) : JSC::jsNull(); }

JSDedicatedWorkerGlobalScope* toJSDedicatedWorkerGlobalScope(JSC::VM&, JSC::JSValue);
JSWorkerGlobalScope* toJSWorkerGlobalScope(JSC::VM&, JSC::JSValue);

#if ENABLE(SERVICE_WORKER)
JSServiceWorkerGlobalScope* toJSServiceWorkerGlobalScope(JSC::VM&, JSC::JSValue);
#endif
} // namespace WebCore
