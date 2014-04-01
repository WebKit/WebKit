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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "InspectorProfilerAgent.h"

#if ENABLE(INSPECTOR)

#include "InspectorWebFrontendDispatchers.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageScriptDebugServer.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "URL.h"
#include "WebInjectedScriptManager.h"
#include "WorkerScriptDebugServer.h"
#include <bindings/ScriptObject.h>
#include <inspector/InjectedScript.h>
#include <inspector/InspectorValues.h>
#include <inspector/agents/InspectorConsoleAgent.h>
#include <runtime/ConsoleTypes.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/StringConcatenate.h>

using namespace Inspector;

namespace WebCore {

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";
static const char* const CPUProfileType = "CPU";

class PageProfilerAgent : public InspectorProfilerAgent {
public:
    PageProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, Page* inspectedPage, WebInjectedScriptManager* injectedScriptManager)
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

std::unique_ptr<InspectorProfilerAgent> InspectorProfilerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, Page* inspectedPage, WebInjectedScriptManager* injectedScriptManager)
{
    return std::make_unique<PageProfilerAgent>(instrumentingAgents, consoleAgent, inspectedPage, injectedScriptManager);
}

class WorkerProfilerAgent : public InspectorProfilerAgent {
public:
    WorkerProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, WorkerGlobalScope* workerGlobalScope, WebInjectedScriptManager* injectedScriptManager)
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

std::unique_ptr<InspectorProfilerAgent> InspectorProfilerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, WorkerGlobalScope* workerGlobalScope, WebInjectedScriptManager* injectedScriptManager)
{
    return std::make_unique<WorkerProfilerAgent>(instrumentingAgents, consoleAgent, workerGlobalScope, injectedScriptManager);
}

InspectorProfilerAgent::InspectorProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, WebInjectedScriptManager* injectedScriptManager)
    : InspectorAgentBase(ASCIILiteral("Profiler"), instrumentingAgents)
    , m_consoleAgent(consoleAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_enabled(false)
    , m_profileHeadersRequested(false)
    , m_recordingCPUProfile(false)
    , m_currentUserInitiatedProfileNumber(-1)
    , m_nextUserInitiatedProfileNumber(1)
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
    m_consoleAgent->addMessageToConsole(MessageSource::ConsoleAPI, MessageType::ProfileEnd, MessageLevel::Debug, message, sourceURL, lineNumber, columnNumber);
}

void InspectorProfilerAgent::addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, unsigned columnNumber, const String& sourceURL)
{
    if (!m_frontendDispatcher)
        return;
    m_consoleAgent->addMessageToConsole(MessageSource::ConsoleAPI, MessageType::Profile, MessageLevel::Debug, title, sourceURL, lineNumber, columnNumber);
}

PassRefPtr<Inspector::TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::createProfileHeader(const ScriptProfile& profile)
{
    return Inspector::TypeBuilder::Profiler::ProfileHeader::create()
        .setTypeId(Inspector::TypeBuilder::Profiler::ProfileHeader::TypeId::CPU)
        .setUid(profile.uid())
        .setTitle(profile.title())
        .release();
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
}

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

void InspectorProfilerAgent::removeProfile(ErrorString*, const String& type, int rawUid)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    if (type == CPUProfileType)
        m_profiles.remove(uid);
}

void InspectorProfilerAgent::resetState()
{
    stop();
    m_profiles.clear();
    m_currentUserInitiatedProfileNumber = 1;
    m_nextUserInitiatedProfileNumber = 1;
    resetFrontendProfiles();
}

void InspectorProfilerAgent::resetFrontendProfiles()
{
    if (!m_frontendDispatcher)
        return;
    if (!m_profileHeadersRequested)
        return;
    if (m_profiles.isEmpty())
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

void InspectorProfilerAgent::toggleRecordButton(bool isProfiling)
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->setRecordingProfile(isProfiling);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
