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
#include "HitTestResult.h"
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorBackend.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorCSSStore.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorDatabaseResource.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorFrontend.h"
#include "InspectorFrontendClient.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResource.h"
#include "InspectorStorageAgent.h"
#include "InspectorTimelineAgent.h"
#include "InspectorValues.h"
#include "InspectorWorkerResource.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderInline.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptCallStack.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "ScriptSourceCode.h"
#include "ScriptString.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "WindowFeatures.h"
#include <wtf/text/CString.h>
#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(DATABASE)
#include "Database.h"
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "InspectorApplicationCacheAgent.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#include "StorageArea.h"
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "ScriptDebugServer.h"
#endif

using namespace std;

namespace WebCore {

static const char* const resourceTrackingEnabledSettingName = "resourceTrackingEnabled";
static const char* const debuggerEnabledSettingName = "debuggerEnabled";
static const char* const profilerEnabledSettingName = "profilerEnabled";
static const char* const inspectorAttachedHeightName = "inspectorAttachedHeight";
static const char* const lastActivePanel = "lastActivePanel";
const char* const InspectorController::ElementsPanel = "elements";
const char* const InspectorController::ConsolePanel = "console";
const char* const InspectorController::ScriptsPanel = "scripts";
const char* const InspectorController::ProfilesPanel = "profiles";

static const char* const monitoringXHRSettingName = "xhrMonitor";

static int connectedFrontendCount = 0;

const String& InspectorController::frontendSettingsSettingName()
{
    DEFINE_STATIC_LOCAL(String, settingName, ("frontendSettings"));
    return settingName;
}

const String& InspectorController::inspectorStartsAttachedSettingName()
{
    DEFINE_STATIC_LOCAL(String, settingName, ("inspectorStartsAttached"));
    return settingName;
}

static const unsigned defaultAttachedHeight = 300;
static const float minimumAttachedHeight = 250.0f;
static const float maximumAttachedHeightRatio = 0.75f;
static const unsigned maximumConsoleMessages = 1000;
static const unsigned expireConsoleMessagesStep = 100;

static unsigned s_inspectorControllerCount;

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_openingFrontend(false)
    , m_cssStore(new InspectorCSSStore(this))
    , m_expiredConsoleMessageCount(0)
    , m_showAfterVisible(lastActivePanel)
    , m_sessionSettings(InspectorObject::create())
    , m_groupLevel(0)
    , m_searchingForNode(false)
    , m_monitoringXHR(false)
    , m_previousMessage(0)
    , m_resourceTrackingEnabled(false)
    , m_settingsLoaded(false)
    , m_inspectorBackend(InspectorBackend::create(this))
    , m_inspectorBackendDispatcher(new InspectorBackendDispatcher(this))
    , m_injectedScriptHost(InjectedScriptHost::create(this))
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_attachDebuggerWhenShown(false)
    , m_profilerAgent(InspectorProfilerAgent::create(this))
#endif
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
    ++s_inspectorControllerCount;
}

InspectorController::~InspectorController()
{
    // These should have been cleared in inspectedPageDestroyed().
    ASSERT(!m_client);
    ASSERT(!m_inspectedPage);
    ASSERT(!m_highlightedNode);

    deleteAllValues(m_frameResources);

    ASSERT(s_inspectorControllerCount);
    --s_inspectorControllerCount;

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

String InspectorController::setting(const String& key) const
{
    Settings::iterator it = m_settings.find(key);
    if (it != m_settings.end())
        return it->second;

    String value;
    m_client->populateSetting(key, &value);
    m_settings.set(key, value);
    return value;
}

void InspectorController::setSetting(const String& key, const String& value)
{
    m_settings.set(key, value);
    m_client->storeSetting(key, value);
}

void InspectorController::saveApplicationSettings(const String& settings)
{
    setSetting(InspectorController::frontendSettingsSettingName(), settings);
}

void InspectorController::saveSessionSettings(const String& settingsJSON)
{
    m_sessionSettings = InspectorValue::parseJSON(settingsJSON);
}

String InspectorController::getBackendSettings()
{
    RefPtr<InspectorObject> runtimeSettings = InspectorObject::create();
    runtimeSettings->setBoolean(monitoringXHRSettingName, m_monitoringXHR);
    runtimeSettings->setBoolean(resourceTrackingEnabledSettingName, m_resourceTrackingEnabled);
    return runtimeSettings->toJSONString();
}

void InspectorController::getSettings(RefPtr<InspectorObject>* settings)
{
    *settings = InspectorObject::create();
    (*settings)->setString("application", setting(frontendSettingsSettingName()));
    (*settings)->setString("session", m_sessionSettings->toJSONString());
    (*settings)->setString("backend", getBackendSettings());
}

void InspectorController::inspect(Node* node)
{
    if (!enabled())
        return;

    show();

    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!m_frontend) {
        m_showAfterVisible = ElementsPanel;
        return;
    }

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

void InspectorController::hideHighlight()
{
    if (!enabled())
        return;
    m_highlightedNode = 0;
    m_client->hideHighlight();
}

void InspectorController::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, ScriptCallStack* callStack, const String& message)
{
    if (!enabled())
        return;

    bool storeStackTrace = type == TraceMessageType || type == UncaughtExceptionMessageType || type == AssertMessageType;
    addConsoleMessage(callStack->state(), new ConsoleMessage(source, type, level, message, callStack, m_groupLevel, storeStackTrace));
}

