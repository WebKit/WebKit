/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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

#include "EventSenderProxy.h"
#include "Options.h"
#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestInvocation.h"
#include <WebKit/WKAuthenticationChallenge.h>
#include <WebKit/WKAuthenticationDecisionListener.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKCookieManager.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKIconDatabase.h>
#include <WebKit/WKNavigationResponseRef.h>
#include <WebKit/WKNotification.h>
#include <WebKit/WKNotificationManager.h>
#include <WebKit/WKNotificationPermissionRequest.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKProtectionSpace.h>
#include <WebKit/WKRetainPtr.h>
#include <algorithm>
#include <cstdio>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <wtf/text/CString.h>

#if PLATFORM(COCOA)
#include <WebKit/WKContextPrivateMac.h>
#include <WebKit/WKPagePrivateMac.h>
#endif

#if !PLATFORM(COCOA)
#include <WebKit/WKTextChecker.h>
#endif

namespace WTR {

const unsigned TestController::viewWidth = 800;
const unsigned TestController::viewHeight = 600;

const unsigned TestController::w3cSVGViewWidth = 480;
const unsigned TestController::w3cSVGViewHeight = 360;

#if ASAN_ENABLED
const double TestController::shortTimeout = 10.0;
#else
const double TestController::shortTimeout = 5.0;
#endif

const double TestController::noTimeout = -1;

static WKURLRef blankURL()
{
    static WKURLRef staticBlankURL = WKURLCreateWithUTF8CString("about:blank");
    return staticBlankURL;
}

static WKDataRef copyWebCryptoMasterKey(WKPageRef, const void*)
{
    // Any 128 bit key would do, all we need for testing is to implement the callback.
    return WKDataCreate((const uint8_t*)"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", 16);
}

static TestController* controller;

TestController& TestController::singleton()
{
    ASSERT(controller);
    return *controller;
}

TestController::TestController(int argc, const char* argv[])
    : m_verbose(false)
    , m_printSeparators(false)
    , m_usingServerMode(false)
    , m_gcBetweenTests(false)
    , m_shouldDumpPixelsForAllTests(false)
    , m_state(Initial)
    , m_doneResetting(false)
    , m_useWaitToDumpWatchdogTimer(true)
    , m_forceNoTimeout(false)
    , m_didPrintWebProcessCrashedMessage(false)
    , m_shouldExitWhenWebProcessCrashes(true)
    , m_beforeUnloadReturnValue(true)
    , m_isGeolocationPermissionSet(false)
    , m_isGeolocationPermissionAllowed(false)
    , m_policyDelegateEnabled(false)
    , m_policyDelegatePermissive(false)
    , m_handlesAuthenticationChallenges(false)
    , m_shouldBlockAllPlugins(false)
    , m_forceComplexText(false)
    , m_shouldUseAcceleratedDrawing(false)
    , m_shouldUseRemoteLayerTree(false)
    , m_shouldLogHistoryClientCallbacks(false)
{
    initialize(argc, argv);
    controller = this;
    run();
    controller = 0;
}

TestController::~TestController()
{
    WKIconDatabaseClose(WKContextGetIconDatabase(m_context.get()));

    platformDestroy();
}

static WKRect getWindowFrame(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    return view->windowFrame();
}

static void setWindowFrame(WKPageRef page, WKRect frame, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    view->setWindowFrame(frame);
}

static bool runBeforeUnloadConfirmPanel(WKPageRef page, WKStringRef message, WKFrameRef frame, const void*)
{
    printf("CONFIRM NAVIGATION: %s\n", toSTD(message).c_str());
    return TestController::singleton().beforeUnloadReturnValue();
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
    view->focus();
    view->setWindowIsKey(true);
}

static void unfocus(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    view->setWindowIsKey(false);
}

static void decidePolicyForGeolocationPermissionRequest(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKGeolocationPermissionRequestRef permissionRequest, const void* clientInfo)
{
    TestController::singleton().handleGeolocationPermissionRequest(permissionRequest);
}

static void decidePolicyForUserMediaPermissionRequest(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKUserMediaPermissionRequestRef permissionRequest, const void* clientInfo)
{
    TestController::singleton().handleUserMediaPermissionRequest(permissionRequest);
}

WKPageRef TestController::createOtherPage(WKPageRef oldPage, WKURLRequestRef, WKDictionaryRef, WKEventModifiers, WKEventMouseButton, const void* clientInfo)
{
    PlatformWebView* parentView = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));

    PlatformWebView* view = new PlatformWebView(WKPageGetContext(oldPage), WKPageGetPageGroup(oldPage), oldPage, parentView->options());
    WKPageRef newPage = view->page();

    view->resizeTo(800, 600);

    WKPageUIClientV5 otherPageUIClient = {
        { 5, view },
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
        getWindowFrame,
        setWindowFrame,
        runBeforeUnloadConfirmPanel,
        0, // didDraw
        0, // pageDidScroll
        0, // exceededDatabaseQuota
        0, // runOpenPanel
        decidePolicyForGeolocationPermissionRequest,
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
        0, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        0, // showColorPicker
        0, // hideColorPicker
        0, // unavailablePluginButtonClicked
        0, // pinnedStateDidChange
        0, // didBeginTrackingPotentialLongMousePress
        0, // didRecognizeLongMousePress
        0, // didCancelTrackingPotentialLongMousePress
        0, // isPlayingAudioDidChange
        decidePolicyForUserMediaPermissionRequest,
    };
    WKPageSetPageUIClient(newPage, &otherPageUIClient.base);
    
    WKPageNavigationClientV0 pageNavigationClient = {
        { 0, &TestController::singleton() },
        decidePolicyForNavigationAction,
        decidePolicyForNavigationResponse,
        decidePolicyForPluginLoad,
        0, // didStartProvisionalNavigation
        0, // didReceiveServerRedirectForProvisionalNavigation
        0, // didFailProvisionalNavigation
        0, // didCommitNavigation
        0, // didFinishNavigation
        0, // didFailNavigation
        0, // didFailProvisionalLoadInSubframe
        0, // didFinishDocumentLoad
        0, // didSameDocumentNavigation
        0, // renderingProgressDidChange
        canAuthenticateAgainstProtectionSpace,
        didReceiveAuthenticationChallenge,
        processDidCrash,
        copyWebCryptoMasterKey,
    };
    WKPageSetPageNavigationClient(newPage, &pageNavigationClient.base);

    view->didInitializeClients();

    TestController::singleton().updateWindowScaleForTest(view, *TestController::singleton().m_currentInvocation);

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

    Options options;
    OptionsHandler optionsHandler(options);

    if (argc < 2) {
        optionsHandler.printHelp();
        exit(1);
    }
    if (!optionsHandler.parse(argc, argv))
        exit(1);

    m_useWaitToDumpWatchdogTimer = options.useWaitToDumpWatchdogTimer;
    m_forceNoTimeout = options.forceNoTimeout;
    m_verbose = options.verbose;
    m_gcBetweenTests = options.gcBetweenTests;
    m_shouldDumpPixelsForAllTests = options.shouldDumpPixelsForAllTests;
    m_forceComplexText = options.forceComplexText;
    m_shouldUseAcceleratedDrawing = options.shouldUseAcceleratedDrawing;
    m_shouldUseRemoteLayerTree = options.shouldUseRemoteLayerTree;
    m_paths = options.paths;

    if (options.printSupportedFeatures) {
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

    auto configuration = adoptWK(WKContextConfigurationCreate());
    WKContextConfigurationSetInjectedBundlePath(configuration.get(), injectedBundlePath());

    if (const char* dumpRenderTreeTemp = libraryPathForTesting()) {
        String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);

        const char separator = '/';

        WKContextConfigurationSetIndexedDBDatabaseDirectory(configuration.get(), toWK(temporaryFolder + separator + "Databases" + separator + "IndexedDB").get());
        WKContextConfigurationSetLocalStorageDirectory(configuration.get(), toWK(temporaryFolder + separator + "LocalStorage").get());
        WKContextConfigurationSetWebSQLDatabaseDirectory(configuration.get(), toWK(temporaryFolder + separator + "Databases" + separator + "WebSQL").get());
    }

    m_context = adoptWK(WKContextCreateWithConfiguration(configuration.get()));
    m_geolocationProvider = std::make_unique<GeolocationProviderMock>(m_context.get());

