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
#include "InspectorController.h"

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
#include "InspectorBackendDispatcher.h"
#include "InspectorBrowserDebuggerAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorClient.h"
#include "InspectorConsoleAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorDatabaseResource.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorFrontend.h"
#include "InspectorFrontendClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorRuntimeAgent.h"
#include "InspectorSettings.h"
#include "InspectorState.h"
#include "InspectorTimelineAgent.h"
#include "InspectorValues.h"
#include "InspectorWorkerResource.h"
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
#include <wtf/text/StringConcatenate.h>
#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "InspectorDatabaseAgent.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "InspectorDOMStorageAgent.h"
#include "Storage.h"
#include "StorageArea.h"
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "InspectorApplicationCacheAgent.h"
#endif

#if ENABLE(FILE_SYSTEM)
#include "InspectorFileSystemAgent.h"
#endif

using namespace std;

namespace WebCore {

const char* const InspectorController::ElementsPanel = "elements";
const char* const InspectorController::ConsolePanel = "console";
const char* const InspectorController::ScriptsPanel = "scripts";
const char* const InspectorController::ProfilesPanel = "profiles";

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_openingFrontend(false)
    , m_cssAgent(new InspectorCSSAgent())
    , m_state(new InspectorState(client))
    , m_inspectorBackendDispatcher(new InspectorBackendDispatcher(this))
    , m_injectedScriptHost(InjectedScriptHost::create(this))
    , m_consoleAgent(new InspectorConsoleAgent(this))
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_attachDebuggerWhenShown(false)
    , m_profilerAgent(InspectorProfilerAgent::create(this))
#endif
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

InspectorController::~InspectorController()
{
    // These should have been cleared in inspectedPageDestroyed().
    ASSERT(!m_client);
    ASSERT(!m_inspectedPage);
    ASSERT(!m_highlightedNode);
}

void InspectorController::inspectedPageDestroyed()
{
    if (m_frontend)
        m_frontend->disconnectFromBackend();

    hideHighlight();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent.clear();
    m_browserDebuggerAgent.clear();
#endif

    ASSERT(m_inspectedPage);
    m_inspectedPage = 0;

    releaseFrontendLifetimeAgents();
    m_injectedScriptHost->disconnectController();

    m_client->inspectorDestroyed();
    m_client = 0;
}

bool InspectorController::enabled() const
{
    if (!m_inspectedPage)
        return false;
    return m_inspectedPage->settings()->developerExtrasEnabled();
}

bool InspectorController::inspectorStartsAttached()
{
    return m_settings->getBoolean(InspectorSettings::InspectorStartsAttached);
}

void InspectorController::setInspectorStartsAttached(bool attached)
{
    m_settings->setBoolean(InspectorSettings::InspectorStartsAttached, attached);
}

void InspectorController::setInspectorAttachedHeight(long height)
{
    m_settings->setLong(InspectorSettings::InspectorAttachedHeight, height);
}

long InspectorController::inspectorAttachedHeight() const
{
    return m_settings->getLong(InspectorSettings::InspectorAttachedHeight);
}

bool InspectorController::searchingForNodeInPage() const
{
    return m_state->getBoolean(InspectorState::searchingForNode);
}

void InspectorController::restoreInspectorStateFromCookie(const String& inspectorStateCookie)
{
    m_state->restoreFromInspectorCookie(inspectorStateCookie);

    if (!m_frontend) {
        connectFrontend();
        m_frontend->frontendReused();
        m_frontend->inspectedURLChanged(inspectedURL().string());
        m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());
        pushDataCollectedOffline();
    }

    m_resourceAgent = InspectorResourceAgent::restore(m_inspectedPage, m_state.get(), m_frontend.get());

    if (m_state->getBoolean(InspectorState::timelineProfilerEnabled))
        startTimelineProfiler();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    restoreDebugger();
    restoreProfiler(ProfilerRestoreResetAgent);
    if (m_state->getBoolean(InspectorState::userInitiatedProfiling))
        startUserInitiatedProfiling();
#endif
}

void InspectorController::inspect(Node* node)
{
    if (!enabled())
        return;

    show();

    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!m_frontend)
        return;

    focusNode();
}

