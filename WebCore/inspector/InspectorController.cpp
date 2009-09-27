/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "CString.h"
#include "CachedResource.h"
#include "Console.h"
#include "ConsoleMessage.h"
#include "CookieJar.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DOMWindow.h"
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
#include "InspectorBackend.h"
#include "InspectorClient.h"
#include "InspectorFrontend.h"
#include "InspectorDatabaseResource.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorTimelineAgent.h"
#include "InspectorResource.h"
#include "JavaScriptProfile.h"
#include "Page.h"
#include "Range.h"
#include "RenderInline.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptCallStack.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include "ScriptObjectQuarantine.h"
#include "ScriptString.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include <wtf/CurrentTime.h>
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
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugServer.h"
#include "JSJavaScriptCallFrame.h"

#include <profiler/Profile.h>
#include <profiler/Profiler.h>
#include <runtime/JSLock.h>
#include <runtime/UString.h>

using namespace JSC;
#endif
using namespace std;

namespace WebCore {

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";
static const char* const resourceTrackingEnabledSettingName = "resourceTrackingEnabled";
static const char* const debuggerEnabledSettingName = "debuggerEnabled";
static const char* const profilerEnabledSettingName = "profilerEnabled";
static const char* const inspectorAttachedHeightName = "inspectorAttachedHeight";
static const char* const lastActivePanelSettingName = "lastActivePanel";
static const char* const timelineEnabledSettingName = "timelineEnabled";

static const unsigned defaultAttachedHeight = 300;
static const float minimumAttachedHeight = 250.0f;
static const float maximumAttachedHeightRatio = 0.75f;

static unsigned s_inspectorControllerCount;
static HashMap<String, InspectorController::Setting*>* s_settingCache;

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_page(0)
    , m_scriptState(0)
    , m_windowVisible(false)
    , m_showAfterVisible(CurrentPanel)
    , m_nextIdentifier(-2)
    , m_groupLevel(0)
    , m_searchingForNode(false)
    , m_previousMessage(0)
    , m_resourceTrackingEnabled(false)
    , m_resourceTrackingSettingsLoaded(false)
    , m_inspectorBackend(InspectorBackend::create(this, client))
    , m_lastBoundObjectId(1)
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_debuggerEnabled(false)
    , m_attachDebuggerWhenShown(false)
    , m_profilerEnabled(false)
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
    ASSERT(!m_scriptState);
    ASSERT(!m_inspectedPage);
    ASSERT(!m_page || (m_page && !m_page->parentInspectorController()));

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);

    ASSERT(s_inspectorControllerCount);
    --s_inspectorControllerCount;

    if (!s_inspectorControllerCount && s_settingCache) {
        deleteAllValues(*s_settingCache);
        delete s_settingCache;
        s_settingCache = 0;
    }
    
    m_inspectorBackend->disconnectController();
}

