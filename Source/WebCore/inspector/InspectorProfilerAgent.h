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

#ifndef InspectorProfilerAgent_h
#define InspectorProfilerAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InspectorArray;
class InspectorConsoleAgent;
class InspectorObject;
}

namespace WebCore {

class InstrumentingAgents;
class Page;
class ScriptHeapSnapshot;
class ScriptProfile;
class WebInjectedScriptManager;
class WorkerGlobalScope;

typedef String ErrorString;

class InspectorProfilerAgent : public InspectorAgentBase, public Inspector::InspectorProfilerBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorProfilerAgent); WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<InspectorProfilerAgent> create(InstrumentingAgents*, Inspector::InspectorConsoleAgent*, Page*, WebInjectedScriptManager*);
    static std::unique_ptr<InspectorProfilerAgent> create(InstrumentingAgents*, Inspector::InspectorConsoleAgent*, WorkerGlobalScope*, WebInjectedScriptManager*);

    virtual ~InspectorProfilerAgent();

    void addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, unsigned columnNumber, const String& sourceURL);
    void addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile>, unsigned lineNumber, unsigned columnNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, unsigned columnNumber, const String& sourceURL);
    virtual void collectGarbage(ErrorString*) override;
    virtual void clearProfiles(ErrorString*) override { resetState(); }
    void resetState();

    virtual void recompileScript() = 0;
    virtual void isSampling(ErrorString*, bool*) override;
    virtual void hasHeapProfiler(ErrorString*, bool*) override;

    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void start(ErrorString* = nullptr) override;
    virtual void stop(ErrorString* = nullptr) override;

    void disable(bool skipRecompile);
    void enable(bool skipRecompile);
    bool enabled() const { return m_enabled; }
    String getCurrentUserInitiatedProfileName(bool incrementProfileNumber = false);
    virtual void getProfileHeaders(ErrorString*, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::ProfileHeader>>&) override;
    virtual void getCPUProfile(ErrorString*, int uid, RefPtr<Inspector::TypeBuilder::Profiler::CPUProfile>&) override;
    virtual void getHeapSnapshot(ErrorString*, int uid) override;
    virtual void removeProfile(ErrorString*, const String& type, int uid) override;

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;

    virtual void takeHeapSnapshot(ErrorString*, const bool* reportProgress) override;
    void toggleRecordButton(bool isProfiling);

    virtual void getObjectByHeapObjectId(ErrorString*, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>& result) override;
    virtual void getHeapObjectId(ErrorString*, const String& objectId, String* heapSnapshotObjectId) override;

protected:
    InspectorProfilerAgent(InstrumentingAgents*, Inspector::InspectorConsoleAgent*, WebInjectedScriptManager*);
    virtual void startProfiling(const String& title) = 0;
    virtual PassRefPtr<ScriptProfile> stopProfiling(const String& title) = 0;

private:
    typedef HashMap<unsigned int, RefPtr<ScriptProfile>> ProfilesMap;
    typedef HashMap<unsigned int, RefPtr<ScriptHeapSnapshot>> HeapSnapshotsMap;

    void resetFrontendProfiles();
    void restoreEnablement();

    PassRefPtr<Inspector::TypeBuilder::Profiler::ProfileHeader> createProfileHeader(const ScriptProfile&);
    PassRefPtr<Inspector::TypeBuilder::Profiler::ProfileHeader> createSnapshotHeader(const ScriptHeapSnapshot&);

    Inspector::InspectorConsoleAgent* m_consoleAgent;
    WebInjectedScriptManager* m_injectedScriptManager;
    std::unique_ptr<Inspector::InspectorProfilerFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorProfilerBackendDispatcher> m_backendDispatcher;
    bool m_enabled;
    bool m_profileHeadersRequested;
    bool m_recordingCPUProfile;
    int m_currentUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedHeapSnapshotNumber;
    ProfilesMap m_profiles;
    HeapSnapshotsMap m_snapshots;

    typedef HashMap<String, double> ProfileNameIdleTimeMap;
    ProfileNameIdleTimeMap* m_profileNameIdleTimeMap;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorProfilerAgent_h)