void InspectorController::focusNode()
{
    if (!enabled())
        return;

    ASSERT(m_frontend);
    ASSERT(m_nodeToFocus);

    long id = m_domAgent->pushNodePathToFrontend(m_nodeToFocus.get());
    m_frontend->updateFocusedNode(id);
    m_nodeToFocus = 0;
}

void InspectorController::highlight(Node* node)
{
    if (!enabled())
        return;
    ASSERT_ARG(node, node);
    m_highlightedNode = node;
    m_client->highlight(node);
}

void InspectorController::highlightDOMNode(long nodeId)
{
    Node* node = 0;
    if (m_domAgent && (node = m_domAgent->nodeForId(nodeId)))
        highlight(node);
}

void InspectorController::highlightFrame(unsigned long frameId)
{
    Frame* mainFrame = m_inspectedPage->mainFrame();
    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext(mainFrame)) {
        if (reinterpret_cast<uintptr_t>(frame) == frameId && frame->ownerElement()) {
            highlight(frame->ownerElement());
            return;
        }
    }
}

void InspectorController::hideHighlight()
{
    if (!enabled())
        return;
    m_highlightedNode = 0;
    m_client->hideHighlight();
}

void InspectorController::mouseDidMoveOverElement(const HitTestResult& result, unsigned)
{
    if (!enabled() || !searchingForNodeInPage())
        return;

    Node* node = result.innerNode();
    while (node && node->nodeType() == Node::TEXT_NODE)
        node = node->parentNode();
    if (node)
        highlight(node);
}

bool InspectorController::handleMousePress()
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

void InspectorController::setInspectorFrontendClient(PassOwnPtr<InspectorFrontendClient> client)
{
    ASSERT(!m_inspectorFrontendClient);
    m_inspectorFrontendClient = client;
}

void InspectorController::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    // If the page is supposed to serve as InspectorFrontend notify inspetor frontend
    // client that it's cleared so that the client can expose inspector bindings.
    if (m_inspectorFrontendClient && frame == m_inspectedPage->mainFrame())
        m_inspectorFrontendClient->windowObjectCleared();

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

void InspectorController::setSearchingForNode(bool enabled)
{
    if (searchingForNodeInPage() == enabled)
        return;
    m_state->setBoolean(InspectorState::searchingForNode, enabled);
    if (!enabled)
        hideHighlight();
}

void InspectorController::setSearchingForNode(bool enabled, bool* newState)
{
    *newState = enabled;
    setSearchingForNode(enabled);
}

void InspectorController::connectFrontend()
{
    m_openingFrontend = false;
    releaseFrontendLifetimeAgents();
    m_frontend = new InspectorFrontend(m_client);
    m_domAgent = InspectorDOMAgent::create(m_injectedScriptHost.get(), m_frontend.get());
    m_runtimeAgent = InspectorRuntimeAgent::create(m_injectedScriptHost.get());
    m_cssAgent->setDOMAgent(m_domAgent.get());

#if ENABLE(DATABASE)
    m_databaseAgent = InspectorDatabaseAgent::create(&m_databaseResources, m_frontend.get());
#endif

#if ENABLE(DOM_STORAGE)
    m_domStorageAgent = InspectorDOMStorageAgent::create(&m_domStorageResources, m_frontend.get());
#endif

    if (m_timelineAgent)
        m_timelineAgent->resetFrontendProxyObject(m_frontend.get());

    m_consoleAgent->setFrontend(m_frontend.get());

    // Initialize Web Inspector title.
    m_frontend->inspectedURLChanged(inspectedURL().string());

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    m_applicationCacheAgent = new InspectorApplicationCacheAgent(this, m_frontend.get());
#endif

#if ENABLE(FILE_SYSTEM)
    m_fileSystemAgent = InspectorFileSystemAgent::create(this, m_frontend.get());
#endif
    
    if (!InspectorInstrumentation::hasFrontends())
        ScriptController::setCaptureCallStackForUncaughtExceptions(true);
    InspectorInstrumentation::frontendCreated();
}

void InspectorController::show()
{
    if (!enabled())
        return;

    if (m_openingFrontend)
        return;

    if (m_frontend)
        m_frontend->bringToFront();
    else {
        m_openingFrontend = true;
        m_client->openInspectorFrontend(this);
    }
}

void InspectorController::showPanel(const String& panel)
{
    if (!enabled())
        return;

    show();

    if (!m_frontend) {
        m_showAfterVisible = panel;
        return;
    }
    m_frontend->showPanel(panel);
}

