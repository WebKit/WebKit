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
#include "InjectedBundle.h"
#include "MessageID.h"
#include "NetscapePlugin.h"
#include "PluginProcessConnection.h"
#include "PluginProcessConnectionManager.h"
#include "PluginProxy.h"
#include "PluginView.h"
#include "WebBackForwardControllerClient.h"
#include "WebBackForwardListProxy.h"
#include "WebChromeClient.h"
#include "WebContextMenuClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebDragClient.h"
#include "WebEditorClient.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebFrame.h"
#include "WebInspectorClient.h"
#include "WebPageCreationParameters.h"
#include "WebPageProxyMessageKinds.h"
#include "WebProcessProxyMessageKinds.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
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

    if (WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->didCreatePage(page.get());

    return page.release();
}

WebPage::WebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
    : m_viewSize(parameters.viewSize)
    , m_drawingArea(DrawingArea::create(parameters.drawingAreaInfo.type, parameters.drawingAreaInfo.id, this))
    , m_isInRedo(false)
#if PLATFORM(MAC)
    , m_windowIsVisible(false)
#elif PLATFORM(WIN)
    , m_nativeWindow(parameters.nativeWindow)
#endif
    , m_pageID(pageID)
{
    ASSERT(m_pageID);

    Page::PageClients pageClients;
    pageClients.chromeClient = new WebChromeClient(this);
    pageClients.contextMenuClient = new WebContextMenuClient(this);
    pageClients.editorClient = new WebEditorClient(this);
    pageClients.dragClient = new WebDragClient(this);
    pageClients.inspectorClient = new WebInspectorClient(this);
    pageClients.backForwardControllerClient = new WebBackForwardControllerClient(this);
    m_page = adoptPtr(new Page(pageClients));

    m_page->settings()->setJavaScriptEnabled(parameters.store.javaScriptEnabled);
    m_page->settings()->setLoadsImagesAutomatically(parameters.store.loadsImagesAutomatically);
    m_page->settings()->setPluginsEnabled(parameters.store.pluginsEnabled);
    m_page->settings()->setOfflineWebApplicationCacheEnabled(parameters.store.offlineWebApplicationCacheEnabled);
    m_page->settings()->setLocalStorageEnabled(parameters.store.localStorageEnabled);
    m_page->settings()->setXSSAuditorEnabled(parameters.store.xssAuditorEnabled);
    m_page->settings()->setFrameFlatteningEnabled(parameters.store.frameFlatteningEnabled);
    m_page->settings()->setMinimumFontSize(parameters.store.minimumFontSize);
    m_page->settings()->setMinimumLogicalFontSize(parameters.store.minimumLogicalFontSize);
    m_page->settings()->setDefaultFontSize(parameters.store.defaultFontSize);
    m_page->settings()->setDefaultFixedFontSize(parameters.store.defaultFixedFontSize);
    m_page->settings()->setStandardFontFamily(parameters.store.standardFontFamily);
    m_page->settings()->setCursiveFontFamily(parameters.store.cursiveFontFamily);
    m_page->settings()->setFantasyFontFamily(parameters.store.fantasyFontFamily);
    m_page->settings()->setFixedFontFamily(parameters.store.fixedFontFamily);
    m_page->settings()->setSansSerifFontFamily(parameters.store.sansSerifFontFamily);
    m_page->settings()->setSerifFontFamily(parameters.store.serifFontFamily);
    m_page->settings()->setJavaScriptCanOpenWindowsAutomatically(true);

    m_page->setGroupName("WebKit2Group");
    
    platformInitialize();
    Settings::setMinDOMTimerInterval(0.004);

    m_mainFrame = WebFrame::createMainFrame(this);
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidCreateMainFrame, m_pageID, CoreIPC::In(m_mainFrame->frameID()));

#ifndef NDEBUG
    webPageCounter.increment();
#endif
}

WebPage::~WebPage()
{
    ASSERT(!m_page);
#if PLATFORM(MAC)
    ASSERT(m_pluginViews.isEmpty());
#endif

#ifndef NDEBUG
    webPageCounter.decrement();
#endif
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

    if (!WebProcess::shared().connection()->sendSync(WebProcessProxyMessage::GetPluginPath, 0, 
                                                     CoreIPC::In(parameters.mimeType, parameters.url.string()), 
                                                     CoreIPC::Out(pluginPath), 
                                                     CoreIPC::Connection::NoTimeout))
        return 0;

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
    bool compositing = layer;
    
    // Tell the UI process that accelerated compositing changed. It may respond by changing
    // drawing area types.
    DrawingArea::DrawingAreaInfo newDrawingAreaInfo;
    WebProcess::shared().connection()->sendSync(WebPageProxyMessage::DidChangeAcceleratedCompositing,
                                                m_pageID, CoreIPC::In(compositing),
                                                CoreIPC::Out(newDrawingAreaInfo),
                                                CoreIPC::Connection::NoTimeout);
    
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
    
#if USE(ACCELERATED_COMPOSITING)
    m_drawingArea->setRootCompositingLayer(layer);
#endif
}

void WebPage::exitAcceleratedCompositingMode()
{
    changeAcceleratedCompositingMode(0);
}
#endif