void InspectorController::inspectedPageDestroyed()
{
    close();

    if (m_scriptState)
        ScriptGlobalObject::remove(m_scriptState, "InspectorController");

    if (m_page) {
        m_page->setParentInspectorController(0);
        m_page = 0;
    }

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

const InspectorController::Setting& InspectorController::setting(const String& key) const
{
    if (!s_settingCache)
        s_settingCache = new HashMap<String, Setting*>;

    if (Setting* cachedSetting = s_settingCache->get(key))
        return *cachedSetting;

    Setting* newSetting = new Setting;
    s_settingCache->set(key, newSetting);

    m_client->populateSetting(key, *newSetting);

    return *newSetting;
}

void InspectorController::setSetting(const String& key, const Setting& setting)
{
    if (setting.type() == Setting::NoType) {
        if (s_settingCache) {
            Setting* cachedSetting = s_settingCache->get(key);
            if (cachedSetting) {
                s_settingCache->remove(key);
                delete cachedSetting;
            }
        }

        m_client->removeSetting(key);
        return;
    }

    if (!s_settingCache)
        s_settingCache = new HashMap<String, Setting*>;

    if (Setting* cachedSetting = s_settingCache->get(key))
        *cachedSetting = setting;
    else
        s_settingCache->set(key, new Setting(setting));

    m_client->storeSetting(key, setting);
}

// Trying to inspect something in a frame with JavaScript disabled would later lead to
// crashes trying to create JavaScript wrappers. Some day we could fix this issue, but
// for now prevent crashes here by never targeting a node in such a frame.
static bool canPassNodeToJavaScript(Node* node)
{
    if (!node)
        return false;
    Frame* frame = node->document()->frame();
    return frame && frame->script()->isEnabled();
}

void InspectorController::inspect(Node* node)
{
    if (!canPassNodeToJavaScript(node) || !enabled())
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
    return m_windowVisible;
}

void InspectorController::setWindowVisible(bool visible, bool attached)
{
    if (visible == m_windowVisible || !m_frontend)
        return;

    m_windowVisible = visible;

    if (m_windowVisible) {
        setAttachedWindow(attached);
        populateScriptObjects();

        // Console panel is implemented as a 'fast view', so there should be
        // real panel opened along with it.
        bool showConsole = m_showAfterVisible == ConsolePanel;
        if (m_showAfterVisible == CurrentPanel || showConsole) {
          Setting lastActivePanelSetting = setting(lastActivePanelSettingName);
          if (lastActivePanelSetting.type() == Setting::StringType)
              m_showAfterVisible = specialPanelForJSName(lastActivePanelSetting.string());
          else
              m_showAfterVisible = ElementsPanel;
        }

        if (m_nodeToFocus)
            focusNode();
#if ENABLE(JAVASCRIPT_DEBUGGER)
        if (m_attachDebuggerWhenShown)
            enableDebugger();
#endif
        showPanel(m_showAfterVisible);
        if (showConsole)
            showPanel(ConsolePanel);
    } else {
#if ENABLE(JAVASCRIPT_DEBUGGER)
        // If the window is being closed with the debugger enabled,
        // remember this state to re-enable debugger on the next window
        // opening.
        bool debuggerWasEnabled = m_debuggerEnabled;
        disableDebugger();
        if (debuggerWasEnabled)
            m_attachDebuggerWhenShown = true;
#endif
        resetScriptObjects();
    }
    m_showAfterVisible = CurrentPanel;
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
    } else {
        m_previousMessage = consoleMessage;
        m_consoleMessages.append(consoleMessage);
    }

    if (m_frontend)
        m_previousMessage->addToConsole(m_frontend.get());
}

void InspectorController::clearConsoleMessages(bool clearUI)
{
    deleteAllValues(m_consoleMessages);
    m_consoleMessages.clear();
    m_previousMessage = 0;
    m_groupLevel = 0;
    m_idToConsoleObject.clear();
    if (m_domAgent)
        m_domAgent->releaseDanglingNodes();
    if (clearUI && m_frontend)
        m_frontend->clearConsoleMessages();
}

void InspectorController::startGroup(MessageSource source, ScriptCallStack* callStack)
{
    ++m_groupLevel;

    addConsoleMessage(callStack->state(), new ConsoleMessage(source, StartGroupMessageType, LogMessageLevel, callStack, m_groupLevel));
}

void InspectorController::endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL)
{
    if (m_groupLevel == 0)
        return;

    --m_groupLevel;

    addConsoleMessage(0, new ConsoleMessage(source, EndGroupMessageType, LogMessageLevel, String(), lineNumber, sourceURL, m_groupLevel));
}

static unsigned constrainedAttachedWindowHeight(unsigned preferredHeight, unsigned totalWindowHeight)
{
    return roundf(max(minimumAttachedHeight, min<float>(preferredHeight, totalWindowHeight * maximumAttachedHeightRatio)));
}

void InspectorController::attachWindow()
{
    if (!enabled())
        return;

    unsigned inspectedPageHeight = m_inspectedPage->mainFrame()->view()->visibleHeight();

    m_client->attachWindow();

    Setting attachedHeight = setting(inspectorAttachedHeightName);
    unsigned preferredHeight = attachedHeight.type() == Setting::IntegerType ? attachedHeight.integerValue() : defaultAttachedHeight;

    // We need to constrain the window height here in case the user has resized the inspected page's window so that
    // the user's preferred height would be too big to display.
    m_client->setAttachedWindowHeight(constrainedAttachedWindowHeight(preferredHeight, inspectedPageHeight));
}