void InspectorController::close()
{
    if (!m_frontend)
        return;
    m_frontend->disconnectFromBackend();
    disconnectFrontend();
}

void InspectorController::disconnectFrontend()
{
    if (!m_frontend)
        return;

    m_frontend.clear();

    InspectorInstrumentation::frontendDeleted();
    if (!InspectorInstrumentation::hasFrontends())
        ScriptController::setCaptureCallStackForUncaughtExceptions(false);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    // If the window is being closed with the debugger enabled,
    // remember this state to re-enable debugger on the next window
    // opening.
    bool debuggerWasEnabled = debuggerEnabled();
    disableDebugger();
    m_attachDebuggerWhenShown = debuggerWasEnabled;
#endif
    setSearchingForNode(false);
    unbindAllResources();
    stopTimelineProfiler();

    hideHighlight();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_profilerAgent->setFrontend(0);
    m_profilerAgent->stopUserInitiatedProfiling(true);
#endif
    m_consoleAgent->setFrontend(0);

    releaseFrontendLifetimeAgents();
    m_timelineAgent.clear();
    m_extraHeaders.clear();
}

InspectorResourceAgent* InspectorController::resourceAgent()
{
    if (!m_resourceAgent && m_frontend)
        m_resourceAgent = InspectorResourceAgent::create(m_inspectedPage, m_state.get(), m_frontend.get());
    return m_resourceAgent.get();
}

void InspectorController::releaseFrontendLifetimeAgents()
{
    m_resourceAgent.clear();
    m_runtimeAgent.clear();

    // This should be invoked prior to m_domAgent destruction.
    m_cssAgent->setDOMAgent(0);

    // m_domAgent is RefPtr. Remove DOM listeners first to ensure that there are
    // no references to the DOM agent from the DOM tree.
    if (m_domAgent)
        m_domAgent->reset();
    m_domAgent.clear();

#if ENABLE(DATABASE)
    if (m_databaseAgent)
        m_databaseAgent->clearFrontend();
    m_databaseAgent.clear();
#endif

#if ENABLE(DOM_STORAGE)
    m_domStorageAgent.clear();
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    m_applicationCacheAgent.clear();
#endif

#if ENABLE(FILE_SYSTEM)
    if (m_fileSystemAgent)
        m_fileSystemAgent->stop(); 
        m_fileSystemAgent.clear();
#endif
}

void InspectorController::populateScriptObjects()
{
    ASSERT(m_frontend);
    if (!m_frontend)
        return;

    if (!m_showAfterVisible.isEmpty()) {
        showPanel(m_showAfterVisible);
        m_showAfterVisible = "";
    }

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_profilerAgent->enabled())
        m_frontend->profilerWasEnabled();
#endif

    pushDataCollectedOffline();

    if (m_nodeToFocus)
        focusNode();

    // Dispatch pending frontend commands
    for (Vector<pair<long, String> >::iterator it = m_pendingEvaluateTestCommands.begin(); it != m_pendingEvaluateTestCommands.end(); ++it)
        m_frontend->evaluateForTestInFrontend((*it).first, (*it).second);
    m_pendingEvaluateTestCommands.clear();

    restoreDebugger();
    restoreProfiler(ProfilerRestoreNoAction);
}

void InspectorController::pushDataCollectedOffline()
{
    m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());

#if ENABLE(DATABASE)
    DatabaseResourcesMap::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesMap::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it)
        it->second->bind(m_frontend.get());
#endif
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesMap::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesMap::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        it->second->bind(m_frontend.get());
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(WORKERS)
    WorkersMap::iterator workersEnd = m_workers.end();
    for (WorkersMap::iterator it = m_workers.begin(); it != workersEnd; ++it) {
        InspectorWorkerResource* worker = it->second.get();
        m_frontend->didCreateWorker(worker->id(), worker->url(), worker->isSharedWorker());
    }
#endif
}

void InspectorController::restoreDebugger()
{
    ASSERT(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent::isDebuggerAlwaysEnabled() || m_attachDebuggerWhenShown || m_settings->getBoolean(InspectorSettings::DebuggerAlwaysEnabled)) {
        enableDebugger(false);
        m_attachDebuggerWhenShown = false;
    }
#endif
}

