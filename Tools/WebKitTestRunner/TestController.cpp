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

#include "config.h"
#include "TestController.h"

#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestInvocation.h"
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKNumber.h>
#include <WebKit2/WKPageGroup.h>
#include <WebKit2/WKPagePrivate.h>
#include <WebKit2/WKPreferencesPrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <cstdio>
#include <wtf/PassOwnPtr.h>

#if PLATFORM(MAC) || PLATFORM(QT) || PLATFORM(GTK)
#include "EventSenderProxy.h"
#endif

namespace WTR {

static const double defaultLongTimeout = 30;
static const double defaultShortTimeout = 5;

static WKURLRef blankURL()
{
    static WKURLRef staticBlankURL = WKURLCreateWithUTF8CString("about:blank");
    return staticBlankURL;
}

static TestController* controller;

TestController& TestController::shared()
{
    ASSERT(controller);
    return *controller;
}

TestController::TestController(int argc, const char* argv[])
    : m_dumpPixels(false)
    , m_verbose(false)
    , m_printSeparators(false)
    , m_usingServerMode(false)
    , m_gcBetweenTests(false)
    , m_state(Initial)
    , m_doneResetting(false)
    , m_longTimeout(defaultLongTimeout)
    , m_shortTimeout(defaultShortTimeout)
    , m_didPrintWebProcessCrashedMessage(false)
    , m_shouldExitWhenWebProcessCrashes(true)
    , m_beforeUnloadReturnValue(true)
#if PLATFORM(MAC) || PLATFORM(QT) || PLATFORM(GTK)
    , m_eventSenderProxy(new EventSenderProxy(this))
#endif
{
    initialize(argc, argv);
    controller = this;
    run();
    controller = 0;
}

TestController::~TestController()
{
}

static WKRect getWindowFrameMainPage(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<TestController*>(const_cast<void*>(clientInfo))->mainWebView();
    return view->windowFrame();
}

static void setWindowFrameMainPage(WKPageRef page, WKRect frame, const void* clientInfo)
{
    PlatformWebView* view = static_cast<TestController*>(const_cast<void*>(clientInfo))->mainWebView();
    view->setWindowFrame(frame);
}

static WKRect getWindowFrameOtherPage(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    return view->windowFrame();
}

static void setWindowFrameOtherPage(WKPageRef page, WKRect frame, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    view->setWindowFrame(frame);
}

static bool runBeforeUnloadConfirmPanel(WKPageRef page, WKStringRef message, WKFrameRef frame, const void *clientInfo)
{
    TestController* testController = static_cast<TestController*>(const_cast<void*>(clientInfo));
    printf("CONFIRM NAVIGATION: %s\n", toSTD(message).c_str());
    return testController->beforeUnloadReturnValue();
}

static unsigned long long exceededDatabaseQuota(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKStringRef, WKStringRef, unsigned long long, unsigned long long, unsigned long long, unsigned long long, const void*)
{
    static const unsigned long long defaultQuota = 5 * 1024 * 1024;    
    return defaultQuota;
}


void TestController::runModal(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    view->setWindowIsKey(false);
    runModal(view);
    view->setWindowIsKey(true);
}

static void closeOtherPage(WKPageRef page, const void* clientInfo)
{
    WKPageClose(page);
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    delete view;
}

static void focus(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    view->setWindowIsKey(true);
}

static void unfocus(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    view->setWindowIsKey(false);
}

WKPageRef TestController::createOtherPage(WKPageRef oldPage, WKURLRequestRef, WKDictionaryRef, WKEventModifiers, WKEventMouseButton, const void*)
{
    PlatformWebView* view = new PlatformWebView(WKPageGetContext(oldPage), WKPageGetPageGroup(oldPage));
    WKPageRef newPage = view->page();

    view->resizeTo(800, 600);

    WKPageUIClient otherPageUIClient = {
        kWKPageUIClientCurrentVersion,
        view,
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        closeOtherPage,
        0, // takeFocus
        focus,
        unfocus,
        0, // runJavaScriptAlert        
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // setStatusText
        0, // mouseDidMoveOverElement_deprecatedForUseWithV0
        0, // missingPluginButtonClicked
        0, // didNotHandleKeyEvent
        0, // didNotHandleWheelEvent
        0, // toolbarsAreVisible
        0, // setToolbarsAreVisible
        0, // menuBarIsVisible
        0, // setMenuBarIsVisible
        0, // statusBarIsVisible
        0, // setStatusBarIsVisible
        0, // isResizable
        0, // setIsResizable
        getWindowFrameOtherPage,
        setWindowFrameOtherPage,
        runBeforeUnloadConfirmPanel,
        0, // didDraw
        0, // pageDidScroll
        exceededDatabaseQuota,
        0, // runOpenPanel
        0, // decidePolicyForGeolocationPermissionRequest
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        runModal,
        0, // didCompleteRubberBandForMainFrame
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        createOtherPage,
        0, // mouseDidMoveOverElement
        0, // decidePolicyForNotificationPermissionRequest
    };
    WKPageSetPageUIClient(newPage, &otherPageUIClient);

    WKRetain(newPage);
    return newPage;
}

const char* TestController::libraryPathForTesting()
{
    // FIXME: This may not be sufficient to prevent interactions/crashes
    // when running more than one copy of DumpRenderTree.
    // See https://bugs.webkit.org/show_bug.cgi?id=10906
    char* dumpRenderTreeTemp = getenv("DUMPRENDERTREE_TEMP");
    if (dumpRenderTreeTemp)
        return dumpRenderTreeTemp;
    return platformLibraryPathForTesting();
}


void TestController::initialize(int argc, const char* argv[])
{
    platformInitialize();

    if (argc < 2) {
        fputs("Usage: WebKitTestRunner [options] filename [filename2..n]\n", stderr);
        // FIXME: Refactor option parsing to allow us to print
        // an auto-generated list of options.
        exit(1);
    }

    bool printSupportedFeatures = false;

    for (int i = 1; i < argc; ++i) {
        std::string argument(argv[i]);

        if (argument == "--timeout" && i + 1 < argc) {
            m_longTimeout = atoi(argv[++i]);
            // Scale up the short timeout to match.
            m_shortTimeout = defaultShortTimeout * m_longTimeout / defaultLongTimeout;
            continue;
        }
        if (argument == "--pixel-tests") {
            m_dumpPixels = true;
            continue;
        }
        if (argument == "--verbose") {
            m_verbose = true;
            continue;
        }
        if (argument == "--gc-between-tests") {
            m_gcBetweenTests = true;
            continue;
        }
        if (argument == "--print-supported-features") {
            printSupportedFeatures = true;
            break;
        }

        // Skip any other arguments that begin with '--'.
        if (argument.length() >= 2 && argument[0] == '-' && argument[1] == '-')
            continue;

        m_paths.push_back(argument);
    }

    if (printSupportedFeatures) {
        // FIXME: On Windows, DumpRenderTree uses this to expose whether it supports 3d
        // transforms and accelerated compositing. When we support those features, we
        // should match DRT's behavior.
        exit(0);
    }

    m_usingServerMode = (m_paths.size() == 1 && m_paths[0] == "-");
    if (m_usingServerMode)
        m_printSeparators = true;
    else
        m_printSeparators = m_paths.size() > 1;

    initializeInjectedBundlePath();
    initializeTestPluginDirectory();

    WKRetainPtr<WKStringRef> pageGroupIdentifier(AdoptWK, WKStringCreateWithUTF8CString("WebKitTestRunnerPageGroup"));
    m_pageGroup.adopt(WKPageGroupCreateWithIdentifier(pageGroupIdentifier.get()));

    m_context.adopt(WKContextCreateWithInjectedBundlePath(injectedBundlePath()));

    const char* path = libraryPathForTesting();
    if (path) {
        Vector<char> databaseDirectory(strlen(path) + strlen("/Databases") + 1);
        sprintf(databaseDirectory.data(), "%s%s", path, "/Databases");
        WKRetainPtr<WKStringRef> databaseDirectoryWK(AdoptWK, WKStringCreateWithUTF8CString(databaseDirectory.data()));
        WKContextSetDatabaseDirectory(m_context.get(), databaseDirectoryWK.get());
    }

    platformInitializeContext();

    WKContextInjectedBundleClient injectedBundleClient = {
        kWKContextInjectedBundleClientCurrentVersion,
        this,
        didReceiveMessageFromInjectedBundle,
        didReceiveSynchronousMessageFromInjectedBundle
    };
    WKContextSetInjectedBundleClient(m_context.get(), &injectedBundleClient);

    WKContextSetAdditionalPluginsDirectory(m_context.get(), testPluginDirectory());

    m_mainWebView = adoptPtr(new PlatformWebView(m_context.get(), m_pageGroup.get()));

    WKPageUIClient pageUIClient = {
        kWKPageUIClientCurrentVersion,
        this,
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        0, // close
        0, // takeFocus
        0, // focus
        0, // unfocus
        0, // runJavaScriptAlert        
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // setStatusText
        0, // mouseDidMoveOverElement_deprecatedForUseWithV0
        0, // missingPluginButtonClicked
        0, // didNotHandleKeyEvent
        0, // didNotHandleWheelEvent
        0, // toolbarsAreVisible
        0, // setToolbarsAreVisible
        0, // menuBarIsVisible
        0, // setMenuBarIsVisible
        0, // statusBarIsVisible
        0, // setStatusBarIsVisible
        0, // isResizable
        0, // setIsResizable
        getWindowFrameMainPage,
        setWindowFrameMainPage,
        runBeforeUnloadConfirmPanel,
        0, // didDraw
        0, // pageDidScroll
        exceededDatabaseQuota,
        0, // runOpenPanel
        0, // decidePolicyForGeolocationPermissionRequest
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        runModal,
        0, // didCompleteRubberBandForMainFrame
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        createOtherPage,
        0, // mouseDidMoveOverElement
        0, // decidePolicyForNotificationPermissionRequest
    };
    WKPageSetPageUIClient(m_mainWebView->page(), &pageUIClient);

    WKPageLoaderClient pageLoaderClient = {
        kWKPageLoaderClientCurrentVersion,
        this,
        0, // didStartProvisionalLoadForFrame
        0, // didReceiveServerRedirectForProvisionalLoadForFrame
        0, // didFailProvisionalLoadWithErrorForFrame
        didCommitLoadForFrame,
        0, // didFinishDocumentLoadForFrame
        didFinishLoadForFrame,
        0, // didFailLoadWithErrorForFrame
        0, // didSameDocumentNavigationForFrame
        0, // didReceiveTitleForFrame
        0, // didFirstLayoutForFrame
        0, // didFirstVisuallyNonEmptyLayoutForFrame
        0, // didRemoveFrameFromHierarchy
        0, // didFailToInitializePlugin
        0, // didDisplayInsecureContentForFrame
        0, // canAuthenticateAgainstProtectionSpaceInFrame
        0, // didReceiveAuthenticationChallengeInFrame
        0, // didStartProgress
        0, // didChangeProgress
        0, // didFinishProgress
        0, // didBecomeUnresponsive
        0, // didBecomeResponsive
        processDidCrash,
        0, // didChangeBackForwardList
        0, // shouldGoToBackForwardListItem
        0, // didRunInsecureContentForFrame
        0  // didDetectXSSForFrame
    };
    WKPageSetPageLoaderClient(m_mainWebView->page(), &pageLoaderClient);
}

bool TestController::resetStateToConsistentValues()
{
    m_state = Resetting;
    
    m_beforeUnloadReturnValue = true;

    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("Reset"));
    WKRetainPtr<WKMutableDictionaryRef> resetMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> shouldGCKey = adoptWK(WKStringCreateWithUTF8CString("ShouldGC"));
    WKRetainPtr<WKBooleanRef> shouldGCValue = adoptWK(WKBooleanCreate(m_gcBetweenTests));
    WKDictionaryAddItem(resetMessageBody.get(), shouldGCKey.get(), shouldGCValue.get());

