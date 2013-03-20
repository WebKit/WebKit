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

#include "config.h"
#include "TestShell.h"

#include "DRTDevToolsAgent.h"
#include "DRTDevToolsClient.h"
#include "MockPlatform.h"
#include "MockWebPrerenderingSupport.h"
#include "WebArrayBufferView.h"
#include "WebDataSource.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebHistoryItem.h"
#include "WebIDBFactory.h"
#include "WebTestingSupport.h"
#include "WebSettings.h"
#include "WebTestProxy.h"
#include "WebTestRunner.h"
#include "WebView.h"
#include "WebViewHost.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/support/webkit_support.h"
#include "webkit/support/webkit_support_gfx.h"
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebPoint.h>
#include <public/WebSize.h>
#include <public/WebString.h>
#include <public/WebThread.h>
#include <public/WebURLRequest.h>
#include <public/WebURLResponse.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <wtf/MD5.h>
#include <wtf/OwnArrayPtr.h>


using namespace WebKit;
using namespace WebTestRunner;
using namespace std;

// Content area size for newly created windows.
static const int testWindowWidth = 800;
static const int testWindowHeight = 600;

// The W3C SVG layout tests use a different size than the other layout tests.
static const int SVGTestWindowWidth = 480;
static const int SVGTestWindowHeight = 360;

static const char layoutTestsPattern[] = "/LayoutTests/";
static const string::size_type layoutTestsPatternSize = sizeof(layoutTestsPattern) - 1;
static const char fileUrlPattern[] = "file:/";
static const char fileTestPrefix[] = "(file test):";
static const char dataUrlPattern[] = "data:";
static const string::size_type dataUrlPatternSize = sizeof(dataUrlPattern) - 1;

// FIXME: Move this to a common place so that it can be shared with
// WebCore::TransparencyWin::makeLayerOpaque().
static void makeCanvasOpaque(SkCanvas* canvas)
{
    const SkBitmap& bitmap = canvas->getTopDevice()->accessBitmap(true);
    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);

    SkAutoLockPixels lock(bitmap);
    for (int y = 0; y < bitmap.height(); y++) {
        uint32_t* row = bitmap.getAddr32(0, y);
        for (int x = 0; x < bitmap.width(); x++)
            row[x] |= 0xFF000000; // Set alpha bits to 1.
    }
}

TestShell::TestShell()
    : m_testIsPending(false)
    , m_testIsPreparing(false)
    , m_focusedWidget(0)
    , m_devTools(0)
    , m_dumpPixelsForCurrentTest(false)
    , m_allowExternalPages(false)
    , m_acceleratedCompositingForVideoEnabled(false)
    , m_acceleratedCompositingForFixedPositionEnabled(false)
    , m_acceleratedCompositingForOverflowScrollEnabled(false)
    , m_softwareCompositingEnabled(false)
    , m_threadedCompositingEnabled(false)
    , m_forceCompositingMode(false)
    , m_threadedHTMLParser(true)
    , m_accelerated2dCanvasEnabled(false)
    , m_deferred2dCanvasEnabled(false)
    , m_perTilePaintingEnabled(false)
    , m_deferredImageDecodingEnabled(false)
    , m_stressOpt(false)
    , m_stressDeopt(false)
    , m_dumpWhenFinished(true)
    , m_isDisplayingModalDialog(false)
{
    // 30 second is the same as the value in Mac DRT.
    // If we use a value smaller than the timeout value of
    // (new-)run-webkit-tests, (new-)run-webkit-tests misunderstands that a
    // timed-out DRT process was crashed.
    m_timeout = 30 * 1000;
}

void TestShell::initialize(MockPlatform* platformSupport)
{
    m_testInterfaces = adoptPtr(new WebTestInterfaces());
    platformSupport->setInterfaces(m_testInterfaces.get());
    m_devToolsTestInterfaces = adoptPtr(new WebTestInterfaces());
#if ENABLE(LINK_PRERENDER)
    m_prerenderingSupport = adoptPtr(new MockWebPrerenderingSupport());
#endif

    WTF::initializeThreading();

    if (m_threadedCompositingEnabled)
        m_webCompositorThread = adoptPtr(WebKit::Platform::current()->createThread("Compositor"));
    webkit_support::SetThreadedCompositorEnabled(m_threadedCompositingEnabled);

    createMainWindow();
}

