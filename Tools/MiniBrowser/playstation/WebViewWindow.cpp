/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "WebViewWindow.h"

#include "MainWindow.h"
#include "StringUtils.h"
#include "ToolkittenUtils.h"
#include <KeyboardEvents.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPagePrivatePlayStation.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKURL.h>
#include <cairo/cairo.h>
#include <map>
#include <toolkitten/Application.h>
#include <toolkitten/Cursor.h>
#include <toolkitten/MessageDialog.h>

using namespace toolkitten;

inline WebViewWindow* toWebView(const void* clientInfo)
{
    return const_cast<WebViewWindow*>(static_cast<const WebViewWindow*>(clientInfo));
}

static void setCursor(WKCursorType cursorType)
{
    switch (cursorType) {
    case kWKCursorTypeHand:
        Cursor::singleton().setType(Cursor::kTypeHand);
        break;
    case kWKCursorTypePointer:
    default:
        Cursor::singleton().setType(Cursor::kTypePointer);
        break;
    }
}

std::unique_ptr<WebViewWindow> WebViewWindow::create(Client&& windowClient, WKPageConfigurationRef configurationRef)
{
    auto context = WebContext::singleton();

    if (configurationRef)
        return std::make_unique<WebViewWindow>(configurationRef, std::move(windowClient));

    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(configuration.get(), context->context());
    WKPageConfigurationSetPageGroup(configuration.get(), context->pageGroup());
    return std::make_unique<WebViewWindow>(configuration.get(), std::move(windowClient));
}

WebViewWindow::WebViewWindow(WKPageConfigurationRef configuration, Client&& windowClient)
    : m_client(std::move(windowClient))
{
    // FIXME: Should child share the preference with parent?
    m_context = WebContext::singleton();
    m_preferences = WKPreferencesCreateCopy(m_context->preferences());
    WKPageConfigurationSetPreferences(configuration, m_preferences.get());

    WKPreferencesSetAcceleratedCompositingEnabled(m_preferences.get(), false);
    WKPreferencesSetFullScreenEnabled(m_preferences.get(), true);

    m_view = WKViewCreate(configuration);
    m_context->addWindow(this);

    WKViewClientV0 viewClient {
        { 0, this },

        // setViewNeedsDisplay
        [](WKViewRef, WKRect rect, const void* clientInfo) {
            toWebView(clientInfo)->updateSurface(toTKRect(rect));
        },

        // enterFullScreen
        [](WKViewRef view, const void*) {
            WKViewWillEnterFullScreen(view);
            WKViewDidEnterFullScreen(view);
        },
        // exitFullScreen
        [](WKViewRef view, const void*) {
            WKViewWillExitFullScreen(view);
            WKViewDidExitFullScreen(view);
        },
        nullptr, // closeFullScreen
        nullptr, // beganEnterFullScreen
        nullptr, // beganExitFullScreen

        // setCursor
        [](WKViewRef, WKCursorType cursorType, const void*) {
            setCursor(cursorType);
        }
    };
    WKViewSetViewClient(m_view.get(), &viewClient.base);

    WKPageStateClientV0 pageStateClient {
        { 0, this },

        nullptr, // willChangeIsLoading
        nullptr, // didChangeIsLoading
        nullptr, // willChangeTitle
        // didChangeTitle
        [](const void* clientInfo) {
            toWebView(clientInfo)->updateTitle();
        },
        nullptr, // willChangeActiveURL
        // didChangeActiveURL
        [](const void* clientInfo) {
            toWebView(clientInfo)->updateURL();
        },
        nullptr, // willChangeHasOnlySecureContent
        nullptr, // didChangeHasOnlySecureContent
        nullptr, // willChangeEstimatedProgress
        // didChangeEstimatedProgress
        [](const void* clientInfo) {
            toWebView(clientInfo)->updateProgress();
        },
        nullptr, // willChangeCanGoBack
        nullptr, // didChangeCanGoBack
        nullptr, // willChangeCanGoForward
        nullptr, // didChangeCanGoForward
        nullptr, // willChangeNetworkRequestsInProgress
        nullptr, // didChangeNetworkRequestsInProgress
        nullptr, // willChangeCertificateInfo
        nullptr, // didChangeCertificateInfo
        nullptr, // willChangeWebProcessIsResponsive
        nullptr, // didChangeWebProcessIsResponsive
        nullptr, // didSwapWebProcesses
    };
    WKPageSetPageStateClient(page(), &pageStateClient.base);

    WKPageUIClientV6 uiClient {
        { 6, this },
        nullptr, // createNewPage_deprecatedForUseWithV0
        nullptr, // showPage
        nullptr, // close
        nullptr, // takeFocus
        nullptr, // focus
        nullptr, // unfocus
        nullptr, // runJavaScriptAlert_deprecatedForUseWithV0
        nullptr, // runJavaScriptConfirm_deprecatedForUseWithV0
        nullptr, // runJavaScriptPrompt_deprecatedForUseWithV0
        nullptr, // setStatusText
        nullptr, // mouseDidMoveOverElement_deprecatedForUseWithV0
        nullptr, // missingPluginButtonClicked_deprecatedForUseWithV0
        nullptr, // didNotHandleKeyEvent
        nullptr, // didNotHandleWheelEvent
        nullptr, // toolbarsAreVisible
        nullptr, // setToolbarsAreVisible
        nullptr, // menuBarIsVisible
        nullptr, // setMenuBarIsVisible
        nullptr, // statusBarIsVisible
        nullptr, // setStatusBarIsVisible
        nullptr, // isResizable
        nullptr, // setIsResizable
        nullptr, // getWindowFrame
        nullptr, // setWindowFrame
        nullptr, // runBeforeUnloadConfirmPanel
        nullptr, // didDraw
        nullptr, // pageDidScroll
        nullptr, // exceededDatabaseQuota
        nullptr, // runOpenPanel
        nullptr, // decidePolicyForGeolocationPermissionRequest
        nullptr, // headerHeight
        nullptr, // footerHeight
        nullptr, // drawHeader
        nullptr, // drawFooter
        nullptr, // printFrame
        nullptr, // runModal
        nullptr, // Used to be didCompleteRubberBandForMainFrame
        nullptr, // saveDataToFileInDownloadsFolder
        nullptr, // shouldInterruptJavaScript_unavailable
        nullptr, // createNewPage_deprecatedForUseWithV1
        nullptr, // mouseDidMoveOverElement
        nullptr, // decidePolicyForNotificationPermissionRequest
        nullptr, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        nullptr, // showColorPicker
        nullptr, // hideColorPicker
        nullptr, // unavailablePluginButtonClicked
        nullptr, // pinnedStateDidChange
        nullptr, // Used to be didBeginTrackingPotentialLongMousePress
        nullptr, // Used to be didRecognizeLongMousePress
        nullptr, // Used to be didCancelTrackingPotentialLongMousePress
        nullptr, // isPlayingAudioDidChange
        nullptr, // decidePolicyForUserMediaPermissionRequest
        nullptr, // didClickAutoFillButton
        nullptr, // runJavaScriptAlert_deprecatedForUseWithV5
        nullptr, // runJavaScriptConfirm_deprecatedForUseWithV5
        nullptr, // runJavaScriptPrompt_deprecatedForUseWithV5
        nullptr, // mediaSessionMetadataDidChange
        // createNewPage
        [](WKPageRef page, WKPageConfigurationRef configuration, WKNavigationActionRef, WKWindowFeaturesRef, const void* clientInfo) ->WKPageRef {
            return toWebView(clientInfo)->createNewPage(page, configuration);
        },
        nullptr, // runJavaScriptAlert
        nullptr, // runJavaScriptConfirm
        nullptr, // runJavaScriptPrompt
        nullptr // checkUserMediaPermissionForOrigin
    };
    WKPageSetPageUIClient(page(), &uiClient.base);
}

