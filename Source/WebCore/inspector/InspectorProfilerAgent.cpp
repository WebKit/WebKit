/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR)

#include "InspectorProfilerAgent.h"

#include "CommandLineAPIHost.h"
#include "Console.h"
#include "ConsoleAPITypes.h"
#include "ConsoleTypes.h"
#include "InspectorConsoleAgent.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InstrumentingAgents.h"
#include "URL.h"
#include "Page.h"
#include "PageInjectedScriptManager.h"
#include "PageScriptDebugServer.h"
#include "ScriptHeapSnapshot.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "WorkerScriptDebugServer.h"
#include <bindings/ScriptObject.h>
#include <inspector/InjectedScript.h>
#include <inspector/InspectorValues.h>
#include <wtf/CurrentTime.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/StringConcatenate.h>

using namespace Inspector;

namespace WebCore {

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";
static const char* const CPUProfileType = "CPU";
static const char* const HeapProfileType = "HEAP";


class PageProfilerAgent : public InspectorProfilerAgent {
public:
    PageProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, Page* inspectedPage, PageInjectedScriptManager* injectedScriptManager)
        : InspectorProfilerAgent(instrumentingAgents, consoleAgent, injectedScriptManager), m_inspectedPage(inspectedPage) { }
    virtual ~PageProfilerAgent() { }

private:
    virtual void recompileScript() override
    {
        PageScriptDebugServer::shared().recompileAllJSFunctions();
    }

    virtual void startProfiling(const String& title) override
    {
        ScriptProfiler::startForPage(m_inspectedPage, title);
    }

    virtual PassRefPtr<ScriptProfile> stopProfiling(const String& title) override
    {
        return ScriptProfiler::stopForPage(m_inspectedPage, title);
    }

    Page* m_inspectedPage;
};

std::unique_ptr<InspectorProfilerAgent> InspectorProfilerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, Page* inspectedPage, PageInjectedScriptManager* injectedScriptManager)
{
    return std::make_unique<PageProfilerAgent>(instrumentingAgents, consoleAgent, inspectedPage, injectedScriptManager);
}

class WorkerProfilerAgent : public InspectorProfilerAgent {
public:
    WorkerProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, WorkerGlobalScope* workerGlobalScope, PageInjectedScriptManager* injectedScriptManager)
        : InspectorProfilerAgent(instrumentingAgents, consoleAgent, injectedScriptManager), m_workerGlobalScope(workerGlobalScope) { }
    virtual ~WorkerProfilerAgent() { }

private:
    virtual void recompileScript() override { }

    virtual void startProfiling(const String& title) override
    {
        ScriptProfiler::startForWorkerGlobalScope(m_workerGlobalScope, title);
    }

    virtual PassRefPtr<ScriptProfile> stopProfiling(const String& title) override
    {
        return ScriptProfiler::stopForWorkerGlobalScope(m_workerGlobalScope, title);
    }

    WorkerGlobalScope* m_workerGlobalScope;
};

std::unique_ptr<InspectorProfilerAgent> InspectorProfilerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, WorkerGlobalScope* workerGlobalScope, PageInjectedScriptManager* injectedScriptManager)
{
    return std::make_unique<WorkerProfilerAgent>(instrumentingAgents, consoleAgent, workerGlobalScope, injectedScriptManager);
}

InspectorProfilerAgent::InspectorProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, PageInjectedScriptManager* injectedScriptManager)
    : InspectorAgentBase(ASCIILiteral("Profiler"), instrumentingAgents)
    , m_consoleAgent(consoleAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_enabled(false)
    , m_profileHeadersRequested(false)
    , m_recordingCPUProfile(false)
    , m_currentUserInitiatedProfileNumber(-1)
    , m_nextUserInitiatedProfileNumber(1)
    , m_nextUserInitiatedHeapSnapshotNumber(1)
    , m_profileNameIdleTimeMap(ScriptProfiler::currentProfileNameIdleTimeMap())
{
    m_instrumentingAgents->setInspectorProfilerAgent(this);
}

InspectorProfilerAgent::~InspectorProfilerAgent()
{
    m_instrumentingAgents->setInspectorProfilerAgent(nullptr);
}

void InspectorProfilerAgent::addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, unsigned columnNumber, const String& sourceURL)
{
    RefPtr<ScriptProfile> profile = prpProfile;
    m_profiles.add(profile->uid(), profile);
    if (m_frontendDispatcher && m_profileHeadersRequested)
        m_frontendDispatcher->addProfileHeader(createProfileHeader(*profile));
    addProfileFinishedMessageToConsole(profile, lineNumber, columnNumber, sourceURL);
}

