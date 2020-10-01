/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "FetchRequestCredentials.h"
#include "ScriptExecutionContext.h"
#include "ScriptSourceCode.h"
#include "WorkerEventLoop.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkletScriptController.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/RuntimeFlags.h>
#include <wtf/CompletionHandler.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class EventLoopTaskGroup;
class WorkerEventLoop;

struct WorkletParameters;

enum WorkletGlobalScopeIdentifierType { };
using WorkletGlobalScopeIdentifier = ObjectIdentifier<WorkletGlobalScopeIdentifierType>;

class WorkletGlobalScope : public RefCounted<WorkletGlobalScope>, public EventTargetWithInlineData, public WorkerOrWorkletGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(WorkletGlobalScope);
public:
    virtual ~WorkletGlobalScope();

    using WorkletGlobalScopesSet = HashSet<const WorkletGlobalScope*>;
    WEBCORE_EXPORT static WorkletGlobalScopesSet& allWorkletGlobalScopesSet();

#if ENABLE(CSS_PAINTING_API)
    virtual bool isPaintWorkletGlobalScope() const { return false; }
#endif
#if ENABLE(WEB_AUDIO)
    virtual bool isAudioWorkletGlobalScope() const { return false; }
#endif

    EventLoopTaskGroup& eventLoop() final;

    const URL& url() const final { return m_url; }

    void evaluate();

    ReferrerPolicy referrerPolicy() const final;

    using RefCounted::ref;
    using RefCounted::deref;

    WorkletScriptController* script() final { return m_script.get(); }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;

    bool isJSExecutionForbidden() const final;
    SecurityOrigin& topOrigin() const final { return m_topOrigin.get(); }

    SocketProvider* socketProvider() final { return nullptr; }

    // WorkerOrWorkletGlobalScope.
    Thread* underlyingThread() const override { return nullptr; }
    bool isClosing() const final { return m_isClosing; }

    bool isContextThread() const final { return true; }
    bool isSecureContext() const final { return false; }

    JSC::RuntimeFlags jsRuntimeFlags() const { return m_jsRuntimeFlags; }

    virtual void prepareForDestruction();

    void fetchAndInvokeScript(const URL&, FetchRequestCredentials, CompletionHandler<void(Optional<Exception>&&)>&&);

protected:
    WorkletGlobalScope(const WorkletParameters&);
    WorkletGlobalScope(Document&, Ref<JSC::VM>&&, ScriptSourceCode&&);
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
    void addMessage(MessageSource, MessageLevel, const String&, const String&, unsigned, unsigned, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject*, unsigned long) final;
    void addConsoleMessage(MessageSource, MessageLevel, const String&, unsigned long) final;

    EventTarget* errorEventTarget() final { return this; }

#if ENABLE(WEB_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final { RELEASE_ASSERT_NOT_REACHED(); return false; }
    bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final { RELEASE_ASSERT_NOT_REACHED(); return false; }
#endif
    URL completeURL(const String&, ForceUTF8 = ForceUTF8::No) const final;
    String userAgent(const URL&) const final;
    void disableEval(const String&) final;
    void disableWebAssembly(const String&) final;

    WeakPtr<Document> m_document;

    std::unique_ptr<WorkletScriptController> m_script;

    Ref<SecurityOrigin> m_topOrigin;

    RefPtr<WorkerEventLoop> m_eventLoop;
    std::unique_ptr<EventLoopTaskGroup> m_defaultTaskGroup;

    URL m_url;
    JSC::RuntimeFlags m_jsRuntimeFlags;
    Optional<ScriptSourceCode> m_code;
    bool m_isClosing { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkletGlobalScope)
static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isWorkletGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