void InspectorController::restoreProfiler(ProfilerRestoreAction action)
{
    ASSERT(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_profilerAgent->setFrontend(m_frontend.get());
    if (!ScriptProfiler::isProfilerAlwaysEnabled() && m_settings->getBoolean(InspectorSettings::ProfilerAlwaysEnabled))
        enableProfiler();
    if (action == ProfilerRestoreResetAgent)
        m_profilerAgent->resetFrontendProfiles();
#endif
}

void InspectorController::unbindAllResources()
{
#if ENABLE(DATABASE)
    DatabaseResourcesMap::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesMap::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it)
        it->second->unbind();
#endif
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesMap::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesMap::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        it->second->unbind();
#endif
    if (m_timelineAgent)
        m_timelineAgent->reset();
}

void InspectorController::didCommitLoad(DocumentLoader* loader)
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

#if ENABLE(JAVASCRIPT_DEBUGGER)
        if (m_debuggerAgent) {
            m_debuggerAgent->clearForPageNavigation();
            if (m_browserDebuggerAgent)
                m_browserDebuggerAgent->inspectedURLChanged(inspectedURL());
        }
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
        m_profilerAgent->stopUserInitiatedProfiling(true);
        m_profilerAgent->resetState();
#endif

        // unbindAllResources should be called before database and DOM storage
        // resources are cleared so that it has a chance to unbind them.
        unbindAllResources();

        if (m_frontend) {
            m_frontend->reset();
            m_domAgent->reset();
            m_cssAgent->reset();
        }
#if ENABLE(WORKERS)
        m_workers.clear();
#endif
#if ENABLE(DATABASE)
        m_databaseResources.clear();
#endif
#if ENABLE(DOM_STORAGE)
        m_domStorageResources.clear();
#endif

        if (m_frontend)
            m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());
    }
}

void InspectorController::mainResourceFiredDOMContentEvent(DocumentLoader* loader, const KURL& url)
{
    if (!enabled() || !isMainResourceLoader(loader, url))
        return;

    if (m_timelineAgent)
        m_timelineAgent->didMarkDOMContentEvent();
    if (m_frontend)
        m_frontend->domContentEventFired(currentTime());
}

void InspectorController::mainResourceFiredLoadEvent(DocumentLoader* loader, const KURL& url)
{
    if (!enabled() || !isMainResourceLoader(loader, url))
        return;

    if (m_timelineAgent)
        m_timelineAgent->didMarkLoadEvent();
    if (m_frontend)
        m_frontend->loadEventFired(currentTime());
}

bool InspectorController::isMainResourceLoader(DocumentLoader* loader, const KURL& requestUrl)
{
    return loader->frame() == m_inspectedPage->mainFrame() && requestUrl == loader->requestURL();
}

void InspectorController::willSendRequest(ResourceRequest& request)
{
    if (!enabled())
        return;

    if (m_frontend) {
        // Only enable load timing and raw headers if front-end is attached, as otherwise we may produce overhead.
        request.setReportLoadTiming(true);
        request.setReportRawHeaders(true);

        if (m_extraHeaders) {
            HTTPHeaderMap::const_iterator end = m_extraHeaders->end();
            for (HTTPHeaderMap::const_iterator it = m_extraHeaders->begin(); it != end; ++it)
                request.setHTTPHeaderField(it->first, it->second);
        }
    }
}

void InspectorController::ensureSettingsLoaded()
{
    if (m_settings)
        return;
    m_settings = new InspectorSettings(m_client);
    m_state->setBoolean(InspectorState::monitoringXHR, m_settings->getBoolean(InspectorSettings::MonitoringXHREnabled));
}

void InspectorController::startTimelineProfiler()
{
    if (!enabled())
        return;

    if (m_timelineAgent)
        return;

    m_timelineAgent = new InspectorTimelineAgent(m_frontend.get());
    if (m_frontend)
        m_frontend->timelineProfilerWasStarted();

    m_state->setBoolean(InspectorState::timelineProfilerEnabled, true);
}

void InspectorController::stopTimelineProfiler()
{
    if (!enabled())
        return;

    if (!m_timelineAgent)
        return;

    m_timelineAgent = 0;
    if (m_frontend)
        m_frontend->timelineProfilerWasStopped();

    m_state->setBoolean(InspectorState::timelineProfilerEnabled, false);
}

