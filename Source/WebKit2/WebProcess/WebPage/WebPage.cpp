/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPage.h"

#include "Arguments.h"
#include "DataReference.h"
#include "DecoderAdapter.h"
#include "DrawingArea.h"
#include "InjectedBundle.h"
#include "InjectedBundleBackForwardList.h"
#include "LayerTreeHost.h"
#include "MessageID.h"
#include "NetscapePlugin.h"
#include "PageOverlay.h"
#include "PluginProxy.h"
#include "PluginView.h"
#include "PrintInfo.h"
#include "RunLoop.h"
#include "SessionState.h"
#include "ShareableBitmap.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListProxy.h"
#include "WebChromeClient.h"
#include "WebContextMenu.h"
#include "WebContextMenuClient.h"
#include "WebContextMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebDragClient.h"
#include "WebEditorClient.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebFrame.h"
#include "WebFullScreenManager.h"
#include "WebGeolocationClient.h"
#include "WebGeometry.h"
#include "WebImage.h"
#include "WebInspector.h"
#include "WebInspectorClient.h"
#include "WebNotificationClient.h"
#include "WebOpenPanelResultListener.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroupProxy.h"
#include "WebPageProxyMessages.h"
#include "WebPopupMenu.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/AbstractDatabase.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/Chrome.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/DragSession.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PrintContext.h>
#include <WebCore/RenderArena.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
#include <WebCore/ReplaceSelectionCommand.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/ScriptValue.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/TextIterator.h>
#include <WebCore/markup.h>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>

#include <WebCore/Range.h>
#include <WebCore/VisiblePosition.h>

#if ENABLE(PLUGIN_PROCESS)
#if PLATFORM(MAC)
#include "MachPort.h"
#endif
#endif

#if PLATFORM(MAC)
#include "BuiltInPDFView.h"
#endif

#if PLATFORM(QT)
#include "HitTestResult.h"
#include <QMimeData>
#endif

#if PLATFORM(GTK)
#include "DataObjectGtk.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace JSC;
using namespace WebCore;

namespace WebKit {

class SendStopResponsivenessTimer {
public:
    SendStopResponsivenessTimer(WebPage* page)
        : m_page(page)
    {
    }
    
    ~SendStopResponsivenessTimer()
    {
        m_page->send(Messages::WebPageProxy::StopResponsivenessTimer());
    }

private:
    WebPage* m_page;
};

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageCounter, ("WebPage"));

PassRefPtr<WebPage> WebPage::create(uint64_t pageID, const WebPageCreationParameters& parameters)
{
    RefPtr<WebPage> page = adoptRef(new WebPage(pageID, parameters));

    if (page->pageGroup()->isVisibleToInjectedBundle() && WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->didCreatePage(page.get());

    return page.release();
}

WebPage::WebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
    : m_viewSize(parameters.viewSize)
    , m_drawsBackground(true)
    , m_drawsTransparentBackground(false)
    , m_isInRedo(false)
    , m_isClosed(false)
    , m_tabToLinks(false)
#if PLATFORM(MAC)
    , m_windowIsVisible(false)
    , m_isSmartInsertDeleteEnabled(parameters.isSmartInsertDeleteEnabled)
    , m_keyboardEventBeingInterpreted(0)
#elif PLATFORM(WIN)
    , m_nativeWindow(parameters.nativeWindow)
#endif
    , m_setCanStartMediaTimer(WebProcess::shared().runLoop(), this, &WebPage::setCanStartMediaTimerFired)
    , m_findController(this)
    , m_geolocationPermissionRequestManager(this)
    , m_pageID(pageID)
    , m_canRunBeforeUnloadConfirmPanel(parameters.canRunBeforeUnloadConfirmPanel)
    , m_canRunModal(parameters.canRunModal)
    , m_isRunningModal(false)
    , m_cachedMainFrameIsPinnedToLeftSide(false)
    , m_cachedMainFrameIsPinnedToRightSide(false)
    , m_cachedPageCount(0)
    , m_isShowingContextMenu(false)
#if PLATFORM(WIN)
    , m_gestureReachedScrollingLimit(false)
#endif
{
    ASSERT(m_pageID);
    // FIXME: This is a non-ideal location for this Setting and
    // 4ms should be adopted project-wide now, https://bugs.webkit.org/show_bug.cgi?id=61214
    Settings::setDefaultMinDOMTimerInterval(0.004);

    Page::PageClients pageClients;
    pageClients.chromeClient = new WebChromeClient(this);
    pageClients.contextMenuClient = new WebContextMenuClient(this);
    pageClients.editorClient = new WebEditorClient(this);
    pageClients.dragClient = new WebDragClient(this);
    pageClients.backForwardClient = WebBackForwardListProxy::create(this);
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    pageClients.geolocationClient = new WebGeolocationClient(this);
#endif
#if ENABLE(INSPECTOR)
    pageClients.inspectorClient = new WebInspectorClient(this);
#endif
#if ENABLE(NOTIFICATIONS)
    pageClients.notificationClient = new WebNotificationClient(this);
#endif
    
    m_page = adoptPtr(new Page(pageClients));

    // Qt does not yet call setIsInWindow. Until it does, just leave
    // this line out so plug-ins and video will work. Eventually all platforms
    // should call setIsInWindow and this comment and #if should be removed,
    // leaving behind the setCanStartMedia call.
#if !PLATFORM(QT)
    m_page->setCanStartMedia(false);
#endif

    updatePreferences(parameters.store);

    m_pageGroup = WebProcess::shared().webPageGroup(parameters.pageGroupData);
    m_page->setGroupName(m_pageGroup->identifier());

    platformInitialize();

    m_drawingArea = DrawingArea::create(this, parameters);
    m_drawingArea->setPaintingEnabled(false);

    m_mainFrame = WebFrame::createMainFrame(this);

    setUseFixedLayout(parameters.useFixedLayout);

    setDrawsBackground(parameters.drawsBackground);
    setDrawsTransparentBackground(parameters.drawsTransparentBackground);

    setPaginationMode(parameters.paginationMode);
    setPageLength(parameters.pageLength);
    setGapBetweenPages(parameters.gapBetweenPages);

    setMemoryCacheMessagesEnabled(parameters.areMemoryCacheClientCallsEnabled);

    setActive(parameters.isActive);
    setFocused(parameters.isFocused);
    setIsInWindow(parameters.isInWindow);

    m_userAgent = parameters.userAgent;

    WebBackForwardListProxy::setHighestItemIDFromUIProcess(parameters.highestUsedBackForwardItemID);
    
    if (!parameters.sessionState.isEmpty())
        restoreSession(parameters.sessionState);

    m_drawingArea->setPaintingEnabled(true);

#ifndef NDEBUG
    webPageCounter.increment();
#endif
}

WebPage::~WebPage()
{
    if (m_backForwardList)
        m_backForwardList->detach();

    ASSERT(!m_page);

    m_sandboxExtensionTracker.invalidate();

#if PLATFORM(MAC)
    ASSERT(m_pluginViews.isEmpty());
#endif

#ifndef NDEBUG
    webPageCounter.decrement();
#endif
}

void WebPage::dummy(bool&)
{
}

CoreIPC::Connection* WebPage::connection() const
{
    return WebProcess::shared().connection();
}

void WebPage::initializeInjectedBundleContextMenuClient(WKBundlePageContextMenuClient* client)
{
    m_contextMenuClient.initialize(client);
}

void WebPage::initializeInjectedBundleEditorClient(WKBundlePageEditorClient* client)
{
    m_editorClient.initialize(client);
}

void WebPage::initializeInjectedBundleFormClient(WKBundlePageFormClient* client)
{
    m_formClient.initialize(client);
}

void WebPage::initializeInjectedBundleLoaderClient(WKBundlePageLoaderClient* client)
{
    m_loaderClient.initialize(client);
}

void WebPage::initializeInjectedBundlePolicyClient(WKBundlePagePolicyClient* client)
{
    m_policyClient.initialize(client);
}

void WebPage::initializeInjectedBundleResourceLoadClient(WKBundlePageResourceLoadClient* client)
{
    m_resourceLoadClient.initialize(client);
}

void WebPage::initializeInjectedBundleUIClient(WKBundlePageUIClient* client)
{
    m_uiClient.initialize(client);
}

#if ENABLE(FULLSCREEN_API)
void WebPage::initializeInjectedBundleFullScreenClient(WKBundlePageFullScreenClient* client)
{
    m_fullScreenClient.initialize(client);
}
#endif

PassRefPtr<Plugin> WebPage::createPlugin(const Plugin::Parameters& parameters)
{
    String pluginPath;

    if (!WebProcess::shared().connection()->sendSync(
            Messages::WebContext::GetPluginPath(parameters.mimeType, parameters.url.string()), 
            Messages::WebContext::GetPluginPath::Reply(pluginPath), 0)) {
        return 0;
    }

    if (pluginPath.isNull()) {
#if PLATFORM(MAC)
        if (parameters.mimeType == "application/pdf"
            || (parameters.mimeType.isEmpty() && parameters.url.path().lower().endsWith(".pdf")))
            return BuiltInPDFView::create(m_page.get());
#endif
        return 0;
    }

#if ENABLE(PLUGIN_PROCESS)
    return PluginProxy::create(pluginPath);
#else
    NetscapePlugin::setSetExceptionFunction(NPRuntimeObjectMap::setGlobalException);
    return NetscapePlugin::create(NetscapePluginModule::getOrCreate(pluginPath));
#endif
}

EditorState WebPage::editorState() const
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    ASSERT(frame);

    EditorState result;
    result.selectionIsNone = frame->selection()->isNone();
    result.selectionIsRange = frame->selection()->isRange();
    result.isContentEditable = frame->selection()->isContentEditable();
    result.isContentRichlyEditable = frame->selection()->isContentRichlyEditable();
    result.isInPasswordField = frame->selection()->isInPasswordField();
    result.hasComposition = frame->editor()->hasComposition();

#if PLATFORM(QT)
    size_t location = 0;
    size_t length = 0;
    Element* scope = frame->selection()->rootEditableElement();

    RefPtr<Range> range;
    if (result.hasComposition && (range = frame->editor()->compositionRange())) {
        TextIterator::getLocationAndLengthFromRange(scope, range.get(), location, length);
        result.compositionStart = location;
        result.compositionLength = length;
        result.compositionRect = range->boundingBox();
    }

    if (!result.selectionIsNone && (range = frame->selection()->selection().firstRange())) {
        TextIterator::getLocationAndLengthFromRange(scope, range.get(), location, length);

        ExceptionCode ec = 0;
        RefPtr<Range> tempRange = range->cloneRange(ec);
        tempRange->setStart(tempRange->startContainer(ec), tempRange->startOffset(ec) + location, ec);
        IntRect rect = frame->editor()->firstRectForRange(tempRange.get());
        bool baseIsFirst = frame->selection()->selection().isBaseFirst();

        result.cursorPosition = (baseIsFirst) ? location + length : location;
        result.anchorPosition = (baseIsFirst) ? location : location + length;
        result.microFocus = frame->view()->contentsToWindow(rect);
        result.selectedText = range->text();
    }

    if (scope && result.isContentEditable && !result.isInPasswordField) {
        result.surroundingText = scope->innerText();
        result.surroundingText.remove(result.compositionStart, result.compositionLength);
    }
#endif

    result.shouldIgnoreCompositionSelectionChange = frame->editor()->ignoreCompositionSelectionChange();

    return result;
}

