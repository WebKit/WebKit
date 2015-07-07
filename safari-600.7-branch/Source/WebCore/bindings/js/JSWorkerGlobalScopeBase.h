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

#ifndef JSWorkerGlobalScopeBase_h
#define JSWorkerGlobalScopeBase_h

#include "JSDOMGlobalObject.h"

namespace WebCore {

    class JSDedicatedWorkerGlobalScope;
    class JSSharedWorkerGlobalScope;
    class JSWorkerGlobalScope;
    class WorkerGlobalScope;

    class JSWorkerGlobalScopeBase : public JSDOMGlobalObject {
        typedef JSDOMGlobalObject Base;
    public:
        static void destroy(JSC::JSCell*);

        DECLARE_INFO;

        WorkerGlobalScope& impl() const { return *m_impl; }
        ScriptExecutionContext* scriptExecutionContext() const;

        static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
        {
            return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::GlobalObjectType, StructureFlags), info());
        }

        static const JSC::GlobalObjectMethodTable s_globalObjectMethodTable;

        static bool allowsAccessFrom(const JSC::JSGlobalObject*, JSC::ExecState*);
        static bool supportsProfiling(const JSC::JSGlobalObject*);
        static bool supportsRichSourceInfo(const JSC::JSGlobalObject*);
        static bool shouldInterruptScript(const JSC::JSGlobalObject*);
        static bool shouldInterruptScriptBeforeTimeout(const JSC::JSGlobalObject*);
        static bool javaScriptExperimentsEnabled(const JSC::JSGlobalObject*);
        static void queueTaskToEventLoop(const JSC::JSGlobalObject*, PassRefPtr<JSC::Microtask>);

    protected:
        JSWorkerGlobalScopeBase(JSC::VM&, JSC::Structure*, PassRefPtr<WorkerGlobalScope>);
        void finishCreation(JSC::VM&);

    private:
        RefPtr<WorkerGlobalScope> m_impl;
    };

    // Returns a JSWorkerGlobalScope or jsNull()
    // Always ignores the execState and passed globalObject, WorkerGlobalScope is itself a globalObject and will always use its own prototype chain.
    JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, WorkerGlobalScope*);
    JSC::JSValue toJS(JSC::ExecState*, WorkerGlobalScope*);

    JSDedicatedWorkerGlobalScope* toJSDedicatedWorkerGlobalScope(JSC::JSValue);
    JSWorkerGlobalScope* toJSWorkerGlobalScope(JSC::JSValue);

#if ENABLE(SHARED_WORKERS)
    JSSharedWorkerGlobalScope* toJSSharedWorkerGlobalScope(JSC::JSValue);
#endif

} // namespace WebCore

#endif // JSWorkerGlobalScopeBase_h