#if PLATFORM(EFL)
    WKContextSetUsesNetworkProcess(m_context.get(), false);
    WKContextSetProcessModel(m_context.get(), kWKProcessModelSharedSecondaryProcess);
#endif

    if (const char* dumpRenderTreeTemp = libraryPathForTesting()) {
        String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);

        const char separator = '/';

        // FIXME: These should be migrated to WKContextConfigurationRef.
        WKContextSetApplicationCacheDirectory(m_context.get(), toWK(temporaryFolder + separator + "ApplicationCache").get());
        WKContextSetDiskCacheDirectory(m_context.get(), toWK(temporaryFolder + separator + "Cache").get());
        WKContextSetCookieStorageDirectory(m_context.get(), toWK(temporaryFolder + separator + "Cookies").get());
        // Disable icon database to avoid fetching <http://127.0.0.1:8000/favicon.ico> and making tests flaky.
        // Invividual tests can enable it using testRunner.setIconDatabaseEnabled, although it's not currently supported in WebKitTestRunner.
        WKContextSetIconDatabasePath(m_context.get(), toWK(emptyString()).get());
    }

    WKContextUseTestingNetworkSession(m_context.get());
    WKContextSetCacheModel(m_context.get(), kWKCacheModelDocumentBrowser);

    platformInitializeContext();

    WKContextInjectedBundleClientV1 injectedBundleClient = {
        { 1, this },
        didReceiveMessageFromInjectedBundle,
        didReceiveSynchronousMessageFromInjectedBundle,
        0 // getInjectedBundleInitializationUserData
    };
    WKContextSetInjectedBundleClient(m_context.get(), &injectedBundleClient.base);

    WKContextClientV1 contextClient = {
        { 1, this },
        0, // plugInAutoStartOriginHashesChanged
        networkProcessDidCrash,
        0, // plugInInformationBecameAvailable
        0, // copyWebCryptoMasterKey
    };
    WKContextSetClient(m_context.get(), &contextClient.base);

    WKContextHistoryClientV0 historyClient = {
        { 0, this },
        didNavigateWithNavigationData,
        didPerformClientRedirect,
        didPerformServerRedirect,
        didUpdateHistoryTitle,
        0, // populateVisitedLinks
    };
    WKContextSetHistoryClient(m_context.get(), &historyClient.base);

    WKNotificationManagerRef notificationManager = WKContextGetNotificationManager(m_context.get());
    WKNotificationProviderV0 notificationKit = m_webNotificationProvider.provider();
    WKNotificationManagerSetProvider(notificationManager, &notificationKit.base);

    if (testPluginDirectory())
        WKContextSetAdditionalPluginsDirectory(m_context.get(), testPluginDirectory());

    if (m_forceComplexText)
        WKContextSetAlwaysUsesComplexTextCodePath(m_context.get(), true);

    // Some preferences (notably mock scroll bars setting) currently cannot be re-applied to an existing view, so we need to set them now.
    resetPreferencesToConsistentValues();

    WKRetainPtr<WKMutableDictionaryRef> viewOptions;
    if (m_shouldUseRemoteLayerTree) {
        viewOptions = adoptWK(WKMutableDictionaryCreate());
        WKRetainPtr<WKStringRef> useRemoteLayerTreeKey = adoptWK(WKStringCreateWithUTF8CString("RemoteLayerTree"));
        WKRetainPtr<WKBooleanRef> useRemoteLayerTreeValue = adoptWK(WKBooleanCreate(m_shouldUseRemoteLayerTree));
        WKDictionarySetItem(viewOptions.get(), useRemoteLayerTreeKey.get(), useRemoteLayerTreeValue.get());
    }

    createWebViewWithOptions(viewOptions.get());
}