String WebPage::renderTreeExternalRepresentation() const
{
    return externalRepresentation(m_mainFrame->coreFrame(), RenderAsTextBehaviorNormal);
}

uint64_t WebPage::renderTreeSize() const
{
    if (!m_page)
        return 0;

    Frame* mainFrame = m_page->mainFrame();
    if (!mainFrame)
        return 0;

    uint64_t size = 0;
    for (Frame* coreFrame = mainFrame; coreFrame; coreFrame = coreFrame->tree()->traverseNext())
        size += coreFrame->document()->renderArena()->totalRenderArenaSize();

    return size;
}

void WebPage::setTracksRepaints(bool trackRepaints)
{
    if (FrameView* view = mainFrameView())
        view->setTracksRepaints(trackRepaints);
}

bool WebPage::isTrackingRepaints() const
{
    if (FrameView* view = mainFrameView())
        return view->isTrackingRepaints();

    return false;
}

void WebPage::resetTrackedRepaints()
{
    if (FrameView* view = mainFrameView())
        view->resetTrackedRepaints();
}

PassRefPtr<ImmutableArray> WebPage::trackedRepaintRects()
{
    FrameView* view = mainFrameView();
    if (!view)
        return ImmutableArray::create();

    const Vector<IntRect>& rects = view->trackedRepaintRects();
    size_t size = rects.size();
    if (!size)
        return ImmutableArray::create();

    Vector<RefPtr<APIObject> > vector;
    vector.reserveInitialCapacity(size);

    for (size_t i = 0; i < size; ++i)
        vector.uncheckedAppend(WebRect::create(toAPI(rects[i])));

    return ImmutableArray::adopt(vector);
}

void WebPage::executeEditingCommand(const String& commandName, const String& argument)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    frame->editor()->command(commandName).execute(argument);
}

bool WebPage::isEditingCommandEnabled(const String& commandName)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return false;
    
    Editor::Command command = frame->editor()->command(commandName);
    return command.isSupported() && command.isEnabled();
}
    
void WebPage::clearMainFrameName()
{
    if (Frame* frame = mainFrame())
        frame->tree()->clearName();
}

#if USE(ACCELERATED_COMPOSITING)
void WebPage::enterAcceleratedCompositingMode(GraphicsLayer* layer)
{
    m_drawingArea->setRootCompositingLayer(layer);
}

void WebPage::exitAcceleratedCompositingMode()
{
    m_drawingArea->setRootCompositingLayer(0);
}
#endif

void WebPage::close()
{
    if (m_isClosed)
        return;

    m_isClosed = true;

    if (pageGroup()->isVisibleToInjectedBundle() && WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->willDestroyPage(this);

#if ENABLE(INSPECTOR)
    m_inspector = 0;
#endif
#if ENABLE(FULLSCREEN_API)
    m_fullScreenManager = 0;
#endif

    if (m_activePopupMenu) {
        m_activePopupMenu->disconnectFromPage();
        m_activePopupMenu = 0;
    }

    if (m_activeOpenPanelResultListener) {
        m_activeOpenPanelResultListener->disconnectFromPage();
        m_activeOpenPanelResultListener = 0;
    }

    m_sandboxExtensionTracker.invalidate();

    m_underlayPage = nullptr;
    m_printContext = nullptr;
    m_mainFrame->coreFrame()->loader()->detachFromParent();
    m_page = nullptr;
    m_drawingArea = nullptr;

    bool isRunningModal = m_isRunningModal;
    m_isRunningModal = false;

    // The WebPage can be destroyed by this call.
    WebProcess::shared().removeWebPage(m_pageID);

    if (isRunningModal)
        WebProcess::shared().runLoop()->stop();
}

void WebPage::tryClose()
{
    SendStopResponsivenessTimer stopper(this);

    if (!m_mainFrame->coreFrame()->loader()->shouldClose()) {
        send(Messages::WebPageProxy::StopResponsivenessTimer());
        return;
    }

    send(Messages::WebPageProxy::ClosePage(true));
}

void WebPage::sendClose()
{
    send(Messages::WebPageProxy::ClosePage(false));
}

void WebPage::loadURL(const String& url, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    loadURLRequest(ResourceRequest(KURL(KURL(), url)), sandboxExtensionHandle);
}

void WebPage::loadURLRequest(const ResourceRequest& request, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper(this);

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);
    m_mainFrame->coreFrame()->loader()->load(request, false);
}

void WebPage::loadData(PassRefPtr<SharedBuffer> sharedBuffer, const String& MIMEType, const String& encodingName, const KURL& baseURL, const KURL& unreachableURL)
{
    SendStopResponsivenessTimer stopper(this);

    ResourceRequest request(baseURL);
    SubstituteData substituteData(sharedBuffer, MIMEType, encodingName, unreachableURL);
    m_mainFrame->coreFrame()->loader()->load(request, substituteData, false);
}

void WebPage::loadHTMLString(const String& htmlString, const String& baseURLString)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(htmlString.characters()), htmlString.length() * sizeof(UChar));
    KURL baseURL = baseURLString.isEmpty() ? blankURL() : KURL(KURL(), baseURLString);
    loadData(sharedBuffer, "text/html", "utf-16", baseURL, KURL());
}

void WebPage::loadAlternateHTMLString(const String& htmlString, const String& baseURLString, const String& unreachableURLString)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(htmlString.characters()), htmlString.length() * sizeof(UChar));
    KURL baseURL = baseURLString.isEmpty() ? blankURL() : KURL(KURL(), baseURLString);
    KURL unreachableURL = unreachableURLString.isEmpty() ? KURL() : KURL(KURL(), unreachableURLString);
    loadData(sharedBuffer, "text/html", "utf-16", baseURL, unreachableURL);
}

void WebPage::loadPlainTextString(const String& string)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(string.characters()), string.length() * sizeof(UChar));
    loadData(sharedBuffer, "text/plain", "utf-16", blankURL(), KURL());
}

void WebPage::linkClicked(const String& url, const WebMouseEvent& event)
{
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return;

    RefPtr<Event> coreEvent;
    if (event.type() != WebEvent::NoType)
        coreEvent = MouseEvent::create(eventNames().clickEvent, frame->document()->defaultView(), platform(event), 0, 0);

    frame->loader()->loadFrameRequest(FrameLoadRequest(frame->document()->securityOrigin(), ResourceRequest(url)), 
        false, false, coreEvent.get(), 0, MaybeSendReferrer);
}

void WebPage::stopLoadingFrame(uint64_t frameID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;

    frame->coreFrame()->loader()->stopForUserCancel();
}

void WebPage::stopLoading()
{
    SendStopResponsivenessTimer stopper(this);

    m_mainFrame->coreFrame()->loader()->stopForUserCancel();
}

void WebPage::setDefersLoading(bool defersLoading)
{
    m_page->setDefersLoading(defersLoading);
}

void WebPage::reload(bool reloadFromOrigin)
{
    SendStopResponsivenessTimer stopper(this);

    m_mainFrame->coreFrame()->loader()->reload(reloadFromOrigin);
}

void WebPage::goForward(uint64_t backForwardItemID, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);
    m_page->goToItem(item, FrameLoadTypeForward);
}

void WebPage::goBack(uint64_t backForwardItemID, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);
    m_page->goToItem(item, FrameLoadTypeBack);
}

void WebPage::goToBackForwardItem(uint64_t backForwardItemID, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);
    m_page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void WebPage::tryRestoreScrollPosition()
{
    m_page->mainFrame()->loader()->history()->restoreScrollPositionAndViewState();
}

void WebPage::layoutIfNeeded()
{
    if (m_mainFrame->coreFrame()->view())
        m_mainFrame->coreFrame()->view()->updateLayoutAndStyleIfNeededRecursive();

    if (m_underlayPage) {
        if (FrameView *frameView = m_underlayPage->mainFrameView())
            frameView->updateLayoutAndStyleIfNeededRecursive();
    }
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
    FrameView* view = m_page->mainFrame()->view();

#if USE(TILED_BACKING_STORE)
    // If we are resizing to content ignore external attempts.
    if (view->useFixedLayout())
        return;
#endif

    if (m_viewSize == viewSize)
        return;

    view->resize(viewSize);
    view->setNeedsLayout();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), viewSize));
    
    m_viewSize = viewSize;
}

#if USE(TILED_BACKING_STORE)
void WebPage::setFixedVisibleContentRect(const IntRect& rect)
{
    ASSERT(m_useFixedLayout);

    Frame* frame = m_page->mainFrame();

    frame->view()->setFixedVisibleContentRect(rect);
}

void WebPage::setResizesToContentsUsingLayoutSize(const IntSize& targetLayoutSize)
{
    ASSERT(m_useFixedLayout);
    ASSERT(!targetLayoutSize.isEmpty());

    FrameView* view = m_page->mainFrame()->view();

    view->setDelegatesScrolling(true);
    view->setUseFixedLayout(true);
    view->setPaintsEntireContents(true);

    if (view->fixedLayoutSize() == targetLayoutSize)
        return;

    // Always reset even when empty.
    view->setFixedLayoutSize(targetLayoutSize);

    // Schedule a layout to use the new target size.
    if (!view->layoutPending()) {
        view->setNeedsLayout();
        view->scheduleRelayout();
    }
}

void WebPage::resizeToContentsIfNeeded()
{
    ASSERT(m_useFixedLayout);

    FrameView* view = m_page->mainFrame()->view();

    if (!view->useFixedLayout())
        return;

    IntSize contentSize = view->contentsSize();
    if (contentSize == m_viewSize)
        return;

    m_viewSize = contentSize;
    view->resize(m_viewSize);
    view->setNeedsLayout();
}

void WebPage::setViewportSize(const IntSize& size)
{
    ASSERT(m_useFixedLayout);

    if (m_viewportSize == size)
        return;

     m_viewportSize = size;

    // Recalculate the recommended layout size, when the available size (device pixel) changes.
    Settings* settings = m_page->settings();

    int minimumLayoutFallbackWidth = std::max(settings->layoutFallbackWidth(), size.width());

    IntSize targetLayoutSize = computeViewportAttributes(m_page->viewportArguments(), minimumLayoutFallbackWidth, settings->deviceWidth(), settings->deviceHeight(), settings->deviceDPI(), size).layoutSize;
    setResizesToContentsUsingLayoutSize(targetLayoutSize);
}

#endif