void TestShell::createMainWindow()
{
    m_drtDevToolsAgent = adoptPtr(new DRTDevToolsAgent);
    m_webViewHost = adoptPtr(createNewWindow(WebURL(), m_drtDevToolsAgent.get(), m_testInterfaces.get()));
    m_webView = m_webViewHost->webView();
    m_testInterfaces->setDelegate(m_webViewHost.get());
    m_testInterfaces->setWebView(m_webView, m_webViewHost->proxy());
    m_drtDevToolsAgent->setWebView(m_webView);
}

TestShell::~TestShell()
{
    if (m_webViewHost)
        m_webViewHost->shutdown();
    m_testInterfaces->setDelegate(0);
    m_testInterfaces->setWebView(0, 0);
    m_devToolsTestInterfaces->setDelegate(0);
    m_devToolsTestInterfaces->setWebView(0, 0);
    m_drtDevToolsAgent->setWebView(0);
}

void TestShell::createDRTDevToolsClient(DRTDevToolsAgent* agent)
{
    m_drtDevToolsClient = adoptPtr(new DRTDevToolsClient(agent, m_devTools->webView()));
}

void TestShell::showDevTools()
{
    if (!m_devTools) {
        WebURL url = webkit_support::GetDevToolsPathAsURL();
        if (!url.isValid()) {
            ASSERT(false);
            return;
        }
        m_devTools = createNewWindow(url, 0, m_devToolsTestInterfaces.get());
        m_devTools->webView()->settings()->setMemoryInfoEnabled(true);
        m_devTools->proxy()->setLogConsoleOutput(false);
        m_devToolsTestInterfaces->setDelegate(m_devTools);
        m_devToolsTestInterfaces->setWebView(m_devTools->webView(), m_devTools->proxy());
        ASSERT(m_devTools);
        createDRTDevToolsClient(m_drtDevToolsAgent.get());
    }
    m_devTools->show(WebKit::WebNavigationPolicyNewWindow);
}

void TestShell::closeDevTools()
{
    if (m_devTools) {
        m_devTools->webView()->settings()->setMemoryInfoEnabled(false);
        m_drtDevToolsAgent->reset();
        m_drtDevToolsClient.clear();
        m_devToolsTestInterfaces->setDelegate(0);
        m_devToolsTestInterfaces->setWebView(0, 0);
        closeWindow(m_devTools);
        m_devTools = 0;
    }
}

void TestShell::resetWebSettings(WebView& webView)
{
    m_prefs.reset();
    m_prefs.acceleratedCompositingEnabled = true;
    m_prefs.acceleratedCompositingForVideoEnabled = m_acceleratedCompositingForVideoEnabled;
    m_prefs.acceleratedCompositingForFixedPositionEnabled = m_acceleratedCompositingForFixedPositionEnabled;
    m_prefs.acceleratedCompositingForOverflowScrollEnabled = m_acceleratedCompositingForOverflowScrollEnabled;
    m_prefs.forceCompositingMode = m_forceCompositingMode;
    m_prefs.accelerated2dCanvasEnabled = m_accelerated2dCanvasEnabled;
    m_prefs.deferred2dCanvasEnabled = m_deferred2dCanvasEnabled;
    m_prefs.perTilePaintingEnabled = m_perTilePaintingEnabled;
    m_prefs.deferredImageDecodingEnabled = m_deferredImageDecodingEnabled;
    m_prefs.threadedHTMLParser = m_threadedHTMLParser;
    m_prefs.applyTo(&webView);
}

