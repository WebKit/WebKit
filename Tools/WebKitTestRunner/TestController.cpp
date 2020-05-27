/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "WebCoreTestSupport.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKAuthenticationChallenge.h>
#include <WebKit/WKAuthenticationDecisionListener.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKFrameHandleRef.h>
#include <WebKit/WKFrameInfoRef.h>
#include <WebKit/WKHTTPCookieStoreRef.h>
#include <WebKit/WKIconDatabase.h>
#include <WebKit/WKMessageListener.h>
#include <WebKit/WKMockDisplay.h>
#include <WebKit/WKMockMediaDevice.h>
#include <WebKit/WKNavigationActionRef.h>
#include <WebKit/WKNavigationResponseRef.h>
#include <WebKit/WKNotification.h>
#include <WebKit/WKNotificationManager.h>
#include <WebKit/WKNotificationPermissionRequest.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKOpenPanelResultListener.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPageInjectedBundleClient.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKPluginInformation.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKProtectionSpace.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKTextChecker.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKUserContentControllerRef.h>
#include <WebKit/WKUserContentExtensionStoreRef.h>
#include <WebKit/WKUserMediaPermissionCheck.h>
#include <WebKit/WKWebsiteDataStoreConfigurationRef.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <WebKit/WKWebsitePolicies.h>
#include <algorithm>
#include <cstdio>
#include <ctype.h>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <wtf/AutodrainedPool.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MainThread.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/UUID.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if PLATFORM(COCOA)
#include <WebKit/WKContextPrivateMac.h>
#include <WebKit/WKPagePrivateMac.h>
#endif

#if PLATFORM(WIN)
#include <direct.h>
#define getcwd _getcwd
#define PATH_MAX _MAX_PATH
#else
#include <unistd.h>
#endif

