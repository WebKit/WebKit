/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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

#include "config.h"
#include "InspectorAgent.h"

#if ENABLE(INSPECTOR)

#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMWindow.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatConversion.h"
#include "FloatQuad.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HTTPHeaderMap.h"
#include "HitTestResult.h"
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorBrowserDebuggerAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorClient.h"
#include "InspectorConsoleAgent.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorFrontend.h"
#include "InspectorFrontendClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorRuntimeAgent.h"
#include "InspectorState.h"
#include "InspectorTimelineAgent.h"
#include "InspectorValues.h"
#include "InspectorWorkerResource.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderInline.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "ScriptSourceCode.h"
#include "ScriptState.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "TextRun.h"
#include "UserGestureIndicator.h"
#include "WindowFeatures.h"
#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/StringConcatenate.h>

#if ENABLE(DATABASE)
#include "InspectorDatabaseAgent.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "InspectorDOMStorageAgent.h"
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "InspectorApplicationCacheAgent.h"
#endif

using namespace std;

namespace WebCore {

namespace InspectorAgentState {
static const char searchingForNode[] = "searchingForNode";
static const char timelineProfilerEnabled[] = "timelineProfilerEnabled";
static const char userInitiatedProfiling[] = "userInitiatedProfiling";
static const char debuggerEnabled[] = "debuggerEnabled";
static const char profilerEnabled[] = "profilerEnabled";
}

static const char scriptsPanelName[] = "scripts";
static const char consolePanelName[] = "console";
static const char profilesPanelName[] = "profiles";

InspectorAgent::InspectorAgent(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_frontend(0)
    , m_instrumentingAgents(new InstrumentingAgents())
    , m_injectedScriptHost(InjectedScriptHost::create(this))
    , m_domAgent(InspectorDOMAgent::create(m_instrumentingAgents.get(), m_injectedScriptHost.get()))
    , m_cssAgent(new InspectorCSSAgent(m_domAgent.get()))
#if ENABLE(DATABASE)
    , m_databaseAgent(InspectorDatabaseAgent::create(m_instrumentingAgents.get()))
#endif
#if ENABLE(DOM_STORAGE)
    , m_domStorageAgent(InspectorDOMStorageAgent::create(m_instrumentingAgents.get()))
#endif
    , m_state(new InspectorState(client))
    , m_timelineAgent(InspectorTimelineAgent::create(m_instrumentingAgents.get(), m_state.get()))
    , m_consoleAgent(new InspectorConsoleAgent(m_instrumentingAgents.get(), this, m_state.get(), m_injectedScriptHost.get(), m_domAgent.get()))
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_debuggerAgent(InspectorDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), page, m_injectedScriptHost.get()))
    , m_browserDebuggerAgent(InspectorBrowserDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), m_domAgent.get(), m_debuggerAgent.get(), this))
    , m_profilerAgent(InspectorProfilerAgent::create(this))
#endif
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
    InspectorInstrumentation::bindInspectorAgent(m_inspectedPage, this);
}

InspectorAgent::~InspectorAgent()
{
    // These should have been cleared in inspectedPageDestroyed().
    ASSERT(!m_client);
    ASSERT(!m_inspectedPage);
    ASSERT(!m_highlightedNode);
}

void InspectorAgent::inspectedPageDestroyed()
{
    if (m_frontend)
        m_frontend->disconnectFromBackend();

    ErrorString error;
    hideHighlight(&error);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent.clear();
    m_browserDebuggerAgent.clear();
#endif

    ASSERT(m_inspectedPage);
    InspectorInstrumentation::unbindInspectorAgent(m_inspectedPage);
    m_inspectedPage = 0;

    releaseFrontendLifetimeAgents();
    m_injectedScriptHost->disconnectController();

    m_client->inspectorDestroyed();
    m_client = 0;
}

bool InspectorAgent::searchingForNodeInPage() const
{
    return m_state->getBoolean(InspectorAgentState::searchingForNode);
}

void InspectorAgent::restoreInspectorStateFromCookie(const String& inspectorStateCookie)
{
    m_state->loadFromCookie(inspectorStateCookie);

    m_frontend->frontendReused();
    m_frontend->inspectedURLChanged(inspectedURL().string());
    pushDataCollectedOffline();

    m_resourceAgent = InspectorResourceAgent::restore(m_inspectedPage, m_state.get(), m_frontend);
    m_timelineAgent->restore(m_state.get(), m_frontend);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->restore();
    restoreProfiler(ProfilerRestoreResetAgent);
    if (m_state->getBoolean(InspectorAgentState::userInitiatedProfiling))
        startUserInitiatedProfiling();
#endif
}