void TestShell::runFileTest(const TestParams& params, bool shouldDumpPixels)
{
    ASSERT(params.testUrl.isValid());
    m_dumpPixelsForCurrentTest = shouldDumpPixels;
    m_testIsPreparing = true;
    m_testInterfaces->setTestIsRunning(true);
    m_params = params;
    string testUrl = m_params.testUrl.spec();
    m_testInterfaces->configureForTestWithURL(m_params.testUrl, shouldDumpPixels);

    if (testUrl.find("compositing/") != string::npos || testUrl.find("compositing\\") != string::npos) {
        if (!m_softwareCompositingEnabled)
            m_prefs.accelerated2dCanvasEnabled = true;
        m_prefs.acceleratedCompositingForVideoEnabled = true;
        m_prefs.deferred2dCanvasEnabled = true;
        m_prefs.mockScrollbarsEnabled = true;
        m_prefs.applyTo(m_webView);
    }

    if (m_dumpWhenFinished)
        m_printer.handleTestHeader(testUrl.c_str());
    loadURL(m_params.testUrl);

    if (m_devTools)
        this->setFocus(m_devTools->webView(), true);

    m_testIsPreparing = false;
    waitTestFinished();
}

static inline bool isSVGTestURL(const WebURL& url)
{
    return url.isValid() && string(url.spec()).find("W3C-SVG-1.1") != string::npos;
}

void TestShell::resizeWindowForTest(WebViewHost* window, const WebURL& url)
{
    int width, height;
    if (isSVGTestURL(url)) {
        width = SVGTestWindowWidth;
        height = SVGTestWindowHeight;
    } else {
        width = testWindowWidth;
        height = testWindowHeight;
    }
    window->setWindowRect(WebRect(0, 0, width + virtualWindowBorder * 2, height + virtualWindowBorder * 2));
}

void TestShell::resetTestController()
{
    resetWebSettings(*webView());
    m_testInterfaces->resetAll();
    m_devToolsTestInterfaces->resetAll();
    m_webViewHost->reset();
#if OS(ANDROID)
    webkit_support::ReleaseMediaResources();
#endif
    m_drtDevToolsAgent->reset();
    if (m_drtDevToolsClient)
        m_drtDevToolsClient->reset();
    webView()->setPageScaleFactor(1, WebPoint(0, 0));
    webView()->enableFixedLayoutMode(false);
    webView()->setFixedLayoutSize(WebSize(0, 0));
    webView()->mainFrame()->clearOpener();
    WebTestingSupport::resetInternalsObject(webView()->mainFrame());
}

void TestShell::loadURL(const WebURL& url)
{
    m_webViewHost->loadURLForFrame(url, string());
}

void TestShell::reload()
{
    m_webViewHost->navigationController()->reload();
}

void TestShell::goToOffset(int offset)
{
     m_webViewHost->navigationController()->goToOffset(offset);
}

int TestShell::navigationEntryCount() const
{
    return m_webViewHost->navigationController()->entryCount();
}

void TestShell::callJSGC()
{
    m_webView->mainFrame()->collectGarbage();
}

void TestShell::setFocus(WebWidget* widget, bool enable)
{
    // Simulate the effects of InteractiveSetFocus(), which includes calling
    // both setFocus() and setIsActive().
    if (enable) {
        if (m_focusedWidget != widget) {
            if (m_focusedWidget)
                m_focusedWidget->setFocus(false);
            webView()->setIsActive(enable);
            widget->setFocus(enable);
            m_focusedWidget = widget;
        }
    } else {
        if (m_focusedWidget == widget) {
            widget->setFocus(enable);
            webView()->setIsActive(enable);
            m_focusedWidget = 0;
        }
    }
}

void TestShell::testFinished(WebViewHost* host)
{
    if (host == m_devTools)
        return;

    if (!m_testIsPending)
        return;
    m_testIsPending = false;
    m_testInterfaces->setTestIsRunning(false);
    if (m_dumpWhenFinished)
        dump();
    webkit_support::QuitMessageLoop();
}

void TestShell::testTimedOut()
{
    m_printer.handleTimedOut();
    testFinished(webViewHost());
}