void InspectorController::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    addConsoleMessage(0, new ConsoleMessage(source, type, level, message, lineNumber, sourceID, m_groupLevel));
}

void InspectorController::addConsoleMessage(ScriptState* scriptState, PassOwnPtr<ConsoleMessage> consoleMessage)
{
    ASSERT(enabled());
    ASSERT_ARG(consoleMessage, consoleMessage);

    if (m_previousMessage && m_previousMessage->isEqual(scriptState, consoleMessage.get())) {
        m_previousMessage->incrementCount();
        if (m_frontend)
            m_previousMessage->updateRepeatCountInConsole(m_frontend.get());
    } else {
        m_previousMessage = consoleMessage.get();
        m_consoleMessages.append(consoleMessage);
        if (m_frontend)
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
    m_groupLevel = 0;
    m_injectedScriptHost->releaseWrapperObjectGroup(0 /* release the group in all scripts */, "console");
    if (m_domAgent)
        m_domAgent->releaseDanglingNodes();
    if (m_frontend)
        m_frontend->consoleMessagesCleared();
}

void InspectorController::startGroup(MessageSource source, ScriptCallStack* callStack, bool collapsed)
{
    ++m_groupLevel;

    addConsoleMessage(callStack->state(), new ConsoleMessage(source, collapsed ? StartGroupCollapsedMessageType : StartGroupMessageType, LogMessageLevel, String(), callStack, m_groupLevel));
}

void InspectorController::endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL)
{
    if (!m_groupLevel)
        return;

    --m_groupLevel;

    addConsoleMessage(0, new ConsoleMessage(source, EndGroupMessageType, LogMessageLevel, String(), lineNumber, sourceURL, m_groupLevel));
}

void InspectorController::markTimeline(const String& message)
{
    if (timelineAgent())
        timelineAgent()->didMarkTimeline(message);
}

void InspectorController::storeLastActivePanel(const String& panelName)
{
    setSetting(lastActivePanel, panelName);
}

void InspectorController::mouseDidMoveOverElement(const HitTestResult& result, unsigned)
{
    if (!enabled() || !m_searchingForNode)
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

    ASSERT(m_searchingForNode);
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
    if (m_searchingForNode == enabled)
        return;
    m_searchingForNode = enabled;
    if (!m_searchingForNode)
        hideHighlight();
    if (m_frontend) {
        if (enabled)
            m_frontend->searchingForNodeWasEnabled();
        else
            m_frontend->searchingForNodeWasDisabled();
    }
}

void InspectorController::setMonitoringXHR(bool enabled, bool* newState)
{
    *newState = enabled;
    if (m_monitoringXHR == enabled)
        return;
    m_monitoringXHR = enabled;
    setSetting(monitoringXHRSettingName, enabled ? "true" : "false");
}

void InspectorController::connectFrontend()
{
    m_openingFrontend = false;
    releaseFrontendLifetimeAgents();
    m_frontend = new InspectorFrontend(m_client);
    m_domAgent = InspectorDOMAgent::create(m_cssStore.get(), m_frontend.get());

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

    if (!connectedFrontendCount)
        ScriptController::setCaptureCallStackForUncaughtExceptions(true);
    connectedFrontendCount++;
}

