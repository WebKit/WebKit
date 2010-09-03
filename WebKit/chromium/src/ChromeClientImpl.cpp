/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ChromeClientImpl.h"

#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "CharacterNames.h"
#include "Console.h"
#include "Cursor.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FileChooser.h"
#include "FloatRect.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "GLES2Context.h"
#include "Geolocation.h"
#include "GeolocationService.h"
#include "GeolocationServiceChromium.h"
#include "GraphicsLayer.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "Node.h"
#include "NotificationPresenterImpl.h"
#include "Page.h"
#include "PopupMenuChromium.h"
#include "SearchPopupMenuChromium.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SharedGraphicsContext3D.h"
#include "WebGeolocationService.h"
#if USE(V8)
#include "V8Proxy.h"
#endif
#include "WebAccessibilityObject.h"
#include "WebConsoleMessage.h"
#include "WebCursorInfo.h"
#include "WebFileChooserCompletionImpl.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebInputEvent.h"
#include "WebKit.h"
#include "WebNode.h"
#include "WebPopupMenuImpl.h"
#include "WebPopupMenuInfo.h"
#include "WebPopupType.h"
#include "WebRect.h"
#include "WebTextDirection.h"
#include "WebURLRequest.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWindowFeatures.h"
#include "WindowFeatures.h"
#include "WrappedResourceRequest.h"

using namespace WebCore;

namespace WebKit {

// Converts a WebCore::PopupContainerType to a WebKit::WebPopupType.
static WebPopupType convertPopupType(PopupContainer::PopupType type)
{
    switch (type) {
    case PopupContainer::Select:
        return WebPopupTypeSelect;
    case PopupContainer::Suggestion:
        return WebPopupTypeSuggestion;
    default:
        ASSERT_NOT_REACHED();
        return WebPopupTypeNone;
    }
}

ChromeClientImpl::ChromeClientImpl(WebViewImpl* webView)
    : m_webView(webView)
    , m_toolbarsVisible(true)
    , m_statusbarVisible(true)
    , m_scrollbarsVisible(true)
    , m_menubarVisible(true)
    , m_resizable(true)
{
}

ChromeClientImpl::~ChromeClientImpl()
{
}

void ChromeClientImpl::chromeDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

void ChromeClientImpl::setWindowRect(const FloatRect& r)
{
    if (m_webView->client())
        m_webView->client()->setWindowRect(IntRect(r));
}

FloatRect ChromeClientImpl::windowRect()
{
    WebRect rect;
    if (m_webView->client())
        rect = m_webView->client()->rootWindowRect();
    else {
        // These numbers will be fairly wrong. The window's x/y coordinates will
        // be the top left corner of the screen and the size will be the content
        // size instead of the window size.
        rect.width = m_webView->size().width;
        rect.height = m_webView->size().height;
    }
    return FloatRect(rect);
}

FloatRect ChromeClientImpl::pageRect()
{
    // We hide the details of the window's border thickness from the web page by
    // simple re-using the window position here.  So, from the point-of-view of
    // the web page, the window has no border.
    return windowRect();
}

float ChromeClientImpl::scaleFactor()
{
    // This is supposed to return the scale factor of the web page. It looks like
    // the implementor of the graphics layer is responsible for doing most of the
    // operations associated with scaling. However, this value is used ins some
    // cases by WebCore. For example, this is used as a scaling factor in canvas
    // so that things drawn in it are scaled just like the web page is.
    //
    // We don't currently implement scaling, so just return 1.0 (no scaling).
    return 1.0;
}

void ChromeClientImpl::focus()
{
    if (m_webView->client())
        m_webView->client()->didFocus();
}

void ChromeClientImpl::unfocus()
{
    if (m_webView->client())
        m_webView->client()->didBlur();
}

bool ChromeClientImpl::canTakeFocus(FocusDirection)
{
    // For now the browser can always take focus if we're not running layout
    // tests.
    return !layoutTestMode();
}

void ChromeClientImpl::takeFocus(FocusDirection direction)
{
    if (!m_webView->client())
        return;
    if (direction == FocusDirectionBackward)
        m_webView->client()->focusPrevious();
    else
        m_webView->client()->focusNext();
}

void ChromeClientImpl::focusedNodeChanged(Node* node)
{
    m_webView->client()->focusedNodeChanged(WebNode(node));

    WebURL focusURL;
    if (node && node->isLink()) {
        // This HitTestResult hack is the easiest way to get a link URL out of a
        // WebCore::Node.
        HitTestResult hitTest(IntPoint(0, 0));
        // This cast must be valid because of the isLink() check.
        hitTest.setURLElement(static_cast<Element*>(node));
        if (hitTest.isLiveLink())
            focusURL = hitTest.absoluteLinkURL();
    }
    m_webView->client()->setKeyboardFocusURL(focusURL);
    
    if (!node)
        return;

    // If accessibility is enabled, we should notify assistive technology that
    // the active AccessibilityObject changed.
    Document* document = node->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    } 
    if (document && document->axObjectCache()->accessibilityEnabled()) {
        // Retrieve the focused AccessibilityObject.
        AccessibilityObject* focusedAccObj =
            document->axObjectCache()->getOrCreate(node->renderer());

        // Alert assistive technology that focus changed.
        if (focusedAccObj)
            m_webView->client()->focusAccessibilityObject(WebAccessibilityObject(focusedAccObj));
    }
}

