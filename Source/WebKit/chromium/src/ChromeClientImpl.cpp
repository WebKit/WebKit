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
#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#include "ColorChooserClient.h"
#include "ColorChooserUIController.h"
#endif
#include "Console.h"
#include "Cursor.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ExternalPopupMenu.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include "FloatRect.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "Geolocation.h"
#include "GraphicsLayer.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Icon.h"
#include "IntRect.h"
#include "NavigationAction.h"
#include "Node.h"
#include "Page.h"
#include "PagePopupDriver.h"
#include "PlatformScreen.h"
#include "PlatformSupport.h"
#include "PopupContainer.h"
#include "PopupMenuChromium.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "SearchPopupMenuChromium.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "TextFieldDecorationElement.h"
#if USE(V8)
#include "V8Proxy.h"
#endif
#include "WebAccessibilityObject.h"
#if ENABLE(INPUT_TYPE_COLOR)
#include "WebColorChooser.h"
#endif
#include "WebConsoleMessage.h"
#include "WebCursorInfo.h"
#include "WebFileChooserCompletionImpl.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebIconLoadingCompletionImpl.h"
#include "WebInputEvent.h"
#include "WebKit.h"
#include "WebNode.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPopupMenuImpl.h"
#include "WebPopupMenuInfo.h"
#include "WebPopupType.h"
#include "platform/WebRect.h"
#include "WebSettings.h"
#include "WebTextDirection.h"
#include "platform/WebURLRequest.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWindowFeatures.h"
#include "WindowFeatures.h"
#include "WrappedResourceRequest.h"
#include <public/Platform.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/unicode/CharacterNames.h>

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

// Converts a WebCore::AXObjectCache::AXNotification to a WebKit::WebAccessibilityNotification
static WebAccessibilityNotification toWebAccessibilityNotification(AXObjectCache::AXNotification notification)
{
    // These enums have the same values; enforced in AssertMatchingEnums.cpp.
    return static_cast<WebAccessibilityNotification>(notification);
}

ChromeClientImpl::ChromeClientImpl(WebViewImpl* webView)
    : m_webView(webView)
    , m_toolbarsVisible(true)
    , m_statusbarVisible(true)
    , m_scrollbarsVisible(true)
    , m_menubarVisible(true)
    , m_resizable(true)
    , m_nextNewWindowNavigationPolicy(WebNavigationPolicyIgnore)
#if ENABLE(PAGE_POPUP)
    , m_pagePopupDriver(webView)
#endif
{
}

ChromeClientImpl::~ChromeClientImpl()
{
}

void* ChromeClientImpl::webView() const
{
    return static_cast<void*>(m_webView);
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
}

void ChromeClientImpl::focusedFrameChanged(Frame*)
{
}

