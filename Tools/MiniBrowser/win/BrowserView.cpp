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

#include "StdAfx.h"
#include "BrowserView.h"

#include "BrowserWindow.h"
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKURLCF.h>

static const unsigned short HIGH_BIT_MASK_SHORT = 0x8000;

BrowserView::BrowserView()
    : m_webView(0)
{
}

// UI Client Callbacks

static WKPageRef createNewPage(WKPageRef page, WKURLRequestRef request, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton mouseButton, const void* clientInfo)
{
    BrowserWindow* browserWindow = BrowserWindow::create();
    browserWindow->createWindow(0, 0, 800, 600);

    return WKViewGetPage(browserWindow->view().webView());
}

static void showPage(WKPageRef page, const void *clientInfo)
{
    static_cast<BrowserWindow*>(const_cast<void*>(clientInfo))->showWindow();
}

static void closePage(WKPageRef page, const void *clientInfo)
{
}

static void runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{
}

static bool runJavaScriptConfirm(WKPageRef page, WKStringRef message, WKFrameRef frame, const void* clientInfo)
{
    return false;
}

static WKStringRef runJavaScriptPrompt(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, const void* clientInfo)
{
    return 0;
}

static void setStatusText(WKPageRef page, WKStringRef text, const void* clientInfo)
{
}

static void mouseDidMoveOverElement(WKPageRef page, WKHitTestResultRef hitTestResult, WKEventModifiers modifiers, WKTypeRef userData, const void *clientInfo)
{
}

void BrowserView::create(RECT webViewRect, BrowserWindow* parentWindow)
{
    assert(!m_webView);

    static WKContextRef context = WKContextCreate();

    m_webView = WKViewCreate(webViewRect, context, 0, parentWindow->window());

    WKPageUIClient uiClient = {
        kWKPageUIClientCurrentVersion,
        parentWindow,   /* clientInfo */
        0,          /* createNewPage_deprecatedForUseWithV0 */
        showPage,
        closePage,
        0,          /* takeFocus */
        0,          /* focus */
        0,          /* unfocus */
        runJavaScriptAlert,
        runJavaScriptConfirm,
        runJavaScriptPrompt,
        setStatusText,
        0,          /* mouseDidMoveOverElement_deprecatedForUseWithV0 */
        0,          /* missingPluginButtonClicked */
        0,          /* didNotHandleKeyEvent */
        0,          /* didNotHandleWheelEvent */
        0,          /* toolbarsAreVisible */
        0,          /* setToolbarsAreVisible */
        0,          /* menuBarIsVisible */
        0,          /* setMenuBarIsVisible */
        0,          /* statusBarIsVisible */
        0,          /* setStatusBarIsVisible */
        0,          /* isResizable */
        0,          /* setIsResizable */
        0,          /* getWindowFrame */
        0,          /* setWindowFrame */
        0,          /* runBeforeUnloadConfirmPanel */
        0,          /* didDraw */
        0,          /* pageDidScroll */
        0,          /* exceededDatabaseQuota */
        0,          /* runOpenPanel */
        0,          /* decidePolicyForGeolocationPermissionRequest */
        0,          /* headerHeight */
        0,          /* footerHeight */
        0,          /* drawHeader */
        0,          /* drawFooter */
        0,          /* printFrame */
        0,          /* runModal */
        0,          /* didCompleteRubberBandForMainFrame */
        0,          /* saveDataToFileInDownloadsFolder */
        0,          /* shouldInterruptJavaScript */
        createNewPage,
        mouseDidMoveOverElement,
    };

    WKPageSetPageUIClient(WKViewGetPage(m_webView), &uiClient);

    WKViewSetIsInWindow(m_webView, true);
}

void BrowserView::setFrame(RECT rect)
{
    HWND webViewWindow = WKViewGetWindow(m_webView);
    ::SetWindowPos(webViewWindow, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

void BrowserView::goToURL(const std::wstring& urlString)
{
    CFStringRef string = CFStringCreateWithCharacters(0, (const UniChar*)urlString.data(), urlString.size());
    CFStringRef escapedString = CFURLCreateStringByAddingPercentEscapes(0, string, 0, 0, kCFStringEncodingUTF8);
    CFRelease(string);
    CFURLRef cfURL = CFURLCreateWithString(0, escapedString, 0);
    CFRelease(escapedString);

    WKURLRef url = WKURLCreateWithCFURL(cfURL);
    CFRelease(cfURL); 

    WKPageRef page = WKViewGetPage(m_webView);
    WKPageLoadURL(page, url);
    WKRelease(url);
}
