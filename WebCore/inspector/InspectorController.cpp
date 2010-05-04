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
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HitTestResult.h"
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorBackend.h"
#include "InspectorCSSStore.h"
#include "InspectorClient.h"
#include "InspectorFrontendClient.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorDatabaseResource.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "InspectorWorkerResource.h"
#include "InspectorTimelineAgent.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderInline.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptBreakpoint.h"
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
#include <wtf/text/CString.h>
#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(DATABASE)
#include "Database.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#include "StorageArea.h"
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "ScriptDebugServer.h"
#if USE(JSC)
#include <runtime/JSLock.h>
#include <runtime/UString.h>
#include "JSScriptProfile.h"
#else
#include "V8ScriptProfile.h"
#endif
#endif

using namespace std;

namespace WebCore {

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";
static const char* const CPUProfileType = "CPU";
static const char* const resourceTrackingEnabledSettingName = "resourceTrackingEnabled";
static const char* const debuggerEnabledSettingName = "debuggerEnabled";
static const char* const profilerEnabledSettingName = "profilerEnabled";
static const char* const inspectorAttachedHeightName = "inspectorAttachedHeight";
static const char* const lastActivePanelSettingName = "lastActivePanel";

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
    , m_cssStore(new InspectorCSSStore())
    , m_expiredConsoleMessageCount(0)
    , m_showAfterVisible(CurrentPanel)
    , m_groupLevel(0)
    , m_searchingForNode(false)
    , m_previousMessage(0)
    , m_resourceTrackingEnabled(false)
    , m_resourceTrackingSettingsLoaded(false)
    , m_inspectorBackend(InspectorBackend::create(this))
    , m_injectedScriptHost(InjectedScriptHost::create(this))
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_debuggerEnabled(false)
    , m_attachDebuggerWhenShown(false)
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_profilerEnabled(!WTF_USE_JSC)
    , m_recordingUserInitiatedProfile(false)
    , m_currentUserInitiatedProfileNumber(-1)
    , m_nextUserInitiatedProfileNumber(1)
    , m_startProfiling(this, &InspectorController::startUserInitiatedProfiling)
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

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);

    ASSERT(s_inspectorControllerCount);
    --s_inspectorControllerCount;

    releaseDOMAgent();

    m_inspectorBackend->disconnectController();
    m_injectedScriptHost->disconnectController();
}

void InspectorController::inspectedPageDestroyed()
{
    if (m_frontend)
        m_frontend->inspectedPageDestroyed();

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

void InspectorController::hideHighlight()
{
    if (!enabled())
        return;
    m_highlightedNode = 0;
    m_client->hideHighlight();
}

bool InspectorController::windowVisible()
{
    return m_frontend;
}

void InspectorController::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, ScriptCallStack* callStack)
{
    if (!enabled())
        return;

    addConsoleMessage(callStack->state(), new ConsoleMessage(source, type, level, callStack, m_groupLevel, type == TraceMessageType));
}

void InspectorController::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    addConsoleMessage(0, new ConsoleMessage(source, type, level, message, lineNumber, sourceID, m_groupLevel));
}

void InspectorController::addConsoleMessage(ScriptState* scriptState, ConsoleMessage* consoleMessage)
{
    ASSERT(enabled());
    ASSERT_ARG(consoleMessage, consoleMessage);

    if (m_previousMessage && m_previousMessage->isEqual(scriptState, consoleMessage)) {
        m_previousMessage->incrementCount();
        delete consoleMessage;
        if (m_frontend)
            m_previousMessage->updateRepeatCountInConsole(m_frontend.get());
    } else {
        m_previousMessage = consoleMessage;
        m_consoleMessages.append(consoleMessage);
        if (m_frontend)
            m_previousMessage->addToFrontend(m_frontend.get(), m_injectedScriptHost.get());
    }

    if (!m_frontend && m_consoleMessages.size() >= maximumConsoleMessages) {
        m_expiredConsoleMessageCount += expireConsoleMessagesStep;
        for (size_t i = 0; i < expireConsoleMessagesStep; ++i)
            delete m_consoleMessages[i];
        m_consoleMessages.remove(0, expireConsoleMessagesStep);
    }
}