Page* ChromeClientImpl::createWindow(
    Frame* frame, const FrameLoadRequest& r, const WindowFeatures& features)
{
    if (!m_webView->client())
        return 0;

    WebViewImpl* newView = static_cast<WebViewImpl*>(
        m_webView->client()->createView(WebFrameImpl::fromFrame(frame), features, r.frameName()));
    if (!newView)
        return 0;

    // The request is empty when we are just being asked to open a blank window.
    // This corresponds to window.open(""), for example.
    if (!r.resourceRequest().isEmpty()) {
        WrappedResourceRequest request(r.resourceRequest());
        newView->mainFrame()->loadRequest(request);
    }

    return newView->page();
}

static inline bool currentEventShouldCauseBackgroundTab(const WebInputEvent* inputEvent)
{
    if (!inputEvent)
        return false;

    if (inputEvent->type != WebInputEvent::MouseUp)
        return false;

    const WebMouseEvent* mouseEvent = static_cast<const WebMouseEvent*>(inputEvent);

    WebNavigationPolicy policy;
    unsigned short buttonNumber;
    switch (mouseEvent->button) {
    case WebMouseEvent::ButtonLeft:
        buttonNumber = 0;
        break;
    case WebMouseEvent::ButtonMiddle:
        buttonNumber = 1;
        break;
    case WebMouseEvent::ButtonRight:
        buttonNumber = 2;
        break;
    default:
        return false;
    }
    bool ctrl = mouseEvent->modifiers & WebMouseEvent::ControlKey;
    bool shift = mouseEvent->modifiers & WebMouseEvent::ShiftKey;
    bool alt = mouseEvent->modifiers & WebMouseEvent::AltKey;
    bool meta = mouseEvent->modifiers & WebMouseEvent::MetaKey;

    if (!WebViewImpl::navigationPolicyFromMouseEvent(buttonNumber, ctrl, shift, alt, meta, &policy))
        return false;

    return policy == WebNavigationPolicyNewBackgroundTab;
}

void ChromeClientImpl::show()
{
    if (!m_webView->client())
        return;

    // If our default configuration was modified by a script or wasn't
    // created by a user gesture, then show as a popup. Else, let this
    // new window be opened as a toplevel window.
    bool asPopup = !m_toolbarsVisible
        || !m_statusbarVisible
        || !m_scrollbarsVisible
        || !m_menubarVisible
        || !m_resizable;

    WebNavigationPolicy policy = WebNavigationPolicyNewForegroundTab;
    if (asPopup)
        policy = WebNavigationPolicyNewPopup;
    if (currentEventShouldCauseBackgroundTab(WebViewImpl::currentInputEvent()))
        policy = WebNavigationPolicyNewBackgroundTab;

    m_webView->client()->show(policy);
}

bool ChromeClientImpl::canRunModal()
{
    return !!m_webView->client();
}

void ChromeClientImpl::runModal()
{
    if (m_webView->client())
        m_webView->client()->runModal();
}

void ChromeClientImpl::setToolbarsVisible(bool value)
{
    m_toolbarsVisible = value;
}

bool ChromeClientImpl::toolbarsVisible()
{
    return m_toolbarsVisible;
}

void ChromeClientImpl::setStatusbarVisible(bool value)
{
    m_statusbarVisible = value;
}

bool ChromeClientImpl::statusbarVisible()
{
    return m_statusbarVisible;
}

void ChromeClientImpl::setScrollbarsVisible(bool value)
{
    m_scrollbarsVisible = value;
    WebFrameImpl* web_frame = static_cast<WebFrameImpl*>(m_webView->mainFrame());
    if (web_frame)
        web_frame->setCanHaveScrollbars(value);
}

bool ChromeClientImpl::scrollbarsVisible()
{
    return m_scrollbarsVisible;
}