void WebPage::scrollMainFrameIfNotAtMaxScrollPosition(const IntSize& scrollOffset)
{
    Frame* frame = m_page->mainFrame();

    IntPoint scrollPosition = frame->view()->scrollPosition();
    IntPoint maximumScrollPosition = frame->view()->maximumScrollPosition();

    // If the current scroll position in a direction is the max scroll position 
    // we don't want to scroll at all.
    IntSize newScrollOffset;
    if (scrollPosition.x() < maximumScrollPosition.x())
        newScrollOffset.setWidth(scrollOffset.width());
    if (scrollPosition.y() < maximumScrollPosition.y())
        newScrollOffset.setHeight(scrollOffset.height());

    if (newScrollOffset.isZero())
        return;

    frame->view()->setScrollPosition(frame->view()->scrollPosition() + newScrollOffset);
}

void WebPage::drawRect(GraphicsContext& graphicsContext, const IntRect& rect)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);
    graphicsContext.clip(rect);

    if (m_underlayPage) {
        m_underlayPage->drawRect(graphicsContext, rect);

        graphicsContext.beginTransparencyLayer(1);
        m_mainFrame->coreFrame()->view()->paint(&graphicsContext, rect);
        graphicsContext.endTransparencyLayer();
        return;
    }

    m_mainFrame->coreFrame()->view()->paint(&graphicsContext, rect);
}

void WebPage::drawPageOverlay(GraphicsContext& graphicsContext, const IntRect& rect)
{
    ASSERT(m_pageOverlay);

    GraphicsContextStateSaver stateSaver(graphicsContext);
    graphicsContext.clip(rect);
    m_pageOverlay->drawRect(graphicsContext, rect);
}

double WebPage::textZoomFactor() const
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->textZoomFactor();
}

void WebPage::setTextZoomFactor(double zoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setTextZoomFactor(static_cast<float>(zoomFactor));
}

double WebPage::pageZoomFactor() const
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->pageZoomFactor();
}

void WebPage::setPageZoomFactor(double zoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setPageZoomFactor(static_cast<float>(zoomFactor));
}

void WebPage::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    return frame->setPageAndTextZoomFactors(static_cast<float>(pageZoomFactor), static_cast<float>(textZoomFactor));
}

void WebPage::windowScreenDidChange(uint64_t displayID)
{
    m_page->windowScreenDidChange(static_cast<PlatformDisplayID>(displayID));
}

void WebPage::scalePage(double scale, const IntPoint& origin)
{
    m_page->setPageScaleFactor(scale, origin);

    send(Messages::WebPageProxy::PageScaleFactorDidChange(scale));
}

double WebPage::pageScaleFactor() const
{
    return m_page->pageScaleFactor();
}

void WebPage::setDeviceScaleFactor(float scaleFactor)
{
    if (scaleFactor == m_page->deviceScaleFactor())
        return;

    m_page->setDeviceScaleFactor(scaleFactor);

    // Tell all our plug-in views that the device scale factor changed.
#if PLATFORM(MAC)
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setDeviceScaleFactor(scaleFactor);
#endif

    if (m_findController.isShowingOverlay()) {
        // We must have updated layout to get the selection rects right.
        layoutIfNeeded();
        m_findController.deviceScaleFactorDidChange();
    }
}

float WebPage::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

void WebPage::setUseFixedLayout(bool fixed)
{
    m_useFixedLayout = fixed;

    FrameView* view = mainFrameView();
    if (!view)
        return;

    view->setUseFixedLayout(fixed);
    if (!fixed)
        view->setFixedLayoutSize(IntSize());
}

void WebPage::setFixedLayoutSize(const IntSize& size)
{
    FrameView* view = mainFrameView();
    if (!view)
        return;

    view->setFixedLayoutSize(size);
    view->forceLayout();
}

void WebPage::setPaginationMode(uint32_t mode)
{
    Page::Pagination pagination = m_page->pagination();
    pagination.mode = static_cast<Page::Pagination::Mode>(mode);
    m_page->setPagination(pagination);
}

void WebPage::setPageLength(double pageLength)
{
    Page::Pagination pagination = m_page->pagination();
    pagination.pageLength = pageLength;
    m_page->setPagination(pagination);
}

void WebPage::setGapBetweenPages(double gap)
{
    Page::Pagination pagination = m_page->pagination();
    pagination.gap = gap;
    m_page->setPagination(pagination);
}

void WebPage::installPageOverlay(PassRefPtr<PageOverlay> pageOverlay)
{
    bool shouldFadeIn = true;
    
    if (m_pageOverlay) {
        m_pageOverlay->setPage(0);

        if (pageOverlay) {
            // We're installing a page overlay when a page overlay is already active.
            // In this case we don't want to fade in the new overlay.
            shouldFadeIn = false;
        }
    }

    m_pageOverlay = pageOverlay;
    m_pageOverlay->setPage(this);

    if (shouldFadeIn)
        m_pageOverlay->startFadeInAnimation();

    m_drawingArea->didInstallPageOverlay();
#if PLATFORM(WIN)
    send(Messages::WebPageProxy::DidInstallOrUninstallPageOverlay(true));
#endif

    m_pageOverlay->setNeedsDisplay();
}

void WebPage::uninstallPageOverlay(PageOverlay* pageOverlay, bool fadeOut)
{
    if (pageOverlay != m_pageOverlay)
        return;

    if (fadeOut) {
        m_pageOverlay->startFadeOutAnimation();
        return;
    }

    m_pageOverlay->setPage(0);
    m_pageOverlay = nullptr;

    m_drawingArea->didUninstallPageOverlay();
#if PLATFORM(WIN)
    send(Messages::WebPageProxy::DidInstallOrUninstallPageOverlay(false));
#endif
}

PassRefPtr<WebImage> WebPage::snapshotInViewCoordinates(const IntRect& rect, ImageOptions options)
{
    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return 0;

    IntSize bitmapSize = rect.size();
    float deviceScaleFactor = corePage()->deviceScaleFactor();
    bitmapSize.scale(deviceScaleFactor);

    RefPtr<WebImage> snapshot = WebImage::create(bitmapSize, options);
    if (!snapshot->bitmap())
        return 0;
    
    OwnPtr<WebCore::GraphicsContext> graphicsContext = snapshot->bitmap()->createGraphicsContext();
    graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
    graphicsContext->translate(-rect.x(), -rect.y());

    frameView->updateLayoutAndStyleIfNeededRecursive();

    PaintBehavior oldBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);
    frameView->paint(graphicsContext.get(), rect);
    frameView->setPaintBehavior(oldBehavior);

    return snapshot.release();
}

PassRefPtr<WebImage> WebPage::scaledSnapshotInDocumentCoordinates(const IntRect& rect, double scaleFactor, ImageOptions options)
{
    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return 0;

    float combinedScaleFactor = scaleFactor * corePage()->deviceScaleFactor();
    IntSize size(ceil(rect.width() * combinedScaleFactor), ceil(rect.height() * combinedScaleFactor));
    RefPtr<WebImage> snapshot = WebImage::create(size, options);
    if (!snapshot->bitmap())
        return 0;

    OwnPtr<WebCore::GraphicsContext> graphicsContext = snapshot->bitmap()->createGraphicsContext();
    graphicsContext->applyDeviceScaleFactor(combinedScaleFactor);
    graphicsContext->translate(-rect.x(), -rect.y());

    frameView->updateLayoutAndStyleIfNeededRecursive();

    PaintBehavior oldBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);
    frameView->paintContents(graphicsContext.get(), rect);
    frameView->setPaintBehavior(oldBehavior);

    return snapshot.release();
}

PassRefPtr<WebImage> WebPage::snapshotInDocumentCoordinates(const IntRect& rect, ImageOptions options)
{
    return scaledSnapshotInDocumentCoordinates(rect, 1, options);
}

void WebPage::pageDidScroll()
{
    // Hide the find indicator.
    m_findController.hideFindIndicator();

    m_uiClient.pageDidScroll(this);

    send(Messages::WebPageProxy::PageDidScroll());
}

#if USE(TILED_BACKING_STORE)
void WebPage::pageDidRequestScroll(const IntPoint& point)
{
    send(Messages::WebPageProxy::PageDidRequestScroll(point));
}
#endif

WebContextMenu* WebPage::contextMenu()
{
    if (!m_contextMenu)
        m_contextMenu = WebContextMenu::create(this);
    return m_contextMenu.get();
}

// Events 

static const WebEvent* g_currentEvent = 0;

// FIXME: WebPage::currentEvent is used by the plug-in code to avoid having to convert from DOM events back to
// WebEvents. When we get the event handling sorted out, this should go away and the Widgets should get the correct
// platform events passed to the event handler code.
const WebEvent* WebPage::currentEvent()
{
    return g_currentEvent;
}

class CurrentEvent {
public:
    explicit CurrentEvent(const WebEvent& event)
        : m_previousCurrentEvent(g_currentEvent)
    {
        g_currentEvent = &event;
    }

    ~CurrentEvent()
    {
        g_currentEvent = m_previousCurrentEvent;
    }

private:
    const WebEvent* m_previousCurrentEvent;
};

static bool isContextClick(const PlatformMouseEvent& event)
{
    if (event.button() == WebCore::RightButton)
        return true;

#if PLATFORM(MAC)
    // FIXME: this really should be about OSX-style UI, not about the Mac port
    if (event.button() == WebCore::LeftButton && event.ctrlKey())
        return true;
#endif

    return false;
}

static bool handleContextMenuEvent(const PlatformMouseEvent& platformMouseEvent, Page* page)
{
    IntPoint point = page->mainFrame()->view()->windowToContents(platformMouseEvent.pos());
    HitTestResult result = page->mainFrame()->eventHandler()->hitTestResultAtPoint(point, false);

    Frame* frame = page->mainFrame();
    if (result.innerNonSharedNode())
        frame = result.innerNonSharedNode()->document()->frame();
    
    bool handled = frame->eventHandler()->sendContextMenuEvent(platformMouseEvent);
    if (handled)
        page->chrome()->showContextMenu();

    return handled;
}

static bool handleMouseEvent(const WebMouseEvent& mouseEvent, Page* page, bool onlyUpdateScrollbars)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    PlatformMouseEvent platformMouseEvent = platform(mouseEvent);

    switch (platformMouseEvent.eventType()) {
        case WebCore::MouseEventPressed:
        {
            if (isContextClick(platformMouseEvent))
                page->contextMenuController()->clearContextMenu();
            
            bool handled = frame->eventHandler()->handleMousePressEvent(platformMouseEvent);
            if (isContextClick(platformMouseEvent))
                handled = handleContextMenuEvent(platformMouseEvent, page);

            return handled;
        }
        case WebCore::MouseEventReleased:
            return frame->eventHandler()->handleMouseReleaseEvent(platformMouseEvent);
        case WebCore::MouseEventMoved:
            return frame->eventHandler()->mouseMoved(platformMouseEvent, onlyUpdateScrollbars);

        default:
            ASSERT_NOT_REACHED();
            return false;
    }
}