void InspectorProfilerAgent::addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, unsigned columnNumber, const String& sourceURL)
{
    if (!m_frontendDispatcher)
        return;
    RefPtr<ScriptProfile> profile = prpProfile;
    String message = makeString(profile->title(), '#', String::number(profile->uid()));
    m_consoleAgent->addMessageToConsole(ConsoleAPIMessageSource, ProfileEndMessageType, DebugMessageLevel, message, sourceURL, lineNumber, columnNumber);
}

void InspectorProfilerAgent::addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, unsigned columnNumber, const String& sourceURL)
{
    if (!m_frontendDispatcher)
        return;
    m_consoleAgent->addMessageToConsole(ConsoleAPIMessageSource, ProfileMessageType, DebugMessageLevel, title, sourceURL, lineNumber, columnNumber);
}

void InspectorProfilerAgent::collectGarbage(WebCore::ErrorString*)
{
    ScriptProfiler::collectGarbage();
}

PassRefPtr<Inspector::TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::createProfileHeader(const ScriptProfile& profile)
{
    return Inspector::TypeBuilder::Profiler::ProfileHeader::create()
        .setTypeId(Inspector::TypeBuilder::Profiler::ProfileHeader::TypeId::CPU)
        .setUid(profile.uid())
        .setTitle(profile.title())
        .release();
}

PassRefPtr<Inspector::TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::createSnapshotHeader(const ScriptHeapSnapshot& snapshot)
{
    RefPtr<Inspector::TypeBuilder::Profiler::ProfileHeader> header = Inspector::TypeBuilder::Profiler::ProfileHeader::create()
        .setTypeId(Inspector::TypeBuilder::Profiler::ProfileHeader::TypeId::HEAP)
        .setUid(snapshot.uid())
        .setTitle(snapshot.title());
    return header.release();
}

void InspectorProfilerAgent::isSampling(ErrorString*, bool* result)
{
    *result = ScriptProfiler::isSampling();
}

void InspectorProfilerAgent::hasHeapProfiler(ErrorString*, bool* result)
{
    *result = ScriptProfiler::hasHeapProfiler();
}

void InspectorProfilerAgent::enable(ErrorString*)
{
    enable(false);
}

void InspectorProfilerAgent::disable(ErrorString*)
{
    disable(false);
}

void InspectorProfilerAgent::disable(bool skipRecompile)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_profileHeadersRequested = false;
    if (!skipRecompile)
        recompileScript();
}

void InspectorProfilerAgent::enable(bool skipRecompile)
{
    if (m_enabled)
        return;
    m_enabled = true;
    if (!skipRecompile)
        recompileScript();
}

String InspectorProfilerAgent::getCurrentUserInitiatedProfileName(bool incrementProfileNumber)
{
    if (incrementProfileNumber)
        m_currentUserInitiatedProfileNumber = m_nextUserInitiatedProfileNumber++;

    return makeString(UserInitiatedProfileName, '.', String::number(m_currentUserInitiatedProfileNumber));
}

void InspectorProfilerAgent::getProfileHeaders(ErrorString*, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::ProfileHeader>>& headers)
{
    m_profileHeadersRequested = true;
    headers = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::ProfileHeader>::create();

    ProfilesMap::iterator profilesEnd = m_profiles.end();
    for (ProfilesMap::iterator it = m_profiles.begin(); it != profilesEnd; ++it)
        headers->addItem(createProfileHeader(*it->value));
    HeapSnapshotsMap::iterator snapshotsEnd = m_snapshots.end();
    for (HeapSnapshotsMap::iterator it = m_snapshots.begin(); it != snapshotsEnd; ++it)
        headers->addItem(createSnapshotHeader(*it->value));
}

namespace {

class OutputStream : public ScriptHeapSnapshot::OutputStream {
public:
    OutputStream(InspectorProfilerFrontendDispatcher* frontend, unsigned uid)
        : m_frontendDispatcher(frontend), m_uid(uid) { }
    void Write(const String& chunk) override { m_frontendDispatcher->addHeapSnapshotChunk(m_uid, chunk); }
    void Close() override { m_frontendDispatcher->finishHeapSnapshot(m_uid); }
private:
    InspectorProfilerFrontendDispatcher* m_frontendDispatcher;
    int m_uid;
};

} // namespace