void ChromeClientImpl::setMenubarVisible(bool value)
{
    m_menubarVisible = value;
}

bool ChromeClientImpl::menubarVisible()
{
    return m_menubarVisible;
}

void ChromeClientImpl::setResizable(bool value)
{
    m_resizable = value;
}

void ChromeClientImpl::addMessageToConsole(MessageSource source,
                                           MessageType type,
                                           MessageLevel level,
                                           const String& message,
                                           unsigned lineNumber,
                                           const String& sourceID)
{
    if (m_webView->client()) {
        m_webView->client()->didAddMessageToConsole(
            WebConsoleMessage(static_cast<WebConsoleMessage::Level>(level), message),
            sourceID,
            lineNumber);
    }
}

bool ChromeClientImpl::canRunBeforeUnloadConfirmPanel()
{
    return !!m_webView->client();
}

bool ChromeClientImpl::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    if (m_webView->client()) {
        return m_webView->client()->runModalBeforeUnloadDialog(
            WebFrameImpl::fromFrame(frame), message);
    }
    return false;
}

void ChromeClientImpl::closeWindowSoon()
{
    // Make sure this Page can no longer be found by JS.
    m_webView->page()->setGroupName(String());

    // Make sure that all loading is stopped.  Ensures that JS stops executing!
    m_webView->mainFrame()->stopLoading();

    if (m_webView->client())
        m_webView->client()->closeWidgetSoon();
}

// Although a Frame is passed in, we don't actually use it, since we
// already know our own m_webView.
void ChromeClientImpl::runJavaScriptAlert(Frame* frame, const String& message)
{
    if (m_webView->client()) {
#if USE(V8)
        // Before showing the JavaScript dialog, we give the proxy implementation
        // a chance to process any pending console messages.
        V8Proxy::processConsoleMessages();
#endif
        m_webView->client()->runModalAlertDialog(
            WebFrameImpl::fromFrame(frame), message);
    }
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptConfirm(Frame* frame, const String& message)
{
    if (m_webView->client()) {
        return m_webView->client()->runModalConfirmDialog(
            WebFrameImpl::fromFrame(frame), message);
    }
    return false;
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptPrompt(Frame* frame,
                                           const String& message,
                                           const String& defaultValue,
                                           String& result)
{
    if (m_webView->client()) {
        WebString actualValue;
        bool ok = m_webView->client()->runModalPromptDialog(
            WebFrameImpl::fromFrame(frame),
            message,
            defaultValue,
            &actualValue);
        if (ok)
            result = actualValue;
        return ok;
    }
    return false;
}

void ChromeClientImpl::setStatusbarText(const String& message)
{
    if (m_webView->client())
        m_webView->client()->setStatusText(message);
}

bool ChromeClientImpl::shouldInterruptJavaScript()
{
    // FIXME: implement me
    return false;
}

bool ChromeClientImpl::tabsToLinks() const
{
    // Returns true if anchors should accept keyboard focus with the tab key.
    // This method is used in a convoluted fashion by EventHandler::tabsToLinks.
    // It's a twisted path (self-evident, but more complicated than seems
    // necessary), but the net result is that returning true from here, on a
    // platform other than MAC or QT, lets anchors get keyboard focus.
    return m_webView->tabsToLinks();
}

IntRect ChromeClientImpl::windowResizerRect() const
{
    IntRect result;
    if (m_webView->client())
        result = m_webView->client()->windowResizerRect();
    return result;
}

void ChromeClientImpl::invalidateWindow(const IntRect&, bool)
{
    notImplemented();
}

void ChromeClientImpl::invalidateContentsAndWindow(const IntRect& updateRect, bool /*immediate*/)
{
    if (updateRect.isEmpty())
        return;
    if (m_webView->client())
        m_webView->client()->didInvalidateRect(updateRect);
}

void ChromeClientImpl::invalidateContentsForSlowScroll(const IntRect& updateRect, bool immediate)
{
    m_webView->hidePopups();
    invalidateContentsAndWindow(updateRect, immediate);
}

void ChromeClientImpl::scroll(
    const IntSize& scrollDelta, const IntRect& scrollRect,
    const IntRect& clipRect)
{
    m_webView->hidePopups();
    if (m_webView->client()) {
        int dx = scrollDelta.width();
        int dy = scrollDelta.height();
        m_webView->client()->didScrollRect(dx, dy, clipRect);
    }
}

IntPoint ChromeClientImpl::screenToWindow(const IntPoint&) const
{
    notImplemented();
    return IntPoint();
}

IntRect ChromeClientImpl::windowToScreen(const IntRect& rect) const
{
    IntRect screenRect(rect);

    if (m_webView->client()) {
        WebRect windowRect = m_webView->client()->windowRect();
        screenRect.move(windowRect.x, windowRect.y);
    }

    return screenRect;
}

void ChromeClientImpl::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    WebFrameImpl* webframe = WebFrameImpl::fromFrame(frame);
    if (webframe->client())
        webframe->client()->didChangeContentsSize(webframe, size);
}

void ChromeClientImpl::scrollbarsModeDidChange() const
{
}

void ChromeClientImpl::mouseDidMoveOverElement(
    const HitTestResult& result, unsigned modifierFlags)
{
    if (!m_webView->client())
        return;
    // Find out if the mouse is over a link, and if so, let our UI know...
    if (result.isLiveLink() && !result.absoluteLinkURL().string().isEmpty())
        m_webView->client()->setMouseOverURL(result.absoluteLinkURL());
    else
        m_webView->client()->setMouseOverURL(WebURL());
}

void ChromeClientImpl::setToolTip(const String& tooltipText, TextDirection dir)
{
    if (!m_webView->client())
        return;
    WebTextDirection textDirection = (dir == RTL) ?
        WebTextDirectionRightToLeft :
        WebTextDirectionLeftToRight;
    m_webView->client()->setToolTipText(
        tooltipText, textDirection);
}

void ChromeClientImpl::print(Frame* frame)
{
    if (m_webView->client())
        m_webView->client()->printPage(WebFrameImpl::fromFrame(frame));
}

void ChromeClientImpl::exceededDatabaseQuota(Frame* frame, const String& databaseName)
{
    // Chromium users cannot currently change the default quota
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void ChromeClientImpl::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    ASSERT_NOT_REACHED();
}

void ChromeClientImpl::reachedApplicationCacheOriginQuota(SecurityOrigin*)
{
    ASSERT_NOT_REACHED();
}
#endif

void ChromeClientImpl::runOpenPanel(Frame* frame, PassRefPtr<FileChooser> fileChooser)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return;

    WebFileChooserParams params;
    params.multiSelect = fileChooser->allowsMultipleFiles();
#if ENABLE(DIRECTORY_UPLOAD)
    params.directory = fileChooser->allowsDirectoryUpload();
#else
    params.directory = false;
#endif
    params.acceptTypes = fileChooser->acceptTypes();
    params.selectedFiles = fileChooser->filenames();
    if (params.selectedFiles.size() > 0)
        params.initialValue = params.selectedFiles[0];
    WebFileChooserCompletionImpl* chooserCompletion =
        new WebFileChooserCompletionImpl(fileChooser);

    if (client->runFileChooser(params, chooserCompletion))
        return;

    // Choosing failed, so do callback with an empty list.
    chooserCompletion->didChooseFile(WebVector<WebString>());
}