void InspectorAgent::inspect(Node* node)
{
    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!m_frontend)
        return;

    focusNode();
}

void InspectorAgent::focusNode()
{
    if (!enabled())
        return;

    ASSERT(m_frontend);
    ASSERT(m_nodeToFocus);

    RefPtr<Node> node = m_nodeToFocus.get();
    m_nodeToFocus = 0;

    Document* document = node->ownerDocument();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;

    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptFor(mainWorldScriptState(frame));
    if (injectedScript.hasNoValue())
        return;

    injectedScript.inspectNode(node.get());
}

void InspectorAgent::highlight(ErrorString*, Node* node)
{
    if (!enabled())
        return;
    ASSERT_ARG(node, node);
    m_highlightedNode = node;
    m_client->highlight(node);
}

void InspectorAgent::highlightDOMNode(ErrorString* error, long nodeId)
{
    Node* node = 0;
    if (m_domAgent && (node = m_domAgent->nodeForId(nodeId)))
        highlight(error, node);
}

void InspectorAgent::highlightFrame(ErrorString* error, unsigned long frameId)
{
    Frame* mainFrame = m_inspectedPage->mainFrame();
    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext(mainFrame)) {
        if (reinterpret_cast<uintptr_t>(frame) == frameId && frame->ownerElement()) {
            highlight(error, frame->ownerElement());
            return;
        }
    }
}

void InspectorAgent::hideHighlight(ErrorString*)
{
    if (!enabled())
        return;
    m_highlightedNode = 0;
    m_client->hideHighlight();
}

void InspectorAgent::mouseDidMoveOverElement(const HitTestResult& result, unsigned)
{
    if (!enabled() || !searchingForNodeInPage())
        return;

    Node* node = result.innerNode();
    while (node && node->nodeType() == Node::TEXT_NODE)
        node = node->parentNode();
    if (node) {
        ErrorString error;
        highlight(&error, node);
    }
}

bool InspectorAgent::handleMousePress()
{
    if (!enabled() || !searchingForNodeInPage())
        return false;

    if (m_highlightedNode) {
        RefPtr<Node> node = m_highlightedNode;
        setSearchingForNode(false);
        inspect(node.get());
    }
    return true;
}

void InspectorAgent::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    if (enabled()) {
        if (m_frontend && frame == m_inspectedPage->mainFrame())
            m_injectedScriptHost->discardInjectedScripts();

        if (m_scriptsToEvaluateOnLoad.size()) {
            ScriptState* scriptState = mainWorldScriptState(frame);
            for (Vector<String>::iterator it = m_scriptsToEvaluateOnLoad.begin();
                  it != m_scriptsToEvaluateOnLoad.end(); ++it) {
                m_injectedScriptHost->injectScript(*it, scriptState);
            }
        }
    }

    if (!m_inspectorExtensionAPI.isEmpty())
        m_injectedScriptHost->injectScript(m_inspectorExtensionAPI, mainWorldScriptState(frame));
}

void InspectorAgent::setSearchingForNode(bool enabled)
{
    if (searchingForNodeInPage() == enabled)
        return;
    m_state->setBoolean(InspectorAgentState::searchingForNode, enabled);
    if (!enabled) {
        ErrorString error;
        hideHighlight(&error);
    }
}

void InspectorAgent::setSearchingForNode(ErrorString*, bool enabled, bool* newState)
{
    *newState = enabled;
    setSearchingForNode(enabled);
}