    WKContextPostMessageToInjectedBundle(TestController::shared().context(), messageName.get(), resetMessageBody.get());

    WKContextSetShouldUseFontSmoothing(TestController::shared().context(), false);

    // FIXME: This function should also ensure that there is only one page open.

    // Reset preferences
    WKPreferencesRef preferences = WKPageGroupGetPreferences(m_pageGroup.get());
    WKPreferencesSetOfflineWebApplicationCacheEnabled(preferences, true);
    WKPreferencesSetFontSmoothingLevel(preferences, kWKFontSmoothingLevelNoSubpixelAntiAliasing);
    WKPreferencesSetXSSAuditorEnabled(preferences, false);
    WKPreferencesSetDeveloperExtrasEnabled(preferences, true);
    WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(preferences, true);
    WKPreferencesSetJavaScriptCanAccessClipboard(preferences, true);
    WKPreferencesSetDOMPasteAllowed(preferences, true);
    WKPreferencesSetUniversalAccessFromFileURLsAllowed(preferences, true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences, true);
#if ENABLE(FULLSCREEN_API)
    WKPreferencesSetFullScreenEnabled(preferences, true);
#endif

#if !PLATFORM(QT)
    static WKStringRef standardFontFamily = WKStringCreateWithUTF8CString("Times");
    static WKStringRef cursiveFontFamily = WKStringCreateWithUTF8CString("Apple Chancery");
    static WKStringRef fantasyFontFamily = WKStringCreateWithUTF8CString("Papyrus");
    static WKStringRef fixedFontFamily = WKStringCreateWithUTF8CString("Courier");
    static WKStringRef pictographFontFamily = WKStringCreateWithUTF8CString("Apple Color Emoji");
    static WKStringRef sansSerifFontFamily = WKStringCreateWithUTF8CString("Helvetica");
    static WKStringRef serifFontFamily = WKStringCreateWithUTF8CString("Times");