void ChromeClientImpl::chooseIconForFiles(const Vector<WTF::String>&, WebCore::FileChooser*)
{
    notImplemented();
}

void ChromeClientImpl::popupOpened(PopupContainer* popupContainer,
                                   const IntRect& bounds,
                                   bool handleExternally)
{
    if (!m_webView->client())
        return;

    WebWidget* webwidget;
    if (handleExternally) {
        WebPopupMenuInfo popupInfo;
        getPopupMenuInfo(popupContainer, &popupInfo);
        webwidget = m_webView->client()->createPopupMenu(popupInfo);
    } else {
        webwidget = m_webView->client()->createPopupMenu(
            convertPopupType(popupContainer->popupType()));
        // We only notify when the WebView has to handle the popup, as when
        // the popup is handled externally, the fact that a popup is showing is
        // transparent to the WebView.
        m_webView->popupOpened(popupContainer);
    }
    static_cast<WebPopupMenuImpl*>(webwidget)->Init(popupContainer, bounds);
}

void ChromeClientImpl::popupClosed(WebCore::PopupContainer* popupContainer)
{
    m_webView->popupClosed(popupContainer);
}

void ChromeClientImpl::setCursor(const WebCursorInfo& cursor)
{
    if (m_webView->client())
        m_webView->client()->didChangeCursor(cursor);
}

void ChromeClientImpl::setCursorForPlugin(const WebCursorInfo& cursor)
{
    setCursor(cursor);
}

void ChromeClientImpl::formStateDidChange(const Node* node)
{
    // The current history item is not updated yet.  That happens lazily when
    // WebFrame::currentHistoryItem is requested.
    WebFrameImpl* webframe = WebFrameImpl::fromFrame(node->document()->frame());
    if (webframe->client())
        webframe->client()->didUpdateCurrentHistoryItem(webframe);
}