void InspectorAgent::setFrontend(InspectorFrontend* inspectorFrontend)
{
    // We can reconnect to existing front-end -> unmute state.
    m_state->unmute();

    m_frontend = inspectorFrontend;
    createFrontendLifetimeAgents();

    m_domAgent->setFrontend(m_frontend);
    m_consoleAgent->setFrontend(m_frontend);
    m_timelineAgent->setFrontend(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->setFrontend(m_frontend);
    m_browserDebuggerAgent->setFrontend(m_frontend);
#endif
#if ENABLE(DATABASE)
    m_databaseAgent->setFrontend(m_frontend);
#endif
#if ENABLE(DOM_STORAGE)
    m_domStorageAgent->setFrontend(m_frontend);
#endif
    // Initialize Web Inspector title.
    m_frontend->inspectedURLChanged(inspectedURL().string());
}

void InspectorAgent::disconnectFrontend()
{
    if (!m_frontend)
        return;

    // Destroying agents would change the state, but we don't want that.
    // Pre-disconnect state will be used to restore inspector agents.
    m_state->mute();

    m_frontend = 0;

    ErrorString error;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->clearFrontend();
    m_browserDebuggerAgent->clearFrontend();
#endif
    setSearchingForNode(false);

    hideHighlight(&error);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_profilerAgent->setFrontend(0);
    m_profilerAgent->stopUserInitiatedProfiling(true);
#endif

    m_consoleAgent->clearFrontend();
    m_domAgent->clearFrontend();
    m_timelineAgent->clearFrontend();
#if ENABLE(DATABASE)
    m_databaseAgent->clearFrontend();
#endif
#if ENABLE(DOM_STORAGE)
    m_domStorageAgent->clearFrontend();
#endif

    releaseFrontendLifetimeAgents();
    m_userAgentOverride = "";
}

InspectorResourceAgent* InspectorAgent::resourceAgent()
{
    if (!m_resourceAgent && m_frontend)
        m_resourceAgent = InspectorResourceAgent::create(m_inspectedPage, m_state.get(), m_frontend);
    return m_resourceAgent.get();
}

void InspectorAgent::createFrontendLifetimeAgents()
{
    m_runtimeAgent = InspectorRuntimeAgent::create(m_injectedScriptHost.get());

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    m_applicationCacheAgent = new InspectorApplicationCacheAgent(m_inspectedPage->mainFrame()->loader()->documentLoader(), m_frontend);
#endif
}

void InspectorAgent::releaseFrontendLifetimeAgents()
{
    m_resourceAgent.clear();
    m_runtimeAgent.clear();
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    m_applicationCacheAgent.clear();
#endif
}

void InspectorAgent::populateScriptObjects(ErrorString*)
{
    ASSERT(m_frontend);
    if (!m_frontend)
        return;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_profilerAgent->enabled())
        m_frontend->profilerWasEnabled();
#endif

    pushDataCollectedOffline();

    if (m_nodeToFocus)
        focusNode();

    if (!m_showPanelAfterVisible.isEmpty()) {
        m_frontend->showPanel(m_showPanelAfterVisible);
        m_showPanelAfterVisible = "";
    }

    restoreProfiler(ProfilerRestoreNoAction);

    // Dispatch pending frontend commands
    for (Vector<pair<long, String> >::iterator it = m_pendingEvaluateTestCommands.begin(); it != m_pendingEvaluateTestCommands.end(); ++it)
        m_frontend->evaluateForTestInFrontend((*it).first, (*it).second);
    m_pendingEvaluateTestCommands.clear();
}

void InspectorAgent::pushDataCollectedOffline()
{
    m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(WORKERS)
    WorkersMap::iterator workersEnd = m_workers.end();
    for (WorkersMap::iterator it = m_workers.begin(); it != workersEnd; ++it) {
        InspectorWorkerResource* worker = it->second.get();
        m_frontend->didCreateWorker(worker->id(), worker->url(), worker->isSharedWorker());
    }
#endif
}

void InspectorAgent::restoreProfiler(ProfilerRestoreAction action)
{
    ASSERT(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_profilerAgent->setFrontend(m_frontend);
    if (m_state->getBoolean(InspectorAgentState::profilerEnabled)) {
        ErrorString error;
        enableProfiler(&error);
    }
    if (action == ProfilerRestoreResetAgent)
        m_profilerAgent->resetFrontendProfiles();
#endif
}

void InspectorAgent::didCommitLoad(DocumentLoader* loader)
{
    if (!enabled())
        return;

    if (m_resourceAgent)
        m_resourceAgent->didCommitLoad(loader);

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        if (m_frontend)
            m_frontend->inspectedURLChanged(loader->url().string());

        m_injectedScriptHost->discardInjectedScripts();
        m_consoleAgent->reset();

        if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
            timelineAgent->didCommitLoad();

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        if (m_applicationCacheAgent)
            m_applicationCacheAgent->didCommitLoad(loader);
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
        if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents->inspectorDebuggerAgent()) {
            KURL url = inspectedURLWithoutFragment();
            debuggerAgent->inspectedURLChanged(url);
            if (InspectorBrowserDebuggerAgent* browserDebuggerAgent = m_instrumentingAgents->inspectorBrowserDebuggerAgent())
                browserDebuggerAgent->inspectedURLChanged(url);
        }
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
        m_profilerAgent->stopUserInitiatedProfiling(true);
        m_profilerAgent->resetState();
#endif

        if (m_frontend) {
            m_frontend->reset();
            m_domAgent->reset();
            m_cssAgent->reset();
        }
#if ENABLE(WORKERS)
        m_workers.clear();
#endif
#if ENABLE(DATABASE)
        m_databaseAgent->clearResources();
#endif
#if ENABLE(DOM_STORAGE)
        m_domStorageAgent->clearResources();
#endif
        if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
            domAgent->setDocument(m_inspectedPage->mainFrame()->document());
    }
}