void InspectorController::reuseFrontend()
{
    connectFrontend();
    restoreDebugger();
    restoreProfiler();
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

    if (panel == lastActivePanel)
        return;

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

    connectedFrontendCount--;
    if (!connectedFrontendCount)
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

    m_showAfterVisible = lastActivePanel;

    hideHighlight();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_profilerAgent->setFrontend(0);
    m_profilerAgent->stopUserInitiatedProfiling();
#endif

    releaseFrontendLifetimeAgents();
    m_timelineAgent.clear();
}

void InspectorController::releaseFrontendLifetimeAgents()
{
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
}

void InspectorController::populateScriptObjects()
{
    ASSERT(m_frontend);
    if (!m_frontend)
        return;

    if (m_showAfterVisible == lastActivePanel)
        m_showAfterVisible = setting(lastActivePanel);

    showPanel(m_showAfterVisible);

    if (m_searchingForNode)
        m_frontend->searchingForNodeWasEnabled();


#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_profilerAgent->enabled())
        m_frontend->profilerWasEnabled();
#endif

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        it->second->updateScriptObject(m_frontend.get());

    m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());

    if (m_nodeToFocus)
        focusNode();

    if (m_expiredConsoleMessageCount)
        m_frontend->updateConsoleMessageExpiredCount(m_expiredConsoleMessageCount);
    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        m_consoleMessages[i]->addToFrontend(m_frontend.get(), m_injectedScriptHost.get());

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (debuggerEnabled())
        m_frontend->updatePauseOnExceptionsState(ScriptDebugServer::shared().pauseOnExceptionsState());
#endif
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
#if ENABLE(WORKERS)
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
    restoreProfiler();
}

void InspectorController::restoreDebugger()
{
    ASSERT(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent::isDebuggerAlwaysEnabled())
        enableDebuggerFromFrontend(false);
    else {
        String debuggerEnabled = setting(debuggerEnabledSettingName);
        if (debuggerEnabled == "true" || m_attachDebuggerWhenShown)
            enableDebugger();
    }
#endif
}

void InspectorController::restoreProfiler()
{
    ASSERT(m_frontend);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_profilerAgent->setFrontend(m_frontend.get());
    if (!ScriptProfiler::isProfilerAlwaysEnabled()) {
        String profilerEnabledSetting = setting(profilerEnabledSettingName);
        if (profilerEnabledSetting == "true")
            enableProfiler();
    }
#endif
}

void InspectorController::unbindAllResources()
{
    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        it->second->releaseScriptObject(0);

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

void InspectorController::pruneResources(ResourcesMap* resourceMap, DocumentLoader* loaderToKeep)
{
    ASSERT_ARG(resourceMap, resourceMap);

    ResourcesMap mapCopy(*resourceMap);
    ResourcesMap::iterator end = mapCopy.end();
    for (ResourcesMap::iterator it = mapCopy.begin(); it != end; ++it) {
        InspectorResource* resource = (*it).second.get();
        if (resource == m_mainResource)
            continue;

        if (!loaderToKeep || !resource->isSameLoader(loaderToKeep)) {
            removeResource(resource);
            if (m_frontend)
                resource->releaseScriptObject(m_frontend.get());
        }
    }
}

void InspectorController::didCommitLoad(DocumentLoader* loader)
{
    if (!enabled())
        return;

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        if (m_frontend)
            m_frontend->inspectedURLChanged(loader->url().string());

        m_injectedScriptHost->discardInjectedScripts();
        clearConsoleMessages();

        m_times.clear();
        m_counts.clear();
#if ENABLE(JAVASCRIPT_DEBUGGER)
        if (m_debuggerAgent)
            m_debuggerAgent->clearForPageNavigation();
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
        m_profilerAgent->resetState();
#endif
        // unbindAllResources should be called before database and DOM storage
        // resources are cleared so that it has a chance to unbind them.
        unbindAllResources();

        m_cssStore->reset();
        m_sessionSettings = InspectorObject::create();
        if (m_frontend) {
            m_frontend->reset();
            m_domAgent->reset();
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
            if (!loader->frameLoader()->isLoadingFromCachedPage()) {
                ASSERT(m_mainResource && m_mainResource->isSameLoader(loader));
                // We don't add the main resource until its load is committed. This is
                // needed to keep the load for a user-entered URL from showing up in the
                // list of resources for the page they are navigating away from.
                m_mainResource->updateScriptObject(m_frontend.get());
            } else {
                // Pages loaded from the page cache are committed before
                // m_mainResource is the right resource for this load, so we
                // clear it here. It will be re-assigned in
                // identifierForInitialRequest.
                m_mainResource = 0;
            }
            m_frontend->didCommitLoad();
            m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());
        }
    }

    for (Frame* frame = loader->frame(); frame; frame = frame->tree()->traverseNext(loader->frame()))
        if (ResourcesMap* resourceMap = m_frameResources.get(frame))
            pruneResources(resourceMap, loader);
}

