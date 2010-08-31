/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Page.h"

#include "DeviceMotionController.h"
#include "BackForwardController.h"
#include "BackForwardList.h"
#include "Base64.h"
#include "CSSStyleSelector.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "ContextMenuController.h"
#include "DOMWindow.h"
#include "DeviceOrientationController.h"
#include "DragController.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FileSystem.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#include "InspectorTimelineAgent.h"
#include "Logging.h"
#include "MediaCanStartListener.h"
#include "Navigator.h"
#include "NetworkStateNotifier.h"
#include "PageGroup.h"
#include "PluginData.h"
#include "PluginHalter.h"
#include "PluginView.h"
#include "ProgressTracker.h"
#include "RenderTheme.h"
#include "RenderWidget.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SpeechInput.h"
#include "SpeechInputClient.h"
#include "TextResourceDecoder.h"
#include "Widget.h"
#include <wtf/HashMap.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHash.h>

#if ENABLE(DOM_STORAGE)
#include "StorageArea.h"
#include "StorageNamespace.h"
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "ScriptDebugServer.h"
#endif

#if ENABLE(WML)
#include "WMLPageState.h"
#endif

#if ENABLE(CLIENT_BASED_GEOLOCATION)
#include "GeolocationController.h"
#endif

#if ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "InspectorApplicationCacheAgent.h"
#endif

namespace WebCore {

static HashSet<Page*>* allPages;

#ifndef NDEBUG
static WTF::RefCountedLeakCounter pageCounter("Page");
#endif

static void networkStateChanged()
{
    Vector<RefPtr<Frame> > frames;
    
#if ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
    bool isNowOnline = networkStateNotifier().onLine();
#endif

    // Get all the frames of all the pages in all the page groups
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frames.append(frame);
#if ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
        if (InspectorApplicationCacheAgent* applicationCacheAgent = (*it)->inspectorController()->applicationCacheAgent())
            applicationCacheAgent->updateNetworkState(isNowOnline);
#endif
    }

    AtomicString eventName = networkStateNotifier().onLine() ? eventNames().onlineEvent : eventNames().offlineEvent;
    for (unsigned i = 0; i < frames.size(); i++)
        frames[i]->document()->dispatchWindowEvent(Event::create(eventName, false, false));
}

Page::Page(const PageClients& pageClients)
    : m_chrome(new Chrome(this, pageClients.chromeClient))
    , m_dragCaretController(new SelectionController(0, true))
#if ENABLE(DRAG_SUPPORT)
    , m_dragController(new DragController(this, pageClients.dragClient))
#endif
    , m_focusController(new FocusController(this))
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuController(new ContextMenuController(this, pageClients.contextMenuClient))
#endif
#if ENABLE(INSPECTOR)
    , m_inspectorController(new InspectorController(this, pageClients.inspectorClient))
#endif
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    , m_geolocationController(new GeolocationController(this, pageClients.geolocationControllerClient))
#endif
#if ENABLE(DEVICE_ORIENTATION)
    , m_deviceMotionController(RuntimeEnabledFeatures::deviceMotionEnabled() ? new DeviceMotionController(pageClients.deviceMotionClient) : 0)
    , m_deviceOrientationController(RuntimeEnabledFeatures::deviceOrientationEnabled() ? new DeviceOrientationController(this, pageClients.deviceOrientationClient) : 0)
#endif
#if ENABLE(INPUT_SPEECH)
    , m_speechInputClient(pageClients.speechInputClient)
