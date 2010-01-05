/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "HostWindow.h"
#include "Test.h"
#include <WebCore/COMPtr.h>
#include <WebKit/WebKit.h>
#include <WebKit/WebKitCOMAPI.h>
#include <wtf/PassOwnPtr.h>

namespace WebKitAPITest {

template <typename T>
static HRESULT WebKitCreateInstance(REFCLSID clsid, T** object)
{
    return WebKitCreateInstance(clsid, 0, __uuidof(T), reinterpret_cast<void**>(object));
}

static int webViewCount()
{
    COMPtr<IWebKitStatistics> statistics;
    if (FAILED(WebKitCreateInstance(__uuidof(WebKitStatistics), &statistics)))
        return -1;
    int count;
    if (FAILED(statistics->webViewCount(&count)))
        return -1;
    return count;
}

static COMPtr<IWebView> createWebView(const HostWindow& window)
{
    COMPtr<IWebView> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), &webView)))
        return 0;

    if (FAILED(webView->setHostWindow(reinterpret_cast<OLE_HANDLE>(window.window()))))
        return 0;

    if (FAILED(webView->initWithFrame(window.clientRect(), 0, 0)))
        return 0;

    return webView;
}

static void runMessagePump(DWORD timeoutMilliseconds)
{
    DWORD startTickCount = GetTickCount();
    MSG msg;
    BOOL result;
    while ((result = GetMessage(&msg, 0, 0, 0)) && GetTickCount() - startTickCount <= timeoutMilliseconds) {
        if (result == -1)
            break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Tests that calling IWebView::close without calling DestroyWindow, then releasing a WebView doesn't crash. <http://webkit.org/b/32827>
TEST(WebViewDestruction, CloseWithoutDestroyWindow)
{
    HostWindow window;
    TEST_ASSERT(window.initialize());
    COMPtr<IWebView> webView = createWebView(window);
    TEST_ASSERT(webView);

    TEST_ASSERT(SUCCEEDED(webView->close()));

    // Allow window messages to be processed to trigger the crash.
    runMessagePump(50);

    // We haven't crashed. Release the WebView and ensure that its view window gets destroyed and the WebView doesn't leak.
    COMPtr<IWebViewPrivate> viewPrivate(Query, webView);
    TEST_ASSERT(viewPrivate);

    HWND viewWindow;
    TEST_ASSERT(SUCCEEDED(viewPrivate->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))));
    TEST_ASSERT(viewWindow);

    int currentWebViewCount = webViewCount();
    TEST_ASSERT(currentWebViewCount > 0);

    webView = 0;
    viewPrivate = 0;

    TEST_ASSERT(webViewCount() == currentWebViewCount - 1);
    TEST_ASSERT(!IsWindow(viewWindow));
}

// Tests that calling IWebView::mainFrame after calling IWebView::close doesn't crash. <http://webkit.org/b/32868>
TEST(WebViewDestruction, MainFrameAfterClose)
{
    HostWindow window;
    TEST_ASSERT(window.initialize());
    COMPtr<IWebView> webView = createWebView(window);
    TEST_ASSERT(webView);

    TEST_ASSERT(SUCCEEDED(webView->close()));
    COMPtr<IWebFrame> mainFrame;
    TEST_ASSERT(SUCCEEDED(webView->mainFrame(&mainFrame)));
}

// Tests that releasing a WebView without calling IWebView::close or DestroyWindow doesn't leak. <http://webkit.org/b/33162>
TEST(WebViewDestruction, NoCloseOrDestroyWindow)
{
    HostWindow window;
    TEST_ASSERT(window.initialize());
    COMPtr<IWebView> webView = createWebView(window);
    TEST_ASSERT(webView);

    webView = 0;
    TEST_ASSERT(!webViewCount());
}

} // namespace WebKitAPITest