void WebPage::mouseEvent(const WebMouseEvent& mouseEvent)
{
    // Don't try to handle any pending mouse events if a context menu is showing.
    if (m_isShowingContextMenu) {
        send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(mouseEvent.type()), false));
        return;
    }
    
    bool handled = false;
    
    if (m_pageOverlay) {
        // Let the page overlay handle the event.
        handled = m_pageOverlay->mouseEvent(mouseEvent);
    }

    if (!handled) {
        CurrentEvent currentEvent(mouseEvent);

        // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
        // button is currently pressed. It is possible that neither of those things will be true since on 
        // Lion when legacy scrollbars are enabled, WebKit receives mouse events all the time. If it is one 
        // of those cases where the page is not active and the mouse is not pressed, then we can fire a more
        // efficient scrollbars-only version of the event.
        bool onlyUpdateScrollbars = !(m_page->focusController()->isActive() || (mouseEvent.button() != WebMouseEvent::NoButton));
        handled = handleMouseEvent(mouseEvent, m_page.get(), onlyUpdateScrollbars);
    }

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(mouseEvent.type()), handled));
}

void WebPage::mouseEventSyncForTesting(const WebMouseEvent& mouseEvent, bool& handled)
{
    // Don't try to handle any pending mouse events if a context menu is showing.
    if (m_isShowingContextMenu) {
        handled = true;
        return;
    }

    handled = m_pageOverlay && m_pageOverlay->mouseEvent(mouseEvent);

    if (!handled) {
        CurrentEvent currentEvent(mouseEvent);

        // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
        // button is currently pressed. It is possible that neither of those things will be true since on 
        // Lion when legacy scrollbars are enabled, WebKit receives mouse events all the time. If it is one 
        // of those cases where the page is not active and the mouse is not pressed, then we can fire a more
        // efficient scrollbars-only version of the event.
        bool onlyUpdateScrollbars = !(m_page->focusController()->isActive() || (mouseEvent.button() != WebMouseEvent::NoButton));
        handled = handleMouseEvent(mouseEvent, m_page.get(), onlyUpdateScrollbars);
    }
}

static bool handleWheelEvent(const WebWheelEvent& wheelEvent, Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    PlatformWheelEvent platformWheelEvent = platform(wheelEvent);
    return frame->eventHandler()->handleWheelEvent(platformWheelEvent);
}

void WebPage::wheelEvent(const WebWheelEvent& wheelEvent)
{
    CurrentEvent currentEvent(wheelEvent);

#if PLATFORM(MAC)
    if (wheelEvent.momentumPhase() == WebWheelEvent::PhaseBegan || wheelEvent.phase() == WebWheelEvent::PhaseBegan)
        m_drawingArea->disableDisplayThrottling();
    else if (wheelEvent.momentumPhase() == WebWheelEvent::PhaseEnded || wheelEvent.phase() == WebWheelEvent::PhaseEnded)
        m_drawingArea->enableDisplayThrottling();
#endif

    bool handled = handleWheelEvent(wheelEvent, m_page.get());
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(wheelEvent.type()), handled));
}

void WebPage::wheelEventSyncForTesting(const WebWheelEvent& wheelEvent, bool& handled)
{
    CurrentEvent currentEvent(wheelEvent);

#if PLATFORM(MAC)
    if (wheelEvent.momentumPhase() == WebWheelEvent::PhaseBegan || wheelEvent.phase() == WebWheelEvent::PhaseBegan)
        m_drawingArea->disableDisplayThrottling();
    else if (wheelEvent.momentumPhase() == WebWheelEvent::PhaseEnded || wheelEvent.phase() == WebWheelEvent::PhaseEnded)
        m_drawingArea->enableDisplayThrottling();
#endif

    handled = handleWheelEvent(wheelEvent, m_page.get());
}

static bool handleKeyEvent(const WebKeyboardEvent& keyboardEvent, Page* page)
{
    if (!page->mainFrame()->view())
        return false;

    if (keyboardEvent.type() == WebEvent::Char && keyboardEvent.isSystemKey())
        return page->focusController()->focusedOrMainFrame()->eventHandler()->handleAccessKey(platform(keyboardEvent));
    return page->focusController()->focusedOrMainFrame()->eventHandler()->keyEvent(platform(keyboardEvent));
}

void WebPage::keyEvent(const WebKeyboardEvent& keyboardEvent)
{
    CurrentEvent currentEvent(keyboardEvent);

    bool handled = handleKeyEvent(keyboardEvent, m_page.get());
    // FIXME: Platform default behaviors should be performed during normal DOM event dispatch (in most cases, in default keydown event handler).
    if (!handled)
        handled = performDefaultBehaviorForKeyEvent(keyboardEvent);

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(keyboardEvent.type()), handled));
}

void WebPage::keyEventSyncForTesting(const WebKeyboardEvent& keyboardEvent, bool& handled)
{
    CurrentEvent currentEvent(keyboardEvent);

    handled = handleKeyEvent(keyboardEvent, m_page.get());
    if (!handled)
        handled = performDefaultBehaviorForKeyEvent(keyboardEvent);
}

#if ENABLE(GESTURE_EVENTS)
static bool handleGestureEvent(const WebGestureEvent& gestureEvent, Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    PlatformGestureEvent platformGestureEvent = platform(gestureEvent);
    return frame->eventHandler()->handleGestureEvent(platformGestureEvent);
}

void WebPage::gestureEvent(const WebGestureEvent& gestureEvent)
{
    CurrentEvent currentEvent(gestureEvent);

    bool handled = handleGestureEvent(gestureEvent, m_page.get());
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(gestureEvent.type()), handled));
}
#endif

void WebPage::validateCommand(const String& commandName, uint64_t callbackID)
{
    bool isEnabled = false;
    int32_t state = 0;
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (frame) {
        Editor::Command command = frame->editor()->command(commandName);
        state = command.state();
        isEnabled = command.isSupported() && command.isEnabled();
    }

    send(Messages::WebPageProxy::ValidateCommandCallback(commandName, isEnabled, state, callbackID));
}

void WebPage::executeEditCommand(const String& commandName)
{
    executeEditingCommand(commandName, String());
}

uint64_t WebPage::restoreSession(const SessionState& sessionState)
{
    const BackForwardListItemVector& list = sessionState.list();
    size_t size = list.size();
    uint64_t currentItemID = 0;
    for (size_t i = 0; i < size; ++i) {
        WebBackForwardListItem* webItem = list[i].get();
        DecoderAdapter decoder(webItem->backForwardData().data(), webItem->backForwardData().size());
        
        RefPtr<HistoryItem> item = HistoryItem::decodeBackForwardTree(webItem->url(), webItem->title(), webItem->originalURL(), decoder);
        if (!item) {
            LOG_ERROR("Failed to decode a HistoryItem from session state data.");
            return 0;
        }
        
        if (i == sessionState.currentIndex())
            currentItemID = webItem->itemID();
        
        WebBackForwardListProxy::addItemFromUIProcess(list[i]->itemID(), item.release());
    }    
    ASSERT(currentItemID);
    return currentItemID;
}

void WebPage::restoreSessionAndNavigateToCurrentItem(const SessionState& sessionState, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    if (uint64_t currentItemID = restoreSession(sessionState))
        goToBackForwardItem(currentItemID, sandboxExtensionHandle);
}

#if ENABLE(TOUCH_EVENTS)
static bool handleTouchEvent(const WebTouchEvent& touchEvent, Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    return frame->eventHandler()->handleTouchEvent(platform(touchEvent));
}

void WebPage::touchEvent(const WebTouchEvent& touchEvent)
{
    CurrentEvent currentEvent(touchEvent);

    bool handled = handleTouchEvent(touchEvent, m_page.get());

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(touchEvent.type()), handled));
}

void WebPage::touchEventSyncForTesting(const WebTouchEvent& touchEvent, bool& handled)
{
    CurrentEvent currentEvent(touchEvent);
    handled = handleTouchEvent(touchEvent, m_page.get());
}
#endif

void WebPage::scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->scrollRecursively(direction, granularity);
}

void WebPage::logicalScroll(Page* page, ScrollLogicalDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->logicalScrollRecursively(direction, granularity);
}

void WebPage::scrollBy(uint32_t scrollDirection, uint32_t scrollGranularity)
{
    scroll(m_page.get(), static_cast<ScrollDirection>(scrollDirection), static_cast<ScrollGranularity>(scrollGranularity));
}

void WebPage::centerSelectionInVisibleArea()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    
    frame->selection()->revealSelection(ScrollAlignment::alignCenterAlways);
    m_findController.showFindIndicatorInSelection();
}

void WebPage::setActive(bool isActive)
{
    m_page->focusController()->setActive(isActive);

#if PLATFORM(MAC)    
    // Tell all our plug-in views that the window focus changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setWindowIsFocused(isActive);
#endif
}

void WebPage::setDrawsBackground(bool drawsBackground)
{
    if (m_drawsBackground == drawsBackground)
        return;

    m_drawsBackground = drawsBackground;

    for (Frame* coreFrame = m_mainFrame->coreFrame(); coreFrame; coreFrame = coreFrame->tree()->traverseNext()) {
        if (FrameView* view = coreFrame->view())
            view->setTransparent(!drawsBackground);
    }

    m_drawingArea->pageBackgroundTransparencyChanged();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
}

void WebPage::setDrawsTransparentBackground(bool drawsTransparentBackground)
{
    if (m_drawsTransparentBackground == drawsTransparentBackground)
        return;

    m_drawsTransparentBackground = drawsTransparentBackground;

    Color backgroundColor = drawsTransparentBackground ? Color::transparent : Color::white;
    for (Frame* coreFrame = m_mainFrame->coreFrame(); coreFrame; coreFrame = coreFrame->tree()->traverseNext()) {
        if (FrameView* view = coreFrame->view())
            view->setBaseBackgroundColor(backgroundColor);
    }

    m_drawingArea->pageBackgroundTransparencyChanged();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
}

void WebPage::viewWillStartLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    if (Frame* frame = m_page->focusController()->focusedOrMainFrame()) {
        if (FrameView* view = frame->view())
            view->willStartLiveResize();
    }
}

void WebPage::viewWillEndLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    if (Frame* frame = m_page->focusController()->focusedOrMainFrame()) {
        if (FrameView* view = frame->view())
            view->willEndLiveResize();
    }
}

void WebPage::setFocused(bool isFocused)
{
    m_page->focusController()->setFocused(isFocused);
}

void WebPage::setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent& event)
{
    if (!m_page || !m_page->focusController())
        return;

    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    frame->document()->setFocusedNode(0);

    if (isKeyboardEventValid && event.type() == WebEvent::KeyDown) {
        PlatformKeyboardEvent platformEvent(platform(event));
        platformEvent.disambiguateKeyDownEvent(PlatformKeyboardEvent::RawKeyDown);
        m_page->focusController()->setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, KeyboardEvent::create(platformEvent, frame->document()->defaultView()).get());
        return;
    }

    m_page->focusController()->setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, 0);
}