void InspectorAgent::domContentLoadedEventFired(DocumentLoader* loader, const KURL& url)
{
    if (!enabled() || !isMainResourceLoader(loader, url))
        return;

    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        domAgent->mainFrameDOMContentLoaded();
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didMarkDOMContentEvent();
    if (m_frontend)
        m_frontend->domContentEventFired(currentTime());
}

void InspectorAgent::loadEventFired(DocumentLoader* loader, const KURL& url)
{
    if (!enabled())
        return;

    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        domAgent->loadEventFired(loader->frame()->document());

    if (!isMainResourceLoader(loader, url))
        return;

    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didMarkLoadEvent();
    if (m_frontend)
        m_frontend->loadEventFired(currentTime());
}

bool InspectorAgent::isMainResourceLoader(DocumentLoader* loader, const KURL& requestUrl)
{
    return loader->frame() == m_inspectedPage->mainFrame() && requestUrl == loader->requestURL();
}

void InspectorAgent::setUserAgentOverride(ErrorString*, const String& userAgent)
{
    m_userAgentOverride = userAgent;
}

void InspectorAgent::applyUserAgentOverride(String* userAgent) const
{
    if (!m_userAgentOverride.isEmpty())
        *userAgent = m_userAgentOverride;
}

#if ENABLE(WORKERS)
class PostWorkerNotificationToFrontendTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<PostWorkerNotificationToFrontendTask> create(PassRefPtr<InspectorWorkerResource> worker, InspectorAgent::WorkerAction action)
    {
        return new PostWorkerNotificationToFrontendTask(worker, action);
    }

private:
    PostWorkerNotificationToFrontendTask(PassRefPtr<InspectorWorkerResource> worker, InspectorAgent::WorkerAction action)
        : m_worker(worker)
        , m_action(action)
    {
    }

    virtual void performTask(ScriptExecutionContext* scriptContext)
    {
        if (scriptContext->isDocument()) {
            if (InspectorAgent* inspectorAgent = static_cast<Document*>(scriptContext)->page()->inspectorController()->m_inspectorAgent.get())
                inspectorAgent->postWorkerNotificationToFrontend(*m_worker, m_action);
        }
    }

private:
    RefPtr<InspectorWorkerResource> m_worker;
    InspectorAgent::WorkerAction m_action;
};

void InspectorAgent::postWorkerNotificationToFrontend(const InspectorWorkerResource& worker, InspectorAgent::WorkerAction action)
{
    if (!m_frontend)
        return;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    switch (action) {
    case InspectorAgent::WorkerCreated:
        m_frontend->didCreateWorker(worker.id(), worker.url(), worker.isSharedWorker());
        break;
    case InspectorAgent::WorkerDestroyed:
        m_frontend->didDestroyWorker(worker.id());
        break;
    }
#endif
}

void InspectorAgent::didCreateWorker(intptr_t id, const String& url, bool isSharedWorker)
{
    if (!enabled())
        return;

    RefPtr<InspectorWorkerResource> workerResource(InspectorWorkerResource::create(id, url, isSharedWorker));
    m_workers.set(id, workerResource);
    if (m_inspectedPage && m_frontend)
        m_inspectedPage->mainFrame()->document()->postTask(PostWorkerNotificationToFrontendTask::create(workerResource, InspectorAgent::WorkerCreated));
}

void InspectorAgent::didDestroyWorker(intptr_t id)
{
    if (!enabled())
        return;

    WorkersMap::iterator workerResource = m_workers.find(id);
    if (workerResource == m_workers.end())
        return;
    if (m_inspectedPage && m_frontend)
        m_inspectedPage->mainFrame()->document()->postTask(PostWorkerNotificationToFrontendTask::create(workerResource->second, InspectorAgent::WorkerDestroyed));
    m_workers.remove(workerResource);
}
#endif // ENABLE(WORKERS)

void InspectorAgent::getCookies(ErrorString*, RefPtr<InspectorArray>* cookies, WTF::String* cookiesString)
{
    // If we can get raw cookies.
    ListHashSet<Cookie> rawCookiesList;

    // If we can't get raw cookies - fall back to String representation
    String stringCookiesList;

    // Return value to getRawCookies should be the same for every call because
    // the return value is platform/network backend specific, and the call will
    // always return the same true/false value.
    bool rawCookiesImplemented = false;

    for (Frame* frame = m_inspectedPage->mainFrame(); frame; frame = frame->tree()->traverseNext(m_inspectedPage->mainFrame())) {
        Document* document = frame->document();
        const CachedResourceLoader::DocumentResourceMap& allResources = document->cachedResourceLoader()->allCachedResources();
        CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
        for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it) {
            Vector<Cookie> docCookiesList;
            rawCookiesImplemented = getRawCookies(document, KURL(ParsedURLString, it->second->url()), docCookiesList);

            if (!rawCookiesImplemented) {
                // FIXME: We need duplication checking for the String representation of cookies.
                ExceptionCode ec = 0;
                stringCookiesList += document->cookie(ec);
                // Exceptions are thrown by cookie() in sandboxed frames. That won't happen here
                // because "document" is the document of the main frame of the page.
                ASSERT(!ec);
            } else {
                int cookiesSize = docCookiesList.size();
                for (int i = 0; i < cookiesSize; i++) {
                    if (!rawCookiesList.contains(docCookiesList[i]))
                        rawCookiesList.add(docCookiesList[i]);
                }
            }
        }
    }

    if (rawCookiesImplemented)
        *cookies = buildArrayForCookies(rawCookiesList);
    else
        *cookiesString = stringCookiesList;
}