Page* ChromeClientImpl::createWindow(
    Frame* frame, const FrameLoadRequest& r, const WindowFeatures& features, const NavigationAction& action)
{
    if (!m_webView->client())
        return 0;

    // FrameLoaderClientImpl may have given us a policy to use for the next new
    // window navigation. If not, determine the policy using the same logic as
    // show().
    WebNavigationPolicy policy;
    if (m_nextNewWindowNavigationPolicy != WebNavigationPolicyIgnore) {
        policy = m_nextNewWindowNavigationPolicy;
        m_nextNewWindowNavigationPolicy = WebNavigationPolicyIgnore;
    } else
        policy = getNavigationPolicy();

    WrappedResourceRequest request;
    if (!r.resourceRequest().isEmpty())
        request.bind(r.resourceRequest());
    else if (!action.resourceRequest().isEmpty())
        request.bind(action.resourceRequest());
    WebViewImpl* newView = static_cast<WebViewImpl*>(
        m_webView->client()->createView(WebFrameImpl::fromFrame(frame), request, features, r.frameName(), policy));
    if (!newView)
        return 0;

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

WebNavigationPolicy ChromeClientImpl::getNavigationPolicy()
{
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
    return policy;
}

void ChromeClientImpl::show()
{
    if (!m_webView->client())
        return;

    m_webView->client()->show(getNavigationPolicy());
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
    WebFrameImpl* webFrame = static_cast<WebFrameImpl*>(m_webView->mainFrame());
    if (webFrame)
        webFrame->setCanHaveScrollbars(value);
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

KeyboardUIMode ChromeClientImpl::keyboardUIMode()
{
    return m_webView->tabsToLinks() ? KeyboardAccessTabsToLinks : KeyboardAccessDefault;
}

IntRect ChromeClientImpl::windowResizerRect() const
{
    IntRect result;
    if (m_webView->client())
        result = m_webView->client()->windowResizerRect();
    return result;
}

void ChromeClientImpl::invalidateRootView(const IntRect&, bool)
{
    notImplemented();
}

void ChromeClientImpl::invalidateContentsAndRootView(const IntRect& updateRect, bool /*immediate*/)
{
    if (updateRect.isEmpty())
        return;
#if USE(ACCELERATED_COMPOSITING)
    if (!m_webView->isAcceleratedCompositingActive()) {
#endif
        if (m_webView->client())
            m_webView->client()->didInvalidateRect(updateRect);
#if USE(ACCELERATED_COMPOSITING)
    } else
        m_webView->invalidateRootLayerRect(updateRect);
#endif
}

void ChromeClientImpl::invalidateContentsForSlowScroll(const IntRect& updateRect, bool immediate)
{
    invalidateContentsAndRootView(updateRect, immediate);
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void ChromeClientImpl::scheduleAnimation()
{
    m_webView->scheduleAnimation();
}
#endif

void ChromeClientImpl::scroll(
    const IntSize& scrollDelta, const IntRect& scrollRect,
    const IntRect& clipRect)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_webView->isAcceleratedCompositingActive()) {
#endif
        if (m_webView->client()) {
            int dx = scrollDelta.width();
            int dy = scrollDelta.height();
            m_webView->client()->didScrollRect(dx, dy, intersection(scrollRect, clipRect));
        }
#if USE(ACCELERATED_COMPOSITING)
    } else
        m_webView->scrollRootLayerRect(scrollDelta, clipRect);
#endif
}

IntPoint ChromeClientImpl::screenToRootView(const IntPoint& point) const
{
    IntPoint windowPoint(point);

    if (m_webView->client()) {
        WebRect windowRect = m_webView->client()->windowRect();
        windowPoint.move(-windowRect.x, -windowRect.y);
    }

    return windowPoint;
}

IntRect ChromeClientImpl::rootViewToScreen(const IntRect& rect) const
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
    m_webView->didChangeContentsSize();

    WebFrameImpl* webframe = WebFrameImpl::fromFrame(frame);
    webframe->didChangeContentsSize(size);
    if (webframe->client())
        webframe->client()->didChangeContentsSize(webframe, size);
}

void ChromeClientImpl::layoutUpdated(Frame* frame) const
{
    m_webView->layoutUpdated(WebFrameImpl::fromFrame(frame));
}

void ChromeClientImpl::scrollbarsModeDidChange() const
{
}