void WebPage::setWindowResizerSize(const IntSize& windowResizerSize)
{
    if (m_windowResizerSize == windowResizerSize)
        return;

    m_windowResizerSize = windowResizerSize;

    for (Frame* coreFrame = m_mainFrame->coreFrame(); coreFrame; coreFrame = coreFrame->tree()->traverseNext()) {
        FrameView* view = coreFrame->view();
        if (view)
            view->windowResizerRectChanged();
    }
}

void WebPage::setCanStartMediaTimerFired()
{
    if (m_page)
        m_page->setCanStartMedia(true);
}

void WebPage::setIsInWindow(bool isInWindow)
{
    if (!isInWindow) {
        m_setCanStartMediaTimer.stop();
        m_page->setCanStartMedia(false);
        m_page->willMoveOffscreen();
    } else {
        // Defer the call to Page::setCanStartMedia() since it ends up sending a syncrhonous messages to the UI process
        // in order to get plug-in connections, and the UI process will be waiting for the Web process to update the backing
        // store after moving the view into a window, until it times out and paints white. See <rdar://problem/9242771>.
        m_setCanStartMediaTimer.startOneShot(0);
        m_page->didMoveOnscreen();
    }
}

void WebPage::didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction, uint64_t downloadID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;
    frame->didReceivePolicyDecision(listenerID, static_cast<PolicyAction>(policyAction), downloadID);
}

void WebPage::show()
{
    send(Messages::WebPageProxy::ShowPage());
}

void WebPage::setUserAgent(const String& userAgent)
{
    m_userAgent = userAgent;
}
  
IntPoint WebPage::screenToWindow(const IntPoint& point)
{
    IntPoint windowPoint;
    sendSync(Messages::WebPageProxy::ScreenToWindow(point), Messages::WebPageProxy::ScreenToWindow::Reply(windowPoint));
    return windowPoint;
}
    
IntRect WebPage::windowToScreen(const IntRect& rect)
{
    IntRect screenRect;
    sendSync(Messages::WebPageProxy::WindowToScreen(rect), Messages::WebPageProxy::WindowToScreen::Reply(screenRect));
    return screenRect;
}

IntRect WebPage::windowResizerRect() const
{
    if (m_windowResizerSize.isEmpty())
        return IntRect();

    IntSize frameViewSize;
    if (Frame* coreFrame = m_mainFrame->coreFrame()) {
        if (FrameView* view = coreFrame->view())
            frameViewSize = view->size();
    }

    return IntRect(frameViewSize.width() - m_windowResizerSize.width(), frameViewSize.height() - m_windowResizerSize.height(), 
                   m_windowResizerSize.width(), m_windowResizerSize.height());
}

KeyboardUIMode WebPage::keyboardUIMode()
{
    bool fullKeyboardAccessEnabled = WebProcess::shared().fullKeyboardAccessEnabled();
    return static_cast<KeyboardUIMode>((fullKeyboardAccessEnabled ? KeyboardAccessFull : KeyboardAccessDefault) | (m_tabToLinks ? KeyboardAccessTabsToLinks : 0));
}

void WebPage::runJavaScriptInMainFrame(const String& script, uint64_t callbackID)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    // Retain the SerializedScriptValue at this level so it (and the internal data) lives
    // long enough for the DataReference to be encoded by the sent message.
    RefPtr<SerializedScriptValue> serializedResultValue;
    CoreIPC::DataReference dataReference;

    JSLock lock(SilenceAssertionsOnly);
    if (JSValue resultValue = m_mainFrame->coreFrame()->script()->executeScript(script, true).jsValue()) {
        if ((serializedResultValue = SerializedScriptValue::create(m_mainFrame->jsContext(), toRef(m_mainFrame->coreFrame()->script()->globalObject(mainThreadNormalWorld())->globalExec(), resultValue), 0)))
            dataReference = serializedResultValue->data();
    }

    send(Messages::WebPageProxy::ScriptValueCallback(dataReference, callbackID));
}

void WebPage::getContentsAsString(uint64_t callbackID)
{
    String resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getRenderTreeExternalRepresentation(uint64_t callbackID)
{
    String resultString = renderTreeExternalRepresentation();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getSelectionOrContentsAsString(uint64_t callbackID)
{
    String resultString = m_mainFrame->selectionAsString();
    if (resultString.isEmpty())
        resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getSourceForFrame(uint64_t frameID, uint64_t callbackID)
{
    String resultString;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID))
       resultString = frame->source();

    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getMainResourceDataOfFrame(uint64_t frameID, uint64_t callbackID)
{
    CoreIPC::DataReference dataReference;

    RefPtr<SharedBuffer> buffer;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID)) {
        if (DocumentLoader* loader = frame->coreFrame()->loader()->documentLoader()) {
            if ((buffer = loader->mainResourceData()))
                dataReference = CoreIPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
        }
    }

    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

static PassRefPtr<SharedBuffer> resourceDataForFrame(Frame* frame, const KURL& resourceURL)
{
    DocumentLoader* loader = frame->loader()->documentLoader();
    if (!loader)
        return 0;

    RefPtr<ArchiveResource> subresource = loader->subresource(resourceURL);
    if (!subresource)
        return 0;

    return subresource->data();
}

void WebPage::getResourceDataFromFrame(uint64_t frameID, const String& resourceURLString, uint64_t callbackID)
{
    CoreIPC::DataReference dataReference;
    KURL resourceURL(KURL(), resourceURLString);

    RefPtr<SharedBuffer> buffer;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID)) {
        buffer = resourceDataForFrame(frame->coreFrame(), resourceURL);
        if (!buffer) {
            // Try to get the resource data from the cache.
            buffer = cachedResponseDataForURL(resourceURL);
        }

        if (buffer)
            dataReference = CoreIPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
    }

    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::getWebArchiveOfFrame(uint64_t frameID, uint64_t callbackID)
{
    CoreIPC::DataReference dataReference;

#if PLATFORM(MAC) || PLATFORM(WIN)
    RetainPtr<CFDataRef> data;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID)) {
        if ((data = frame->webArchiveData()))
            dataReference = CoreIPC::DataReference(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
    }
#endif

    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::forceRepaintWithoutCallback()
{
    m_drawingArea->forceRepaint();
}

void WebPage::forceRepaint(uint64_t callbackID)
{
    forceRepaintWithoutCallback();
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::preferencesDidChange(const WebPreferencesStore& store)
{
    WebPreferencesStore::removeTestRunnerOverrides();
    updatePreferences(store);
}

void WebPage::updatePreferences(const WebPreferencesStore& store)
{
    Settings* settings = m_page->settings();

    m_tabToLinks = store.getBoolValueForKey(WebPreferencesKey::tabsToLinksKey());

    // FIXME: This should be generated from macro expansion for all preferences,
    // but we currently don't match the naming of WebCore exactly so we are
    // handrolling the boolean and integer preferences until that is fixed.

#define INITIALIZE_SETTINGS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) settings->set##KeyUpper(store.get##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key()));

    FOR_EACH_WEBKIT_STRING_PREFERENCE(INITIALIZE_SETTINGS)

#undef INITIALIZE_SETTINGS

    settings->setScriptEnabled(store.getBoolValueForKey(WebPreferencesKey::javaScriptEnabledKey()));
    settings->setLoadsImagesAutomatically(store.getBoolValueForKey(WebPreferencesKey::loadsImagesAutomaticallyKey()));
    settings->setLoadsSiteIconsIgnoringImageLoadingSetting(store.getBoolValueForKey(WebPreferencesKey::loadsSiteIconsIgnoringImageLoadingPreferenceKey()));
    settings->setPluginsEnabled(store.getBoolValueForKey(WebPreferencesKey::pluginsEnabledKey()));
    settings->setJavaEnabled(store.getBoolValueForKey(WebPreferencesKey::javaEnabledKey()));
    settings->setOfflineWebApplicationCacheEnabled(store.getBoolValueForKey(WebPreferencesKey::offlineWebApplicationCacheEnabledKey()));
    settings->setLocalStorageEnabled(store.getBoolValueForKey(WebPreferencesKey::localStorageEnabledKey()));
    settings->setXSSAuditorEnabled(store.getBoolValueForKey(WebPreferencesKey::xssAuditorEnabledKey()));
    settings->setFrameFlatteningEnabled(store.getBoolValueForKey(WebPreferencesKey::frameFlatteningEnabledKey()));
    settings->setPrivateBrowsingEnabled(store.getBoolValueForKey(WebPreferencesKey::privateBrowsingEnabledKey()));
    settings->setDeveloperExtrasEnabled(store.getBoolValueForKey(WebPreferencesKey::developerExtrasEnabledKey()));
    settings->setTextAreasAreResizable(store.getBoolValueForKey(WebPreferencesKey::textAreasAreResizableKey()));
    settings->setNeedsSiteSpecificQuirks(store.getBoolValueForKey(WebPreferencesKey::needsSiteSpecificQuirksKey()));
    settings->setJavaScriptCanOpenWindowsAutomatically(store.getBoolValueForKey(WebPreferencesKey::javaScriptCanOpenWindowsAutomaticallyKey()));
    settings->setForceFTPDirectoryListings(store.getBoolValueForKey(WebPreferencesKey::forceFTPDirectoryListingsKey()));
    settings->setDNSPrefetchingEnabled(store.getBoolValueForKey(WebPreferencesKey::dnsPrefetchingEnabledKey()));
#if ENABLE(WEB_ARCHIVE)
    settings->setWebArchiveDebugModeEnabled(store.getBoolValueForKey(WebPreferencesKey::webArchiveDebugModeEnabledKey()));
#endif
    settings->setLocalFileContentSniffingEnabled(store.getBoolValueForKey(WebPreferencesKey::localFileContentSniffingEnabledKey()));
    settings->setUsesPageCache(store.getBoolValueForKey(WebPreferencesKey::usesPageCacheKey()));
    settings->setAuthorAndUserStylesEnabled(store.getBoolValueForKey(WebPreferencesKey::authorAndUserStylesEnabledKey()));
    settings->setPaginateDuringLayoutEnabled(store.getBoolValueForKey(WebPreferencesKey::paginateDuringLayoutEnabledKey()));
    settings->setDOMPasteAllowed(store.getBoolValueForKey(WebPreferencesKey::domPasteAllowedKey()));
    settings->setJavaScriptCanAccessClipboard(store.getBoolValueForKey(WebPreferencesKey::javaScriptCanAccessClipboardKey()));
    settings->setShouldPrintBackgrounds(store.getBoolValueForKey(WebPreferencesKey::shouldPrintBackgroundsKey()));
    settings->setWebSecurityEnabled(store.getBoolValueForKey(WebPreferencesKey::webSecurityEnabledKey()));
    settings->setAllowUniversalAccessFromFileURLs(store.getBoolValueForKey(WebPreferencesKey::allowUniversalAccessFromFileURLsKey()));
    settings->setAllowFileAccessFromFileURLs(store.getBoolValueForKey(WebPreferencesKey::allowFileAccessFromFileURLsKey()));

    settings->setMinimumFontSize(store.getUInt32ValueForKey(WebPreferencesKey::minimumFontSizeKey()));
    settings->setMinimumLogicalFontSize(store.getUInt32ValueForKey(WebPreferencesKey::minimumLogicalFontSizeKey()));
    settings->setDefaultFontSize(store.getUInt32ValueForKey(WebPreferencesKey::defaultFontSizeKey()));
    settings->setDefaultFixedFontSize(store.getUInt32ValueForKey(WebPreferencesKey::defaultFixedFontSizeKey()));
    settings->setLayoutFallbackWidth(store.getUInt32ValueForKey(WebPreferencesKey::layoutFallbackWidthKey()));
    settings->setDeviceDPI(store.getUInt32ValueForKey(WebPreferencesKey::deviceDPIKey()));
    settings->setDeviceWidth(store.getUInt32ValueForKey(WebPreferencesKey::deviceWidthKey()));
    settings->setDeviceHeight(store.getUInt32ValueForKey(WebPreferencesKey::deviceHeightKey()));
    settings->setEditableLinkBehavior(static_cast<WebCore::EditableLinkBehavior>(store.getUInt32ValueForKey(WebPreferencesKey::editableLinkBehaviorKey())));
    settings->setShowsToolTipOverTruncatedText(store.getBoolValueForKey(WebPreferencesKey::showsToolTipOverTruncatedTextKey()));

    settings->setAcceleratedCompositingEnabled(store.getBoolValueForKey(WebPreferencesKey::acceleratedCompositingEnabledKey()) && LayerTreeHost::supportsAcceleratedCompositing());
    settings->setAcceleratedDrawingEnabled(store.getBoolValueForKey(WebPreferencesKey::acceleratedDrawingEnabledKey()) && LayerTreeHost::supportsAcceleratedCompositing());
    settings->setCanvasUsesAcceleratedDrawing(store.getBoolValueForKey(WebPreferencesKey::canvasUsesAcceleratedDrawingKey()) && LayerTreeHost::supportsAcceleratedCompositing());
    settings->setShowDebugBorders(store.getBoolValueForKey(WebPreferencesKey::compositingBordersVisibleKey()));
    settings->setShowRepaintCounter(store.getBoolValueForKey(WebPreferencesKey::compositingRepaintCountersVisibleKey()));
    settings->setWebGLEnabled(store.getBoolValueForKey(WebPreferencesKey::webGLEnabledKey()));
    settings->setMediaPlaybackRequiresUserGesture(store.getBoolValueForKey(WebPreferencesKey::mediaPlaybackRequiresUserGestureKey()));
    settings->setMediaPlaybackAllowsInline(store.getBoolValueForKey(WebPreferencesKey::mediaPlaybackAllowsInlineKey()));
    settings->setMockScrollbarsEnabled(store.getBoolValueForKey(WebPreferencesKey::mockScrollbarsEnabledKey()));

#if ENABLE(SQL_DATABASE)
    AbstractDatabase::setIsAvailable(store.getBoolValueForKey(WebPreferencesKey::databasesEnabledKey()));
#endif

#if ENABLE(FULLSCREEN_API)
    settings->setFullScreenEnabled(store.getBoolValueForKey(WebPreferencesKey::fullScreenEnabledKey()));
#endif

    settings->setLocalStorageDatabasePath(WebProcess::shared().localStorageDirectory());

#if USE(AVFOUNDATION)
    settings->setAVFoundationEnabled(store.getBoolValueForKey(WebPreferencesKey::isAVFoundationEnabledKey()));
#endif

#if ENABLE(WEB_SOCKETS)
    settings->setUseHixie76WebSocketProtocol(store.getBoolValueForKey(WebPreferencesKey::hixie76WebSocketProtocolEnabledKey()));
#endif

#if ENABLE(WEB_AUDIO)
    settings->setWebAudioEnabled(store.getBoolValueForKey(WebPreferencesKey::webAudioEnabledKey()));
#endif

    settings->setApplicationChromeMode(store.getBoolValueForKey(WebPreferencesKey::applicationChromeModeKey()));    
    settings->setSuppressIncrementalRendering(store.getBoolValueForKey(WebPreferencesKey::suppressIncrementalRenderingKey()));
    settings->setBackspaceKeyNavigationEnabled(store.getBoolValueForKey(WebPreferencesKey::backspaceKeyNavigationEnabledKey()));
    settings->setCaretBrowsingEnabled(store.getBoolValueForKey(WebPreferencesKey::caretBrowsingEnabledKey()));

#if ENABLE(VIDEO_TRACK)
    settings->setShouldDisplaySubtitles(store.getBoolValueForKey(WebPreferencesKey::shouldDisplaySubtitlesKey()));
    settings->setShouldDisplayCaptions(store.getBoolValueForKey(WebPreferencesKey::shouldDisplayCaptionsKey()));
    settings->setShouldDisplayTextDescriptions(store.getBoolValueForKey(WebPreferencesKey::shouldDisplayTextDescriptionsKey()));
#endif

    platformPreferencesDidChange(store);
}

#if ENABLE(INSPECTOR)
WebInspector* WebPage::inspector()
{
    if (m_isClosed)
        return 0;
    if (!m_inspector)
        m_inspector = WebInspector::create(this);
    return m_inspector.get();
}
#endif

#if ENABLE(FULLSCREEN_API)
WebFullScreenManager* WebPage::fullScreenManager()
{
    if (!m_fullScreenManager)
        m_fullScreenManager = WebFullScreenManager::create(this);
    return m_fullScreenManager.get();
}
#endif

#if !PLATFORM(GTK) && !PLATFORM(MAC)
bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    Node* node = evt->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return false;

    Editor::Command command = frame->editor()->command(interpretKeyEvent(evt));

    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(evt);
    }

    if (command.execute(evt))
        return true;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (evt->charCode() < ' ')
        return false;

    return frame->editor()->insertText(evt->keyEvent()->text(), evt);
}
#endif

