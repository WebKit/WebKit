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

#ifndef InspectorProfilerAgent_h
#define InspectorProfilerAgent_h

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)

#include "InspectorBaseAgent.h"
#include "InspectorFrontend.h"
#include "PlatformString.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class InjectedScriptManager;
class InspectorArray;
class InspectorConsoleAgent;
class InspectorFrontend;
class InspectorObject;
class InspectorState;
class InstrumentingAgents;
class Page;
class ScriptHeapSnapshot;
class ScriptProfile;

typedef String ErrorString;

class InspectorProfilerAgent : public InspectorBaseAgent<InspectorProfilerAgent> {
    WTF_MAKE_NONCOPYABLE(InspectorProfilerAgent); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<InspectorProfilerAgent> create(InstrumentingAgents*, InspectorConsoleAgent*, Page*, InspectorState*, InjectedScriptManager*);
    virtual ~InspectorProfilerAgent();

    void addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL);
    void addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);
    void collectGarbage(ErrorString*);
    void clearProfiles(ErrorString*) { resetState(); }
    void resetState();

    void causesRecompilation(ErrorString*, bool*);
    void isSampling(ErrorString*, bool*);
    void hasHeapProfiler(ErrorString*, bool*);

    void enable(ErrorString*);
    void disable(ErrorString*);
    void start(ErrorString* = 0);
    void stop(ErrorString* = 0);

    void disable();
    void enable(bool skipRecompile);
    bool enabled() { return m_enabled; }
    String getCurrentUserInitiatedProfileName(bool incrementProfileNumber = false);
    void getProfileHeaders(ErrorString* error, RefPtr<InspectorArray>* headers);
    void getProfile(ErrorString* error, const String& type, unsigned uid, RefPtr<InspectorObject>* profileObject);
    void removeProfile(ErrorString* error, const String& type, unsigned uid);

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    void takeHeapSnapshot(ErrorString*);
    void toggleRecordButton(bool isProfiling);

    void getObjectByHeapObjectId(ErrorString*, int id, RefPtr<InspectorObject>* result);

private:
    typedef HashMap<unsigned int, RefPtr<ScriptProfile> > ProfilesMap;
    typedef HashMap<unsigned int, RefPtr<ScriptHeapSnapshot> > HeapSnapshotsMap;

    void resetFrontendProfiles();
    void restoreEnablement();

    InspectorProfilerAgent(InstrumentingAgents*, InspectorConsoleAgent*, Page*, InspectorState*, InjectedScriptManager*);
    PassRefPtr<InspectorObject> createProfileHeader(const ScriptProfile& profile);
    PassRefPtr<InspectorObject> createSnapshotHeader(const ScriptHeapSnapshot& snapshot);

    InspectorConsoleAgent* m_consoleAgent;
    Page* m_inspectedPage;
    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::Profiler* m_frontend;
    bool m_enabled;
    bool m_recordingUserInitiatedProfile;
    int m_currentUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedHeapSnapshotNumber;
    ProfilesMap m_profiles;
    HeapSnapshotsMap m_snapshots;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)

#endif // !defined(InspectorProfilerAgent_h)
