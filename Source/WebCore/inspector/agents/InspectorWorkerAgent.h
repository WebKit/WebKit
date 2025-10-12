/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#pragma once

#include "InspectorWebAgentBase.h"
#include "WorkerInspectorProxy.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/FastMalloc.h>
#include <wtf/Lock.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class InspectorWorkerAgent : public InspectorAgentBase, public Inspector::WorkerBackendDispatcherHandler, public CanMakeThreadSafeCheckedPtr<InspectorWorkerAgent> {
    WTF_MAKE_NONCOPYABLE(InspectorWorkerAgent);
    WTF_MAKE_TZONE_ALLOCATED(InspectorWorkerAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorWorkerAgent);

public:
    ~InspectorWorkerAgent();

    Inspector::WorkerFrontendDispatcher& frontendDispatcher() { return m_frontendDispatcher; }

    // InspectorAgentBase
    void didCreateFrontendAndBackend();
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // WorkerBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<void> initialized(const String& workerId);
    Inspector::Protocol::ErrorStringOr<void> sendMessageToWorker(const String& workerId, const String& message);

    // InspectorInstrumentation
    bool shouldWaitForDebuggerOnStart() const;
    void workerStarted(WorkerInspectorProxy&);
    void workerTerminated(WorkerInspectorProxy&);

protected:
    InspectorWorkerAgent(WebAgentContext&);

    virtual void connectToAllWorkerInspectorProxies() = 0;

    void connectToWorkerInspectorProxy(WorkerInspectorProxy&);

private:
    class PageChannel final : public WorkerInspectorProxy::PageChannel, public ThreadSafeRefCounted<PageChannel> {
        WTF_MAKE_TZONE_ALLOCATED_INLINE(PageChannel);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageChannel);

    public:
        static Ref<PageChannel> create(InspectorWorkerAgent&);

        void ref() const final { ThreadSafeRefCounted::ref(); }
        void deref() const final { ThreadSafeRefCounted::deref(); }

        void detachFromParentAgent();
        void sendMessageFromWorkerToFrontend(WorkerInspectorProxy&, String&&);

    private:
        explicit PageChannel(InspectorWorkerAgent&);

        Lock m_parentAgentLock;
        CheckedPtr<InspectorWorkerAgent> m_parentAgent WTF_GUARDED_BY_LOCK(m_parentAgentLock);
    };

    void disconnectFromAllWorkerInspectorProxies();
    void disconnectFromWorkerInspectorProxy(WorkerInspectorProxy&);

    const Ref<PageChannel> m_pageChannel;

    const UniqueRef<Inspector::WorkerFrontendDispatcher> m_frontendDispatcher;
    const Ref<Inspector::WorkerBackendDispatcher> m_backendDispatcher;

    MemoryCompactRobinHoodHashMap<String, WeakPtr<WorkerInspectorProxy>> m_connectedProxies;
    bool m_enabled { false };
};

} // namespace WebCore