#if ENABLE(WORKERS)
class PostWorkerNotificationToFrontendTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<PostWorkerNotificationToFrontendTask> create(PassRefPtr<InspectorWorkerResource> worker, InspectorController::WorkerAction action)
    {
        return new PostWorkerNotificationToFrontendTask(worker, action);
    }

private:
    PostWorkerNotificationToFrontendTask(PassRefPtr<InspectorWorkerResource> worker, InspectorController::WorkerAction action)
        : m_worker(worker)
        , m_action(action)
    {
    }

    virtual void performTask(ScriptExecutionContext* scriptContext)
    {
        if (scriptContext->isDocument()) {
            if (InspectorController* inspector = static_cast<Document*>(scriptContext)->page()->inspectorController())
                inspector->postWorkerNotificationToFrontend(*m_worker, m_action);
        }
    }

private:
    RefPtr<InspectorWorkerResource> m_worker;
    InspectorController::WorkerAction m_action;
};

void InspectorController::postWorkerNotificationToFrontend(const InspectorWorkerResource& worker, InspectorController::WorkerAction action)
{
    if (!m_frontend)
        return;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    switch (action) {
    case InspectorController::WorkerCreated:
        m_frontend->didCreateWorker(worker.id(), worker.url(), worker.isSharedWorker());
        break;
    case InspectorController::WorkerDestroyed:
        m_frontend->didDestroyWorker(worker.id());
        break;
    }
#endif
}

void InspectorController::didCreateWorker(intptr_t id, const String& url, bool isSharedWorker)
{
    if (!enabled())
        return;

    RefPtr<InspectorWorkerResource> workerResource(InspectorWorkerResource::create(id, url, isSharedWorker));
    m_workers.set(id, workerResource);
    if (m_inspectedPage && m_frontend)
        m_inspectedPage->mainFrame()->document()->postTask(PostWorkerNotificationToFrontendTask::create(workerResource, InspectorController::WorkerCreated));
}

void InspectorController::didDestroyWorker(intptr_t id)
{
    if (!enabled())
        return;

    WorkersMap::iterator workerResource = m_workers.find(id);
    if (workerResource == m_workers.end())
        return;
    if (m_inspectedPage && m_frontend)
        m_inspectedPage->mainFrame()->document()->postTask(PostWorkerNotificationToFrontendTask::create(workerResource->second, InspectorController::WorkerDestroyed));
    m_workers.remove(workerResource);
}
#endif // ENABLE(WORKERS)

#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(PassRefPtr<Database> database, const String& domain, const String& name, const String& version)
{
    if (!enabled())
        return;

    RefPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);

    m_databaseResources.set(resource->id(), resource);

    // Resources are only bound while visible.
    if (m_frontend)
        resource->bind(m_frontend.get());
}
#endif

void InspectorController::getCookies(RefPtr<InspectorArray>* cookies, WTF::String* cookiesString)
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

PassRefPtr<InspectorArray> InspectorController::buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    RefPtr<InspectorArray> cookies = InspectorArray::create();

    ListHashSet<Cookie>::iterator end = cookiesList.end();
    ListHashSet<Cookie>::iterator it = cookiesList.begin();
    for (int i = 0; it != end; ++it, i++)
        cookies->pushObject(buildObjectForCookie(*it));

    return cookies;
}

PassRefPtr<InspectorObject> InspectorController::buildObjectForCookie(const Cookie& cookie)
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

void InspectorController::deleteCookie(const String& cookieName, const String& domain)
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

#if ENABLE(DOM_STORAGE)
void InspectorController::didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
    if (!enabled())
        return;

    DOMStorageResourcesMap::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesMap::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        if (it->second->isSameHostAndType(frame, isLocalStorage))
            return;

    RefPtr<Storage> domStorage = Storage::create(frame, storageArea);
    RefPtr<InspectorDOMStorageResource> resource = InspectorDOMStorageResource::create(domStorage.get(), isLocalStorage, frame);

    m_domStorageResources.set(resource->id(), resource);

    // Resources are only bound while visible.
    if (m_frontend)
        resource->bind(m_frontend.get());
}
#endif

#if ENABLE(WEB_SOCKETS)
void InspectorController::didCreateWebSocket(unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
    if (!enabled())
        return;
    ASSERT(m_inspectedPage);

    if (m_resourceAgent)
        m_resourceAgent->didCreateWebSocket(identifier, requestURL);
    UNUSED_PARAM(documentURL);
}