PassRefPtr<InspectorArray> InspectorAgent::buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    RefPtr<InspectorArray> cookies = InspectorArray::create();

    ListHashSet<Cookie>::iterator end = cookiesList.end();
    ListHashSet<Cookie>::iterator it = cookiesList.begin();
    for (int i = 0; it != end; ++it, i++)
        cookies->pushObject(buildObjectForCookie(*it));

    return cookies;
}

PassRefPtr<InspectorObject> InspectorAgent::buildObjectForCookie(const Cookie& cookie)
{
    RefPtr<InspectorObject> value = InspectorObject::create();
    value->setString("name", cookie.name);
    value->setString("value", cookie.value);
    value->setString("domain", cookie.domain);
    value->setString("path", cookie.path);
    value->setNumber("expires", cookie.expires);
    value->setNumber("size", (cookie.name.length() + cookie.value.length()));
    value->setBoolean("httpOnly", cookie.httpOnly);
    value->setBoolean("secure", cookie.secure);
    value->setBoolean("session", cookie.session);
    return value;
}

void InspectorAgent::deleteCookie(ErrorString*, const String& cookieName, const String& domain)
{
    for (Frame* frame = m_inspectedPage->mainFrame(); frame; frame = frame->tree()->traverseNext(m_inspectedPage->mainFrame())) {
        Document* document = frame->document();
        if (document->url().host() != domain)
            continue;
        const CachedResourceLoader::DocumentResourceMap& allResources = document->cachedResourceLoader()->allCachedResources();
        CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
        for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it)
            WebCore::deleteCookie(document, KURL(ParsedURLString, it->second->url()), cookieName);
    }
}

#if ENABLE(WEB_SOCKETS)
void InspectorAgent::didCreateWebSocket(unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
    if (!enabled())
        return;
    ASSERT(m_inspectedPage);

    if (m_resourceAgent)
        m_resourceAgent->didCreateWebSocket(identifier, requestURL);
    UNUSED_PARAM(documentURL);
}

void InspectorAgent::willSendWebSocketHandshakeRequest(unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    if (m_resourceAgent)
        m_resourceAgent->willSendWebSocketHandshakeRequest(identifier, request);
}

void InspectorAgent::didReceiveWebSocketHandshakeResponse(unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    if (m_resourceAgent)
        m_resourceAgent->didReceiveWebSocketHandshakeResponse(identifier, response);
}

void InspectorAgent::didCloseWebSocket(unsigned long identifier)
{
    if (m_resourceAgent)
        m_resourceAgent->didCloseWebSocket(identifier);
}
#endif // ENABLE(WEB_SOCKETS)

#if ENABLE(JAVASCRIPT_DEBUGGER)
bool InspectorAgent::isRecordingUserInitiatedProfile() const
{
    return m_profilerAgent->isRecordingUserInitiatedProfile();
}

void InspectorAgent::startUserInitiatedProfiling()
{
    if (!enabled())
        return;
    m_profilerAgent->startUserInitiatedProfiling();
    m_state->setBoolean(InspectorAgentState::userInitiatedProfiling, true);
}

void InspectorAgent::stopUserInitiatedProfiling()
{
    m_profilerAgent->stopUserInitiatedProfiling();
    m_state->setBoolean(InspectorAgentState::userInitiatedProfiling, false);
    showPanel(profilesPanelName);
}

bool InspectorAgent::profilerEnabled() const
{
    return enabled() && m_profilerAgent->enabled();
}

void InspectorAgent::enableProfiler(ErrorString*)
{
    if (profilerEnabled())
        return;
    m_state->setBoolean(InspectorAgentState::profilerEnabled, true);
    m_profilerAgent->enable(false);
}