void InspectorController::detachWindow()
{
    if (!enabled())
        return;
    m_client->detachWindow();
}

void InspectorController::setAttachedWindow(bool attached)
{
    if (!enabled() || !m_frontend)
        return;

    m_frontend->setAttachedWindow(attached);
}

void InspectorController::setAttachedWindowHeight(unsigned height)
{
    if (!enabled())
        return;
    
    unsigned totalHeight = m_page->mainFrame()->view()->visibleHeight() + m_inspectedPage->mainFrame()->view()->visibleHeight();
    unsigned attachedHeight = constrainedAttachedWindowHeight(height, totalHeight);
    
    setSetting(inspectorAttachedHeightName, Setting(attachedHeight));
    
    m_client->setAttachedWindowHeight(attachedHeight);
}

void InspectorController::storeLastActivePanel(const String& panelName)
{
    setSetting(lastActivePanelSettingName, Setting(panelName));
}

void InspectorController::toggleSearchForNodeInPage()
{
    if (!enabled())
        return;

    m_searchingForNode = !m_searchingForNode;
    if (!m_searchingForNode)
        hideHighlight();
}

void InspectorController::mouseDidMoveOverElement(const HitTestResult& result, unsigned)
{
    if (!enabled() || !m_searchingForNode)
        return;

    Node* node = result.innerNode();
    if (node)
        highlight(node);
}

void InspectorController::handleMousePressOnNode(Node* node)
{
    if (!enabled())
        return;

    ASSERT(m_searchingForNode);
    ASSERT(node);
    if (!node)
        return;

    // inspect() will implicitly call ElementsPanel's focusedNodeChanged() and the hover feedback will be stopped there.
    inspect(node);
}

void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame)
{
    if (!enabled() || !m_frontend || frame != m_inspectedPage->mainFrame())
        return;
    resetInjectedScript();
}

void InspectorController::windowScriptObjectAvailable()
{
    if (!m_page || !enabled())
        return;

    // Grant the inspector the ability to script the inspected page.
    m_page->mainFrame()->document()->securityOrigin()->grantUniversalAccess();
    m_scriptState = scriptStateFromPage(m_page);
    ScriptGlobalObject::set(m_scriptState, "InspectorController", m_inspectorBackend.get());
}

void InspectorController::scriptObjectReady()
{
    ASSERT(m_scriptState);
    if (!m_scriptState)
        return;

    ScriptObject webInspectorObj;
    if (!ScriptGlobalObject::get(m_scriptState, "WebInspector", webInspectorObj))
        return;
    ScriptObject injectedScriptObj;
    if (!ScriptGlobalObject::get(m_scriptState, "InjectedScript", injectedScriptObj))
        return;
    setFrontendProxyObject(m_scriptState, webInspectorObj, injectedScriptObj);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    Setting debuggerEnabled = setting(debuggerEnabledSettingName);
    if (debuggerEnabled.type() == Setting::BooleanType && debuggerEnabled.booleanValue())
        enableDebugger();
    Setting profilerEnabled = setting(profilerEnabledSettingName);
    if (profilerEnabled.type() == Setting::BooleanType && profilerEnabled.booleanValue())
        enableProfiler();
#endif

    // Make sure our window is visible now that the page loaded
    showWindow();

    m_client->inspectorWindowObjectCleared();
}

void InspectorController::setFrontendProxyObject(ScriptState* scriptState, ScriptObject webInspectorObj, ScriptObject injectedScriptObj)
{
    m_scriptState = scriptState;
    m_injectedScriptObj = injectedScriptObj;
    m_frontend.set(new InspectorFrontend(this, scriptState, webInspectorObj));
    m_domAgent = new InspectorDOMAgent(m_frontend.get());

    Setting timelineEnabled = setting(timelineEnabledSettingName);
    if (timelineEnabled.type() == Setting::BooleanType && timelineEnabled.booleanValue())
        m_timelineAgent = new InspectorTimelineAgent(m_frontend.get());
}

