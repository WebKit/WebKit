/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebPage.h"

#include "Arguments.h"
#include "DrawingArea.h"
#if PLATFORM(QT)
#include "HitTestResult.h"
#endif
#include "InjectedBundle.h"
#include "InjectedBundleBackForwardList.h"
#include "MessageID.h"
#include "NetscapePlugin.h"
#include "PageOverlay.h"
#include "PluginProcessConnection.h"
#include "PluginProcessConnectionManager.h"
#include "PluginProxy.h"
#include "PluginView.h"
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
#include "WebInspector.h"
#include "WebInspectorClient.h"
#include "WebPageCreationParameters.h"
#include "WebPageProxyMessages.h"
#include "WebPopupMenu.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include "WebProcessProxyMessageKinds.h"
#include <WebCore/Chrome.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameView.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SubstituteData.h>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>

#if ENABLE(PLUGIN_PROCESS)
// FIXME: This is currently mac specific!
#include "MachPort.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace JSC;
using namespace WebCore;

namespace WebKit {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webPageCounter("WebPage");
#endif

PassRefPtr<WebPage> WebPage::create(uint64_t pageID, const WebPageCreationParameters& parameters)
{
    RefPtr<WebPage> page = adoptRef(new WebPage(pageID, parameters));

    if (parameters.visibleToInjectedBundle && WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->didCreatePage(page.get());

    return page.release();
}

WebPage::WebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
    : m_viewSize(parameters.viewSize)
    , m_isInRedo(false)
    , m_isClosed(false)
    , m_isVisibleToInjectedBundle(parameters.visibleToInjectedBundle)
#if PLATFORM(MAC)
    , m_windowIsVisible(false)
#elif PLATFORM(WIN)
    , m_nativeWindow(parameters.nativeWindow)
#endif
    , m_findController(this)
    , m_pageID(pageID)
{
    ASSERT(m_pageID);

    Page::PageClients pageClients;
    pageClients.chromeClient = new WebChromeClient(this);
    pageClients.contextMenuClient = new WebContextMenuClient(this);
    pageClients.editorClient = new WebEditorClient(this);
    pageClients.dragClient = new WebDragClient(this);
    pageClients.inspectorClient = new WebInspectorClient(this);
    pageClients.backForwardClient = WebBackForwardListProxy::create(this);
    m_page = adoptPtr(new Page(pageClients));

    updatePreferences(parameters.store);

    m_page->setGroupName("WebKit2Group");

    platformInitialize();
    Settings::setMinDOMTimerInterval(0.004);

    m_drawingArea = DrawingArea::create(parameters.drawingAreaInfo.type, parameters.drawingAreaInfo.id, this);

    m_mainFrame = WebFrame::createMainFrame(this);

#ifndef NDEBUG
    webPageCounter.increment();
#endif
}

WebPage::~WebPage()
{
    if (m_backForwardList)
        m_backForwardList->detach();

    ASSERT(!m_page);

#if PLATFORM(MAC)
    ASSERT(m_pluginViews.isEmpty());
#endif

#ifndef NDEBUG
    webPageCounter.decrement();
#endif
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

void WebPage::initializeInjectedBundleUIClient(WKBundlePageUIClient* client)
{
    m_uiClient.initialize(client);
}

PassRefPtr<Plugin> WebPage::createPlugin(const Plugin::Parameters& parameters)
{
    String pluginPath;

    if (!WebProcess::shared().connection()->sendSync(
            Messages::WebContext::GetPluginPath(parameters.mimeType, parameters.url.string()), 
            Messages::WebContext::GetPluginPath::Reply(pluginPath), 0)) {
        return 0;
    }

    if (pluginPath.isNull())
        return 0;

#if ENABLE(PLUGIN_PROCESS)
    PluginProcessConnection* pluginProcessConnection = PluginProcessConnectionManager::shared().getPluginProcessConnection(pluginPath);

    if (!pluginProcessConnection)
        return 0;

    return PluginProxy::create(pluginProcessConnection);
#else
    RefPtr<NetscapePluginModule> pluginModule = NetscapePluginModule::getOrCreate(pluginPath);
    if (!pluginModule)
        return 0;

    return NetscapePlugin::create(pluginModule.release());
#endif
}

String WebPage::renderTreeExternalRepresentation() const
{
    return externalRepresentation(m_mainFrame->coreFrame(), RenderAsTextBehaviorNormal);
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
    mainFrame()->coreFrame()->tree()->clearName();
}

#if USE(ACCELERATED_COMPOSITING)
void WebPage::changeAcceleratedCompositingMode(WebCore::GraphicsLayer* layer)
{
    if (m_isClosed)
        return;

    bool compositing = layer;
    
    // Tell the UI process that accelerated compositing changed. It may respond by changing
    // drawing area types.
    DrawingArea::DrawingAreaInfo newDrawingAreaInfo;

    if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::DidChangeAcceleratedCompositing(compositing), Messages::WebPageProxy::DidChangeAcceleratedCompositing::Reply(newDrawingAreaInfo), m_pageID))
        return;
    
    if (newDrawingAreaInfo.type != drawingArea()->info().type) {
        m_drawingArea = 0;
        if (newDrawingAreaInfo.type != DrawingArea::None) {
            m_drawingArea = DrawingArea::create(newDrawingAreaInfo.type, newDrawingAreaInfo.id, this);
            m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
        }
    }
}

void WebPage::enterAcceleratedCompositingMode(GraphicsLayer* layer)
{
    changeAcceleratedCompositingMode(layer);
    m_drawingArea->setRootCompositingLayer(layer);
}

void WebPage::exitAcceleratedCompositingMode()
{
    changeAcceleratedCompositingMode(0);
}
#endif

void WebPage::close()
{
    if (m_isClosed)
        return;

    m_isClosed = true;

    if (m_isVisibleToInjectedBundle && WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->willDestroyPage(this);

    m_inspector = 0;

    if (m_activePopupMenu) {
        m_activePopupMenu->disconnectFromPage();
        m_activePopupMenu = 0;
    }

    m_mainFrame->coreFrame()->loader()->detachFromParent();
    m_page.clear();

    WebProcess::shared().removeWebPage(m_pageID);
}

void WebPage::tryClose()
{
    if (!m_mainFrame->coreFrame()->loader()->shouldClose())
        return;

    sendClose();
}

void WebPage::sendClose()
{
    send(Messages::WebPageProxy::ClosePage());
}

void WebPage::loadURL(const String& url)
{
    loadURLRequest(ResourceRequest(KURL(KURL(), url)));
}

void WebPage::loadURLRequest(const ResourceRequest& request)
{
    m_mainFrame->coreFrame()->loader()->load(request, false);
}

void WebPage::loadData(PassRefPtr<SharedBuffer> sharedBuffer, const String& MIMEType, const String& encodingName, const KURL& baseURL, const KURL& unreachableURL)
{
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
    KURL unreachableURL = unreachableURLString.isEmpty() ? KURL() : KURL(KURL(), unreachableURLString)  ;
    loadData(sharedBuffer, "text/html", "utf-16", baseURL, unreachableURL);
}

void WebPage::loadPlainTextString(const String& string)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(string.characters()), string.length() * sizeof(UChar));
    loadData(sharedBuffer, "text/plain", "utf-16", blankURL(), KURL());
}