#endif
    , m_settings(new Settings(this))
    , m_progress(new ProgressTracker)
    , m_backForwardController(new BackForwardController(this, pageClients.backForwardControllerClient))
    , m_theme(RenderTheme::themeForPage(this))
    , m_editorClient(pageClients.editorClient)
    , m_frameCount(0)
    , m_openedByDOM(false)
    , m_tabKeyCyclesThroughElements(true)
    , m_defersLoading(false)
    , m_inLowQualityInterpolationMode(false)
    , m_cookieEnabled(true)
    , m_areMemoryCacheClientCallsEnabled(true)
    , m_mediaVolume(1)
    , m_javaScriptURLsAreAllowed(true)
    , m_didLoadUserStyleSheet(false)
    , m_userStyleSheetModificationTime(0)
    , m_group(0)
    , m_debugger(0)
    , m_customHTMLTokenizerTimeDelay(-1)
    , m_customHTMLTokenizerChunkSize(-1)
    , m_canStartMedia(true)
    , m_viewMode(ViewModeWindowed)
{
    if (!allPages) {
        allPages = new HashSet<Page*>;
        
        networkStateNotifier().setNetworkStateChangedFunction(networkStateChanged);
    }

    ASSERT(!allPages->contains(this));
    allPages->add(this);

    if (pageClients.pluginHalterClient) {
        m_pluginHalter.set(new PluginHalter(pageClients.pluginHalterClient));
        m_pluginHalter->setPluginAllowedRunTime(m_settings->pluginAllowedRunTime());
    }

#if ENABLE(JAVASCRIPT_DEBUGGER)
    ScriptDebugServer::shared().pageCreated(this);
#endif

#ifndef NDEBUG
    pageCounter.increment();
#endif
}

Page::~Page()
{
    m_mainFrame->setView(0);
    setGroupName(String());
    allPages->remove(this);
    
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->pageDestroyed();

    m_editorClient->pageDestroyed();
    if (m_pluginData)
        m_pluginData->disconnectPage();
    
#if ENABLE(INSPECTOR)
    m_inspectorController->inspectedPageDestroyed();
#endif

    backForwardList()->close();

#ifndef NDEBUG
    pageCounter.decrement();

    // Cancel keepAlive timers, to ensure we release all Frames before exiting.
    // It's safe to do this because we prohibit closing a Page while JavaScript
    // is executing.
    Frame::cancelAllKeepAlive();
#endif
}

struct ViewModeInfo {
    const char* name;
    Page::ViewMode type;
};
static const int viewModeMapSize = 5;
static ViewModeInfo viewModeMap[viewModeMapSize] = {
    {"windowed", Page::ViewModeWindowed},
    {"floating", Page::ViewModeFloating},
    {"fullscreen", Page::ViewModeFullscreen},
    {"maximized", Page::ViewModeMaximized},
    {"minimized", Page::ViewModeMinimized}
};

Page::ViewMode Page::stringToViewMode(const String& text)
{
    for (int i = 0; i < viewModeMapSize; ++i) {
        if (text == viewModeMap[i].name)
            return viewModeMap[i].type;
    }
    return Page::ViewModeInvalid;
}

void Page::setViewMode(ViewMode viewMode)
{
    if (viewMode == m_viewMode || viewMode == ViewModeInvalid)
        return;

    m_viewMode = viewMode;

    if (!m_mainFrame)
        return;

    if (m_mainFrame->view())
        m_mainFrame->view()->forceLayout();

    if (m_mainFrame->document())
        m_mainFrame->document()->styleSelectorChanged(RecalcStyleImmediately);
}

void Page::setMainFrame(PassRefPtr<Frame> mainFrame)
{
    ASSERT(!m_mainFrame); // Should only be called during initialization
    m_mainFrame = mainFrame;
}

bool Page::openedByDOM() const
{
    return m_openedByDOM;
}

void Page::setOpenedByDOM()
{
    m_openedByDOM = true;
}

BackForwardList* Page::backForwardList() const
{
    return m_backForwardController->list();
}

bool Page::goBack()
{
    HistoryItem* item = backForwardList()->backItem();
    
    if (item) {
        goToItem(item, FrameLoadTypeBack);
        return true;
    }
    return false;
}

bool Page::goForward()
{
    HistoryItem* item = backForwardList()->forwardItem();
    
    if (item) {
        goToItem(item, FrameLoadTypeForward);
        return true;
    }
    return false;
}

bool Page::canGoBackOrForward(int distance) const
{
    if (distance == 0)
        return true;
    if (distance > 0 && distance <= backForwardList()->forwardListCount())
        return true;
    if (distance < 0 && -distance <= backForwardList()->backListCount())
        return true;
    return false;
}

