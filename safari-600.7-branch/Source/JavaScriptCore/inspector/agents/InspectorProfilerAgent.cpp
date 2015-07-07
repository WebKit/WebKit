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

#include "InspectorValues.h"
#include "LegacyProfiler.h"
#include "Profile.h"
#include "ScriptDebugServer.h"
#include "ScriptObject.h"
#include <wtf/CurrentTime.h>
#include <wtf/text/StringConcatenate.h>

namespace Inspector {

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";
static const char* const CPUProfileType = "CPU";

InspectorProfilerAgent::InspectorProfilerAgent()
    : InspectorAgentBase(ASCIILiteral("Profiler"))
    , m_scriptDebugServer(nullptr)
    , m_enabled(false)
    , m_profileHeadersRequested(false)
    , m_recordingProfileCount(0)
    , m_nextUserInitiatedProfileNumber(1)
{
}

InspectorProfilerAgent::~InspectorProfilerAgent()
{
}

void InspectorProfilerAgent::didCreateFrontendAndBackend(InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorProfilerFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorProfilerBackendDispatcher::create(backendDispatcher, this);
}

void InspectorProfilerAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason reason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    reset();

    disable(reason == InspectorDisconnectReason::InspectedTargetDestroyed ? SkipRecompile : Recompile);
}

void InspectorProfilerAgent::addProfile(PassRefPtr<JSC::Profile> prpProfile)
{
    RefPtr<JSC::Profile> profile = prpProfile;
    m_profiles.add(profile->uid(), profile);

    if (m_frontendDispatcher && m_profileHeadersRequested)
        m_frontendDispatcher->addProfileHeader(createProfileHeader(*profile));
}

PassRefPtr<TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::createProfileHeader(const JSC::Profile& profile)
{
    return TypeBuilder::Profiler::ProfileHeader::create()
        .setTypeId(TypeBuilder::Profiler::ProfileHeader::TypeId::CPU)
        .setUid(profile.uid())
        .setTitle(profile.title())
        .release();
}

void InspectorProfilerAgent::enable(ErrorString*)
{
    enable(Recompile);
}

void InspectorProfilerAgent::disable(ErrorString*)
{
    disable(Recompile);
}

void InspectorProfilerAgent::enable(ShouldRecompile shouldRecompile)
{
    if (m_enabled)
        return;

    m_enabled = true;

    if (shouldRecompile == Recompile)
        m_scriptDebugServer->recompileAllJSFunctions();
}

void InspectorProfilerAgent::disable(ShouldRecompile shouldRecompile)
{
    if (!m_enabled)
        return;

    m_enabled = false;
    m_profileHeadersRequested = false;

    if (shouldRecompile == Recompile)
        m_scriptDebugServer->recompileAllJSFunctions();
}

String InspectorProfilerAgent::getUserInitiatedProfileName()
{
    return makeString(UserInitiatedProfileName, '.', String::number(m_nextUserInitiatedProfileNumber++));
}

void InspectorProfilerAgent::getProfileHeaders(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::ProfileHeader>>& headers)
{
    m_profileHeadersRequested = true;
    headers = TypeBuilder::Array<TypeBuilder::Profiler::ProfileHeader>::create();

    for (auto& profile : m_profiles.values())
        headers->addItem(createProfileHeader(*profile.get()));
}

static PassRefPtr<TypeBuilder::Profiler::CPUProfileNodeCall> buildInspectorObject(const JSC::ProfileNode::Call& call)
{
    RefPtr<TypeBuilder::Profiler::CPUProfileNodeCall> result = TypeBuilder::Profiler::CPUProfileNodeCall::create()
        .setStartTime(call.startTime())
        .setTotalTime(call.totalTime());
    return result.release();
}