#if PLATFORM(WIN)
void WebPage::performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const WebCore::DragDataMap& dataMap, uint32_t flags)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(WebCore::DragSession()));
        return;
    }

    DragData dragData(dataMap, clientPosition, globalPosition, static_cast<DragOperation>(draggingSourceOperationMask), static_cast<DragApplicationFlags>(flags));
    switch (action) {
    case DragControllerActionEntered:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragEntered(&dragData)));
        break;

    case DragControllerActionUpdated:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragUpdated(&dragData)));
        break;
        
    case DragControllerActionExited:
        m_page->dragController()->dragExited(&dragData);
        break;
        
    case DragControllerActionPerformDrag:
        m_page->dragController()->performDrag(&dragData);
        break;
        
    default:
        ASSERT_NOT_REACHED();
    }
}

#elif PLATFORM(QT) || PLATFORM(GTK)
void WebPage::performDragControllerAction(uint64_t action, WebCore::DragData dragData)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(WebCore::DragSession()));
#if PLATFORM(QT)
        QMimeData* data = const_cast<QMimeData*>(dragData.platformData());
#elif PLATFORM(GTK)
        DataObjectGtk* data = const_cast<DataObjectGtk*>(dragData.platformData());
#endif
        delete data;
        return;
    }

    switch (action) {
    case DragControllerActionEntered:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragEntered(&dragData)));
        break;

    case DragControllerActionUpdated:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragUpdated(&dragData)));
        break;

    case DragControllerActionExited:
        m_page->dragController()->dragExited(&dragData);
        break;

    case DragControllerActionPerformDrag: {
        m_page->dragController()->performDrag(&dragData);
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
    // DragData does not delete its platformData so we need to do that here.
#if PLATFORM(QT)
    QMimeData* data = const_cast<QMimeData*>(dragData.platformData());
#elif PLATFORM(GTK)
    DataObjectGtk* data = const_cast<DataObjectGtk*>(dragData.platformData());
#endif
    delete data;
}

#else
void WebPage::performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const String& dragStorageName, uint32_t flags, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(WebCore::DragSession()));
        return;
    }

    DragData dragData(dragStorageName, clientPosition, globalPosition, static_cast<DragOperation>(draggingSourceOperationMask), static_cast<DragApplicationFlags>(flags));
    switch (action) {
    case DragControllerActionEntered:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragEntered(&dragData)));
        break;

    case DragControllerActionUpdated:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragUpdated(&dragData)));
        break;
        
    case DragControllerActionExited:
        m_page->dragController()->dragExited(&dragData);
        break;
        
    case DragControllerActionPerformDrag: {
        ASSERT(!m_pendingDropSandboxExtension);

        m_pendingDropSandboxExtension = SandboxExtension::create(sandboxExtensionHandle);

        m_page->dragController()->performDrag(&dragData);

        // If we started loading a local file, the sandbox extension tracker would have adopted this
        // pending drop sandbox extension. If not, we'll play it safe and invalidate it.
        if (m_pendingDropSandboxExtension) {
            m_pendingDropSandboxExtension->invalidate();
            m_pendingDropSandboxExtension = nullptr;
        }

        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
}
#endif

void WebPage::dragEnded(WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t operation)
{
    IntPoint adjustedClientPosition(clientPosition.x() + m_page->dragController()->dragOffset().x(), clientPosition.y() + m_page->dragController()->dragOffset().y());
    IntPoint adjustedGlobalPosition(globalPosition.x() + m_page->dragController()->dragOffset().x(), globalPosition.y() + m_page->dragController()->dragOffset().y());

    m_page->dragController()->dragEnded();
    FrameView* view = m_page->mainFrame()->view();
    if (!view)
        return;
    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(adjustedClientPosition, adjustedGlobalPosition, LeftButton, MouseEventMoved, 0, false, false, false, false, currentTime());
    m_page->mainFrame()->eventHandler()->dragSourceEndedAt(event, (DragOperation)operation);
}

void WebPage::willPerformLoadDragDestinationAction()
{
    m_sandboxExtensionTracker.willPerformLoadDragDestinationAction(m_pendingDropSandboxExtension.release());
}

WebEditCommand* WebPage::webEditCommand(uint64_t commandID)
{
    return m_editCommandMap.get(commandID).get();
}

void WebPage::addWebEditCommand(uint64_t commandID, WebEditCommand* command)
{
    m_editCommandMap.set(commandID, command);
}

void WebPage::removeWebEditCommand(uint64_t commandID)
{
    m_editCommandMap.remove(commandID);
}

void WebPage::unapplyEditCommand(uint64_t commandID)
{
    WebEditCommand* command = webEditCommand(commandID);
    if (!command)
        return;

    command->command()->unapply();
}

void WebPage::reapplyEditCommand(uint64_t commandID)
{
    WebEditCommand* command = webEditCommand(commandID);
    if (!command)
        return;

    m_isInRedo = true;
    command->command()->reapply();
    m_isInRedo = false;
}

void WebPage::didRemoveEditCommand(uint64_t commandID)
{
    removeWebEditCommand(commandID);
}