void TestController::createWebViewWithOptions(WKDictionaryRef options)
{
    m_mainWebView = std::make_unique<PlatformWebView>(m_context.get(), m_pageGroup.get(), nullptr, options);
    WKPageUIClientV5 pageUIClient = {
        { 5, m_mainWebView.get() },
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        0, // close
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
        getWindowFrame,
        setWindowFrame,
        runBeforeUnloadConfirmPanel,
        0, // didDraw
        0, // pageDidScroll
        0, // exceededDatabaseQuota,
        0, // runOpenPanel
        decidePolicyForGeolocationPermissionRequest,
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
        decidePolicyForNotificationPermissionRequest, // decidePolicyForNotificationPermissionRequest
        0, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        0, // showColorPicker
        0, // hideColorPicker
        unavailablePluginButtonClicked,
        0, // pinnedStateDidChange
        0, // didBeginTrackingPotentialLongMousePress
        0, // didRecognizeLongMousePress
        0, // didCancelTrackingPotentialLongMousePress
        0, // isPlayingAudioDidChange
        decidePolicyForUserMediaPermissionRequest,
    };
    WKPageSetPageUIClient(m_mainWebView->page(), &pageUIClient.base);

    WKPageNavigationClientV0 pageNavigationClient = {
        { 0, this },
        decidePolicyForNavigationAction,
        decidePolicyForNavigationResponse,
        decidePolicyForPluginLoad,
        0, // didStartProvisionalNavigation
        0, // didReceiveServerRedirectForProvisionalNavigation
        0, // didFailProvisionalNavigation
        didCommitNavigation,
        didFinishNavigation,
        0, // didFailNavigation
        0, // didFailProvisionalLoadInSubframe
        0, // didFinishDocumentLoad
        0, // didSameDocumentNavigation
        0, // renderingProgressDidChange
        canAuthenticateAgainstProtectionSpace,
        didReceiveAuthenticationChallenge,
        processDidCrash,
        copyWebCryptoMasterKey,
    };
    WKPageSetPageNavigationClient(m_mainWebView->page(), &pageNavigationClient.base);

    m_mainWebView->didInitializeClients();

    // Generally, the tests should default to running at 1x. updateWindowScaleForTest() will adjust the scale to
    // something else for specific tests that need to run at a different window scale.
    m_mainWebView->changeWindowScaleIfNeeded(1);
}

void TestController::ensureViewSupportsOptions(WKDictionaryRef options)
{
    if (m_mainWebView && !m_mainWebView->viewSupportsOptions(options)) {
        WKPageSetPageUIClient(m_mainWebView->page(), 0);
        WKPageSetPageNavigationClient(m_mainWebView->page(), 0);
        WKPageClose(m_mainWebView->page());
        
        m_mainWebView = nullptr;

        createWebViewWithOptions(options);
        resetStateToConsistentValues();
    }
}

void TestController::resetPreferencesToConsistentValues()
{
    // Reset preferences
    WKPreferencesRef preferences = WKPageGroupGetPreferences(m_pageGroup.get());
    WKPreferencesResetTestRunnerOverrides(preferences);
    WKPreferencesSetOfflineWebApplicationCacheEnabled(preferences, true);
    WKPreferencesSetFontSmoothingLevel(preferences, kWKFontSmoothingLevelNoSubpixelAntiAliasing);
    WKPreferencesSetXSSAuditorEnabled(preferences, false);
    WKPreferencesSetWebAudioEnabled(preferences, true);
    WKPreferencesSetMediaStreamEnabled(preferences, true);
    WKPreferencesSetDeveloperExtrasEnabled(preferences, true);
    WKPreferencesSetJavaScriptRuntimeFlags(preferences, kWKJavaScriptRuntimeFlagsAllEnabled);
    WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(preferences, true);
    WKPreferencesSetJavaScriptCanAccessClipboard(preferences, true);
    WKPreferencesSetDOMPasteAllowed(preferences, true);
    WKPreferencesSetUniversalAccessFromFileURLsAllowed(preferences, true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences, true);
#if ENABLE(FULLSCREEN_API)
    WKPreferencesSetFullScreenEnabled(preferences, true);
#endif
    WKPreferencesSetPageCacheEnabled(preferences, false);
    WKPreferencesSetAsynchronousPluginInitializationEnabled(preferences, false);
    WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(preferences, false);
    WKPreferencesSetArtificialPluginInitializationDelayEnabled(preferences, false);
    WKPreferencesSetTabToLinksEnabled(preferences, false);
    WKPreferencesSetInteractiveFormValidationEnabled(preferences, true);
    WKPreferencesSetMockScrollbarsEnabled(preferences, true);

    static WKStringRef defaultTextEncoding = WKStringCreateWithUTF8CString("ISO-8859-1");
    WKPreferencesSetDefaultTextEncodingName(preferences, defaultTextEncoding);

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
    WKPreferencesSetAsynchronousSpellCheckingEnabled(preferences, false);
#if ENABLE(WEB_AUDIO)
    WKPreferencesSetMediaSourceEnabled(preferences, true);
#endif

    WKPreferencesSetAcceleratedDrawingEnabled(preferences, m_shouldUseAcceleratedDrawing);

    WKCookieManagerDeleteAllCookies(WKContextGetCookieManager(m_context.get()));

    platformResetPreferencesToConsistentValues();
}

