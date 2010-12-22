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
#include "Console.h"
#include "ConsoleMessage.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMWindow.h"
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
#include "InspectorBackend.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorCSSAgent.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorDatabaseResource.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorFrontend.h"
#include "InspectorFrontendClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorState.h"
#include "InspectorStorageAgent.h"
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
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "InspectorApplicationCacheAgent.h"
#endif

#if ENABLE(FILE_SYSTEM)
#include "InspectorFileSystemAgent.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#include "StorageArea.h"
#endif

using namespace std;

namespace WebCore {

const char* const InspectorController::ElementsPanel = "elements";
const char* const InspectorController::ConsolePanel = "console";
const char* const InspectorController::ScriptsPanel = "scripts";
const char* const InspectorController::ProfilesPanel = "profiles";

const unsigned InspectorController::defaultAttachedHeight = 300;

static const unsigned maximumConsoleMessages = 1000;
static const unsigned expireConsoleMessagesStep = 100;

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_openingFrontend(false)
    , m_cssAgent(new InspectorCSSAgent())
    , m_mainResourceIdentifier(0)
    , m_expiredConsoleMessageCount(0)
    , m_previousMessage(0)
    , m_settingsLoaded(false)
    , m_inspectorBackend(InspectorBackend::create(this))
    , m_inspectorBackendDispatcher(new InspectorBackendDispatcher(this))
    , m_injectedScriptHost(InjectedScriptHost::create(this))
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_attachDebuggerWhenShown(false)
    , m_hasXHRBreakpointWithEmptyURL(false)
    , m_profilerAgent(InspectorProfilerAgent::create(this))
#endif
{
    m_state = new InspectorState(client);
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

InspectorController::~InspectorController()
{
    // These should have been cleared in inspectedPageDestroyed().
    ASSERT(!m_client);
    ASSERT(!m_inspectedPage);
    ASSERT(!m_highlightedNode);

    releaseFrontendLifetimeAgents();

    m_inspectorBackend->disconnectController();
    m_injectedScriptHost->disconnectController();
}

void InspectorController::inspectedPageDestroyed()
{
    if (m_frontend)
        m_frontend->disconnectFromBackend();

    hideHighlight();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent.clear();
#endif
    ASSERT(m_inspectedPage);
    m_inspectedPage = 0;

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
    return m_state->getBoolean(InspectorState::inspectorStartsAttached);
}

void InspectorController::setInspectorStartsAttached(bool attached)
{
    m_state->setBoolean(InspectorState::inspectorStartsAttached, attached);
}

void InspectorController::setInspectorAttachedHeight(long height)
{
    m_state->setLong(InspectorState::inspectorAttachedHeight, height);
}

int InspectorController::inspectorAttachedHeight() const
{
    return m_state->getBoolean(InspectorState::inspectorAttachedHeight);
}

bool InspectorController::searchingForNodeInPage() const
{
    return m_state->getBoolean(InspectorState::searchingForNode);
}

void InspectorController::getInspectorState(RefPtr<InspectorObject>* state)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_debuggerAgent)
        m_state->setLong(InspectorState::pauseOnExceptionsState, m_debuggerAgent->pauseOnExceptionsState());
#endif
    *state = m_state->generateStateObjectForFrontend();
}

void InspectorController::restoreInspectorStateFromCookie(const String& inspectorStateCookie)
{
    m_state->restoreFromInspectorCookie(inspectorStateCookie);
    if (m_state->getBoolean(InspectorState::timelineProfilerEnabled))
        startTimelineProfiler();
#if ENABLE(JAVASCRIPT_DEBUGGER)
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

void InspectorController::setConsoleMessagesEnabled(bool enabled, bool* newState)
{
    *newState = enabled;
    setConsoleMessagesEnabled(enabled);
}

void InspectorController::setConsoleMessagesEnabled(bool enabled)
{
    m_state->setBoolean(InspectorState::consoleMessagesEnabled, enabled);
    if (!enabled)
        return;

    if (m_expiredConsoleMessageCount)
        m_frontend->updateConsoleMessageExpiredCount(m_expiredConsoleMessageCount);
    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        m_consoleMessages[i]->addToFrontend(m_frontend.get(), m_injectedScriptHost.get());
}

void InspectorController::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    if (!enabled())
        return;

    addConsoleMessage(new ConsoleMessage(source, type, level, message, arguments, callStack));
}