namespace WTR {

#if OS(WINDOWS)
static constexpr auto pathSeparator = '\\';
#else
static constexpr auto pathSeparator = '/';
#endif

const unsigned TestController::viewWidth = 800;
const unsigned TestController::viewHeight = 600;

const unsigned TestController::w3cSVGViewWidth = 480;
const unsigned TestController::w3cSVGViewHeight = 360;

const WTF::Seconds TestController::defaultShortTimeout = 5_s;
const WTF::Seconds TestController::noTimeout = -1_s;

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

static WKStringRef copySignedPublicKeyAndChallengeString(WKPageRef, const void*)
{
    // Any fake response would do, all we need for testing is to implement the callback.
    return WKStringCreateWithUTF8CString("MIHFMHEwXDANBgkqhkiG9w0BAQEFAANLADBIAkEAnX0TILJrOMUue%2BPtwBRE6XfV%0AWtKQbsshxk5ZhcUwcwyvcnIq9b82QhJdoACdD34rqfCAIND46fXKQUnb0mvKzQID%0AAQABFhFNb3ppbGxhSXNNeUZyaWVuZDANBgkqhkiG9w0BAQQFAANBAAKv2Eex2n%2FS%0Ar%2F7iJNroWlSzSMtTiQTEB%2BADWHGj9u1xrUrOilq%2Fo2cuQxIfZcNZkYAkWP4DubqW%0Ai0%2F%2FrgBvmco%3D");
}

AsyncTask* AsyncTask::m_currentTask;

bool AsyncTask::run()
{
    m_currentTask = this;
    m_task();
    TestController::singleton().runUntil(m_taskDone, m_timeout);
    m_currentTask = nullptr;
    return m_taskDone;
}

AsyncTask* AsyncTask::currentTask()
{
    return m_currentTask;
}

static TestController* controller;

TestController& TestController::singleton()
{
    ASSERT(controller);
    return *controller;
}

TestController::TestController(int argc, const char* argv[])
{
    initialize(argc, argv);
    controller = this;
    run();
    controller = nullptr;
}

TestController::~TestController()
{
    // The context will be null if WebKitTestRunner was in server mode, but ran no tests.
    if (m_context)
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

static void runOpenPanel(WKPageRef page, WKFrameRef frame, WKOpenPanelParametersRef parameters, WKOpenPanelResultListenerRef resultListenerRef, const void*)
{
    printf("OPEN FILE PANEL\n");
    if (WKOpenPanelParametersGetAllowsDirectories(parameters))
        printf("-> DIRECTORIES ARE ALLOWED\n");
    WKArrayRef fileURLs = TestController::singleton().openPanelFileURLs();
    if (!fileURLs || !WKArrayGetSize(fileURLs)) {
        WKOpenPanelResultListenerCancel(resultListenerRef);
        return;
    }
    
    WKTypeRef firstItem = WKArrayGetItemAtIndex(fileURLs, 0);
    
#if PLATFORM(IOS_FAMILY)
    auto displayString = adoptWK(WKURLCopyLastPathComponent(static_cast<WKURLRef>(firstItem)));
    WKDataRef mediaIcon = TestController::singleton().openPanelFileURLsMediaIcon();
    
    if (mediaIcon) {
        if (WKOpenPanelParametersGetAllowsMultipleFiles(parameters)) {
            WKOpenPanelResultListenerChooseMediaFiles(resultListenerRef, fileURLs, displayString.get(), mediaIcon);
            return;
        }
        
        WKOpenPanelResultListenerChooseMediaFiles(resultListenerRef, adoptWK(WKArrayCreate(&firstItem, 1)).get(), displayString.get(), mediaIcon);
        return;
    }
#endif

    if (WKOpenPanelParametersGetAllowsMultipleFiles(parameters)) {
        WKOpenPanelResultListenerChooseFiles(resultListenerRef, fileURLs);
        return;
    }

    WKOpenPanelResultListenerChooseFiles(resultListenerRef, adoptWK(WKArrayCreate(&firstItem, 1)).get());
}

void TestController::runModal(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    TestController::singleton().mainWebView()->setWindowIsKey(false);
    runModal(view);
    TestController::singleton().mainWebView()->setWindowIsKey(true);
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

static void decidePolicyForUserMediaPermissionRequest(WKPageRef, WKFrameRef frame, WKSecurityOriginRef userMediaDocumentOrigin, WKSecurityOriginRef topLevelDocumentOrigin, WKUserMediaPermissionRequestRef permissionRequest, const void* clientInfo)
{
    TestController::singleton().handleUserMediaPermissionRequest(frame, userMediaDocumentOrigin, topLevelDocumentOrigin, permissionRequest);
}

static void runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, WKSecurityOriginRef securityOrigin, WKPageRunJavaScriptAlertResultListenerRef listener, const void *clientInfo)
{
    TestController::singleton().handleJavaScriptAlert(listener);
}

static void checkUserMediaPermissionForOrigin(WKPageRef, WKFrameRef frame, WKSecurityOriginRef userMediaDocumentOrigin, WKSecurityOriginRef topLevelDocumentOrigin, WKUserMediaPermissionCheckRef checkRequest, const void*)
{
    TestController::singleton().handleCheckOfUserMediaPermissionForOrigin(frame, userMediaDocumentOrigin, topLevelDocumentOrigin, checkRequest);
}

static void requestPointerLock(WKPageRef page, const void*)
{
    WKPageDidAllowPointerLock(page);
}

static void printFrame(WKPageRef page, WKFrameRef frame, const void*)
{
    WKPageBeginPrinting(page, frame, WKPrintInfo { 1, 21, 29.7f });
    WKPageEndPrinting(page);
}

static bool shouldAllowDeviceOrientationAndMotionAccess(WKPageRef, WKSecurityOriginRef origin, const void*)
{
    return TestController::singleton().handleDeviceOrientationAndMotionAccessRequest(origin);
}

// A placeholder to tell WebKit the client is WebKitTestRunner.
static void runWebAuthenticationPanel()
{
}

WKPageRef TestController::createOtherPage(WKPageRef, WKPageConfigurationRef configuration, WKNavigationActionRef navigationAction, WKWindowFeaturesRef windowFeatures, const void *clientInfo)
{
    PlatformWebView* parentView = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    return TestController::singleton().createOtherPage(parentView, configuration, navigationAction, windowFeatures);
}

WKPageRef TestController::createOtherPage(PlatformWebView* parentView, WKPageConfigurationRef configuration, WKNavigationActionRef navigationAction, WKWindowFeaturesRef windowFeatures)
{
    m_currentInvocation->willCreateNewPage();

    // The test needs to call testRunner.setCanOpenWindows() to open new windows.
    if (!m_currentInvocation->canOpenWindows())
        return nullptr;

    m_createdOtherPage = true;

    PlatformWebView* view = platformCreateOtherPage(parentView, configuration, parentView->options());
    WKPageRef newPage = view->page();

    view->resizeTo(800, 600);

    WKPageUIClientV8 otherPageUIClient = {
        { 8, view },
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        closeOtherPage,
        0, // takeFocus
        focus,
        unfocus,
        0, // runJavaScriptAlert_deprecatedForUseWithV0
        0, // runJavaScriptAlert_deprecatedForUseWithV0
        0, // runJavaScriptAlert_deprecatedForUseWithV0
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
        runOpenPanel,
        decidePolicyForGeolocationPermissionRequest,
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        printFrame,
        runModal,
        0, // didCompleteRubberBandForMainFrame
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        0, // createNewPage_deprecatedForUseWithV1
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
        0, // didClickAutofillButton
        0, // runJavaScriptAlert
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // mediaSessionMetadataDidChange
        createOtherPage,
        runJavaScriptAlert,
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        checkUserMediaPermissionForOrigin,
        0, // runBeforeUnloadConfirmPanel
        0, // fullscreenMayReturnToInline
        requestPointerLock,
        0,
    };
    WKPageSetPageUIClient(newPage, &otherPageUIClient.base);
    
    WKPageNavigationClientV3 pageNavigationClient = {
        { 3, &TestController::singleton() },
        decidePolicyForNavigationAction,
        decidePolicyForNavigationResponse,
        decidePolicyForPluginLoad,
        0, // didStartProvisionalNavigation
        didReceiveServerRedirectForProvisionalNavigation,
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
        didBeginNavigationGesture,
        willEndNavigationGesture,
        didEndNavigationGesture,
        didRemoveNavigationGestureSnapshot,
        0, // webProcessDidTerminate
        0, // contentRuleListNotification
        copySignedPublicKeyAndChallengeString
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
    AutodrainedPool pool;

    JSC::initializeThreading();
    RunLoop::initializeMainRunLoop();
    WTF::setProcessPrivileges(allPrivileges());

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
    m_allowedHosts = options.allowedHosts;
    m_shouldShowWebView = options.shouldShowWebView;
    m_shouldShowTouches = options.shouldShowTouches;
    m_checkForWorldLeaks = options.checkForWorldLeaks;
    m_allowAnyHTTPSCertificateForAllowedHosts = options.allowAnyHTTPSCertificateForAllowedHosts;
    m_internalFeatures = options.internalFeatures;
    m_experimentalFeatures = options.experimentalFeatures;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    m_accessibilityIsolatedTreeMode = options.accessibilityIsolatedTreeMode;
#endif
    m_usingServerMode = (m_paths.size() == 1 && m_paths[0] == "-");
    if (m_usingServerMode)
        m_printSeparators = true;
    else
        m_printSeparators = m_paths.size() > 1;

    initializeInjectedBundlePath();
    initializeTestPluginDirectory();

#if PLATFORM(MAC)
    WebCoreTestSupport::installMockGamepadProvider();
#endif

    WKRetainPtr<WKStringRef> pageGroupIdentifier = adoptWK(WKStringCreateWithUTF8CString("WebKitTestRunnerPageGroup"));
    m_pageGroup.adopt(WKPageGroupCreateWithIdentifier(pageGroupIdentifier.get()));

    m_eventSenderProxy = makeUnique<EventSenderProxy>(this);
}

WKRetainPtr<WKContextConfigurationRef> TestController::generateContextConfiguration(const TestOptions::ContextOptions& options) const
{
    auto configuration = adoptWK(WKContextConfigurationCreate());
    WKContextConfigurationSetInjectedBundlePath(configuration.get(), injectedBundlePath());
    WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(configuration.get(), true);
    WKContextConfigurationSetIgnoreSynchronousMessagingTimeoutsForTesting(configuration.get(), options.ignoreSynchronousMessagingTimeouts);

    WKRetainPtr<WKMutableArrayRef> overrideLanguages = adoptWK(WKMutableArrayCreate());
    for (auto& language : options.overrideLanguages)
        WKArrayAppendItem(overrideLanguages.get(), adoptWK(WKStringCreateWithUTF8CString(language.utf8().data())).get());
    WKContextConfigurationSetOverrideLanguages(configuration.get(), overrideLanguages.get());

    if (options.shouldEnableProcessSwapOnNavigation()) {
        WKContextConfigurationSetProcessSwapsOnNavigation(configuration.get(), true);
        if (options.enableProcessSwapOnWindowOpen)
            WKContextConfigurationSetProcessSwapsOnWindowOpenWithOpener(configuration.get(), true);
    }

    WKContextConfigurationSetShouldConfigureJSCForTesting(configuration.get(), true);

    return configuration;
}

void TestController::configureWebsiteDataStoreTemporaryDirectories(WKWebsiteDataStoreConfigurationRef configuration)
{
    if (const char* dumpRenderTreeTemp = libraryPathForTesting()) {
        String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);
        auto randomNumber = cryptographicallyRandomNumber();

        WKWebsiteDataStoreConfigurationSetApplicationCacheDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "ApplicationCache", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetNetworkCacheDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "Cache", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetCacheStorageDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "CacheStorage", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetIndexedDBDatabaseDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "Databases", pathSeparator, "IndexedDB", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetLocalStorageDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "LocalStorage", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetWebSQLDatabaseDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "Databases", pathSeparator, "WebSQL", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetMediaKeysStorageDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "MediaKeys", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetResourceLoadStatisticsDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "ResourceLoadStatistics", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetServiceWorkerRegistrationDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "ServiceWorkers", pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetPerOriginStorageQuota(configuration, 400 * 1024);
        WKWebsiteDataStoreConfigurationSetNetworkCacheSpeculativeValidationEnabled(configuration, true);
        WKWebsiteDataStoreConfigurationSetStaleWhileRevalidateEnabled(configuration, true);
        WKWebsiteDataStoreConfigurationSetTestingSessionEnabled(configuration, true);
    }
}

WKWebsiteDataStoreRef TestController::defaultWebsiteDataStore()
{
    static WKWebsiteDataStoreRef dataStore = nullptr;
    if (!dataStore) {
        auto configuration = adoptWK(WKWebsiteDataStoreConfigurationCreate());
        configureWebsiteDataStoreTemporaryDirectories(configuration.get());
        dataStore = WKWebsiteDataStoreCreateWithConfiguration(configuration.get());
    }
    return dataStore;
}

WKWebsiteDataStoreRef TestController::websiteDataStore()
{
    return m_websiteDataStore.get();
}

WKRetainPtr<WKPageConfigurationRef> TestController::generatePageConfiguration(const TestOptions& options)
{
    if (!m_context || !m_contextOptions->hasSameInitializationOptions(options.contextOptions)) {
        auto contextConfiguration = generateContextConfiguration(options.contextOptions);
        m_context = platformAdjustContext(adoptWK(WKContextCreateWithConfiguration(contextConfiguration.get())).get(), contextConfiguration.get());
        m_contextOptions = options.contextOptions;

        m_geolocationProvider = makeUnique<GeolocationProviderMock>(m_context.get());

        if (const char* dumpRenderTreeTemp = libraryPathForTesting()) {
            String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);

            // FIXME: This should be migrated to WKContextConfigurationRef.
            // Disable icon database to avoid fetching <http://127.0.0.1:8000/favicon.ico> and making tests flaky.
            // Invividual tests can enable it using testRunner.setIconDatabaseEnabled, although it's not currently supported in WebKitTestRunner.
            WKContextSetIconDatabasePath(m_context.get(), toWK(emptyString()).get());
        }

        WKContextUseTestingNetworkSession(m_context.get());
        WKContextSetCacheModel(m_context.get(), kWKCacheModelDocumentBrowser);

        platformInitializeContext();
    }

    WKContextInjectedBundleClientV2 injectedBundleClient = {
        { 2, this },
        didReceiveMessageFromInjectedBundle,
        nullptr,
        getInjectedBundleInitializationUserData,
        didReceiveSynchronousMessageFromInjectedBundleWithListener,
    };
    WKContextSetInjectedBundleClient(m_context.get(), &injectedBundleClient.base);

    WKContextClientV3 contextClient = {
        { 3, this },
        0, // plugInAutoStartOriginHashesChanged
        networkProcessDidCrash,
        0, // plugInInformationBecameAvailable
        0, // copyWebCryptoMasterKey
        serviceWorkerProcessDidCrash,
        gpuProcessDidCrash
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

    auto pageConfiguration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(pageConfiguration.get(), m_context.get());
    WKPageConfigurationSetPageGroup(pageConfiguration.get(), m_pageGroup.get());
    
    if (options.useEphemeralSession) {
        auto ephemeralDataStore = adoptWK(WKWebsiteDataStoreCreateNonPersistentDataStore());
        WKPageConfigurationSetWebsiteDataStore(pageConfiguration.get(), ephemeralDataStore.get());
    }

    m_userContentController = adoptWK(WKUserContentControllerCreate());
    WKPageConfigurationSetUserContentController(pageConfiguration.get(), userContentController());
    return pageConfiguration;
}

void TestController::createWebViewWithOptions(const TestOptions& options)
{
#if PLATFORM(COCOA)
    if (!options.applicationBundleIdentifier.isEmpty()) {
        // The bundle identifier can only be set once per test, and is cleared between tests.
        RELEASE_ASSERT(!m_hasSetApplicationBundleIdentifier);
        setApplicationBundleIdentifier(options.applicationBundleIdentifier);
        m_hasSetApplicationBundleIdentifier = true;
    }
#endif

    auto configuration = generatePageConfiguration(options);
    platformInitializeDataStore(configuration.get(), options);

    // Some preferences (notably mock scroll bars setting) currently cannot be re-applied to an existing view, so we need to set them now.
    // FIXME: Migrate these preferences to WKContextConfigurationRef.
    resetPreferencesToConsistentValues(options);

    platformCreateWebView(configuration.get(), options);
    WKPageUIClientV14 pageUIClient = {
        { 14, m_mainWebView.get() },
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        0, // close
        0, // takeFocus
        focus,
        unfocus,
        0, // runJavaScriptAlert_deprecatedForUseWithV0
        0, // runJavaScriptAlert_deprecatedForUseWithV0
        0, // runJavaScriptAlert_deprecatedForUseWithV0
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
        options.shouldHandleRunOpenPanel ? runOpenPanel : 0,
        decidePolicyForGeolocationPermissionRequest,
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        printFrame,
        runModal,
        0, // didCompleteRubberBandForMainFrame
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        0, // createNewPage_deprecatedForUseWithV1
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
        0, // didClickAutofillButton
        0, // runJavaScriptAlert
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // mediaSessionMetadataDidChange
        createOtherPage,
        runJavaScriptAlert,
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        checkUserMediaPermissionForOrigin,
        0, // runBeforeUnloadConfirmPanel
        0, // fullscreenMayReturnToInline
        requestPointerLock,
        0, // didLosePointerLock
        0, // handleAutoplayEvent
        0, // hasVideoInPictureInPictureDidChange
        0, // didExceedBackgroundResourceLimitWhileInForeground
        0, // didResignInputElementStrongPasswordAppearance
        0, // requestStorageAccessConfirm
        shouldAllowDeviceOrientationAndMotionAccess,
        runWebAuthenticationPanel
    };
    WKPageSetPageUIClient(m_mainWebView->page(), &pageUIClient.base);

    WKPageNavigationClientV3 pageNavigationClient = {
        { 3, this },
        decidePolicyForNavigationAction,
        decidePolicyForNavigationResponse,
        decidePolicyForPluginLoad,
        0, // didStartProvisionalNavigation
        didReceiveServerRedirectForProvisionalNavigation,
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
        didBeginNavigationGesture,
        willEndNavigationGesture,
        didEndNavigationGesture,
        didRemoveNavigationGestureSnapshot,
        0, // webProcessDidTerminate
        0, // contentRuleListNotification
        copySignedPublicKeyAndChallengeString
    };
    WKPageSetPageNavigationClient(m_mainWebView->page(), &pageNavigationClient.base);

    WKContextDownloadClientV1 downloadClient = {
        { 1, this },
        downloadDidStart,
        0, // didReceiveAuthenticationChallenge
        0, // didReceiveResponse
        0, // didReceiveData
        0, // shouldDecodeSourceDataOfMIMEType
        decideDestinationWithSuggestedFilename,
        0, // didCreateDestination
        downloadDidFinish,
        downloadDidFail,
        downloadDidCancel,
        0, // processDidCrash;
        downloadDidReceiveServerRedirectToURL
    };
    WKContextSetDownloadClient(context(), &downloadClient.base);
    
    // this should just be done on the page?
    WKPageInjectedBundleClientV1 injectedBundleClient = {
        { 1, this },
        didReceivePageMessageFromInjectedBundle,
        nullptr,
        didReceiveSynchronousPageMessageFromInjectedBundleWithListener,
    };
    WKPageSetPageInjectedBundleClient(m_mainWebView->page(), &injectedBundleClient.base);

    m_mainWebView->didInitializeClients();

    // Generally, the tests should default to running at 1x. updateWindowScaleForTest() will adjust the scale to
    // something else for specific tests that need to run at a different window scale.
    m_mainWebView->changeWindowScaleIfNeeded(1);
    
    if (!options.applicationBundleIdentifier.isEmpty()) {
        reinitializeAppBoundDomains();
        updateBundleIdentifierInNetworkProcess(options.applicationBundleIdentifier);
    }
}

void TestController::ensureViewSupportsOptionsForTest(const TestInvocation& test)
{
    auto options = test.options();

    if (m_mainWebView) {
        // Having created another page (via window.open()) prevents process swapping on navigation and it may therefore
        // cause flakiness to reuse the view.
        if (!m_createdOtherPage && m_mainWebView->viewSupportsOptions(options))
            return;

        willDestroyWebView();

        WKPageSetPageUIClient(m_mainWebView->page(), nullptr);
        WKPageSetPageNavigationClient(m_mainWebView->page(), nullptr);
        WKPageClose(m_mainWebView->page());

        m_mainWebView = nullptr;
        m_createdOtherPage = false;
    }


    createWebViewWithOptions(options);

    if (!resetStateToConsistentValues(options, ResetStage::BeforeTest))
        TestInvocation::dumpWebProcessUnresponsiveness("<unknown> - TestController::run - Failed to reset state to consistent values\n");
}

void TestController::resetPreferencesToConsistentValues(const TestOptions& options)
{
    // Reset preferences
    WKPreferencesRef preferences = platformPreferences();
    WKPreferencesResetTestRunnerOverrides(preferences);

    WKPreferencesEnableAllExperimentalFeatures(preferences);
    for (const auto& experimentalFeature : options.experimentalFeatures)
        WKPreferencesSetExperimentalFeatureForKey(preferences, experimentalFeature.value, toWK(experimentalFeature.key).get());

    WKPreferencesResetAllInternalDebugFeatures(preferences);

    // Set internal features that have different default values for testing.
    static WKStringRef asyncOverflowScrollingFeature = WKStringCreateWithUTF8CString("AsyncOverflowScrollingEnabled");
    WKPreferencesSetInternalDebugFeatureForKey(preferences, false, asyncOverflowScrollingFeature);

    static WKStringRef asyncFrameScrollingFeature = WKStringCreateWithUTF8CString("AsyncFrameScrollingEnabled");
    WKPreferencesSetInternalDebugFeatureForKey(preferences, false, asyncFrameScrollingFeature);

    for (const auto& internalDebugFeature : options.internalDebugFeatures)
        WKPreferencesSetInternalDebugFeatureForKey(preferences, internalDebugFeature.value, toWK(internalDebugFeature.key).get());

#if PLATFORM(COCOA)
    WKPreferencesSetCaptureVideoInUIProcessEnabled(preferences, options.enableCaptureVideoInUIProcess);
    WKPreferencesSetCaptureVideoInGPUProcessEnabled(preferences, options.enableCaptureVideoInGPUProcess);
    WKPreferencesSetCaptureAudioInGPUProcessEnabled(preferences, options.enableCaptureAudioInGPUProcess);
#endif
    WKPreferencesSetProcessSwapOnNavigationEnabled(preferences, options.contextOptions.shouldEnableProcessSwapOnNavigation());
    WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled(preferences, options.enableAppNap);
    WKPreferencesSetOfflineWebApplicationCacheEnabled(preferences, true);
    WKPreferencesSetSubpixelAntialiasedLayerTextEnabled(preferences, false);
    WKPreferencesSetXSSAuditorEnabled(preferences, false);
    WKPreferencesSetWebAudioEnabled(preferences, true);
    WKPreferencesSetMediaDevicesEnabled(preferences, true);
    WKPreferencesSetWebRTCMDNSICECandidatesEnabled(preferences, false);
    WKPreferencesSetDeveloperExtrasEnabled(preferences, true);
    WKPreferencesSetJavaScriptRuntimeFlags(preferences, kWKJavaScriptRuntimeFlagsAllEnabled);
    WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(preferences, true);
    WKPreferencesSetJavaScriptCanAccessClipboard(preferences, true);
    WKPreferencesSetDOMPasteAllowed(preferences, options.domPasteAllowed);
    WKPreferencesSetUniversalAccessFromFileURLsAllowed(preferences, true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences, true);
    WKPreferencesSetTopNavigationToDataURLsAllowed(preferences, options.allowTopNavigationToDataURLs);
#if ENABLE(FULLSCREEN_API)
    WKPreferencesSetFullScreenEnabled(preferences, true);
#endif
    WKPreferencesSetAsynchronousPluginInitializationEnabled(preferences, false);
    WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(preferences, false);
    WKPreferencesSetArtificialPluginInitializationDelayEnabled(preferences, false);
    WKPreferencesSetTabToLinksEnabled(preferences, false);
    WKPreferencesSetInteractiveFormValidationEnabled(preferences, true);
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetCustomPasteboardDataEnabled(preferences, true);
    WKPreferencesSetDialogElementEnabled(preferences, true);

    WKPreferencesSetMockScrollbarsEnabled(preferences, options.useMockScrollbars);
    WKPreferencesSetNeedsSiteSpecificQuirks(preferences, options.needsSiteSpecificQuirks);
    WKPreferencesSetAttachmentElementEnabled(preferences, options.enableAttachmentElement);
    WKPreferencesSetMenuItemElementEnabled(preferences, options.enableMenuItemElement);
    WKPreferencesSetKeygenElementEnabled(preferences, options.enableKeygenElement);
    WKPreferencesSetModernMediaControlsEnabled(preferences, options.enableModernMediaControls);
    WKPreferencesSetWebAuthenticationEnabled(preferences, options.enableWebAuthentication);
    WKPreferencesSetWebAuthenticationLocalAuthenticatorEnabled(preferences, options.enableWebAuthenticationLocalAuthenticator);
    WKPreferencesSetIsSecureContextAttributeEnabled(preferences, options.enableIsSecureContextAttribute);
    WKPreferencesSetAllowCrossOriginSubresourcesToAskForCredentials(preferences, options.allowCrossOriginSubresourcesToAskForCredentials);
    WKPreferencesSetColorFilterEnabled(preferences, options.enableColorFilter);
    WKPreferencesSetPunchOutWhiteBackgroundsInDarkMode(preferences, options.punchOutWhiteBackgroundsInDarkMode);
    WKPreferencesSetPageCacheEnabled(preferences, options.enableBackForwardCache);
    WKPreferencesSetLazyImageLoadingEnabled(preferences, options.enableLazyImageLoading);

    static WKStringRef defaultTextEncoding = WKStringCreateWithUTF8CString("ISO-8859-1");
    WKPreferencesSetDefaultTextEncodingName(preferences, defaultTextEncoding);

    static WKStringRef standardFontFamily = WKStringCreateWithUTF8CString("Times");
    static WKStringRef cursiveFontFamily = WKStringCreateWithUTF8CString("Apple Chancery");
    static WKStringRef fantasyFontFamily = WKStringCreateWithUTF8CString("Papyrus");
    static WKStringRef fixedFontFamily = WKStringCreateWithUTF8CString("Courier");
    static WKStringRef pictographFontFamily = WKStringCreateWithUTF8CString("Apple Color Emoji");
    static WKStringRef sansSerifFontFamily = WKStringCreateWithUTF8CString("Helvetica");
    static WKStringRef serifFontFamily = WKStringCreateWithUTF8CString("Times");

    WKPreferencesSetMinimumFontSize(preferences, 0);
    WKPreferencesSetStandardFontFamily(preferences, standardFontFamily);
    WKPreferencesSetCursiveFontFamily(preferences, cursiveFontFamily);
    WKPreferencesSetFantasyFontFamily(preferences, fantasyFontFamily);
    WKPreferencesSetFixedFontFamily(preferences, fixedFontFamily);
    WKPreferencesSetPictographFontFamily(preferences, pictographFontFamily);
    WKPreferencesSetSansSerifFontFamily(preferences, sansSerifFontFamily);
    WKPreferencesSetSerifFontFamily(preferences, serifFontFamily);
    WKPreferencesSetAsynchronousSpellCheckingEnabled(preferences, false);
#if ENABLE(MEDIA_SOURCE)
    WKPreferencesSetMediaSourceEnabled(preferences, true);
    WKPreferencesSetSourceBufferChangeTypeEnabled(preferences, true);
#endif
    WKPreferencesSetHighlightAPIEnabled(preferences, true);

    WKPreferencesSetHiddenPageDOMTimerThrottlingEnabled(preferences, false);
    WKPreferencesSetHiddenPageCSSAnimationSuspensionEnabled(preferences, false);

    WKPreferencesSetAcceleratedDrawingEnabled(preferences, m_shouldUseAcceleratedDrawing || options.useAcceleratedDrawing);
    // FIXME: We should be testing the default.
    WKPreferencesSetStorageBlockingPolicy(preferences, kWKAllowAllStorage);

    WKPreferencesSetIsNSURLSessionWebSocketEnabled(preferences, false);

    WKPreferencesSetFetchAPIKeepAliveEnabled(preferences, true);
    WKPreferencesSetResourceTimingEnabled(preferences, true);
    WKPreferencesSetUserTimingEnabled(preferences, true);
    WKPreferencesSetMediaPreloadingEnabled(preferences, true);
    WKPreferencesSetMediaPlaybackAllowsInline(preferences, true);
    WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute(preferences, false);
    WKPreferencesSetRemotePlaybackEnabled(preferences, true);
    WKPreferencesSetBeaconAPIEnabled(preferences, true);
    WKPreferencesSetDirectoryUploadEnabled(preferences, true);

    WKHTTPCookieStoreDeleteAllCookies(WKWebsiteDataStoreGetHTTPCookieStore(websiteDataStore()), nullptr, nullptr);

    WKPreferencesSetMockCaptureDevicesEnabled(preferences, true);
    
    WKPreferencesSetLargeImageAsyncDecodingEnabled(preferences, false);

    WKPreferencesSetInspectorAdditionsEnabled(preferences, options.enableInspectorAdditions);

    WKPreferencesSetStorageAccessAPIEnabled(preferences, true);
    
    WKPreferencesSetAccessibilityObjectModelEnabled(preferences, true);
    WKPreferencesSetCSSOMViewScrollingAPIEnabled(preferences, true);
    WKPreferencesSetMediaCapabilitiesEnabled(preferences, true);

    WKPreferencesSetRestrictedHTTPResponseAccess(preferences, true);

    WKPreferencesSetServerTimingEnabled(preferences, true);

    WKPreferencesSetWebSQLDisabled(preferences, false);

    WKPreferencesSetMediaPlaybackRequiresUserGesture(preferences, false);
    WKPreferencesSetVideoPlaybackRequiresUserGesture(preferences, false);
    WKPreferencesSetAudioPlaybackRequiresUserGesture(preferences, false);

    WKPreferencesSetShouldUseServiceWorkerShortTimeout(preferences, options.contextOptions.useServiceWorkerShortTimeout);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    WKPreferencesSetIsAccessibilityIsolatedTreeEnabled(preferences, accessibilityIsolatedTreeMode());
#endif
    
    platformResetPreferencesToConsistentValues();
}

bool TestController::resetStateToConsistentValues(const TestOptions& options, ResetStage resetStage)
{
    SetForScope<State> changeState(m_state, Resetting);
    m_beforeUnloadReturnValue = true;

    // This setting differs between the antique and modern Mac WebKit2 API.
    // For now, maintain the antique behavior, because some tests depend on it!
    // FIXME: We should be testing the default.
    WKPageSetBackgroundExtendsBeyondPage(m_mainWebView->page(), false);

    WKPageSetCustomUserAgent(m_mainWebView->page(), nullptr);

    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("Reset"));
    WKRetainPtr<WKMutableDictionaryRef> resetMessageBody = adoptWK(WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> shouldGCKey = adoptWK(WKStringCreateWithUTF8CString("ShouldGC"));
    WKRetainPtr<WKBooleanRef> shouldGCValue = adoptWK(WKBooleanCreate(m_gcBetweenTests));
    WKDictionarySetItem(resetMessageBody.get(), shouldGCKey.get(), shouldGCValue.get());

    WKRetainPtr<WKStringRef> allowedHostsKey = adoptWK(WKStringCreateWithUTF8CString("AllowedHosts"));
    WKRetainPtr<WKMutableArrayRef> allowedHostsValue = adoptWK(WKMutableArrayCreate());
    for (auto& host : m_allowedHosts) {
        WKRetainPtr<WKStringRef> wkHost = adoptWK(WKStringCreateWithUTF8CString(host.c_str()));
        WKArrayAppendItem(allowedHostsValue.get(), wkHost.get());
    }
    WKDictionarySetItem(resetMessageBody.get(), allowedHostsKey.get(), allowedHostsValue.get());

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    WKRetainPtr<WKStringRef> axIsolatedModeKey = adoptWK(WKStringCreateWithUTF8CString("AccessibilityIsolatedTree"));
    WKRetainPtr<WKBooleanRef> axIsolatedModeValue = adoptWK(WKBooleanCreate(m_accessibilityIsolatedTreeMode));
    WKDictionarySetItem(resetMessageBody.get(), axIsolatedModeKey.get(), axIsolatedModeValue.get());
#endif
    
    if (options.jscOptions.length()) {
        WKRetainPtr<WKStringRef> jscOptionsKey = adoptWK(WKStringCreateWithUTF8CString("JSCOptions"));
        WKRetainPtr<WKStringRef> jscOptionsValue = adoptWK(WKStringCreateWithUTF8CString(options.jscOptions.c_str()));
        WKDictionarySetItem(resetMessageBody.get(), jscOptionsKey.get(), jscOptionsValue.get());
    }

#if PLATFORM(COCOA)
    WebCoreTestSupport::setAdditionalSupportedImageTypesForTesting(options.additionalSupportedImageTypes.c_str());
#endif

    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), resetMessageBody.get());

