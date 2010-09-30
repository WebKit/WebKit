/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef TestShell_h
#define TestShell_h

#include "AccessibilityController.h"
#include "EventSender.h"
#include "LayoutTestController.h"
#include "NotificationPresenter.h"
#include "PlainTextController.h"
#include "TestEventPrinter.h"
#include "TextInputController.h"
#include "WebPreferences.h"
#include "WebViewHost.h"
#include <string>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

// TestShell is a container of global variables and has bridge functions between
// various objects. Only one instance is created in one DRT process.

namespace WebKit {
class WebDevToolsAgentClient;
class WebFrame;
class WebNotificationPresenter;
class WebView;
class WebURL;
}
namespace skia {
class PlatformCanvas;
}

class DRTDevToolsAgent;
class DRTDevToolsCallArgs;
class DRTDevToolsClient;

struct TestParams {
    bool dumpTree;
    bool dumpPixels;
    bool printSeparators;
    WebKit::WebURL testUrl;
    // Resultant image file name. Reqruired only if the test_shell mode.
    std::string pixelFileName;
    std::string pixelHash;

    TestParams()
        : dumpTree(true)
        , dumpPixels(false)
        , printSeparators(false) {}
};

class TestShell {
public:
    TestShell(bool testShellMode);
    ~TestShell();

    // The main WebView.
    WebKit::WebView* webView() const { return m_webView; }
    // Returns the host for the main WebView.
    WebViewHost* webViewHost() const { return m_webViewHost; }
    LayoutTestController* layoutTestController() const { return m_layoutTestController.get(); }
    EventSender* eventSender() const { return m_eventSender.get(); }
    AccessibilityController* accessibilityController() const { return m_accessibilityController.get(); }
    NotificationPresenter* notificationPresenter() const { return m_notificationPresenter.get(); }
    TestEventPrinter* printer() const { return m_printer.get(); }

    WebPreferences* preferences() { return &m_prefs; }
    void applyPreferences() { m_prefs.applyTo(m_webView); }

    void bindJSObjectsToWindow(WebKit::WebFrame*);
    void runFileTest(const TestParams&);
    void callJSGC();
    void resetTestController();
    void waitTestFinished();

    // Operations to the main window.
    void loadURL(const WebKit::WebURL& url);
    void reload();
    void goToOffset(int offset);
    int navigationEntryCount() const;

    void setFocus(WebKit::WebWidget*, bool enable);
    bool shouldDumpFrameLoadCallbacks() const { return (m_testIsPreparing || m_testIsPending) && layoutTestController()->shouldDumpFrameLoadCallbacks(); }
    bool shouldDumpResourceLoadCallbacks() const  { return (m_testIsPreparing || m_testIsPending) && layoutTestController()->shouldDumpResourceLoadCallbacks(); }
    bool shouldDumpResourceResponseMIMETypes() const  { return (m_testIsPreparing || m_testIsPending) && layoutTestController()->shouldDumpResourceResponseMIMETypes(); }
    void setIsLoading(bool flag) { m_isLoading = flag; }

    // Called by the LayoutTestController to signal test completion.
    void testFinished();
    // Called by LayoutTestController when a test hits the timeout, but does not
    // cause a hang. We can avoid killing TestShell in this case and still dump
    // the test results.
    void testTimedOut();

    bool allowExternalPages() const { return m_allowExternalPages; }
    void setAllowExternalPages(bool allowExternalPages) { m_allowExternalPages = allowExternalPages; }

    void setAcceleratedCompositingEnabled(bool enabled) { m_acceleratedCompositingEnabled = enabled; }
    void setAccelerated2dCanvasEnabled(bool enabled) { m_accelerated2dCanvasEnabled = enabled; }

#if defined(OS_WIN)
    // Access to the finished event.  Used by the static WatchDog thread.
    HANDLE finishedEvent() { return m_finishedEvent; }
#endif

    // Get the timeout for running a test in milliseconds.
    int layoutTestTimeout() { return m_timeout; }
    int layoutTestTimeoutForWatchDog() { return layoutTestTimeout() + 1000; }
    void setLayoutTestTimeout(int timeout) { m_timeout = timeout; }

    WebViewHost* createWebView();
    WebViewHost* createNewWindow(const WebKit::WebURL&);
    void closeWindow(WebViewHost*);
    void closeRemainingWindows();
    int windowCount();
    static void resizeWindowForTest(WebViewHost*, const WebKit::WebURL&);

    void showDevTools();
    void closeDevTools();

    DRTDevToolsAgent* drtDevToolsAgent() { return m_drtDevToolsAgent.get(); }
    DRTDevToolsClient* drtDevToolsClient() { return m_drtDevToolsClient.get(); }

    static const int virtualWindowBorder = 3;

private:
    void createDRTDevToolsClient(DRTDevToolsAgent*);

    void resetWebSettings(WebKit::WebView&);
    void dump();
    std::string dumpAllBackForwardLists();
    void dumpImage(skia::PlatformCanvas*) const;

    bool m_testIsPending;
    bool m_testIsPreparing;
    bool m_isLoading;
    WebKit::WebView* m_webView;
    WebKit::WebWidget* m_focusedWidget;
    bool m_testShellMode;
    WebViewHost* m_webViewHost;
    WebViewHost* m_devTools;
    OwnPtr<DRTDevToolsAgent> m_drtDevToolsAgent;
    OwnPtr<DRTDevToolsClient> m_drtDevToolsClient;
    OwnPtr<AccessibilityController> m_accessibilityController;
    OwnPtr<EventSender> m_eventSender;
    OwnPtr<LayoutTestController> m_layoutTestController;
    OwnPtr<PlainTextController> m_plainTextController;
    OwnPtr<TextInputController> m_textInputController;
    OwnPtr<NotificationPresenter> m_notificationPresenter;
    OwnPtr<TestEventPrinter> m_printer;
    TestParams m_params;
    int m_timeout; // timeout value in millisecond
    bool m_allowExternalPages;
    bool m_acceleratedCompositingEnabled;
    bool m_accelerated2dCanvasEnabled;
    WebPreferences m_prefs;

    // List of all windows in this process.
    // The main window should be put into windowList[0].
    typedef Vector<WebViewHost*> WindowList;
    WindowList m_windowList;

#if defined(OS_WIN)
    // Used by the watchdog to know when it's finished.
    HANDLE m_finishedEvent;
#endif
};

void platformInit(int*, char***);
void openStartupDialog();
bool checkLayoutTestSystemDependencies();

#endif // TestShell_h