    WKPreferencesSetStandardFontFamily(preferences, standardFontFamily);
    WKPreferencesSetCursiveFontFamily(preferences, cursiveFontFamily);
    WKPreferencesSetFantasyFontFamily(preferences, fantasyFontFamily);
    WKPreferencesSetFixedFontFamily(preferences, fixedFontFamily);
    WKPreferencesSetPictographFontFamily(preferences, pictographFontFamily);
    WKPreferencesSetSansSerifFontFamily(preferences, sansSerifFontFamily);
    WKPreferencesSetSerifFontFamily(preferences, serifFontFamily);
#endif

    // in the case that a test using the chrome input field failed, be sure to clean up for the next test
    m_mainWebView->removeChromeInputField();
    m_mainWebView->focus();

    // Re-set to the default backing scale factor by setting the custom scale factor to 0.
    WKPageSetCustomBackingScaleFactor(m_mainWebView->page(), 0);

    // Reset main page back to about:blank
    m_doneResetting = false;

    WKPageLoadURL(m_mainWebView->page(), blankURL());
    runUntil(m_doneResetting, ShortTimeout);
    return m_doneResetting;
}

bool TestController::runTest(const char* test)
{
    if (!resetStateToConsistentValues()) {
        fputs("#CRASHED - WebProcess\n", stderr);
        fflush(stderr);
        return false;
    }

    std::string pathOrURL(test);
    std::string expectedPixelHash;
    size_t separatorPos = pathOrURL.find("'");
    if (separatorPos != std::string::npos) {
        pathOrURL = std::string(std::string(test), 0, separatorPos);
        expectedPixelHash = std::string(std::string(test), separatorPos + 1);
    }

    m_state = RunningTest;

    m_currentInvocation = adoptPtr(new TestInvocation(pathOrURL));
    if (m_dumpPixels)
        m_currentInvocation->setIsPixelTest(expectedPixelHash);    

    m_currentInvocation->invoke();
    m_currentInvocation.clear();

    return true;
}