static PassRefPtr<TypeBuilder::Profiler::CPUProfileNode> buildInspectorObject(const JSC::ProfileNode* node)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNodeCall>> calls = TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNodeCall>::create();
    for (const JSC::ProfileNode::Call& call : node->calls())
        calls->addItem(buildInspectorObject(call));

    RefPtr<TypeBuilder::Profiler::CPUProfileNode> result = TypeBuilder::Profiler::CPUProfileNode::create()
        .setId(node->id())
        .setCalls(calls.release());

    if (!node->functionName().isEmpty())
        result->setFunctionName(node->functionName());

    if (!node->url().isEmpty()) {
        result->setUrl(node->url());
        result->setLineNumber(node->lineNumber());
        result->setColumnNumber(node->columnNumber());
    }

    if (!node->children().isEmpty()) {
        RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode>> children = TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode>::create();
        for (RefPtr<JSC::ProfileNode> profileNode : node->children())
            children->addItem(buildInspectorObject(profileNode.get()));
        result->setChildren(children);
    }

    return result.release();
}

PassRefPtr<TypeBuilder::Profiler::CPUProfile> InspectorProfilerAgent::buildProfileInspectorObject(const JSC::Profile* profile)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode>> rootNodes = TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode>::create();
    for (RefPtr<JSC::ProfileNode> profileNode : profile->head()->children())
        rootNodes->addItem(buildInspectorObject(profileNode.get()));

    RefPtr<TypeBuilder::Profiler::CPUProfile> result = TypeBuilder::Profiler::CPUProfile::create()
        .setRootNodes(rootNodes);

    if (profile->idleTime())
        result->setIdleTime(profile->idleTime());

    return result.release();
}

void InspectorProfilerAgent::getCPUProfile(ErrorString* errorString, int rawUid, RefPtr<TypeBuilder::Profiler::CPUProfile>& profileObject)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    auto it = m_profiles.find(uid);
    if (it == m_profiles.end()) {
        *errorString = ASCIILiteral("Profile wasn't found");
        return;
    }

    profileObject = buildProfileInspectorObject(it->value.get());
}

void InspectorProfilerAgent::removeProfile(ErrorString*, const String& type, int rawUid)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    if (type == CPUProfileType)
        m_profiles.remove(uid);
}

void InspectorProfilerAgent::reset()
{
    stop();

    m_profiles.clear();
    m_nextUserInitiatedProfileNumber = 1;
    m_profileHeadersRequested = false;

    if (m_frontendDispatcher && m_profileHeadersRequested)
        m_frontendDispatcher->resetProfiles();
}

void InspectorProfilerAgent::start(ErrorString*)
{
    startProfiling();
}

void InspectorProfilerAgent::stop(ErrorString*)
{
    stopProfiling();
}

void InspectorProfilerAgent::setRecordingProfile(bool isProfiling)
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->setRecordingProfile(isProfiling);
}

String InspectorProfilerAgent::startProfiling(const String &title, JSC::ExecState* exec)
{
    if (!enabled())
        enable(Recompile);

    bool wasNotRecording = !m_recordingProfileCount;
    ++m_recordingProfileCount;

    String resolvedTitle = title;
    if (title.isEmpty())
        resolvedTitle = getUserInitiatedProfileName();

    if (!exec)
        exec = profilingGlobalExecState();
    ASSERT(exec);

    JSC::LegacyProfiler::profiler()->startProfiling(exec, resolvedTitle);

    if (wasNotRecording)
        setRecordingProfile(true);

    return resolvedTitle;
}

PassRefPtr<JSC::Profile> InspectorProfilerAgent::stopProfiling(const String& title, JSC::ExecState* exec)
{
    if (!m_recordingProfileCount)
        return nullptr;

    --m_recordingProfileCount;

    if (!exec)
        exec = profilingGlobalExecState();
    ASSERT(exec);

    RefPtr<JSC::Profile> profile = JSC::LegacyProfiler::profiler()->stopProfiling(exec, title);
    if (profile)
        addProfile(profile);

    if (!m_recordingProfileCount)
        setRecordingProfile(false);

    return profile.release();
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