bool TestController::resetStateToConsistentValues()
{
    m_state = Resetting;

    m_beforeUnloadReturnValue = true;

    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("Reset"));
    WKRetainPtr<WKMutableDictionaryRef> resetMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> shouldGCKey = adoptWK(WKStringCreateWithUTF8CString("ShouldGC"));
    WKRetainPtr<WKBooleanRef> shouldGCValue = adoptWK(WKBooleanCreate(m_gcBetweenTests));
    WKDictionarySetItem(resetMessageBody.get(), shouldGCKey.get(), shouldGCValue.get());

    WKContextPostMessageToInjectedBundle(TestController::singleton().context(), messageName.get(), resetMessageBody.get());

    WKContextSetShouldUseFontSmoothing(TestController::singleton().context(), false);

    WKContextSetCacheModel(TestController::singleton().context(), kWKCacheModelDocumentBrowser);

    // FIXME: This function should also ensure that there is only one page open.

    // Reset the EventSender for each test.
    m_eventSenderProxy = std::make_unique<EventSenderProxy>(this);

    // FIXME: Is this needed? Nothing in TestController changes preferences during tests, and if there is
    // some other code doing this, it should probably be responsible for cleanup too.
    resetPreferencesToConsistentValues();

#if !PLATFORM(COCOA)
    WKTextCheckerContinuousSpellCheckingEnabledStateChanged(true);
#endif

    // in the case that a test using the chrome input field failed, be sure to clean up for the next test
    m_mainWebView->removeChromeInputField();
    m_mainWebView->focus();

    // Re-set to the default backing scale factor by setting the custom scale factor to 0.
    WKPageSetCustomBackingScaleFactor(m_mainWebView->page(), 0);

#if PLATFORM(EFL)
    // EFL use a real window while other ports such as Qt don't.
    // In EFL, we need to resize the window to the original size after calls to window.resizeTo.
    WKRect rect = m_mainWebView->windowFrame();
    m_mainWebView->setWindowFrame(WKRectMake(rect.origin.x, rect.origin.y, TestController::viewWidth, TestController::viewHeight));
#endif

    // Reset notification permissions
    m_webNotificationProvider.reset();

    // Reset Geolocation permissions.
    m_geolocationPermissionRequests.clear();
    m_isGeolocationPermissionSet = false;
    m_isGeolocationPermissionAllowed = false;

    // Reset UserMedia permissions.
    m_userMediaPermissionRequests.clear();
    m_isUserMediaPermissionSet = false;
    m_isUserMediaPermissionAllowed = false;

    // Reset Custom Policy Delegate.
    setCustomPolicyDelegate(false, false);

    m_workQueueManager.clearWorkQueue();

    m_handlesAuthenticationChallenges = false;
    m_authenticationUsername = String();
    m_authenticationPassword = String();

    m_shouldBlockAllPlugins = false;

    m_shouldLogHistoryClientCallbacks = false;

    // Reset main page back to about:blank
    m_doneResetting = false;

    WKPageLoadURL(m_mainWebView->page(), blankURL());
    runUntil(m_doneResetting, shortTimeout);
    return m_doneResetting;
}

void TestController::terminateWebContentProcess()
{
    WKPageTerminate(m_mainWebView->page());
}

void TestController::reattachPageToWebProcess()
{
    // Loading a web page is the only way to reattach an existing page to a process.
    m_doneResetting = false;
    WKPageLoadURL(m_mainWebView->page(), blankURL());
    runUntil(m_doneResetting, shortTimeout);
}

const char* TestController::webProcessName()
{
    // FIXME: Find a way to not hardcode the process name.
#if PLATFORM(IOS)
    return "com.apple.WebKit.WebContent";
#elif PLATFORM(MAC)
    return "com.apple.WebKit.WebContent.Development";
#else
    return "WebProcess";
#endif
}

const char* TestController::networkProcessName()
{
    // FIXME: Find a way to not hardcode the process name.
#if PLATFORM(IOS)
    return "com.apple.WebKit.Networking";
#elif PLATFORM(MAC)
    return "com.apple.WebKit.Networking.Development";
#else
    return "NetworkProcess";
#endif
}

void TestController::updateWebViewSizeForTest(const TestInvocation& test)
{
    bool isSVGW3CTest = strstr(test.pathOrURL(), "svg/W3C-SVG-1.1") || strstr(test.pathOrURL(), "svg\\W3C-SVG-1.1");

    unsigned width = viewWidth;
    unsigned height = viewHeight;
    if (isSVGW3CTest) {
        width = w3cSVGViewWidth;
        height = w3cSVGViewHeight;
    }

    mainWebView()->resizeTo(width, height);
}

void TestController::updateWindowScaleForTest(PlatformWebView* view, const TestInvocation& test)
{
    WTF::String localPathOrUrl = String(test.pathOrURL());
    bool needsHighDPIWindow = localPathOrUrl.findIgnoringCase("/hidpi-") != notFound;
    view->changeWindowScaleIfNeeded(needsHighDPIWindow ? 2 : 1);
}

// FIXME: move into relevant platformConfigureViewForTest()?
static bool shouldUseFixedLayout(const char* pathOrURL)
{
#if ENABLE(CSS_DEVICE_ADAPTATION)
    if (strstr(pathOrURL, "device-adapt/") || strstr(pathOrURL, "device-adapt\\"))
        return true;
#endif

#if USE(TILED_BACKING_STORE) && PLATFORM(EFL)
    if (strstr(pathOrURL, "sticky/") || strstr(pathOrURL, "sticky\\"))
        return true;
#endif
    return false;

    UNUSED_PARAM(pathOrURL);
}