void InspectorController::show()
{
    if (!enabled())
        return;
    
    if (!m_page) {
        if (m_frontend)
            return;  // We are using custom frontend - no need to create page.

        m_page = m_client->createPage();
        if (!m_page)
            return;
        m_page->setParentInspectorController(this);

        // showWindow() will be called after the page loads in scriptObjectReady()
        return;
    }

    showWindow();
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
    if (!enabled())
        return;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    stopUserInitiatedProfiling();
    disableDebugger();
#endif
    closeWindow();

    m_frontend.set(0);
    m_injectedScriptObj = ScriptObject();
    // m_domAgent is RefPtr. Remove DOM listeners first to ensure that there are
    // no references to the DOM agent from the DOM tree.
    if (m_domAgent)
        m_domAgent->setDocument(0);
    m_domAgent = 0;
    m_timelineAgent = 0;
    m_scriptState = 0;
}

void InspectorController::showWindow()
{
    ASSERT(enabled());

    unsigned inspectedPageHeight = m_inspectedPage->mainFrame()->view()->visibleHeight();

    m_client->showWindow();

    Setting attachedHeight = setting(inspectorAttachedHeightName);
    unsigned preferredHeight = attachedHeight.type() == Setting::IntegerType ? attachedHeight.integerValue() : defaultAttachedHeight;

    // This call might not go through (if the window starts out detached), but if the window is initially created attached,
    // InspectorController::attachWindow is never called, so we need to make sure to set the attachedWindowHeight.
    // FIXME: Clean up code so we only have to call setAttachedWindowHeight in InspectorController::attachWindow
    m_client->setAttachedWindowHeight(constrainedAttachedWindowHeight(preferredHeight, inspectedPageHeight));
}

void InspectorController::closeWindow()
{
    m_client->closeWindow();
}

void InspectorController::populateScriptObjects()
{
    ASSERT(m_frontend);
    if (!m_frontend)
        return;

    m_domAgent->setDocument(m_inspectedPage->mainFrame()->document());

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        it->second->createScriptObject(m_frontend.get());

    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        m_consoleMessages[i]->addToConsole(m_frontend.get());

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it)
        (*it)->bind(m_frontend.get());
#endif
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        (*it)->bind(m_frontend.get());
#endif

    m_frontend->populateInterface();
}

void InspectorController::resetScriptObjects()
{
    if (!m_frontend)
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        it->second->releaseScriptObject(m_frontend.get(), false);

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it)
        (*it)->unbind();
#endif
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        (*it)->unbind();
#endif

    if (m_timelineAgent)
        m_timelineAgent->reset();

    m_frontend->reset();
    m_domAgent->setDocument(0);
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
                resource->releaseScriptObject(m_frontend.get(), true);
        }
    }
}

void InspectorController::didCommitLoad(DocumentLoader* loader)
{
    if (!enabled())
        return;

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        m_client->inspectedURLChanged(loader->url().string());

        clearConsoleMessages(false);

        m_times.clear();
        m_counts.clear();
#if ENABLE(JAVASCRIPT_DEBUGGER)
        m_profiles.clear();
        m_currentUserInitiatedProfileNumber = 1;
        m_nextUserInitiatedProfileNumber = 1;
#endif
        // resetScriptObjects should be called before database and DOM storage
        // resources are cleared so that it has a chance to unbind them.
        resetScriptObjects();
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
                m_mainResource->createScriptObject(m_frontend.get());
            } else {
                // Pages loaded from the page cache are committed before
                // m_mainResource is the right resource for this load, so we
                // clear it here. It will be re-assigned in
                // identifierForInitialRequest.
                m_mainResource = 0;
            }
            if (windowVisible())
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