void ChromeClientImpl::mouseDidMoveOverElement(
    const HitTestResult& result, unsigned modifierFlags)
{
    if (!m_webView->client())
        return;

    WebURL url;
    // Find out if the mouse is over a link, and if so, let our UI know...
    if (result.isLiveLink() && !result.absoluteLinkURL().string().isEmpty())
        url = result.absoluteLinkURL();
    else if (result.innerNonSharedNode()
             && (result.innerNonSharedNode()->hasTagName(HTMLNames::objectTag)
                 || result.innerNonSharedNode()->hasTagName(HTMLNames::embedTag))) {
        RenderObject* object = result.innerNonSharedNode()->renderer();
        if (object && object->isWidget()) {
            Widget* widget = toRenderWidget(object)->widget();
            if (widget && widget->isPluginContainer()) {
                WebPluginContainerImpl* plugin = static_cast<WebPluginContainerImpl*>(widget);
                url = plugin->plugin()->linkAtPosition(result.roundedPoint());
            }
        }
    }

    m_webView->client()->setMouseOverURL(url);
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

void ChromeClientImpl::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
#if ENABLE(VIEWPORT)
    if (!m_webView->settings()->viewportEnabled() || !m_webView->isFixedLayoutModeEnabled() || !m_webView->client() || !m_webView->page())
        return;

    ViewportArguments args;
    if (arguments == args) {
        // Default viewport arguments passed in. This is a signal to reset the viewport.
        args.width = ViewportArguments::ValueDesktopWidth;
    } else
        args = arguments;

    FrameView* frameView = m_webView->mainFrameImpl()->frameView();
    int dpi = screenHorizontalDPI(frameView);
    ASSERT(dpi > 0);

    WebViewClient* client = m_webView->client();
    WebRect deviceRect = client->windowRect();
    // If the window size has not been set yet don't attempt to set the viewport
    if (!deviceRect.width || !deviceRect.height)
        return;

    Settings* settings = m_webView->page()->settings();
    // Call the common viewport computing logic in ViewportArguments.cpp.
    ViewportAttributes computed = computeViewportAttributes(
        args, settings->layoutFallbackWidth(), deviceRect.width, deviceRect.height,
        dpi / ViewportArguments::deprecatedTargetDPI, IntSize(deviceRect.width, deviceRect.height));

    if (m_webView->ignoreViewportTagMaximumScale()) {
        computed.maximumScale = max(computed.maximumScale, m_webView->maxPageScaleFactor);
        computed.userScalable = true;
    }

    int layoutWidth = computed.layoutSize.width();
    int layoutHeight = computed.layoutSize.height();
    m_webView->setFixedLayoutSize(IntSize(layoutWidth, layoutHeight));

    bool needInitializePageScale = !m_webView->isPageScaleFactorSet();
    m_webView->setDeviceScaleFactor(computed.devicePixelRatio);
    m_webView->setPageScaleFactorLimits(computed.minimumScale, computed.maximumScale);
    if (needInitializePageScale)
        m_webView->setPageScaleFactorPreservingScrollOffset(computed.initialScale * computed.devicePixelRatio);
#endif
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

void ChromeClientImpl::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    ASSERT_NOT_REACHED();
}

void ChromeClientImpl::reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t)
{
    ASSERT_NOT_REACHED();
}

#if ENABLE(INPUT_TYPE_COLOR)
PassOwnPtr<ColorChooser> ChromeClientImpl::createColorChooser(ColorChooserClient* chooserClient, const Color&)
{
    return adoptPtr(new ColorChooserUIController(this, chooserClient));
}
PassOwnPtr<WebColorChooser> ChromeClientImpl::createWebColorChooser(WebColorChooserClient* chooserClient, const WebColor& initialColor)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return nullptr;
    return adoptPtr(client->createColorChooser(chooserClient, initialColor));
}
#endif

void ChromeClientImpl::runOpenPanel(Frame* frame, PassRefPtr<FileChooser> fileChooser)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return;

    WebFileChooserParams params;
    params.multiSelect = fileChooser->settings().allowsMultipleFiles;
#if ENABLE(DIRECTORY_UPLOAD)
    params.directory = fileChooser->settings().allowsDirectoryUpload;
#else
    params.directory = false;
#endif
    params.acceptTypes = fileChooser->settings().acceptTypes();
    params.selectedFiles = fileChooser->settings().selectedFiles;
    if (params.selectedFiles.size() > 0)
        params.initialValue = params.selectedFiles[0];
#if ENABLE(MEDIA_CAPTURE)
    params.capture = fileChooser->settings().capture;
#else
    params.capture = WebString();
#endif
    WebFileChooserCompletionImpl* chooserCompletion =
        new WebFileChooserCompletionImpl(fileChooser);

    if (client->runFileChooser(params, chooserCompletion))
        return;

    // Choosing failed, so do callback with an empty list.
    chooserCompletion->didChooseFile(WebVector<WebString>());
}