void Page::goBackOrForward(int distance)
{
    if (distance == 0)
        return;

    HistoryItem* item = backForwardList()->itemAtIndex(distance);
    if (!item) {
        if (distance > 0) {
            int forwardListCount = backForwardList()->forwardListCount();
            if (forwardListCount > 0) 
                item = backForwardList()->itemAtIndex(forwardListCount);
        } else {
            int backListCount = backForwardList()->backListCount();
            if (backListCount > 0)
                item = backForwardList()->itemAtIndex(-backListCount);
        }
    }

    ASSERT(item); // we should not reach this line with an empty back/forward list
    if (item)
        goToItem(item, FrameLoadTypeIndexedBackForward);
}

void Page::goToItem(HistoryItem* item, FrameLoadType type)
{
    if (defersLoading())
        return;
    
    // Abort any current load unless we're navigating the current document to a new state object
    HistoryItem* currentItem = m_mainFrame->loader()->history()->currentItem();
    if (!item->stateObject() || !currentItem || item->documentSequenceNumber() != currentItem->documentSequenceNumber() || item == currentItem) {
        // Define what to do with any open database connections. By default we stop them and terminate the database thread.
        DatabasePolicy databasePolicy = DatabasePolicyStop;

#if ENABLE(DATABASE)
        // If we're navigating the history via a fragment on the same document, then we do not want to stop databases.
        const KURL& currentURL = m_mainFrame->loader()->url();
        const KURL& newURL = item->url();
    
        if (newURL.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(currentURL, newURL))
            databasePolicy = DatabasePolicyContinue;
#endif

        m_mainFrame->loader()->stopAllLoaders(databasePolicy);
    }
        
    m_mainFrame->loader()->history()->goToItem(item, type);
}

int Page::getHistoryLength()
{
    return backForwardList()->backListCount() + 1 + backForwardList()->forwardListCount();
}

void Page::setGlobalHistoryItem(HistoryItem* item)
{
    m_globalHistoryItem = item;
}

void Page::setGroupName(const String& name)
{
    if (m_group && !m_group->name().isEmpty()) {
        ASSERT(m_group != m_singlePageGroup.get());
        ASSERT(!m_singlePageGroup);
        m_group->removePage(this);
    }

    if (name.isEmpty())
        m_group = m_singlePageGroup.get();
    else {
        m_singlePageGroup.clear();
        m_group = PageGroup::pageGroup(name);
        m_group->addPage(this);
    }
}

const String& Page::groupName() const
{
    DEFINE_STATIC_LOCAL(String, nullString, ());
    return m_group ? m_group->name() : nullString;
}

void Page::initGroup()
{
    ASSERT(!m_singlePageGroup);
    ASSERT(!m_group);
    m_singlePageGroup.set(new PageGroup(this));
    m_group = m_singlePageGroup.get();
}

void Page::setNeedsReapplyStyles()
{
    if (!allPages)
        return;
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->setNeedsReapplyStyles();
}

void Page::refreshPlugins(bool reload)
{
    if (!allPages)
        return;

    PluginData::refresh();

    Vector<RefPtr<Frame> > framesNeedingReload;

    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it) {
        Page* page = *it;
        
        // Clear out the page's plug-in data.
        if (page->m_pluginData) {
            page->m_pluginData->disconnectPage();
            page->m_pluginData = 0;
        }

        if (!reload)
            continue;
        
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (frame->loader()->subframeLoader()->containsPlugins())
                framesNeedingReload.append(frame);
        }
    }

    for (size_t i = 0; i < framesNeedingReload.size(); ++i)
        framesNeedingReload[i]->loader()->reload();
}

PluginData* Page::pluginData() const
{
    if (!mainFrame()->loader()->subframeLoader()->allowPlugins(NotAboutToInstantiatePlugin))
        return 0;
    if (!m_pluginData)
        m_pluginData = PluginData::create(this);
    return m_pluginData.get();
}