    WKContextSetShouldUseFontSmoothing(TestController::singleton().context(), false);

    WKContextSetCacheModel(TestController::singleton().context(), kWKCacheModelDocumentBrowser);

    WKContextClearCachedCredentials(TestController::singleton().context());

    WKContextResetServiceWorkerFetchTimeoutForTesting(TestController::singleton().context());

    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(websiteDataStore(), true);
    WKContextSetStorageAccessAPIEnabled(platformContext(), true);
    WKWebsiteDataStoreClearAllDeviceOrientationPermissions(websiteDataStore());

    clearIndexedDatabases();
    clearLocalStorage();

    clearServiceWorkerRegistrations();
    clearDOMCaches();

    resetQuota();

    WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(platformContext(), true);

    WKContextClearCurrentModifierStateForTesting(TestController::singleton().context());

    WKContextSetUseSeparateServiceWorkerProcess(TestController::singleton().context(), false);

    WKPageSetMockCameraOrientation(m_mainWebView->page(), 0);
    resetMockMediaDevices();

    // FIXME: This function should also ensure that there is only one page open.

    // Reset the EventSender for each test.
    m_eventSenderProxy = makeUnique<EventSenderProxy>(this);

    // FIXME: Is this needed? Nothing in TestController changes preferences during tests, and if there is
    // some other code doing this, it should probably be responsible for cleanup too.
    resetPreferencesToConsistentValues(options);

#if PLATFORM(GTK)
    WKTextCheckerContinuousSpellCheckingEnabledStateChanged(true);
#endif

    // Make sure the view is in the window (a test can unparent it).
    m_mainWebView->addToWindow();

    // In the case that a test using the chrome input field failed, be sure to clean up for the next test.
    m_mainWebView->removeChromeInputField();
    m_mainWebView->focus();

    // Re-set to the default backing scale factor by setting the custom scale factor to 0.
    WKPageSetCustomBackingScaleFactor(m_mainWebView->page(), 0);

    WKPageClearWheelEventTestMonitor(m_mainWebView->page());

    WKPageSetMuted(m_mainWebView->page(), true);

    WKPageClearUserMediaState(m_mainWebView->page());

    // Reset notification permissions
    m_webNotificationProvider.reset();

    // Reset Geolocation permissions.
    m_geolocationPermissionRequests.clear();
    m_isGeolocationPermissionSet = false;
    m_isGeolocationPermissionAllowed = false;

    // Reset UserMedia permissions.
    m_userMediaPermissionRequests.clear();
    m_cachedUserMediaPermissions.clear();
    setUserMediaPermission(true);

    // Reset Custom Policy Delegate.
    setCustomPolicyDelegate(false, false);

    // Reset Content Extensions.
    resetContentExtensions();

    m_shouldDownloadUndisplayableMIMETypes = false;

    m_shouldAllowDeviceOrientationAndMotionAccess = false;

    m_workQueueManager.clearWorkQueue();

    m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges = false;
    m_handlesAuthenticationChallenges = false;
    m_authenticationUsername = String();
    m_authenticationPassword = String();

    setBlockAllPlugins(false);
    setPluginSupportedMode({ });

    m_shouldLogDownloadCallbacks = false;
    m_shouldLogHistoryClientCallbacks = false;
    m_shouldLogCanAuthenticateAgainstProtectionSpace = false;

    setHidden(false);

    if (!platformResetStateToConsistentValues(options))
        return false;

    m_shouldDecideNavigationPolicyAfterDelay = false;
    m_shouldDecideResponsePolicyAfterDelay = false;

    setNavigationGesturesEnabled(false);
    
    setIgnoresViewportScaleLimits(options.ignoresViewportScaleLimits);

    m_openPanelFileURLs = nullptr;
#if PLATFORM(IOS_FAMILY)
    m_openPanelFileURLsMediaIcon = nullptr;
#endif

    setAllowsAnySSLCertificate(true);

    statisticsResetToConsistentState();
    clearLoadedThirdPartyDomains();
    clearAppBoundSession();
    clearAdClickAttribution();

    m_didReceiveServerRedirectForProvisionalNavigation = false;
    m_serverTrustEvaluationCallbackCallsCount = 0;
    m_shouldDismissJavaScriptAlertsAsynchronously = false;

    auto loadAboutBlank = [this] {
        m_doneResetting = false;
        WKPageLoadURL(m_mainWebView->page(), blankURL());
        runUntil(m_doneResetting, m_currentInvocation->shortTimeout());
        return m_doneResetting;
    };

    // Reset main page back to about:blank
    if (!loadAboutBlank()) {
        WTFLogAlways("Failed to load 'about:blank', terminating process and trying again.");
        WKPageTerminate(m_mainWebView->page());
        if (!loadAboutBlank()) {
            WTFLogAlways("Failed to load 'about:blank' again after termination.");
            return false;
        }
    }
    
    if (resetStage == ResetStage::AfterTest) {
        updateLiveDocumentsAfterTest();
#if PLATFORM(COCOA)
        clearApplicationBundleIdentifierTestingOverride();
#endif
        clearBundleIdentifierInNetworkProcess();
    }

    return m_doneResetting;
}

void TestController::updateLiveDocumentsAfterTest()
{
    if (!m_checkForWorldLeaks)
        return;

    AsyncTask([]() {
        // After each test, we update the list of live documents so that we can detect when an abandoned document first showed up.
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("GetLiveDocuments"));
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
    }, 5_s).run();
}

void TestController::checkForWorldLeaks()
{
    if (!m_checkForWorldLeaks || !TestController::singleton().mainWebView())
        return;

    AsyncTask([]() {
        // This runs at the end of a series of tests. It clears caches, runs a GC and then fetches the list of documents.
        WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("CheckForWorldLeaks"));
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), messageName.get(), nullptr);
    }, 20_s).run();
}

void TestController::dumpResponse(const String& result)
{
    unsigned resultLength = result.length();
    printf("Content-Type: text/plain\n");
    printf("Content-Length: %u\n", resultLength);
    fwrite(result.utf8().data(), 1, resultLength, stdout);
    printf("#EOF\n");
    fprintf(stderr, "#EOF\n");
    fflush(stdout);
    fflush(stderr);
}

void TestController::findAndDumpWebKitProcessIdentifiers()
{
#if PLATFORM(COCOA)
    dumpResponse(makeString(TestController::webProcessName(), ": ",
        WKPageGetProcessIdentifier(TestController::singleton().mainWebView()->page()), '\n',
        TestController::networkProcessName(), ": ",
        WKContextGetNetworkProcessIdentifier(m_context.get()), '\n'));
#else
    dumpResponse("\n"_s);
#endif
}

void TestController::findAndDumpWorldLeaks()
{
    if (!m_checkForWorldLeaks)
        return;

    checkForWorldLeaks();

    StringBuilder builder;
    
    if (m_abandonedDocumentInfo.size()) {
        for (const auto& it : m_abandonedDocumentInfo) {
            auto documentURL = it.value.abandonedDocumentURL;
            if (documentURL.isEmpty())
                documentURL = "(no url)";
            builder.append("TEST: ");
            builder.append(it.value.testURL);
            builder.append('\n');
            builder.append("ABANDONED DOCUMENT: ");
            builder.append(documentURL);
            builder.append('\n');
        }
    } else
        builder.append("no abandoned documents\n");

    dumpResponse(builder.toString());
}

void TestController::willDestroyWebView()
{
    // Before we kill the web view, look for abandoned documents before that web process goes away.
    checkForWorldLeaks();
}

void TestController::terminateWebContentProcess()
{
    WKPageTerminate(m_mainWebView->page());
}

void TestController::reattachPageToWebProcess()
{
    // Loading a web page is the only way to reattach an existing page to a process.
    SetForScope<State> changeState(m_state, Resetting);
    m_doneResetting = false;
    WKPageLoadURL(m_mainWebView->page(), blankURL());
    runUntil(m_doneResetting, noTimeout);
}

const char* TestController::webProcessName()
{
    // FIXME: Find a way to not hardcode the process name.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    return "com.apple.WebKit.WebContent";
#elif PLATFORM(COCOA)
    return "com.apple.WebKit.WebContent.Development";
#elif PLATFORM(GTK)
    return "WebKitWebProcess";
#elif PLATFORM(WPE)
    return "WPEWebProcess";
#else
    return "WebProcess";
#endif
}

const char* TestController::networkProcessName()
{
    // FIXME: Find a way to not hardcode the process name.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    return "com.apple.WebKit.Networking";
#elif PLATFORM(COCOA)
    return "com.apple.WebKit.Networking.Development";
#elif PLATFORM(GTK)
    return "WebKitNetworkProcess";
#elif PLATFORM(WPE)
    return "WPENetworkProcess";
#else
    return "NetworkProcess";
#endif
}

#if !PLATFORM(COCOA)
void TestController::setAllowsAnySSLCertificate(bool allows)
{
    m_allowsAnySSLCertificate = allows;
    WKContextSetAllowsAnySSLCertificateForWebSocketTesting(platformContext(), allows);
}
#endif

static std::string testPath(WKURLRef url)
{
    auto scheme = adoptWK(WKURLCopyScheme(url));
    if (WKStringIsEqualToUTF8CStringIgnoringCase(scheme.get(), "file")) {
        auto path = adoptWK(WKURLCopyPath(url));
        auto buffer = std::vector<char>(WKStringGetMaximumUTF8CStringSize(path.get()));
        auto length = WKStringGetUTF8CString(path.get(), buffer.data(), buffer.size());
        RELEASE_ASSERT(length > 0);
#if OS(WINDOWS)
        // Remove the first '/' if it starts with something like "/C:/".
        if (length >= 4 && buffer[0] == '/' && buffer[2] == ':' && buffer[3] == '/')
            return std::string(buffer.data() + 1, length - 1);
#endif
        return std::string(buffer.data(), length - 1);
    }
    return std::string();
}

static WKURLRef createTestURL(const char* pathOrURL)
{
    if (strstr(pathOrURL, "http://") || strstr(pathOrURL, "https://") || strstr(pathOrURL, "file://"))
        return WKURLCreateWithUTF8CString(pathOrURL);

    // Creating from filesytem path.
    size_t length = strlen(pathOrURL);
    if (!length)
        return 0;

#if PLATFORM(WIN)
    bool isAbsolutePath = false;
    if (strlen(pathOrURL) >= 3 && pathOrURL[1] == ':' && pathOrURL[2] == pathSeparator)
        isAbsolutePath = true;
#else
    bool isAbsolutePath = pathOrURL[0] == pathSeparator;
#endif
    const char* filePrefix = "file://";
    static const size_t prefixLength = strlen(filePrefix);

    UniqueArray<char> buffer;
    if (isAbsolutePath) {
        buffer = makeUniqueArray<char>(prefixLength + length + 1);
        strcpy(buffer.get(), filePrefix);
        strcpy(buffer.get() + prefixLength, pathOrURL);
    } else {
        buffer = makeUniqueArray<char>(prefixLength + PATH_MAX + length + 2); // 1 for the pathSeparator
        strcpy(buffer.get(), filePrefix);
        if (!getcwd(buffer.get() + prefixLength, PATH_MAX))
            return 0;
        size_t numCharacters = strlen(buffer.get());
        buffer[numCharacters] = pathSeparator;
        strcpy(buffer.get() + numCharacters + 1, pathOrURL);
    }

    return WKURLCreateWithUTF8CString(buffer.get());
}

static bool parseBooleanTestHeaderValue(const std::string& value)
{
    if (value == "true")
        return true;
    if (value == "false")
        return false;

    LOG_ERROR("Found unexpected value '%s' for boolean option. Expected 'true' or 'false'.", value.c_str());
    return false;
}

static std::string parseStringTestHeaderValueAsRelativePath(const std::string& value, const std::string& pathOrURL)
{
    WKRetainPtr<WKURLRef> baseURL = adoptWK(createTestURL(pathOrURL.c_str()));
    WKRetainPtr<WKURLRef> relativeURL = adoptWK(WKURLCreateWithBaseURL(baseURL.get(), value.c_str()));
    return toSTD(adoptWK(WKURLCopyPath(relativeURL.get())));
}

static std::string parseStringTestHeaderValueAsURL(const std::string& value)
{
    return toSTD(adoptWK(WKURLCopyString(createTestURL(value.c_str()))));
}