void InspectorController::frameDetachedFromParent(Frame* frame)
{
    if (!enabled())
        return;
    if (ResourcesMap* resourceMap = m_frameResources.get(frame))
        removeAllResources(resourceMap);
}

void InspectorController::addResource(InspectorResource* resource)
{
    m_resources.set(resource->identifier(), resource);
    m_knownResources.add(resource->requestURL());

    Frame* frame = resource->frame();
    if (!frame)
        return;
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (resourceMap)
        resourceMap->set(resource->identifier(), resource);
    else {
        resourceMap = new ResourcesMap;
        resourceMap->set(resource->identifier(), resource);
        m_frameResources.set(frame, resourceMap);
    }
}

void InspectorController::removeResource(InspectorResource* resource)
{
    m_resources.remove(resource->identifier());
    String requestURL = resource->requestURL();
    if (!requestURL.isNull())
        m_knownResources.remove(requestURL);

    Frame* frame = resource->frame();
    if (!frame)
        return;
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (!resourceMap) {
        ASSERT_NOT_REACHED();
        return;
    }

    resourceMap->remove(resource->identifier());
    if (resourceMap->isEmpty()) {
        m_frameResources.remove(frame);
        delete resourceMap;
    }
}

InspectorResource* InspectorController::getTrackedResource(unsigned long identifier)
{
    if (!enabled())
        return 0;

    if (m_resourceTrackingEnabled)
        return m_resources.get(identifier).get();

    bool isMainResource = m_mainResource && m_mainResource->identifier() == identifier;
    if (isMainResource)
        return m_mainResource.get();

    return 0;
}

InspectorResource* InspectorController::resourceForURL(const String& url)
{
    for (InspectorController::ResourcesMap::iterator resIt = m_resources.begin(); resIt != m_resources.end(); ++resIt) {
        if (resIt->second->requestURL().string() == url)
            return resIt->second.get();
    }
    return 0;
}

