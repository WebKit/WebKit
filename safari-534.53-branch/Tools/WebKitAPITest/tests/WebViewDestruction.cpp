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

#include "config.h"

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

static void createAndInitializeWebView(COMPtr<IWebView>& outWebView, HostWindow& window, HWND& viewWindow)
{
    COMPtr<IWebView> webView;
    TEST_ASSERT(SUCCEEDED(WebKitCreateInstance(__uuidof(WebView), &webView)));

    TEST_ASSERT(window.initialize());
    TEST_ASSERT(SUCCEEDED(webView->setHostWindow(reinterpret_cast<OLE_HANDLE>(window.window()))));
    TEST_ASSERT(SUCCEEDED(webView->initWithFrame(window.clientRect(), 0, 0)));

    COMPtr<IWebViewPrivate> viewPrivate(Query, webView);
    TEST_ASSERT(viewPrivate);
    TEST_ASSERT(SUCCEEDED(viewPrivate->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))));
    TEST_ASSERT(IsWindow(viewWindow));

    outWebView.adoptRef(webView.releaseRef());
}

static void runMessagePump(DWORD timeoutMilliseconds)
{
    DWORD startTickCount = GetTickCount();
    MSG msg;
    BOOL result;
    while ((result = PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) && GetTickCount() - startTickCount <= timeoutMilliseconds) {
        if (result == -1)
            break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void finishWebViewDestructionTest(COMPtr<IWebView>& webView, HWND viewWindow)
{
    // Allow window messages to be processed, because in some cases that would trigger a crash (e.g., <http://webkit.org/b/32827>).
    runMessagePump(50);

    // We haven't crashed. Release the WebView and ensure that its view window has been destroyed and the WebView doesn't leak.
    int currentWebViewCount = webViewCount();
    TEST_ASSERT(currentWebViewCount > 0);

    webView = 0;

    TEST_ASSERT(webViewCount() == currentWebViewCount - 1);
    TEST_ASSERT(!IsWindow(viewWindow));
}

// Tests that releasing a WebView without calling IWebView::initWithFrame works.
TEST(WebViewDestruction, NoInitWithFrame)
{
    COMPtr<IWebView> webView;
    TEST_ASSERT(SUCCEEDED(WebKitCreateInstance(__uuidof(WebView), &webView)));

    finishWebViewDestructionTest(webView, 0);
}

TEST(WebViewDestruction, CloseWithoutInitWithFrame)
{
    COMPtr<IWebView> webView;
    TEST_ASSERT(SUCCEEDED(WebKitCreateInstance(__uuidof(WebView), &webView)));

    TEST_ASSERT(SUCCEEDED(webView->close()));

    finishWebViewDestructionTest(webView, 0);
}

// Tests that releasing a WebView without calling IWebView::close or DestroyWindow doesn't leak. <http://webkit.org/b/33162>
TEST(WebViewDestruction, NoCloseOrDestroyViewWindow)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    finishWebViewDestructionTest(webView, viewWindow);
}

// Tests that calling IWebView::close without calling DestroyWindow, then releasing a WebView doesn't crash. <http://webkit.org/b/32827>
TEST(WebViewDestruction, CloseWithoutDestroyViewWindow)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    TEST_ASSERT(SUCCEEDED(webView->close()));

    finishWebViewDestructionTest(webView, viewWindow);
}

TEST(WebViewDestruction, DestroyViewWindowWithoutClose)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    DestroyWindow(viewWindow);

    finishWebViewDestructionTest(webView, viewWindow);
}

TEST(WebViewDestruction, CloseThenDestroyViewWindow)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    TEST_ASSERT(SUCCEEDED(webView->close()));
    DestroyWindow(viewWindow);

    finishWebViewDestructionTest(webView, viewWindow);
}

TEST(WebViewDestruction, DestroyViewWindowThenClose)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    DestroyWindow(viewWindow);
    TEST_ASSERT(SUCCEEDED(webView->close()));

    finishWebViewDestructionTest(webView, viewWindow);
}

TEST(WebViewDestruction, DestroyHostWindow)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    DestroyWindow(window.window());

    finishWebViewDestructionTest(webView, viewWindow);
}

TEST(WebViewDestruction, DestroyHostWindowThenClose)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    DestroyWindow(window.window());
    TEST_ASSERT(SUCCEEDED(webView->close()));

    finishWebViewDestructionTest(webView, viewWindow);
}

TEST(WebViewDestruction, CloseThenDestroyHostWindow)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    TEST_ASSERT(SUCCEEDED(webView->close()));
    DestroyWindow(window.window());

    finishWebViewDestructionTest(webView, viewWindow);
}

// Tests that calling IWebView::mainFrame after calling IWebView::close doesn't crash. <http://webkit.org/b/32868>
TEST(WebViewDestruction, MainFrameAfterClose)
{
    COMPtr<IWebView> webView;
    HostWindow window;
    HWND viewWindow;
    createAndInitializeWebView(webView, window, viewWindow);

    TEST_ASSERT(SUCCEEDED(webView->close()));
    COMPtr<IWebFrame> mainFrame;
    TEST_ASSERT(SUCCEEDED(webView->mainFrame(&mainFrame)));

    finishWebViewDestructionTest(webView, viewWindow);
}

} // namespace WebKitAPITest
