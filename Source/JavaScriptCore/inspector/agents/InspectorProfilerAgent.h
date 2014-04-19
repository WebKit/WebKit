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

#include "InspectorJSBackendDispatchers.h"
#include "InspectorJSFrontendDispatchers.h"
#include "inspector/InspectorAgentBase.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class ExecState;
class Profile;
}

namespace Inspector {

class ScriptDebugServer;

typedef String ErrorString;

class JS_EXPORT_PRIVATE InspectorProfilerAgent : public InspectorAgentBase, public InspectorProfilerBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorProfilerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~InspectorProfilerAgent();

    virtual void didCreateFrontendAndBackend(InspectorFrontendChannel*, InspectorBackendDispatcher*) override final;
    virtual void willDestroyFrontendAndBackend(InspectorDisconnectReason) override final;

    virtual void enable(ErrorString*) override final;
    virtual void disable(ErrorString*) override final;

    virtual void start(ErrorString* = nullptr) override final;
    virtual void stop(ErrorString* = nullptr) override final;

    virtual void clearProfiles(ErrorString*) override final { reset(); }

    virtual void getProfileHeaders(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::ProfileHeader>>&) override final;
    virtual void getCPUProfile(ErrorString*, int uid, RefPtr<TypeBuilder::Profiler::CPUProfile>&) override final;
    virtual void removeProfile(ErrorString*, const String& type, int uid) override final;

    static PassRefPtr<TypeBuilder::Profiler::CPUProfile> buildProfileInspectorObject(const JSC::Profile*);

    enum ShouldRecompile { SkipRecompile, Recompile };

    virtual void enable(ShouldRecompile);
    virtual void disable(ShouldRecompile);

    bool enabled() const { return m_enabled; }

    void reset();

    void setScriptDebugServer(ScriptDebugServer* scriptDebugServer) { m_scriptDebugServer = scriptDebugServer; }

    String startProfiling(const String& title = String(), JSC::ExecState* = nullptr);
    PassRefPtr<JSC::Profile> stopProfiling(const String& title = String(), JSC::ExecState* = nullptr);

protected:
    InspectorProfilerAgent();

    virtual JSC::ExecState* profilingGlobalExecState() const = 0;

private:
    void addProfile(PassRefPtr<JSC::Profile>);

    void setRecordingProfile(bool isProfiling);

    String getUserInitiatedProfileName();

    PassRefPtr<TypeBuilder::Profiler::ProfileHeader> createProfileHeader(const JSC::Profile&);

    std::unique_ptr<InspectorProfilerFrontendDispatcher> m_frontendDispatcher;
    RefPtr<InspectorProfilerBackendDispatcher> m_backendDispatcher;
    HashMap<unsigned, RefPtr<JSC::Profile>> m_profiles;
    ScriptDebugServer* m_scriptDebugServer;
    bool m_enabled;
    bool m_profileHeadersRequested;
    unsigned m_recordingProfileCount;
    unsigned m_nextUserInitiatedProfileNumber;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorProfilerAgent_h)