void ChromeClientImpl::loadIconForFiles(const Vector<String>& filenames, FileIconLoader* loader)
{
    if (!m_webView->client())
        return;
    WebIconLoadingCompletionImpl* iconCompletion = new WebIconLoadingCompletionImpl(loader);
    if (!m_webView->client()->queryIconForFiles(filenames, iconCompletion))
        iconCompletion->didLoadIcon(WebData());
}

#if ENABLE(DIRECTORY_UPLOAD)
void ChromeClientImpl::enumerateChosenDirectory(FileChooser* fileChooser)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return;

    WebFileChooserCompletionImpl* chooserCompletion =
        new WebFileChooserCompletionImpl(fileChooser);

    ASSERT(fileChooser && fileChooser->settings().selectedFiles.size());

    // If the enumeration can't happen, call the callback with an empty list.
    if (!client->enumerateChosenDirectory(fileChooser->settings().selectedFiles[0], chooserCompletion))
        chooserCompletion->didChooseFile(WebVector<WebString>());
}
#endif

void ChromeClientImpl::popupOpened(PopupContainer* popupContainer,
                                   const IntRect& bounds,
                                   bool handleExternally)
{
    // For Autofill popups, if the popup will not be fully visible, we shouldn't
    // show it at all. Among other things, this prevents users from being able
    // to interact via the keyboard with an invisible popup.
    if (popupContainer->popupType() == PopupContainer::Suggestion) {
        FrameView* view = m_webView->page()->mainFrame()->view();
        IntRect visibleRect = view->visibleContentRect(true /* include scrollbars */);
        // |bounds| is in screen coordinates, so make sure to convert it to
        // content coordinates prior to comparing to |visibleRect|.
        IntRect screenRect = bounds;
        screenRect.setLocation(view->screenToContents(bounds.location()));
        if (!visibleRect.contains(screenRect)) {
            m_webView->hideAutofillPopup();
            return;
        }
    }

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
    static_cast<WebPopupMenuImpl*>(webwidget)->init(popupContainer, bounds);
}

void ChromeClientImpl::popupClosed(WebCore::PopupContainer* popupContainer)
{
    m_webView->popupClosed(popupContainer);
}

void ChromeClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    setCursor(WebCursorInfo(cursor));
}

void ChromeClientImpl::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
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

void ChromeClientImpl::setNewWindowNavigationPolicy(WebNavigationPolicy policy)
{
    m_nextNewWindowNavigationPolicy = policy;
}

