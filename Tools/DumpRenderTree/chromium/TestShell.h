/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "TestEventPrinter.h"
#include "WebPreferences.h"
#include "WebTestInterfaces.h"
#include "WebViewHost.h"
#include <string>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

// TestShell is a container of global variables and has bridge functions between
// various objects. Only one instance is created in one DRT process.

namespace WebKit {
class WebDevToolsAgentClient;
class WebFrame;
class WebThread;
class WebView;
class WebThemeEngine;
class WebURL;
}

class DRTDevToolsAgent;
class DRTDevToolsCallArgs;
class DRTDevToolsClient;
class MockWebPrerenderingSupport;
class MockPlatform;

struct TestParams {
    bool dumpTree;
    bool debugRenderTree;
    bool debugLayerTree;
    bool printSeparators;
    WebKit::WebURL testUrl;
    std::string pixelHash;

    TestParams()
        : dumpTree(true)
        , debugRenderTree(false)
        , debugLayerTree(false)
        , printSeparators(false) { }
};

class TestShell {
public:
    TestShell();
    ~TestShell();

    void initialize(MockPlatform*);

    // The main WebView.
    WebKit::WebView* webView() const { return m_webView; }
    // Returns the host for the main WebView.
    WebViewHost* webViewHost() const { return m_webViewHost.get(); }
    WebTestRunner::WebTestRunner* testRunner() const { return m_testInterfaces->testRunner(); }
    const TestEventPrinter* printer() const { return &m_printer; }

    WebTestRunner::WebPreferences* preferences() { return &m_prefs; }
    void applyPreferences() { m_prefs.applyTo(m_webView); }

    void bindJSObjectsToWindow(WebKit::WebFrame*);
    void runFileTest(const TestParams&, bool shouldDumpPixelTests);
    void callJSGC();
    void resetTestController();
    void waitTestFinished();

    // Operations to the main window.
    void loadURL(const WebKit::WebURL&);
    void reload();
    void goToOffset(int offset);
    int navigationEntryCount() const;

    void setFocus(WebKit::WebWidget*, bool enable);

    // Called by the DRTTestRunner to signal test completion.
    void testFinished(WebViewHost*);
    // Called by DRTTestRunner when a test hits the timeout, but does not
    // cause a hang. We can avoid killing TestShell in this case and still dump
    // the test results.
    void testTimedOut();

    bool allowExternalPages() const { return m_allowExternalPages; }
    void setAllowExternalPages(bool allowExternalPages) { m_allowExternalPages = allowExternalPages; }

    void setAcceleratedCompositingForVideoEnabled(bool enabled) { m_acceleratedCompositingForVideoEnabled = enabled; }
    void setAcceleratedCompositingForFixedPositionEnabled(bool enabled) { m_acceleratedCompositingForFixedPositionEnabled = enabled; }
    void setAcceleratedCompositingForOverflowScrollEnabled(bool enabled) { m_acceleratedCompositingForOverflowScrollEnabled = enabled; }
    bool softwareCompositingEnabled() { return m_softwareCompositingEnabled; }
    void setSoftwareCompositingEnabled(bool enabled) { m_softwareCompositingEnabled = enabled; }
    void setThreadedCompositingEnabled(bool enabled) { m_threadedCompositingEnabled = enabled; }
    void setForceCompositingMode(bool enabled) { m_forceCompositingMode = enabled; }
    void setThreadedHTMLParser(bool enabled) { m_threadedHTMLParser = enabled; }
    void setAccelerated2dCanvasEnabled(bool enabled) { m_accelerated2dCanvasEnabled = enabled; }
    void setDeferred2dCanvasEnabled(bool enabled) { m_deferred2dCanvasEnabled = enabled; }
    void setPerTilePaintingEnabled(bool enabled) { m_perTilePaintingEnabled = enabled; }
    void setDeferredImageDecodingEnabled(bool enabled) { m_deferredImageDecodingEnabled = enabled; }
#if defined(OS_WIN)
    // Access to the finished event. Used by the static WatchDog thread.
    HANDLE finishedEvent() { return m_finishedEvent; }
#endif

    // Get the timeout for running a test in milliseconds.
    int layoutTestTimeout() { return m_timeout; }
    int layoutTestTimeoutForWatchDog() { return layoutTestTimeout() + 1000; }
    void setLayoutTestTimeout(int timeout) { m_timeout = timeout; }