void InspectorController::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    addConsoleMessage(new ConsoleMessage(source, type, level, message, lineNumber, sourceID));
}

void InspectorController::addConsoleMessage(PassOwnPtr<ConsoleMessage> consoleMessage)
{
    ASSERT(enabled());
    ASSERT_ARG(consoleMessage, consoleMessage);

    if (m_previousMessage && m_previousMessage->isEqual(consoleMessage.get())) {
        m_previousMessage->incrementCount();
        if (m_state->getBoolean(InspectorState::consoleMessagesEnabled) && m_frontend)
            m_previousMessage->updateRepeatCountInConsole(m_frontend.get());
    } else {
        m_previousMessage = consoleMessage.get();
        m_consoleMessages.append(consoleMessage);
        if (m_state->getBoolean(InspectorState::consoleMessagesEnabled) && m_frontend)
            m_previousMessage->addToFrontend(m_frontend.get(), m_injectedScriptHost.get());
    }

    if (!m_frontend && m_consoleMessages.size() >= maximumConsoleMessages) {
        m_expiredConsoleMessageCount += expireConsoleMessagesStep;
        m_consoleMessages.remove(0, expireConsoleMessagesStep);
    }
}

void InspectorController::clearConsoleMessages()
{
    m_consoleMessages.clear();
    m_expiredConsoleMessageCount = 0;
    m_previousMessage = 0;
    m_injectedScriptHost->releaseWrapperObjectGroup(0 /* release the group in all scripts */, "console");
    if (m_domAgent)
        m_domAgent->releaseDanglingNodes();
    if (m_frontend)
        m_frontend->consoleMessagesCleared();
}

void InspectorController::startGroup(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack, bool collapsed)
{
    addConsoleMessage(new ConsoleMessage(JSMessageSource, collapsed ? StartGroupCollapsedMessageType : StartGroupMessageType, LogMessageLevel, "", arguments, callStack));
}

void InspectorController::endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL)
{
    addConsoleMessage(new ConsoleMessage(source, EndGroupMessageType, LogMessageLevel, String(), lineNumber, sourceURL));
}