void InspectorAgent::disableProfiler(ErrorString*)
{
    m_state->setBoolean(InspectorAgentState::profilerEnabled, false);
    m_profilerAgent->disable();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorAgent::showScriptsPanel()
{
    showPanel(scriptsPanelName);
}
#endif

void InspectorAgent::evaluateForTestInFrontend(long callId, const String& script)
{
    if (m_frontend)
        m_frontend->evaluateForTestInFrontend(callId, script);
    else
        m_pendingEvaluateTestCommands.append(pair<long, String>(callId, script));
}

void InspectorAgent::didEvaluateForTestInFrontend(ErrorString*, long callId, const String& jsonResult)
{
    ScriptState* scriptState = scriptStateFromPage(debuggerWorld(), m_inspectedPage);
    ScriptObject window;
    ScriptGlobalObject::get(scriptState, "window", window);
    ScriptFunctionCall function(window, "didEvaluateForTestInFrontend");
    function.appendArgument(callId);
    function.appendArgument(jsonResult);
    function.call();
}

static Path quadToPath(const FloatQuad& quad)
{
    Path quadPath;
    quadPath.moveTo(quad.p1());
    quadPath.addLineTo(quad.p2());
    quadPath.addLineTo(quad.p3());
    quadPath.addLineTo(quad.p4());
    quadPath.closeSubpath();
    return quadPath;
}

static void drawOutlinedQuad(GraphicsContext& context, const FloatQuad& quad, const Color& fillColor)
{
    static const int outlineThickness = 2;
    static const Color outlineColor(62, 86, 180, 228);

    Path quadPath = quadToPath(quad);

    // Clip out the quad, then draw with a 2px stroke to get a pixel
    // of outline (because inflating a quad is hard)
    {
        context.save();
        context.clipOut(quadPath);

        context.setStrokeThickness(outlineThickness);
        context.setStrokeColor(outlineColor, ColorSpaceDeviceRGB);
        context.strokePath(quadPath);

        context.restore();
    }

    // Now do the fill
    context.setFillColor(fillColor, ColorSpaceDeviceRGB);
    context.fillPath(quadPath);
}

static void drawOutlinedQuadWithClip(GraphicsContext& context, const FloatQuad& quad, const FloatQuad& clipQuad, const Color& fillColor)
{
    context.save();
    Path clipQuadPath = quadToPath(clipQuad);
    context.clipOut(clipQuadPath);
    drawOutlinedQuad(context, quad, fillColor);
    context.restore();
}

static void drawHighlightForBox(GraphicsContext& context, const FloatQuad& contentQuad, const FloatQuad& paddingQuad, const FloatQuad& borderQuad, const FloatQuad& marginQuad)
{
    static const Color contentBoxColor(125, 173, 217, 128);
    static const Color paddingBoxColor(125, 173, 217, 160);
    static const Color borderBoxColor(125, 173, 217, 192);
    static const Color marginBoxColor(125, 173, 217, 228);

    if (marginQuad != borderQuad)
        drawOutlinedQuadWithClip(context, marginQuad, borderQuad, marginBoxColor);
    if (borderQuad != paddingQuad)
        drawOutlinedQuadWithClip(context, borderQuad, paddingQuad, borderBoxColor);
    if (paddingQuad != contentQuad)
        drawOutlinedQuadWithClip(context, paddingQuad, contentQuad, paddingBoxColor);

    drawOutlinedQuad(context, contentQuad, contentBoxColor);
}

static void drawHighlightForLineBoxesOrSVGRenderer(GraphicsContext& context, const Vector<FloatQuad>& lineBoxQuads)
{
    static const Color lineBoxColor(125, 173, 217, 128);

    for (size_t i = 0; i < lineBoxQuads.size(); ++i)
        drawOutlinedQuad(context, lineBoxQuads[i], lineBoxColor);
}

static inline void convertFromFrameToMainFrame(Frame* frame, IntRect& rect)
{
    rect = frame->page()->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(rect));
}

static inline IntSize frameToMainFrameOffset(Frame* frame)
{
    IntPoint mainFramePoint = frame->page()->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(IntPoint()));
    return mainFramePoint - IntPoint();
}