static void updateTestOptionsFromTestHeader(TestOptions& testOptions, const std::string& pathOrURL, const std::string& absolutePath)
{
    std::string filename = absolutePath;
    if (filename.empty()) {
        // Gross. Need to reduce conversions between all the string types and URLs.
        WKRetainPtr<WKURLRef> wkURL = adoptWK(createTestURL(pathOrURL.c_str()));
        filename = testPath(wkURL.get());
    }

    if (filename.empty())
        return;

    std::string options;
    std::ifstream testFile(filename.data());
    if (!testFile.good())
        return;
    getline(testFile, options);
    std::string beginString("webkit-test-runner [ ");
    std::string endString(" ]");
    size_t beginLocation = options.find(beginString);
    if (beginLocation == std::string::npos)
        return;
    size_t endLocation = options.find(endString, beginLocation);
    if (endLocation == std::string::npos) {
        LOG_ERROR("Could not find end of test header in %s", filename.c_str());
        return;
    }
    std::string pairString = options.substr(beginLocation + beginString.size(), endLocation - (beginLocation + beginString.size()));
    size_t pairStart = 0;
    while (pairStart < pairString.size()) {
        size_t pairEnd = pairString.find(" ", pairStart);
        if (pairEnd == std::string::npos)
            pairEnd = pairString.size();
        size_t equalsLocation = pairString.find("=", pairStart);
        if (equalsLocation == std::string::npos) {
            LOG_ERROR("Malformed option in test header (could not find '=' character) in %s", filename.c_str());
            break;
        }
        auto key = pairString.substr(pairStart, equalsLocation - pairStart);
        auto value = pairString.substr(equalsLocation + 1, pairEnd - (equalsLocation + 1));

        if (!key.rfind("experimental:")) {
            key = key.substr(13);
            testOptions.experimentalFeatures.add(String(key.c_str()), parseBooleanTestHeaderValue(value));
        }

        if (!key.rfind("internal:")) {
            key = key.substr(9);
            testOptions.internalDebugFeatures.add(String(key.c_str()), parseBooleanTestHeaderValue(value));
        }

        if (key == "language")
            testOptions.contextOptions.overrideLanguages = String(value.c_str()).split(',');
        else if (key == "useThreadedScrolling")
            testOptions.useThreadedScrolling = parseBooleanTestHeaderValue(value);
        else if (key == "useAcceleratedDrawing")
            testOptions.useAcceleratedDrawing = parseBooleanTestHeaderValue(value);
        else if (key == "useFlexibleViewport")
            testOptions.useFlexibleViewport = parseBooleanTestHeaderValue(value);
        else if (key == "useDataDetection")
            testOptions.useDataDetection = parseBooleanTestHeaderValue(value);
        else if (key == "useMockScrollbars")
            testOptions.useMockScrollbars = parseBooleanTestHeaderValue(value);
        else if (key == "needsSiteSpecificQuirks")
            testOptions.needsSiteSpecificQuirks = parseBooleanTestHeaderValue(value);
        else if (key == "ignoresViewportScaleLimits")
            testOptions.ignoresViewportScaleLimits = parseBooleanTestHeaderValue(value);
        else if (key == "useCharacterSelectionGranularity")
            testOptions.useCharacterSelectionGranularity = parseBooleanTestHeaderValue(value);
        else if (key == "enableAttachmentElement")
            testOptions.enableAttachmentElement = parseBooleanTestHeaderValue(value);
        else if (key == "enableIntersectionObserver")
            testOptions.enableIntersectionObserver = parseBooleanTestHeaderValue(value);
        else if (key == "useEphemeralSession")
            testOptions.useEphemeralSession = parseBooleanTestHeaderValue(value);
        else if (key == "enableMenuItemElement")
            testOptions.enableMenuItemElement = parseBooleanTestHeaderValue(value);
        else if (key == "enableKeygenElement")
            testOptions.enableKeygenElement = parseBooleanTestHeaderValue(value);
        else if (key == "enableModernMediaControls")
            testOptions.enableModernMediaControls = parseBooleanTestHeaderValue(value);
        else if (key == "enablePointerLock")
            testOptions.enablePointerLock = parseBooleanTestHeaderValue(value);
        else if (key == "enableWebAuthentication")
            testOptions.enableWebAuthentication = parseBooleanTestHeaderValue(value);
        else if (key == "enableWebAuthenticationLocalAuthenticator")
            testOptions.enableWebAuthenticationLocalAuthenticator = parseBooleanTestHeaderValue(value);
        else if (key == "enableIsSecureContextAttribute")
            testOptions.enableIsSecureContextAttribute = parseBooleanTestHeaderValue(value);
        else if (key == "enableInspectorAdditions")
            testOptions.enableInspectorAdditions = parseBooleanTestHeaderValue(value);
        else if (key == "dumpJSConsoleLogInStdErr")
            testOptions.dumpJSConsoleLogInStdErr = parseBooleanTestHeaderValue(value);
        else if (key == "applicationManifest")
            testOptions.applicationManifest = parseStringTestHeaderValueAsRelativePath(value, pathOrURL);
        else if (key == "allowCrossOriginSubresourcesToAskForCredentials")
            testOptions.allowCrossOriginSubresourcesToAskForCredentials = parseBooleanTestHeaderValue(value);
        else if (key == "domPasteAllowed")
            testOptions.domPasteAllowed = parseBooleanTestHeaderValue(value);
        else if (key == "enableProcessSwapOnNavigation")
            testOptions.contextOptions.enableProcessSwapOnNavigation = parseBooleanTestHeaderValue(value);
        else if (key == "enableProcessSwapOnWindowOpen")
            testOptions.contextOptions.enableProcessSwapOnWindowOpen = parseBooleanTestHeaderValue(value);
        else if (key == "useServiceWorkerShortTimeout")
            testOptions.contextOptions.useServiceWorkerShortTimeout = parseBooleanTestHeaderValue(value);
        else if (key == "enableColorFilter")
            testOptions.enableColorFilter = parseBooleanTestHeaderValue(value);
        else if (key == "punchOutWhiteBackgroundsInDarkMode")
            testOptions.punchOutWhiteBackgroundsInDarkMode = parseBooleanTestHeaderValue(value);
        else if (key == "jscOptions")
            testOptions.jscOptions = value;
        else if (key == "additionalSupportedImageTypes")
            testOptions.additionalSupportedImageTypes = value;
        else if (key == "runSingly")
            testOptions.runSingly = parseBooleanTestHeaderValue(value);
        else if (key == "shouldIgnoreMetaViewport")
            testOptions.shouldIgnoreMetaViewport = parseBooleanTestHeaderValue(value);
        else if (key == "spellCheckingDots")
            testOptions.shouldShowSpellCheckingDots = parseBooleanTestHeaderValue(value);
        else if (key == "enableServiceControls")
            testOptions.enableServiceControls = parseBooleanTestHeaderValue(value);
        else if (key == "enableEditableImages")
            testOptions.enableEditableImages = parseBooleanTestHeaderValue(value);
        else if (key == "editable")
            testOptions.editable = parseBooleanTestHeaderValue(value);
        else if (key == "shouldHandleRunOpenPanel")
            testOptions.shouldHandleRunOpenPanel = parseBooleanTestHeaderValue(value);
        else if (key == "shouldPresentPopovers")
            testOptions.shouldPresentPopovers = parseBooleanTestHeaderValue(value);
        else if (key == "contentInset.top")
            testOptions.contentInsetTop = std::stod(value);
        else if (key == "ignoreSynchronousMessagingTimeouts")
            testOptions.contextOptions.ignoreSynchronousMessagingTimeouts = parseBooleanTestHeaderValue(value);
        else if (key == "contentMode")
            testOptions.contentMode = { value.c_str() };
        else if (key == "applicationBundleIdentifier")
            testOptions.applicationBundleIdentifier = { value.c_str() };
        else if (key == "enableAppNap")
            testOptions.enableAppNap = parseBooleanTestHeaderValue(value);
        else if (key == "enableBackForwardCache")
            testOptions.enableBackForwardCache = parseBooleanTestHeaderValue(value);
        else if (key == "enableLazyImageLoading")
            testOptions.enableLazyImageLoading = parseBooleanTestHeaderValue(value);
        else if (key == "allowsLinkPreview")
            testOptions.allowsLinkPreview = parseBooleanTestHeaderValue(value);
        else if (key == "enableCaptureVideoInUIProcess")
            testOptions.enableCaptureVideoInUIProcess = parseBooleanTestHeaderValue(value);
        else if (key == "enableCaptureVideoInGPUProcess")
            testOptions.enableCaptureVideoInGPUProcess = parseBooleanTestHeaderValue(value);
        else if (key == "enableCaptureAudioInGPUProcess")
            testOptions.enableCaptureAudioInGPUProcess = parseBooleanTestHeaderValue(value);
        else if (key == "allowTopNavigationToDataURLs")
            testOptions.allowTopNavigationToDataURLs = parseBooleanTestHeaderValue(value);
        else if (key == "enableInAppBrowserPrivacy")
            testOptions.enableInAppBrowserPrivacy = parseBooleanTestHeaderValue(value);
        else if (key == "standaloneWebApplicationURL")
            testOptions.standaloneWebApplicationURL = parseStringTestHeaderValueAsURL(value);
        else if (key == "isAppBoundWebView")
            testOptions.isAppBoundWebView = parseBooleanTestHeaderValue(value);
        pairStart = pairEnd + 1;
    }
}

TestOptions TestController::testOptionsForTest(const TestCommand& command) const
{
    TestOptions options(command.pathOrURL);

    options.useRemoteLayerTree = m_shouldUseRemoteLayerTree;
    options.shouldShowWebView = m_shouldShowWebView;

    for (auto& feature : m_internalFeatures)
        options.internalDebugFeatures.add(feature.key, feature.value);
    for (auto& feature : m_experimentalFeatures)
        options.experimentalFeatures.add(feature.key, feature.value);

    updatePlatformSpecificTestOptionsForTest(options, command.pathOrURL);
    updateTestOptionsFromTestHeader(options, command.pathOrURL, command.absolutePath);
    platformAddTestOptions(options);

    return options;
}

void TestController::updateWebViewSizeForTest(const TestInvocation& test)
{
    unsigned width = viewWidth;
    unsigned height = viewHeight;
    if (test.options().isSVGTest) {
        width = w3cSVGViewWidth;
        height = w3cSVGViewHeight;
    }

    mainWebView()->resizeTo(width, height);
}

void TestController::updateWindowScaleForTest(PlatformWebView* view, const TestInvocation& test)
{
    view->changeWindowScaleIfNeeded(test.options().deviceScaleFactor);
}

void TestController::configureViewForTest(const TestInvocation& test)
{
    ensureViewSupportsOptionsForTest(test);
    updateWebViewSizeForTest(test);
    updateWindowScaleForTest(mainWebView(), test);
    configureContentExtensionForTest(test);
    platformConfigureViewForTest(test);
}

#if ENABLE(CONTENT_EXTENSIONS) && !PLATFORM(COCOA)
struct ContentExtensionStoreCallbackContext {
    explicit ContentExtensionStoreCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    uint32_t status { kWKUserContentExtensionStoreSuccess };
    WKRetainPtr<WKUserContentFilterRef> filter;
    bool done { false };
};

static void contentExtensionStoreCallback(WKUserContentFilterRef filter, uint32_t status, void* userData)
{
    auto* context = static_cast<ContentExtensionStoreCallbackContext*>(userData);
    context->status = status;
    context->filter = filter ? adoptWK(filter) : nullptr;
    context->done = true;
    context->testController.notifyDone();
}

static std::string contentExtensionJSONPath(WKURLRef url)
{
    auto path = testPath(url);
    if (path.length())
        return path + ".json";

    auto p = adoptWK(WKURLCopyPath(url));
    auto buffer = std::vector<char>(WKStringGetMaximumUTF8CStringSize(p.get()));
    const auto length = WKStringGetUTF8CString(p.get(), buffer.data(), buffer.size());
    return std::string("LayoutTests/http/tests") + std::string(buffer.data(), length - 1) + ".json";
}
#endif

#if !PLATFORM(COCOA)
#if ENABLE(CONTENT_EXTENSIONS)
void TestController::configureContentExtensionForTest(const TestInvocation& test)
{
    const char* contentExtensionsPath = libraryPathForTesting();
    if (!contentExtensionsPath)
        contentExtensionsPath = "/tmp/wktr-contentextensions";

    if (!test.urlContains("contentextensions/")) {
        WKPageSetUserContentExtensionsEnabled(m_mainWebView->page(), false);
        return;
    }

    std::string jsonFilePath(contentExtensionJSONPath(test.url()));
    std::ifstream jsonFile(jsonFilePath);
    if (!jsonFile.good()) {
        WTFLogAlways("Could not open file '%s'", jsonFilePath.c_str());
        return;
    }

    std::string jsonFileContents {std::istreambuf_iterator<char>(jsonFile), std::istreambuf_iterator<char>()};
    auto jsonSource = adoptWK(WKStringCreateWithUTF8CString(jsonFileContents.c_str()));

    auto storePath = adoptWK(WKStringCreateWithUTF8CString(contentExtensionsPath));
    auto extensionStore = adoptWK(WKUserContentExtensionStoreCreate(storePath.get()));
    ASSERT(extensionStore);

    auto filterIdentifier = adoptWK(WKStringCreateWithUTF8CString("TestContentExtension"));

    ContentExtensionStoreCallbackContext context(*this);
    WKUserContentExtensionStoreCompile(extensionStore.get(), filterIdentifier.get(), jsonSource.get(), &context, contentExtensionStoreCallback);
    runUntil(context.done, noTimeout);
    ASSERT(context.status == kWKUserContentExtensionStoreSuccess);
    ASSERT(context.filter);

    WKPageSetUserContentExtensionsEnabled(mainWebView()->page(), true);
    WKUserContentControllerAddUserContentFilter(userContentController(), context.filter.get());
}

void TestController::resetContentExtensions()
{
    if (!mainWebView())
        return;

    WKPageSetUserContentExtensionsEnabled(mainWebView()->page(), false);

    const char* contentExtensionsPath = libraryPathForTesting();
    if (!contentExtensionsPath)
        return;

    WKUserContentControllerRemoveAllUserContentFilters(userContentController());

    auto storePath = adoptWK(WKStringCreateWithUTF8CString(contentExtensionsPath));
    auto extensionStore = adoptWK(WKUserContentExtensionStoreCreate(storePath.get()));
    ASSERT(extensionStore);

    auto filterIdentifier = adoptWK(WKStringCreateWithUTF8CString("TestContentExtension"));

    ContentExtensionStoreCallbackContext context(*this);
    WKUserContentExtensionStoreRemove(extensionStore.get(), filterIdentifier.get(), &context, contentExtensionStoreCallback);
    runUntil(context.done, noTimeout);
    ASSERT(!context.filter);
}
#else // ENABLE(CONTENT_EXTENSIONS)
void TestController::configureContentExtensionForTest(const TestInvocation&)
{
}

void TestController::resetContentExtensions()
{
}
#endif // ENABLE(CONTENT_EXTENSIONS)
#endif // !PLATFORM(COCOA)

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

static TestCommand parseInputLine(const std::string& inputLine)
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
            result.timeout = Seconds::fromMilliseconds(atoi(timeoutToken.c_str()));
        } else if (arg == std::string("-p") || arg == std::string("--pixel-test")) {
            result.shouldDumpPixels = true;
            if (tokenizer.hasNext())
                result.expectedPixelHash = tokenizer.next();
        } else if (arg == std::string("--dump-jsconsolelog-in-stderr"))
            result.dumpJSConsoleLogInStdErr = true;
        else if (arg == std::string("--absolutePath"))
            result.absolutePath = tokenizer.next();
        else
            die(inputLine);
    }
    return result;
}

bool TestController::runTest(const char* inputLine)
{
    AutodrainedPool pool;
    
    WKTextCheckerSetTestingMode(true);
    TestCommand command = parseInputLine(std::string(inputLine));

    m_state = RunningTest;
    
    TestOptions options = testOptionsForTest(command);

    WKRetainPtr<WKURLRef> wkURL = adoptWK(createTestURL(command.pathOrURL.c_str()));
    m_currentInvocation = makeUnique<TestInvocation>(wkURL.get(), options);

    if (command.shouldDumpPixels || m_shouldDumpPixelsForAllTests)
        m_currentInvocation->setIsPixelTest(command.expectedPixelHash);

    if (command.timeout > 0_s)
        m_currentInvocation->setCustomTimeout(command.timeout);

    m_currentInvocation->setDumpJSConsoleLogInStdErr(command.dumpJSConsoleLogInStdErr || options.dumpJSConsoleLogInStdErr);

    platformWillRunTest(*m_currentInvocation);

    m_currentInvocation->invoke();
    m_currentInvocation = nullptr;

    return true;
}

bool TestController::waitForCompletion(const WTF::Function<void ()>& function, WTF::Seconds timeout)
{
    m_doneResetting = false;
    function();
    runUntil(m_doneResetting, timeout);
    return !m_doneResetting;
}

bool TestController::handleControlCommand(const char* command)
{
    if (!strncmp("#CHECK FOR WORLD LEAKS", command, 22)) {
        if (m_checkForWorldLeaks)
            findAndDumpWorldLeaks();
        else
            WTFLogAlways("WebKitTestRunner asked to check for world leaks, but was not run with --world-leaks");
        return true;
    }

    if (!strncmp("#LIST CHILD PROCESSES", command, 21)) {
        findAndDumpWebKitProcessIdentifiers();
        return true;
    }

    return false;
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

        if (handleControlCommand(filenameBuffer))
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
        if (m_checkForWorldLeaks)
            findAndDumpWorldLeaks();
    }
}

void TestController::runUntil(bool& done, WTF::Seconds timeout)
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

void TestController::didReceiveSynchronousMessageFromInjectedBundleWithListener(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef listener, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody, listener);
}

WKTypeRef TestController::getInjectedBundleInitializationUserData(WKContextRef, const void* clientInfo)
{
    return static_cast<TestController*>(const_cast<void*>(clientInfo))->getInjectedBundleInitializationUserData().leakRef();
}

// WKPageInjectedBundleClient

void TestController::didReceivePageMessageFromInjectedBundle(WKPageRef page, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

void TestController::didReceiveSynchronousPageMessageFromInjectedBundleWithListener(WKPageRef page, WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef listener, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody, listener);
}