void WebPage::stopLoading()
{
    m_mainFrame->coreFrame()->loader()->stopForUserCancel();
}

void WebPage::reload(bool reloadFromOrigin)
{
    m_mainFrame->coreFrame()->loader()->reload(reloadFromOrigin);
}

void WebPage::goForward(uint64_t backForwardItemID)
{
    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    m_page->goToItem(item, FrameLoadTypeForward);
}

void WebPage::goBack(uint64_t backForwardItemID)
{
    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    m_page->goToItem(item, FrameLoadTypeBack);
}

void WebPage::goToBackForwardItem(uint64_t backForwardItemID)
{
    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    m_page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void WebPage::layoutIfNeeded()
{
    if (m_mainFrame->coreFrame()->view())
        m_mainFrame->coreFrame()->view()->updateLayoutAndStyleIfNeededRecursive();
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
#if ENABLE(TILED_BACKING_STORE)
    // If we are resizing to content ignore external attempts.
    if (!m_resizesToContentsLayoutSize.isEmpty())
        return;
#endif

    if (m_viewSize == viewSize)
        return;

    Frame* frame = m_page->mainFrame();
    
    frame->view()->resize(viewSize);
    frame->view()->setNeedsLayout();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), viewSize));
    
    m_viewSize = viewSize;
}