void TestController::runTestingServerLoop()
{
    char filenameBuffer[2048];
    while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
        char* newLineCharacter = strchr(filenameBuffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (strlen(filenameBuffer) == 0)
            continue;

        if (!runTest(filenameBuffer))
            break;
    }
}

void TestController::run()
{
    if (m_usingServerMode)
        runTestingServerLoop();
    else {
        for (size_t i = 0; i < m_paths.size(); ++i) {
            if (!runTest(m_paths[i].c_str()))
                break;
        }
    }
}

void TestController::runUntil(bool& done, TimeoutDuration timeoutDuration)
{
    platformRunUntil(done, timeoutDuration == ShortTimeout ? m_shortTimeout : m_longTimeout);
}

// WKContextInjectedBundleClient

void TestController::didReceiveMessageFromInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

void TestController::didReceiveSynchronousMessageFromInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, WKTypeRef* returnData, const void* clientInfo)
{
    *returnData = static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody).leakRef();
}

void TestController::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (!m_currentInvocation)
        return;
    m_currentInvocation->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

WKRetainPtr<WKTypeRef> TestController::didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
#if PLATFORM(MAC) || PLATFORM(QT) || PLATFORM(GTK)
    if (WKStringIsEqualToUTF8CString(messageName, "EventSender")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
        WKStringRef subMessageName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subMessageKey.get()));

        if (WKStringIsEqualToUTF8CString(subMessageName, "KeyDown")) {
            WKRetainPtr<WKStringRef> keyKey = adoptWK(WKStringCreateWithUTF8CString("Key"));
            WKStringRef key = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, keyKey.get()));

            WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
            WKEventModifiers modifiers = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifiersKey.get()))));

            WKRetainPtr<WKStringRef> locationKey = adoptWK(WKStringCreateWithUTF8CString("Location"));
            unsigned location = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, locationKey.get()))));

            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->keyDown(key, modifiers, location);
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