void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader* loader, const CachedResource* cachedResource)
{
    if (!enabled())
        return;

    // If the resource URL is already known, we don't need to add it again since this is just a cached load.
    if (m_knownResources.contains(cachedResource->url()))
        return;

    ASSERT(m_inspectedPage);
    bool isMainResource = isMainResourceLoader(loader, KURL(ParsedURLString, cachedResource->url()));
    ensureSettingsLoaded();
    if (!isMainResource && !m_resourceTrackingEnabled)
        return;

    RefPtr<InspectorResource> resource = InspectorResource::createCached(m_inspectedPage->progress()->createUniqueIdentifier(), loader, cachedResource);

    if (isMainResource) {
        m_mainResource = resource;
        resource->markMainResource();
    }

    addResource(resource.get());

    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::identifierForInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    if (!enabled())
        return;
    ASSERT(m_inspectedPage);

    bool isMainResource = isMainResourceLoader(loader, request.url());
    ensureSettingsLoaded();
    if (!isMainResource && !m_resourceTrackingEnabled)
        return;

    RefPtr<InspectorResource> resource = InspectorResource::create(identifier, loader, request.url());

    if (isMainResource) {
        m_mainResource = resource;
        resource->markMainResource();
    }

    addResource(resource.get());

    if (m_frontend && loader->frameLoader()->isLoadingFromCachedPage() && resource == m_mainResource)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::mainResourceFiredDOMContentEvent(DocumentLoader* loader, const KURL& url)
{
    if (!enabled() || !isMainResourceLoader(loader, url))
        return;

    if (m_mainResource) {
        m_mainResource->markDOMContentEventTime();
        if (m_timelineAgent)
            m_timelineAgent->didMarkDOMContentEvent();
        if (m_frontend)
            m_mainResource->updateScriptObject(m_frontend.get());
    }
}

void InspectorController::mainResourceFiredLoadEvent(DocumentLoader* loader, const KURL& url)
{
    if (!enabled() || !isMainResourceLoader(loader, url))
        return;

    if (m_mainResource) {
        m_mainResource->markLoadEventTime();
        if (m_timelineAgent)
            m_timelineAgent->didMarkLoadEvent();
        if (m_frontend)
            m_mainResource->updateScriptObject(m_frontend.get());
    }
}

bool InspectorController::isMainResourceLoader(DocumentLoader* loader, const KURL& requestUrl)
{
    return loader->frame() == m_inspectedPage->mainFrame() && requestUrl == loader->requestURL();
}

void InspectorController::willSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!enabled())
        return;

    bool isMainResource = (m_mainResource && m_mainResource->identifier() == identifier);
    if (m_timelineAgent)
        m_timelineAgent->willSendResourceRequest(identifier, isMainResource, request);

    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    request.setReportLoadTiming(true);

    if (!redirectResponse.isNull()) {
        // Redirect may have empty URL and we'd like to not crash with invalid HashMap entry.
        // See http/tests/misc/will-send-request-returns-null-on-redirect.html
        if (!request.url().isEmpty()) {
            resource->endTiming(0);
            resource->updateResponse(redirectResponse);

            // We always store last redirect by the original id key. Rest of the redirects are stored within the last one.
            unsigned long id = m_inspectedPage->progress()->createUniqueIdentifier();
            RefPtr<InspectorResource> withRedirect = resource->appendRedirect(id, request.url());
            removeResource(resource.get());
            addResource(withRedirect.get());
            if (isMainResource) {
                m_mainResource = withRedirect;
                withRedirect->markMainResource();
            }
            resource = withRedirect;
        }
    }

    resource->startTiming();
    resource->updateRequest(request);

    if (resource != m_mainResource && m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    if (!enabled())
        return;

    if (RefPtr<InspectorResource> resource = getTrackedResource(identifier)) {
        resource->updateResponse(response);

        if (resource != m_mainResource && m_frontend)
            resource->updateScriptObject(m_frontend.get());
    }
    if (response.httpStatusCode() >= 400) {
        // The ugly code below is due to that String::format() is not utf8-safe at the moment.
        String message = String::format("Failed to load resource: the server responded with a status of %u (", response.httpStatusCode()) + response.httpStatusText() + ")";

        addMessageToConsole(OtherMessageSource, LogMessageType, ErrorMessageLevel, message, 0, response.url().string());
    }
}