void InspectorController::willSendWebSocketHandshakeRequest(unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    if (m_resourceAgent)
        m_resourceAgent->willSendWebSocketHandshakeRequest(identifier, request);
}

void InspectorController::didReceiveWebSocketHandshakeResponse(unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    if (m_resourceAgent)
        m_resourceAgent->didReceiveWebSocketHandshakeResponse(identifier, response);
}

void InspectorController::didCloseWebSocket(unsigned long identifier)
{
    if (m_resourceAgent)
        m_resourceAgent->didCloseWebSocket(identifier);
}
#endif // ENABLE(WEB_SOCKETS)

#if ENABLE(JAVASCRIPT_DEBUGGER)
bool InspectorController::isRecordingUserInitiatedProfile() const
{
    return m_profilerAgent->isRecordingUserInitiatedProfile();
}

void InspectorController::startUserInitiatedProfiling()
{
    if (!enabled())
        return;
    m_profilerAgent->startUserInitiatedProfiling();
    m_state->setBoolean(InspectorState::userInitiatedProfiling, true);
}

void InspectorController::stopUserInitiatedProfiling()
{
    if (!enabled())
        return;
    m_profilerAgent->stopUserInitiatedProfiling();
    m_state->setBoolean(InspectorState::userInitiatedProfiling, false);
}

bool InspectorController::profilerEnabled() const
{
    return enabled() && m_profilerAgent->enabled();
}

void InspectorController::enableProfiler(bool always, bool skipRecompile)
{
    if (always)
        m_settings->setBoolean(InspectorSettings::ProfilerAlwaysEnabled, true);
    m_profilerAgent->enable(skipRecompile);
}

void InspectorController::disableProfiler(bool always)
{
    if (always)
        m_settings->setBoolean(InspectorSettings::ProfilerAlwaysEnabled, false);
    m_profilerAgent->disable();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::showAndEnableDebugger()
{
    if (!enabled())
        return;

    if (debuggerEnabled())
        return;

    if (!m_frontend) {
        m_attachDebuggerWhenShown = true;
        showPanel(ScriptsPanel);
    } else
        enableDebugger(false);
}

void InspectorController::enableDebugger(bool always)
{
    ASSERT(!debuggerEnabled());
    if (always)
        m_settings->setBoolean(InspectorSettings::DebuggerAlwaysEnabled, true);

    ASSERT(m_inspectedPage);

    m_debuggerAgent = InspectorDebuggerAgent::create(this, m_frontend.get());
    m_browserDebuggerAgent = InspectorBrowserDebuggerAgent::create(this);
    m_browserDebuggerAgent->inspectedURLChanged(inspectedURL());

    m_frontend->debuggerWasEnabled();
}

void InspectorController::disableDebugger(bool always)
{
    if (!enabled())
        return;

    if (always)
        m_settings->setBoolean(InspectorSettings::DebuggerAlwaysEnabled, false);

    ASSERT(m_inspectedPage);

    m_debuggerAgent.clear();
    m_browserDebuggerAgent.clear();

    m_attachDebuggerWhenShown = false;

    if (m_frontend)
        m_frontend->debuggerWasDisabled();
}

void InspectorController::resume()
{
    if (m_debuggerAgent)
        m_debuggerAgent->resume();
}

void InspectorController::setAllBrowserBreakpoints(PassRefPtr<InspectorObject> breakpoints)
{
    m_state->setObject(InspectorState::browserBreakpoints, breakpoints);
}
#endif

void InspectorController::evaluateForTestInFrontend(long callId, const String& script)
{
    if (m_frontend)
        m_frontend->evaluateForTestInFrontend(callId, script);
    else
        m_pendingEvaluateTestCommands.append(pair<long, String>(callId, script));
}

void InspectorController::didEvaluateForTestInFrontend(long callId, const String& jsonResult)
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

void InspectorController::drawNodeHighlight(GraphicsContext& context) const
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

    IntRect titleReferenceBox = boundingBox;

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

        titleReferenceBox = marginBox;
        titleReferenceBox.move(mainFrameOffset);
        titleReferenceBox.move(boundingBox.x(), boundingBox.y());

        FloatQuad absContentQuad = renderBox->localToAbsoluteQuad(FloatRect(contentBox));
        FloatQuad absPaddingQuad = renderBox->localToAbsoluteQuad(FloatRect(paddingBox));
        FloatQuad absBorderQuad = renderBox->localToAbsoluteQuad(FloatRect(borderBox));
        FloatQuad absMarginQuad = renderBox->localToAbsoluteQuad(FloatRect(marginBox));

        absContentQuad.move(mainFrameOffset);
        absPaddingQuad.move(mainFrameOffset);
        absBorderQuad.move(mainFrameOffset);
        absMarginQuad.move(mainFrameOffset);

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
    drawElementTitle(context, titleReferenceBox, overlayRect, settings);
}