void InspectorController::markTimeline(const String& message)
{
    if (timelineAgent())
        timelineAgent()->didMarkTimeline(message);
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

void InspectorController::handleMousePress()
{
    if (!enabled())
        return;

    ASSERT(searchingForNodeInPage());
    if (!m_highlightedNode)
        return;

    RefPtr<Node> node = m_highlightedNode;
    setSearchingForNode(false);
    inspect(node.get());
}

void InspectorController::setInspectorFrontendClient(PassOwnPtr<InspectorFrontendClient> client)
{
    ASSERT(!m_inspectorFrontendClient);
    m_inspectorFrontendClient = client;
}

void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame)
{
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

void InspectorController::setMonitoringXHREnabled(bool enabled, bool* newState)
{
    *newState = enabled;
    m_state->setBoolean(InspectorState::monitoringXHR, enabled);
}

void InspectorController::connectFrontend()
{
    m_openingFrontend = false;
    releaseFrontendLifetimeAgents();
    m_frontend = new InspectorFrontend(m_client);
    m_domAgent = InspectorDOMAgent::create(m_frontend.get());
    m_resourceAgent = InspectorResourceAgent::create(m_inspectedPage, m_frontend.get());

    m_cssAgent->setDOMAgent(m_domAgent.get());

#if ENABLE(DATABASE)
    m_storageAgent = InspectorStorageAgent::create(m_frontend.get());
#endif

    if (m_timelineAgent)
        m_timelineAgent->resetFrontendProxyObject(m_frontend.get());

    // Initialize Web Inspector title.
    m_frontend->inspectedURLChanged(m_inspectedPage->mainFrame()->loader()->url().string());

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

void InspectorController::reuseFrontend()
{
    connectFrontend();
    restoreDebugger();
    restoreProfiler(ProfilerRestoreResetAgent);
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

    releaseFrontendLifetimeAgents();
    m_timelineAgent.clear();
    m_extraHeaders.clear();
}

void InspectorController::releaseFrontendLifetimeAgents()
{
    m_resourceAgent.clear();

    // This should be invoked prior to m_domAgent destruction.
    m_cssAgent->setDOMAgent(0);

    // m_domAgent is RefPtr. Remove DOM listeners first to ensure that there are
    // no references to the DOM agent from the DOM tree.
    if (m_domAgent)
        m_domAgent->reset();
    m_domAgent.clear();

#if ENABLE(DATABASE)
    if (m_storageAgent)
        m_storageAgent->clearFrontend();
    m_storageAgent.clear();
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

    m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());

    if (m_nodeToFocus)
        focusNode();

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

    // Dispatch pending frontend commands
    for (Vector<pair<long, String> >::iterator it = m_pendingEvaluateTestCommands.begin(); it != m_pendingEvaluateTestCommands.end(); ++it)
        m_frontend->evaluateForTestInFrontend((*it).first, (*it).second);
    m_pendingEvaluateTestCommands.clear();

    restoreDebugger();
    restoreProfiler(ProfilerRestoreNoAction);
}

void InspectorController::restoreDebugger()
{
    ASSERT(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent::isDebuggerAlwaysEnabled())
        enableDebuggerFromFrontend(false);
    else {
        if (m_state->getBoolean(InspectorState::debuggerAlwaysEnabled) || m_attachDebuggerWhenShown)
            enableDebugger();
    }
#endif
}

void InspectorController::restoreProfiler(ProfilerRestoreAction action)
{
    ASSERT(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_profilerAgent->setFrontend(m_frontend.get());
    if (!ScriptProfiler::isProfilerAlwaysEnabled() && m_state->getBoolean(InspectorState::profilerAlwaysEnabled))
        enableProfiler();
    if (action == ProfilerRestoreResetAgent)
        m_profilerAgent->resetState();
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
        clearConsoleMessages();

        m_times.clear();
        m_counts.clear();

#if ENABLE(JAVASCRIPT_DEBUGGER)
        if (m_debuggerAgent) {
            m_debuggerAgent->clearForPageNavigation();
            restoreStickyBreakpoints();
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

        if (m_frontend) {
            m_mainResourceIdentifier = 0;
            m_frontend->didCommitLoad();
            m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());
        }
    }
}

void InspectorController::frameDetachedFromParent(Frame* rootFrame)
{
    if (!enabled())
        return;

    if (m_resourceAgent)
        m_resourceAgent->frameDetachedFromParent(rootFrame);
}

void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader* loader, const CachedResource* cachedResource)
{
    if (!enabled())
        return;

    ensureSettingsLoaded();

    if (m_resourceAgent)
        m_resourceAgent->didLoadResourceFromMemoryCache(loader, cachedResource);
}

void InspectorController::identifierForInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    if (!enabled())
        return;
    ASSERT(m_inspectedPage);

    bool isMainResource = isMainResourceLoader(loader, request.url());
    if (isMainResource)
        m_mainResourceIdentifier = identifier;

    ensureSettingsLoaded();

    if (m_resourceAgent)
        m_resourceAgent->identifierForInitialRequest(identifier, request.url(), loader);
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

void InspectorController::willSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!enabled())
        return;

    request.setReportLoadTiming(true);

    if (m_frontend) {
        // Only enable raw headers if front-end is attached, as otherwise we may lack
        // permissions to fetch the headers.
        request.setReportRawHeaders(true);

        if (m_extraHeaders) {
            HTTPHeaderMap::const_iterator end = m_extraHeaders->end();
            for (HTTPHeaderMap::const_iterator it = m_extraHeaders->begin(); it != end; ++it)
                request.setHTTPHeaderField(it->first, it->second);
        }
    }

    bool isMainResource = m_mainResourceIdentifier == identifier;
    if (m_timelineAgent)
        m_timelineAgent->willSendResourceRequest(identifier, isMainResource, request);

    if (m_resourceAgent)
        m_resourceAgent->willSendRequest(identifier, request, redirectResponse);
}

void InspectorController::markResourceAsCached(unsigned long identifier)
{
    if (!enabled())
        return;

    if (m_resourceAgent)
        m_resourceAgent->markResourceAsCached(identifier);
}

