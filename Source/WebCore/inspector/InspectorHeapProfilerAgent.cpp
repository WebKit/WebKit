/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "InspectorHeapProfilerAgent.h"

#if ENABLE(INSPECTOR)

#include "CommandLineAPIHost.h"
#include "InstrumentingAgents.h"
#include "ScriptProfiler.h"
#include "WebInjectedScriptManager.h"
#include <inspector/InjectedScript.h>

using namespace Inspector;

namespace WebCore {

static const char* const UserInitiatedProfileNameHeap = "org.webkit.profiles.user-initiated";

InspectorHeapProfilerAgent::InspectorHeapProfilerAgent(InstrumentingAgents* instrumentingAgents, WebInjectedScriptManager* injectedScriptManager)
    : InspectorAgentBase(ASCIILiteral("HeapProfiler"), instrumentingAgents)
    , m_injectedScriptManager(injectedScriptManager)
    , m_nextUserInitiatedHeapSnapshotNumber(1)
    , m_profileHeadersRequested(false)
{
    m_instrumentingAgents->setInspectorHeapProfilerAgent(this);
}

InspectorHeapProfilerAgent::~InspectorHeapProfilerAgent()
{
    m_instrumentingAgents->setInspectorHeapProfilerAgent(nullptr);
}

void InspectorHeapProfilerAgent::resetState()
{
    m_snapshots.clear();
    m_nextUserInitiatedHeapSnapshotNumber = 1;
    resetFrontendProfiles();

    if (CommandLineAPIHost* commandLineAPIHost = m_injectedScriptManager->commandLineAPIHost())
        commandLineAPIHost->clearInspectedObjects();
}

void InspectorHeapProfilerAgent::resetFrontendProfiles()
{
    if (!m_frontendDispatcher)
        return;
    if (!m_profileHeadersRequested)
        return;
    if (m_snapshots.isEmpty())
        m_frontendDispatcher->resetProfiles();
}

void InspectorHeapProfilerAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorHeapProfilerFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorHeapProfilerBackendDispatcher::create(backendDispatcher, this);
}

void InspectorHeapProfilerAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    m_profileHeadersRequested = false;
}

void InspectorHeapProfilerAgent::collectGarbage(WebCore::ErrorString*)
{
    ScriptProfiler::collectGarbage();
}

PassRefPtr<Inspector::TypeBuilder::HeapProfiler::ProfileHeader> InspectorHeapProfilerAgent::createSnapshotHeader(const ScriptHeapSnapshot& snapshot)
{
    RefPtr<Inspector::TypeBuilder::HeapProfiler::ProfileHeader> header = Inspector::TypeBuilder::HeapProfiler::ProfileHeader::create()
        .setUid(snapshot.uid())
        .setTitle(snapshot.title());
    return header.release();
}

void InspectorHeapProfilerAgent::hasHeapProfiler(ErrorString*, bool* result)
{
    *result = ScriptProfiler::hasHeapProfiler();
}

void InspectorHeapProfilerAgent::getProfileHeaders(ErrorString*, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::HeapProfiler::ProfileHeader>>& headers)
{
    m_profileHeadersRequested = true;
    headers = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::HeapProfiler::ProfileHeader>::create();

    IdToHeapSnapshotMap::iterator snapshotsEnd = m_snapshots.end();
    for (IdToHeapSnapshotMap::iterator it = m_snapshots.begin(); it != snapshotsEnd; ++it)
        headers->addItem(createSnapshotHeader(*it->value));
}

void InspectorHeapProfilerAgent::getHeapSnapshot(ErrorString* errorString, int rawUid)
{
    class OutputStream : public ScriptHeapSnapshot::OutputStream {
    public:
        OutputStream(InspectorHeapProfilerFrontendDispatcher* frontend, unsigned uid)
            : m_frontendDispatcher(frontend), m_uid(uid) { }
        void Write(const String& chunk) override { m_frontendDispatcher->addHeapSnapshotChunk(m_uid, chunk); }
        void Close() override { m_frontendDispatcher->finishHeapSnapshot(m_uid); }
    private:
        InspectorHeapProfilerFrontendDispatcher* m_frontendDispatcher;
        int m_uid;
    };

    unsigned uid = static_cast<unsigned>(rawUid);
    IdToHeapSnapshotMap::iterator it = m_snapshots.find(uid);
    if (it == m_snapshots.end()) {
        *errorString = "Profile wasn't found";
        return;
    }
    RefPtr<ScriptHeapSnapshot> snapshot = it->value;
    if (m_frontendDispatcher) {
        OutputStream stream(m_frontendDispatcher.get(), uid);
        snapshot->writeJSON(&stream);
    }
}

void InspectorHeapProfilerAgent::removeProfile(ErrorString*, int rawUid)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    if (m_snapshots.contains(uid))
        m_snapshots.remove(uid);
}

void InspectorHeapProfilerAgent::takeHeapSnapshot(ErrorString*, const bool* reportProgress)
{
    class HeapSnapshotProgress: public ScriptProfiler::HeapSnapshotProgress {
    public:
        explicit HeapSnapshotProgress(InspectorHeapProfilerFrontendDispatcher* frontend)
            : m_frontendDispatcher(frontend) { }
        void Start(int totalWork) override
        {
            m_totalWork = totalWork;
        }
        void Worked(int workDone) override
        {
            if (m_frontendDispatcher)
                m_frontendDispatcher->reportHeapSnapshotProgress(workDone, m_totalWork);
        }
        void Done() override { }
        bool isCanceled() { return false; }
    private:
        InspectorHeapProfilerFrontendDispatcher* m_frontendDispatcher;
        int m_totalWork;
    };

    String title = makeString(UserInitiatedProfileNameHeap, '.', String::number(m_nextUserInitiatedHeapSnapshotNumber));
    ++m_nextUserInitiatedHeapSnapshotNumber;

    HeapSnapshotProgress progress(reportProgress && *reportProgress ? m_frontendDispatcher.get() : nullptr);
    RefPtr<ScriptHeapSnapshot> snapshot = ScriptProfiler::takeHeapSnapshot(title, &progress);
    if (snapshot) {
        m_snapshots.add(snapshot->uid(), snapshot);
        if (m_frontendDispatcher)
            m_frontendDispatcher->addProfileHeader(createSnapshotHeader(*snapshot));
    }
}

void InspectorHeapProfilerAgent::getObjectByHeapObjectId(ErrorString* error, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>& result)
{
    bool ok;
    unsigned id = heapSnapshotObjectId.toUInt(&ok);
    if (!ok) {
        *error = "Invalid heap snapshot object id";
        return;
    }
    Deprecated::ScriptObject heapObject = ScriptProfiler::objectByHeapObjectId(id);
    if (heapObject.hasNoValue()) {
        *error = "Object is not available";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(heapObject.scriptState());
    if (injectedScript.hasNoValue()) {
        *error = "Object is not available. Inspected context is gone";
        return;
    }
    result = injectedScript.wrapObject(heapObject, objectGroup ? *objectGroup : "");
    if (!result)
        *error = "Failed to wrap object";
}

void InspectorHeapProfilerAgent::getHeapObjectId(ErrorString* errorString, const String& objectId, String* heapSnapshotObjectId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected context has gone";
        return;
    }
    Deprecated::ScriptValue value = injectedScript.findObjectById(objectId);
    if (value.hasNoValue() || value.isUndefined()) {
        *errorString = "Object with given id not found";
        return;
    }
    unsigned id = ScriptProfiler::getHeapObjectId(value);
    *heapSnapshotObjectId = String::number(id);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