    // V8 JavaScript stress test options.
    int stressOpt() { return m_stressOpt; }
    void setStressOpt(bool stressOpt) { m_stressOpt = stressOpt; }
    int stressDeopt() { return m_stressDeopt; }
    void setStressDeopt(int stressDeopt) { m_stressDeopt = stressDeopt; }

    // The JavaScript flags specified as a strings.
    std::string javaScriptFlags() { return m_javaScriptFlags; }
    void setJavaScriptFlags(std::string javaScriptFlags) { m_javaScriptFlags = javaScriptFlags; }

    // Set whether to dump when the loaded page has finished processing. This is used with multiple load
    // testing where we only want to have the output from the last load.
    void setDumpWhenFinished(bool dumpWhenFinished) { m_dumpWhenFinished = dumpWhenFinished; }

    void setIsDisplayingModalDialog(bool isDisplayingModalDialog) { m_isDisplayingModalDialog = isDisplayingModalDialog; }
    bool isDisplayingModalDialog() const { return m_isDisplayingModalDialog; }

    // Set whether the binary data output should be encoded in base64 text.
    void setEncodeBinary(bool encodeBinary) { m_printer.setEncodeBinary(encodeBinary); }

    WebViewHost* createNewWindow(const WebKit::WebURL&);
    void closeWindow(WebViewHost*);
    void closeRemainingWindows();
    int windowCount();
    void captureHistoryForWindow(size_t windowIndex, WebKit::WebVector<WebKit::WebHistoryItem>*, size_t* currentEntryIndex);
    static void resizeWindowForTest(WebViewHost*, const WebKit::WebURL&);

    void showDevTools();
    void closeDevTools();

    DRTDevToolsAgent* drtDevToolsAgent() { return m_drtDevToolsAgent.get(); }
    DRTDevToolsClient* drtDevToolsClient() { return m_drtDevToolsClient.get(); }
    WebViewHost* devToolsWebView() { return m_devTools; }

    static const int virtualWindowBorder = 3;

    typedef Vector<WebViewHost*> WindowList;
    WindowList windowList() const { return m_windowList; }

    WebKit::WebThread* webCompositorThread() const { return m_webCompositorThread.get(); }

private:
    WebViewHost* createNewWindow(const WebKit::WebURL&, DRTDevToolsAgent*, WebTestRunner::WebTestInterfaces*);
    void createMainWindow();
    void createDRTDevToolsClient(DRTDevToolsAgent*);

    void resetWebSettings(WebKit::WebView&);
    void dump();
    std::string dumpAllBackForwardLists();
    void dumpImage(SkCanvas*) const;

    bool m_testIsPending;
    bool m_testIsPreparing;
    WebKit::WebView* m_webView;
    WebKit::WebWidget* m_focusedWidget;
    WebViewHost* m_devTools;

    // Be careful of the destruction order of the following objects.
    TestEventPrinter m_printer;
    OwnPtr<DRTDevToolsAgent> m_drtDevToolsAgent;
    OwnPtr<DRTDevToolsClient> m_drtDevToolsClient;
    OwnPtr<WebTestRunner::WebTestInterfaces> m_testInterfaces;
    OwnPtr<WebTestRunner::WebTestInterfaces> m_devToolsTestInterfaces;
    // It's important that this thread is destroyed after the WebViewHost.
    OwnPtr<WebKit::WebThread> m_webCompositorThread;
    OwnPtr<WebViewHost> m_webViewHost;
#if ENABLE(LINK_PRERENDER)
    OwnPtr<MockWebPrerenderingSupport> m_prerenderingSupport;
#endif

    TestParams m_params;
    bool m_dumpPixelsForCurrentTest;
    int m_timeout; // timeout value in millisecond
    bool m_allowExternalPages;
    bool m_acceleratedCompositingForVideoEnabled;
    bool m_acceleratedCompositingForFixedPositionEnabled;
    bool m_acceleratedCompositingForOverflowScrollEnabled;
    bool m_softwareCompositingEnabled;
    bool m_threadedCompositingEnabled;
    bool m_forceCompositingMode;
    bool m_threadedHTMLParser;
    bool m_accelerated2dCanvasEnabled;
    bool m_deferred2dCanvasEnabled;
    bool m_perTilePaintingEnabled;
    bool m_deferredImageDecodingEnabled;
    WebTestRunner::WebPreferences m_prefs;
    bool m_stressOpt;
    bool m_stressDeopt;
    std::string m_javaScriptFlags;
    bool m_dumpWhenFinished;
    bool m_isDisplayingModalDialog;


    // List of all windows in this process.
    // The main window should be put into windowList[0].
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