void TestController::updateLayoutTypeForTest(const TestInvocation& test)
{
    auto viewOptions = adoptWK(WKMutableDictionaryCreate());
    auto useFixedLayoutKey = adoptWK(WKStringCreateWithUTF8CString("UseFixedLayout"));
    auto useFixedLayoutValue = adoptWK(WKBooleanCreate(shouldUseFixedLayout(test.pathOrURL())));
    WKDictionarySetItem(viewOptions.get(), useFixedLayoutKey.get(), useFixedLayoutValue.get());

    ensureViewSupportsOptions(viewOptions.get());
}

#if !PLATFORM(COCOA)
void TestController::platformConfigureViewForTest(const TestInvocation&)
{
}

void TestController::platformResetPreferencesToConsistentValues()
{
}
#endif

void TestController::configureViewForTest(const TestInvocation& test)
{
    updateWebViewSizeForTest(test);
    updateWindowScaleForTest(mainWebView(), test);
    updateLayoutTypeForTest(test);

    platformConfigureViewForTest(test);
}

struct TestCommand {
    TestCommand() : shouldDumpPixels(false), timeout(0) { }

    std::string pathOrURL;
    bool shouldDumpPixels;
    std::string expectedPixelHash;
    int timeout;
};

class CommandTokenizer {
public:
    explicit CommandTokenizer(const std::string& input)
        : m_input(input)
        , m_posNextSeparator(0)
    {
        pump();
    }

    bool hasNext() const;
    std::string next();

private:
    void pump();
    static const char kSeparator = '\'';
    const std::string& m_input;
    std::string m_next;
    size_t m_posNextSeparator;
};

void CommandTokenizer::pump()
{
    if (m_posNextSeparator == std::string::npos || m_posNextSeparator == m_input.size()) {
        m_next = std::string();
        return;
    }
    size_t start = m_posNextSeparator ? m_posNextSeparator + 1 : 0;
    m_posNextSeparator = m_input.find(kSeparator, start);
    size_t size = m_posNextSeparator == std::string::npos ? std::string::npos : m_posNextSeparator - start;
    m_next = std::string(m_input, start, size);
}

std::string CommandTokenizer::next()
{
    ASSERT(hasNext());

    std::string oldNext = m_next;
    pump();
    return oldNext;
}

bool CommandTokenizer::hasNext() const
{
    return !m_next.empty();
}

NO_RETURN static void die(const std::string& inputLine)
{
    fprintf(stderr, "Unexpected input line: %s\n", inputLine.c_str());
    exit(1);
}

TestCommand parseInputLine(const std::string& inputLine)
{
    TestCommand result;
    CommandTokenizer tokenizer(inputLine);
    if (!tokenizer.hasNext())
        die(inputLine);

    std::string arg = tokenizer.next();
    result.pathOrURL = arg;
    while (tokenizer.hasNext()) {
        arg = tokenizer.next();
        if (arg == std::string("--timeout")) {
            std::string timeoutToken = tokenizer.next();
            result.timeout = atoi(timeoutToken.c_str());
        } else if (arg == std::string("-p") || arg == std::string("--pixel-test")) {
            result.shouldDumpPixels = true;
            if (tokenizer.hasNext())
                result.expectedPixelHash = tokenizer.next();
        } else
            die(inputLine);
    }
    return result;
}

bool TestController::runTest(const char* inputLine)
{
    TestCommand command = parseInputLine(std::string(inputLine));

    m_state = RunningTest;

    m_currentInvocation = std::make_unique<TestInvocation>(command.pathOrURL);
    if (command.shouldDumpPixels || m_shouldDumpPixelsForAllTests)
        m_currentInvocation->setIsPixelTest(command.expectedPixelHash);
    if (command.timeout > 0)
        m_currentInvocation->setCustomTimeout(command.timeout);

    platformWillRunTest(*m_currentInvocation);

    m_currentInvocation->invoke();
    m_currentInvocation = nullptr;

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
    if (!resetStateToConsistentValues()) {
        TestInvocation::dumpWebProcessUnresponsiveness("<unknown> - TestController::run - Failed to reset state to consistent values\n");
        return;
    }

    if (m_usingServerMode)
        runTestingServerLoop();
    else {
        for (size_t i = 0; i < m_paths.size(); ++i) {
            if (!runTest(m_paths[i].c_str()))
                break;
        }
    }
}

void TestController::runUntil(bool& done, double timeout)
{
    if (m_forceNoTimeout)
        timeout = noTimeout;

    platformRunUntil(done, timeout);
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

void TestController::networkProcessDidCrash(WKContextRef context, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->networkProcessDidCrash();
}

void TestController::didReceiveKeyDownMessageFromInjectedBundle(WKDictionaryRef messageBodyDictionary, bool synchronous)
{
    WKRetainPtr<WKStringRef> keyKey = adoptWK(WKStringCreateWithUTF8CString("Key"));
    WKStringRef key = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, keyKey.get()));

    WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
    WKEventModifiers modifiers = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifiersKey.get()))));

    WKRetainPtr<WKStringRef> locationKey = adoptWK(WKStringCreateWithUTF8CString("Location"));
    unsigned location = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, locationKey.get()))));

    if (synchronous)
        WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);

    m_eventSenderProxy->keyDown(key, modifiers, location);

    if (synchronous)
        WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
}