void InspectorController::clearConsoleMessages()
{
    deleteAllValues(m_consoleMessages);
    m_consoleMessages.clear();
    m_expiredConsoleMessageCount = 0;
    m_previousMessage = 0;
    m_groupLevel = 0;
    m_injectedScriptHost->releaseWrapperObjectGroup(0 /* release the group in all scripts */, "console");
    if (m_domAgent)
        m_domAgent->releaseDanglingNodes();
    if (m_frontend)
        m_frontend->clearConsoleMessages();
}

void InspectorController::startGroup(MessageSource source, ScriptCallStack* callStack)
{
    ++m_groupLevel;

    addConsoleMessage(callStack->state(), new ConsoleMessage(source, StartGroupMessageType, LogMessageLevel, callStack, m_groupLevel));
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
    setSetting(lastActivePanelSettingName, panelName);
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

    if (!enabled() || !m_frontend || frame != m_inspectedPage->mainFrame())
        return;
    m_injectedScriptHost->discardInjectedScripts();
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

void InspectorController::setFrontend(PassOwnPtr<InspectorFrontend> frontend)
{
    ASSERT(frontend);
    m_openingFrontend = false;
    m_frontend = frontend;
    releaseDOMAgent();
    m_domAgent = InspectorDOMAgent::create(m_cssStore.get(), m_frontend.get());
    if (m_timelineAgent)
        m_timelineAgent->resetFrontendProxyObject(m_frontend.get());
#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
    String debuggerEnabled = setting(debuggerEnabledSettingName);
    if (debuggerEnabled == "true")
        enableDebugger();
    String profilerEnabled = setting(profilerEnabledSettingName);
    if (profilerEnabled == "true")
        enableProfiler();
#endif

    // Initialize Web Inspector title.
    m_frontend->inspectedURLChanged(m_inspectedPage->mainFrame()->loader()->url().string());

    populateScriptObjects();
    
    if (m_showAfterVisible == CurrentPanel) {
        String lastActivePanelSetting = setting(lastActivePanelSettingName);
        m_showAfterVisible = specialPanelForJSName(lastActivePanelSetting);
    }
    
    if (m_nodeToFocus)
        focusNode();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_attachDebuggerWhenShown)
        enableDebugger();
#endif
    showPanel(m_showAfterVisible);
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

void InspectorController::showPanel(SpecialPanels panel)
{
    if (!enabled())
        return;

    show();

    if (!m_frontend) {
        m_showAfterVisible = panel;
        return;
    }

    if (panel == CurrentPanel)
        return;

    m_frontend->showPanel(panel);
}

void InspectorController::close()
{
    if (!m_frontend)
        return;
    m_frontend->close();
}

void InspectorController::disconnectFrontend()
{
    if (!m_frontend)
        return;
    m_frontend.clear();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    // If the window is being closed with the debugger enabled,
    // remember this state to re-enable debugger on the next window
    // opening.
    bool debuggerWasEnabled = m_debuggerEnabled;
    disableDebugger();
    if (debuggerWasEnabled)
        m_attachDebuggerWhenShown = true;
#endif
    setSearchingForNode(false);
    unbindAllResources();
    stopTimelineProfiler();

    m_showAfterVisible = CurrentPanel;

    hideHighlight();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    stopUserInitiatedProfiling();
#endif

    releaseDOMAgent();
    m_timelineAgent.clear();
}

void InspectorController::releaseDOMAgent()
{
    // m_domAgent is RefPtr. Remove DOM listeners first to ensure that there are
    // no references to the DOM agent from the DOM tree.
    if (m_domAgent)
        m_domAgent->reset();
    m_domAgent.clear();
}

void InspectorController::populateScriptObjects()
{
    ASSERT(m_frontend);
    if (!m_frontend)
        return;

    m_frontend->populateFrontendSettings(setting(frontendSettingsSettingName()));

    if (m_resourceTrackingEnabled)
        m_frontend->resourceTrackingWasEnabled();

    if (m_searchingForNode)
        m_frontend->searchingForNodeWasEnabled();
    else
        m_frontend->searchingForNodeWasDisabled();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_profilerEnabled)
        m_frontend->profilerWasEnabled();
#endif

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        it->second->updateScriptObject(m_frontend.get());

    m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());

    if (m_expiredConsoleMessageCount)
        m_frontend->updateConsoleMessageExpiredCount(m_expiredConsoleMessageCount);
    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        m_consoleMessages[i]->addToFrontend(m_frontend.get(), m_injectedScriptHost.get());

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (m_debuggerEnabled)
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
    for (WorkersMap::iterator it = m_workers.begin(); it != workersEnd; ++it)
        m_frontend->didCreateWorker(*it->second);