void TestShell::dump()
{
    // Dump the requested representation.
    WebFrame* frame = m_webView->mainFrame();
    if (!frame)
        return;
    bool shouldDumpAsAudio = m_testInterfaces->testRunner()->shouldDumpAsAudio();
    bool shouldGeneratePixelResults = m_testInterfaces->testRunner()->shouldGeneratePixelResults();
    bool dumpedAnything = false;

    if (shouldDumpAsAudio) {
        const WebKit::WebArrayBufferView* webArrayBufferView = m_testInterfaces->testRunner()->audioData();
        m_printer.handleAudio(webArrayBufferView->baseAddress(), webArrayBufferView->byteLength());
        m_printer.handleAudioFooter();
        m_printer.handleTestFooter(true);

        fflush(stdout);
        fflush(stderr);
        return;
    }

    if (m_params.dumpTree) {
        dumpedAnything = true;
        m_printer.handleTextHeader();
        string dataUtf8 = m_webViewHost->proxy()->captureTree(m_params.debugRenderTree);
        if (fwrite(dataUtf8.c_str(), 1, dataUtf8.size(), stdout) != dataUtf8.size())
            FATAL("Short write to stdout, disk full?\n");
    }
    if (dumpedAnything && m_params.printSeparators)
        m_printer.handleTextFooter();

    if (m_dumpPixelsForCurrentTest && shouldGeneratePixelResults) {
        // Image output: we write the image data to the file given on the
        // command line (for the dump pixels argument), and the MD5 sum to
        // stdout.
        dumpedAnything = true;
        dumpImage(m_webViewHost->proxy()->capturePixels());
    }
    m_printer.handleTestFooter(dumpedAnything);
    fflush(stdout);
    fflush(stderr);
}

void TestShell::dumpImage(SkCanvas* canvas) const
{
    // Fix the alpha. The expected PNGs on Mac have an alpha channel, so we want
    // to keep it. On Windows, the alpha channel is wrong since text/form control
    // drawing may have erased it in a few places. So on Windows we force it to
    // opaque and also don't write the alpha channel for the reference. Linux
    // doesn't have the wrong alpha like Windows, but we match Windows.
#if OS(MAC_OS_X)
    bool discardTransparency = false;
#else
    bool discardTransparency = true;
    makeCanvasOpaque(canvas);
#endif

    const SkBitmap& sourceBitmap = canvas->getTopDevice()->accessBitmap(false);
    SkAutoLockPixels sourceBitmapLock(sourceBitmap);

    // Compute MD5 sum.
    MD5 digester;
    Vector<uint8_t, 16> digestValue;
#if OS(ANDROID)
    // On Android, pixel layout is RGBA (see third_party/skia/include/core/SkColorPriv.h);
    // however, other Chrome platforms use BGRA (see skia/config/SkUserConfig.h).
    // To match the checksum of other Chrome platforms, we need to reorder the layout of pixels.
    // NOTE: The following code assumes we use SkBitmap::kARGB_8888_Config,
    // which has been checked in device.makeOpaque() (see above).
    const uint8_t* rawPixels = reinterpret_cast<const uint8_t*>(sourceBitmap.getPixels());
    size_t bitmapSize = sourceBitmap.getSize();
    OwnArrayPtr<uint8_t> reorderedPixels = adoptArrayPtr(new uint8_t[bitmapSize]);
    for (size_t i = 0; i < bitmapSize; i += 4) {
        reorderedPixels[i] = rawPixels[i + 2]; // R
        reorderedPixels[i + 1] = rawPixels[i + 1]; // G
        reorderedPixels[i + 2] = rawPixels[i]; // B
        reorderedPixels[i + 3] = rawPixels[i + 3]; // A
    }
    digester.addBytes(reorderedPixels.get(), bitmapSize);
    reorderedPixels.clear();
#else
    digester.addBytes(reinterpret_cast<const uint8_t*>(sourceBitmap.getPixels()), sourceBitmap.getSize());
#endif
    digester.checksum(digestValue);
    string md5hash;
    md5hash.reserve(16 * 2);
    for (unsigned i = 0; i < 16; ++i) {
        char hex[3];
        // Use "x", not "X". The string must be lowercased.
        sprintf(hex, "%02x", digestValue[i]);
        md5hash.append(hex);
    }

    // Only encode and dump the png if the hashes don't match. Encoding the
    // image is really expensive.
    if (md5hash.compare(m_params.pixelHash)) {
        std::vector<unsigned char> png;
#if OS(ANDROID)
        webkit_support::EncodeRGBAPNGWithChecksum(reinterpret_cast<const unsigned char*>(sourceBitmap.getPixels()), sourceBitmap.width(),
            sourceBitmap.height(), static_cast<int>(sourceBitmap.rowBytes()), discardTransparency, md5hash, &png);
#else
        webkit_support::EncodeBGRAPNGWithChecksum(reinterpret_cast<const unsigned char*>(sourceBitmap.getPixels()), sourceBitmap.width(),
            sourceBitmap.height(), static_cast<int>(sourceBitmap.rowBytes()), discardTransparency, md5hash, &png);
#endif

        m_printer.handleImage(md5hash.c_str(), m_params.pixelHash.c_str(), &png[0], png.size());
    } else
        m_printer.handleImage(md5hash.c_str(), m_params.pixelHash.c_str(), 0, 0);
}