void TestController::networkProcessDidCrash(WKContextRef context, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->networkProcessDidCrash();
}

void TestController::serviceWorkerProcessDidCrash(WKContextRef context, WKProcessID processID, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->serviceWorkerProcessDidCrash(processID);
}

void TestController::gpuProcessDidCrash(WKContextRef context, WKProcessID processID, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->gpuProcessDidCrash(processID);
}

void TestController::didReceiveKeyDownMessageFromInjectedBundle(WKDictionaryRef messageBodyDictionary, bool synchronous)
{
    WKRetainPtr<WKStringRef> keyKey = adoptWK(WKStringCreateWithUTF8CString("Key"));
    WKStringRef key = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, keyKey.get()));

    WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
    WKEventModifiers modifiers = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifiersKey.get()))));

    WKRetainPtr<WKStringRef> locationKey = adoptWK(WKStringCreateWithUTF8CString("Location"));
    unsigned location = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, locationKey.get()))));

    m_eventSenderProxy->keyDown(key, modifiers, location);
}

void TestController::didReceiveLiveDocumentsList(WKArrayRef liveDocumentList)
{
    auto numDocuments = WKArrayGetSize(liveDocumentList);

    HashMap<uint64_t, String> documentInfo;
    for (size_t i = 0; i < numDocuments; ++i) {
        WKTypeRef item = WKArrayGetItemAtIndex(liveDocumentList, i);
        if (item && WKGetTypeID(item) == WKDictionaryGetTypeID()) {
            WKDictionaryRef liveDocumentItem = static_cast<WKDictionaryRef>(item);

            WKRetainPtr<WKStringRef> idKey = adoptWK(WKStringCreateWithUTF8CString("id"));
            WKUInt64Ref documentID = static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(liveDocumentItem, idKey.get()));

            WKRetainPtr<WKStringRef> urlKey = adoptWK(WKStringCreateWithUTF8CString("url"));
            WKStringRef documentURL = static_cast<WKStringRef>(WKDictionaryGetItemForKey(liveDocumentItem, urlKey.get()));

            documentInfo.add(WKUInt64GetValue(documentID), toWTFString(documentURL));
        }
    }

    if (!documentInfo.size()) {
        m_abandonedDocumentInfo.clear();
        return;
    }

    // Remove any documents which are no longer live.
    m_abandonedDocumentInfo.removeIf([&](auto& keyAndValue) {
        return !documentInfo.contains(keyAndValue.key);
    });
    
    // Add newly abandoned documents.
    String currentTestURL = m_currentInvocation ? toWTFString(adoptWK(WKURLCopyString(m_currentInvocation->url()))) : "no test";
    for (const auto& it : documentInfo)
        m_abandonedDocumentInfo.add(it.key, AbandonedDocumentInfo(currentTestURL, it.value));
}

void TestController::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "LiveDocuments")) {
        ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());
        didReceiveLiveDocumentsList(static_cast<WKArrayRef>(messageBody));
        AsyncTask::currentTask()->taskComplete();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "EventSender")) {
        if (m_state != RunningTest)
            return;

        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
        WKStringRef subMessageName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subMessageKey.get()));

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown") || WKStringIsEqualToUTF8CString(subMessageName, "MouseUp")) {
            WKRetainPtr<WKStringRef> buttonKey = adoptWK(WKStringCreateWithUTF8CString("Button"));
            unsigned button = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, buttonKey.get()))));

            WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
            WKEventModifiers modifiers = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifiersKey.get()))));

            // Forward to WebProcess
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

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseScrollBy")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));

            // Forward to WebProcess
            m_eventSenderProxy->mouseScrollBy(x, y);
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
            m_eventSenderProxy->mouseScrollByWithWheelAndMomentumPhases(x, y, phase, momentum);

            return;
        }

        ASSERT_NOT_REACHED();
    }

    if (!m_currentInvocation)
        return;

    m_currentInvocation->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

void TestController::didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef listener)
{
    auto completionHandler = [listener = retainWK(listener)] (WKTypeRef reply) {
        WKMessageListenerSendReply(listener.get(), reply);
    };

    if (WKStringIsEqualToUTF8CString(messageName, "EventSender")) {
        if (m_state != RunningTest)
            return completionHandler(nullptr);

        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> subMessageKey = adoptWK(WKStringCreateWithUTF8CString("SubMessage"));
        WKStringRef subMessageName = static_cast<WKStringRef>(WKDictionaryGetItemForKey(messageBodyDictionary, subMessageKey.get()));

        if (WKStringIsEqualToUTF8CString(subMessageName, "KeyDown")) {
            didReceiveKeyDownMessageFromInjectedBundle(messageBodyDictionary, true);

            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown") || WKStringIsEqualToUTF8CString(subMessageName, "MouseUp")) {
            WKRetainPtr<WKStringRef> buttonKey = adoptWK(WKStringCreateWithUTF8CString("Button"));
            unsigned button = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, buttonKey.get()))));

            WKRetainPtr<WKStringRef> modifiersKey = adoptWK(WKStringCreateWithUTF8CString("Modifiers"));
            WKEventModifiers modifiers = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifiersKey.get()))));

            // Forward to WebProcess
            if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown"))
                m_eventSenderProxy->mouseDown(button, modifiers);
            else
                m_eventSenderProxy->mouseUp(button, modifiers);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseMoveTo")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));

            // Forward to WebProcess
            m_eventSenderProxy->mouseMoveTo(x, y);
            return completionHandler(nullptr);
        }

#if PLATFORM(MAC)
        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseForceClick")) {
            m_eventSenderProxy->mouseForceClick();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "StartAndCancelMouseForceClick")) {
            m_eventSenderProxy->startAndCancelMouseForceClick();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseForceDown")) {
            m_eventSenderProxy->mouseForceDown();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseForceUp")) {
            m_eventSenderProxy->mouseForceUp();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseForceChanged")) {
            WKRetainPtr<WKStringRef> forceKey = adoptWK(WKStringCreateWithUTF8CString("Force"));
            double force = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, forceKey.get())));

            m_eventSenderProxy->mouseForceChanged(force);
            return completionHandler(nullptr);
        }
#endif // PLATFORM(MAC)

        if (WKStringIsEqualToUTF8CString(subMessageName, "ContinuousMouseScrollBy")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            double x = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get())));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            double y = WKDoubleGetValue(static_cast<WKDoubleRef>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get())));

            WKRetainPtr<WKStringRef> pagedKey = adoptWK(WKStringCreateWithUTF8CString("Paged"));
            bool paged = static_cast<bool>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, pagedKey.get()))));

            // Forward to WebProcess
            m_eventSenderProxy->continuousMouseScrollBy(x, y, paged);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "LeapForward")) {
            WKRetainPtr<WKStringRef> timeKey = adoptWK(WKStringCreateWithUTF8CString("TimeInMilliseconds"));
            unsigned time = static_cast<unsigned>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, timeKey.get()))));

            m_eventSenderProxy->leapForward(time);
            return completionHandler(nullptr);
        }

#if ENABLE(TOUCH_EVENTS)
        if (WKStringIsEqualToUTF8CString(subMessageName, "AddTouchPoint")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            int x = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get()))));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            int y = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get()))));

            m_eventSenderProxy->addTouchPoint(x, y);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "UpdateTouchPoint")) {
            WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
            int index = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, indexKey.get()))));

            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("X"));
            int x = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get()))));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("Y"));
            int y = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get()))));

            m_eventSenderProxy->updateTouchPoint(index, x, y);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetTouchModifier")) {
            WKRetainPtr<WKStringRef> modifierKey = adoptWK(WKStringCreateWithUTF8CString("Modifier"));
            WKEventModifiers modifier = static_cast<WKEventModifiers>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, modifierKey.get()))));

            WKRetainPtr<WKStringRef> enableKey = adoptWK(WKStringCreateWithUTF8CString("Enable"));
            bool enable = static_cast<bool>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, enableKey.get()))));

            m_eventSenderProxy->setTouchModifier(modifier, enable);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetTouchPointRadius")) {
            WKRetainPtr<WKStringRef> xKey = adoptWK(WKStringCreateWithUTF8CString("RadiusX"));
            int x = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, xKey.get()))));

            WKRetainPtr<WKStringRef> yKey = adoptWK(WKStringCreateWithUTF8CString("RadiusY"));
            int y = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, yKey.get()))));

            m_eventSenderProxy->setTouchPointRadius(x, y);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchStart")) {
            m_eventSenderProxy->touchStart();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchMove")) {
            m_eventSenderProxy->touchMove();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchEnd")) {
            m_eventSenderProxy->touchEnd();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "TouchCancel")) {
            m_eventSenderProxy->touchCancel();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "ClearTouchPoints")) {
            m_eventSenderProxy->clearTouchPoints();
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "ReleaseTouchPoint")) {
            WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
            int index = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, indexKey.get()))));
            m_eventSenderProxy->releaseTouchPoint(index);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "CancelTouchPoint")) {
            WKRetainPtr<WKStringRef> indexKey = adoptWK(WKStringCreateWithUTF8CString("Index"));
            int index = static_cast<int>(WKUInt64GetValue(static_cast<WKUInt64Ref>(WKDictionaryGetItemForKey(messageBodyDictionary, indexKey.get()))));
            m_eventSenderProxy->cancelTouchPoint(index);
            return completionHandler(nullptr);
        }
#endif
        ASSERT_NOT_REACHED();
    }

    auto setHTTPCookieAcceptPolicy = [&] (WKHTTPCookieAcceptPolicy policy, CompletionHandler<void(WKTypeRef)>&& completionHandler) {
        auto context = new CompletionHandler<void(WKTypeRef)>(WTFMove(completionHandler));
        WKHTTPCookieStoreSetHTTPCookieAcceptPolicy(WKWebsiteDataStoreGetHTTPCookieStore(websiteDataStore()), policy, context, [] (void* context) {
            auto completionHandlerPointer = static_cast<CompletionHandler<void(WKTypeRef)>*>(context);
            (*completionHandlerPointer)(nullptr);
            delete completionHandlerPointer;
        });
    };

    if (WKStringIsEqualToUTF8CString(messageName, "SetAlwaysAcceptCookies")) {
        WKBooleanRef accept = static_cast<WKBooleanRef>(messageBody);
        WKHTTPCookieAcceptPolicy policy = WKBooleanGetValue(accept) ? kWKHTTPCookieAcceptPolicyAlways : kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
        return setHTTPCookieAcceptPolicy(policy, WTFMove(completionHandler));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetOnlyAcceptFirstPartyCookies")) {
        WKBooleanRef accept = static_cast<WKBooleanRef>(messageBody);
        WKHTTPCookieAcceptPolicy policy = WKBooleanGetValue(accept) ? kWKHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain : kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
        return setHTTPCookieAcceptPolicy(policy, WTFMove(completionHandler));
    }

    completionHandler(m_currentInvocation->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody).get());
}

WKRetainPtr<WKTypeRef> TestController::getInjectedBundleInitializationUserData()
{
    return nullptr;
}

// WKContextClient

void TestController::networkProcessDidCrash()
{
    pid_t pid = WKContextGetNetworkProcessIdentifier(m_context.get());
    fprintf(stderr, "#CRASHED - %s (pid %ld)\n", networkProcessName(), static_cast<long>(pid));
    exit(1);
}

void TestController::serviceWorkerProcessDidCrash(WKProcessID processID)
{
    fprintf(stderr, "#CRASHED - ServiceWorkerProcess (pid %ld)\n", static_cast<long>(processID));
    if (m_shouldExitWhenWebProcessCrashes)
        exit(1);
}

void TestController::gpuProcessDidCrash(WKProcessID processID)
{
    fprintf(stderr, "#CRASHED - GPUProcess (pid %ld)\n", static_cast<long>(processID));
    if (m_shouldExitWhenWebProcessCrashes)
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

void TestController::didReceiveServerRedirectForProvisionalNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveServerRedirectForProvisionalNavigation(page, navigation, userData);
}

bool TestController::canAuthenticateAgainstProtectionSpace(WKPageRef page, WKProtectionSpaceRef protectionSpace, const void* clientInfo)
{
    return static_cast<TestController*>(const_cast<void*>(clientInfo))->canAuthenticateAgainstProtectionSpace(page, protectionSpace);
}

void TestController::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveAuthenticationChallenge(page, /*frame,*/ authenticationChallenge);
}

void TestController::processDidCrash(WKPageRef page, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->processDidCrash();
}

void TestController::didBeginNavigationGesture(WKPageRef page, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didBeginNavigationGesture(page);
}

void TestController::willEndNavigationGesture(WKPageRef page, WKBackForwardListItemRef backForwardListItem, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->willEndNavigationGesture(page, backForwardListItem);
}

void TestController::didEndNavigationGesture(WKPageRef page, WKBackForwardListItemRef backForwardListItem, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didEndNavigationGesture(page, backForwardListItem);
}

void TestController::didRemoveNavigationGestureSnapshot(WKPageRef page, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didRemoveNavigationGestureSnapshot(page);
}

WKPluginLoadPolicy TestController::decidePolicyForPluginLoad(WKPageRef page, WKPluginLoadPolicy currentPluginLoadPolicy, WKDictionaryRef pluginInformation, WKStringRef* unavailabilityDescription, const void* clientInfo)
{
    return static_cast<TestController*>(const_cast<void*>(clientInfo))->decidePolicyForPluginLoad(page, currentPluginLoadPolicy, pluginInformation, unavailabilityDescription);
}

WKPluginLoadPolicy TestController::decidePolicyForPluginLoad(WKPageRef, WKPluginLoadPolicy currentPluginLoadPolicy, WKDictionaryRef pluginInformation, WKStringRef* unavailabilityDescription)
{
    if (m_shouldBlockAllPlugins)
        return kWKPluginLoadPolicyBlocked;

#if PLATFORM(MAC)
    WKStringRef bundleIdentifier = (WKStringRef)WKDictionaryGetItemForKey(pluginInformation, WKPluginInformationBundleIdentifierKey());
    if (!bundleIdentifier)
        return currentPluginLoadPolicy;

    if (WKStringIsEqualToUTF8CString(bundleIdentifier, "com.apple.QuickTime Plugin.plugin"))
        return currentPluginLoadPolicy;

    if (WKStringIsEqualToUTF8CString(bundleIdentifier, "com.apple.testnetscapeplugin"))
        return currentPluginLoadPolicy;

    // Please don't use any other plug-ins in tests, as they will not be installed on all machines.
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Unexpected plugin bundle identifier: %s", toSTD(bundleIdentifier).c_str());
#else
    return currentPluginLoadPolicy;
#endif
}

void TestController::setBlockAllPlugins(bool shouldBlock)
{
    m_shouldBlockAllPlugins = shouldBlock;

#if PLATFORM(MAC)
    auto policy = shouldBlock ? kWKPluginLoadClientPolicyBlock : kWKPluginLoadClientPolicyAllow;

    WKRetainPtr<WKStringRef> nameNetscape = adoptWK(WKStringCreateWithUTF8CString("com.apple.testnetscapeplugin"));
    WKRetainPtr<WKStringRef> nameFlash = adoptWK(WKStringCreateWithUTF8CString("com.macromedia.Flash Player.plugin"));
    WKRetainPtr<WKStringRef> emptyString = adoptWK(WKStringCreateWithUTF8CString(""));
    WKContextSetPluginLoadClientPolicy(m_context.get(), policy, emptyString.get(), nameNetscape.get(), emptyString.get());
    WKContextSetPluginLoadClientPolicy(m_context.get(), policy, emptyString.get(), nameFlash.get(), emptyString.get());
#endif
}