#endif

    m_frontend->populateInterface();

    // Dispatch pending frontend commands
    for (Vector<pair<long, String> >::iterator it = m_pendingEvaluateTestCommands.begin(); it != m_pendingEvaluateTestCommands.end(); ++it)
        m_frontend->evaluateForTestInFrontend((*it).first, (*it).second);
    m_pendingEvaluateTestCommands.clear();
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
        m_sourceIDToURL.clear();
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
        m_profiles.clear();
        m_currentUserInitiatedProfileNumber = 1;
        m_nextUserInitiatedProfileNumber = 1;
#endif
        // unbindAllResources should be called before database and DOM storage
        // resources are cleared so that it has a chance to unbind them.
        unbindAllResources();
        
        m_cssStore->reset();
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

    if (m_scriptsToEvaluateOnLoad.size()) {
        ScriptState* scriptState = mainWorldScriptState(loader->frame());
        for (Vector<String>::iterator it = m_scriptsToEvaluateOnLoad.begin();
             it != m_scriptsToEvaluateOnLoad.end(); ++it) {
            m_injectedScriptHost->injectScript(*it, scriptState);
        }
    }
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

void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader* loader, const CachedResource* cachedResource)
{
    if (!enabled())
        return;

    // If the resource URL is already known, we don't need to add it again since this is just a cached load.
    if (m_knownResources.contains(cachedResource->url()))
        return;

    ASSERT(m_inspectedPage);
    bool isMainResource = isMainResourceLoader(loader, KURL(ParsedURLString, cachedResource->url()));
    ensureResourceTrackingSettingsLoaded();
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
    ensureResourceTrackingSettingsLoaded();
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

void InspectorController::willSendRequest(unsigned long identifier, const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    bool isMainResource = (m_mainResource && m_mainResource->identifier() == identifier);
    if (m_timelineAgent)
        m_timelineAgent->willSendResourceRequest(identifier, isMainResource, request);

    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    if (!redirectResponse.isNull()) {
        resource->markResponseReceivedTime();
        resource->endTiming();
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

    resource->startTiming();
    resource->updateRequest(request);

    if (resource != m_mainResource && m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    if (RefPtr<InspectorResource> resource = getTrackedResource(identifier)) {
        resource->updateResponse(response);
        resource->markResponseReceivedTime();

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
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->addLength(lengthReceived);

    if (resource != m_mainResource && m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didFinishLoading(unsigned long identifier)
{
    if (m_timelineAgent)
        m_timelineAgent->didFinishLoadingResource(identifier, false);

    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->endTiming();

    // No need to mute this event for main resource since it happens after did commit load.
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didFailLoading(unsigned long identifier, const ResourceError& error)
{
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
    resource->endTiming();

    // No need to mute this event for main resource since it happens after did commit load.
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString)
{
    if (!enabled() || !m_resourceTrackingEnabled)
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

void InspectorController::enableResourceTracking(bool always, bool reload)
{
    if (!enabled())
        return;

    if (always)
        setSetting(resourceTrackingEnabledSettingName, "true");

    if (m_resourceTrackingEnabled)
        return;

    ASSERT(m_inspectedPage);
    m_resourceTrackingEnabled = true;
    if (m_frontend)
        m_frontend->resourceTrackingWasEnabled();

    if (reload)
        m_inspectedPage->mainFrame()->redirectScheduler()->scheduleRefresh(true);
}

void InspectorController::disableResourceTracking(bool always)
{
    if (!enabled())
        return;

    if (always)
        setSetting(resourceTrackingEnabledSettingName, "false");

    ASSERT(m_inspectedPage);
    m_resourceTrackingEnabled = false;
    if (m_frontend)
        m_frontend->resourceTrackingWasDisabled();
}

void InspectorController::ensureResourceTrackingSettingsLoaded()
{
    if (m_resourceTrackingSettingsLoaded)
        return;
    m_resourceTrackingSettingsLoaded = true;

    String resourceTracking = setting(resourceTrackingEnabledSettingName);
    if (resourceTracking == "true")
        m_resourceTrackingEnabled = true;
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
        m_frontend->didCreateWorker(worker);
        break;
    case InspectorController::WorkerDestroyed:
        m_frontend->didDestroyWorker(worker);
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

void InspectorController::didOpenDatabase(Database* database, const String& domain, const String& name, const String& version)
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

void InspectorController::getCookies(long callId)
{
    if (!m_frontend)
        return;

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

    if (!rawCookiesImplemented)
        m_frontend->didGetCookies(callId, m_frontend->newScriptArray(), stringCookiesList);
    else
        m_frontend->didGetCookies(callId, buildArrayForCookies(rawCookiesList), String());
}
    
ScriptArray InspectorController::buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    ScriptArray cookies = m_frontend->newScriptArray();

    ListHashSet<Cookie>::iterator end = cookiesList.end();
    ListHashSet<Cookie>::iterator it = cookiesList.begin();
    for (int i = 0; it != end; ++it, i++)
        cookies.set(i, buildObjectForCookie(*it));

    return cookies;
}

ScriptObject InspectorController::buildObjectForCookie(const Cookie& cookie)
{
    ScriptObject value = m_frontend->newScriptObject();
    value.set("name", cookie.name);
    value.set("value", cookie.value);
    value.set("domain", cookie.domain);
    value.set("path", cookie.path);
    value.set("expires", cookie.expires);
    value.set("size", (cookie.name.length() + cookie.value.length()));
    value.set("httpOnly", cookie.httpOnly);
    value.set("secure", cookie.secure);
    value.set("session", cookie.session);
    return value;
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

void InspectorController::getDOMStorageEntries(long callId, long storageId)
{
    if (!m_frontend)
        return;

    ScriptArray jsonArray = m_frontend->newScriptArray();
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        storageResource->startReportingChangesToFrontend();
        Storage* domStorage = storageResource->domStorage();
        for (unsigned i = 0; i < domStorage->length(); ++i) {
            String name(domStorage->key(i));
            String value(domStorage->getItem(name));
            ScriptArray entry = m_frontend->newScriptArray();
            entry.set(0, name);
            entry.set(1, value);
            jsonArray.set(i, entry);
        }
    }
    m_frontend->didGetDOMStorageEntries(callId, jsonArray);
}

void InspectorController::setDOMStorageItem(long callId, long storageId, const String& key, const String& value)
{
    if (!m_frontend)
        return;

    bool success = false;
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        ExceptionCode exception = 0;
        storageResource->domStorage()->setItem(key, value, exception);
        success = !exception;
    }
    m_frontend->didSetDOMStorageItem(callId, success);
}

void InspectorController::removeDOMStorageItem(long callId, long storageId, const String& key)
{
    if (!m_frontend)
        return;

    bool success = false;
    InspectorDOMStorageResource* storageResource = getDOMStorageResourceForId(storageId);
    if (storageResource) {
        storageResource->domStorage()->removeItem(key);
        success = true;
    }
    m_frontend->didRemoveDOMStorageItem(callId, success);
}

InspectorDOMStorageResource* InspectorController::getDOMStorageResourceForId(long storageId)
{
    DOMStorageResourcesMap::iterator it = m_domStorageResources.find(storageId);
    if (it == m_domStorageResources.end())
        return 0;
    return it->second.get();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    if (!enabled())
        return;

    RefPtr<ScriptProfile> profile = prpProfile;
    m_profiles.add(profile->uid(), profile);

    if (m_frontend) {
#if USE(JSC)
        JSC::JSLock lock(JSC::SilenceAssertionsOnly);
#endif
        m_frontend->addProfileHeader(createProfileHeader(*profile));
    }

    addProfileFinishedMessageToConsole(profile, lineNumber, sourceURL);
}

void InspectorController::addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    RefPtr<ScriptProfile> profile = prpProfile;

#if USE(JSC)
    String title = ustringToString(profile->title());
#else
    String title = profile->title();
#endif
    String message = String::format("Profile \"webkit-profile://%s/%s#%d\" finished.", CPUProfileType, encodeWithURLEscapeSequences(title).utf8().data(), profile->uid());
    addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, message, lineNumber, sourceURL);
}

void InspectorController::addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL)
{
    String message = String::format("Profile \"webkit-profile://%s/%s#0\" started.", CPUProfileType, encodeWithURLEscapeSequences(title).utf8().data());
    addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, message, lineNumber, sourceURL);
}

void InspectorController::getProfileHeaders(long callId)
{
    if (!m_frontend)
        return;
    ScriptArray result = m_frontend->newScriptArray();
    ProfilesMap::iterator profilesEnd = m_profiles.end();
    int i = 0;
    for (ProfilesMap::iterator it = m_profiles.begin(); it != profilesEnd; ++it)
        result.set(i++, createProfileHeader(*it->second));
    m_frontend->didGetProfileHeaders(callId, result);
}

void InspectorController::getProfile(long callId, unsigned uid)
{
    if (!m_frontend)
        return;
    ProfilesMap::iterator it = m_profiles.find(uid);
    if (it != m_profiles.end())
#if USE(JSC)
        m_frontend->didGetProfile(callId, toJS(m_frontend->scriptState(), it->second.get()));
#else
        m_frontend->didGetProfile(callId, toV8(it->second.get()));
#endif
}

ScriptObject InspectorController::createProfileHeader(const ScriptProfile& profile)
{
#if USE(JSC)
    String title = ustringToString(profile.title());
#else
    String title = profile.title();
#endif

    ScriptObject header = m_frontend->newScriptObject();
    header.set("title", title);
    header.set("uid", profile.uid());
    header.set("typeId", String(CPUProfileType));
    return header;
}

String InspectorController::getCurrentUserInitiatedProfileName(bool incrementProfileNumber = false)
{
    if (incrementProfileNumber)
        m_currentUserInitiatedProfileNumber = m_nextUserInitiatedProfileNumber++;        

    return String::format("%s.%d", UserInitiatedProfileName, m_currentUserInitiatedProfileNumber);
}

void InspectorController::startUserInitiatedProfilingSoon()
{
    m_startProfiling.startOneShot(0);
}

void InspectorController::startUserInitiatedProfiling(Timer<InspectorController>*)
{
    if (!enabled())
        return;

    if (!profilerEnabled()) {
        enableProfiler(false, true);
        ScriptDebugServer::shared().recompileAllJSFunctions();
    }

    m_recordingUserInitiatedProfile = true;

    String title = getCurrentUserInitiatedProfileName(true);

#if USE(JSC)
    JSC::ExecState* scriptState = toJSDOMWindow(m_inspectedPage->mainFrame(), debuggerWorld())->globalExec();
#else
    ScriptState* scriptState = 0;
#endif
    ScriptProfiler::start(scriptState, title);

    addStartProfilingMessageToConsole(title, 0, String());

    toggleRecordButton(true);
}

void InspectorController::stopUserInitiatedProfiling()
{
    if (!enabled())
        return;

    m_recordingUserInitiatedProfile = false;

    String title = getCurrentUserInitiatedProfileName();

#if USE(JSC)
    JSC::ExecState* scriptState = toJSDOMWindow(m_inspectedPage->mainFrame(), debuggerWorld())->globalExec();
#else
    ScriptState* scriptState = 0;
#endif
    RefPtr<ScriptProfile> profile = ScriptProfiler::stop(scriptState, title);
    if (profile)
        addProfile(profile, 0, String());

    toggleRecordButton(false);
}

void InspectorController::toggleRecordButton(bool isProfiling)
{
    if (!m_frontend)
        return;
    m_frontend->setRecordingProfile(isProfiling);
}

void InspectorController::enableProfiler(bool always, bool skipRecompile)
{
    if (always)
        setSetting(profilerEnabledSettingName, "true");

    if (m_profilerEnabled)
        return;

    m_profilerEnabled = true;

    if (!skipRecompile)
        ScriptDebugServer::shared().recompileAllJSFunctionsSoon();

    if (m_frontend)
        m_frontend->profilerWasEnabled();
}

void InspectorController::disableProfiler(bool always)
{
    if (always)
        setSetting(profilerEnabledSettingName, "false");

    if (!m_profilerEnabled)
        return;

    m_profilerEnabled = false;

    ScriptDebugServer::shared().recompileAllJSFunctionsSoon();

    if (m_frontend)
        m_frontend->profilerWasDisabled();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::enableDebuggerFromFrontend(bool always)
{
    if (always)
        setSetting(debuggerEnabledSettingName, "true");

    ASSERT(m_inspectedPage);

    ScriptDebugServer::shared().addListener(this, m_inspectedPage);
    ScriptDebugServer::shared().clearBreakpoints();

    m_debuggerEnabled = true;
    m_frontend->debuggerWasEnabled();
}

void InspectorController::enableDebugger()
{
    if (!enabled())
        return;

    if (m_debuggerEnabled)
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

    ScriptDebugServer::shared().removeListener(this, m_inspectedPage);

    m_debuggerEnabled = false;
    m_attachDebuggerWhenShown = false;

    if (m_frontend)
        m_frontend->debuggerWasDisabled();
}

void InspectorController::resumeDebugger()
{
    if (!m_debuggerEnabled)
        return;
    ScriptDebugServer::shared().continueProgram();
}

void InspectorController::setBreakpoint(const String& sourceID, unsigned lineNumber, bool enabled, const String& condition)
{
    ScriptBreakpoint breakpoint(enabled, condition);
    ScriptDebugServer::shared().setBreakpoint(sourceID, lineNumber, breakpoint);
    String url = m_sourceIDToURL.get(sourceID);
    if (url.isEmpty())
        return;

    HashMap<String, SourceBreakpoints>::iterator it = m_stickyBreakpoints.find(url);
    if (it == m_stickyBreakpoints.end())
        it = m_stickyBreakpoints.set(url, SourceBreakpoints()).first;
    it->second.set(lineNumber, breakpoint);
}

void InspectorController::removeBreakpoint(const String& sourceID, unsigned lineNumber)
{
    ScriptDebugServer::shared().removeBreakpoint(sourceID, lineNumber);
 
    String url = m_sourceIDToURL.get(sourceID);
    if (url.isEmpty())
        return;

    HashMap<String, SourceBreakpoints>::iterator it = m_stickyBreakpoints.find(url);
    if (it != m_stickyBreakpoints.end())
        it->second.remove(lineNumber);
}

// JavaScriptDebugListener functions

void InspectorController::didParseSource(const String& sourceID, const String& url, const String& data, int firstLine)
{
    m_frontend->parsedScriptSource(sourceID, url, data, firstLine);

    if (url.isEmpty())
        return;

    HashMap<String, SourceBreakpoints>::iterator it = m_stickyBreakpoints.find(url);
    if (it != m_stickyBreakpoints.end()) {
        for (SourceBreakpoints::iterator breakpointIt = it->second.begin(); breakpointIt != it->second.end(); ++breakpointIt) {
            if (firstLine <= breakpointIt->first) {
                ScriptDebugServer::shared().setBreakpoint(sourceID, breakpointIt->first, breakpointIt->second);
                m_frontend->restoredBreakpoint(sourceID, url, breakpointIt->first, breakpointIt->second.enabled, breakpointIt->second.condition);
            }
        }
    }

    m_sourceIDToURL.set(sourceID, url);
}

void InspectorController::failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage)
{
    m_frontend->failedToParseScriptSource(url, data, firstLine, errorLine, errorMessage);
}

void InspectorController::didPause()
{
    ScriptState* scriptState = ScriptDebugServer::shared().currentCallFrameState();
    ASSERT(scriptState);
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptFor(scriptState);
    RefPtr<SerializedScriptValue> callFrames = injectedScript.callFrames();
    m_frontend->pausedScript(callFrames.get());
}

void InspectorController::didContinue()
{
    m_frontend->resumedScript();
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

static void drawHighlightForLineBoxes(GraphicsContext& context, const Vector<FloatQuad>& lineBoxQuads)
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

    if (renderer->isBox()) {
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
    } else if (renderer->isRenderInline()) {
        RenderInline* renderInline = toRenderInline(renderer);

        // FIXME: We should show margins/padding/border for inlines.
        Vector<FloatQuad> lineBoxQuads;
        renderInline->absoluteQuads(lineBoxQuads);
        for (unsigned i = 0; i < lineBoxQuads.size(); ++i)
            lineBoxQuads[i] += mainFrameOffset;

        drawHighlightForLineBoxes(context, lineBoxQuads);
    }
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

InspectorController::SpecialPanels InspectorController::specialPanelForJSName(const String& panelName)
{
    if (panelName == "elements")
        return ElementsPanel;
    if (panelName == "resources")
        return ResourcesPanel;
    if (panelName == "scripts")
        return ScriptsPanel;
    if (panelName == "timeline")
        return TimelinePanel;
    if (panelName == "profiles")
        return ProfilesPanel;
    if (panelName == "storage" || panelName == "databases")
        return StoragePanel;
    if (panelName == "audits")
        return AuditsPanel;
    if (panelName == "console")
        return ConsolePanel;
    return ElementsPanel;
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

} // namespace WebCore
    
#endif // ENABLE(INSPECTOR)