void InspectorController::didReceiveResponse(unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response)
{
    if (!enabled())
        return;

    if (m_resourceAgent)
        m_resourceAgent->didReceiveResponse(identifier, loader, response);

    if (response.httpStatusCode() >= 400) {
        String message = makeString("Failed to load resource: the server responded with a status of ", String::number(response.httpStatusCode()), " (", response.httpStatusText(), ')');
        addConsoleMessage(new ConsoleMessage(OtherMessageSource, NetworkErrorMessageType, ErrorMessageLevel, message, response.url().string(), identifier));
    }
}

void InspectorController::didReceiveContentLength(unsigned long identifier, int lengthReceived)
{
    if (!enabled())
        return;

    if (m_resourceAgent)
        m_resourceAgent->didReceiveContentLength(identifier, lengthReceived);
}

void InspectorController::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (!enabled())
        return;

    if (m_timelineAgent)
        m_timelineAgent->didFinishLoadingResource(identifier, false, finishTime);

    if (m_resourceAgent)
        m_resourceAgent->didFinishLoading(identifier, finishTime);
}

void InspectorController::didFailLoading(unsigned long identifier, const ResourceError& error)
{
    if (!enabled())
        return;

    if (m_timelineAgent)
        m_timelineAgent->didFinishLoadingResource(identifier, true, 0);

    String message = "Failed to load resource";
        if (!error.localizedDescription().isEmpty())
            message += ": " + error.localizedDescription();
        addConsoleMessage(new ConsoleMessage(OtherMessageSource, NetworkErrorMessageType, ErrorMessageLevel, message, error.failingURL(), identifier));

    if (m_resourceAgent)
        m_resourceAgent->didFailLoading(identifier, error);
}

void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    if (!enabled())
        return;

    if (m_state->getBoolean(InspectorState::monitoringXHR))
        addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, "XHR finished loading: \"" + url + "\".", sendLineNumber, sendURL);

    if (m_resourceAgent)
        m_resourceAgent->setInitialContent(identifier, sourceString, "XHR");
}

void InspectorController::scriptImported(unsigned long identifier, const String& sourceString)
{
    if (!enabled())
        return;

    if (m_resourceAgent)
        m_resourceAgent->setInitialContent(identifier, sourceString, "Script");
}

void InspectorController::ensureSettingsLoaded()
{
    if (m_settingsLoaded)
        return;
    m_settingsLoaded = true;

    m_state->loadFromSettings();
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
        if (InspectorController* inspector = scriptContext->inspectorController())
            inspector->postWorkerNotificationToFrontend(*m_worker, m_action);
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
void InspectorController::selectDatabase(Database* database)
{
    if (!m_frontend)
        return;

    for (DatabaseResourcesMap::iterator it = m_databaseResources.begin(); it != m_databaseResources.end(); ++it) {
        if (it->second->database() == database) {
            m_frontend->selectDatabase(it->first);
            break;
        }
    }
}

Database* InspectorController::databaseForId(long databaseId)
{
    DatabaseResourcesMap::iterator it = m_databaseResources.find(databaseId);
    if (it == m_databaseResources.end())
        return 0;
    return it->second->database();
}

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

void InspectorController::selectDOMStorage(Storage* storage)
{
    ASSERT(storage);
    if (!m_frontend)
        return;

    Frame* frame = storage->frame();
    ExceptionCode ec = 0;
    bool isLocalStorage = (frame->domWindow()->localStorage(ec) == storage && !ec);
    long storageResourceId = 0;
    DOMStorageResourcesMap::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesMap::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it) {
        if (it->second->isSameHostAndType(frame, isLocalStorage)) {
            storageResourceId = it->first;
            break;
        }
    }
    if (storageResourceId)
        m_frontend->selectDOMStorage(storageResourceId);
}

void InspectorController::getDOMStorageEntries(long storageId, RefPtr<InspectorArray>* entries)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        storageResource->startReportingChangesToFrontend();
        Storage* domStorage = storageResource->domStorage();
        for (unsigned i = 0; i < domStorage->length(); ++i) {
            String name(domStorage->key(i));
            String value(domStorage->getItem(name));
            RefPtr<InspectorArray> entry = InspectorArray::create();
            entry->pushString(name);
            entry->pushString(value);
            (*entries)->pushArray(entry);
        }
    }
}