void InspectorController::didReceiveContentLength(unsigned long identifier, int lengthReceived)
{
    if (!enabled())
        return;

    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->addLength(lengthReceived);

    if (resource != m_mainResource && m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (!enabled())
        return;

    if (m_timelineAgent)
        m_timelineAgent->didFinishLoadingResource(identifier, false);

    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->endTiming(finishTime);

    // No need to mute this event for main resource since it happens after did commit load.
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didFailLoading(unsigned long identifier, const ResourceError& error)
{
    if (!enabled())
        return;

    if (m_timelineAgent)
        m_timelineAgent->didFinishLoadingResource(identifier, true);

    String message = "Failed to load resource";
    if (!error.localizedDescription().isEmpty())
        message += ": " + error.localizedDescription();
    addMessageToConsole(OtherMessageSource, LogMessageType, ErrorMessageLevel, message, 0, error.failingURL());

    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->markFailed();
    resource->endTiming(0);

    // No need to mute this event for main resource since it happens after did commit load.
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    if (!enabled())
        return;

    if (m_monitoringXHR)
        addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, "XHR finished loading: \"" + url + "\".", sendLineNumber, sendURL);

    if (!m_resourceTrackingEnabled)
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->setOverrideContent(sourceString, InspectorResource::XHR);

    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::scriptImported(unsigned long identifier, const String& sourceString)
{
    if (!enabled() || !m_resourceTrackingEnabled)
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->setOverrideContent(ScriptString(sourceString), InspectorResource::Script);

    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::setResourceTracking(bool enable, bool always, bool* newState)
{
    *newState = enable;

    if (always)
        setSetting(resourceTrackingEnabledSettingName, enable ? "true" : "false");

    if (m_resourceTrackingEnabled == enable)
        return;

    ASSERT(m_inspectedPage);
    m_resourceTrackingEnabled = enable;

    if (enable) {
        m_client->resourceTrackingWasEnabled();
        m_inspectedPage->mainFrame()->redirectScheduler()->scheduleRefresh(true);
    } else
        m_client->resourceTrackingWasDisabled();
}

void InspectorController::setResourceTracking(bool enable)
{
    if (!enabled())
        return;

    ASSERT(m_inspectedPage);
    m_resourceTrackingEnabled = enable;
}

void InspectorController::ensureSettingsLoaded()
{
    if (m_settingsLoaded)
        return;
    m_settingsLoaded = true;

    String resourceTracking = setting(resourceTrackingEnabledSettingName);
    if (resourceTracking == "true")
        m_resourceTrackingEnabled = true;
    m_client->resourceTrackingWasEnabled();

    String monitoringXHR = setting(monitoringXHRSettingName);
    if (monitoringXHR == "true")
        m_monitoringXHR = true;
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
    m_client->timelineProfilerWasStarted();
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
    m_client->timelineProfilerWasStopped();
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
    switch (action) {
    case InspectorController::WorkerCreated:
        m_frontend->didCreateWorker(worker.id(), worker.url(), worker.isSharedWorker());
        break;
    case InspectorController::WorkerDestroyed:
        m_frontend->didDestroyWorker(worker.id());
        break;
    }
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

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it) {
        Document* document = it->second->frame()->document();
        Vector<Cookie> docCookiesList;
        rawCookiesImplemented = getRawCookies(document, it->second->requestURL(), docCookiesList);

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

    if (rawCookiesImplemented)
        *cookies = buildArrayForCookies(rawCookiesList);
    else {
        *cookiesString = stringCookiesList;
    }
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
    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it) {
        Document* document = it->second->frame()->document();
        if (document->url().host() == domain)
            WebCore::deleteCookie(document, it->second->requestURL(), cookieName);
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

    RefPtr<InspectorResource> resource = InspectorResource::createWebSocket(identifier, requestURL, documentURL);
    addResource(resource.get());

    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::willSendWebSocketHandshakeRequest(unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;
    resource->startTiming();
    resource->updateWebSocketRequest(request);
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didReceiveWebSocketHandshakeResponse(unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;
    // Calling resource->markResponseReceivedTime() here makes resources bar chart confusing, because
    // we cannot apply the "latency + download" model of regular resources to WebSocket connections.
    // FIXME: Design a new UI for bar charts of WebSocket resources, and record timing here.
    resource->updateWebSocketResponse(response);
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didCloseWebSocket(unsigned long identifier)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->endTiming(0);
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
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
}

void InspectorController::stopUserInitiatedProfiling()
{
    if (!enabled())
        return;
    m_profilerAgent->stopUserInitiatedProfiling();
}

bool InspectorController::profilerEnabled() const
{
    return enabled() && m_profilerAgent->enabled();
}

void InspectorController::enableProfiler(bool always, bool skipRecompile)
{
    if (always)
        setSetting(profilerEnabledSettingName, "true");
    m_profilerAgent->enable(skipRecompile);
}

void InspectorController::disableProfiler(bool always)
{
    if (always)
        setSetting(profilerEnabledSettingName, "false");
    m_profilerAgent->disable();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::enableDebuggerFromFrontend(bool always)
{
    ASSERT(!debuggerEnabled());
    if (always)
        setSetting(debuggerEnabledSettingName, "true");

    ASSERT(m_inspectedPage);

    m_debuggerAgent = InspectorDebuggerAgent::create(this, m_frontend.get());

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
        setSetting(debuggerEnabledSettingName, "false");

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

#if ENABLE(JAVASCRIPT_DEBUGGER)
String InspectorController::breakpointsSettingKey()
{
    DEFINE_STATIC_LOCAL(String, keyPrefix, ("breakpoints:"));
    return keyPrefix + InspectorDebuggerAgent::md5Base16(m_mainResource->requestURL());
}

PassRefPtr<InspectorValue> InspectorController::loadBreakpoints()
{
    return InspectorValue::parseJSON(setting(breakpointsSettingKey()));
}

void InspectorController::saveBreakpoints(PassRefPtr<InspectorObject> breakpoints)
{
    setSetting(breakpointsSettingKey(), breakpoints->toJSONString());
}
#endif

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
        context.addPath(quadPath);
        context.clipOut(quadPath);

        context.addPath(quadPath);
        context.setStrokeThickness(outlineThickness);
        context.setStrokeColor(outlineColor, DeviceColorSpace);
        context.strokePath();

        context.restore();
    }

    // Now do the fill
    context.addPath(quadPath);
    context.setFillColor(fillColor, DeviceColorSpace);
    context.fillPath();
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

        drawHighlightForBox(context, absContentQuad, absPaddingQuad, absBorderQuad, absMarginQuad);
    } else if (renderer->isRenderInline() || isSVGRenderer) {
        // FIXME: We should show margins/padding/border for inlines.
        Vector<FloatQuad> lineBoxQuads;
        renderer->absoluteQuads(lineBoxQuads);
        for (unsigned i = 0; i < lineBoxQuads.size(); ++i)
            lineBoxQuads[i] += mainFrameOffset;

        drawHighlightForLineBoxesOrSVGRenderer(context, lineBoxQuads);
    }
}

void InspectorController::openInInspectedWindow(const String& url)
{
    ResourceRequest request;
    FrameLoadRequest frameRequest(request, "_blank");
    bool created;
    Frame* mainFrame = m_inspectedPage->mainFrame();
    WindowFeatures windowFeatures;
    Frame* newFrame = WebCore::createWindow(mainFrame, mainFrame, frameRequest, windowFeatures, created);
    if (!newFrame)
        return;

    newFrame->loader()->setOpener(mainFrame);
    newFrame->page()->setOpenedByDOM();
    newFrame->loader()->changeLocation(newFrame->loader()->completeURL(url), "", false, false, true);
}

void InspectorController::count(const String& title, unsigned lineNumber, const String& sourceID)
{
    String identifier = title + String::format("@%s:%d", sourceID.utf8().data(), lineNumber);
    HashMap<String, unsigned>::iterator it = m_counts.find(identifier);
    int count;
    if (it == m_counts.end())
        count = 1;
    else {
        count = it->second + 1;
        m_counts.remove(it);
    }

    m_counts.add(identifier, count);

    String message = String::format("%s: %d", title.utf8().data(), count);
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

void InspectorController::getResourceContent(unsigned long identifier, String* content)
{
    RefPtr<InspectorResource> resource = m_resources.get(identifier);
    *content = resource ? resource->sourceString() : String();
}

void InspectorController::reloadPage()
{
    m_inspectedPage->mainFrame()->redirectScheduler()->scheduleRefresh(true);
}

void InspectorController::willInsertDOMNodeImpl(Node* node, Node* parent)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (!m_debuggerAgent || !m_domAgent)
        return;
    PassRefPtr<InspectorValue> details;
    if (m_domAgent->shouldBreakOnNodeInsertion(node, parent, &details))
        m_debuggerAgent->breakProgram(details);
#endif
}

void InspectorController::didInsertDOMNodeImpl(Node* node)
{
    if (m_domAgent)
        m_domAgent->didInsertDOMNode(node);
}

void InspectorController::willRemoveDOMNodeImpl(Node* node)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (!m_debuggerAgent || !m_domAgent)
        return;
    PassRefPtr<InspectorValue> details;
    if (m_domAgent->shouldBreakOnNodeRemoval(node, &details))
        m_debuggerAgent->breakProgram(details);
#endif
}

void InspectorController::didRemoveDOMNodeImpl(Node* node)
{
    if (m_domAgent)
        m_domAgent->didRemoveDOMNode(node);
}

void InspectorController::willModifyDOMAttrImpl(Element* element)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (!m_debuggerAgent || !m_domAgent)
        return;
    PassRefPtr<InspectorValue> details;
    if (m_domAgent->shouldBreakOnAttributeModification(element, &details))
        m_debuggerAgent->breakProgram(details);
#endif
}

void InspectorController::didModifyDOMAttrImpl(Element* element)
{
    if (m_domAgent)
        m_domAgent->didModifyDOMAttr(element);
}


} // namespace WebCore

#endif // ENABLE(INSPECTOR)
