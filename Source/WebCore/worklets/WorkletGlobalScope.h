/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_PAINTING_API)

#include "EventTarget.h"
#include "ExceptionOr.h"
#include "ScriptExecutionContext.h"
#include "ScriptSourceCode.h"
#include "URL.h"
#include "WorkerEventQueue.h"

#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/RuntimeFlags.h>
#include <pal/SessionID.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class WorkletScriptController;
class ScriptSourceCode;

enum WorkletGlobalScopeIdentifierType { };
using WorkletGlobalScopeIdentifier = ObjectIdentifier<WorkletGlobalScopeIdentifierType>;

class WorkletGlobalScope : public RefCounted<WorkletGlobalScope>, public ScriptExecutionContext, public EventTargetWithInlineData {
public:
    virtual ~WorkletGlobalScope();

    using WorkletGlobalScopesSet = HashSet<const WorkletGlobalScope*>;
    WEBCORE_EXPORT static WorkletGlobalScopesSet& allWorkletGlobalScopesSet();

    virtual bool isPaintWorkletGlobalScope() const { return false; }

    const URL& url() const final { return m_code.url(); }
    String origin() const final;

    void evaluate();

    using RefCounted::ref;
    using RefCounted::deref;

    WorkletScriptController* script() { return m_script.get(); }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;

    bool isJSExecutionForbidden() const final;
    SecurityOrigin& topOrigin() const final { return m_topOrigin.get(); }

    SocketProvider* socketProvider() final { return nullptr; }

    bool isContextThread() const final { return true; }
    bool isSecureContext() const final { return false; }

    JSC::RuntimeFlags jsRuntimeFlags() const { return m_jsRuntimeFlags; }

    virtual void prepareForDestruction();

protected:
    WorkletGlobalScope(Document&, ScriptSourceCode&&);
    WorkletGlobalScope(const WorkletGlobalScope&) = delete;
    WorkletGlobalScope(WorkletGlobalScope&&) = delete;

    Document* responsibleDocument() { return m_document.get(); }
    const Document* responsibleDocument() const { return m_document.get(); }

private:
#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionProxy* idbConnectionProxy() final { ASSERT_NOT_REACHED(); return nullptr; }
#endif

    void postTask(Task&&) final { ASSERT_NOT_REACHED(); }

    void refScriptExecutionContext() final { ref(); }
    void derefScriptExecutionContext() final { deref(); }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    ScriptExecutionContext* scriptExecutionContext() const final { return const_cast<WorkletGlobalScope*>(this); }
    EventTargetInterface eventTargetInterface() const final { return WorkletGlobalScopeEventTargetInterfaceType; }

    bool isWorkletGlobalScope() const final { return true; }

    void logExceptionToConsole(const String& errorMessage, const String&, int, int, RefPtr<Inspector::ScriptCallStack>&&) final;
    void addMessage(MessageSource, MessageLevel, const String&, const String&, unsigned, unsigned, RefPtr<Inspector::ScriptCallStack>&&, JSC::ExecState*, unsigned long) final;
    void addConsoleMessage(MessageSource, MessageLevel, const String&, unsigned long) final;

    EventTarget* errorEventTarget() final { return this; }
    EventQueue& eventQueue() const final { ASSERT_NOT_REACHED(); return m_eventQueue; }

#if ENABLE(SUBTLE_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final { RELEASE_ASSERT_NOT_REACHED(); return false; }
    bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final { RELEASE_ASSERT_NOT_REACHED(); return false; }
#endif
    URL completeURL(const String&) const final;
    PAL::SessionID sessionID() const final { return m_sessionID; }
    String userAgent(const URL&) const final;
    void disableEval(const String&) final;
    void disableWebAssembly(const String&) final;

    WeakPtr<Document> m_document;

    PAL::SessionID m_sessionID;
    std::unique_ptr<WorkletScriptController> m_script;

    Ref<SecurityOrigin> m_topOrigin;

    // FIXME: This is not implemented properly, it just satisfies the compiler.
    // https://bugs.webkit.org/show_bug.cgi?id=191136
    mutable WorkerEventQueue m_eventQueue;

    JSC::RuntimeFlags m_jsRuntimeFlags;
    ScriptSourceCode m_code;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkletGlobalScope)
static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isWorkletGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
#endif