void InspectorController::setDOMStorageItem(long storageId, const String& key, const String& value, bool* success)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        ExceptionCode exception = 0;
        storageResource->domStorage()->setItem(key, value, exception);
        *success = !exception;
    }
}

void InspectorController::removeDOMStorageItem(long storageId, const String& key, bool* success)
{
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        storageResource->domStorage()->removeItem(key);
        *success = true;
    }
}

InspectorDOMStorageResource* InspectorController::getDOMStorageResourceForId(long storageId)
{
    DOMStorageResourcesMap::iterator it = m_domStorageResources.find(storageId);
    if (it == m_domStorageResources.end())
        return 0;
    return it->second.get();
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
void InspectorController::addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    if (!enabled())
        return;
    m_profilerAgent->addProfile(prpProfile, lineNumber, sourceURL);
}

void InspectorController::addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    m_profilerAgent->addProfileFinishedMessageToConsole(prpProfile, lineNumber, sourceURL);
}

void InspectorController::addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL)
{
    m_profilerAgent->addStartProfilingMessageToConsole(title, lineNumber, sourceURL);
}


bool InspectorController::isRecordingUserInitiatedProfile() const
{
    return m_profilerAgent->isRecordingUserInitiatedProfile();
}

String InspectorController::getCurrentUserInitiatedProfileName(bool incrementProfileNumber)
{
    return m_profilerAgent->getCurrentUserInitiatedProfileName(incrementProfileNumber);
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
        m_state->setBoolean(InspectorState::profilerAlwaysEnabled, true);
    m_profilerAgent->enable(skipRecompile);
}