void ChromeClientImpl::getPopupMenuInfo(PopupContainer* popupContainer,
                                        WebPopupMenuInfo* info)
{
    const Vector<PopupItem*>& inputItems = popupContainer->popupData();

    WebVector<WebPopupMenuInfo::Item> outputItems(inputItems.size());

    for (size_t i = 0; i < inputItems.size(); ++i) {
        const PopupItem& inputItem = *inputItems[i];
        WebPopupMenuInfo::Item& outputItem = outputItems[i];

        outputItem.label = inputItem.label;
        outputItem.enabled = inputItem.enabled;

        switch (inputItem.type) {
        case PopupItem::TypeOption:
            outputItem.type = WebPopupMenuInfo::Item::Option;
            break;
        case PopupItem::TypeGroup:
            outputItem.type = WebPopupMenuInfo::Item::Group;
            break;
        case PopupItem::TypeSeparator:
            outputItem.type = WebPopupMenuInfo::Item::Separator;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    info->itemHeight = popupContainer->menuItemHeight();
    info->itemFontSize = popupContainer->menuItemFontSize();
    info->selectedIndex = popupContainer->selectedIndex();
    info->items.swap(outputItems);
    info->rightAligned = popupContainer->menuStyle().textDirection() == RTL;
}

void ChromeClientImpl::didChangeAccessibilityObjectState(AccessibilityObject* obj)
{
    // Alert assistive technology about the accessibility object state change
    if (obj)
        m_webView->client()->didChangeAccessibilityObjectState(WebAccessibilityObject(obj));
}

void ChromeClientImpl::didChangeAccessibilityObjectChildren(WebCore::AccessibilityObject* obj)
{
    // Alert assistive technology about the accessibility object children change
    if (obj)
        m_webView->client()->didChangeAccessibilityObjectChildren(WebAccessibilityObject(obj));
}

#if ENABLE(NOTIFICATIONS)
NotificationPresenter* ChromeClientImpl::notificationPresenter() const
{
    return m_webView->notificationPresenterImpl();
}
#endif

void ChromeClientImpl::requestGeolocationPermissionForFrame(Frame* frame, Geolocation* geolocation)
{
    GeolocationServiceChromium* geolocationService = static_cast<GeolocationServiceChromium*>(geolocation->getGeolocationService());
    geolocationService->geolocationServiceBridge()->attachBridgeIfNeeded();
    m_webView->client()->geolocationService()->requestPermissionForFrame(geolocationService->geolocationServiceBridge()->getBridgeId(), frame->document()->url());
}

void ChromeClientImpl::cancelGeolocationPermissionRequestForFrame(Frame* frame, Geolocation* geolocation)
{
    GeolocationServiceChromium* geolocationService = static_cast<GeolocationServiceChromium*>(geolocation->getGeolocationService());
    m_webView->client()->geolocationService()->cancelPermissionRequestForFrame(geolocationService->geolocationServiceBridge()->getBridgeId(), frame->document()->url());
}

#if USE(ACCELERATED_COMPOSITING)
void ChromeClientImpl::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer)
{
    m_webView->setRootGraphicsLayer(graphicsLayer ? graphicsLayer->platformLayer() : 0);
}

void ChromeClientImpl::scheduleCompositingLayerSync()
{
    m_webView->setRootLayerNeedsDisplay();
}

bool ChromeClientImpl::allowsAcceleratedCompositing() const
{
    return m_webView->allowsAcceleratedCompositing();
}
#endif

WebCore::SharedGraphicsContext3D* ChromeClientImpl::getSharedGraphicsContext3D()
{
    return m_webView->getSharedGraphicsContext3D();
}

bool ChromeClientImpl::supportsFullscreenForNode(const WebCore::Node* node)
{
    if (m_webView->client() && node->hasTagName(WebCore::HTMLNames::videoTag))
        return m_webView->client()->supportsFullscreen();
    return false;
}

void ChromeClientImpl::enterFullscreenForNode(WebCore::Node* node)
{
    if (m_webView->client())
        m_webView->client()->enterFullscreenForNode(WebNode(node));
}

void ChromeClientImpl::exitFullscreenForNode(WebCore::Node* node)
{
    if (m_webView->client())
        m_webView->client()->exitFullscreenForNode(WebNode(node));
}

bool ChromeClientImpl::selectItemWritingDirectionIsNatural()
{
    return false;
}

PassRefPtr<PopupMenu> ChromeClientImpl::createPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuChromium(client));
}

PassRefPtr<SearchPopupMenu> ChromeClientImpl::createSearchPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuChromium(client));
}

} // namespace WebKit
