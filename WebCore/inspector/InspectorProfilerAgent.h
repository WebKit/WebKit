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

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "PlatformString.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class InspectorArray;
class InspectorController;
class InspectorObject;
class ScriptProfile;
class RemoteInspectorFrontend;

class InspectorProfilerAgent : public Noncopyable {
public:
    static PassOwnPtr<InspectorProfilerAgent> create(InspectorController*);
    virtual ~InspectorProfilerAgent();

    void addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL);
    void addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);
    void clearProfiles() { resetState(); }
    void disable();
    void enable(bool skipRecompile);
    bool enabled() { return m_enabled; }
    String getCurrentUserInitiatedProfileName(bool incrementProfileNumber = false);
    void getProfileHeaders(RefPtr<InspectorArray>* headers);
    void getProfile(unsigned uid, RefPtr<InspectorObject>* profileObject);
    bool isRecordingUserInitiatedProfile() { return m_recordingUserInitiatedProfile; }
    void removeProfile(unsigned uid);
    void resetState();
    void setRemoteFrontend(RemoteInspectorFrontend* frontend) { m_remoteFrontend = frontend; }
    void startProfiling() { startUserInitiatedProfiling(); }
    void startUserInitiatedProfiling();
    void stopProfiling() { stopUserInitiatedProfiling(); }
    void stopUserInitiatedProfiling();
    void toggleRecordButton(bool isProfiling);

private:
    typedef HashMap<unsigned int, RefPtr<ScriptProfile> > ProfilesMap;

    InspectorProfilerAgent(InspectorController*);
    PassRefPtr<InspectorObject> createProfileHeader(const ScriptProfile& profile);

    InspectorController* m_inspectorController;
    RemoteInspectorFrontend* m_remoteFrontend;
    bool m_enabled;
    bool m_recordingUserInitiatedProfile;
    int m_currentUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedProfileNumber;
    ProfilesMap m_profiles;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // !defined(InspectorProfilerAgent_h)
