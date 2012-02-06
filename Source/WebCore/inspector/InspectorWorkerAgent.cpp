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

#include "config.h"

#if ENABLE(WORKERS) && ENABLE(INSPECTOR)

#include "InspectorWorkerAgent.h"

#include "InspectorFrontend.h"
#include "InspectorFrontendChannel.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "KURL.h"
#include "WorkerContextProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

namespace WorkerAgentState {
static const char workerInspectionEnabled[] = "workerInspectionEnabled";
static const char autoconnectToWorkers[] = "autoconnectToWorkers";
};

class InspectorWorkerAgent::WorkerFrontendChannel : public WorkerContextProxy::PageInspector {
public:
    explicit WorkerFrontendChannel(InspectorFrontend* frontend, WorkerContextProxy* proxy)
        : m_frontend(frontend)
        , m_proxy(proxy)
        , m_id(s_nextId++)
        , m_connected(false)
    {
    }
    virtual ~WorkerFrontendChannel()
    {
    }

    int id() const { return m_id; }
    WorkerContextProxy* proxy() const { return m_proxy; }

    void connectToWorkerContext()
    {
        if (m_connected)
            return;
        m_connected = true;
        m_proxy->connectToInspector(this);
    }

    void disconnectFromWorkerContext()
    {
        if (!m_connected)
            return;
        m_connected = false;
        m_proxy->disconnectFromInspector();
    }

private:
    // WorkerContextProxy::PageInspector implementation
    virtual void dispatchMessageFromWorker(const String& message)
    {
        RefPtr<InspectorValue> value = InspectorValue::parseJSON(message);
        if (!value)
            return;
        RefPtr<InspectorObject> messageObject = value->asObject();
        if (!messageObject)
            return;
        m_frontend->worker()->dispatchMessageFromWorker(m_id, messageObject);
    }

    InspectorFrontend* m_frontend;
    WorkerContextProxy* m_proxy;
    int m_id;
    bool m_connected;
    static int s_nextId;
};

int InspectorWorkerAgent::WorkerFrontendChannel::s_nextId = 1;

PassOwnPtr<InspectorWorkerAgent> InspectorWorkerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorState* inspectorState)
{
    return adoptPtr(new InspectorWorkerAgent(instrumentingAgents, inspectorState));
}

InspectorWorkerAgent::InspectorWorkerAgent(InstrumentingAgents* instrumentingAgents, InspectorState* inspectorState)
    : InspectorBaseAgent<InspectorWorkerAgent>("Worker", instrumentingAgents, inspectorState)
    , m_inspectorFrontend(0)
{
    m_instrumentingAgents->setInspectorWorkerAgent(this);
}

InspectorWorkerAgent::~InspectorWorkerAgent()
{
    m_instrumentingAgents->setInspectorWorkerAgent(0);
}

void InspectorWorkerAgent::setFrontend(InspectorFrontend* frontend)
{
    m_inspectorFrontend = frontend;
}

void InspectorWorkerAgent::restore()
{
    if (m_state->getBoolean(WorkerAgentState::workerInspectionEnabled))
        createWorkerFrontendChannelsForExistingWorkers();
}

void InspectorWorkerAgent::clearFrontend()
{
    m_inspectorFrontend = 0;
    m_state->setBoolean(WorkerAgentState::autoconnectToWorkers, false);
    destroyWorkerFrontendChannels();
}

void InspectorWorkerAgent::setWorkerInspectionEnabled(ErrorString*, bool value)
{
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, value);
    if (!m_inspectorFrontend)
        return;
    if (value)
        createWorkerFrontendChannelsForExistingWorkers();
    else
        destroyWorkerFrontendChannels();
}

void InspectorWorkerAgent::connectToWorker(ErrorString* error, int workerId)
{
    WorkerFrontendChannel* channel = m_idToChannel.get(workerId);
    if (channel)
        channel->connectToWorkerContext();
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::disconnectFromWorker(ErrorString* error, int workerId)
{
    WorkerFrontendChannel* channel = m_idToChannel.get(workerId);
    if (channel)
        channel->disconnectFromWorkerContext();
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::sendMessageToWorker(ErrorString* error, int workerId, const RefPtr<InspectorObject>& message)
{
    WorkerFrontendChannel* channel = m_idToChannel.get(workerId);
    if (channel)
        channel->proxy()->sendMessageToInspector(message->toJSONString());
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::setAutoconnectToWorkers(ErrorString*, bool value)
{
    m_state->setBoolean(WorkerAgentState::autoconnectToWorkers, value);
}

bool InspectorWorkerAgent::shouldPauseDedicatedWorkerOnStart()
{
    return m_state->getBoolean(WorkerAgentState::autoconnectToWorkers);
}

void InspectorWorkerAgent::didStartWorkerContext(WorkerContextProxy* workerContextProxy, const KURL& url)
{
    m_dedicatedWorkers.set(workerContextProxy, url.string());
    if (m_inspectorFrontend && m_state->getBoolean(WorkerAgentState::workerInspectionEnabled))
        createWorkerFrontendChannel(workerContextProxy, url.string());
}

void InspectorWorkerAgent::workerContextTerminated(WorkerContextProxy* proxy)
{
    m_dedicatedWorkers.remove(proxy);
    for (WorkerChannels::iterator it = m_idToChannel.begin(); it != m_idToChannel.end(); ++it) {
        if (proxy == it->second->proxy()) {
            m_inspectorFrontend->worker()->workerTerminated(it->first);
            delete it->second;
            m_idToChannel.remove(it);
            return;
        }
    }
}

void InspectorWorkerAgent::createWorkerFrontendChannelsForExistingWorkers()
{
    for (DedicatedWorkers::iterator it = m_dedicatedWorkers.begin(); it != m_dedicatedWorkers.end(); ++it)
        createWorkerFrontendChannel(it->first, it->second);
}

void InspectorWorkerAgent::destroyWorkerFrontendChannels()
{
    for (WorkerChannels::iterator it = m_idToChannel.begin(); it != m_idToChannel.end(); ++it) {
        it->second->disconnectFromWorkerContext();
        delete it->second;
    }
    m_idToChannel.clear();
}

void InspectorWorkerAgent::createWorkerFrontendChannel(WorkerContextProxy* workerContextProxy, const String& url)
{
    WorkerFrontendChannel* channel = new WorkerFrontendChannel(m_inspectorFrontend, workerContextProxy);
    m_idToChannel.set(channel->id(), channel);

    ASSERT(m_inspectorFrontend);
    bool autoconnectToWorkers = m_state->getBoolean(WorkerAgentState::autoconnectToWorkers);
    if (autoconnectToWorkers)
        channel->connectToWorkerContext();
    m_inspectorFrontend->worker()->workerCreated(channel->id(), url, autoconnectToWorkers);
}

} // namespace WebCore

#endif // ENABLE(WORKERS) && ENABLE(INSPECTOR)