WebViewWindow::~WebViewWindow()
{
    m_context->removeWindow(this);
    WKPageClose(page());
}

WKPageRef WebViewWindow::page()
{
    return WKViewGetPage(m_view.get());
}

void WebViewWindow::loadURL(const char* url)
{
    WKRetainPtr<WKURLRef> urlStr = adoptWK(WKURLCreateWithUTF8CString(url));
    WKPageLoadURL(page(), urlStr.get());
}

void WebViewWindow::goBack()
{
    WKPageGoBack(page());
}

void WebViewWindow::goForward()
{
    WKPageGoForward(page());
}

void WebViewWindow::reload()
{
    WKPageReloadFromOrigin(page());
}

void WebViewWindow::toggleZoomFactor()
{
    const int maxZoomFactor = 3;
    int nextZoomFactor = (int)WKPageGetPageZoomFactor(page()) + 1;
    if (nextZoomFactor > maxZoomFactor)
        nextZoomFactor = 1;

    WKPageSetPageZoomFactor(page(), nextZoomFactor);
}

void WebViewWindow::setSize(toolkitten::IntSize size)
{
    Widget::setSize(size);
    WKViewSetSize(m_view.get(), toWKSize(size));

    size_t surfaceSize = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, this->m_size.w) * this->m_size.h;
    m_surface = std::make_unique<unsigned char[]>(surfaceSize);
    memset(m_surface.get(), 0xff, surfaceSize);
}

bool WebViewWindow::onKeyUp(int32_t virtualKeyCode)
{
    switch (virtualKeyCode) {
    case VK_RETURN:
        return false;
    case VK_TRIANGLE:
    case VK_SQUARE:
    case VK_OPTIONS:
    case VK_L1:
    case VK_R1:
    case VK_L2:
    case VK_R2:
    case VK_L3:
    case VK_R3:
        // Ignore these key code.
        return false;
    }

    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyUp, kWKInputTypeNormal, "", 0, keyIdentifierForKeyCode(virtualKeyCode), virtualKeyCode, -1, 0, 0));
    return true;
}