void InspectorAgent::drawNodeHighlight(GraphicsContext& context) const
{
    if (!m_highlightedNode)
        return;

    RenderObject* renderer = m_highlightedNode->renderer();
    Frame* containingFrame = m_highlightedNode->document()->frame();
    if (!renderer || !containingFrame)
        return;

    IntSize mainFrameOffset = frameToMainFrameOffset(containingFrame);
    IntRect boundingBox = renderer->absoluteBoundingBoxRect(true);
    boundingBox.move(mainFrameOffset);

    IntRect titleAnchorBox = boundingBox;

    ASSERT(m_inspectedPage);

    FrameView* view = m_inspectedPage->mainFrame()->view();
    FloatRect overlayRect = view->visibleContentRect();
    if (!overlayRect.contains(boundingBox) && !boundingBox.contains(enclosingIntRect(overlayRect)))
        overlayRect = view->visibleContentRect();
    context.translate(-overlayRect.x(), -overlayRect.y());

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
#if ENABLE(SVG)
    bool isSVGRenderer = renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot();
#else
    bool isSVGRenderer = false;
#endif

    if (renderer->isBox() && !isSVGRenderer) {
        RenderBox* renderBox = toRenderBox(renderer);

        IntRect contentBox = renderBox->contentBoxRect();

        IntRect paddingBox(contentBox.x() - renderBox->paddingLeft(), contentBox.y() - renderBox->paddingTop(),
                           contentBox.width() + renderBox->paddingLeft() + renderBox->paddingRight(), contentBox.height() + renderBox->paddingTop() + renderBox->paddingBottom());
        IntRect borderBox(paddingBox.x() - renderBox->borderLeft(), paddingBox.y() - renderBox->borderTop(),
                          paddingBox.width() + renderBox->borderLeft() + renderBox->borderRight(), paddingBox.height() + renderBox->borderTop() + renderBox->borderBottom());
        IntRect marginBox(borderBox.x() - renderBox->marginLeft(), borderBox.y() - renderBox->marginTop(),
                          borderBox.width() + renderBox->marginLeft() + renderBox->marginRight(), borderBox.height() + renderBox->marginTop() + renderBox->marginBottom());


        FloatQuad absContentQuad = renderBox->localToAbsoluteQuad(FloatRect(contentBox));
        FloatQuad absPaddingQuad = renderBox->localToAbsoluteQuad(FloatRect(paddingBox));
        FloatQuad absBorderQuad = renderBox->localToAbsoluteQuad(FloatRect(borderBox));
        FloatQuad absMarginQuad = renderBox->localToAbsoluteQuad(FloatRect(marginBox));

        absContentQuad.move(mainFrameOffset);
        absPaddingQuad.move(mainFrameOffset);
        absBorderQuad.move(mainFrameOffset);
        absMarginQuad.move(mainFrameOffset);

        titleAnchorBox = absMarginQuad.enclosingBoundingBox();

        drawHighlightForBox(context, absContentQuad, absPaddingQuad, absBorderQuad, absMarginQuad);
    } else if (renderer->isRenderInline() || isSVGRenderer) {
        // FIXME: We should show margins/padding/border for inlines.
        Vector<FloatQuad> lineBoxQuads;
        renderer->absoluteQuads(lineBoxQuads);
        for (unsigned i = 0; i < lineBoxQuads.size(); ++i)
            lineBoxQuads[i] += mainFrameOffset;

        drawHighlightForLineBoxesOrSVGRenderer(context, lineBoxQuads);
    }

    // Draw node title if necessary.

    if (!m_highlightedNode->isElementNode())
        return;

    WebCore::Settings* settings = containingFrame->settings();
    drawElementTitle(context, boundingBox, titleAnchorBox, overlayRect, settings);
}