void WebPage::setActivePopupMenu(WebPopupMenu* menu)
{
    m_activePopupMenu = menu;
}

void WebPage::setActiveOpenPanelResultListener(PassRefPtr<WebOpenPanelResultListener> openPanelResultListener)
{
    m_activeOpenPanelResultListener = openPanelResultListener;
}

bool WebPage::findStringFromInjectedBundle(const String& target, FindOptions options)
{
    return m_page->findString(target, options);
}

void WebPage::findString(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.findString(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::hideFindUI()
{
    m_findController.hideFindUI();
}

void WebPage::countStringMatches(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.countStringMatches(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::didChangeSelectedIndexForActivePopupMenu(int32_t newIndex)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->didChangeSelectedIndex(newIndex);
    m_activePopupMenu = 0;
}

void WebPage::didChooseFilesForOpenPanel(const Vector<String>& files)
{
    if (!m_activeOpenPanelResultListener)
        return;

    m_activeOpenPanelResultListener->didChooseFiles(files);
    m_activeOpenPanelResultListener = 0;
}

void WebPage::didCancelForOpenPanel()
{
    m_activeOpenPanelResultListener = 0;
}

#if ENABLE(WEB_PROCESS_SANDBOX)
void WebPage::extendSandboxForFileFromOpenPanel(const SandboxExtension::Handle& handle)
{
    SandboxExtension::create(handle)->consumePermanently();
}
#endif

void WebPage::didReceiveGeolocationPermissionDecision(uint64_t geolocationID, bool allowed)
{
    m_geolocationPermissionRequestManager.didReceiveGeolocationPermissionDecision(geolocationID, allowed);
}

void WebPage::advanceToNextMisspelling(bool startBeforeSelection)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    frame->editor()->advanceToNextMisspelling(startBeforeSelection);
}

void WebPage::changeSpellingToWord(const String& word)
{
    replaceSelectionWithText(m_page->focusController()->focusedOrMainFrame(), word);
}

void WebPage::unmarkAllMisspellings()
{
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (Document* document = frame->document())
            document->markers()->removeMarkers(DocumentMarker::Spelling);
    }
}

void WebPage::unmarkAllBadGrammar()
{
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (Document* document = frame->document())
            document->markers()->removeMarkers(DocumentMarker::Grammar);
    }
}

#if PLATFORM(MAC)
void WebPage::uppercaseWord()
{
    m_page->focusController()->focusedOrMainFrame()->editor()->uppercaseWord();
}

void WebPage::lowercaseWord()
{
    m_page->focusController()->focusedOrMainFrame()->editor()->lowercaseWord();
}

void WebPage::capitalizeWord()
{
    m_page->focusController()->focusedOrMainFrame()->editor()->capitalizeWord();
}
#endif
    
void WebPage::setTextForActivePopupMenu(int32_t index)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->setTextForIndex(index);
}

#if PLATFORM(GTK)
void WebPage::failedToShowPopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->client()->popupDidHide();
}
#endif

void WebPage::didSelectItemFromActiveContextMenu(const WebContextMenuItemData& item)
{
    if (!m_contextMenu)
        return;

    m_contextMenu->itemSelected(item);
    m_contextMenu = 0;
}

void WebPage::replaceSelectionWithText(Frame* frame, const String& text)
{
    if (frame->selection()->isNone())
        return;

    RefPtr<DocumentFragment> textFragment = createFragmentFromText(frame->selection()->toNormalizedRange().get(), text);
    applyCommand(ReplaceSelectionCommand::create(frame->document(), textFragment.release(), ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::MatchStyle | ReplaceSelectionCommand::PreventNesting));
    frame->selection()->revealSelection(ScrollAlignment::alignToEdgeIfNeeded);
}

void WebPage::clearSelection()
{
    m_page->focusController()->focusedOrMainFrame()->selection()->clear();
}

bool WebPage::mainFrameHasCustomRepresentation() const
{
    if (Frame* frame = mainFrame())
        return static_cast<WebFrameLoaderClient*>(frame->loader()->client())->frameHasCustomRepresentation();

    return false;
}

void WebPage::didChangeScrollOffsetForMainFrame()
{
    Frame* frame = m_page->mainFrame();
    IntPoint scrollPosition = frame->view()->scrollPosition();
    IntPoint maximumScrollPosition = frame->view()->maximumScrollPosition();
    IntPoint minimumScrollPosition = frame->view()->minimumScrollPosition();

    bool isPinnedToLeftSide = (scrollPosition.x() <= minimumScrollPosition.x());
    bool isPinnedToRightSide = (scrollPosition.x() >= maximumScrollPosition.x());

    if (isPinnedToLeftSide != m_cachedMainFrameIsPinnedToLeftSide || isPinnedToRightSide != m_cachedMainFrameIsPinnedToRightSide) {
        send(Messages::WebPageProxy::DidChangeScrollOffsetPinningForMainFrame(isPinnedToLeftSide, isPinnedToRightSide));
        
        m_cachedMainFrameIsPinnedToLeftSide = isPinnedToLeftSide;
        m_cachedMainFrameIsPinnedToRightSide = isPinnedToRightSide;
    }
}

void WebPage::mainFrameDidLayout()
{
    unsigned pageCount = m_page->pageCount();
    if (pageCount != m_cachedPageCount) {
        send(Messages::WebPageProxy::DidChangePageCount(pageCount));
        m_cachedPageCount = pageCount;
    }
}

#if PLATFORM(MAC)

void WebPage::addPluginView(PluginView* pluginView)
{
    ASSERT(!m_pluginViews.contains(pluginView));

    m_pluginViews.add(pluginView);
}

void WebPage::removePluginView(PluginView* pluginView)
{
    ASSERT(m_pluginViews.contains(pluginView));

    m_pluginViews.remove(pluginView);
}

void WebPage::setWindowIsVisible(bool windowIsVisible)
{
    m_windowIsVisible = windowIsVisible;

    // Tell all our plug-in views that the window visibility changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setWindowIsVisible(windowIsVisible);
}

void WebPage::windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates, const WebCore::IntPoint& accessibilityViewCoordinates)
{
    m_windowFrameInScreenCoordinates = windowFrameInScreenCoordinates;
    m_viewFrameInWindowCoordinates = viewFrameInWindowCoordinates;
    m_accessibilityPosition = accessibilityViewCoordinates;
    
    // Tell all our plug-in views that the window and view frames have changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->windowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates);
}

#endif

bool WebPage::windowIsFocused() const
{
#if PLATFORM(MAC)
    if (!m_windowIsVisible)
        return false;
#endif
    return m_page->focusController()->isFocused() && m_page->focusController()->isActive();
}    

void WebPage::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingArea>()) {
        if (m_drawingArea)
            m_drawingArea->didReceiveDrawingAreaMessage(connection, messageID, arguments);
        return;
    }

#if USE(TILED_BACKING_STORE) && USE(ACCELERATED_COMPOSITING)
    if (messageID.is<CoreIPC::MessageClassLayerTreeHost>()) {
        if (m_drawingArea)
            m_drawingArea->didReceiveLayerTreeHostMessage(connection, messageID, arguments);
        return;
    }
#endif
    
#if ENABLE(INSPECTOR)
    if (messageID.is<CoreIPC::MessageClassWebInspector>()) {
        if (WebInspector* inspector = this->inspector())
            inspector->didReceiveWebInspectorMessage(connection, messageID, arguments);
        return;
    }
#endif

#if ENABLE(FULLSCREEN_API)
    if (messageID.is<CoreIPC::MessageClassWebFullScreenManager>()) {
        fullScreenManager()->didReceiveMessage(connection, messageID, arguments);
        return;
    }
#endif

    didReceiveWebPageMessage(connection, messageID, arguments);
}

void WebPage::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{   
    didReceiveSyncWebPageMessage(connection, messageID, arguments, reply);
}
    
InjectedBundleBackForwardList* WebPage::backForwardList()
{
    if (!m_backForwardList)
        m_backForwardList = InjectedBundleBackForwardList::create(this);
    return m_backForwardList.get();
}

#if PLATFORM(QT)
void WebPage::findZoomableAreaForPoint(const WebCore::IntPoint& point)
{
    Frame* mainframe = m_mainFrame->coreFrame();
    HitTestResult result = mainframe->eventHandler()->hitTestResultAtPoint(mainframe->view()->windowToContents(point), /*allowShadowContent*/ false, /*ignoreClipping*/ true);

    Node* node = result.innerNode();

    if (!node)
        return;

    IntRect zoomableArea = node->getRect();

    while (true) {
        bool found = !node->isTextNode() && !node->isShadowRoot();

        // No candidate found, bail out.
        if (!found && !node->parentNode())
            return;

        // Candidate found, and it is a better candidate than its parent.
        // NB: A parent is considered a better candidate iff the node is
        // contained by it and it is the only child.
        if (found && (!node->parentNode() || node->parentNode()->childNodeCount() != 1))
            break;

        node = node->parentNode();
        zoomableArea.unite(node->getRect());
    }

    if (node->document() && node->document()->frame() && node->document()->frame()->view()) {
        const ScrollView* view = node->document()->frame()->view();
        zoomableArea = view->contentsToWindow(zoomableArea);
    }

    send(Messages::WebPageProxy::DidFindZoomableArea(point, zoomableArea));
}
#endif

WebPage::SandboxExtensionTracker::~SandboxExtensionTracker()
{
    invalidate();
}

void WebPage::SandboxExtensionTracker::invalidate()
{
    if (m_pendingProvisionalSandboxExtension) {
        m_pendingProvisionalSandboxExtension->invalidate();
        m_pendingProvisionalSandboxExtension = 0;
    }

    if (m_provisionalSandboxExtension) {
        m_provisionalSandboxExtension->invalidate();
        m_provisionalSandboxExtension = 0;
    }

    if (m_committedSandboxExtension) {
        m_committedSandboxExtension->invalidate();
        m_committedSandboxExtension = 0;
    }
}

void WebPage::SandboxExtensionTracker::willPerformLoadDragDestinationAction(PassRefPtr<SandboxExtension> pendingDropSandboxExtension)
{
    setPendingProvisionalSandboxExtension(pendingDropSandboxExtension);
}

void WebPage::SandboxExtensionTracker::beginLoad(WebFrame* frame, const SandboxExtension::Handle& handle)
{
    ASSERT(frame->isMainFrame());

    setPendingProvisionalSandboxExtension(SandboxExtension::create(handle));
}

void WebPage::SandboxExtensionTracker::setPendingProvisionalSandboxExtension(PassRefPtr<SandboxExtension> pendingProvisionalSandboxExtension)
{
    // If we get two beginLoad calls in succession, without a provisional load starting, then
    // m_pendingProvisionalSandboxExtension will be non-null. Invalidate and null out the extension if that is the case.
    if (m_pendingProvisionalSandboxExtension) {
        m_pendingProvisionalSandboxExtension->invalidate();
        m_pendingProvisionalSandboxExtension = nullptr;
    }
    
    m_pendingProvisionalSandboxExtension = pendingProvisionalSandboxExtension;    
}