void TestController::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "EventSender")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
        WKStringRef subMessageName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subMessageKey.get()));

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown") || WKStringIsEqualToUTF8CString(subMessageName, "MouseUp")) {
            WKRetainPtr<WKStringRef> buttonKey = adoptWK(WKStringCreateWithUTF8CString("Button"));
            unsigned button = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, buttonKey.get()))));

            WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
            WKEventModifiers modifiers = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifiersKey.get()))));

            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown"))
                m_eventSenderProxy->mouseDown(button, modifiers);
            else
                m_eventSenderProxy->mouseUp(button, modifiers);

            return;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "KeyDown")) {
            didReceiveKeyDownMessageFromInjectedBundle(messageBodyDictionary, false);

            return;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseScrollByWithWheelAndMomentumPhases")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));
            
            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));
            
            WKRetainPtr<WKStringRef> phaseKey = adoptWK(WKStringCreateWithUTF8CString("Phase"));
            int phase = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, phaseKey.get()))));
            WKRetainPtr<WKStringRef> momentumKey = adoptWK(WKStringCreateWithUTF8CString("Momentum"));
            int momentum = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, momentumKey.get()))));
            
            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            m_eventSenderProxy->mouseScrollByWithWheelAndMomentumPhases(x, y, phase, momentum);

            return;
        }

        ASSERT_NOT_REACHED();
    }

    if (!m_currentInvocation)
        return;

    m_currentInvocation->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

WKRetainPtr<WKTypeRef> TestController::didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "EventSender")) {
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> subMessageKey(AdoptWK, WKStringCreateWithUTF8CString("SubMessage"));
        WKStringRef subMessageName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subMessageKey.get()));

        if (WKStringIsEqualToUTF8CString(subMessageName, "KeyDown")) {
            didReceiveKeyDownMessageFromInjectedBundle(messageBodyDictionary, true);

            return 0;
        }

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

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseScrollByWithWheelAndMomentumPhases")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));
            
            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));
            
            WKRetainPtr<WKStringRef> phaseKey = adoptWK(WKStringCreateWithUTF8CString("Phase"));
            int phase = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, phaseKey.get()))));
            WKRetainPtr<WKStringRef> momentumKey = adoptWK(WKStringCreateWithUTF8CString("Momentum"));
            int momentum = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, momentumKey.get()))));

            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->mouseScrollByWithWheelAndMomentumPhases(x, y, phase, momentum);
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }
        
        if (WKStringIsEqualToUTF8CString(subMessageName, "ContinuousMouseScrollBy")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));

            WKRetainPtr<WKStringRef> pagedKey = adoptWK(WKStringCreateWithUTF8CString("Paged"));
            bool paged = static_cast<bool>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, pagedKey.get()))));

            // Forward to WebProcess
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->continuousMouseScrollBy(x, y, paged);
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), false);
            return 0;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "LeapForward")) {
            WKRetainPtr<WKStringRef> timeKey = adoptWK(WKStringCreateWithUTF8CString("TimeInMilliseconds"));
            unsigned time = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, timeKey.get()))));

            m_eventSenderProxy->leapForward(time);
            return 0;
        }

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

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetTouchPointRadius")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("RadiusX"));
            int x = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get()))));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("RadiusY"));
            int y = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get()))));

            m_eventSenderProxy->setTouchPointRadius(x, y);
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

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchCancel")) {
            WKPageSetShouldSendEventsSynchronously(mainWebView()->page(), true);
            m_eventSenderProxy->touchCancel();
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

        if (WKStringIsEqualToUTF8CString(subMessageName, "CancelTouchPoint")) {
            WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
            int index = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, indexKey.get()))));
            m_eventSenderProxy->cancelTouchPoint(index);
            return 0;
        }
#endif
        ASSERT_NOT_REACHED();
    }
    return m_currentInvocation->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody);
}

// WKContextClient

void TestController::networkProcessDidCrash()
{
#if PLATFORM(COCOA)
    pid_t pid = WKContextGetNetworkProcessIdentifier(m_context.get());
    fprintf(stderr, "#CRASHED - %s (pid %ld)\n", networkProcessName(), static_cast<long>(pid));
#else
    fprintf(stderr, "#CRASHED - %s\n", networkProcessName());
#endif
    exit(1);
}

// WKPageNavigationClient

void TestController::didCommitNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didCommitNavigation(page, navigation);
}

void TestController::didFinishNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didFinishNavigation(page, navigation);
}

bool TestController::canAuthenticateAgainstProtectionSpace(WKPageRef, WKProtectionSpaceRef protectionSpace, const void*)
{
    WKProtectionSpaceAuthenticationScheme authenticationScheme = WKProtectionSpaceGetAuthenticationScheme(protectionSpace);

    if (authenticationScheme == kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {
        std::string host = toSTD(adoptWK(WKProtectionSpaceCopyHost(protectionSpace)).get());
        return host == "localhost" || host == "127.0.0.1";
    }

    return authenticationScheme <= kWKProtectionSpaceAuthenticationSchemeHTTPDigest;
}

void TestController::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveAuthenticationChallenge(page, /*frame,*/ authenticationChallenge);
}

void TestController::processDidCrash(WKPageRef page, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->processDidCrash();
}

WKPluginLoadPolicy TestController::decidePolicyForPluginLoad(WKPageRef page, WKPluginLoadPolicy currentPluginLoadPolicy, WKDictionaryRef pluginInformation, WKStringRef* unavailabilityDescription, const void* clientInfo)
{
    return static_cast<TestController*>(const_cast<void*>(clientInfo))->decidePolicyForPluginLoad(page, currentPluginLoadPolicy, pluginInformation, unavailabilityDescription);
}

WKPluginLoadPolicy TestController::decidePolicyForPluginLoad(WKPageRef, WKPluginLoadPolicy currentPluginLoadPolicy, WKDictionaryRef pluginInformation, WKStringRef* unavailabilityDescription)
{
    if (m_shouldBlockAllPlugins)
        return kWKPluginLoadPolicyBlocked;
    return currentPluginLoadPolicy;
}

void TestController::didCommitNavigation(WKPageRef page, WKNavigationRef navigation)
{
    mainWebView()->focus();
}

