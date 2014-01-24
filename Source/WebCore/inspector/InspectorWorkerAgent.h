/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorWorkerAgent_h
#define InspectorWorkerAgent_h

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace Inspector {
class InspectorObject;
class InspectorWorkerFrontendDispatcher;
}

namespace WebCore {
class InstrumentingAgents;
class URL;
class WorkerGlobalScopeProxy;

typedef String ErrorString;

class InspectorWorkerAgent : public InspectorAgentBase, public Inspector::InspectorWorkerBackendDispatcherHandler {
public:
    explicit InspectorWorkerAgent(InstrumentingAgents*);
    ~InspectorWorkerAgent();

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;

    // Called from InspectorInstrumentation
    bool shouldPauseDedicatedWorkerOnStart() const;
    void didStartWorkerGlobalScope(WorkerGlobalScopeProxy*, const URL&);
    void workerGlobalScopeTerminated(WorkerGlobalScopeProxy*);

    // Called from InspectorBackendDispatcher
    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void canInspectWorkers(ErrorString*, bool*) override;
    virtual void connectToWorker(ErrorString*, int workerId) override;
    virtual void disconnectFromWorker(ErrorString*, int workerId) override;
    virtual void sendMessageToWorker(ErrorString*, int workerId, const RefPtr<Inspector::InspectorObject>& message) override;
    virtual void setAutoconnectToWorkers(ErrorString*, bool value) override;

private:
    void createWorkerFrontendChannelsForExistingWorkers();
    void createWorkerFrontendChannel(WorkerGlobalScopeProxy*, const String& url);
    void destroyWorkerFrontendChannels();

    std::unique_ptr<Inspector::InspectorWorkerFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorWorkerBackendDispatcher> m_backendDispatcher;
    bool m_enabled;
    bool m_shouldPauseDedicatedWorkerOnStart;

    class WorkerFrontendChannel;
    typedef HashMap<int, WorkerFrontendChannel*> WorkerChannels;
    WorkerChannels m_idToChannel;
    typedef HashMap<WorkerGlobalScopeProxy*, String> DedicatedWorkers;
    DedicatedWorkers m_dedicatedWorkers;
};

} // namespace WebCore

#endif // !defined(InspectorWorkerAgent_h)