void TestController::setPluginSupportedMode(const String& mode)
{
    if (m_unsupportedPluginMode == mode)
        return;

    m_unsupportedPluginMode = mode;
    if (m_unsupportedPluginMode.isEmpty()) {
        WKContextClearSupportedPlugins(m_context.get());
        return;
    }

    WKRetainPtr<WKMutableArrayRef> emptyArray = adoptWK(WKMutableArrayCreate());
    WKRetainPtr<WKStringRef> allOrigins = adoptWK(WKStringCreateWithUTF8CString(""));
    WKRetainPtr<WKStringRef> specificOrigin = adoptWK(WKStringCreateWithUTF8CString("localhost"));

    WKRetainPtr<WKStringRef> pdfName = adoptWK(WKStringCreateWithUTF8CString("My personal PDF"));
    WKContextAddSupportedPlugin(m_context.get(), allOrigins.get(), pdfName.get(), emptyArray.get(), emptyArray.get());

    WKRetainPtr<WKStringRef> nameNetscape = adoptWK(WKStringCreateWithUTF8CString("com.apple.testnetscapeplugin"));
    WKRetainPtr<WKStringRef> mimeTypeNetscape = adoptWK(WKStringCreateWithUTF8CString("application/x-webkit-test-netscape"));
    WKRetainPtr<WKMutableArrayRef> mimeTypesNetscape = adoptWK(WKMutableArrayCreate());
    WKArrayAppendItem(mimeTypesNetscape.get(), mimeTypeNetscape.get());

    WKRetainPtr<WKStringRef> namePdf = adoptWK(WKStringCreateWithUTF8CString("WebKit built-in PDF"));

    if (m_unsupportedPluginMode == "allOrigins") {
        WKContextAddSupportedPlugin(m_context.get(), allOrigins.get(), nameNetscape.get(), mimeTypesNetscape.get(), emptyArray.get());
        WKContextAddSupportedPlugin(m_context.get(), allOrigins.get(), namePdf.get(), emptyArray.get(), emptyArray.get());
        return;
    }

    if (m_unsupportedPluginMode == "specificOrigin") {
        WKContextAddSupportedPlugin(m_context.get(), specificOrigin.get(), nameNetscape.get(), mimeTypesNetscape.get(), emptyArray.get());
        WKContextAddSupportedPlugin(m_context.get(), specificOrigin.get(), namePdf.get(), emptyArray.get(), emptyArray.get());
        return;
    }
}

void TestController::didCommitNavigation(WKPageRef page, WKNavigationRef navigation)
{
    mainWebView()->focus();
}

void TestController::didReceiveServerRedirectForProvisionalNavigation(WKPageRef page, WKNavigationRef navigation, WKTypeRef userData)
{
    m_didReceiveServerRedirectForProvisionalNavigation = true;
    return;
}

static const char* toString(WKProtectionSpaceAuthenticationScheme scheme)
{
    switch (scheme) {
    case kWKProtectionSpaceAuthenticationSchemeDefault:
        return "ProtectionSpaceAuthenticationSchemeDefault";
    case kWKProtectionSpaceAuthenticationSchemeHTTPBasic:
        return "ProtectionSpaceAuthenticationSchemeHTTPBasic";
    case kWKProtectionSpaceAuthenticationSchemeHTMLForm:
        return "ProtectionSpaceAuthenticationSchemeHTMLForm";
    case kWKProtectionSpaceAuthenticationSchemeNTLM:
        return "ProtectionSpaceAuthenticationSchemeNTLM";
    case kWKProtectionSpaceAuthenticationSchemeNegotiate:
        return "ProtectionSpaceAuthenticationSchemeNegotiate";
    case kWKProtectionSpaceAuthenticationSchemeClientCertificateRequested:
        return "ProtectionSpaceAuthenticationSchemeClientCertificateRequested";
    case kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        return "ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested";
    case kWKProtectionSpaceAuthenticationSchemeOAuth:
        return "ProtectionSpaceAuthenticationSchemeOAuth";
    case kWKProtectionSpaceAuthenticationSchemeUnknown:
        return "ProtectionSpaceAuthenticationSchemeUnknown";
    }
    ASSERT_NOT_REACHED();
    return "ProtectionSpaceAuthenticationSchemeUnknown";
}

bool TestController::canAuthenticateAgainstProtectionSpace(WKPageRef page, WKProtectionSpaceRef protectionSpace)
{
    if (m_shouldLogCanAuthenticateAgainstProtectionSpace)
        m_currentInvocation->outputText("canAuthenticateAgainstProtectionSpace\n");
    WKProtectionSpaceAuthenticationScheme authenticationScheme = WKProtectionSpaceGetAuthenticationScheme(protectionSpace);
    
    if (authenticationScheme == kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {
        std::string host = toSTD(adoptWK(WKProtectionSpaceCopyHost(protectionSpace)).get());
        return host == "localhost" || host == "127.0.0.1" || (m_allowAnyHTTPSCertificateForAllowedHosts && m_allowedHosts.find(host) != m_allowedHosts.end());
    }
    
    return authenticationScheme <= kWKProtectionSpaceAuthenticationSchemeHTTPDigest || authenticationScheme == kWKProtectionSpaceAuthenticationSchemeOAuth;
}

void TestController::didFinishNavigation(WKPageRef page, WKNavigationRef navigation)
{
    if (m_state != Resetting)
        return;

    WKRetainPtr<WKURLRef> wkURL = adoptWK(WKFrameCopyURL(WKPageGetMainFrame(page)));
    if (!WKURLIsEqual(wkURL.get(), blankURL()))
        return;

    m_doneResetting = true;
    singleton().notifyDone();
}

void TestController::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge)
{
    WKProtectionSpaceRef protectionSpace = WKAuthenticationChallengeGetProtectionSpace(authenticationChallenge);
    WKAuthenticationDecisionListenerRef decisionListener = WKAuthenticationChallengeGetDecisionListener(authenticationChallenge);
    WKProtectionSpaceAuthenticationScheme authenticationScheme = WKProtectionSpaceGetAuthenticationScheme(protectionSpace);

    if (authenticationScheme == kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {
        // Any non-empty credential signals to accept the server trust. Since the cross-platform API
        // doesn't expose a way to create a credential from server trust, we use a password credential.

        m_serverTrustEvaluationCallbackCallsCount++;

        if (m_allowsAnySSLCertificate) {
            WKRetainPtr<WKCredentialRef> credential = adoptWK(WKCredentialCreate(toWK("accept server trust").get(), toWK("").get(), kWKCredentialPersistenceNone));
            WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
            return;
        }
        WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(decisionListener);
        return;
    }

    if (m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges) {
        m_currentInvocation->outputText("Simulating reject protection space and continue for authentication challenge\n");
        WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(decisionListener);
        return;
    }

    std::string host = toSTD(adoptWK(WKProtectionSpaceCopyHost(protectionSpace)).get());
    int port = WKProtectionSpaceGetPort(protectionSpace);
    String message = makeString(host.c_str(), ':', port, " - didReceiveAuthenticationChallenge - ", toString(authenticationScheme), " - ");
    if (!m_handlesAuthenticationChallenges)
        message.append("Simulating cancelled authentication sheet\n");
    else
        message.append("Responding with " + m_authenticationUsername + ":" + m_authenticationPassword + "\n");
    m_currentInvocation->outputText(message);

    if (!m_handlesAuthenticationChallenges) {
        WKAuthenticationDecisionListenerUseCredential(decisionListener, 0);
        return;
    }
    WKRetainPtr<WKStringRef> username = adoptWK(WKStringCreateWithUTF8CString(m_authenticationUsername.utf8().data()));
    WKRetainPtr<WKStringRef> password = adoptWK(WKStringCreateWithUTF8CString(m_authenticationPassword.utf8().data()));
    WKRetainPtr<WKCredentialRef> credential = adoptWK(WKCredentialCreate(username.get(), password.get(), kWKCredentialPersistenceForSession));
    WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
}

    
// WKContextDownloadClient

void TestController::downloadDidStart(WKContextRef context, WKDownloadRef download, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidStart(context, download);
}
    
WKStringRef TestController::decideDestinationWithSuggestedFilename(WKContextRef context, WKDownloadRef download, WKStringRef filename, bool* allowOverwrite, const void* clientInfo)
{
    return static_cast<TestController*>(const_cast<void*>(clientInfo))->decideDestinationWithSuggestedFilename(context, download, filename, allowOverwrite);
}

void TestController::downloadDidFinish(WKContextRef context, WKDownloadRef download, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidFinish(context, download);
}

void TestController::downloadDidFail(WKContextRef context, WKDownloadRef download, WKErrorRef error, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidFail(context, download, error);
}

void TestController::downloadDidCancel(WKContextRef context, WKDownloadRef download, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidCancel(context, download);
}

void TestController::downloadDidReceiveServerRedirectToURL(WKContextRef context, WKDownloadRef download, WKURLRef url, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidReceiveServerRedirectToURL(context, download, url);
}

void TestController::downloadDidStart(WKContextRef context, WKDownloadRef download)
{
    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText("Download started.\n");
}

WKStringRef TestController::decideDestinationWithSuggestedFilename(WKContextRef, WKDownloadRef, WKStringRef filename, bool*& allowOverwrite)
{
    String suggestedFilename = toWTFString(filename);

    if (m_shouldLogDownloadCallbacks) {
        StringBuilder builder;
        builder.append("Downloading URL with suggested filename \"");
        builder.append(suggestedFilename);
        builder.append("\"\n");
        m_currentInvocation->outputText(builder.toString());
    }

    const char* dumpRenderTreeTemp = libraryPathForTesting();
    if (!dumpRenderTreeTemp)
        return nullptr;

    *allowOverwrite = true;
    String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);
    if (suggestedFilename.isEmpty())
        suggestedFilename = "Unknown";

    return toWK(temporaryFolder + "/" + suggestedFilename).leakRef();
}

void TestController::downloadDidFinish(WKContextRef, WKDownloadRef)
{
    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText("Download completed.\n");
    m_currentInvocation->notifyDownloadDone();
}

void TestController::downloadDidReceiveServerRedirectToURL(WKContextRef, WKDownloadRef, WKURLRef url)
{
    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText(makeString("Download was redirected to \"", toWTFString(adoptWK(WKURLCopyString(url))), "\".\n"));
}

void TestController::downloadDidFail(WKContextRef, WKDownloadRef, WKErrorRef error)
{
    if (m_shouldLogDownloadCallbacks) {
        m_currentInvocation->outputText("Download failed.\n"_s);

        auto domain = toWTFString(adoptWK(WKErrorCopyDomain(error)));
        auto description = toWTFString(adoptWK(WKErrorCopyLocalizedDescription(error)));
        int code = WKErrorGetErrorCode(error);

        m_currentInvocation->outputText(makeString("Failed: ", domain, ", code=", code, ", description=", description, "\n"));
    }
    m_currentInvocation->notifyDownloadDone();
}

void TestController::downloadDidCancel(WKContextRef, WKDownloadRef)
{
    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText("Download cancelled.\n");
    m_currentInvocation->notifyDownloadDone();
}

void TestController::processDidCrash()
{
    // This function can be called multiple times when crash logs are being saved on Windows, so
    // ensure we only print the crashed message once.
    if (!m_didPrintWebProcessCrashedMessage) {
        pid_t pid = WKPageGetProcessIdentifier(m_mainWebView->page());
        fprintf(stderr, "#CRASHED - %s (pid %ld)\n", webProcessName(), static_cast<long>(pid));
        fflush(stderr);
        m_didPrintWebProcessCrashedMessage = true;
    }

    if (m_shouldExitWhenWebProcessCrashes)
        exit(1);
}

void TestController::didBeginNavigationGesture(WKPageRef)
{
    m_currentInvocation->didBeginSwipe();
}

void TestController::willEndNavigationGesture(WKPageRef, WKBackForwardListItemRef)
{
    m_currentInvocation->willEndSwipe();
}

void TestController::didEndNavigationGesture(WKPageRef, WKBackForwardListItemRef)
{
    m_currentInvocation->didEndSwipe();
}

void TestController::didRemoveNavigationGestureSnapshot(WKPageRef)
{
    m_currentInvocation->didRemoveSwipeSnapshot();
}

void TestController::simulateWebNotificationClick(uint64_t notificationID)
{
    m_webNotificationProvider.simulateWebNotificationClick(mainWebView()->page(), notificationID);
}

void TestController::setGeolocationPermission(bool enabled)
{
    m_isGeolocationPermissionSet = true;
    m_isGeolocationPermissionAllowed = enabled;
    decidePolicyForGeolocationPermissionRequestIfPossible();
}

void TestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, bool providesFloorLevel, double floorLevel)
{
    m_geolocationProvider->setPosition(latitude, longitude, accuracy, providesAltitude, altitude, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed, providesFloorLevel, floorLevel);
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

bool TestController::isGeolocationProviderActive() const
{
    return m_geolocationProvider->isActive();
}

static String originUserVisibleName(WKSecurityOriginRef origin)
{
    if (!origin)
        return emptyString();

    auto host = toWTFString(adoptWK(WKSecurityOriginCopyHost(origin)));
    auto protocol = toWTFString(adoptWK(WKSecurityOriginCopyProtocol(origin)));

    if (host.isEmpty() || protocol.isEmpty())
        return emptyString();

    if (int port = WKSecurityOriginGetPort(origin))
        return makeString(protocol, "://", host, ':', port);

    return makeString(protocol, "://", host);
}

static String userMediaOriginHash(WKSecurityOriginRef userMediaDocumentOrigin, WKSecurityOriginRef topLevelDocumentOrigin)
{
    String userMediaDocumentOriginString = originUserVisibleName(userMediaDocumentOrigin);
    String topLevelDocumentOriginString = originUserVisibleName(topLevelDocumentOrigin);

    if (topLevelDocumentOriginString.isEmpty())
        return userMediaDocumentOriginString;

    return makeString(userMediaDocumentOriginString, '-', topLevelDocumentOriginString);
}

static String userMediaOriginHash(WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString)
{
    auto userMediaDocumentOrigin = adoptWK(WKSecurityOriginCreateFromString(userMediaDocumentOriginString));
    if (!WKStringGetLength(topLevelDocumentOriginString))
        return userMediaOriginHash(userMediaDocumentOrigin.get(), nullptr);

    auto topLevelDocumentOrigin = adoptWK(WKSecurityOriginCreateFromString(topLevelDocumentOriginString));
    return userMediaOriginHash(userMediaDocumentOrigin.get(), topLevelDocumentOrigin.get());
}

void TestController::setUserMediaPermission(bool enabled)
{
    m_isUserMediaPermissionSet = true;
    m_isUserMediaPermissionAllowed = enabled;
    decidePolicyForUserMediaPermissionRequestIfPossible();
}

void TestController::resetUserMediaPermission()
{
    m_isUserMediaPermissionSet = false;
}

void TestController::setShouldDismissJavaScriptAlertsAsynchronously(bool value)
{
    m_shouldDismissJavaScriptAlertsAsynchronously = value;
}

void TestController::handleJavaScriptAlert(WKPageRunJavaScriptAlertResultListenerRef listener)
{
    if (!m_shouldDismissJavaScriptAlertsAsynchronously) {
        WKPageRunJavaScriptAlertResultListenerCall(listener);
        return;
    }

    WKRetain(listener);
    callOnMainThread([listener] {
        WKPageRunJavaScriptAlertResultListenerCall(listener);
        WKRelease(listener);
    });
}

class OriginSettings : public RefCounted<OriginSettings> {
public:
    explicit OriginSettings()
    {
    }

    bool persistentPermission() const { return m_persistentPermission; }
    void setPersistentPermission(bool permission) { m_persistentPermission = permission; }

    String persistentSalt() const { return m_persistentSalt; }
    void setPersistentSalt(const String& salt) { m_persistentSalt = salt; }

    HashMap<uint64_t, String>& ephemeralSalts() { return m_ephemeralSalts; }

    void incrementRequestCount() { ++m_requestCount; }
    void resetRequestCount() { m_requestCount = 0; }
    unsigned requestCount() const { return m_requestCount; }

private:
    HashMap<uint64_t, String> m_ephemeralSalts;
    String m_persistentSalt;
    unsigned m_requestCount { 0 };
    bool m_persistentPermission { false };
};

String TestController::saltForOrigin(WKFrameRef frame, String originHash)
{
    auto& settings = settingsForOrigin(originHash);
    auto& ephemeralSalts = settings.ephemeralSalts();
    auto frameHandle = adoptWK(WKFrameCreateFrameHandle(frame));
    uint64_t frameIdentifier = WKFrameHandleGetFrameID(frameHandle.get());
    String frameSalt = ephemeralSalts.get(frameIdentifier);

    if (settings.persistentPermission()) {
        if (frameSalt.length())
            return frameSalt;

        if (!settings.persistentSalt().length())
            settings.setPersistentSalt(createCanonicalUUIDString());

        return settings.persistentSalt();
    }

    if (!frameSalt.length()) {
        frameSalt = createCanonicalUUIDString();
        ephemeralSalts.add(frameIdentifier, frameSalt);
    }

    return frameSalt;
}

void TestController::setUserMediaPersistentPermissionForOrigin(bool permission, WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString)
{
    auto originHash = userMediaOriginHash(userMediaDocumentOriginString, topLevelDocumentOriginString);
    auto& settings = settingsForOrigin(originHash);
    settings.setPersistentPermission(permission);
}

void TestController::handleCheckOfUserMediaPermissionForOrigin(WKFrameRef frame, WKSecurityOriginRef userMediaDocumentOrigin, WKSecurityOriginRef topLevelDocumentOrigin, const WKUserMediaPermissionCheckRef& checkRequest)
{
    auto originHash = userMediaOriginHash(userMediaDocumentOrigin, topLevelDocumentOrigin);
    auto salt = saltForOrigin(frame, originHash);
    WKRetainPtr<WKStringRef> saltString = adoptWK(WKStringCreateWithUTF8CString(salt.utf8().data()));

    WKUserMediaPermissionCheckSetUserMediaAccessInfo(checkRequest, saltString.get(), settingsForOrigin(originHash).persistentPermission());
}

bool TestController::handleDeviceOrientationAndMotionAccessRequest(WKSecurityOriginRef origin)
{
    m_currentInvocation->outputText(makeString("Received device orientation & motion access request for security origin \"", originUserVisibleName(origin), "\".\n"));
    return m_shouldAllowDeviceOrientationAndMotionAccess;
}

void TestController::handleUserMediaPermissionRequest(WKFrameRef frame, WKSecurityOriginRef userMediaDocumentOrigin, WKSecurityOriginRef topLevelDocumentOrigin, WKUserMediaPermissionRequestRef request)
{
    auto originHash = userMediaOriginHash(userMediaDocumentOrigin, topLevelDocumentOrigin);
    m_userMediaPermissionRequests.append(std::make_pair(originHash, request));
    decidePolicyForUserMediaPermissionRequestIfPossible();
}

OriginSettings& TestController::settingsForOrigin(const String& originHash)
{
    RefPtr<OriginSettings> settings = m_cachedUserMediaPermissions.get(originHash);
    if (!settings) {
        settings = adoptRef(*new OriginSettings());
        m_cachedUserMediaPermissions.add(originHash, settings);
    }

    return *settings;
}

unsigned TestController::userMediaPermissionRequestCountForOrigin(WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString)
{
    auto originHash = userMediaOriginHash(userMediaDocumentOriginString, topLevelDocumentOriginString);
    return settingsForOrigin(originHash).requestCount();
}

void TestController::resetUserMediaPermissionRequestCountForOrigin(WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString)
{
    auto originHash = userMediaOriginHash(userMediaDocumentOriginString, topLevelDocumentOriginString);
    settingsForOrigin(originHash).resetRequestCount();
}

void TestController::decidePolicyForUserMediaPermissionRequestIfPossible()
{
    if (!m_isUserMediaPermissionSet)
        return;

    for (auto& pair : m_userMediaPermissionRequests) {
        auto originHash = pair.first;
        auto request = pair.second.get();

        auto& settings = settingsForOrigin(originHash);
        settings.incrementRequestCount();

        if (!m_isUserMediaPermissionAllowed && !settings.persistentPermission()) {
            WKUserMediaPermissionRequestDeny(request, kWKPermissionDenied);
            continue;
        }

        WKRetainPtr<WKArrayRef> audioDeviceUIDs = adoptWK(WKUserMediaPermissionRequestAudioDeviceUIDs(request));
        WKRetainPtr<WKArrayRef> videoDeviceUIDs = adoptWK(WKUserMediaPermissionRequestVideoDeviceUIDs(request));

        if (!WKArrayGetSize(videoDeviceUIDs.get()) && !WKArrayGetSize(audioDeviceUIDs.get())) {
            WKUserMediaPermissionRequestDeny(request, kWKNoConstraints);
            continue;
        }

        WKRetainPtr<WKStringRef> videoDeviceUID;
        if (WKArrayGetSize(videoDeviceUIDs.get()))
            videoDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(videoDeviceUIDs.get(), 0));
        else
            videoDeviceUID = adoptWK(WKStringCreateWithUTF8CString(""));

        WKRetainPtr<WKStringRef> audioDeviceUID;
        if (WKArrayGetSize(audioDeviceUIDs.get()))
            audioDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(audioDeviceUIDs.get(), 0));
        else
            audioDeviceUID = adoptWK(WKStringCreateWithUTF8CString(""));

        WKUserMediaPermissionRequestAllow(request, audioDeviceUID.get(), videoDeviceUID.get());
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
    static_cast<TestController*>(const_cast<void*>(clientInfo))->decidePolicyForNavigationAction(navigationAction, listener);
}