#if PLATFORM(MAC) || PLATFORM(QT) || PLATFORM(GTK)
        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown") || WKStringIsEqualToUTF8CString(subMessageName, "MouseUp")) {
            WKRetainPtr<WKStringRef> buttonKey = adoptWK(WKStringCreateWithUTF8CString("Button"));
            unsigned button = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, buttonKey.get()))));

            WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
            WKEventModifiers modifiers = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifiersKey.get()))));

            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown"))
                m_eventSenderProxy->mouseDown(button, modifiers);
            else
                m_eventSenderProxy->mouseUp(button, modifiers);
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseMoveTo")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));

            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->mouseMoveTo(x, y);
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseScrollBy")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));

            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->mouseScrollBy(x, y);
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "LeapForward")) {
            WKRetainPtr<WKStringRef> timeKey = adoptWK(WKStringCreateWithUTF8CString("TimeInMilliseconds"));
            unsigned time = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, timeKey.get()))));

            m_eventSenderProxy->leapForward(time);
            return 0;
        }
#endif

#if ENABLE(TOUCH_EVENTS)
        if (WKStringIsEqualToUTF8CString(subMessageName, "AddTouchPoint")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            int x = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get()))));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            int y = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get()))));

            m_eventSenderProxy->addTouchPoint(x, y);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "UpdateTouchPoint")) {
            WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
            int index = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, indexKey.get()))));

            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            int x = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get()))));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            int y = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get()))));

            m_eventSenderProxy->updateTouchPoint(index, x, y);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetTouchModifier")) {
            WKRetainPtr<WKStringRef> modifierKey = adoptWK(WKStringCreateWithUTF8CString("Modifier"));
            WKEventModifiers modifier = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifierKey.get()))));

            WKRetainPtr<WKStringRef> enableKey = adoptWK(WKStringCreateWithUTF8CString("Enable"));
            bool enable = static_cast<bool>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, enableKey.get()))));

            m_eventSenderProxy->setTouchModifier(modifier, enable);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchStart")) {
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->touchStart();
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchMove")) {
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->touchMove();
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchEnd")) {
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->touchEnd();
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "ClearTouchPoints")) {
            m_eventSenderProxy->clearTouchPoints();
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "ReleaseTouchPoint")) {
            WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
            int index = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, indexKey.get()))));
            m_eventSenderProxy->releaseTouchPoint(index);
            return 0;
        }
#endif
        ASSERT_NOT_REACHED();
    }
#endif
    return m_currentInvocation->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody);
}

// WKPageLoaderClient

void TestController::didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didCommitLoadForFrame(page, frame);
}

void TestController::didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didFinishLoadForFrame(page, frame);
}

void TestController::processDidCrash(WKPageRef page, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->processDidCrash();
}

void TestController::didCommitLoadForFrame(WKPageRef page, WKFrameRef frame)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    mainWebView()->focus();
}

void TestController::didFinishLoadForFrame(WKPageRef page, WKFrameRef frame)
{
    if (m_state != Resetting)
        return;

    if (!WKFrameIsMainFrame(frame))
        return;

    WKRetainPtr<WKURLRef> wkURL(AdoptWK, WKFrameCopyURL(frame));
    if (!WKURLIsEqual(wkURL.get(), blankURL()))
        return;

    m_doneResetting = true;
    shared().notifyDone();
}

void TestController::processDidCrash()
{
    // This function can be called multiple times when crash logs are being saved on Windows, so
    // ensure we only print the crashed message once.
    if (!m_didPrintWebProcessCrashedMessage) {
        fputs("#CRASHED - WebProcess\n", stderr);
        fflush(stderr);
        m_didPrintWebProcessCrashedMessage = true;
    }

    if (m_shouldExitWhenWebProcessCrashes)
        exit(1);
}

} // namespace WTR