void InspectorController::drawElementTitle(GraphicsContext& context, const IntRect& boundingBox, const FloatRect& overlayRect, WebCore::Settings* settings) const
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

    Element* highlightedElement = m_highlightedNode->isElementNode() ? static_cast<Element*>(m_highlightedNode.get()) : 0;
    nodeTitle += " [";
    nodeTitle += String::number(highlightedElement ? highlightedElement->offsetWidth() : boundingBox.width());
    nodeTitle.append(static_cast<UChar>(0x00D7)); // &times;
    nodeTitle += String::number(highlightedElement ? highlightedElement->offsetHeight() : boundingBox.height());
    nodeTitle += "]";

    FontDescription desc;
    FontFamily family;
    family.setFamily(settings->fixedFontFamily());
    desc.setFamily(family);
    desc.setComputedSize(fontHeightPx);
    Font font = Font(desc, 0, 0);
    font.update(0);

    TextRun nodeTitleRun(nodeTitle);
    IntPoint titleBasePoint = boundingBox.bottomLeft();
    titleBasePoint.move(rectInflatePx, rectInflatePx);
    IntRect titleRect = enclosingIntRect(font.selectionRectForText(nodeTitleRun, titleBasePoint, fontHeightPx));
    titleRect.inflate(rectInflatePx);

    // The initial offsets needed to compensate for a 1px-thick border stroke (which is not a part of the rectangle).
    int dx = -borderWidthPx;
    int dy = borderWidthPx;

    // If the tip sticks beyond the right of overlayRect, right-align the tip with the said boundary.
    if (titleRect.right() > overlayRect.right())
        dx = overlayRect.right() - titleRect.right();

    // If the tip sticks beyond the left of overlayRect, left-align the tip with the said boundary.
    if (titleRect.x() + dx < overlayRect.x())
        dx = overlayRect.x() - titleRect.x() - borderWidthPx;

    // If the tip sticks beyond the bottom of overlayRect, show the tip at top of bounding box.
    if (titleRect.bottom() > overlayRect.bottom()) {
        dy = boundingBox.y() - titleRect.bottom() - borderWidthPx;
        // If the tip still sticks beyond the bottom of overlayRect, bottom-align the tip with the said boundary.
        if (titleRect.bottom() + dy > overlayRect.bottom())
            dy = overlayRect.bottom() - titleRect.bottom();
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
    context.drawText(font, nodeTitleRun, IntPoint(titleRect.x() + rectInflatePx, titleRect.y() + font.height()));
}

void InspectorController::openInInspectedWindow(const String& url)
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

void InspectorController::addScriptToEvaluateOnLoad(const String& source)
{
    m_scriptsToEvaluateOnLoad.append(source);
}

void InspectorController::removeAllScriptsToEvaluateOnLoad()
{
    m_scriptsToEvaluateOnLoad.clear();
}

void InspectorController::setInspectorExtensionAPI(const String& source)
{
    m_inspectorExtensionAPI = source;
}

KURL InspectorController::inspectedURL() const
{
    return m_inspectedPage->mainFrame()->loader()->url();
}

void InspectorController::reloadPage()
{
    // FIXME: Why do we set the user gesture indicator here?
    UserGestureIndicator indicator(DefinitelyProcessingUserGesture);
    m_inspectedPage->mainFrame()->navigationScheduler()->scheduleRefresh();
}

void InspectorController::setExtraHeaders(PassRefPtr<InspectorObject> headers)
{
    m_extraHeaders = adoptPtr(new HTTPHeaderMap());
    InspectorObject::const_iterator end = headers->end();
    for (InspectorObject::const_iterator it = headers->begin(); it != end; ++it) {
        String value;
        if (!it->second->asString(&value))
            continue;
        m_extraHeaders->add(it->first, value);
    }
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