void TestShell::bindJSObjectsToWindow(WebFrame* frame)
{
    WebTestingSupport::injectInternalsObject(frame);
    if (m_devTools && m_devTools->webView() == frame->view())
        m_devToolsTestInterfaces->bindTo(frame);
    else
        m_testInterfaces->bindTo(frame);
}

WebViewHost* TestShell::createNewWindow(const WebKit::WebURL& url)
{
    return createNewWindow(url, 0, m_testInterfaces.get());
}

WebViewHost* TestShell::createNewWindow(const WebKit::WebURL& url, DRTDevToolsAgent* devToolsAgent, WebTestInterfaces *testInterfaces)
{
    WebTestProxy<WebViewHost, TestShell*>* host = new WebTestProxy<WebViewHost, TestShell*>(this);
    host->setInterfaces(testInterfaces);
    if (m_webViewHost)
        host->setDelegate(m_webViewHost.get());
    else
        host->setDelegate(host);
    host->setProxy(host);
    WebView* view = WebView::create(host);
    view->setPermissionClient(testInterfaces->testRunner()->webPermissions());
    view->setDevToolsAgentClient(devToolsAgent);
    host->setWebWidget(view);
    m_prefs.applyTo(view);
    view->initializeMainFrame(host);
    m_windowList.append(host);
    host->loadURLForFrame(url, string());
    return host;
}

void TestShell::closeWindow(WebViewHost* window)
{
    size_t i = m_windowList.find(window);
    if (i == notFound) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_windowList.remove(i);
    WebWidget* focusedWidget = m_focusedWidget;
    if (window->webWidget() == m_focusedWidget)
        focusedWidget = 0;

    window->shutdown();
    delete window;
    // We set the focused widget after deleting the web view host because it
    // can change the focus.
    m_focusedWidget = focusedWidget;
    if (m_focusedWidget) {
        webView()->setIsActive(true);
        m_focusedWidget->setFocus(true);
    }
}

void TestShell::closeRemainingWindows()
{
    // Just close devTools window manually because we have custom deinitialization code for it.
    closeDevTools();

    // Iterate through the window list and close everything except the main
    // window. We don't want to delete elements as we're iterating, so we copy
    // to a temp vector first.
    Vector<WebViewHost*> windowsToDelete;
    for (unsigned i = 0; i < m_windowList.size(); ++i) {
        if (m_windowList[i] != webViewHost())
            windowsToDelete.append(m_windowList[i]);
    }
    ASSERT(windowsToDelete.size() + 1 == m_windowList.size());
    for (unsigned i = 0; i < windowsToDelete.size(); ++i)
        closeWindow(windowsToDelete[i]);
    ASSERT(m_windowList.size() == 1);
}

int TestShell::windowCount()
{
    return m_windowList.size();
}

void TestShell::captureHistoryForWindow(size_t windowIndex, WebVector<WebHistoryItem>* history, size_t* currentEntryIndex)
{
    ASSERT(history);
    ASSERT(currentEntryIndex);
    if (windowIndex >= m_windowList.size())
        return;
    TestNavigationController& navigationController = *m_windowList[windowIndex]->navigationController();
    size_t entryCount = navigationController.entryCount();
    WebVector<WebHistoryItem> result(entryCount);
    *currentEntryIndex = navigationController.lastCommittedEntryIndex();
    for (size_t index = 0; index < entryCount; ++index) {
        WebHistoryItem historyItem = navigationController.entryAtIndex(index)->contentState();
        if (historyItem.isNull()) {
            historyItem.initialize();
            historyItem.setURLString(navigationController.entryAtIndex(index)->URL().spec().utf16());
        }
        result[index] = historyItem;
    }
    history->swap(result);
}