static bool shouldReuseCommittedSandboxExtension(WebFrame* frame)
{
    ASSERT(frame->isMainFrame());

    FrameLoader* frameLoader = frame->coreFrame()->loader();
    FrameLoadType frameLoadType = frameLoader->loadType();

    // If the page is being reloaded, it should reuse whatever extension is committed.
    if (frameLoadType == FrameLoadTypeReload || frameLoadType == FrameLoadTypeReloadFromOrigin)
        return true;

    DocumentLoader* documentLoader = frameLoader->documentLoader();
    DocumentLoader* provisionalDocumentLoader = frameLoader->provisionalDocumentLoader();
    if (!documentLoader || !provisionalDocumentLoader)
        return false;

    if (documentLoader->url().isLocalFile() && provisionalDocumentLoader->url().isLocalFile())
        return true;

    return false;
}

void WebPage::SandboxExtensionTracker::didStartProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    // We should only reuse the commited sandbox extension if it is not null. It can be
    // null if the last load was for an error page.
    if (m_committedSandboxExtension && shouldReuseCommittedSandboxExtension(frame)) {
        m_pendingProvisionalSandboxExtension = m_committedSandboxExtension.release();
        ASSERT(!m_committedSandboxExtension);
    }

    ASSERT(!m_provisionalSandboxExtension);

    m_provisionalSandboxExtension = m_pendingProvisionalSandboxExtension.release();
    if (!m_provisionalSandboxExtension)
        return;

    m_provisionalSandboxExtension->consume();
}

void WebPage::SandboxExtensionTracker::didCommitProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;
    
    ASSERT(!m_pendingProvisionalSandboxExtension);

    // The provisional load has been committed. Invalidate the currently committed sandbox
    // extension and make the provisional sandbox extension the committed sandbox extension.
    if (m_committedSandboxExtension)
        m_committedSandboxExtension->invalidate();

    m_committedSandboxExtension = m_provisionalSandboxExtension.release();
}

void WebPage::SandboxExtensionTracker::didFailProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    if (!m_provisionalSandboxExtension)
        return;

    m_provisionalSandboxExtension->invalidate();
    m_provisionalSandboxExtension = nullptr;
}

bool WebPage::hasLocalDataForURL(const KURL& url)
{
    if (url.isLocalFile())
        return true;

    FrameLoader* frameLoader = m_page->mainFrame()->loader();
    DocumentLoader* documentLoader = frameLoader ? frameLoader->documentLoader() : 0;
    if (documentLoader && documentLoader->subresource(url))
        return true;

    return platformHasLocalDataForURL(url);
}

void WebPage::setCustomTextEncodingName(const String& encoding)
{
    m_page->mainFrame()->loader()->reloadWithOverrideEncoding(encoding);
}

void WebPage::didRemoveBackForwardItem(uint64_t itemID)
{
    WebBackForwardListProxy::removeItem(itemID);
}

#if PLATFORM(MAC)

bool WebPage::isSpeaking()
{
    bool result;
    return sendSync(Messages::WebPageProxy::GetIsSpeaking(), Messages::WebPageProxy::GetIsSpeaking::Reply(result)) && result;
}

void WebPage::speak(const String& string)
{
    send(Messages::WebPageProxy::Speak(string));
}

void WebPage::stopSpeaking()
{
    send(Messages::WebPageProxy::StopSpeaking());
}

#endif

void WebPage::beginPrinting(uint64_t frameID, const PrintInfo& printInfo)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;

    Frame* coreFrame = frame->coreFrame();
    if (!coreFrame)
        return;

    if (!m_printContext)
        m_printContext = adoptPtr(new PrintContext(coreFrame));

    m_printContext->begin(printInfo.availablePaperWidth, printInfo.availablePaperHeight);

    float fullPageHeight;
    m_printContext->computePageRects(FloatRect(0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight), 0, 0, printInfo.pageSetupScaleFactor, fullPageHeight, true);
}

void WebPage::endPrinting()
{
    m_printContext = nullptr;
}

void WebPage::computePagesForPrinting(uint64_t frameID, const PrintInfo& printInfo, uint64_t callbackID)
{
    Vector<IntRect> resultPageRects;
    double resultTotalScaleFactorForPrinting = 1;

    beginPrinting(frameID, printInfo);

    if (m_printContext) {
        resultPageRects = m_printContext->pageRects();
        resultTotalScaleFactorForPrinting = m_printContext->computeAutomaticScaleFactor(FloatSize(printInfo.availablePaperWidth, printInfo.availablePaperHeight)) * printInfo.pageSetupScaleFactor;
    }

    // If we're asked to print, we should actually print at least a blank page.
    if (resultPageRects.isEmpty())
        resultPageRects.append(IntRect(0, 0, 1, 1));

    send(Messages::WebPageProxy::ComputedPagesCallback(resultPageRects, resultTotalScaleFactorForPrinting, callbackID));
}

#if PLATFORM(MAC) || PLATFORM(WIN)
void WebPage::drawRectToPDF(uint64_t frameID, const WebCore::IntRect& rect, uint64_t callbackID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    Frame* coreFrame = frame ? frame->coreFrame() : 0;

    RetainPtr<CFMutableDataRef> pdfPageData(AdoptCF, CFDataCreateMutable(0, 0));

    if (coreFrame) {
        ASSERT(coreFrame->document()->printing());

#if USE(CG)
        // FIXME: Use CGDataConsumerCreate with callbacks to avoid copying the data.
        RetainPtr<CGDataConsumerRef> pdfDataConsumer(AdoptCF, CGDataConsumerCreateWithCFData(pdfPageData.get()));

        CGRect mediaBox = CGRectMake(0, 0, rect.width(), rect.height());
        RetainPtr<CGContextRef> context(AdoptCF, CGPDFContextCreate(pdfDataConsumer.get(), &mediaBox, 0));
        RetainPtr<CFDictionaryRef> pageInfo(AdoptCF, CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CGPDFContextBeginPage(context.get(), pageInfo.get());

        GraphicsContext ctx(context.get());
        ctx.scale(FloatSize(1, -1));
        ctx.translate(0, -rect.height());
        m_printContext->spoolRect(ctx, rect);

        CGPDFContextEndPage(context.get());
        CGPDFContextClose(context.get());
#endif
    }

    send(Messages::WebPageProxy::DataCallback(CoreIPC::DataReference(CFDataGetBytePtr(pdfPageData.get()), CFDataGetLength(pdfPageData.get())), callbackID));
}

void WebPage::drawPagesToPDF(uint64_t frameID, uint32_t first, uint32_t count, uint64_t callbackID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    Frame* coreFrame = frame ? frame->coreFrame() : 0;

    RetainPtr<CFMutableDataRef> pdfPageData(AdoptCF, CFDataCreateMutable(0, 0));

    if (coreFrame) {
        ASSERT(coreFrame->document()->printing());

#if USE(CG)
        // FIXME: Use CGDataConsumerCreate with callbacks to avoid copying the data.
        RetainPtr<CGDataConsumerRef> pdfDataConsumer(AdoptCF, CGDataConsumerCreateWithCFData(pdfPageData.get()));

        CGRect mediaBox = m_printContext->pageCount() ? m_printContext->pageRect(0) : CGRectMake(0, 0, 1, 1);
        RetainPtr<CGContextRef> context(AdoptCF, CGPDFContextCreate(pdfDataConsumer.get(), &mediaBox, 0));
        for (uint32_t page = first; page < first + count; ++page) {
            if (page >= m_printContext->pageCount())
                break;

            RetainPtr<CFDictionaryRef> pageInfo(AdoptCF, CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            CGPDFContextBeginPage(context.get(), pageInfo.get());

            GraphicsContext ctx(context.get());
            ctx.scale(FloatSize(1, -1));
            ctx.translate(0, -m_printContext->pageRect(page).height());
            m_printContext->spoolPage(ctx, page, m_printContext->pageRect(page).width());

            CGPDFContextEndPage(context.get());
        }
        CGPDFContextClose(context.get());
#endif
    }

    send(Messages::WebPageProxy::DataCallback(CoreIPC::DataReference(CFDataGetBytePtr(pdfPageData.get()), CFDataGetLength(pdfPageData.get())), callbackID));
}
#endif

void WebPage::runModal()
{
    if (m_isClosed)
        return;
    if (m_isRunningModal)
        return;

    m_isRunningModal = true;
    send(Messages::WebPageProxy::RunModal());
    RunLoop::run();
    ASSERT(!m_isRunningModal);
}

void WebPage::setMemoryCacheMessagesEnabled(bool memoryCacheMessagesEnabled)
{
    m_page->setMemoryCacheClientCallsEnabled(memoryCacheMessagesEnabled);
}

bool WebPage::canHandleRequest(const WebCore::ResourceRequest& request)
{
    if (SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(request.url().protocol()))
        return true;
    return platformCanHandleRequest(request);
}

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD)
void WebPage::handleCorrectionPanelResult(const String& result)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    frame->editor()->handleCorrectionPanelResult(result);
}
#endif

void WebPage::simulateMouseDown(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseDown, static_cast<WebMouseEvent::Button>(button), position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time));
}

void WebPage::simulateMouseUp(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseUp, static_cast<WebMouseEvent::Button>(button), position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time));
}

void WebPage::simulateMouseMotion(WebCore::IntPoint position, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseMove, WebMouseEvent::NoButton, position, position, 0, 0, 0, 0, WebMouseEvent::Modifiers(), time));
}

String WebPage::viewportConfigurationAsText(int deviceDPI, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight)
{
    ViewportArguments arguments = mainFrame()->document()->viewportArguments();
    ViewportAttributes attrs = WebCore::computeViewportAttributes(arguments, /* default layout width for non-mobile pages */ 980, deviceWidth, deviceHeight, deviceDPI, IntSize(availableWidth, availableHeight));
    WebCore::restrictMinimumScaleFactorToViewportSize(attrs, IntSize(availableWidth, availableHeight));
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(attrs);
    return String::format("viewport size %dx%d scale %f with limits [%f, %f] and userScalable %f\n", attrs.layoutSize.width(), attrs.layoutSize.height(), attrs.initialScale, attrs.minimumScale, attrs.maximumScale, attrs.userScalable);
}

void WebPage::setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, compositionString.length(), Color(Color::black), false));
    frame->editor()->setComposition(compositionString, underlines, from, from + length);
}

bool WebPage::hasCompositionForTesting()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    return frame && frame->editor()->hasComposition();
}

void WebPage::confirmCompositionForTesting(const String& compositionString)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;

    if (compositionString.isNull())
        frame->editor()->confirmComposition();
    frame->editor()->confirmComposition(compositionString);
}

Frame* WebPage::mainFrame() const
{
    return m_page ? m_page->mainFrame() : 0;
}

FrameView* WebPage::mainFrameView() const
{
    if (Frame* frame = mainFrame())
        return frame->view();
    
    return 0;
}

} // namespace WebKit