inline MediaCanStartListener* Page::takeAnyMediaCanStartListener()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (MediaCanStartListener* listener = frame->document()->takeAnyMediaCanStartListener())
            return listener;
    }
    return 0;
}

void Page::setCanStartMedia(bool canStartMedia)
{
    if (m_canStartMedia == canStartMedia)
        return;

    m_canStartMedia = canStartMedia;

    while (m_canStartMedia) {
        MediaCanStartListener* listener = takeAnyMediaCanStartListener();
        if (!listener)
            break;
        listener->mediaCanStart();
    }
}

static Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

bool Page::findString(const String& target, TextCaseSensitivity caseSensitivity, FindDirection direction, bool shouldWrap)
{
    if (target.isEmpty() || !mainFrame())
        return false;

    Frame* frame = focusController()->focusedOrMainFrame();
    Frame* startFrame = frame;
    do {
        if (frame->findString(target, direction == FindDirectionForward, caseSensitivity == TextCaseSensitive, false, true)) {
            if (frame != startFrame)
                startFrame->selection()->clear();
            focusController()->setFocusedFrame(frame);
            return true;
        }
        frame = incrementFrame(frame, direction == FindDirectionForward, shouldWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (shouldWrap && !startFrame->selection()->isNone()) {
        bool found = startFrame->findString(target, direction == FindDirectionForward, caseSensitivity == TextCaseSensitive, true, true);
        focusController()->setFocusedFrame(frame);
        return found;
    }

    return false;
}

unsigned int Page::markAllMatchesForText(const String& target, TextCaseSensitivity caseSensitivity, bool shouldHighlight, unsigned limit)
{
    if (target.isEmpty() || !mainFrame())
        return 0;

    unsigned matches = 0;

    Frame* frame = mainFrame();
    do {
        frame->setMarkedTextMatchesAreHighlighted(shouldHighlight);
        matches += frame->countMatchesForText(target, caseSensitivity == TextCaseSensitive, (limit == 0) ? 0 : (limit - matches), true);
        frame = incrementFrame(frame, true, false);
    } while (frame);

    return matches;
}

void Page::unmarkAllTextMatches()
{
    if (!mainFrame())
        return;

    Frame* frame = mainFrame();
    do {
        frame->document()->markers()->removeMarkers(DocumentMarker::TextMatch);
        frame = incrementFrame(frame, true, false);
    } while (frame);
}

const VisibleSelection& Page::selection() const
{
    return focusController()->focusedOrMainFrame()->selection()->selection();
}

void Page::setDefersLoading(bool defers)
{
    if (!m_settings->loadDeferringEnabled())
        return;

    if (defers == m_defersLoading)
        return;

    m_defersLoading = defers;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->loader()->setDefersLoading(defers);
}

void Page::clearUndoRedoOperations()
{
    m_editorClient->clearUndoRedoOperations();
}

bool Page::inLowQualityImageInterpolationMode() const
{
    return m_inLowQualityInterpolationMode;
}

void Page::setInLowQualityImageInterpolationMode(bool mode)
{
    m_inLowQualityInterpolationMode = mode;
}

void Page::setMediaVolume(float volume)
{
    if (volume < 0 || volume > 1)
        return;

    if (m_mediaVolume == volume)
        return;

    m_mediaVolume = volume;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        frame->document()->mediaVolumeDidChange();
    }
}

void Page::didMoveOnscreen()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->view())
            frame->view()->didMoveOnscreen();
    }
}

void Page::willMoveOffscreen()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->view())
            frame->view()->willMoveOffscreen();
    }
}