InspectorResource* InspectorController::getTrackedResource(long long identifier)
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
    
    RefPtr<InspectorResource> resource = InspectorResource::createCached(m_nextIdentifier--, loader, cachedResource);

    if (isMainResource) {
        m_mainResource = resource;
        resource->markMainResource();
    }

    addResource(resource.get());

    if (m_frontend)
        resource->createScriptObject(m_frontend.get());
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

    RefPtr<InspectorResource> resource = InspectorResource::create(identifier, loader);

    resource->updateRequest(request);

    if (isMainResource) {
        m_mainResource = resource;
        resource->markMainResource();
    }

    addResource(resource.get());

    if (m_frontend && loader->frameLoader()->isLoadingFromCachedPage() && resource == m_mainResource)
        resource->createScriptObject(m_frontend.get());
}

bool InspectorController::isMainResourceLoader(DocumentLoader* loader, const KURL& requestUrl)
{
    return loader->frame() == m_inspectedPage->mainFrame() && requestUrl == loader->requestURL();
}

void InspectorController::willSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->startTiming();

    if (!redirectResponse.isNull()) {
        resource->updateRequest(request);
        resource->updateResponse(redirectResponse);
    }

    if (resource != m_mainResource && m_frontend)
        resource->createScriptObject(m_frontend.get());
}

void InspectorController::didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse& response)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->updateResponse(response);
    resource->markResponseReceivedTime();

    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    resource->addLength(lengthReceived);

    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didFinishLoading(DocumentLoader*, unsigned long identifier)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->endTiming();

    addResource(resource.get());

    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::didFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError& /*error*/)
{
    RefPtr<InspectorResource> resource = getTrackedResource(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->markFailed();
    resource->endTiming();

    addResource(resource.get());

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

    resource->setXMLHttpResponseText(sourceString);

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
    
    // FIXME: imported script and XHR response are currently viewed as the same
    // thing by the Inspector. They should be made into distinct types.
    resource->setXMLHttpResponseText(ScriptString(sourceString));
    
    if (m_frontend)
        resource->updateScriptObject(m_frontend.get());
}

void InspectorController::enableResourceTracking(bool always)
{
    if (!enabled())
        return;

    if (always)
        setSetting(resourceTrackingEnabledSettingName, Setting(true));

    if (m_resourceTrackingEnabled)
        return;

    ASSERT(m_inspectedPage);
    m_resourceTrackingEnabled = true;
    if (m_frontend)
        m_frontend->resourceTrackingWasEnabled();

    m_inspectedPage->mainFrame()->loader()->reload();
}

void InspectorController::disableResourceTracking(bool always)
{
    if (!enabled())
        return;

    if (always)
        setSetting(resourceTrackingEnabledSettingName, Setting(false));

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

    Setting resourceTracking = setting(resourceTrackingEnabledSettingName);
    if (resourceTracking.type() == Setting::BooleanType && resourceTracking.booleanValue())
        m_resourceTrackingEnabled = true;
}

void InspectorController::enableTimeline(bool always)
{
    if (!enabled())
        return;

    if (always)
        setSetting(timelineEnabledSettingName, Setting(true));

    if (m_timelineAgent.get())
        return;

    m_timelineAgent = new InspectorTimelineAgent(m_frontend.get());
    if (m_frontend)
        m_frontend->timelineWasEnabled();
}

void InspectorController::disableTimeline(bool always)
{
    if (!enabled())
        return;

    if (always)
        setSetting(timelineEnabledSettingName, Setting(false));

    if (!m_timelineAgent.get())
        return;

    m_timelineAgent.set(0);
    if (m_frontend)
        m_frontend->timelineWasDisabled();
}

bool InspectorController::timelineEnabled() const
{
    if (!enabled())
        return false;

    return m_timelineAgent.get();
}

#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(Database* database, const String& domain, const String& name, const String& version)
{
    if (!enabled())
        return;

    RefPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);

    m_databaseResources.add(resource);

    if (m_frontend)
        resource->bind(m_frontend.get());
}
#endif

#if ENABLE(DOM_STORAGE)
void InspectorController::didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
    if (!enabled())
        return;

    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        if ((*it)->isSameHostAndType(frame, isLocalStorage))
            return;

    RefPtr<Storage> domStorage = Storage::create(frame, storageArea);
    RefPtr<InspectorDOMStorageResource> resource = InspectorDOMStorageResource::create(domStorage.get(), isLocalStorage, frame);

    m_domStorageResources.add(resource);

    if (m_frontend)
        resource->bind(m_frontend.get());
}

