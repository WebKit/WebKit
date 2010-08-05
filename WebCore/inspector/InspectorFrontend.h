/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef InspectorFrontend_h
#define InspectorFrontend_h

#include "ScriptObject.h"
#include "ScriptState.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {
    class InspectorClient;
    class InspectorWorkerResource;

    class InspectorFrontend : public Noncopyable {
    public:
        // We are in transition from JS transport via webInspector to native
        // transport via inspectorClient. After migration, webInspector parameter should
        // be removed.
        InspectorFrontend(ScriptObject webInspector, InspectorClient* inspectorClient);
        ~InspectorFrontend();

        void close();
        void inspectedPageDestroyed();

        void didCommitLoad();

        void populateApplicationSettings(const String& settings);
        void populateSessionSettings(const String& settings);

        void updateFocusedNode(long nodeId);
        void showPanel(int panel);
        void populateInterface();
        void reset();
        void resetProfilesPanel();

        void bringToFront();
        void inspectedURLChanged(const String&);

        void resourceTrackingWasEnabled();
        void resourceTrackingWasDisabled();

        void searchingForNodeWasEnabled();
        void searchingForNodeWasDisabled();

        void monitoringXHRWasEnabled();
        void monitoringXHRWasDisabled();

        void updatePauseOnExceptionsState(long state);

#if ENABLE(JAVASCRIPT_DEBUGGER)
        void attachDebuggerWhenShown();
        void debuggerWasEnabled();
        void debuggerWasDisabled();

        void didSetBreakpoint(long callId, bool success, unsigned line);

        void parsedScriptSource(const String& sourceID, const String& url, const String& data, int firstLine, int scriptWorldType);
        void restoredBreakpoint(const String& sourceID, const String& url, int line, bool enabled, const String& condition);
        void failedToParseScriptSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage);

        void profilerWasEnabled();
        void profilerWasDisabled();
        void setRecordingProfile(bool isProfiling);
#endif

        void didPushNodeByPathToFrontend(long callId, long nodeId);

        void timelineProfilerWasStarted();
        void timelineProfilerWasStopped();

#if ENABLE(WORKERS)
        void didCreateWorker(const InspectorWorkerResource&);
        void didDestroyWorker(const InspectorWorkerResource&);
#endif // ENABLE(WORKER)

        void contextMenuItemSelected(int itemId);
        void contextMenuCleared();

        ScriptState* scriptState() const { return m_webInspector.scriptState(); }

        void evaluateForTestInFrontend(long callId, const String& script);

    private:
        void callSimpleFunction(const String& functionName);
        ScriptObject m_webInspector;
        InspectorClient* m_inspectorClient;
    };

} // namespace WebCore

#endif // !defined(InspectorFrontend_h)