void Page::userStyleSheetLocationChanged()
{
    // FIXME: Eventually we will move to a model of just being handed the sheet
    // text instead of loading the URL ourselves.
    KURL url = m_settings->userStyleSheetLocation();
    if (url.isLocalFile())
        m_userStyleSheetPath = url.fileSystemPath();
    else
        m_userStyleSheetPath = String();

    m_didLoadUserStyleSheet = false;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = 0;
    
    // Data URLs with base64-encoded UTF-8 style sheets are common. We can process them
    // synchronously and avoid using a loader. 
    if (url.protocolIs("data") && url.string().startsWith("data:text/css;charset=utf-8;base64,")) {
        m_didLoadUserStyleSheet = true;
        
        const unsigned prefixLength = 35;
        Vector<char> encodedData(url.string().length() - prefixLength);
        for (unsigned i = prefixLength; i < url.string().length(); ++i)
            encodedData[i - prefixLength] = static_cast<char>(url.string()[i]);

        Vector<char> styleSheetAsUTF8;
        if (base64Decode(encodedData, styleSheetAsUTF8))
            m_userStyleSheet = String::fromUTF8(styleSheetAsUTF8.data(), styleSheetAsUTF8.size());
    }
    
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            frame->document()->updatePageUserSheet();
    }
}

const String& Page::userStyleSheet() const
{
    if (m_userStyleSheetPath.isEmpty())
        return m_userStyleSheet;

    time_t modTime;
    if (!getFileModificationTime(m_userStyleSheetPath, modTime)) {
        // The stylesheet either doesn't exist, was just deleted, or is
        // otherwise unreadable. If we've read the stylesheet before, we should
        // throw away that data now as it no longer represents what's on disk.
        m_userStyleSheet = String();
        return m_userStyleSheet;
    }

    // If the stylesheet hasn't changed since the last time we read it, we can
    // just return the old data.
    if (m_didLoadUserStyleSheet && modTime <= m_userStyleSheetModificationTime)
        return m_userStyleSheet;

    m_didLoadUserStyleSheet = true;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = modTime;

    // FIXME: It would be better to load this asynchronously to avoid blocking
    // the process, but we will first need to create an asynchronous loading
    // mechanism that is not tied to a particular Frame. We will also have to
    // determine what our behavior should be before the stylesheet is loaded
    // and what should happen when it finishes loading, especially with respect
    // to when the load event fires, when Document::close is called, and when
    // layout/paint are allowed to happen.
    RefPtr<SharedBuffer> data = SharedBuffer::createWithContentsOfFile(m_userStyleSheetPath);
    if (!data)
        return m_userStyleSheet;

    RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("text/css");
    m_userStyleSheet = decoder->decode(data->data(), data->size());
    m_userStyleSheet += decoder->flush();

    return m_userStyleSheet;
}

void Page::removeAllVisitedLinks()
{
    if (!allPages)
        return;
    HashSet<PageGroup*> groups;
    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        if (PageGroup* group = (*it)->groupPtr())
            groups.add(group);
    }
    HashSet<PageGroup*>::iterator groupsEnd = groups.end();
    for (HashSet<PageGroup*>::iterator it = groups.begin(); it != groupsEnd; ++it)
        (*it)->removeVisitedLinks();
}

void Page::allVisitedStateChanged(PageGroup* group)
{
    ASSERT(group);
    if (!allPages)
        return;

    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != group)
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree()->traverseNext()) {
            if (CSSStyleSelector* styleSelector = frame->document()->styleSelector())
                styleSelector->allVisitedStateChanged();
        }
    }
}

void Page::visitedStateChanged(PageGroup* group, LinkHash visitedLinkHash)
{
    ASSERT(group);
    if (!allPages)
        return;

    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != group)
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree()->traverseNext()) {
            if (CSSStyleSelector* styleSelector = frame->document()->styleSelector())
                styleSelector->visitedStateChanged(visitedLinkHash);
        }
    }
}

void Page::setDebuggerForAllPages(JSC::Debugger* debugger)
{
    ASSERT(allPages);

    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        (*it)->setDebugger(debugger);
}

void Page::setDebugger(JSC::Debugger* debugger)
{
    if (m_debugger == debugger)
        return;

    m_debugger = debugger;

    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree()->traverseNext())
        frame->script()->attachDebugger(m_debugger);
}

#if ENABLE(DOM_STORAGE)
StorageNamespace* Page::sessionStorage(bool optionalCreate)
{
    if (!m_sessionStorage && optionalCreate)
        m_sessionStorage = StorageNamespace::sessionStorageNamespace(this, m_settings->sessionStorageQuota());

    return m_sessionStorage.get();
}