void InspectorAgent::drawElementTitle(GraphicsContext& context, const IntRect& boundingBox, const IntRect& anchorBox, const FloatRect& overlayRect, WebCore::Settings* settings) const
{
    static const int rectInflatePx = 4;
    static const int fontHeightPx = 12;
    static const int borderWidthPx = 1;
    static const Color tooltipBackgroundColor(255, 255, 194, 255);
    static const Color tooltipBorderColor(Color::black);
    static const Color tooltipFontColor(Color::black);

    Element* element = static_cast<Element*>(m_highlightedNode.get());
    bool isXHTML = element->document()->isXHTMLDocument();
    String nodeTitle = isXHTML ? element->nodeName() : element->nodeName().lower();
    const AtomicString& idValue = element->getIdAttribute();
    if (!idValue.isNull() && !idValue.isEmpty()) {
        nodeTitle += "#";
        nodeTitle += idValue;
    }
    if (element->hasClass() && element->isStyledElement()) {
        const SpaceSplitString& classNamesString = static_cast<StyledElement*>(element)->classNames();
        size_t classNameCount = classNamesString.size();
        if (classNameCount) {
            HashSet<AtomicString> usedClassNames;
            for (size_t i = 0; i < classNameCount; ++i) {
                const AtomicString& className = classNamesString[i];
                if (usedClassNames.contains(className))
                    continue;
                usedClassNames.add(className);
                nodeTitle += ".";
                nodeTitle += className;
            }
        }
    }

    nodeTitle += " [";
    nodeTitle += String::number(boundingBox.width());
    nodeTitle.append(static_cast<UChar>(0x00D7)); // &times;
    nodeTitle += String::number(boundingBox.height());
    nodeTitle += "]";

    FontDescription desc;
    FontFamily family;
    family.setFamily(settings->fixedFontFamily());
    desc.setFamily(family);
    desc.setComputedSize(fontHeightPx);
    Font font = Font(desc, 0, 0);
    font.update(0);

    TextRun nodeTitleRun(nodeTitle);
    IntPoint titleBasePoint = IntPoint(anchorBox.x(), anchorBox.maxY() - 1);
    titleBasePoint.move(rectInflatePx, rectInflatePx);
    IntRect titleRect = enclosingIntRect(font.selectionRectForText(nodeTitleRun, titleBasePoint, fontHeightPx));
    titleRect.inflate(rectInflatePx);

    // The initial offsets needed to compensate for a 1px-thick border stroke (which is not a part of the rectangle).
    int dx = -borderWidthPx;
    int dy = borderWidthPx;

    // If the tip sticks beyond the right of overlayRect, right-align the tip with the said boundary.
    if (titleRect.maxX() > overlayRect.maxX())
        dx = overlayRect.maxX() - titleRect.maxX();

    // If the tip sticks beyond the left of overlayRect, left-align the tip with the said boundary.
    if (titleRect.x() + dx < overlayRect.x())
        dx = overlayRect.x() - titleRect.x() - borderWidthPx;

    // If the tip sticks beyond the bottom of overlayRect, show the tip at top of bounding box.
    if (titleRect.maxY() > overlayRect.maxY()) {
        dy = anchorBox.y() - titleRect.maxY() - borderWidthPx;
        // If the tip still sticks beyond the bottom of overlayRect, bottom-align the tip with the said boundary.
        if (titleRect.maxY() + dy > overlayRect.maxY())
            dy = overlayRect.maxY() - titleRect.maxY();
    }

    // If the tip sticks beyond the top of overlayRect, show the tip at top of overlayRect.
    if (titleRect.y() + dy < overlayRect.y())
        dy = overlayRect.y() - titleRect.y() + borderWidthPx;

    titleRect.move(dx, dy);
    context.setStrokeColor(tooltipBorderColor, ColorSpaceDeviceRGB);
    context.setStrokeThickness(borderWidthPx);
    context.setFillColor(tooltipBackgroundColor, ColorSpaceDeviceRGB);
    context.drawRect(titleRect);
    context.setFillColor(tooltipFontColor, ColorSpaceDeviceRGB);
    context.drawText(font, nodeTitleRun, IntPoint(titleRect.x() + rectInflatePx, titleRect.y() + font.fontMetrics().height()));
}

void InspectorAgent::openInInspectedWindow(ErrorString*, const String& url)
{
    Frame* mainFrame = m_inspectedPage->mainFrame();

    FrameLoadRequest request(mainFrame->document()->securityOrigin(), ResourceRequest(), "_blank");

    bool created;
    WindowFeatures windowFeatures;
    Frame* newFrame = WebCore::createWindow(mainFrame, mainFrame, request, windowFeatures, created);
    if (!newFrame)
        return;

    UserGestureIndicator indicator(DefinitelyProcessingUserGesture);
    newFrame->loader()->setOpener(mainFrame);
    newFrame->page()->setOpenedByDOM();
    newFrame->loader()->changeLocation(mainFrame->document()->securityOrigin(), newFrame->loader()->completeURL(url), "", false, false);
}

void InspectorAgent::addScriptToEvaluateOnLoad(ErrorString*, const String& source)
{
    m_scriptsToEvaluateOnLoad.append(source);
}

void InspectorAgent::removeAllScriptsToEvaluateOnLoad(ErrorString*)
{
    m_scriptsToEvaluateOnLoad.clear();
}

void InspectorAgent::setInspectorExtensionAPI(const String& source)
{
    m_inspectorExtensionAPI = source;
}

KURL InspectorAgent::inspectedURL() const
{
    return m_inspectedPage->mainFrame()->document()->url();
}

KURL InspectorAgent::inspectedURLWithoutFragment() const
{
    KURL url = inspectedURL();
    url.removeFragmentIdentifier();
    return url;
}

void InspectorAgent::reloadPage(ErrorString*, bool ignoreCache)
{
    m_inspectedPage->mainFrame()->loader()->reload(ignoreCache);
}

bool InspectorAgent::enabled() const
{
    if (!m_inspectedPage)
        return false;
    return m_inspectedPage->settings()->developerExtrasEnabled();
}

void InspectorAgent::showConsole()
{
    showPanel(consolePanelName);
}

void InspectorAgent::showPanel(const String& panel)
{
    if (!m_frontend) {
        m_showPanelAfterVisible = panel;
        return;
    }
    m_frontend->showPanel(panel);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