void InspectorController::selectDOMStorage(Storage* storage)
{
    ASSERT(storage);
    if (!m_frontend)
        return;

    Frame* frame = storage->frame();
    bool isLocalStorage = (frame->domWindow()->localStorage() == storage);
    int storageResourceId = 0;
    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it) {
        if ((*it)->isSameHostAndType(frame, isLocalStorage)) {
            storageResourceId = (*it)->id();
            break;
        }
    }
    if (storageResourceId)
        m_frontend->selectDOMStorage(storageResourceId);
}

void InspectorController::getDOMStorageEntries(int callId, int storageId)
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
        success = (exception == 0);
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

InspectorDOMStorageResource* InspectorController::getDOMStorageResourceForId(int storageId)
{
    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        if ((*it)->id() == storageId)
            return it->get();
    return 0;
}
#endif

void InspectorController::moveWindowBy(float x, float y) const
{
    if (!m_page || !enabled())
        return;

    FloatRect frameRect = m_page->chrome()->windowRect();
    frameRect.move(x, y);
    m_page->chrome()->setWindowRect(frameRect);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::addProfile(PassRefPtr<Profile> prpProfile, unsigned lineNumber, const UString& sourceURL)
{
    if (!enabled())
        return;

    RefPtr<Profile> profile = prpProfile;
    m_profiles.append(profile);

    if (m_frontend) {
        JSLock lock(SilenceAssertionsOnly);
        m_frontend->addProfile(toJS(m_scriptState, profile.get()));
    }

    addProfileFinishedMessageToConsole(profile, lineNumber, sourceURL);
}

void InspectorController::addProfileFinishedMessageToConsole(PassRefPtr<Profile> prpProfile, unsigned lineNumber, const UString& sourceURL)
{
    RefPtr<Profile> profile = prpProfile;

    UString message = "Profile \"webkit-profile://";
    message += encodeWithURLEscapeSequences(profile->title());
    message += "/";
    message += UString::from(profile->uid());
    message += "\" finished.";
    addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, message, lineNumber, sourceURL);
}

void InspectorController::addStartProfilingMessageToConsole(const UString& title, unsigned lineNumber, const UString& sourceURL)
{
    UString message = "Profile \"webkit-profile://";
    message += encodeWithURLEscapeSequences(title);
    message += "/0\" started.";
    addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, message, lineNumber, sourceURL);
}

UString InspectorController::getCurrentUserInitiatedProfileName(bool incrementProfileNumber = false)
{
    if (incrementProfileNumber)
        m_currentUserInitiatedProfileNumber = m_nextUserInitiatedProfileNumber++;        

    UString title = UserInitiatedProfileName;
    title += ".";
    title += UString::from(m_currentUserInitiatedProfileNumber);
    
    return title;
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
        JavaScriptDebugServer::shared().recompileAllJSFunctions();
    }

    m_recordingUserInitiatedProfile = true;

    UString title = getCurrentUserInitiatedProfileName(true);

    ExecState* scriptState = toJSDOMWindow(m_inspectedPage->mainFrame())->globalExec();
    Profiler::profiler()->startProfiling(scriptState, title);

    addStartProfilingMessageToConsole(title, 0, UString());

    toggleRecordButton(true);
}

void InspectorController::stopUserInitiatedProfiling()
{
    if (!enabled())
        return;

    m_recordingUserInitiatedProfile = false;

    UString title = getCurrentUserInitiatedProfileName();

    ExecState* scriptState = toJSDOMWindow(m_inspectedPage->mainFrame())->globalExec();
    RefPtr<Profile> profile = Profiler::profiler()->stopProfiling(scriptState, title);
    if (profile)
        addProfile(profile, 0, UString());

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
        setSetting(profilerEnabledSettingName, Setting(true));

    if (m_profilerEnabled)
        return;

    m_profilerEnabled = true;

    if (!skipRecompile)
        JavaScriptDebugServer::shared().recompileAllJSFunctionsSoon();

    if (m_frontend)
        m_frontend->profilerWasEnabled();
}