#if ENABLE(TILED_BACKING_STORE)
void WebPage::setActualVisibleContentRect(const IntRect& rect)
{
    Frame* frame = m_page->mainFrame();

    frame->view()->setActualVisibleContentRect(rect);
}

void WebPage::setResizesToContentsUsingLayoutSize(const IntSize& targetLayoutSize)
{
    m_resizesToContentsLayoutSize = targetLayoutSize;

    Frame* frame = m_page->mainFrame();
    if (m_resizesToContentsLayoutSize.isEmpty()) {
        m_page->settings()->setShouldDelegateScrolling(false);
        frame->view()->setUseFixedLayout(false);
        frame->view()->setPaintsEntireContents(false);
    } else {
        m_page->settings()->setShouldDelegateScrolling(true);
        frame->view()->setUseFixedLayout(true);
        frame->view()->setFixedLayoutSize(m_resizesToContentsLayoutSize);
        frame->view()->setPaintsEntireContents(true);
    }
    frame->view()->forceLayout();
}

void WebPage::resizeToContentsIfNeeded()
{
    if (m_resizesToContentsLayoutSize.isEmpty())
        return;

    Frame* frame = m_page->mainFrame();

    IntSize contentSize = frame->view()->contentsSize();
    if (contentSize == m_viewSize)
        return;

    m_viewSize = contentSize;
    frame->view()->resize(m_viewSize);
    frame->view()->setNeedsLayout();
}
#endif

void WebPage::drawRect(GraphicsContext& graphicsContext, const IntRect& rect)
{
    graphicsContext.save();
    graphicsContext.clip(rect);
    m_mainFrame->coreFrame()->view()->paint(&graphicsContext, rect);
    graphicsContext.restore();

    if (m_pageOverlay) {
        graphicsContext.save();
        graphicsContext.clip(rect);
        m_pageOverlay->drawRect(graphicsContext, rect);
        graphicsContext.restore();
    }
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

void WebPage::scaleWebView(double scale, const IntPoint& origin)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->scalePage(scale, origin);
}

double WebPage::viewScaleFactor() const
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->pageScaleFactor();
}

void WebPage::installPageOverlay(PassRefPtr<PageOverlay> pageOverlay)
{
    if (m_pageOverlay)
        pageOverlay->setPage(0);

    m_pageOverlay = pageOverlay;
    m_pageOverlay->setPage(this);
    m_pageOverlay->setNeedsDisplay();
}

void WebPage::uninstallPageOverlay(PageOverlay* pageOverlay)
{
    if (pageOverlay != m_pageOverlay)
        return;

    m_pageOverlay->setPage(0);
    m_pageOverlay = nullptr;
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
}

void WebPage::pageDidScroll()
{
    // Hide the find indicator.
    m_findController.hideFindIndicator();

    m_uiClient.pageDidScroll(this);

    send(Messages::WebPageProxy::PageDidScroll());
}

#if ENABLE(TILED_BACKING_STORE)
void WebPage::pageDidRequestScroll(const IntSize& delta)
{
    send(Messages::WebPageProxy::PageDidRequestScroll(delta));
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

static bool handleMouseEvent(const WebMouseEvent& mouseEvent, Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    PlatformMouseEvent platformMouseEvent = platform(mouseEvent);

    switch (platformMouseEvent.eventType()) {
        case WebCore::MouseEventPressed:
        {
            if (platformMouseEvent.button() == WebCore::RightButton)
                page->contextMenuController()->clearContextMenu();
            
            bool handled = frame->eventHandler()->handleMousePressEvent(platformMouseEvent);
            
            if (platformMouseEvent.button() == WebCore::RightButton) {
                handled = frame->eventHandler()->sendContextMenuEvent(platformMouseEvent);
                if (handled)
                    page->chrome()->showContextMenu();
            }

            return handled;
        }
        case WebCore::MouseEventReleased:
            return frame->eventHandler()->handleMouseReleaseEvent(platformMouseEvent);
        case WebCore::MouseEventMoved:
            return frame->eventHandler()->mouseMoved(platformMouseEvent);
        default:
            ASSERT_NOT_REACHED();
            return false;
    }
}

void WebPage::mouseEvent(const WebMouseEvent& mouseEvent)
{
    bool handled = false;
    
    if (m_pageOverlay) {
        // Let the page overlay handle the event.
        handled = m_pageOverlay->mouseEvent(mouseEvent);
    }

    if (!handled) {
        CurrentEvent currentEvent(mouseEvent);

        handled = handleMouseEvent(mouseEvent, m_page.get());
    }

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(mouseEvent.type()), handled));
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

    bool handled = handleWheelEvent(wheelEvent, m_page.get());
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(wheelEvent.type()), handled));
}