void ChromeClientImpl::formStateDidChange(const Node* node)
{
    if (m_webView->client())
        m_webView->client()->didChangeFormState(WebNode(const_cast<Node*>(node)));

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

    WebVector<WebMenuItemInfo> outputItems(inputItems.size());

    for (size_t i = 0; i < inputItems.size(); ++i) {
        const PopupItem& inputItem = *inputItems[i];
        WebMenuItemInfo& outputItem = outputItems[i];

        outputItem.label = inputItem.label;
        outputItem.enabled = inputItem.enabled;
        if (inputItem.textDirection == WebCore::RTL)
            outputItem.textDirection = WebTextDirectionRightToLeft;
        else
            outputItem.textDirection = WebTextDirectionLeftToRight;
        outputItem.hasTextDirectionOverride = inputItem.hasTextDirectionOverride;

        switch (inputItem.type) {
        case PopupItem::TypeOption:
            outputItem.type = WebMenuItemInfo::Option;
            break;
        case PopupItem::TypeGroup:
            outputItem.type = WebMenuItemInfo::Group;
            break;
        case PopupItem::TypeSeparator:
            outputItem.type = WebMenuItemInfo::Separator;
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

void ChromeClientImpl::postAccessibilityNotification(AccessibilityObject* obj, AXObjectCache::AXNotification notification)
{
    // Alert assistive technology about the accessibility object notification.
    if (obj)
        m_webView->client()->postAccessibilityNotification(WebAccessibilityObject(obj), toWebAccessibilityNotification(notification));
}

bool ChromeClientImpl::paintCustomOverhangArea(GraphicsContext* context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    Frame* frame = m_webView->mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        return pluginContainer->paintCustomOverhangArea(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect);
    return false;
}

#if USE(ACCELERATED_COMPOSITING)
void ChromeClientImpl::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer)
{
    m_webView->setRootGraphicsLayer(graphicsLayer);
}

void ChromeClientImpl::scheduleCompositingLayerSync()
{
    m_webView->scheduleCompositingLayerSync();
}

ChromeClient::CompositingTriggerFlags ChromeClientImpl::allowedCompositingTriggers() const
{
    if (!m_webView->allowsAcceleratedCompositing())
        return 0;

    CompositingTriggerFlags flags = 0;
    Settings* settings = m_webView->page()->settings();
    if (settings->acceleratedCompositingFor3DTransformsEnabled())
        flags |= ThreeDTransformTrigger;
    if (settings->acceleratedCompositingForVideoEnabled())
        flags |= VideoTrigger;
    if (settings->acceleratedCompositingForPluginsEnabled())
        flags |= PluginTrigger;
    if (settings->acceleratedCompositingForAnimationEnabled())
        flags |= AnimationTrigger;
    if (settings->acceleratedCompositingForCanvasEnabled())
        flags |= CanvasTrigger;

    return flags;
}
#endif

bool ChromeClientImpl::supportsFullscreenForNode(const Node* node)
{
    return false;
}

void ChromeClientImpl::enterFullscreenForNode(Node* node)
{
    ASSERT_NOT_REACHED();
}

void ChromeClientImpl::exitFullscreenForNode(Node* node)
{
    ASSERT_NOT_REACHED();
}

#if ENABLE(FULLSCREEN_API)
bool ChromeClientImpl::supportsFullScreenForElement(const Element* element, bool withKeyboard)
{
    return true;
}

void ChromeClientImpl::enterFullScreenForElement(Element* element)
{
    m_webView->enterFullScreenForElement(element);
}

void ChromeClientImpl::exitFullScreenForElement(Element* element)
{
    m_webView->exitFullScreenForElement(element);
}

void ChromeClientImpl::fullScreenRendererChanged(RenderBox*)
{
    notImplemented();
}
#endif

bool ChromeClientImpl::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool ChromeClientImpl::selectItemAlignmentFollowsMenuWritingDirection()
{
    return true;
}

bool ChromeClientImpl::hasOpenedPopup() const
{
    return m_webView->hasOpenedPopup();
}

PassRefPtr<PopupMenu> ChromeClientImpl::createPopupMenu(PopupMenuClient* client) const
{
    if (WebViewImpl::useExternalPopupMenus())
        return adoptRef(new ExternalPopupMenu(client, m_webView->client()));

    return adoptRef(new PopupMenuChromium(client));
}

PassRefPtr<SearchPopupMenu> ChromeClientImpl::createSearchPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuChromium(client));
}

#if ENABLE(PAGE_POPUP)
PagePopup* ChromeClientImpl::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    ASSERT(m_pagePopupDriver);
    return m_pagePopupDriver->openPagePopup(client, originBoundsInRootView);
}

void ChromeClientImpl::closePagePopup(PagePopup* popup)
{
    ASSERT(m_pagePopupDriver);
    m_pagePopupDriver->closePagePopup(popup);
}

void ChromeClientImpl::setPagePopupDriver(PagePopupDriver* driver)
{
    ASSERT(driver);
    m_pagePopupDriver = driver;
}

void ChromeClientImpl::resetPagePopupDriver()
{
    m_pagePopupDriver = m_webView;
}
#endif

bool ChromeClientImpl::willAddTextFieldDecorationsTo(HTMLInputElement* input)
{
    ASSERT(input);
    const Vector<OwnPtr<TextFieldDecorator> >& decorators = m_webView->textFieldDecorators();
    for (unsigned i = 0; i < decorators.size(); ++i) {
        if (decorators[i]->willAddDecorationTo(input))
            return true;
    }
    return false;
}

