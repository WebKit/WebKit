/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorAgent_h
#define InspectorAgent_h

#include "PlatformString.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMWrapperWorld;
class DocumentLoader;
class Frame;
class InjectedScriptManager;
class InspectorFrontend;
class InspectorObject;
class InspectorWorkerResource;
class InstrumentingAgents;
class KURL;
class Page;

typedef String ErrorString;

class InspectorAgent {
    WTF_MAKE_NONCOPYABLE(InspectorAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorAgent(Page*, InjectedScriptManager*, InstrumentingAgents*);
    virtual ~InspectorAgent();

    void inspectedPageDestroyed();

    bool enabled() const;

    KURL inspectedURL() const;
    KURL inspectedURLWithoutFragment() const;
    void reloadPage(ErrorString*, bool ignoreCache);
    void showConsole();

    void setFrontend(InspectorFrontend*);
    InspectorFrontend* frontend() const { return m_frontend; }
    void clearFrontend();
    void restore();

    void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);

    void didCommitLoad();
    void domContentLoadedEventFired();

#if ENABLE(WORKERS)
    enum WorkerAction { WorkerCreated, WorkerDestroyed };

    void postWorkerNotificationToFrontend(const InspectorWorkerResource&, WorkerAction);
    void didCreateWorker(intptr_t, const String& url, bool isSharedWorker);
    void didDestroyWorker(intptr_t);
#endif

    bool hasFrontend() const { return m_frontend; }


#if ENABLE(JAVASCRIPT_DEBUGGER)
    void showProfilesPanel();
#endif

    // Generic code called from custom implementations.
    void evaluateForTestInFrontend(long testCallId, const String& script);

    void setInspectorExtensionAPI(const String& source);

    // InspectorAgent API
    void getInspectorState(RefPtr<InspectorObject>* state);
    void setMonitoringXHREnabled(bool enabled, bool* newState);

private:
    void showPanel(const String& panel);
    void unbindAllResources();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    void toggleRecordButton(bool);
#endif

    bool isMainResourceLoader(DocumentLoader*, const KURL& requestUrl);
    void issueEvaluateForTestCommands();

    Page* m_inspectedPage;
    InspectorFrontend* m_frontend;
    InstrumentingAgents* m_instrumentingAgents;
    InjectedScriptManager* m_injectedScriptManager;

    Vector<pair<long, String> > m_pendingEvaluateTestCommands;
    String m_showPanelAfterVisible;
    String m_inspectorExtensionAPI;
#if ENABLE(WORKERS)
    typedef HashMap<intptr_t, RefPtr<InspectorWorkerResource> > WorkersMap;
    WorkersMap m_workers;
#endif
    bool m_canIssueEvaluateForTestInFrontend;
};

} // namespace WebCore

#endif // !defined(InspectorAgent_h)