void InspectorController::disableProfiler(bool always)
{
    if (always)
        m_state->setBoolean(InspectorState::profilerAlwaysEnabled, false);
    m_profilerAgent->disable();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::enableDebuggerFromFrontend(bool always)
{
    ASSERT(!debuggerEnabled());
    if (always)
        m_state->setBoolean(InspectorState::debuggerAlwaysEnabled, true);

    ASSERT(m_inspectedPage);

    m_debuggerAgent = InspectorDebuggerAgent::create(this, m_frontend.get());
    restoreStickyBreakpoints();

    m_frontend->debuggerWasEnabled();
}

void InspectorController::enableDebugger()
{
    if (!enabled())
        return;

    if (debuggerEnabled())
        return;

    if (!m_frontend)
        m_attachDebuggerWhenShown = true;
    else {
        m_frontend->attachDebuggerWhenShown();
        m_attachDebuggerWhenShown = false;
    }
}

void InspectorController::disableDebugger(bool always)
{
    if (!enabled())
        return;

    if (always)
        m_state->setBoolean(InspectorState::debuggerAlwaysEnabled, false);

    ASSERT(m_inspectedPage);

    m_debuggerAgent.clear();

    m_attachDebuggerWhenShown = false;

    if (m_frontend)
        m_frontend->debuggerWasDisabled();
}

void InspectorController::resume()
{
    if (m_debuggerAgent)
        m_debuggerAgent->resume();
}

void InspectorController::setStickyBreakpoints(PassRefPtr<InspectorObject> breakpoints)
{
    m_state->setObject(InspectorState::stickyBreakpoints, breakpoints);
}

void InspectorController::restoreStickyBreakpoints()
{
    m_eventListenerBreakpoints.clear();
    m_XHRBreakpoints.clear();
    m_hasXHRBreakpointWithEmptyURL = false;

    RefPtr<InspectorObject> allBreakpoints = m_state->getObject(InspectorState::stickyBreakpoints);
    KURL url = m_inspectedPage->mainFrame()->loader()->url();
    url.removeFragmentIdentifier();
    RefPtr<InspectorArray> breakpoints = allBreakpoints->getArray(url);
    if (!breakpoints)
        return;
    for (unsigned i = 0; i < breakpoints->length(); ++i)
        restoreStickyBreakpoint(breakpoints->get(i)->asObject());
}

void InspectorController::restoreStickyBreakpoint(PassRefPtr<InspectorObject> breakpoint)
{
    DEFINE_STATIC_LOCAL(String, eventListenerBreakpointType, ("EventListener"));
    DEFINE_STATIC_LOCAL(String, javaScriptBreakpointType, ("JS"));
    DEFINE_STATIC_LOCAL(String, xhrBreakpointType, ("XHR"));

    if (!breakpoint)
        return;
    String type;
    if (!breakpoint->getString("type", &type))
        return;
    bool enabled;
    if (!breakpoint->getBoolean("enabled", &enabled))
        return;
    RefPtr<InspectorObject> condition = breakpoint->getObject("condition");
    if (!condition)
        return;

    if (type == eventListenerBreakpointType) {
        if (!enabled)
            return;
        String eventName;
        if (condition->getString("eventName", &eventName))
            setEventListenerBreakpoint(eventName);
    } else if (type == javaScriptBreakpointType) {
        String url;
        if (!condition->getString("url", &url))
            return;
        double lineNumber;
        if (!condition->getNumber("lineNumber", &lineNumber))
            return;
        String javaScriptCondition;
        if (!condition->getString("condition", &javaScriptCondition))
            return;
        if (m_debuggerAgent)
            m_debuggerAgent->setStickyBreakpoint(url, static_cast<unsigned>(lineNumber), javaScriptCondition, enabled);
    } else if (type == xhrBreakpointType) {
        if (!enabled)
            return;
        String url;
        if (condition->getString("url", &url))
            setXHRBreakpoint(url);
    }
}

void InspectorController::setEventListenerBreakpoint(const String& eventName)
{
    m_eventListenerBreakpoints.add(eventName);
}

void InspectorController::removeEventListenerBreakpoint(const String& eventName)
{
    m_eventListenerBreakpoints.remove(eventName);
}

bool InspectorController::hasEventListenerBreakpoint(const String& eventName)
{
    return m_eventListenerBreakpoints.contains(eventName);
}

void InspectorController::setXHRBreakpoint(const String& url)
{
    if (url.isEmpty())
        m_hasXHRBreakpointWithEmptyURL = true;
    else
        m_XHRBreakpoints.add(url);
}

void InspectorController::removeXHRBreakpoint(const String& url)
{
    if (url.isEmpty())
        m_hasXHRBreakpointWithEmptyURL = false;
    else
        m_XHRBreakpoints.remove(url);
}

bool InspectorController::hasXHRBreakpoint(const String& url, String* breakpointURL)
{
    if (m_hasXHRBreakpointWithEmptyURL) {
        *breakpointURL = "";
        return true;
    }
    for (HashSet<String>::iterator it = m_XHRBreakpoints.begin(); it != m_XHRBreakpoints.end(); ++it) {
        if (url.contains(*it)) {
            *breakpointURL = *it;
            return true;
        }
    }
    return false;
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

void InspectorController::count(const String& title, unsigned lineNumber, const String& sourceID)
{
    String identifier = makeString(title, '@', sourceID, ':', String::number(lineNumber));
    HashMap<String, unsigned>::iterator it = m_counts.find(identifier);
    int count;
    if (it == m_counts.end())
        count = 1;
    else {
        count = it->second + 1;
        m_counts.remove(it);
    }

    m_counts.add(identifier, count);

    String message = makeString(title, ": ", String::number(count));
    addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, message, lineNumber, sourceID);
}

void InspectorController::startTiming(const String& title)
{
    m_times.add(title, currentTime() * 1000);
}

bool InspectorController::stopTiming(const String& title, double& elapsed)
{
    HashMap<String, double>::iterator it = m_times.find(title);
    if (it == m_times.end())
        return false;

    double startTime = it->second;
    m_times.remove(it);

    elapsed = currentTime() * 1000 - startTime;
    return true;
}

InjectedScript InspectorController::injectedScriptForNodeId(long id)
{

    Frame* frame = 0;
    if (id) {
        ASSERT(m_domAgent);
        Node* node = m_domAgent->nodeForId(id);
        if (node) {
            Document* document = node->ownerDocument();
            if (document)
                frame = document->frame();
        }
    } else
        frame = m_inspectedPage->mainFrame();

    if (frame)
        return m_injectedScriptHost->injectedScriptFor(mainWorldScriptState(frame));

    return InjectedScript();
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