void ChromeClientImpl::addTextFieldDecorationsTo(HTMLInputElement* input)
{
    ASSERT(willAddTextFieldDecorationsTo(input));
    const Vector<OwnPtr<TextFieldDecorator> >& decorators = m_webView->textFieldDecorators();
    for (unsigned i = 0; i < decorators.size(); ++i) {
        if (!decorators[i]->willAddDecorationTo(input))
            continue;
        RefPtr<TextFieldDecorationElement> decoration = TextFieldDecorationElement::create(input->document(), decorators[i].get());
        decoration->decorate(input, decorators[i]->visibleByDefault());
    }
}

bool ChromeClientImpl::shouldRunModalDialogDuringPageDismissal(const DialogType& dialogType, const String& dialogMessage, FrameLoader::PageDismissalType dismissalType) const
{
    const char* kDialogs[] = {"alert", "confirm", "prompt", "showModalDialog"};
    int dialog = static_cast<int>(dialogType);
    ASSERT(0 <= dialog && dialog < static_cast<int>(arraysize(kDialogs)));

    const char* kDismissals[] = {"beforeunload", "pagehide", "unload"};
    int dismissal = static_cast<int>(dismissalType) - 1; // Exclude NoDismissal.
    ASSERT(0 <= dismissal && dismissal < static_cast<int>(arraysize(kDismissals)));

    WebKit::Platform::current()->histogramEnumeration("Renderer.ModalDialogsDuringPageDismissal", dismissal * arraysize(kDialogs) + dialog, arraysize(kDialogs) * arraysize(kDismissals));

    m_webView->mainFrame()->addMessageToConsole(WebConsoleMessage(WebConsoleMessage::LevelError, makeString("Blocked ", kDialogs[dialog], "('", dialogMessage, "') during ", kDismissals[dismissal], ".")));

    return false;
}

bool ChromeClientImpl::shouldRubberBandInDirection(WebCore::ScrollDirection direction) const
{
    ASSERT(direction != WebCore::ScrollUp && direction != WebCore::ScrollDown);

    if (!m_webView->client())
        return false;

    if (direction == WebCore::ScrollLeft)
        return !m_webView->client()->historyBackListCount();
    if (direction == WebCore::ScrollRight)
        return !m_webView->client()->historyForwardListCount();

    ASSERT_NOT_REACHED();
    return true;
}

void ChromeClientImpl::numWheelEventHandlersChanged(unsigned numberOfWheelHandlers)
{
    m_webView->numberOfWheelEventHandlersChanged(numberOfWheelHandlers);
}

#if ENABLE(TOUCH_EVENTS)
void ChromeClientImpl::needTouchEvents(bool needsTouchEvents)
{
    m_webView->hasTouchEventHandlers(needsTouchEvents);
}
#endif // ENABLE(TOUCH_EVENTS)

#if ENABLE(POINTER_LOCK)
bool ChromeClientImpl::requestPointerLock()
{
    return m_webView->requestPointerLock();
}

void ChromeClientImpl::requestPointerUnlock()
{
    return m_webView->requestPointerUnlock();
}

bool ChromeClientImpl::isPointerLocked()
{
    return m_webView->isPointerLocked();
}
#endif

#if ENABLE(WIDGET_REGION)
void ChromeClientImpl::dashboardRegionsChanged()
{
    WebViewClient* client = m_webView->client();
    if (client)
        client->draggableRegionsChanged();
}
#endif

#if ENABLE(REGISTER_PROTOCOL_HANDLER)
PassOwnPtr<RegisterProtocolHandlerClientImpl> RegisterProtocolHandlerClientImpl::create(WebViewImpl* webView)
{
    return adoptPtr(new RegisterProtocolHandlerClientImpl(webView));
}

RegisterProtocolHandlerClientImpl::RegisterProtocolHandlerClientImpl(WebViewImpl* webView)
    : m_webView(webView)
{
}

void RegisterProtocolHandlerClientImpl::registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title) 
{ 
    m_webView->client()->registerProtocolHandler(scheme, baseURL, url, title);
} 
#endif

} // namespace WebKit