void TestController::didFinishNavigation(WKPageRef page, WKNavigationRef navigation)
{
    if (m_state != Resetting)
        return;

    WKRetainPtr<WKURLRef> wkURL(AdoptWK, WKFrameCopyURL(WKPageGetMainFrame(page)));
    if (!WKURLIsEqual(wkURL.get(), blankURL()))
        return;

    m_doneResetting = true;
    singleton().notifyDone();
}

void TestController::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge)
{
    WKProtectionSpaceRef protectionSpace = WKAuthenticationChallengeGetProtectionSpace(authenticationChallenge);
    WKAuthenticationDecisionListenerRef decisionListener = WKAuthenticationChallengeGetDecisionListener(authenticationChallenge);

    if (WKProtectionSpaceGetAuthenticationScheme(protectionSpace) == kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {
        // Any non-empty credential signals to accept the server trust. Since the cross-platform API
        // doesn't expose a way to create a credential from server trust, we use a password credential.

        WKRetainPtr<WKCredentialRef> credential = adoptWK(WKCredentialCreate(toWK("accept server trust").get(), toWK("").get(), kWKCredentialPersistenceNone));
        WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
        return;
    }

    std::string host = toSTD(adoptWK(WKProtectionSpaceCopyHost(protectionSpace)).get());
    int port = WKProtectionSpaceGetPort(protectionSpace);
    String message = String::format("%s:%d - didReceiveAuthenticationChallenge - ", host.c_str(), port);
    if (!m_handlesAuthenticationChallenges)
        message.append("Simulating cancelled authentication sheet\n");
    else
        message.append(String::format("Responding with %s:%s\n", m_authenticationUsername.utf8().data(), m_authenticationPassword.utf8().data()));
    m_currentInvocation->outputText(message);

    if (!m_handlesAuthenticationChallenges) {
        WKAuthenticationDecisionListenerUseCredential(decisionListener, 0);
        return;
    }
    WKRetainPtr<WKStringRef> username(AdoptWK, WKStringCreateWithUTF8CString(m_authenticationUsername.utf8().data()));
    WKRetainPtr<WKStringRef> password(AdoptWK, WKStringCreateWithUTF8CString(m_authenticationPassword.utf8().data()));
    WKRetainPtr<WKCredentialRef> credential(AdoptWK, WKCredentialCreate(username.get(), password.get(), kWKCredentialPersistenceForSession));
    WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
}

void TestController::processDidCrash()
{
    // This function can be called multiple times when crash logs are being saved on Windows, so
    // ensure we only print the crashed message once.
    if (!m_didPrintWebProcessCrashedMessage) {
#if PLATFORM(COCOA)
        pid_t pid = WKPageGetProcessIdentifier(m_mainWebView->page());
        fprintf(stderr, "#CRASHED - %s (pid %ld)\n", webProcessName(), static_cast<long>(pid));
#else
        fprintf(stderr, "#CRASHED - %s\n", webProcessName());
#endif
        fflush(stderr);
        m_didPrintWebProcessCrashedMessage = true;
    }

    if (m_shouldExitWhenWebProcessCrashes)
        exit(1);
}

void TestController::simulateWebNotificationClick(uint64_t notificationID)
{
    m_webNotificationProvider.simulateWebNotificationClick(notificationID);
}

void TestController::setGeolocationPermission(bool enabled)
{
    m_isGeolocationPermissionSet = true;
    m_isGeolocationPermissionAllowed = enabled;
    decidePolicyForGeolocationPermissionRequestIfPossible();
}

void TestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed)
{
    m_geolocationProvider->setPosition(latitude, longitude, accuracy, providesAltitude, altitude, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed);
}

void TestController::setMockGeolocationPositionUnavailableError(WKStringRef errorMessage)
{
    m_geolocationProvider->setPositionUnavailableError(errorMessage);
}

void TestController::handleGeolocationPermissionRequest(WKGeolocationPermissionRequestRef geolocationPermissionRequest)
{
    m_geolocationPermissionRequests.append(geolocationPermissionRequest);
    decidePolicyForGeolocationPermissionRequestIfPossible();
}

void TestController::setUserMediaPermission(bool enabled)
{
    m_isUserMediaPermissionSet = true;
    m_isUserMediaPermissionAllowed = enabled;
    decidePolicyForUserMediaPermissionRequestIfPossible();
}

void TestController::handleUserMediaPermissionRequest(WKUserMediaPermissionRequestRef userMediaPermissionRequest)
{
    m_userMediaPermissionRequests.append(userMediaPermissionRequest);
    decidePolicyForUserMediaPermissionRequestIfPossible();
}

void TestController::decidePolicyForUserMediaPermissionRequestIfPossible()
{
    if (!m_isUserMediaPermissionSet)
        return;

    for (auto& request : m_userMediaPermissionRequests) {
        if (m_isUserMediaPermissionAllowed)
            WKUserMediaPermissionRequestAllow(request.get());
        else
            WKUserMediaPermissionRequestDeny(request.get());
    }
    m_userMediaPermissionRequests.clear();
}

void TestController::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    m_policyDelegateEnabled = enabled;
    m_policyDelegatePermissive = permissive;
}

void TestController::decidePolicyForGeolocationPermissionRequestIfPossible()
{
    if (!m_isGeolocationPermissionSet)
        return;

    for (size_t i = 0; i < m_geolocationPermissionRequests.size(); ++i) {
        WKGeolocationPermissionRequestRef permissionRequest = m_geolocationPermissionRequests[i].get();
        if (m_isGeolocationPermissionAllowed)
            WKGeolocationPermissionRequestAllow(permissionRequest);
        else
            WKGeolocationPermissionRequestDeny(permissionRequest);
    }
    m_geolocationPermissionRequests.clear();
}

