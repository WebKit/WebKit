/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "JSEventTarget.h"
#include "WorkletGlobalScope.h"

namespace WebCore {

class JSWorkletGlobalScopeBase : public JSDOMGlobalObject {
public:
    using Base = JSDOMGlobalObject;

    template<typename, JSC::SubspaceAccess>
    static void subspaceFor(JSC::VM&) { RELEASE_ASSERT_NOT_REACHED(); }

    static void destroy(JSC::JSCell*);

    DECLARE_INFO;

    WorkletGlobalScope& wrapped() const { return *m_wrapped; }
    ScriptExecutionContext* scriptExecutionContext() const;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::GlobalObjectType, StructureFlags), info());
    }

    static bool supportsRichSourceInfo(const JSC::JSGlobalObject*);
    static bool shouldInterruptScript(const JSC::JSGlobalObject*);
    static bool shouldInterruptScriptBeforeTimeout(const JSC::JSGlobalObject*);
    static JSC::RuntimeFlags javaScriptRuntimeFlags(const JSC::JSGlobalObject*);
    static JSC::ScriptExecutionStatus scriptExecutionStatus(JSC::JSGlobalObject*, JSC::JSObject*);
    static void queueMicrotaskToEventLoop(JSC::JSGlobalObject&, Ref<JSC::Microtask>&&);
    static void reportViolationForUnsafeEval(JSC::JSGlobalObject*, JSC::JSString*);

protected:
    JSWorkletGlobalScopeBase(JSC::VM&, JSC::Structure*, RefPtr<WorkletGlobalScope>&&);
    void finishCreation(JSC::VM&, JSC::JSGlobalProxy*);

    DECLARE_VISIT_CHILDREN;

private:
    static const JSC::GlobalObjectMethodTable* globalObjectMethodTable();

    RefPtr<WorkletGlobalScope> m_wrapped;
};

// Returns a JSWorkletGlobalScope or jsNull()
// Always ignores the execState and passed globalObject, WorkletGlobalScope is itself a globalObject and will always use its own prototype chain.
JSC::JSValue toJS(JSC::JSGlobalObject*, JSDOMGlobalObject*, WorkletGlobalScope&);
inline JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, WorkletGlobalScope* scope) { return scope ? toJS(lexicalGlobalObject, globalObject, *scope) : JSC::jsNull(); }
JSC::JSValue toJS(JSC::JSGlobalObject*, WorkletGlobalScope&);
inline JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, WorkletGlobalScope* scope) { return scope ? toJS(lexicalGlobalObject, *scope) : JSC::jsNull(); }

} // namespace WebCore