static bool handleKeyEvent(const WebKeyboardEvent& keyboardEvent, Page* page)
{
    if (!page->mainFrame()->view())
        return false;

    return page->focusController()->focusedOrMainFrame()->eventHandler()->keyEvent(platform(keyboardEvent));
}

void WebPage::keyEvent(const WebKeyboardEvent& keyboardEvent)
{
    CurrentEvent currentEvent(keyboardEvent);

    bool handled = handleKeyEvent(keyboardEvent, m_page.get());
    if (!handled)
        handled = performDefaultBehaviorForKeyEvent(keyboardEvent);

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(keyboardEvent.type()), handled));
}

void WebPage::validateMenuItem(const String& commandName)
{
    bool isEnabled = false;
    int32_t state = 0;
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (frame) {
        Editor::Command command = frame->editor()->command(commandName);
        state = command.state();
        isEnabled = command.isSupported() && command.isEnabled();
    }

    send(Messages::WebPageProxy::DidValidateMenuItem(commandName, isEnabled, state));
}

void WebPage::executeEditCommand(const String& commandName)
{
    executeEditingCommand(commandName, String());
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
#endif

void WebPage::setActive(bool isActive)
{
    m_page->focusController()->setActive(isActive);

#if PLATFORM(MAC)    
    // Tell all our plug-in views that the window focus changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setWindowIsFocused(isActive);
#endif
}

void WebPage::setFocused(bool isFocused)
{
    m_page->focusController()->setFocused(isFocused);
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

void WebPage::setIsInWindow(bool isInWindow)
{
    if (!isInWindow) {
        m_page->setCanStartMedia(false);
        m_page->willMoveOffscreen();
    } else {
        m_page->setCanStartMedia(true);
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

void WebPage::setCustomUserAgent(const String& customUserAgent)
{
    m_customUserAgent = customUserAgent;
}

String WebPage::userAgent() const
{
    if (!m_customUserAgent.isEmpty())
        return m_customUserAgent;

    // FIXME: This should be based on an application name.
    return "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6; en-us) AppleWebKit/531.4 (KHTML, like Gecko) Version/4.0.3 Safari/531.4";
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

void WebPage::runJavaScriptInMainFrame(const String& script, uint64_t callbackID)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    JSLock lock(SilenceAssertionsOnly);
    JSValue resultValue = m_mainFrame->coreFrame()->script()->executeScript(script, true).jsValue();
    String resultString;
    if (resultValue)
        resultString = ustringToString(resultValue.toString(m_mainFrame->coreFrame()->script()->globalObject(mainThreadNormalWorld())->globalExec()));

    send(Messages::WebPageProxy::DidRunJavaScriptInMainFrame(resultString, callbackID));
}

void WebPage::getContentsAsString(uint64_t callbackID)
{
    String resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::DidGetContentsAsString(resultString, callbackID));
}

void WebPage::getRenderTreeExternalRepresentation(uint64_t callbackID)
{
    String resultString = renderTreeExternalRepresentation();
    send(Messages::WebPageProxy::DidGetRenderTreeExternalRepresentation(resultString, callbackID));
}

void WebPage::getSourceForFrame(uint64_t frameID, uint64_t callbackID)
{
    String resultString;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID))
       resultString = frame->source();

    send(Messages::WebPageProxy::DidGetSourceForFrame(resultString, callbackID));
}

void WebPage::preferencesDidChange(const WebPreferencesStore& store)
{
    WebPreferencesStore::removeTestRunnerOverrides();
    updatePreferences(store);
}

void WebPage::updatePreferences(const WebPreferencesStore& store)
{
    Settings* settings = m_page->settings();
    
    settings->setJavaScriptEnabled(store.javaScriptEnabled);
    settings->setLoadsImagesAutomatically(store.loadsImagesAutomatically);
    settings->setPluginsEnabled(store.pluginsEnabled);
    settings->setJavaEnabled(store.javaEnabled);
    settings->setOfflineWebApplicationCacheEnabled(store.offlineWebApplicationCacheEnabled);
    settings->setLocalStorageEnabled(store.localStorageEnabled);
    settings->setXSSAuditorEnabled(store.xssAuditorEnabled);
    settings->setFrameFlatteningEnabled(store.frameFlatteningEnabled);
    settings->setPrivateBrowsingEnabled(store.privateBrowsingEnabled);
    settings->setDeveloperExtrasEnabled(store.developerExtrasEnabled);
    settings->setNeedsSiteSpecificQuirks(store.needsSiteSpecificQuirks);
    settings->setMinimumFontSize(store.minimumFontSize);
    settings->setMinimumLogicalFontSize(store.minimumLogicalFontSize);
    settings->setDefaultFontSize(store.defaultFontSize);
    settings->setDefaultFixedFontSize(store.defaultFixedFontSize);
    settings->setStandardFontFamily(store.standardFontFamily);
    settings->setCursiveFontFamily(store.cursiveFontFamily);
    settings->setFantasyFontFamily(store.fantasyFontFamily);
    settings->setFixedFontFamily(store.fixedFontFamily);
    settings->setSansSerifFontFamily(store.sansSerifFontFamily);
    settings->setSerifFontFamily(store.serifFontFamily);
    settings->setJavaScriptCanOpenWindowsAutomatically(true);

#if PLATFORM(WIN)
    // Temporarily turn off accelerated compositing until we have a good solution for rendering it.
    settings->setAcceleratedCompositingEnabled(false);
#else
    settings->setAcceleratedCompositingEnabled(store.acceleratedCompositingEnabled);
#endif
    settings->setShowDebugBorders(store.compositingBordersVisible);
    settings->setShowRepaintCounter(store.compositingRepaintCountersVisible);
    
    platformPreferencesDidChange(store);
}

WebInspector* WebPage::inspector()
{
    if (m_isClosed)
        return 0;
    if (!m_inspector)
        m_inspector = adoptPtr(new WebInspector(this));
    return m_inspector.get();
}

#if !PLATFORM(MAC)
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

void WebPage::findString(const String& string, uint32_t findDirection, uint32_t findOptions, uint32_t maxMatchCount)
{
    m_findController.findString(string, static_cast<FindDirection>(findDirection), static_cast<FindOptions>(findOptions), maxMatchCount);
}

void WebPage::hideFindUI()
{
    m_findController.hideFindUI();
}

void WebPage::countStringMatches(const String& string, bool caseInsensitive, uint32_t maxMatchCount)
{
    m_findController.countStringMatches(string, caseInsensitive, maxMatchCount);
}

void WebPage::didChangeSelectedIndexForActivePopupMenu(int32_t newIndex)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->didChangeSelectedIndex(newIndex);
    m_activePopupMenu = 0;
}