void InspectorController::disableProfiler(bool always)
{
    if (always)
        setSetting(profilerEnabledSettingName, Setting(false));

    if (!m_profilerEnabled)
        return;

    m_profilerEnabled = false;

    JavaScriptDebugServer::shared().recompileAllJSFunctionsSoon();

    if (m_frontend)
        m_frontend->profilerWasDisabled();
}

void InspectorController::enableDebuggerFromFrontend(bool always)
{
    if (always)
        setSetting(debuggerEnabledSettingName, Setting(true));

    ASSERT(m_inspectedPage);

    JavaScriptDebugServer::shared().addListener(this, m_inspectedPage);
    JavaScriptDebugServer::shared().clearBreakpoints();

    m_debuggerEnabled = true;
    m_frontend->debuggerWasEnabled();
}

void InspectorController::enableDebugger()
{
    if (!enabled())
        return;

    if (m_debuggerEnabled)
        return;

    if (!m_scriptState || !m_frontend) {
        m_attachDebuggerWhenShown = true;
    } else {
        m_frontend->attachDebuggerWhenShown();
        m_attachDebuggerWhenShown = false;
    }
}

void InspectorController::disableDebugger(bool always)
{
    if (!enabled())
        return;

    if (always)
        setSetting(debuggerEnabledSettingName, Setting(false));

    ASSERT(m_inspectedPage);

    JavaScriptDebugServer::shared().removeListener(this, m_inspectedPage);

    m_debuggerEnabled = false;
    m_attachDebuggerWhenShown = false;

    if (m_frontend)
        m_frontend->debuggerWasDisabled();
}

void InspectorController::resumeDebugger()
{
    if (!m_debuggerEnabled)
        return;
    JavaScriptDebugServer::shared().continueProgram();
}

// JavaScriptDebugListener functions

void InspectorController::didParseSource(ExecState*, const SourceCode& source)
{
    m_frontend->parsedScriptSource(source);
}

void InspectorController::failedToParseSource(ExecState*, const SourceCode& source, int errorLine, const UString& errorMessage)
{
    m_frontend->failedToParseScriptSource(source, errorLine, errorMessage);
}

void InspectorController::didPause()
{
    ScriptFunctionCall function(m_scriptState, m_injectedScriptObj, "getCallFrames");
    ScriptValue callFrames = function.call();
    m_frontend->pausedScript(callFrames);
}

void InspectorController::didContinue()
{
    m_frontend->resumedScript();
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
        context.setStrokeColor(outlineColor);
        context.strokePath();

        context.restore();
    }
    
    // Now do the fill
    context.addPath(quadPath);
    context.setFillColor(fillColor);
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
    else if (panelName == "resources")
        return ResourcesPanel;
    else if (panelName == "scripts")
        return ScriptsPanel;
    else if (panelName == "profiles")
        return ProfilesPanel;
    else if (panelName == "storage" || panelName == "databases")
        return StoragePanel;
    else
        return ElementsPanel;
}

ScriptValue InspectorController::wrapObject(const ScriptValue& quarantinedObject)
{
    ScriptFunctionCall function(m_scriptState, m_injectedScriptObj, "createProxyObject");
    function.appendArgument(quarantinedObject);
    if (quarantinedObject.isObject()) {
        long id = m_lastBoundObjectId++;
        String objectId = String::format("object#%ld", id);
        m_idToConsoleObject.set(objectId, quarantinedObject);

        function.appendArgument(objectId);
    }
    ScriptValue wrapper = function.call();
    return wrapper;
}

ScriptValue InspectorController::unwrapObject(const String& objectId)
{
    HashMap<String, ScriptValue>::iterator it = m_idToConsoleObject.find(objectId);
    if (it != m_idToConsoleObject.end())
        return it->second;
    return ScriptValue();
}

void InspectorController::resetInjectedScript()
{
    ScriptFunctionCall function(m_scriptState, m_injectedScriptObj, "reset");
    function.call();
}

void InspectorController::deleteCookie(const String& cookieName)
{
    Document* document = m_inspectedPage->mainFrame()->document();
    WebCore::deleteCookie(document, document->cookieURL(), cookieName);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