void InspectorProfilerAgent::getCPUProfile(ErrorString* errorString, int rawUid, RefPtr<Inspector::TypeBuilder::Profiler::CPUProfile>& profileObject)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    ProfilesMap::iterator it = m_profiles.find(uid);
    if (it == m_profiles.end()) {
        *errorString = "Profile wasn't found";
        return;
    }

    profileObject = it->value->buildInspectorObject();
}

void InspectorProfilerAgent::getHeapSnapshot(ErrorString* errorString, int rawUid)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    HeapSnapshotsMap::iterator it = m_snapshots.find(uid);
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

void InspectorProfilerAgent::removeProfile(ErrorString*, const String& type, int rawUid)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    if (type == CPUProfileType)
        m_profiles.remove(uid);
    else if (type == HeapProfileType)
        m_snapshots.remove(uid);
}

void InspectorProfilerAgent::resetState()
{
    stop();
    m_profiles.clear();
    m_snapshots.clear();
    m_currentUserInitiatedProfileNumber = 1;
    m_nextUserInitiatedProfileNumber = 1;
    m_nextUserInitiatedHeapSnapshotNumber = 1;
    resetFrontendProfiles();

    if (CommandLineAPIHost* commandLineAPIHost = m_injectedScriptManager->commandLineAPIHost())
        commandLineAPIHost->clearInspectedObjects();
}

void InspectorProfilerAgent::resetFrontendProfiles()
{
    if (!m_frontendDispatcher)
        return;
    if (!m_profileHeadersRequested)
        return;
    if (m_profiles.isEmpty() && m_snapshots.isEmpty())
        m_frontendDispatcher->resetProfiles();
}

void InspectorProfilerAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorProfilerFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorProfilerBackendDispatcher::create(backendDispatcher, this);
}

void InspectorProfilerAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason reason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    stop();

    bool skipRecompile = reason == InspectorDisconnectReason::InspectedTargetDestroyed;
    disable(skipRecompile);
}

void InspectorProfilerAgent::start(ErrorString*)
{
    if (m_recordingCPUProfile)
        return;
    if (!enabled()) {
        enable(true);
        PageScriptDebugServer::shared().recompileAllJSFunctions();
    }
    m_recordingCPUProfile = true;
    String title = getCurrentUserInitiatedProfileName(true);
    startProfiling(title);
    addStartProfilingMessageToConsole(title, 0, 0, String());
    toggleRecordButton(true);
}

void InspectorProfilerAgent::stop(ErrorString*)
{
    if (!m_recordingCPUProfile)
        return;
    m_recordingCPUProfile = false;
    String title = getCurrentUserInitiatedProfileName();
    RefPtr<ScriptProfile> profile = stopProfiling(title);
    if (profile)
        addProfile(profile, 0, 0, String());
    toggleRecordButton(false);
}

namespace {

class HeapSnapshotProgress: public ScriptProfiler::HeapSnapshotProgress {
public:
    explicit HeapSnapshotProgress(InspectorProfilerFrontendDispatcher* frontend)
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
    InspectorProfilerFrontendDispatcher* m_frontendDispatcher;
    int m_totalWork;
};

};

void InspectorProfilerAgent::takeHeapSnapshot(ErrorString*, const bool* reportProgress)
{
    String title = makeString(UserInitiatedProfileName, '.', String::number(m_nextUserInitiatedHeapSnapshotNumber));
    ++m_nextUserInitiatedHeapSnapshotNumber;

    HeapSnapshotProgress progress(reportProgress && *reportProgress ? m_frontendDispatcher.get() : nullptr);
    RefPtr<ScriptHeapSnapshot> snapshot = ScriptProfiler::takeHeapSnapshot(title, &progress);
    if (snapshot) {
        m_snapshots.add(snapshot->uid(), snapshot);
        if (m_frontendDispatcher)
            m_frontendDispatcher->addProfileHeader(createSnapshotHeader(*snapshot));
    }
}

void InspectorProfilerAgent::toggleRecordButton(bool isProfiling)
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->setRecordingProfile(isProfiling);
}

void InspectorProfilerAgent::getObjectByHeapObjectId(ErrorString* error, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>& result)
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

void InspectorProfilerAgent::getHeapObjectId(ErrorString* errorString, const String& objectId, String* heapSnapshotObjectId)
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