void TestController::decidePolicyForNavigationAction(WKNavigationActionRef navigationAction, WKFramePolicyListenerRef listener)
{
    WKRetainPtr<WKFramePolicyListenerRef> retainedListener { listener };
    WKRetainPtr<WKNavigationActionRef> retainedNavigationAction { navigationAction };
    const bool shouldIgnore { m_policyDelegateEnabled && !m_policyDelegatePermissive };
    auto decisionFunction = [shouldIgnore, retainedListener, retainedNavigationAction, shouldSwapToEphemeralSessionOnNextNavigation = m_shouldSwapToEphemeralSessionOnNextNavigation, shouldSwapToDefaultSessionOnNextNavigation = m_shouldSwapToDefaultSessionOnNextNavigation]() {
        if (shouldIgnore)
            WKFramePolicyListenerIgnore(retainedListener.get());
        else if (WKNavigationActionShouldPerformDownload(retainedNavigationAction.get()))
            WKFramePolicyListenerDownload(retainedListener.get());
        else {
            if (shouldSwapToEphemeralSessionOnNextNavigation || shouldSwapToDefaultSessionOnNextNavigation) {
                ASSERT(shouldSwapToEphemeralSessionOnNextNavigation != shouldSwapToDefaultSessionOnNextNavigation);
                auto policies = adoptWK(WKWebsitePoliciesCreate());
                WKRetainPtr<WKWebsiteDataStoreRef> newSession = TestController::defaultWebsiteDataStore();
                if (shouldSwapToEphemeralSessionOnNextNavigation)
                    newSession = adoptWK(WKWebsiteDataStoreCreateNonPersistentDataStore());
                WKWebsitePoliciesSetDataStore(policies.get(), newSession.get());
                WKFramePolicyListenerUseWithPolicies(retainedListener.get(), policies.get());
            } else
                WKFramePolicyListenerUse(retainedListener.get());
        }
    };
    m_shouldSwapToEphemeralSessionOnNextNavigation = false;
    m_shouldSwapToDefaultSessionOnNextNavigation = false;

    if (m_shouldDecideNavigationPolicyAfterDelay)
        RunLoop::main().dispatch(WTFMove(decisionFunction));
    else
        decisionFunction();
}

void TestController::decidePolicyForNavigationResponse(WKPageRef, WKNavigationResponseRef navigationResponse, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->decidePolicyForNavigationResponse(navigationResponse, listener);
}

void TestController::decidePolicyForNavigationResponse(WKNavigationResponseRef navigationResponse, WKFramePolicyListenerRef listener)
{
    WKRetainPtr<WKNavigationResponseRef> retainedNavigationResponse { navigationResponse };
    WKRetainPtr<WKFramePolicyListenerRef> retainedListener { listener };

    bool shouldDownloadUndisplayableMIMETypes = m_shouldDownloadUndisplayableMIMETypes;
    auto decisionFunction = [shouldDownloadUndisplayableMIMETypes, retainedNavigationResponse, retainedListener]() {
        // Even though Response was already checked by WKBundlePagePolicyClient, the check did not include plugins
        // so we have to re-check again.
        if (WKNavigationResponseCanShowMIMEType(retainedNavigationResponse.get())) {
            WKFramePolicyListenerUse(retainedListener.get());
            return;
        }

        if (shouldDownloadUndisplayableMIMETypes)
            WKFramePolicyListenerDownload(retainedListener.get());
        else
            WKFramePolicyListenerIgnore(retainedListener.get());
    };

    if (m_shouldDecideResponsePolicyAfterDelay)
        RunLoop::main().dispatch(WTFMove(decisionFunction));
    else
        decisionFunction();
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
    auto url = adoptWK(WKNavigationDataCopyURL(navigationData));
    auto urlString = toWTFString(adoptWK(WKURLCopyString(url.get())));
    // Title
    auto title = toWTFString(adoptWK(WKNavigationDataCopyTitle(navigationData)));
    // HTTP method
    auto request = adoptWK(WKNavigationDataCopyOriginalRequest(navigationData));
    auto method = toWTFString(adoptWK(WKURLRequestCopyHTTPMethod(request.get())));

    // FIXME: Determine whether the navigation was successful / a client redirect rather than hard-coding the message here.
    m_currentInvocation->outputText(makeString("WebView navigated to url \"", urlString, "\" with title \"", title, "\" with HTTP equivalent method \"", method,
        "\".  The navigation was successful and was not a client redirect.\n"));
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

    auto source = toWTFString(adoptWK(WKURLCopyString(sourceURL)));
    auto destination = toWTFString(adoptWK(WKURLCopyString(destinationURL)));

    m_currentInvocation->outputText(makeString("WebView performed a client redirect from \"", source, "\" to \"", destination, "\".\n"));
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

    auto source = toWTFString(adoptWK(WKURLCopyString(sourceURL)));
    auto destination = toWTFString(adoptWK(WKURLCopyString(destinationURL)));

    m_currentInvocation->outputText(makeString("WebView performed a server redirect from \"", source, "\" to \"", destination, "\".\n"));
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

    auto urlString = toWTFString(adoptWK(WKURLCopyString(URL)));
    m_currentInvocation->outputText(makeString("WebView updated the title for history URL \"", urlString, "\" to \"", toWTFString(title), "\".\n"));
}

void TestController::setNavigationGesturesEnabled(bool value)
{
    m_mainWebView->setNavigationGesturesEnabled(value);
}

void TestController::setIgnoresViewportScaleLimits(bool ignoresViewportScaleLimits)
{
    WKPageSetIgnoresViewportScaleLimits(m_mainWebView->page(), ignoresViewportScaleLimits);
}

void TestController::terminateNetworkProcess()
{
    WKContextTerminateNetworkProcess(platformContext());
}

void TestController::terminateServiceWorkers()
{
    WKContextTerminateServiceWorkers(platformContext());
}

#if !PLATFORM(COCOA)
void TestController::platformWillRunTest(const TestInvocation&)
{
}

void TestController::platformInitializeDataStore(WKPageConfigurationRef, const TestOptions&)
{
    m_websiteDataStore = defaultWebsiteDataStore();
}

void TestController::platformCreateWebView(WKPageConfigurationRef configuration, const TestOptions& options)
{
    m_mainWebView = makeUnique<PlatformWebView>(configuration, options);
}

PlatformWebView* TestController::platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef configuration, const TestOptions& options)
{
    return new PlatformWebView(configuration, options);
}

WKContextRef TestController::platformAdjustContext(WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
    WKContextSetPrimaryWebsiteDataStore(context, defaultWebsiteDataStore());
    return context;
}

bool TestController::platformResetStateToConsistentValues(const TestOptions&)
{
    return true;
}

unsigned TestController::imageCountInGeneralPasteboard() const
{
    return 0;
}

void TestController::removeAllSessionCredentials()
{
}

void TestController::getAllStorageAccessEntries()
{
}

struct LoadedThirdPartyDomainsCallbackContext {
    explicit LoadedThirdPartyDomainsCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    bool done { false };
    Vector<String> result;
};

static void loadedThirdPartyDomainsCallback(WKArrayRef domains, void* userData)
{
    auto* context = static_cast<LoadedThirdPartyDomainsCallbackContext*>(userData);
    context->done = true;

    if (domains) {
        auto size = WKArrayGetSize(domains);
        context->result.reserveInitialCapacity(size);
        for (size_t index = 0; index < size; ++index)
            context->result.uncheckedAppend(toWTFString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(domains, index))));
    }

    context->testController.notifyDone();
}

void TestController::loadedThirdPartyDomains()
{
    LoadedThirdPartyDomainsCallbackContext context(*this);
    WKPageLoadedThirdPartyDomains(m_mainWebView->page(), loadedThirdPartyDomainsCallback, &context);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didReceiveLoadedThirdPartyDomains(WTFMove(context.result));
}

void TestController::clearLoadedThirdPartyDomains()
{
    WKPageClearLoadedThirdPartyDomains(m_mainWebView->page());
}

#endif // !PLATFORM(COCOA)

struct ClearServiceWorkerRegistrationsCallbackContext {
    explicit ClearServiceWorkerRegistrationsCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    bool done { false };
};