void Page::setSessionStorage(PassRefPtr<StorageNamespace> newStorage)
{
    m_sessionStorage = newStorage;
}
#endif

#if ENABLE(WML)
WMLPageState* Page::wmlPageState()
{
    if (!m_wmlPageState)    
        m_wmlPageState.set(new WMLPageState(this));
    return m_wmlPageState.get(); 
}
#endif

void Page::setCustomHTMLTokenizerTimeDelay(double customHTMLTokenizerTimeDelay)
{
    if (customHTMLTokenizerTimeDelay < 0) {
        m_customHTMLTokenizerTimeDelay = -1;
        return;
    }
    m_customHTMLTokenizerTimeDelay = customHTMLTokenizerTimeDelay;
}

void Page::setCustomHTMLTokenizerChunkSize(int customHTMLTokenizerChunkSize)
{
    if (customHTMLTokenizerChunkSize < 0) {
        m_customHTMLTokenizerChunkSize = -1;
        return;
    }
    m_customHTMLTokenizerChunkSize = customHTMLTokenizerChunkSize;
}

void Page::setMemoryCacheClientCallsEnabled(bool enabled)
{
    if (m_areMemoryCacheClientCallsEnabled == enabled)
        return;

    m_areMemoryCacheClientCallsEnabled = enabled;
    if (!enabled)
        return;

    for (RefPtr<Frame> frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->loader()->tellClientAboutPastMemoryCacheLoads();
}

void Page::setJavaScriptURLsAreAllowed(bool areAllowed)
{
    m_javaScriptURLsAreAllowed = areAllowed;
}

bool Page::javaScriptURLsAreAllowed() const
{
    return m_javaScriptURLsAreAllowed;
}

#if ENABLE(INSPECTOR)
InspectorTimelineAgent* Page::inspectorTimelineAgent() const
{
    return m_inspectorController->timelineAgent();
}
#endif

#if ENABLE(INPUT_SPEECH)
SpeechInput* Page::speechInput()
{
    ASSERT(m_speechInputClient);
    if (!m_speechInput.get())
        m_speechInput.set(new SpeechInput(m_speechInputClient));
    return m_speechInput.get();
}
#endif

void Page::privateBrowsingStateChanged()
{
    bool privateBrowsingEnabled = m_settings->privateBrowsingEnabled();

    // Collect the PluginViews in to a vector to ensure that action the plug-in takes
    // from below privateBrowsingStateChanged does not affect their lifetime.

    Vector<RefPtr<PluginView>, 32> pluginViews;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        FrameView* view = frame->view();
        if (!view)
            return;

        const HashSet<RefPtr<Widget> >* children = view->children();
        ASSERT(children);

        HashSet<RefPtr<Widget> >::const_iterator end = children->end();
        for (HashSet<RefPtr<Widget> >::const_iterator it = children->begin(); it != end; ++it) {
            Widget* widget = (*it).get();
            if (!widget->isPluginView())
                continue;
            pluginViews.append(static_cast<PluginView*>(widget));
        }
    }

    for (size_t i = 0; i < pluginViews.size(); i++)
        pluginViews[i]->privateBrowsingStateChanged(privateBrowsingEnabled);
}

void Page::pluginAllowedRunTimeChanged()
{
    if (m_pluginHalter)
        m_pluginHalter->setPluginAllowedRunTime(m_settings->pluginAllowedRunTime());
}

void Page::didStartPlugin(HaltablePlugin* obj)
{
    if (m_pluginHalter)
        m_pluginHalter->didStartPlugin(obj);
}

void Page::didStopPlugin(HaltablePlugin* obj)
{
    if (m_pluginHalter)
        m_pluginHalter->didStopPlugin(obj);
}

#if !ASSERT_DISABLED
void Page::checkFrameCountConsistency() const
{
    ASSERT(m_frameCount >= 0);

    int frameCount = 0;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        ++frameCount;

    ASSERT(m_frameCount + 1 == frameCount);
}
#endif
} // namespace WebCore