void WebPage::didSelectItemFromActiveContextMenu(const WebContextMenuItemData& item)
{
    ASSERT(m_contextMenu);
    m_contextMenu->itemSelected(item);
    m_contextMenu = 0;
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

void WebPage::setWindowFrame(const IntRect& windowFrame)
{
    m_windowFrame = windowFrame;

    // Tell all our plug-in views that the window frame changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setWindowFrame(windowFrame);
}

bool WebPage::windowIsFocused() const
{
    return m_page->focusController()->isActive();
}   

#endif

void WebPage::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingArea>()) {
        if (m_drawingArea)
            m_drawingArea->didReceiveMessage(connection, messageID, arguments);
        return;
    }

    if (messageID.is<CoreIPC::MessageClassWebInspector>()) {
        if (WebInspector* inspector = this->inspector())
            inspector->didReceiveWebInspectorMessage(connection, messageID, arguments);
        return;
    }

    didReceiveWebPageMessage(connection, messageID, arguments);
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
    const int minimumZoomTargetWidth = 100;

    Frame* mainframe = m_mainFrame->coreFrame();
    HitTestResult result = mainframe->eventHandler()->hitTestResultAtPoint(mainframe->view()->windowToContents(point), /*allowShadowContent*/ false, /*ignoreClipping*/ true);

    Node* node = result.innerNode();
    while (node && node->getRect().width() < minimumZoomTargetWidth)
        node = node->parentNode();

    IntRect zoomableArea;
    if (node)
        zoomableArea = node->getRect();
    send(Messages::WebPageProxy::DidFindZoomableArea(zoomableArea));
}

#endif

} // namespace WebKit