static void clearServiceWorkerRegistrationsCallback(void* userData)
{
    auto* context = static_cast<ClearServiceWorkerRegistrationsCallbackContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

void TestController::clearServiceWorkerRegistrations()
{
    ClearServiceWorkerRegistrationsCallbackContext context(*this);

    WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(websiteDataStore(), &context, clearServiceWorkerRegistrationsCallback);
    runUntil(context.done, noTimeout);
}

struct ClearDOMCacheCallbackContext {
    explicit ClearDOMCacheCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    bool done { false };
};

static void clearDOMCacheCallback(void* userData)
{
    auto* context = static_cast<ClearDOMCacheCallbackContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

void TestController::clearDOMCache(WKStringRef origin)
{
    ClearDOMCacheCallbackContext context(*this);

    auto cacheOrigin = adoptWK(WKSecurityOriginCreateFromString(origin));
    WKWebsiteDataStoreRemoveFetchCacheForOrigin(websiteDataStore(), cacheOrigin.get(), &context, clearDOMCacheCallback);
    runUntil(context.done, noTimeout);
}

void TestController::clearDOMCaches()
{
    ClearDOMCacheCallbackContext context(*this);

    WKWebsiteDataStoreRemoveAllFetchCaches(websiteDataStore(), &context, clearDOMCacheCallback);
    runUntil(context.done, noTimeout);
}

struct StorageVoidCallbackContext {
    explicit StorageVoidCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    bool done { false };
};

static void StorageVoidCallback(void* userData)
{
    auto* context = static_cast<StorageVoidCallbackContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

void TestController::clearIndexedDatabases()
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreRemoveAllIndexedDatabases(websiteDataStore(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);
}

void TestController::clearLocalStorage()
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreRemoveLocalStorage(websiteDataStore(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);

    StorageVoidCallbackContext legacyContext(*this);
    WKContextClearLegacyPrivateBrowsingLocalStorage(platformContext(), &legacyContext, StorageVoidCallback);
    runUntil(legacyContext.done, noTimeout);
}

void TestController::syncLocalStorage()
{
    StorageVoidCallbackContext context(*this);
    WKContextSyncLocalStorage(platformContext(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);
}

void TestController::resetQuota()
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreResetQuota(TestController::websiteDataStore(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);
}

struct FetchCacheOriginsCallbackContext {
    FetchCacheOriginsCallbackContext(TestController& controller, WKStringRef origin)
        : testController(controller)
        , origin(origin)
    {
    }

    TestController& testController;
    WKStringRef origin;

    bool done { false };
    bool result { false };
};

static void fetchCacheOriginsCallback(WKArrayRef origins, void* userData)
{
    auto* context = static_cast<FetchCacheOriginsCallbackContext*>(userData);
    context->done = true;

    auto size = WKArrayGetSize(origins);
    for (size_t index = 0; index < size && !context->result; ++index) {
        WKSecurityOriginRef securityOrigin = reinterpret_cast<WKSecurityOriginRef>(WKArrayGetItemAtIndex(origins, index));
        if (WKStringIsEqual(context->origin, adoptWK(WKSecurityOriginCopyToString(securityOrigin)).get()))
            context->result = true;
    }
    context->testController.notifyDone();
}

bool TestController::hasDOMCache(WKStringRef origin)
{
    FetchCacheOriginsCallbackContext context(*this, origin);
    WKWebsiteDataStoreGetFetchCacheOrigins(websiteDataStore(), &context, fetchCacheOriginsCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

struct FetchCacheSizeForOriginCallbackContext {
    explicit FetchCacheSizeForOriginCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;

    bool done { false };
    uint64_t result { 0 };
};

static void fetchCacheSizeForOriginCallback(uint64_t size, void* userData)
{
    auto* context = static_cast<FetchCacheSizeForOriginCallbackContext*>(userData);
    context->done = true;
    context->result = size;
    context->testController.notifyDone();
}

uint64_t TestController::domCacheSize(WKStringRef origin)
{
    FetchCacheSizeForOriginCallbackContext context(*this);
    WKWebsiteDataStoreGetFetchCacheSizeForOrigin(websiteDataStore(), origin, &context, fetchCacheSizeForOriginCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

#if !PLATFORM(COCOA)
void TestController::setAllowStorageQuotaIncrease(bool)
{
    // FIXME: To implement.
}

bool TestController::isDoingMediaCapture() const
{
    return false;
}

#endif

struct ResourceStatisticsCallbackContext {
    explicit ResourceStatisticsCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    bool done { false };
    bool result { false };
    WKRetainPtr<WKStringRef> resourceLoadStatisticsRepresentation;
};
    
static void resourceStatisticsStringResultCallback(WKStringRef resourceLoadStatisticsRepresentation, void* userData)
{
    auto* context = static_cast<ResourceStatisticsCallbackContext*>(userData);
    context->resourceLoadStatisticsRepresentation = resourceLoadStatisticsRepresentation;
    context->done = true;
    context->testController.notifyDone();
}

static void resourceStatisticsVoidResultCallback(void* userData)
{
    auto* context = static_cast<ResourceStatisticsCallbackContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

static void resourceStatisticsBooleanResultCallback(bool result, void* userData)
{
    auto* context = static_cast<ResourceStatisticsCallbackContext*>(userData);
    context->result = result;
    context->done = true;
    context->testController.notifyDone();
}

void TestController::clearStatisticsDataForDomain(WKStringRef domain)
{
    ResourceStatisticsCallbackContext context(*this);

    WKWebsiteDataStoreRemoveITPDataForDomain(websiteDataStore(), domain, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

bool TestController::doesStatisticsDomainIDExistInDatabase(unsigned domainID)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(websiteDataStore(), domainID, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::setStatisticsEnabled(bool value)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(websiteDataStore(), value);
}

bool TestController::isStatisticsEphemeral()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsEphemeral(websiteDataStore(), &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::setStatisticsDebugMode(bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(websiteDataStore(), value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetStatisticsDebugMode();
}

void TestController::setStatisticsPrevalentResourceForDebugMode(WKStringRef hostName)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(websiteDataStore(), hostName, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetPrevalentResourceForDebugMode();
}

void TestController::setStatisticsLastSeen(WKStringRef host, double seconds)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsLastSeen(websiteDataStore(), host, seconds, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetLastSeen();
}

void TestController::setStatisticsMergeStatistic(WKStringRef host, WKStringRef topFrameDomain1, WKStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, int dataRecordsRemoved)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsMergeStatistic(websiteDataStore(), host, topFrameDomain1, topFrameDomain2, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didMergeStatistic();
}

void TestController::setStatisticsExpiredStatistic(WKStringRef host, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsExpiredStatistic(websiteDataStore(), host, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetExpiredStatistic();
}

void TestController::setStatisticsPrevalentResource(WKStringRef host, bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsPrevalentResource(websiteDataStore(), host, value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetPrevalentResource();
}

void TestController::setStatisticsVeryPrevalentResource(WKStringRef host, bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(websiteDataStore(), host, value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetVeryPrevalentResource();
}
    
String TestController::dumpResourceLoadStatistics()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreDumpResourceLoadStatistics(websiteDataStore(), &context, resourceStatisticsStringResultCallback);
    runUntil(context.done, noTimeout);
    return toWTFString(context.resourceLoadStatisticsRepresentation.get());
}

bool TestController::isStatisticsPrevalentResource(WKStringRef host)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsPrevalentResource(websiteDataStore(), host, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

bool TestController::isStatisticsVeryPrevalentResource(WKStringRef host)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(websiteDataStore(), host, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

bool TestController::isStatisticsRegisteredAsSubresourceUnder(WKStringRef subresourceHost, WKStringRef topFrameHost)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(websiteDataStore(), subresourceHost, topFrameHost, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

bool TestController::isStatisticsRegisteredAsSubFrameUnder(WKStringRef subFrameHost, WKStringRef topFrameHost)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(websiteDataStore(), subFrameHost, topFrameHost, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

bool TestController::isStatisticsRegisteredAsRedirectingTo(WKStringRef hostRedirectedFrom, WKStringRef hostRedirectedTo)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(websiteDataStore(), hostRedirectedFrom, hostRedirectedTo, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::setStatisticsHasHadUserInteraction(WKStringRef host, bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(websiteDataStore(), host, value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetHasHadUserInteraction();
}

bool TestController::isStatisticsHasHadUserInteraction(WKStringRef host)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(websiteDataStore(), host, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

bool TestController::isStatisticsOnlyInDatabaseOnce(WKStringRef subHost, WKStringRef topHost)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(websiteDataStore(), subHost, topHost, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::setStatisticsGrandfathered(WKStringRef host, bool value)
{
    WKWebsiteDataStoreSetStatisticsGrandfathered(websiteDataStore(), host, value);
}

bool TestController::isStatisticsGrandfathered(WKStringRef host)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreIsStatisticsGrandfathered(websiteDataStore(), host, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::setUseITPDatabase(bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetUseITPDatabase(websiteDataStore(), value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::setStatisticsSubframeUnderTopFrameOrigin(WKStringRef host, WKStringRef topFrameHost)
{
    WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(websiteDataStore(), host, topFrameHost);
}

void TestController::setStatisticsSubresourceUnderTopFrameOrigin(WKStringRef host, WKStringRef topFrameHost)
{
    WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(websiteDataStore(), host, topFrameHost);
}

void TestController::setStatisticsSubresourceUniqueRedirectTo(WKStringRef host, WKStringRef hostRedirectedTo)
{
    WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(websiteDataStore(), host, hostRedirectedTo);
}

void TestController::setStatisticsSubresourceUniqueRedirectFrom(WKStringRef host, WKStringRef hostRedirectedFrom)
{
    WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(websiteDataStore(), host, hostRedirectedFrom);
}

void TestController::setStatisticsTopFrameUniqueRedirectTo(WKStringRef host, WKStringRef hostRedirectedTo)
{
    WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(websiteDataStore(), host, hostRedirectedTo);
}

void TestController::setStatisticsTopFrameUniqueRedirectFrom(WKStringRef host, WKStringRef hostRedirectedFrom)
{
    WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(websiteDataStore(), host, hostRedirectedFrom);
}

void TestController::setStatisticsCrossSiteLoadWithLinkDecoration(WKStringRef fromHost, WKStringRef toHost)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(websiteDataStore(), fromHost, toHost, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::setStatisticsTimeToLiveUserInteraction(double seconds)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(websiteDataStore(), seconds, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::statisticsProcessStatisticsAndDataRecords()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(websiteDataStore(), &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::statisticsUpdateCookieBlocking()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsUpdateCookieBlocking(websiteDataStore(), &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetBlockCookiesForHost();
}

void TestController::statisticsSubmitTelemetry()
{
    WKWebsiteDataStoreStatisticsSubmitTelemetry(websiteDataStore());
}

void TestController::setStatisticsNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(websiteDataStore(), value);
}

void TestController::setStatisticsIsRunningTest(bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsIsRunningTest(websiteDataStore(), value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(websiteDataStore(), value);
}

void TestController::setStatisticsNotifyPagesWhenTelemetryWasCaptured(bool value)
{
    WKWebsiteDataStoreSetStatisticsNotifyPagesWhenTelemetryWasCaptured(websiteDataStore(), value);
}

void TestController::setStatisticsMinimumTimeBetweenDataRecordsRemoval(double seconds)
{
    WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(websiteDataStore(), seconds);
}

void TestController::setStatisticsGrandfatheringTime(double seconds)
{
    WKWebsiteDataStoreSetStatisticsGrandfatheringTime(websiteDataStore(), seconds);
}

void TestController::setStatisticsMaxStatisticsEntries(unsigned entries)
{
    WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(websiteDataStore(), entries);
}

void TestController::setStatisticsPruneEntriesDownTo(unsigned entries)
{
    WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(websiteDataStore(), entries);
}

void TestController::statisticsClearInMemoryAndPersistentStore()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(websiteDataStore(), &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didClearStatisticsInMemoryAndPersistentStore();
}

void TestController::statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(websiteDataStore(), hours, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didClearStatisticsInMemoryAndPersistentStore();
}

void TestController::statisticsClearThroughWebsiteDataRemoval()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(websiteDataStore(), &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didClearStatisticsThroughWebsiteDataRemoval();
}

void TestController::statisticsDeleteCookiesForHost(WKStringRef host, bool includeHttpOnlyCookies)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(websiteDataStore(), host, includeHttpOnlyCookies, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

bool TestController::isStatisticsHasLocalStorage(WKStringRef host)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsHasLocalStorage(websiteDataStore(), host, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::setStatisticsCacheMaxAgeCap(double seconds)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(websiteDataStore(), seconds, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

bool TestController::hasStatisticsIsolatedSession(WKStringRef host)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsHasIsolatedSession(websiteDataStore(), host, &context, resourceStatisticsBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::setStatisticsShouldDowngradeReferrer(bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(websiteDataStore(), value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetShouldDowngradeReferrer();
}

void TestController::setStatisticsShouldBlockThirdPartyCookies(bool value, bool onlyOnSitesWithoutUserInteraction)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(websiteDataStore(), value, onlyOnSitesWithoutUserInteraction, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetShouldBlockThirdPartyCookies();
}

void TestController::setStatisticsFirstPartyWebsiteDataRemovalMode(bool value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(websiteDataStore(), value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetFirstPartyWebsiteDataRemovalMode();
}

void TestController::setStatisticsToSameSiteStrictCookies(WKStringRef hostName)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(websiteDataStore(), hostName, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetToSameSiteStrictCookies();
}

struct AppBoundDomainsCallbackContext {
    explicit AppBoundDomainsCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    bool done { false };
    TestController& testController;
};

static void didSetAppBoundDomainsCallback(void* callbackContext)
{
    auto* context = static_cast<AppBoundDomainsCallbackContext*>(callbackContext);
    context->done = true;
}

void TestController::setAppBoundDomains(WKArrayRef originURLs)
{
    AppBoundDomainsCallbackContext context(*this);
    WKWebsiteDataStoreSetAppBoundDomainsForTesting(originURLs, &context, didSetAppBoundDomainsCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didSetAppBoundDomains();
}

void TestController::statisticsResetToConsistentState()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsResetToConsistentState(websiteDataStore(), &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
    m_currentInvocation->didResetStatisticsToConsistentState();
}

void TestController::addMockMediaDevice(WKStringRef persistentID, WKStringRef label, WKStringRef type)
{
    WKAddMockMediaDevice(platformContext(), persistentID, label, type);
}

void TestController::clearMockMediaDevices()
{
    WKClearMockMediaDevices(platformContext());
}

void TestController::removeMockMediaDevice(WKStringRef persistentID)
{
    WKRemoveMockMediaDevice(platformContext(), persistentID);
}

void TestController::resetMockMediaDevices()
{
    WKResetMockMediaDevices(platformContext());
}

void TestController::setMockCameraOrientation(uint64_t orientation)
{
    WKPageSetMockCameraOrientation(m_mainWebView->page(), orientation);
}

bool TestController::isMockRealtimeMediaSourceCenterEnabled() const
{
    return WKPageIsMockRealtimeMediaSourceCenterEnabled(m_mainWebView->page());
}

struct InAppBrowserPrivacyCallbackContext {
    explicit InAppBrowserPrivacyCallbackContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    bool done { false };
    bool result { false };
};

static void inAppBrowserPrivacyBooleanResultCallback(bool result, void* userData)
{
    auto* context = static_cast<InAppBrowserPrivacyCallbackContext*>(userData);
    context->result = result;
    context->done = true;
    context->testController.notifyDone();
}

static void inAppBrowserPrivacyVoidResultCallback(void* userData)
{
    auto* context = static_cast<InAppBrowserPrivacyCallbackContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

bool TestController::hasAppBoundSession()
{
    InAppBrowserPrivacyCallbackContext context(*this);
    WKWebsiteDataStoreHasAppBoundSession(TestController::websiteDataStore(), &context, inAppBrowserPrivacyBooleanResultCallback);
    runUntil(context.done, noTimeout);
    return context.result;
}

void TestController::clearAppBoundSession()
{
    InAppBrowserPrivacyCallbackContext context(*this);
    WKWebsiteDataStoreClearAppBoundSession(TestController::websiteDataStore(), &context, inAppBrowserPrivacyVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::reinitializeAppBoundDomains()
{
    WKWebsiteDataStoreReinitializeAppBoundDomains(TestController::websiteDataStore());
}

void TestController::updateBundleIdentifierInNetworkProcess(const String& bundleIdentifier)
{
    InAppBrowserPrivacyCallbackContext context(*this);
    WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(TestController::websiteDataStore(), adoptWK(WKStringCreateWithUTF8CString(bundleIdentifier.utf8().data())).get(), &context, inAppBrowserPrivacyVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::clearBundleIdentifierInNetworkProcess()
{
    InAppBrowserPrivacyCallbackContext context(*this);
    WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(TestController::websiteDataStore(), &context, inAppBrowserPrivacyVoidResultCallback);
    runUntil(context.done, noTimeout);
}

#if !PLATFORM(COCOA)
void TestController::platformAddTestOptions(TestOptions&) const
{
}

void TestController::injectUserScript(WKStringRef)
{
}

void TestController::addTestKeyToKeychain(const String&, const String&, const String&)
{
}

void TestController::cleanUpKeychain(const String&, const String&)
{
}

bool TestController::keyExistsInKeychain(const String&, const String&)
{
    return false;
}

void TestController::installCustomMenuAction(const String&, bool)
{
}

void TestController::setAllowedMenuActions(const Vector<String>&)
{
}

#endif

void TestController::sendDisplayConfigurationChangedMessageForTesting()
{
    WKSendDisplayConfigurationChangedMessageForTesting(platformContext());
}

void TestController::setServiceWorkerFetchTimeoutForTesting(double seconds)
{
    WKContextSetServiceWorkerFetchTimeoutForTesting(platformContext(), seconds);
}

struct AdClickAttributionStringResultCallbackContext {
    explicit AdClickAttributionStringResultCallbackContext(TestController& controller)
        : testController(controller)
    {
    }
    
    TestController& testController;
    bool done { false };
    WKRetainPtr<WKStringRef> adClickAttributionRepresentation;
};

static void adClickAttributionStringResultCallback(WKStringRef adClickAttributionRepresentation, void* userData)
{
    auto* context = static_cast<AdClickAttributionStringResultCallbackContext*>(userData);
    context->adClickAttributionRepresentation = adClickAttributionRepresentation;
    context->done = true;
    context->testController.notifyDone();
}

String TestController::dumpAdClickAttribution()
{
    AdClickAttributionStringResultCallbackContext callbackContext(*this);
    WKPageDumpAdClickAttribution(m_mainWebView->page(), adClickAttributionStringResultCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
    return toWTFString(callbackContext.adClickAttributionRepresentation.get());
}

struct AdClickAttributionVoidCallbackContext {
    explicit AdClickAttributionVoidCallbackContext(TestController& controller)
        : testController(controller)
    {
    }
    
    TestController& testController;
    bool done { false };
};

static void adClickAttributionVoidCallback(void* userData)
{
    auto* context = static_cast<AdClickAttributionVoidCallbackContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

void TestController::clearAdClickAttribution()
{
    AdClickAttributionVoidCallbackContext callbackContext(*this);
    WKPageClearAdClickAttribution(m_mainWebView->page(), adClickAttributionVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::clearAdClickAttributionsThroughWebsiteDataRemoval()
{
    AdClickAttributionVoidCallbackContext callbackContext(*this);
    WKWebsiteDataStoreClearAdClickAttributionsThroughWebsiteDataRemoval(websiteDataStore(), &callbackContext, adClickAttributionVoidCallback);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setAdClickAttributionOverrideTimerForTesting(bool value)
{
    AdClickAttributionVoidCallbackContext callbackContext(*this);
    WKPageSetAdClickAttributionOverrideTimerForTesting(m_mainWebView->page(), value, adClickAttributionVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setAdClickAttributionConversionURLForTesting(WKURLRef url)
{
    AdClickAttributionVoidCallbackContext callbackContext(*this);
    WKPageSetAdClickAttributionConversionURLForTesting(m_mainWebView->page(), url, adClickAttributionVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::markAdClickAttributionsAsExpiredForTesting()
{
    AdClickAttributionVoidCallbackContext callbackContext(*this);
    WKPageMarkAdClickAttributionsAsExpiredForTesting(m_mainWebView->page(), adClickAttributionVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

} // namespace WTR
