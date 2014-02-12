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

#ifndef InspectorHeapProfilerAgent_h
#define InspectorHeapProfilerAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ScriptHeapSnapshot;
class WebInjectedScriptManager;

typedef String ErrorString;

class InspectorHeapProfilerAgent : public InspectorAgentBase, public Inspector::InspectorHeapProfilerBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorHeapProfilerAgent); WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorHeapProfilerAgent(InstrumentingAgents*, WebInjectedScriptManager*);
    virtual ~InspectorHeapProfilerAgent();

    virtual void collectGarbage(ErrorString*) override;
    virtual void clearProfiles(ErrorString*) override { resetState(); }
    void resetState();

    virtual void hasHeapProfiler(ErrorString*, bool*) override;

    virtual void getProfileHeaders(ErrorString*, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::HeapProfiler::ProfileHeader>>&) override;
    virtual void getHeapSnapshot(ErrorString*, int uid) override;
    virtual void removeProfile(ErrorString*, int uid) override;

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;

    virtual void takeHeapSnapshot(ErrorString*, const bool* reportProgress) override;

    virtual void getObjectByHeapObjectId(ErrorString*, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>& result) override;
    virtual void getHeapObjectId(ErrorString*, const String& objectId, String* heapSnapshotObjectId) override;

private:
    typedef HashMap<unsigned, RefPtr<ScriptHeapSnapshot>> IdToHeapSnapshotMap;

    void resetFrontendProfiles();

    PassRefPtr<Inspector::TypeBuilder::HeapProfiler::ProfileHeader> createSnapshotHeader(const ScriptHeapSnapshot&);

    WebInjectedScriptManager* m_injectedScriptManager;
    std::unique_ptr<Inspector::InspectorHeapProfilerFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorHeapProfilerBackendDispatcher> m_backendDispatcher;
    unsigned m_nextUserInitiatedHeapSnapshotNumber;
    IdToHeapSnapshotMap m_snapshots;
    bool m_profileHeadersRequested;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorHeapProfilerAgent_h)