void WebPage::close()
{
    if (WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->willDestroyPage(this);

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
    WebProcess::shared().connection()->send(WebPageProxyMessage::ClosePage, m_pageID, CoreIPC::In());
}

void WebPage::loadURL(const String& url)
{
    loadURLRequest(ResourceRequest(KURL(KURL(), url)));
}

void WebPage::loadURLRequest(const ResourceRequest& request)
{
    m_mainFrame->coreFrame()->loader()->load(request, false);
}

void WebPage::loadData(PassRefPtr<SharedBuffer> sharedBuffer, const String& MIMEType, const String& encodingName, const KURL& baseURL, const KURL& failingURL)
{
    ResourceRequest request(baseURL);
    SubstituteData substituteData(sharedBuffer, MIMEType, encodingName, failingURL);
    m_mainFrame->coreFrame()->loader()->load(request, substituteData, false);
}

void WebPage::loadHTMLString(const String& htmlString, const String& baseURLString)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(htmlString.characters()), htmlString.length() * sizeof(UChar));
    KURL baseURL = baseURLString.isEmpty() ? blankURL() : KURL(KURL(), baseURLString);
    loadData(sharedBuffer, "text/html", "utf-16", baseURL, KURL());
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
    if (m_viewSize == viewSize)
        return;

    Frame* frame = m_page->mainFrame();
    
    frame->view()->resize(viewSize);
    frame->view()->setNeedsLayout();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), viewSize));
    
    m_viewSize = viewSize;
}

void WebPage::drawRect(GraphicsContext& graphicsContext, const IntRect& rect)
{
    graphicsContext.save();
    graphicsContext.clip(rect);
    m_mainFrame->coreFrame()->view()->paint(&graphicsContext, rect);
    graphicsContext.restore();
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
            return frame->eventHandler()->handleMousePressEvent(platformMouseEvent);
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
    CurrentEvent currentEvent(mouseEvent);

    bool handled = handleMouseEvent(mouseEvent, m_page.get());

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(mouseEvent.type()), handled));
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
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(wheelEvent.type()), handled));
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

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(keyboardEvent.type()), handled));
}

void WebPage::validateMenuItem(const String& commandName)
{
    bool isEnabled = false;
    int state = 0;
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (frame) {
        Editor::Command command = frame->editor()->command(commandName);
        state = command.state();
        isEnabled = command.isSupported() && command.isEnabled();
    }
    
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidValidateMenuItem, m_pageID, CoreIPC::In(commandName, isEnabled, state));
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

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(touchEvent.type()), handled));
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

void WebPage::didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;
    frame->didReceivePolicyDecision(listenerID, static_cast<WebCore::PolicyAction>(policyAction));
}

void WebPage::show()
{
    WebProcess::shared().connection()->send(WebPageProxyMessage::ShowPage, m_pageID, CoreIPC::In());
}

void WebPage::setCustomUserAgent(const String& customUserAgent)
{
    m_customUserAgent = customUserAgent;
}

String WebPage::userAgent() const
{
    if (!m_customUserAgent.isNull())
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
    String resultString = ustringToString(resultValue.toString(m_mainFrame->coreFrame()->script()->globalObject(mainThreadNormalWorld())->globalExec()));

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidRunJavaScriptInMainFrame, m_pageID, CoreIPC::In(resultString, callbackID));
}

void WebPage::getRenderTreeExternalRepresentation(uint64_t callbackID)
{
    String resultString = renderTreeExternalRepresentation();
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidGetRenderTreeExternalRepresentation, m_pageID, CoreIPC::In(resultString, callbackID));
}

void WebPage::getSourceForFrame(uint64_t frameID, uint64_t callbackID)
{
    String resultString;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID))
       resultString = frame->source();
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidGetSourceForFrame, m_pageID, CoreIPC::In(resultString, callbackID));
}

void WebPage::preferencesDidChange(const WebPreferencesStore& store)
{
    WebPreferencesStore::removeTestRunnerOverrides();

    m_page->settings()->setJavaScriptEnabled(store.javaScriptEnabled);
    m_page->settings()->setLoadsImagesAutomatically(store.loadsImagesAutomatically);
    m_page->settings()->setPluginsEnabled(store.pluginsEnabled);
    m_page->settings()->setOfflineWebApplicationCacheEnabled(store.offlineWebApplicationCacheEnabled);
    m_page->settings()->setLocalStorageEnabled(store.localStorageEnabled);
    m_page->settings()->setXSSAuditorEnabled(store.xssAuditorEnabled);
    m_page->settings()->setFrameFlatteningEnabled(store.frameFlatteningEnabled);
    m_page->settings()->setStandardFontFamily(store.standardFontFamily);
    m_page->settings()->setCursiveFontFamily(store.cursiveFontFamily);
    m_page->settings()->setFantasyFontFamily(store.fantasyFontFamily);
    m_page->settings()->setFixedFontFamily(store.fixedFontFamily);
    m_page->settings()->setSansSerifFontFamily(store.sansSerifFontFamily);
    m_page->settings()->setSerifFontFamily(store.serifFontFamily);

    platformPreferencesDidChange(store);
}

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

    didReceiveWebPageMessage(connection, messageID, arguments);
}

} // namespace WebKit
