/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
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

#ifndef Worker_h
#define Worker_h

#include "AbstractWorker.h"
#include "ActiveDOMObject.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "MessagePort.h"
#include "WorkerScriptLoaderClient.h"
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

    class ScriptExecutionContext;
    class WorkerGlobalScopeProxy;
    class WorkerScriptLoader;

    typedef int ExceptionCode;

    class Worker final : public AbstractWorker, public ActiveDOMObject, private WorkerScriptLoaderClient {
    public:
        static RefPtr<Worker> create(ScriptExecutionContext&, const String& url, ExceptionCode&);
        virtual ~Worker();

        EventTargetInterface eventTargetInterface() const override { return WorkerEventTargetInterfaceType; }

        void postMessage(RefPtr<SerializedScriptValue>&& message, const MessagePortArray*, ExceptionCode&);

        void terminate();

        // EventTarget API.
        ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

        // ActiveDOMObject API.
        bool hasPendingActivity() const override;

    private:
        explicit Worker(ScriptExecutionContext&);

        void notifyNetworkStateChange(bool isOnline);

        // WorkerScriptLoaderClient callbacks
        void didReceiveResponse(unsigned long identifier, const ResourceResponse&) override;
        void notifyFinished() override;

        // ActiveDOMObject API.
        bool canSuspendForDocumentSuspension() const override;
        void stop() override;
        const char* activeDOMObjectName() const override;

        friend void networkStateChanged(bool isOnLine);

        RefPtr<WorkerScriptLoader> m_scriptLoader;
        WorkerGlobalScopeProxy* m_contextProxy; // The proxy outlives the worker to perform thread shutdown.
        Optional<ContentSecurityPolicyResponseHeaders> m_contentSecurityPolicyResponseHeaders;
        bool m_shouldBypassMainWorldContentSecurityPolicy { false };
    };

} // namespace WebCore

#endif // Worker_h