bool WebViewWindow::onKeyDown(int32_t virtualKeyCode)
{
    switch (virtualKeyCode) {
    case VK_RETURN:
        return false;
    case VK_TRIANGLE:
    case VK_SQUARE:
    case VK_OPTIONS:
    case VK_L1:
    case VK_R1:
    case VK_L2:
    case VK_R2:
    case VK_L3:
    case VK_R3:
        // Ignore these key code.
        return false;
    }
    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyDown, kWKInputTypeNormal, "", 0, keyIdentifierForKeyCode(virtualKeyCode), virtualKeyCode, -1, 0, 0));
    return true;
}

bool WebViewWindow::onMouseMove(toolkitten::IntPoint point)
{
    point = globalToClientPosition(point);
    WKPageHandleMouseEvent(page(), WKMouseEventMake(kWKEventMouseMove, kWKEventMouseButtonNoButton, WKPointMake(point.x, point.y), 0, 0));
    setFocused();
    return true;
}

bool WebViewWindow::onMouseDown(toolkitten::IntPoint point)
{
    point = globalToClientPosition(point);

    WKPageHandleMouseEvent(page(), WKMouseEventMake(kWKEventMouseDown, kWKEventMouseButtonLeftButton, WKPointMake(point.x, point.y), 0, 0));
    return true;
}

bool WebViewWindow::onMouseUp(toolkitten::IntPoint point)
{
    point = globalToClientPosition(point);
    WKPageHandleMouseEvent(page(), WKMouseEventMake(kWKEventMouseUp, kWKEventMouseButtonLeftButton, WKPointMake(point.x, point.y), 0, 0));
    return true;
}

bool WebViewWindow::onWheelMove(toolkitten::IntPoint point, toolkitten::IntPoint wheelTicks)
{
    point = globalToClientPosition(point);

    int wheelTicksX = wheelTicks.x;
    int wheelTicksY = wheelTicks.y;
    if (wheelTicksX || wheelTicksY) {
        int deltaX = wheelTicksX * 10;
        int deltaY = wheelTicksY * 10;

        WKPageHandleWheelEvent(page(), WKWheelEventMake(kWKEventWheel, WKPointMake(point.x, point.y), WKSizeMake(deltaX, deltaY), WKSizeMake(wheelTicksX, wheelTicksY), 0));
        return true;
    }
    return false;
}

void WebViewWindow::paintSelf(IntPoint position)
{
    if (!dirtyRects().empty()) {
        cairo_surface_t* wkviewSurface = cairo_image_surface_create_for_data(m_surface.get(), CAIRO_FORMAT_ARGB32, this->m_size.w, this->m_size.h, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, this->m_size.w));
        std::list<toolkitten::IntRect>::const_iterator it = dirtyRects().begin();
        toolkitten::IntRect unionRect = *it;
        it++;
        while (it != dirtyRects().end()) {
            unionRect += *it;
            it++;
        }
        if (cairo_surface_status(wkviewSurface) == CAIRO_STATUS_SUCCESS) {
            cairo_t* cr = cairo_create(this->surface());
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_surface(cr, wkviewSurface, 0, 0);
            cairo_rectangle(cr, unionRect.left(), unionRect.top(), unionRect.width(), unionRect.height());
            cairo_fill(cr);
            cairo_destroy(cr);
            cairo_surface_destroy(wkviewSurface);
        }
    }
    if (m_active)
        Widget::paintSelf(position);
}

void WebViewWindow::updateSelf()
{
}

void WebViewWindow::updateSurface(toolkitten::IntRect dirtyRect)
{
    WKPagePaint(page(), m_surface.get(), WKSizeMake(this->m_size.w, this->m_size.h), toWKRect(dirtyRect));
    m_dirtyRects += dirtyRect;
}

void WebViewWindow::updateTitle()
{
    WKRetainPtr<WKStringRef> title = adoptWK(WKPageCopyTitle(page()));
    if (!title)
        return;

    m_title = toUTF8String(title.get());
    if (m_client.didUpdateTitle)
        m_client.didUpdateTitle(this);
}

void WebViewWindow::updateURL()
{
    WKRetainPtr<WKURLRef> url = adoptWK(WKPageCopyActiveURL(page()));
    if (!url)
        return;

    WKRetainPtr<WKStringRef> urlStr = adoptWK(WKURLCopyString(url.get()));
    m_url = toUTF8String(urlStr.get());
    if (m_client.didUpdateURL)
        m_client.didUpdateURL(this);
}

void WebViewWindow::updateProgress()
{
    m_progress = WKPageGetEstimatedProgress(page());
    if (m_client.didUpdateProgress)
        m_client.didUpdateProgress(this);
}

void WebViewWindow::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
}

WKPageRef WebViewWindow::createNewPage(WKPageRef, WKPageConfigurationRef configuration)
{
    if (m_client.createNewWindow) {
        auto newWebView = m_client.createNewWindow(configuration);
        auto newPage = newWebView->page();
        return newPage;
    }
    return nullptr;
}
