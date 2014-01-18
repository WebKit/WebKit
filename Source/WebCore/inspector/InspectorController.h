/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorController_h
#define InspectorController_h

#if ENABLE(INSPECTOR)

#include "InspectorInstrumentationCookie.h"
#include <inspector/InspectorAgentRegistry.h>
#include <inspector/InspectorEnvironment.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InspectorAgent;
class InspectorObject;
class InspectorFrontendChannel;
class InspectorBackendDispatcher;
}

namespace WebCore {

class DOMWrapperWorld;
class Frame;
class GraphicsContext;
class InspectorApplicationCacheAgent;
class InspectorClient;
class InspectorDOMAgent;
class InspectorDOMDebuggerAgent;
class InspectorDebuggerAgent;
class InspectorFrontendClient;
class InspectorMemoryAgent;
class InspectorOverlay;
class InspectorPageAgent;
class InspectorProfilerAgent;
class InspectorResourceAgent;
class InstrumentingAgents;
class IntSize;
class Page;
class PageInjectedScriptManager;
class Node;

struct Highlight;

class InspectorController final : public Inspector::InspectorEnvironment {
    WTF_MAKE_NONCOPYABLE(InspectorController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorController(Page&, InspectorClient*);
    virtual ~InspectorController();

    void inspectedPageDestroyed();

    bool enabled() const;
    Page& inspectedPage() const;

    void show();
    void close();

    void setInspectorFrontendClient(std::unique_ptr<InspectorFrontendClient>);
    bool hasInspectorFrontendClient() const;
    void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld&);

    void dispatchMessageFromFrontend(const String& message);

    bool hasFrontend() const { return !!m_inspectorFrontendChannel; }
    bool hasLocalFrontend() const;
    bool hasRemoteFrontend() const;

    void connectFrontend(Inspector::InspectorFrontendChannel*);
    void disconnectFrontend();
    void setProcessId(long);

#if ENABLE(REMOTE_INSPECTOR)
    void setHasRemoteFrontend(bool hasRemote) { m_hasRemoteFrontend = hasRemote; }
#endif

    void inspect(Node*);
    void drawHighlight(GraphicsContext&) const;
    void getHighlight(Highlight*) const;
    void hideHighlight();
    Node* highlightedNode() const;

    void setIndicating(bool);

    PassRefPtr<Inspector::InspectorObject> buildObjectForHighlightedNode() const;

    bool isUnderTest();
    void evaluateForTestInFrontend(long callId, const String& script);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    bool profilerEnabled() const;
    void setProfilerEnabled(bool);

    void resume();
#endif

    void setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize);

    InspectorClient* inspectorClient() const { return m_inspectorClient; }
    InspectorPageAgent* pageAgent() const { return m_pageAgent; }

    void didBeginFrame();
    void didCancelFrame();
    void willComposite();
    void didComposite();

    virtual bool developerExtrasEnabled() const override;
    virtual bool canAccessInspectedScriptState(JSC::ExecState*) const override;
    virtual Inspector::InspectorFunctionCallHandler functionCallHandler() const override;
    virtual Inspector::InspectorEvaluateHandler evaluateHandler() const override;
    virtual void willCallInjectedScriptFunction(JSC::ExecState*, const String& scriptName, int scriptLine) override;
    virtual void didCallInjectedScriptFunction() override;

private:
    friend InstrumentingAgents* instrumentationForPage(Page*);

    RefPtr<InstrumentingAgents> m_instrumentingAgents;
    std::unique_ptr<PageInjectedScriptManager> m_injectedScriptManager;
    std::unique_ptr<InspectorOverlay> m_overlay;

    Inspector::InspectorAgent* m_inspectorAgent;
    InspectorDOMAgent* m_domAgent;
    InspectorResourceAgent* m_resourceAgent;
    InspectorPageAgent* m_pageAgent;
    InspectorMemoryAgent* m_memoryAgent;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* m_debuggerAgent;
    InspectorDOMDebuggerAgent* m_domDebuggerAgent;
    InspectorProfilerAgent* m_profilerAgent;
#endif

    RefPtr<Inspector::InspectorBackendDispatcher> m_inspectorBackendDispatcher;
    std::unique_ptr<InspectorFrontendClient> m_inspectorFrontendClient;
    Inspector::InspectorFrontendChannel* m_inspectorFrontendChannel;
    Page& m_page;
    InspectorClient* m_inspectorClient;
    Inspector::InspectorAgentRegistry m_agents;
    Vector<InspectorInstrumentationCookie, 2> m_injectedScriptInstrumentationCookies;
    bool m_isUnderTest;

#if ENABLE(REMOTE_INSPECTOR)
    bool m_hasRemoteFrontend;
#endif
};

}

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorController_h)