void TestController::decidePolicyForNotificationPermissionRequest(WKPageRef page, WKSecurityOriginRef origin, WKNotificationPermissionRequestRef request, const void*)
{
    TestController::singleton().decidePolicyForNotificationPermissionRequest(page, origin, request);
}

void TestController::decidePolicyForNotificationPermissionRequest(WKPageRef, WKSecurityOriginRef, WKNotificationPermissionRequestRef request)
{
    WKNotificationPermissionRequestAllow(request);
}

void TestController::unavailablePluginButtonClicked(WKPageRef, WKPluginUnavailabilityReason, WKDictionaryRef, const void*)
{
    printf("MISSING PLUGIN BUTTON PRESSED\n");
}

void TestController::decidePolicyForNavigationAction(WKPageRef, WKNavigationActionRef navigationAction, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->decidePolicyForNavigationAction(listener);
}

void TestController::decidePolicyForNavigationAction(WKFramePolicyListenerRef listener)
{
    if (m_policyDelegateEnabled && !m_policyDelegatePermissive) {
        WKFramePolicyListenerIgnore(listener);
        return;
    }

    WKFramePolicyListenerUse(listener);
}

void TestController::decidePolicyForNavigationResponse(WKPageRef, WKNavigationResponseRef navigationResponse, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->decidePolicyForNavigationResponse(navigationResponse, listener);
}

void TestController::decidePolicyForNavigationResponse(WKNavigationResponseRef navigationResponse, WKFramePolicyListenerRef listener)
{
    // Even though Response was already checked by WKBundlePagePolicyClient, the check did not include plugins
    // so we have to re-check again.
    if (WKNavigationResponseCanShowMIMEType(navigationResponse)) {
        WKFramePolicyListenerUse(listener);
        return;
    }

    WKFramePolicyListenerIgnore(listener);
}

void TestController::didNavigateWithNavigationData(WKContextRef, WKPageRef, WKNavigationDataRef navigationData, WKFrameRef frame, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didNavigateWithNavigationData(navigationData, frame);
}

void TestController::didNavigateWithNavigationData(WKNavigationDataRef navigationData, WKFrameRef)
{
    if (m_state != RunningTest)
        return;

    if (!m_shouldLogHistoryClientCallbacks)
        return;

    // URL
    WKRetainPtr<WKURLRef> urlWK = adoptWK(WKNavigationDataCopyURL(navigationData));
    WKRetainPtr<WKStringRef> urlStringWK = adoptWK(WKURLCopyString(urlWK.get()));
    // Title
    WKRetainPtr<WKStringRef> titleWK = adoptWK(WKNavigationDataCopyTitle(navigationData));
    // HTTP method
    WKRetainPtr<WKURLRequestRef> requestWK = adoptWK(WKNavigationDataCopyOriginalRequest(navigationData));
    WKRetainPtr<WKStringRef> methodWK = adoptWK(WKURLRequestCopyHTTPMethod(requestWK.get()));

    // FIXME: Determine whether the navigation was successful / a client redirect rather than hard-coding the message here.
    m_currentInvocation->outputText(String::format("WebView navigated to url \"%s\" with title \"%s\" with HTTP equivalent method \"%s\".  The navigation was successful and was not a client redirect.\n",
        toSTD(urlStringWK).c_str(), toSTD(titleWK).c_str(), toSTD(methodWK).c_str()));
}

void TestController::didPerformClientRedirect(WKContextRef, WKPageRef, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef frame, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didPerformClientRedirect(sourceURL, destinationURL, frame);
}

void TestController::didPerformClientRedirect(WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef)
{
    if (m_state != RunningTest)
        return;

    if (!m_shouldLogHistoryClientCallbacks)
        return;

    WKRetainPtr<WKStringRef> sourceStringWK = adoptWK(WKURLCopyString(sourceURL));
    WKRetainPtr<WKStringRef> destinationStringWK = adoptWK(WKURLCopyString(destinationURL));

    m_currentInvocation->outputText(String::format("WebView performed a client redirect from \"%s\" to \"%s\".\n", toSTD(sourceStringWK).c_str(), toSTD(destinationStringWK).c_str()));
}

void TestController::didPerformServerRedirect(WKContextRef, WKPageRef, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef frame, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didPerformServerRedirect(sourceURL, destinationURL, frame);
}

void TestController::didPerformServerRedirect(WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef)
{
    if (m_state != RunningTest)
        return;

    if (!m_shouldLogHistoryClientCallbacks)
        return;

    WKRetainPtr<WKStringRef> sourceStringWK = adoptWK(WKURLCopyString(sourceURL));
    WKRetainPtr<WKStringRef> destinationStringWK = adoptWK(WKURLCopyString(destinationURL));

    m_currentInvocation->outputText(String::format("WebView performed a server redirect from \"%s\" to \"%s\".\n", toSTD(sourceStringWK).c_str(), toSTD(destinationStringWK).c_str()));
}

void TestController::didUpdateHistoryTitle(WKContextRef, WKPageRef, WKStringRef title, WKURLRef URL, WKFrameRef frame, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didUpdateHistoryTitle(title, URL, frame);
}

void TestController::didUpdateHistoryTitle(WKStringRef title, WKURLRef URL, WKFrameRef)
{
    if (m_state != RunningTest)
        return;

    if (!m_shouldLogHistoryClientCallbacks)
        return;

    WKRetainPtr<WKStringRef> urlStringWK(AdoptWK, WKURLCopyString(URL));
    m_currentInvocation->outputText(String::format("WebView updated the title for history URL \"%s\" to \"%s\".\n", toSTD(urlStringWK).c_str(), toSTD(title).c_str()));
}

} // namespace WTR
