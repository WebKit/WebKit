/*
 * Copyright (C) 2008, 2010, 2016 Apple Inc. All Rights Reserved.
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

#pragma once

#include "AbstractWorker.h"
#include "ActiveDOMObject.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "EventTarget.h"
#include "MessagePort.h"
#include "WorkerScriptLoaderClient.h"
#include <JavaScriptCore/RuntimeFlags.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>
#include <wtf/text/AtomicStringHash.h>

namespace JSC {
class ExecState;
class JSObject;
class JSValue;
}

namespace WebCore {

class ScriptExecutionContext;
class WorkerGlobalScopeProxy;
class WorkerScriptLoader;

class Worker final : public AbstractWorker, public ActiveDOMObject, private WorkerScriptLoaderClient {
public:
    struct Options {
        String name;
    };
    static ExceptionOr<Ref<Worker>> create(ScriptExecutionContext&, JSC::RuntimeFlags, const String& url, const Options&);
    virtual ~Worker();

    ExceptionOr<void> postMessage(JSC::ExecState&, JSC::JSValue message, Vector<JSC::Strong<JSC::JSObject>>&&);

    void terminate();

    bool hasPendingActivity() const final;

    String identifier() const { return m_identifier; }

    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

private:
    explicit Worker(ScriptExecutionContext&, JSC::RuntimeFlags, const Options&);

    EventTargetInterface eventTargetInterface() const final { return WorkerEventTargetInterfaceType; }

    void notifyNetworkStateChange(bool isOnline);

    void didReceiveResponse(unsigned long identifier, const ResourceResponse&) final;
    void notifyFinished() final;

    bool canSuspendForDocumentSuspension() const final;
    void stop() final;
    const char* activeDOMObjectName() const final;

    static void networkStateChanged(bool isOnLine);

    RefPtr<WorkerScriptLoader> m_scriptLoader;
    String m_name;
    String m_identifier;
    WorkerGlobalScopeProxy& m_contextProxy; // The proxy outlives the worker to perform thread shutdown.
    Optional<ContentSecurityPolicyResponseHeaders> m_contentSecurityPolicyResponseHeaders;
    MonotonicTime m_workerCreationTime;
    bool m_shouldBypassMainWorldContentSecurityPolicy { false };
    JSC::RuntimeFlags m_runtimeFlags;
};

} // namespace WebCore
