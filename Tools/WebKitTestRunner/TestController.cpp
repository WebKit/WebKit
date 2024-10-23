/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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

#include "DictionaryFunctions.h"
#include "EventSenderProxy.h"
#include "Options.h"
#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestCommand.h"
#include "TestInvocation.h"
#include "WebCoreTestSupport.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKAuthenticationChallenge.h>
#include <WebKit/WKAuthenticationDecisionListener.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKDownloadClient.h>
#include <WebKit/WKDownloadRef.h>
#include <WebKit/WKFrameHandleRef.h>
#include <WebKit/WKFrameInfoRef.h>
#include <WebKit/WKHTTPCookieStoreRef.h>
#include <WebKit/WKIconDatabase.h>
#include <WebKit/WKMediaKeySystemPermissionCallback.h>
#include <WebKit/WKMessageListener.h>
#include <WebKit/WKMockMediaDevice.h>
#include <WebKit/WKNavigationActionRef.h>
#include <WebKit/WKNavigationResponseRef.h>
#include <WebKit/WKNotification.h>
#include <WebKit/WKNotificationManager.h>
#include <WebKit/WKNotificationPermissionRequest.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKOpenPanelResultListener.h>
#include <WebKit/WKPageInjectedBundleClient.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKPluginInformation.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKProtectionSpace.h>
#include <WebKit/WKQueryPermissionResultCallback.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKSpeechRecognitionPermissionCallback.h>
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
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/UUID.h>
#include <wtf/UniqueArray.h>
#include <wtf/UniqueRef.h>
#include <wtf/WTFProcess.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include <WebKit/WKContextPrivateMac.h>
#include <WebKit/WKPagePrivateMac.h>
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include <WebKit/WKContextConfigurationGlib.h>
#endif

#if PLATFORM(WIN)
#include <direct.h>
#include <shlwapi.h>
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

void TestController::navigationDidBecomeDownloadShared(WKDownloadRef download, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidStart(download);

    WKDownloadClientV0 client {
        { 0, clientInfo },
        TestController::downloadDidReceiveServerRedirectToURL,
        TestController::downloadDidReceiveAuthenticationChallenge,
        TestController::decideDestinationWithSuggestedFilename,
        TestController::downloadDidWriteData,
        TestController::downloadDidFinish,
        TestController::downloadDidFail
    };

    WKDownloadSetClient(download, &client.base);
}

void TestController::navigationActionDidBecomeDownload(WKPageRef, WKNavigationActionRef, WKDownloadRef download, const void* clientInfo)
{
    navigationDidBecomeDownloadShared(download, clientInfo);
}

void TestController::navigationResponseDidBecomeDownload(WKPageRef, WKNavigationResponseRef, WKDownloadRef download, const void* clientInfo)
{
    navigationDidBecomeDownloadShared(download, clientInfo);
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

    auto allowedMIMETypes = adoptWK(WKOpenPanelParametersCopyAllowedMIMETypes(parameters));

    if (WKOpenPanelParametersGetAllowsMultipleFiles(parameters)) {
        WKOpenPanelResultListenerChooseFiles(resultListenerRef, fileURLs, allowedMIMETypes.get());
        return;
    }

    WKOpenPanelResultListenerChooseFiles(resultListenerRef, adoptWK(WKArrayCreate(&firstItem, 1)).get(), allowedMIMETypes.get());
}

void TestController::runModal(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    TestController::singleton().mainWebView()->setWindowIsKey(false);
    runModal(view);
    TestController::singleton().mainWebView()->setWindowIsKey(true);
}

void TestController::closeOtherPage(WKPageRef page, const void* clientInfo)
{
    PlatformWebView* view = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    TestController::singleton().closeOtherPage(page, view);
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
}

static bool shouldAllowDeviceOrientationAndMotionAccess(WKPageRef, WKSecurityOriginRef origin, WKFrameInfoRef frame, const void*)
{
    return TestController::singleton().handleDeviceOrientationAndMotionAccessRequest(origin, frame);
}

// A placeholder to tell WebKit the client is WebKitTestRunner.
static void runWebAuthenticationPanel()
{
}

void TestController::setIsSpeechRecognitionPermissionGranted(bool granted)
{
    m_isSpeechRecognitionPermissionGranted = granted;
}

static void decidePolicyForMediaKeySystemPermissionRequest(WKPageRef, WKSecurityOriginRef, WKStringRef, WKMediaKeySystemPermissionCallbackRef callback)
{
    TestController::singleton().completeMediaKeySystemPermissionCheck(callback);
}

void TestController::completeMediaKeySystemPermissionCheck(WKMediaKeySystemPermissionCallbackRef callback)
{
    WKMediaKeySystemPermissionCallbackComplete(callback, m_isMediaKeySystemPermissionGranted);
}

void TestController::setIsMediaKeySystemPermissionGranted(bool granted)
{
    m_isMediaKeySystemPermissionGranted = granted;
}

static void queryPermission(WKStringRef string, WKSecurityOriginRef securityOrigin, WKQueryPermissionResultCallbackRef callback)
{
    TestController::singleton().handleQueryPermission(string, securityOrigin, callback);
}

void TestController::handleQueryPermission(WKStringRef string, WKSecurityOriginRef securityOrigin, WKQueryPermissionResultCallbackRef callback)
{
    if (toWTFString(string) == "notifications"_s) {
        auto permissionState = m_webNotificationProvider.permissionState(securityOrigin);
        if (permissionState) {
            if (permissionState.value())
                WKQueryPermissionResultCallbackCompleteWithGranted(callback);
            else
                WKQueryPermissionResultCallbackCompleteWithDenied(callback);
            return;
        }
    }

    if (toWTFString(string) == "geolocation"_s) {
        m_geolocationPermissionQueryOrigins.add(toWTFString(adoptWK(WKSecurityOriginCopyToString(securityOrigin))));

        if (m_isGeolocationPermissionSet) {
            if (m_isGeolocationPermissionAllowed)
                WKQueryPermissionResultCallbackCompleteWithGranted(callback);
            else
                WKQueryPermissionResultCallbackCompleteWithDenied(callback);
            return;
        }
    }

    if (toWTFString(string) == "screen-wake-lock"_s) {
        if (m_screenWakeLockPermission) {
            if (*m_screenWakeLockPermission)
                WKQueryPermissionResultCallbackCompleteWithGranted(callback);
            else
                WKQueryPermissionResultCallbackCompleteWithDenied(callback);
            return;
        }
    }

    WKQueryPermissionResultCallbackCompleteWithPrompt(callback);
}

#if PLATFORM(IOS) || PLATFORM(VISION)
static void lockScreenOrientationCallback(WKPageRef, WKScreenOrientationType orientation)
{
    TestController::singleton().lockScreenOrientation(orientation);
}

static void unlockScreenOrientationCallback(WKPageRef)
{
    TestController::singleton().unlockScreenOrientation();
}
#endif

void TestController::closeOtherPage(WKPageRef page, PlatformWebView* view)
{
    WKPageClose(page);
    auto index = m_auxiliaryWebViews.findIf([view](auto& auxiliaryWebView) { return auxiliaryWebView.ptr() == view; });
    if (index != notFound)
        m_auxiliaryWebViews.remove(index);
}

WKPageRef TestController::createOtherPage(WKPageRef, WKPageConfigurationRef configuration, WKNavigationActionRef navigationAction, WKWindowFeaturesRef windowFeatures, const void *clientInfo)
{
    PlatformWebView* parentView = static_cast<PlatformWebView*>(const_cast<void*>(clientInfo));
    return TestController::singleton().createOtherPage(parentView, configuration, navigationAction, windowFeatures);
}

WKPageRef TestController::createOtherPage(PlatformWebView* parentView, WKPageConfigurationRef configuration, WKNavigationActionRef navigationAction, WKWindowFeaturesRef windowFeatures)
{
    auto* platformWebView = createOtherPlatformWebView(parentView, configuration, navigationAction, windowFeatures);
    if (!platformWebView)
        return nullptr;

    auto* page = platformWebView->page();
    WKRetain(page);
    return page;
}

PlatformWebView* TestController::createOtherPlatformWebView(PlatformWebView* parentView, WKPageConfigurationRef configuration, WKNavigationActionRef, WKWindowFeaturesRef)
{
    m_currentInvocation->willCreateNewPage();

    // The test called testRunner.preventPopupWindows() to prevent opening new windows.
    if (!m_currentInvocation->canOpenWindows())
        return nullptr;

    m_createdOtherPage = true;

    auto options = parentView ? parentView->options() : m_mainWebView->options();
    auto view = platformCreateOtherPage(parentView, configuration, options);
    WKPageRef newPage = view->page();

    view->resizeTo(800, 600);

    WKPageUIClientV8 otherPageUIClient = {
        { 8, view.ptr() },
        nullptr, // createNewPage_deprecatedForUseWithV0
        nullptr, // showPage
        closeOtherPage,
        nullptr, // takeFocus
        focus,
        unfocus,
        nullptr, // runJavaScriptAlert_deprecatedForUseWithV0
        nullptr, // runJavaScriptAlert_deprecatedForUseWithV0
        nullptr, // runJavaScriptAlert_deprecatedForUseWithV0
        nullptr, // setStatusText
        nullptr, // mouseDidMoveOverElement_deprecatedForUseWithV0
        nullptr, // missingPluginButtonClicked
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
        getWindowFrame,
        setWindowFrame,
        runBeforeUnloadConfirmPanel,
        nullptr, // didDraw
        nullptr, // pageDidScroll
        nullptr, // exceededDatabaseQuota
        runOpenPanel,
        decidePolicyForGeolocationPermissionRequest,
        nullptr, // headerHeight
        nullptr, // footerHeight
        nullptr, // drawHeader
        nullptr, // drawFooter
        printFrame,
        runModal,
        nullptr, // didCompleteRubberBandForMainFrame
        nullptr, // saveDataToFileInDownloadsFolder
        nullptr, // shouldInterruptJavaScript
        nullptr, // createNewPage_deprecatedForUseWithV1
        nullptr, // mouseDidMoveOverElement
        nullptr, // decidePolicyForNotificationPermissionRequest
        nullptr, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        nullptr, // showColorPicker
        nullptr, // hideColorPicker
        nullptr, // unavailablePluginButtonClicked
        nullptr, // pinnedStateDidChange
        nullptr, // didBeginTrackingPotentialLongMousePress
        nullptr, // didRecognizeLongMousePress
        nullptr, // didCancelTrackingPotentialLongMousePress
        nullptr, // isPlayingAudioDidChange
        decidePolicyForUserMediaPermissionRequest,
        nullptr, // didClickAutofillButton
        nullptr, // runJavaScriptAlert
        nullptr, // runJavaScriptConfirm
        nullptr, // runJavaScriptPrompt
        nullptr, // unused5
        createOtherPage,
        runJavaScriptAlert,
        nullptr, // runJavaScriptConfirm
        nullptr, // runJavaScriptPrompt
        checkUserMediaPermissionForOrigin,
        nullptr, // runBeforeUnloadConfirmPanel
        nullptr, // fullscreenMayReturnToInline
        requestPointerLock,
        nullptr, // didLosePointerLock
    };
    WKPageSetPageUIClient(newPage, &otherPageUIClient.base);
    
    WKPageNavigationClientV3 pageNavigationClient = {
        { 3, &TestController::singleton() },
        decidePolicyForNavigationAction,
        decidePolicyForNavigationResponse,
        decidePolicyForPluginLoad,
        nullptr, // didStartProvisionalNavigation
        didReceiveServerRedirectForProvisionalNavigation,
        didFailProvisionalNavigation,
        nullptr, // didCommitNavigation
        nullptr, // didFinishNavigation
        nullptr, // didFailNavigation
        nullptr, // didFailProvisionalLoadInSubframe
        nullptr, // didFinishDocumentLoad
        nullptr, // didSameDocumentNavigation
        nullptr, // renderingProgressDidChange
        canAuthenticateAgainstProtectionSpace,
        didReceiveAuthenticationChallenge,
        nullptr, // webProcessDidCrash
        copyWebCryptoMasterKey,
        didBeginNavigationGesture,
        willEndNavigationGesture,
        didEndNavigationGesture,
        didRemoveNavigationGestureSnapshot,
        webProcessDidTerminate, // webProcessDidTerminate
        nullptr, // contentRuleListNotification
        nullptr, // copySignedPublicKeyAndChallengeString
        navigationActionDidBecomeDownload,
        navigationResponseDidBecomeDownload,
        nullptr // contextMenuDidCreateDownload
    };
    WKPageSetPageNavigationClient(newPage, &pageNavigationClient.base);

    WKPageInjectedBundleClientV1 injectedBundleClient = {
        { 1, this },
        didReceivePageMessageFromInjectedBundle,
        nullptr,
        didReceiveSynchronousPageMessageFromInjectedBundleWithListener,
        didReceiveAsyncPageMessageFromInjectedBundleWithListener
    };
    WKPageSetPageInjectedBundleClient(newPage, &injectedBundleClient.base);

    view->didInitializeClients();

    TestController::singleton().updateWindowScaleForTest(view.ptr(), *TestController::singleton().m_currentInvocation);

    PlatformWebView* viewToReturn = view.ptr();
    m_auxiliaryWebViews.append(WTFMove(view));
    return viewToReturn;
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

    JSC::initialize();
    WTF::initializeMainThread();
    WTF::setProcessPrivileges(allPrivileges());
    WebCoreTestSupport::initializeNames();
    WebCoreTestSupport::populateJITOperations();

    Options options;
    OptionsHandler optionsHandler(options);

    if (argc < 2) {
        optionsHandler.printHelp();
        exitProcess(1);
    }
    if (!optionsHandler.parse(argc, argv))
        exitProcess(1);

    platformInitialize(options);

    m_useWaitToDumpWatchdogTimer = options.useWaitToDumpWatchdogTimer;
    m_forceNoTimeout = options.forceNoTimeout;
    m_verbose = options.verbose;
    m_gcBetweenTests = options.gcBetweenTests;
    m_shouldDumpPixelsForAllTests = options.shouldDumpPixelsForAllTests;
    m_forceComplexText = options.forceComplexText;
    m_paths = options.paths;
    m_allowedHosts = options.allowedHosts;
    m_localhostAliases = options.localhostAliases;
    m_checkForWorldLeaks = options.checkForWorldLeaks;
    m_allowAnyHTTPSCertificateForAllowedHosts = options.allowAnyHTTPSCertificateForAllowedHosts;
    m_enableAllExperimentalFeatures = options.enableAllExperimentalFeatures;
    m_globalFeatures = std::move(options.features);
#if PLATFORM(WPE)
    m_useWPEPlatformAPI = options.useWPEPlatformAPI;
#endif

    /* localhost is implicitly allowed and so should aliases to it. */
    for (const auto& alias : m_localhostAliases)
        m_allowedHosts.insert(alias);

    m_usingServerMode = (m_paths.size() == 1 && m_paths[0] == "-");
    if (m_usingServerMode)
        m_printSeparators = true;
    else
        m_printSeparators = m_paths.size() > 1;

    initializeInjectedBundlePath();
    initializeTestPluginDirectory();

#if ENABLE(GAMEPAD)
    WebCoreTestSupport::installMockGamepadProvider();
#endif

    m_preferences = adoptWK(WKPreferencesCreate());
    m_eventSenderProxy = makeUnique<EventSenderProxy>(this);
}

WKRetainPtr<WKContextConfigurationRef> TestController::generateContextConfiguration(const TestOptions& options) const
{
    auto configuration = adoptWK(WKContextConfigurationCreate());
    WKContextConfigurationSetInjectedBundlePath(configuration.get(), injectedBundlePath());
    WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(configuration.get(), true);
    WKContextConfigurationSetIgnoreSynchronousMessagingTimeoutsForTesting(configuration.get(), options.ignoreSynchronousMessagingTimeouts());

    auto overrideLanguages = adoptWK(WKMutableArrayCreate());
    for (auto& language : options.overrideLanguages())
        WKArrayAppendItem(overrideLanguages.get(), toWK(language).get());
    WKContextConfigurationSetOverrideLanguages(configuration.get(), overrideLanguages.get());

    if (options.shouldEnableProcessSwapOnNavigation())
        WKContextConfigurationSetProcessSwapsOnNavigation(configuration.get(), true);

    WKContextConfigurationSetShouldConfigureJSCForTesting(configuration.get(), true);

#if PLATFORM(GTK) || PLATFORM(WPE)
    WKContextConfigurationSetDisableFontHintingForTesting(configuration.get(), true);
#endif

    return configuration;
}

void TestController::configureWebsiteDataStoreTemporaryDirectories(WKWebsiteDataStoreConfigurationRef configuration)
{
    if (const char* dumpRenderTreeTemp = libraryPathForTesting()) {
        String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);
        auto randomNumber = cryptographicallyRandomNumber<uint32_t>();

        WKWebsiteDataStoreConfigurationSetApplicationCacheDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "ApplicationCache"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetNetworkCacheDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "Cache"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetCacheStorageDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "CacheStorage"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetIndexedDBDatabaseDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "Databases"_s, pathSeparator, "IndexedDB"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetLocalStorageDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "LocalStorage"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetMediaKeysStorageDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "MediaKeys"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetResourceLoadStatisticsDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "ResourceLoadStatistics"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetServiceWorkerRegistrationDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "ServiceWorkers"_s, pathSeparator, randomNumber)).get());
        WKWebsiteDataStoreConfigurationSetGeneralStorageDirectory(configuration, toWK(makeString(temporaryFolder, pathSeparator, "Default"_s, pathSeparator, randomNumber)).get());
#if PLATFORM(WIN)
        WKWebsiteDataStoreConfigurationSetCookieStorageFile(configuration, toWK(makeString(temporaryFolder, pathSeparator, "cookies"_s, pathSeparator, randomNumber, pathSeparator, "cookiejar.db"_s)).get());
#endif
        WKWebsiteDataStoreConfigurationSetPerOriginStorageQuota(configuration, 400 * 1024);
        WKWebsiteDataStoreConfigurationSetNetworkCacheSpeculativeValidationEnabled(configuration, true);
        WKWebsiteDataStoreConfigurationSetStaleWhileRevalidateEnabled(configuration, true);
        WKWebsiteDataStoreConfigurationSetTestingSessionEnabled(configuration, true);
        WKWebsiteDataStoreConfigurationSetPCMMachServiceName(configuration, nullptr);
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
    if (!m_context || !m_mainWebView || !m_mainWebView->viewSupportsOptions(options)) {
        auto contextConfiguration = generateContextConfiguration(options);
        m_context = platformAdjustContext(adoptWK(WKContextCreateWithConfiguration(contextConfiguration.get())).get(), contextConfiguration.get());

        auto localhostAliases = adoptWK(WKMutableArrayCreate());
        for (const auto& alias : m_localhostAliases)
            WKArrayAppendItem(localhostAliases.get(), toWK(alias.c_str()).get());
        WKContextSetLocalhostAliases(m_context.get(), localhostAliases.get());

        m_geolocationProvider = makeUnique<GeolocationProviderMock>(m_context.get());

        if (const char* dumpRenderTreeTemp = libraryPathForTesting()) {
            String temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);

            // FIXME: This should be migrated to WKContextConfigurationRef.
            // Disable icon database to avoid fetching <http://127.0.0.1:8000/favicon.ico> and making tests flaky.
            // Invividual tests can enable it using testRunner.setIconDatabaseEnabled, although it's not currently supported in WebKitTestRunner.
            WKContextSetIconDatabasePath(m_context.get(), toWK(emptyString()).get());
        }

        WKContextSetCacheModel(m_context.get(), kWKCacheModelDocumentBrowser);
        WKContextSetDisableFontSubpixelAntialiasingForTesting(TestController::singleton().context(), true);

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

    WKContextClientV4 contextClient = {
        { 4, this },
        0, // plugInAutoStartOriginHashesChanged
        0, // networkProcessDidCrash,
        0, // plugInInformationBecameAvailable
        0, // copyWebCryptoMasterKey
        0, // serviceWorkerProcessDidCrash,
        0, // gpuProcessDidCrash
        networkProcessDidCrashWithDetails,
        serviceWorkerProcessDidCrashWithDetails,
        gpuProcessDidCrashWithDetails,
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
    WKNotificationManagerSetProvider(WKNotificationManagerGetSharedServiceWorkerNotificationManager(), &notificationKit.base);

    if (testPluginDirectory())
        WKContextSetAdditionalPluginsDirectory(m_context.get(), testPluginDirectory());

    if (m_forceComplexText)
        WKContextSetAlwaysUsesComplexTextCodePath(m_context.get(), true);

    auto pageConfiguration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(pageConfiguration.get(), m_context.get());
    WKPageConfigurationSetPreferences(pageConfiguration.get(), m_preferences.get());
    
    if (options.useEphemeralSession()) {
        auto ephemeralDataStore = adoptWK(WKWebsiteDataStoreCreateNonPersistentDataStore());
        WKPageConfigurationSetWebsiteDataStore(pageConfiguration.get(), ephemeralDataStore.get());
    }

    if (options.allowTestOnlyIPC())
        WKPageConfigurationSetAllowTestOnlyIPC(pageConfiguration.get(), true);

    m_userContentController = adoptWK(WKUserContentControllerCreate());
    WKPageConfigurationSetUserContentController(pageConfiguration.get(), userContentController());
    WKPageConfigurationSetPortsForUpgradingInsecureSchemeForTesting(pageConfiguration.get(), options.insecureUpgradePort(), options.secureUpgradePort());
    return pageConfiguration;
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
        return makeString(protocol, "://"_s, host, ':', port);

    return makeString(protocol, "://"_s, host);
}

bool TestController::grantNotificationPermission(WKStringRef originString)
{
    auto origin = adoptWK(WKSecurityOriginCreateFromString(originString));
    auto previousPermissionState = m_webNotificationProvider.permissionState(origin.get());

    m_webNotificationProvider.setPermission(toWTFString(originString), true);
    WKNotificationManagerProviderDidUpdateNotificationPolicy(WKNotificationManagerGetSharedServiceWorkerNotificationManager(), origin.get(), true);

    if (!previousPermissionState || !*previousPermissionState)
        WKPagePermissionChanged(toWK("notifications").get(), originString);

    return true;
}

bool TestController::denyNotificationPermission(WKStringRef originString)
{
    auto origin = adoptWK(WKSecurityOriginCreateFromString(originString));
    auto previousPermissionState = m_webNotificationProvider.permissionState(origin.get());

    m_webNotificationProvider.setPermission(toWTFString(originString), false);
    WKNotificationManagerProviderDidUpdateNotificationPolicy(WKNotificationManagerGetSharedServiceWorkerNotificationManager(), origin.get(), false);

    if (!previousPermissionState || *previousPermissionState)
        WKPagePermissionChanged(toWK("notifications").get(), originString);

    return true;
}

bool TestController::denyNotificationPermissionOnPrompt(WKStringRef originString)
{
    auto origin = adoptWK(WKSecurityOriginCreateFromString(originString));
    auto originName = originUserVisibleName(origin.get());
    m_notificationOriginsToDenyOnPrompt.add(originName);
    return true;
}

#if !PLATFORM(COCOA)
WKRetainPtr<WKStringRef> TestController::getBackgroundFetchIdentifier()
{
    return { };
}

void TestController::abortBackgroundFetch(WKStringRef)
{
}

void TestController::pauseBackgroundFetch(WKStringRef)
{
}

void TestController::resumeBackgroundFetch(WKStringRef)
{
}

void TestController::simulateClickBackgroundFetch(WKStringRef)
{
}
#endif

void TestController::createWebViewWithOptions(const TestOptions& options)
{
    auto applicationBundleIdentifier = options.applicationBundleIdentifier();
#if PLATFORM(COCOA)
    if (!applicationBundleIdentifier.empty()) {
        // The bundle identifier can only be set once per test, and is cleared between tests.
        RELEASE_ASSERT(!m_hasSetApplicationBundleIdentifier);
        setApplicationBundleIdentifier(applicationBundleIdentifier);
        m_hasSetApplicationBundleIdentifier = true;
    }
#endif

    auto configuration = generatePageConfiguration(options);
    platformInitializeDataStore(configuration.get(), options);

    // Some preferences (notably mock scroll bars setting) currently cannot be re-applied to an existing view, so we need to set them now.
    // FIXME: Migrate these preferences to WKContextConfigurationRef.
    resetPreferencesToConsistentValues(options);

    WKHTTPCookieStoreDeleteAllCookies(WKWebsiteDataStoreGetHTTPCookieStore(websiteDataStore()), nullptr, nullptr);

    platformCreateWebView(configuration.get(), options);
    WKPageUIClientV19 pageUIClient = {
        { 19, m_mainWebView.get() },
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
        options.shouldHandleRunOpenPanel() ? runOpenPanel : 0,
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
        0, // unused5
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
        runWebAuthenticationPanel,
        0,
        decidePolicyForMediaKeySystemPermissionRequest,
        queryPermission,
#if PLATFORM(IOS) || PLATFORM(VISION)
        lockScreenOrientationCallback,
        unlockScreenOrientationCallback
#else
        0, // lockScreenOrientation
        0 // unlockScreenOrientation
#endif
    };
    WKPageSetPageUIClient(m_mainWebView->page(), &pageUIClient.base);

    WKPageNavigationClientV3 pageNavigationClient = {
        { 3, this },
        decidePolicyForNavigationAction,
        decidePolicyForNavigationResponse,
        decidePolicyForPluginLoad,
        nullptr, // didStartProvisionalNavigation
        didReceiveServerRedirectForProvisionalNavigation,
        didFailProvisionalNavigation,
        didCommitNavigation,
        didFinishNavigation,
        nullptr, // didFailNavigation
        nullptr, // didFailProvisionalLoadInSubframe
        nullptr, // didFinishDocumentLoad
        nullptr, // didSameDocumentNavigation
        nullptr, // renderingProgressDidChange
        canAuthenticateAgainstProtectionSpace,
        didReceiveAuthenticationChallenge,
        nullptr,
        copyWebCryptoMasterKey,
        didBeginNavigationGesture,
        willEndNavigationGesture,
        didEndNavigationGesture,
        didRemoveNavigationGestureSnapshot,
        webProcessDidTerminate, // webProcessDidTerminate
        nullptr, // contentRuleListNotification
        nullptr, // copySignedPublicKeyAndChallengeString
        navigationActionDidBecomeDownload,
        navigationResponseDidBecomeDownload,
        nullptr // contextMenuDidCreateDownload
    };
    WKPageSetPageNavigationClient(m_mainWebView->page(), &pageNavigationClient.base);
    
    // this should just be done on the page?
    WKPageInjectedBundleClientV1 injectedBundleClient = {
        { 1, this },
        didReceivePageMessageFromInjectedBundle,
        nullptr,
        didReceiveSynchronousPageMessageFromInjectedBundleWithListener,
        didReceiveAsyncPageMessageFromInjectedBundleWithListener
    };
    WKPageSetPageInjectedBundleClient(m_mainWebView->page(), &injectedBundleClient.base);

    m_mainWebView->didInitializeClients();

    // Generally, the tests should default to running at 1x. updateWindowScaleForTest() will adjust the scale to
    // something else for specific tests that need to run at a different window scale.
    m_mainWebView->changeWindowScaleIfNeeded(1);
    
    if (!applicationBundleIdentifier.empty()) {
        reinitializeAppBoundDomains();
        updateBundleIdentifierInNetworkProcess(applicationBundleIdentifier);
    }
}

void TestController::ensureViewSupportsOptionsForTest(const TestInvocation& test)
{
    auto options = test.options();

    if (m_mainWebView) {
        // Having created another page (via window.open()) prevents process swapping on navigation and it may therefore
        // cause flakiness to reuse the view. We should also always make a new view if the test is marked as app-bound, because
        // the view configuration must change.
        if (!m_createdOtherPage && m_mainWebView->viewSupportsOptions(options) && !options.isAppBoundWebView())
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

template<typename F> static void batchUpdatePreferences(WKPreferencesRef preferences, F&& functor)
{
    WKPreferencesStartBatchingUpdates(preferences);
    functor(preferences);
    WKPreferencesEndBatchingUpdates(preferences);
}

void TestController::resetPreferencesToConsistentValues(const TestOptions& options)
{
    batchUpdatePreferences(platformPreferences(), [options, enableAllExperimentalFeatures = m_enableAllExperimentalFeatures] (auto preferences) {
        WKPreferencesResetTestRunnerOverrides(preferences);

        if (enableAllExperimentalFeatures) {
            WKPreferencesEnableAllExperimentalFeatures(preferences);
            WKPreferencesSetExperimentalFeatureForKey(preferences, false, toWK("SiteIsolationEnabled").get());
            WKPreferencesSetExperimentalFeatureForKey(preferences, true, toWK("WebGPUEnabled").get());
            WKPreferencesSetExperimentalFeatureForKey(preferences, false, toWK("HTTPSByDefaultEnabled").get());
            WKPreferencesSetExperimentalFeatureForKey(preferences, false, toWK("WebRTCL4SEnabled").get()); // FIXME: Remove this once L4S SDP negotation is supported.
        }

        WKPreferencesResetAllInternalDebugFeatures(preferences);

        WKPreferencesSetProcessSwapOnNavigationEnabled(preferences, options.shouldEnableProcessSwapOnNavigation());
        WKPreferencesSetStorageBlockingPolicy(preferences, kWKAllowAllStorage); // FIXME: We should be testing the default.
        WKPreferencesSetMinimumFontSize(preferences, 0);

        WKPreferencesSetBoolValueForKeyForTesting(preferences, options.allowTestOnlyIPC(), toWK("AllowTestOnlyIPC").get());

        for (const auto& [key, value] : options.boolWebPreferenceFeatures())
            WKPreferencesSetBoolValueForKeyForTesting(preferences, value, toWK(key).get());

        for (const auto& [key, value] : options.doubleWebPreferenceFeatures())
            WKPreferencesSetDoubleValueForKeyForTesting(preferences, value, toWK(key).get());

        for (const auto& [key, value] : options.uint32WebPreferenceFeatures())
            WKPreferencesSetUInt32ValueForKeyForTesting(preferences, value, toWK(key).get());

        for (const auto& [key, value] : options.stringWebPreferenceFeatures())
            WKPreferencesSetStringValueForKeyForTesting(preferences, toWK(value).get(), toWK(key).get());
    });
}

bool TestController::resetStateToConsistentValues(const TestOptions& options, ResetStage resetStage)
{
    SetForScope changeState(m_state, Resetting);
    m_beforeUnloadReturnValue = true;

    for (auto& auxiliaryWebView : std::exchange(m_auxiliaryWebViews, { }))
        WKPageClose(auxiliaryWebView->page());

    WKPageSetCustomUserAgent(m_mainWebView->page(), nullptr);

    auto resetMessageBody = adoptWK(WKMutableDictionaryCreate());

    if (resetStage == ResetStage::AfterTest)
        WKPageStopLoading(m_mainWebView->page());
    setValue(resetMessageBody, "ResetStage", resetStage == ResetStage::AfterTest ? "AfterTest" : "BeforeTest");

    setValue(resetMessageBody, "ShouldGC", m_gcBetweenTests);

    auto allowedHostsValue = adoptWK(WKMutableArrayCreate());
    for (auto& host : m_allowedHosts)
        WKArrayAppendItem(allowedHostsValue.get(), toWK(host.c_str()).get());
    setValue(resetMessageBody, "AllowedHosts", allowedHostsValue);

    auto jscOptions = options.jscOptions();
    if (!jscOptions.empty())
        setValue(resetMessageBody, "JSCOptions", jscOptions.c_str());

    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), toWK("Reset").get(), resetMessageBody.get());

    WKContextSetCacheModel(TestController::singleton().context(), kWKCacheModelDocumentBrowser);

    WKWebsiteDataStoreResetServiceWorkerFetchTimeoutForTesting(websiteDataStore());

    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(websiteDataStore(), true);
    WKWebsiteDataStoreClearAllDeviceOrientationPermissions(websiteDataStore());

    WKHTTPCookieStoreDeleteAllCookies(WKWebsiteDataStoreGetHTTPCookieStore(websiteDataStore()), nullptr, nullptr);

    clearStorage();
    resetQuota();
    resetStoragePersistedState();

    WKContextClearCurrentModifierStateForTesting(TestController::singleton().context());
    WKContextSetUseSeparateServiceWorkerProcess(TestController::singleton().context(), false);
    WKContextClearMockGamepadsForTesting(TestController::singleton().context());

    WKPageSetMockCameraOrientation(m_mainWebView->page(), 0);
    resetMockMediaDevices();
    WKPageSetMediaCaptureReportingDelayForTesting(m_mainWebView->page(), 0);

    // FIXME: This function should also ensure that there is only one page open.

    // Reset the EventSender for each test.
    m_eventSenderProxy = makeUnique<EventSenderProxy>(this);

    // FIXME: Is this needed? Nothing in TestController changes preferences during tests, and if there is
    // some other code doing this, it should probably be responsible for cleanup too.
    resetPreferencesToConsistentValues(options);

    // Make sure the view is in the window (a test can unparent it).
    m_mainWebView->addToWindow();

    // In the case that a test using the chrome input field failed, be sure to clean up for the next test.
    m_mainWebView->removeChromeInputField();
    m_mainWebView->focus();

    // Re-set to the default backing scale factor by setting the custom scale factor to 0.
    WKPageSetCustomBackingScaleFactor(m_mainWebView->page(), 0);

    WKPageClearWheelEventTestMonitor(m_mainWebView->page());

    // GStreamer uses fakesink to avoid sound output during testing and doing this creates trouble with volume events.
#if !USE(GSTREAMER)
    WKPageSetMediaVolume(m_mainWebView->page(), 0);
#endif

    WKPageClearUserMediaState(m_mainWebView->page());

    // Reset notification permissions
    m_webNotificationProvider.reset();
    m_notificationOriginsToDenyOnPrompt.clear();
    WKPageClearNotificationPermissionState(m_mainWebView->page());

    // Reset Geolocation permissions.
    m_geolocationPermissionRequests.clear();
    m_isGeolocationPermissionSet = false;
    m_isGeolocationPermissionAllowed = false;
    m_geolocationPermissionQueryOrigins.clear();

    // Reset Screen Wake Lock permission.
    m_screenWakeLockPermission = std::nullopt;

    // Reset UserMedia permissions.
    m_userMediaPermissionRequests.clear();
    m_cachedUserMediaPermissions.clear();
    setCameraPermission(true);
    setMicrophonePermission(true);

    // Reset Custom Policy Delegate.
    setCustomPolicyDelegate(false, false);
    m_skipPolicyDelegateNotifyDone = false;

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

    m_shouldLogDownloadSize = false;
    m_shouldLogDownloadExpectedSize = false;
    m_shouldLogDownloadCallbacks = false;
    m_shouldLogHistoryClientCallbacks = false;
    m_shouldLogCanAuthenticateAgainstProtectionSpace = false;

    setHidden(false);
    setAllowStorageQuotaIncrease(true);
    setQuota(40 * KB);
    setOriginQuotaRatioEnabled(true);

    if (!platformResetStateToConsistentValues(options))
        return false;

    m_shouldDecideNavigationPolicyAfterDelay = false;
    m_shouldDecideResponsePolicyAfterDelay = false;

    setNavigationGesturesEnabled(false);
    
    setIgnoresViewportScaleLimits(options.ignoresViewportScaleLimits());

    m_openPanelFileURLs = nullptr;
#if PLATFORM(IOS_FAMILY)
    m_openPanelFileURLsMediaIcon = nullptr;
#endif

    setAllowsAnySSLCertificate(true);
    setBackgroundFetchPermission(true);

    statisticsResetToConsistentState();
    clearLoadedSubresourceDomains();
    clearAppBoundSession();
    clearPrivateClickMeasurement();

    WKPageDispatchActivityStateUpdateForTesting(m_mainWebView->page());

    WKPageResetProcessState(m_mainWebView->page());

    m_didReceiveServerRedirectForProvisionalNavigation = false;
    m_serverTrustEvaluationCallbackCallsCount = 0;
    m_shouldDismissJavaScriptAlertsAsynchronously = false;

    setIsSpeechRecognitionPermissionGranted(true);

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
        clearAppPrivacyReportTestingData();
#endif
        clearBundleIdentifierInNetworkProcess();
    }

    m_downloadTotalBytesWritten = { };
    m_downloadIndex = 0;
    m_shouldDownloadContentDispositionAttachments = true;
    m_dumpPolicyDelegateCallbacks = false;

    return m_doneResetting;
}

void TestController::updateLiveDocumentsAfterTest()
{
    if (!m_checkForWorldLeaks)
        return;

    AsyncTask([]() {
        // After each test, we update the list of live documents so that we can detect when an abandoned document first showed up.
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), toWK("GetLiveDocuments").get(), nullptr);
    }, 5_s).run();
}

void TestController::checkForWorldLeaks()
{
    if (!m_checkForWorldLeaks || !TestController::singleton().mainWebView())
        return;

    AsyncTask([]() {
        // This runs at the end of a series of tests. It clears caches, runs a GC and then fetches the list of documents.
        WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), toWK("CheckForWorldLeaks").get(), nullptr);
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
    auto page = TestController::singleton().mainWebView()->page();
    dumpResponse(makeString(
        TestController::webProcessName(), ": "_s
        , WKPageGetProcessIdentifier(page), '\n'
        , TestController::networkProcessName(), ": "_s
        , WKWebsiteDataStoreGetNetworkProcessIdentifier(websiteDataStore()), '\n'
#if ENABLE(GPU_PROCESS)
        , TestController::gpuProcessName(), ": "_s
        , WKPageGetGPUProcessIdentifier(page), '\n'
#endif
    ));
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
                documentURL = "(no url)"_s;
            builder.append("TEST: "_s);
            builder.append(it.value.testURL);
            builder.append('\n');
            builder.append("ABANDONED DOCUMENT: "_s);
            builder.append(documentURL);
            builder.append('\n');
        }
    } else
        builder.append("no abandoned documents\n"_s);

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
    SetForScope changeState(m_state, Resetting);
    m_doneResetting = false;
    WKPageLoadURL(m_mainWebView->page(), blankURL());
    runUntil(m_doneResetting, noTimeout);
}

ASCIILiteral TestController::webProcessName()
{
    // FIXME: Find a way to not hardcode the process name.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    return "com.apple.WebKit.WebContent"_s;
#elif PLATFORM(COCOA)
    return "com.apple.WebKit.WebContent.Development"_s;
#elif PLATFORM(GTK)
    return "WebKitWebProcess"_s;
#elif PLATFORM(WPE)
    return "WPEWebProcess"_s;
#else
    return "WebProcess"_s;
#endif
}

ASCIILiteral TestController::networkProcessName()
{
    // FIXME: Find a way to not hardcode the process name.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    return "com.apple.WebKit.Networking"_s;
#elif PLATFORM(COCOA)
    return "com.apple.WebKit.Networking.Development"_s;
#elif PLATFORM(GTK)
    return "WebKitNetworkProcess"_s;
#elif PLATFORM(WPE)
    return "WPENetworkProcess"_s;
#else
    return "NetworkProcess"_s;
#endif
}

ASCIILiteral TestController::gpuProcessName()
{
    // FIXME: Find a way to not hardcode the process name.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    return "com.apple.WebKit.GPU"_s;
#elif PLATFORM(COCOA)
    return "com.apple.WebKit.GPU.Development"_s;
#else
    return "GPUProcess"_s;
#endif
}

#if !PLATFORM(COCOA)

void TestController::setAllowsAnySSLCertificate(bool allows)
{
    m_allowsAnySSLCertificate = allows;
}

void TestController::setBackgroundFetchPermission(bool)
{
    // FIXME: Add support.
}

WKRetainPtr<WKStringRef> TestController::lastAddedBackgroundFetchIdentifier() const
{
    return adoptWK(WKStringCreateWithUTF8CString("not implemented"));
}

WKRetainPtr<WKStringRef> TestController::lastRemovedBackgroundFetchIdentifier() const
{
    return adoptWK(WKStringCreateWithUTF8CString("not implemented"));
}

WKRetainPtr<WKStringRef> TestController::lastUpdatedBackgroundFetchIdentifier() const
{
    return adoptWK(WKStringCreateWithUTF8CString("not implemented"));
}

WKRetainPtr<WKStringRef> TestController::backgroundFetchState(WKStringRef)
{
    return { };
}
#endif

WKURLRef TestController::createTestURL(const char* pathOrURL)
{
    if (strstr(pathOrURL, "http://") || strstr(pathOrURL, "https://"))
        return WKURLCreateWithUTF8CString(pathOrURL);

    size_t length = strlen(pathOrURL);
    if (!length)
        return 0;

    if (length >= 7 && strstr(pathOrURL, "file://")) {
        auto url = adoptWK(WKURLCreateWithUTF8CString(pathOrURL));
        auto path = testPath(url.get());
        if (!m_usingServerMode && !WTF::FileSystemImpl::fileExists(String(std::span { path }))) {
            printf("Failed: File for URL ‘%s’ was not found or is inaccessible\n", pathOrURL);
            return 0;
        }
        return url.leakRef();
    }

    // Creating from filesytem path.

#if PLATFORM(WIN)
    bool isAbsolutePath = !PathIsRelativeA(pathOrURL);
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

    auto cPath = buffer.get();
    auto url = adoptWK(WKURLCreateWithUTF8CString(cPath));
    auto path = testPath(url.get());
    if (!m_usingServerMode && !WTF::FileSystemImpl::fileExists(String(std::span { path }))) {
        printf("Failed: File ‘%s’ was not found or is inaccessible\n", pathOrURL);
        return 0;
    }
    return url.leakRef();
}

TestOptions TestController::testOptionsForTest(const TestCommand& command) const
{
    TestFeatures features = TestOptions::defaults();
    merge(features, m_globalFeatures);
    merge(features, hardcodedFeaturesBasedOnPathForTest(command));
    merge(features, platformSpecificFeatureDefaultsForTest(command));
    merge(features, featureDefaultsFromSelfComparisonHeader(command, TestOptions::keyTypeMapping()));
    merge(features, featureDefaultsFromTestHeaderForTest(command, TestOptions::keyTypeMapping()));
    merge(features, featureFromAdditionalHeaderOption(command, TestOptions::keyTypeMapping()));
    merge(features, platformSpecificFeatureOverridesDefaultsForTest(command));

    return TestOptions { features };
}

void TestController::updateWebViewSizeForTest(const TestInvocation& test)
{
    mainWebView()->resizeTo(test.options().viewWidth(), test.options().viewHeight());
}

void TestController::updateWindowScaleForTest(PlatformWebView* view, const TestInvocation& test)
{
    view->changeWindowScaleIfNeeded(test.options().deviceScaleFactor());
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

    return "LayoutTests/http/tests" + toSTD(adoptWK(WKURLCopyPath(url)).get()) + ".json";
}

void TestController::configureContentExtensionForTest(const TestInvocation& test)
{
    const char* contentExtensionsPath = libraryPathForTesting();
    if (!contentExtensionsPath)
        contentExtensionsPath = "/tmp/wktr-contentextensions";

    if (!test.urlContains("contentextensions/"_s)) {
        return;
    }

    std::string jsonFilePath(contentExtensionJSONPath(test.url()));
    std::ifstream jsonFile(jsonFilePath);
    if (!jsonFile.good()) {
        WTFLogAlways("Could not open file '%s'", jsonFilePath.c_str());
        return;
    }

    std::string jsonFileContents {std::istreambuf_iterator<char>(jsonFile), std::istreambuf_iterator<char>()};
    auto jsonSource = toWK(jsonFileContents.c_str());

    auto storePath = toWK(contentExtensionsPath);
    auto extensionStore = adoptWK(WKUserContentExtensionStoreCreate(storePath.get()));
    ASSERT(extensionStore);

    auto filterIdentifier = toWK("TestContentExtension");

    ContentExtensionStoreCallbackContext context(*this);
    WKUserContentExtensionStoreCompile(extensionStore.get(), filterIdentifier.get(), jsonSource.get(), &context, contentExtensionStoreCallback);
    runUntil(context.done, noTimeout);
    ASSERT(context.status == kWKUserContentExtensionStoreSuccess);
    ASSERT(context.filter);

    WKUserContentControllerAddUserContentFilter(userContentController(), context.filter.get());
}

void TestController::resetContentExtensions()
{
    if (!mainWebView())
        return;

    const char* contentExtensionsPath = libraryPathForTesting();
    if (!contentExtensionsPath)
        return;

    WKUserContentControllerRemoveAllUserContentFilters(userContentController());

    auto storePath = toWK(contentExtensionsPath);
    auto extensionStore = adoptWK(WKUserContentExtensionStoreCreate(storePath.get()));
    ASSERT(extensionStore);

    auto filterIdentifier = toWK("TestContentExtension");

    ContentExtensionStoreCallbackContext context(*this);
    WKUserContentExtensionStoreRemove(extensionStore.get(), filterIdentifier.get(), &context, contentExtensionStoreCallback);
    runUntil(context.done, noTimeout);
    ASSERT(!context.filter);
}

#endif // ENABLE(CONTENT_EXTENSIONS) && !PLATFORM(COCOA)

#if !ENABLE(CONTENT_EXTENSIONS)

void TestController::configureContentExtensionForTest(const TestInvocation&)
{
}

void TestController::resetContentExtensions()
{
}

#endif // !ENABLE(CONTENT_EXTENSIONS)

bool TestController::runTest(const char* inputLine)
{
    AutodrainedPool pool;

    WKTextCheckerSetTestingMode(true);
    
    auto command = parseInputLine(std::string(inputLine));

    m_state = RunningTest;
    
    TestOptions options = testOptionsForTest(command);

    m_mainResourceURL = adoptWK(createTestURL(command.pathOrURL.c_str()));
    if (!m_mainResourceURL)
        return false;

    m_currentInvocation = makeUnique<TestInvocation>(m_mainResourceURL.get(), options);

    if (command.shouldDumpPixels || m_shouldDumpPixelsForAllTests)
        m_currentInvocation->setIsPixelTest(command.expectedPixelHash);

    if (command.forceDumpPixels)
        m_currentInvocation->setForceDumpPixels(true);

    if (command.timeout > 0_s)
        m_currentInvocation->setCustomTimeout(command.timeout);

    m_currentInvocation->setDumpJSConsoleLogInStdErr(command.dumpJSConsoleLogInStdErr || options.dumpJSConsoleLogInStdErr());

    platformWillRunTest(*m_currentInvocation);

    m_currentInvocation->invoke();
    m_currentInvocation = nullptr;
    m_mainResourceURL = nullptr;

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
    auto* testController = static_cast<TestController*>(const_cast<void*>(clientInfo));
    if (page != testController->mainWebView()->page()) {
        // If this is a Done message from an auxiliary view in its own WebProcess (due to process-swapping), we need to notify the injected bundle of the main WebView
        // that the test is done.
        if (WKStringIsEqualToUTF8CString(messageName, "Done") && testController->m_currentInvocation)
            WKPagePostMessageToInjectedBundle(testController->mainWebView()->page(), toWK("NotifyDone").get(), nullptr);
        if (!WKStringIsEqualToUTF8CString(messageName, "TextOutput"))
            return;
    }
    testController->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

void TestController::didReceiveSynchronousPageMessageFromInjectedBundleWithListener(WKPageRef page, WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef listener, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody, listener);
}

void TestController::didReceiveAsyncPageMessageFromInjectedBundleWithListener(WKPageRef, WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef listener, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveAsyncMessageFromInjectedBundle(messageName, messageBody, listener);
}

void TestController::networkProcessDidCrashWithDetails(WKContextRef context, WKProcessID processID, WKProcessTerminationReason reason, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->networkProcessDidCrash(processID, reason);
}

void TestController::serviceWorkerProcessDidCrashWithDetails(WKContextRef context, WKProcessID processID, WKProcessTerminationReason reason, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->serviceWorkerProcessDidCrash(processID, reason);
}

void TestController::gpuProcessDidCrashWithDetails(WKContextRef context, WKProcessID processID, WKProcessTerminationReason reason, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->gpuProcessDidCrash(processID, reason);
}

void TestController::didReceiveKeyDownMessageFromInjectedBundle(WKDictionaryRef dictionary, bool synchronous)
{
    m_eventSenderProxy->keyDown(stringValue(dictionary, "Key"), uint64Value(dictionary, "Modifiers"), uint64Value(dictionary, "Location"));
}

void TestController::didReceiveRawKeyDownMessageFromInjectedBundle(WKDictionaryRef dictionary, bool synchronous)
{
    m_eventSenderProxy->rawKeyDown(stringValue(dictionary, "Key"), uint64Value(dictionary, "Modifiers"), uint64Value(dictionary, "Location"));
}

void TestController::didReceiveRawKeyUpMessageFromInjectedBundle(WKDictionaryRef dictionary, bool synchronous)
{
    m_eventSenderProxy->rawKeyUp(stringValue(dictionary, "Key"), uint64Value(dictionary, "Modifiers"), uint64Value(dictionary, "Location"));
}

void TestController::didReceiveLiveDocumentsList(WKArrayRef liveDocumentList)
{
    auto numDocuments = WKArrayGetSize(liveDocumentList);

    HashMap<String, String> documentInfo;
    for (size_t i = 0; i < numDocuments; ++i) {
        if (auto dictionary = dictionaryValue(WKArrayGetItemAtIndex(liveDocumentList, i)))
            documentInfo.add(toWTFString(stringValue(dictionary, "id")), toWTFString(stringValue(dictionary, "url")));
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
    String currentTestURL = m_currentInvocation ? toWTFString(adoptWK(WKURLCopyString(m_currentInvocation->url()))) : "no test"_s;
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

        auto dictionary = dictionaryValue(messageBody);
        auto subMessageName = stringValue(dictionary, "SubMessage");

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown")) {
            m_eventSenderProxy->mouseDown(uint64Value(dictionary, "Button"), uint64Value(dictionary, "Modifiers"), stringValue(dictionary, "PointerType"));
            return;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseUp")) {
            m_eventSenderProxy->mouseUp(uint64Value(dictionary, "Button"), uint64Value(dictionary, "Modifiers"), stringValue(dictionary, "PointerType"));
            return;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "KeyDown")) {
            didReceiveKeyDownMessageFromInjectedBundle(dictionary, false);
            return;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "RawKeyDown")) {
            didReceiveRawKeyDownMessageFromInjectedBundle(dictionary, false);
            return;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "RawKeyUp")) {
            didReceiveRawKeyUpMessageFromInjectedBundle(dictionary, false);
            return;
        }
        
        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseScrollBy")) {
            m_eventSenderProxy->mouseScrollBy(doubleValue(dictionary, "X"), doubleValue(dictionary, "Y"));
            return;
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseScrollByWithWheelAndMomentumPhases")) {
            auto x = doubleValue(dictionary, "X");
            auto y = doubleValue(dictionary, "Y");
            auto phase = uint64Value(dictionary, "Phase");
            auto momentum = uint64Value(dictionary, "Momentum");
            m_eventSenderProxy->mouseScrollByWithWheelAndMomentumPhases(x, y, phase, momentum);
            return;
        }

#if PLATFORM(GTK)
        if (WKStringIsEqualToUTF8CString(subMessageName, "SetWheelHasPreciseDeltas")) {
            auto hasPreciseDeltas = booleanValue(dictionary, "HasPreciseDeltas");
            m_eventSenderProxy->setWheelHasPreciseDeltas(hasPreciseDeltas);
            return;
        }
#endif

        ASSERT_NOT_REACHED();
    }

    if (!m_currentInvocation)
        return;

    m_currentInvocation->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

static void adoptAndCallCompletionHandler(void* context)
{
    auto completionHandler = WTF::adopt(static_cast<CompletionHandler<void(WKTypeRef)>::Impl*>(context));
    completionHandler(nullptr);
}

void TestController::didReceiveAsyncMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef listener)
{
    CompletionHandler<void(WKTypeRef)> completionHandler = [listener = retainWK(listener)] (WKTypeRef reply) {
        WKMessageListenerSendReply(listener.get(), reply);
    };

    if (WKStringIsEqualToUTF8CString(messageName, "EventSender")) {
        auto dictionary = dictionaryValue(messageBody);
        auto subMessageName = stringValue(dictionary, "SubMessage");

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown"))
            m_eventSenderProxy->mouseDown(uint64Value(dictionary, "Button"), uint64Value(dictionary, "Modifiers"), stringValue(dictionary, "PointerType"));
        else if (WKStringIsEqualToUTF8CString(subMessageName, "MouseUp"))
            m_eventSenderProxy->mouseUp(uint64Value(dictionary, "Button"), uint64Value(dictionary, "Modifiers"), stringValue(dictionary, "PointerType"));
        else if (WKStringIsEqualToUTF8CString(subMessageName, "MouseMoveTo"))
            m_eventSenderProxy->mouseMoveTo(doubleValue(dictionary, "X"), doubleValue(dictionary, "Y"), stringValue(dictionary, "PointerType"));
        else {
            ASSERT_NOT_REACHED();
            return completionHandler(nullptr);
        }

        m_eventSenderProxy->waitForPendingMouseEvents();
        return completionHandler(nullptr);
    }

    if (WKStringIsEqualToUTF8CString(messageName, "FlushConsoleLogs"))
        return completionHandler(nullptr);

    if (WKStringIsEqualToUTF8CString(messageName, "SetPageScaleFactor")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto scaleFactor = doubleValue(messageBodyDictionary, "scaleFactor");
        auto x = doubleValue(messageBodyDictionary, "x");
        auto y = doubleValue(messageBodyDictionary, "y");
        return setPageScaleFactor(static_cast<float>(scaleFactor), static_cast<int>(x), static_cast<int>(y), WTFMove(completionHandler));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "GetAllStorageAccessEntries"))
        return getAllStorageAccessEntries(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "GetAndClearReportedWindowProxyAccessDomains"))
        return completionHandler(getAndClearReportedWindowProxyAccessDomains().get());

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveAllCookies"))
        return removeAllCookies(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "TakeViewPortSnapshot"))
        return completionHandler(takeViewPortSnapshot().get());

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsDebugMode"))
        return setStatisticsDebugMode(booleanValue(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsShouldBlockThirdPartyCookiesOnSitesWithoutUserInteraction"))
        return setStatisticsShouldBlockThirdPartyCookies(booleanValue(messageBody), true, WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsShouldBlockThirdPartyCookies"))
        return setStatisticsShouldBlockThirdPartyCookies(booleanValue(messageBody), false, WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsPrevalentResourceForDebugMode")) {
        WKStringRef hostName = stringValue(messageBody);
        setStatisticsPrevalentResourceForDebugMode(hostName, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsLastSeen")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto value = doubleValue(messageBodyDictionary, "Value");
        setStatisticsLastSeen(hostName, value, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsMergeStatistic")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto topFrameDomain1 = stringValue(messageBodyDictionary, "TopFrameDomain1");
        auto topFrameDomain2 = stringValue(messageBodyDictionary, "TopFrameDomain2");
        auto lastSeen = doubleValue(messageBodyDictionary, "LastSeen");
        auto hadUserInteraction = booleanValue(messageBodyDictionary, "HadUserInteraction");
        auto mostRecentUserInteraction = doubleValue(messageBodyDictionary, "MostRecentUserInteraction");
        auto isGrandfathered = booleanValue(messageBodyDictionary, "IsGrandfathered");
        auto isPrevalent = booleanValue(messageBodyDictionary, "IsPrevalent");
        auto isVeryPrevalent = booleanValue(messageBodyDictionary, "IsVeryPrevalent");
        auto dataRecordsRemoved = uint64Value(messageBodyDictionary, "DataRecordsRemoved");
        setStatisticsMergeStatistic(hostName, topFrameDomain1, topFrameDomain2, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsExpiredStatistic")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto numberOfOperatingDaysPassed = uint64Value(messageBodyDictionary, "NumberOfOperatingDaysPassed");
        auto hadUserInteraction = booleanValue(messageBodyDictionary, "HadUserInteraction");
        auto isScheduledForAllButCookieDataRemoval = booleanValue(messageBodyDictionary, "IsScheduledForAllButCookieDataRemoval");
        auto isPrevalent = booleanValue(messageBodyDictionary, "IsPrevalent");
        setStatisticsExpiredStatistic(hostName, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsPrevalentResource")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto value = booleanValue(messageBodyDictionary, "Value");
        setStatisticsPrevalentResource(hostName, value, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsVeryPrevalentResource")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto value = booleanValue(messageBodyDictionary, "Value");
        setStatisticsVeryPrevalentResource(hostName, value, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearInMemoryAndPersistentStore"))
        return statisticsClearInMemoryAndPersistentStore(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearThroughWebsiteDataRemoval"))
        return statisticsClearThroughWebsiteDataRemoval(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsClearInMemoryAndPersistentStoreModifiedSinceHours"))
        return statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(uint64Value(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsShouldDowngradeReferrer"))
        return setStatisticsShouldDowngradeReferrer(booleanValue(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsFirstPartyWebsiteDataRemovalMode"))
        return setStatisticsFirstPartyWebsiteDataRemovalMode(booleanValue(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetToSameSiteStrictCookies"))
        return setStatisticsToSameSiteStrictCookies(stringValue(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetFirstPartyHostCNAMEDomain")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto firstPartyURLString = stringValue(messageBodyDictionary, "FirstPartyURL");
        auto cnameURLString = stringValue(messageBodyDictionary, "CNAME");
        setStatisticsFirstPartyHostCNAMEDomain(firstPartyURLString, cnameURLString, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetThirdPartyCNAMEDomain"))
        return setStatisticsThirdPartyCNAMEDomain(stringValue(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsHasHadUserInteraction")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto value = booleanValue(messageBodyDictionary, "Value");
        setStatisticsHasHadUserInteraction(hostName, value, WTFMove(completionHandler));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsUpdateCookieBlocking"))
        return statisticsUpdateCookieBlocking(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsResetToConsistentState")) {
        m_currentInvocation->dumpResourceLoadStatisticsIfNecessary();
        statisticsResetToConsistentState();
        return completionHandler(nullptr);
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsDeleteCookiesForHost")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto includeHttpOnlyCookies = booleanValue(messageBodyDictionary, "IncludeHttpOnlyCookies");
        return TestController::singleton().statisticsDeleteCookiesForHost(hostName, includeHttpOnlyCookies, WTFMove(completionHandler));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsProcessStatisticsAndDataRecords"))
        return TestController::singleton().statisticsProcessStatisticsAndDataRecords(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "AddChromeInputField")) {
        mainWebView()->addChromeInputField();
        return completionHandler(nullptr);
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetTextInChromeInputField")) {
        mainWebView()->setTextInChromeInputField(toWTFString(stringValue(messageBody)));
        return completionHandler(nullptr);
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SelectChromeInputField")) {
        mainWebView()->selectChromeInputField();
        return completionHandler(nullptr);
    }

    if (WKStringIsEqualToUTF8CString(messageName, "GetSelectedTextInChromeInputField")) {
        auto selectedText = mainWebView()->getSelectedTextInChromeInputField();
        return completionHandler(toWK(selectedText).get());
    }

    if (WKStringIsEqualToUTF8CString(messageName, "FocusWebView")) {
        mainWebView()->makeWebViewFirstResponder();
        return completionHandler(nullptr);
    }

    if (WKStringIsEqualToUTF8CString(messageName, "LoadedSubresourceDomains"))
        return loadedSubresourceDomains(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "GetApplicationManifest")) {
        WKPageGetApplicationManifest(mainWebView()->page(), completionHandler.leak(), adoptAndCallCompletionHandler);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveChromeInputField")) {
        mainWebView()->removeChromeInputField();
        return completionHandler(nullptr);
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetManagedDomains"))
        return setManagedDomains(arrayValue(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetAppBoundDomains"))
        return setAppBoundDomains(arrayValue(messageBody), WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetBackingScaleFactor")) {
        WKPageSetCustomBackingScaleFactorWithCallback(TestController::singleton().mainWebView()->page(), doubleValue(messageBody), completionHandler.leak(), adoptAndCallCompletionHandler);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveAllSessionCredentials"))
        return TestController::singleton().removeAllSessionCredentials(WTFMove(completionHandler));

    if (WKStringIsEqualToUTF8CString(messageName, "SetTopContentInset"))
        return WKPageSetTopContentInsetForTesting(TestController::singleton().mainWebView()->page(), static_cast<float>(doubleValue(messageBody)), completionHandler.leak(), adoptAndCallCompletionHandler);

    ASSERT_NOT_REACHED();
}

void TestController::didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef listener)
{
    auto completionHandler = [listener = retainWK(listener)] (WKTypeRef reply) {
        WKMessageListenerSendReply(listener.get(), reply);
    };

    if (WKStringIsEqualToUTF8CString(messageName, "EventSender")) {
        if (m_state != RunningTest)
            return completionHandler(nullptr);

        auto dictionary = dictionaryValue(messageBody);
        auto subMessageName = stringValue(dictionary, "SubMessage");

        if (WKStringIsEqualToUTF8CString(subMessageName, "KeyDown")) {
            didReceiveKeyDownMessageFromInjectedBundle(dictionary, true);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseDown")) {
            m_eventSenderProxy->mouseDown(uint64Value(dictionary, "Button"), uint64Value(dictionary, "Modifiers"), stringValue(dictionary, "PointerType"));
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseUp")) {
            m_eventSenderProxy->mouseUp(uint64Value(dictionary, "Button"), uint64Value(dictionary, "Modifiers"), stringValue(dictionary, "PointerType"));
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "RawKeyDown")) {
            didReceiveRawKeyDownMessageFromInjectedBundle(dictionary, true);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "RawKeyUp")) {
            didReceiveRawKeyUpMessageFromInjectedBundle(dictionary, true);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "MouseMoveTo")) {
            m_eventSenderProxy->mouseMoveTo(doubleValue(dictionary, "X"), doubleValue(dictionary, "Y"), stringValue(dictionary, "PointerType"));
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "WaitForDeferredMouseEvents"))
            return completionHandler(nullptr);

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
            m_eventSenderProxy->mouseForceChanged(doubleValue(dictionary, "Force"));
            return completionHandler(nullptr);
        }
#endif // PLATFORM(MAC)

        if (WKStringIsEqualToUTF8CString(subMessageName, "ContinuousMouseScrollBy")) {
            auto x = doubleValue(dictionary, "X");
            auto y = doubleValue(dictionary, "Y");
            auto paged = booleanValue(dictionary, "Paged");
            m_eventSenderProxy->continuousMouseScrollBy(x, y, paged);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "LeapForward")) {
            m_eventSenderProxy->leapForward(uint64Value(dictionary, "TimeInMilliseconds"));
            return completionHandler(nullptr);
        }

#if ENABLE(TOUCH_EVENTS)
        if (WKStringIsEqualToUTF8CString(subMessageName, "AddTouchPoint")) {
            m_eventSenderProxy->addTouchPoint(uint64Value(dictionary, "X"), uint64Value(dictionary, "Y"));
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "UpdateTouchPoint")) {
            auto index = uint64Value(dictionary, "Index");
            auto x = uint64Value(dictionary, "X");
            auto y = uint64Value(dictionary, "Y");
            m_eventSenderProxy->updateTouchPoint(index, x, y);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetTouchModifier")) {
            auto modifier = uint64Value(dictionary, "Modifier");
            auto enable = booleanValue(dictionary, "Enable");
            m_eventSenderProxy->setTouchModifier(modifier, enable);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetTouchPointRadius")) {
            auto x = uint64Value(dictionary, "RadiusX");
            auto y = uint64Value(dictionary, "RadiusY");
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
            m_eventSenderProxy->releaseTouchPoint(uint64Value(dictionary, "Index"));
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "CancelTouchPoint")) {
            m_eventSenderProxy->cancelTouchPoint(uint64Value(dictionary, "Index"));
            return completionHandler(nullptr);
        }
#endif

#if PLATFORM(MAC)
        if (WKStringIsEqualToUTF8CString(subMessageName, "SmartMagnify")) {
            m_eventSenderProxy->smartMagnify();
            return completionHandler(nullptr);
        }
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
        if (WKStringIsEqualToUTF8CString(subMessageName, "ScaleGestureStart")) {
            auto scale = doubleValue(dictionary, "Scale");
            m_eventSenderProxy->scaleGestureStart(scale);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "ScaleGestureChange")) {
            auto scale = doubleValue(dictionary, "Scale");
            m_eventSenderProxy->scaleGestureChange(scale);
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "ScaleGestureEnd")) {
            auto scale = doubleValue(dictionary, "Scale");
            m_eventSenderProxy->scaleGestureEnd(scale);
            return completionHandler(nullptr);
        }
#endif // ENABLE(MAC_GESTURE_EVENTS)

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetPageZoom")) {
            auto* page = mainWebView()->page();
            WKPageSetTextZoomFactor(page, 1);
            WKPageSetPageZoomFactor(page, doubleValue(dictionary, "ZoomFactor"));
            return completionHandler(nullptr);
        }

        if (WKStringIsEqualToUTF8CString(subMessageName, "SetTextZoom")) {
            auto* page = mainWebView()->page();
            WKPageSetPageZoomFactor(page, 1);
            WKPageSetTextZoomFactor(page, doubleValue(dictionary, "ZoomFactor"));
            return completionHandler(nullptr);
        }

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
        auto policy = WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody))
            ? kWKHTTPCookieAcceptPolicyAlways
            : kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
        return setHTTPCookieAcceptPolicy(policy, WTFMove(completionHandler));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetOnlyAcceptFirstPartyCookies")) {
        auto policy = WKBooleanGetValue(static_cast<WKBooleanRef>(messageBody))
            ? kWKHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain
            : kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
        return setHTTPCookieAcceptPolicy(policy, WTFMove(completionHandler));
    }

    completionHandler(m_currentInvocation->didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody).get());
}

WKRetainPtr<WKTypeRef> TestController::getInjectedBundleInitializationUserData()
{
    return nullptr;
}

// WKContextClient

static const char* terminationReasonToString(WKProcessTerminationReason reason)
{
    switch (reason) {
    case kWKProcessTerminationReasonExceededMemoryLimit:
        return "exceeded memory limit";
    case kWKProcessTerminationReasonExceededCPULimit:
        return "exceeded cpu limit";
        break;
    case kWKProcessTerminationReasonRequestedByClient:
        return "requested by client";
    case kWKProcessTerminationReasonCrash:
        return "crash";
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return "unknown reason";
}

void TestController::networkProcessDidCrash(WKProcessID processID, WKProcessTerminationReason reason)
{
    fprintf(stderr, "%s terminated (pid %ld) for reason: %s\n", networkProcessName().characters(), static_cast<long>(processID), terminationReasonToString(reason));
    fprintf(stderr, "#CRASHED - %s (pid %ld)\n", networkProcessName().characters(), static_cast<long>(processID));
    if (m_shouldExitWhenAuxiliaryProcessCrashes)
        exitProcess(1);
}

void TestController::serviceWorkerProcessDidCrash(WKProcessID processID, WKProcessTerminationReason reason)
{
    fprintf(stderr, "%s terminated (pid %ld) for reason: %s\n", "ServiceWorkerProcess", static_cast<long>(processID), terminationReasonToString(reason));
    fprintf(stderr, "#CRASHED - ServiceWorkerProcess (pid %ld)\n", static_cast<long>(processID));
    if (m_shouldExitWhenAuxiliaryProcessCrashes)
        exitProcess(1);
}

void TestController::gpuProcessDidCrash(WKProcessID processID, WKProcessTerminationReason reason)
{
    fprintf(stderr, "%s terminated (pid %ld) for reason: %s\n", gpuProcessName().characters(), static_cast<long>(processID), terminationReasonToString(reason));
    fprintf(stderr, "#CRASHED - %s (pid %ld)\n", gpuProcessName().characters(), static_cast<long>(processID));
    if (m_shouldExitWhenAuxiliaryProcessCrashes)
        exitProcess(1);
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

void TestController::didFailProvisionalNavigation(WKPageRef page, WKNavigationRef navigation, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didFailProvisionalNavigation(page, error);
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

void TestController::webProcessDidTerminate(WKPageRef page, WKProcessTerminationReason reason, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->webProcessDidTerminate(reason);
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
    WKContextSetPluginLoadClientPolicy(m_context.get(), policy, toWK("").get(), toWK("com.apple.testnetscapeplugin").get(), toWK("").get());
    WKContextSetPluginLoadClientPolicy(m_context.get(), policy, toWK("").get(), toWK("com.macromedia.Flash Player.plugin").get(), toWK("").get());
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

    auto emptyArray = adoptWK(WKMutableArrayCreate());

    WKContextAddSupportedPlugin(m_context.get(), toWK("").get(), toWK("My personal PDF").get(), emptyArray.get(), emptyArray.get());

    auto nameNetscape = toWK("com.apple.testnetscapeplugin");
    auto mimeTypesNetscape = adoptWK(WKMutableArrayCreate());
    WKArrayAppendItem(mimeTypesNetscape.get(), toWK("application/x-webkit-test-netscape").get());
    auto namePdf = toWK("WebKit built-in PDF");

    if (m_unsupportedPluginMode == "allOrigins"_s) {
        WKContextAddSupportedPlugin(m_context.get(), toWK("").get(), nameNetscape.get(), mimeTypesNetscape.get(), emptyArray.get());
        WKContextAddSupportedPlugin(m_context.get(), toWK("").get(), namePdf.get(), emptyArray.get(), emptyArray.get());
        return;
    }

    if (m_unsupportedPluginMode == "specificOrigin"_s) {
        WKContextAddSupportedPlugin(m_context.get(), toWK("localhost").get(), nameNetscape.get(), mimeTypesNetscape.get(), emptyArray.get());
        WKContextAddSupportedPlugin(m_context.get(), toWK("localhost").get(), namePdf.get(), emptyArray.get(), emptyArray.get());
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

static ASCIILiteral toString(WKProtectionSpaceAuthenticationScheme scheme)
{
    switch (scheme) {
    case kWKProtectionSpaceAuthenticationSchemeDefault:
        return "ProtectionSpaceAuthenticationSchemeDefault"_s;
    case kWKProtectionSpaceAuthenticationSchemeHTTPBasic:
        return "ProtectionSpaceAuthenticationSchemeHTTPBasic"_s;
    case kWKProtectionSpaceAuthenticationSchemeHTMLForm:
        return "ProtectionSpaceAuthenticationSchemeHTMLForm"_s;
    case kWKProtectionSpaceAuthenticationSchemeNTLM:
        return "ProtectionSpaceAuthenticationSchemeNTLM"_s;
    case kWKProtectionSpaceAuthenticationSchemeNegotiate:
        return "ProtectionSpaceAuthenticationSchemeNegotiate"_s;
    case kWKProtectionSpaceAuthenticationSchemeClientCertificateRequested:
        return "ProtectionSpaceAuthenticationSchemeClientCertificateRequested"_s;
    case kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        return "ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested"_s;
    case kWKProtectionSpaceAuthenticationSchemeOAuth:
        return "ProtectionSpaceAuthenticationSchemeOAuth"_s;
    case kWKProtectionSpaceAuthenticationSchemeUnknown:
        return "ProtectionSpaceAuthenticationSchemeUnknown"_s;
    }
    ASSERT_NOT_REACHED();
    return "ProtectionSpaceAuthenticationSchemeUnknown"_s;
}

bool TestController::canAuthenticateAgainstProtectionSpace(WKPageRef page, WKProtectionSpaceRef protectionSpace)
{
    if (m_shouldLogCanAuthenticateAgainstProtectionSpace)
        m_currentInvocation->outputText("canAuthenticateAgainstProtectionSpace\n"_s);
    auto scheme = WKProtectionSpaceGetAuthenticationScheme(protectionSpace);
    if (scheme == kWKProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {
        auto host = toSTD(adoptWK(WKProtectionSpaceCopyHost(protectionSpace)));
        return host == "localhost" || host == "127.0.0.1" || m_localhostAliases.find(host) != m_localhostAliases.end() || (m_allowAnyHTTPSCertificateForAllowedHosts && m_allowedHosts.find(host) != m_allowedHosts.end());
    }
    return scheme <= kWKProtectionSpaceAuthenticationSchemeHTTPDigest || scheme == kWKProtectionSpaceAuthenticationSchemeOAuth;
}

void TestController::didFinishNavigation(WKPageRef page, WKNavigationRef navigation)
{
    if (m_state != Resetting)
        return;

    if (!WKURLIsEqual(adoptWK(WKFrameCopyURL(WKPageGetMainFrame(page))).get(), blankURL()))
        return;

    m_doneResetting = true;
    singleton().notifyDone();
}

void TestController::didFailProvisionalNavigation(WKPageRef page, WKErrorRef error)
{
    if (m_usingServerMode)
        return;

    auto failingURL = adoptWK(WKErrorCopyFailingURL(error));
    if (!m_mainResourceURL || !failingURL || !WKURLIsEqual(failingURL.get(), m_mainResourceURL.get()))
        return;

    auto failingURLString = toWTFString(adoptWK(WKURLCopyString(failingURL.get())));
    auto errorDomain = toWTFString(adoptWK(WKErrorCopyDomain(error)));
    auto errorDescription = toWTFString(adoptWK(WKErrorCopyLocalizedDescription(error)));
    int errorCode = WKErrorGetErrorCode(error);
    auto errorMessage = makeString("Failed: "_s, errorDescription, " (errorDomain="_s, errorDomain, ", code="_s, errorCode, ") for URL "_s, failingURLString);
    printf("%s\n", errorMessage.utf8().data());
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
            auto credential = adoptWK(WKCredentialCreate(toWK("accept server trust").get(), toWK("").get(), kWKCredentialPersistenceNone));
            WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
            return;
        }
        WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(decisionListener);
        return;
    }

    if (m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges) {
        m_currentInvocation->outputText("Simulating reject protection space and continue for authentication challenge\n"_s);
        WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(decisionListener);
        return;
    }

    auto host = toWTFString(adoptWK(WKProtectionSpaceCopyHost(protectionSpace)).get());
    int port = WKProtectionSpaceGetPort(protectionSpace);
    StringBuilder message;
    message.append(host, ':', port, " - didReceiveAuthenticationChallenge - "_s, toString(authenticationScheme), " - "_s);
    if (!m_handlesAuthenticationChallenges)
        message.append("Simulating cancelled authentication sheet\n"_s);
    else
        message.append("Responding with "_s, m_authenticationUsername, ':', m_authenticationPassword, '\n');
    m_currentInvocation->outputText(message.toString());

    if (!m_handlesAuthenticationChallenges) {
        WKAuthenticationDecisionListenerUseCredential(decisionListener, 0);
        return;
    }
    auto credential = adoptWK(WKCredentialCreate(toWK(m_authenticationUsername).get(), toWK(m_authenticationPassword).get(), kWKCredentialPersistenceForSession));
    WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
}


// WKDownloadClient
    
WKStringRef TestController::decideDestinationWithSuggestedFilename(WKDownloadRef download, WKURLResponseRef response, WKStringRef suggestedFilename, const void* clientInfo)
{
    return static_cast<TestController*>(const_cast<void*>(clientInfo))->decideDestinationWithSuggestedFilename(download, suggestedFilename);
}

void TestController::downloadDidFinish(WKDownloadRef download, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidFinish(download);
}

void TestController::downloadDidFail(WKDownloadRef download, WKErrorRef error, WKDataRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidFail(download, error);
}

bool TestController::downloadDidReceiveServerRedirectToURL(WKDownloadRef download, WKURLResponseRef, WKURLRequestRef newRequest, const void* clientInfo)
{
    return static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidReceiveServerRedirectToURL(download, newRequest);
}

void TestController::downloadDidStart(WKDownloadRef download)
{
    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText("Download started.\n"_s);
}

WKStringRef TestController::decideDestinationWithSuggestedFilename(WKDownloadRef download, WKStringRef filename)
{
    auto suggestedFilename = toWTFString(filename);

    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText(makeString("Downloading URL with suggested filename \""_s, suggestedFilename, "\"\n"_s));

    const char* dumpRenderTreeTemp = libraryPathForTesting();
    if (!dumpRenderTreeTemp)
        return nullptr;

    auto temporaryFolder = String::fromUTF8(dumpRenderTreeTemp);
    if (suggestedFilename.isEmpty())
        suggestedFilename = "Unknown"_s;
    
    auto destination = makeString(temporaryFolder, pathSeparator, suggestedFilename);
    if (auto downloadIndex = m_downloadIndex++)
        destination = makeString(destination, downloadIndex);
    if (FileSystem::fileExists(destination))
        FileSystem::deleteFile(destination);

    return toWK(destination).leakRef();
}

void TestController::downloadDidFinish(WKDownloadRef)
{
    if (m_shouldLogDownloadSize)
        m_currentInvocation->outputText(makeString("Download size: "_s, m_downloadTotalBytesWritten.value_or(0), ".\n"_s));
    if (m_shouldLogDownloadExpectedSize)
        m_currentInvocation->outputText(makeString("Download expected size: "_s, m_downloadTotalBytesExpectedToWrite.value_or(0), ".\n"_s));
    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText("Download completed.\n"_s);
    m_currentInvocation->notifyDownloadDone();
}

bool TestController::downloadDidReceiveServerRedirectToURL(WKDownloadRef, WKURLRequestRef request)
{
    auto url = adoptWK(WKURLRequestCopyURL(request));
    if (m_shouldLogDownloadCallbacks)
        m_currentInvocation->outputText(makeString("Download was redirected to \""_s, toWTFString(adoptWK(WKURLCopyString(url.get()))), "\".\n"_s));
    return true;
}

void TestController::downloadDidFail(WKDownloadRef, WKErrorRef error)
{
    if (m_shouldLogDownloadCallbacks) {
        m_currentInvocation->outputText("Download failed.\n"_s);

        auto domain = toWTFString(adoptWK(WKErrorCopyDomain(error)));
        auto description = toWTFString(adoptWK(WKErrorCopyLocalizedDescription(error)));
        int code = WKErrorGetErrorCode(error);

        m_currentInvocation->outputText(makeString("Failed: "_s, domain, ", code="_s, code, ", description="_s, description, '\n'));
    }
    m_currentInvocation->notifyDownloadDone();
}

void TestController::receivedServiceWorkerConsoleMessage(const String& message)
{
    m_currentInvocation->outputText(makeString("Received ServiceWorker Console Message: "_s, message, '\n'));
}

void TestController::downloadDidReceiveAuthenticationChallenge(WKDownloadRef, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->didReceiveAuthenticationChallenge(nullptr, authenticationChallenge);
}

void TestController::downloadDidWriteData(long long totalBytesWritten, long long totalBytesExpectedToWrite)
{
    if (!m_shouldLogDownloadCallbacks)
        return;
    m_downloadTotalBytesWritten = totalBytesWritten;
    m_downloadTotalBytesExpectedToWrite = totalBytesExpectedToWrite;
}

void TestController::downloadDidWriteData(WKDownloadRef download, long long bytesWritten, long long totalBytesWritten, long long totalBytesExpectedToWrite, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->downloadDidWriteData(totalBytesWritten, totalBytesExpectedToWrite);
}

void TestController::webProcessDidTerminate(WKProcessTerminationReason reason)
{
    // This function can be called multiple times when crash logs are being saved on Windows, so
    // ensure we only print the crashed message once.
    if (!m_didPrintWebProcessCrashedMessage) {
        pid_t pid = WKPageGetProcessIdentifier(m_mainWebView->page());
        fprintf(stderr, "%s terminated (pid %ld) for reason: %s\n", webProcessName().characters(), static_cast<long>(pid), terminationReasonToString(reason));
        if (reason == kWKProcessTerminationReasonRequestedByClient) {
            fflush(stderr);
            return;
        }

        fprintf(stderr, "#CRASHED - %s (pid %ld)\n", webProcessName().characters(), static_cast<long>(pid));
        fflush(stderr);
        m_didPrintWebProcessCrashedMessage = true;
    }

    if (m_shouldExitWhenAuxiliaryProcessCrashes)
        exitProcess(1);
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

void TestController::simulateWebNotificationClick(WKDataRef notificationID)
{
    m_webNotificationProvider.simulateWebNotificationClick(mainWebView()->page(), notificationID);
}

void TestController::simulateWebNotificationClickForServiceWorkerNotifications()
{
    m_webNotificationProvider.simulateWebNotificationClickForServiceWorkerNotifications();
}

void TestController::setGeolocationPermission(bool enabled)
{
    bool permissionChanged = false;
    if (!m_isGeolocationPermissionSet || m_isGeolocationPermissionAllowed != enabled)
        permissionChanged = true;

    m_isGeolocationPermissionSet = true;
    m_isGeolocationPermissionAllowed = enabled;
    decidePolicyForGeolocationPermissionRequestIfPossible();

    if (!permissionChanged)
        return;

    for (auto& originString : m_geolocationPermissionQueryOrigins)
        WKPagePermissionChanged(toWK("geolocation").get(), toWK(originString).get());
}

void TestController::setScreenWakeLockPermission(bool enabled)
{
    m_screenWakeLockPermission = enabled;
}

void TestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy, std::optional<double> altitude, std::optional<double> altitudeAccuracy, std::optional<double> heading, std::optional<double> speed, std::optional<double> floorLevel)
{
    m_geolocationProvider->setPosition(latitude, longitude, accuracy, altitude, altitudeAccuracy, heading, speed, floorLevel);
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

void TestController::setCameraPermission(bool enabled)
{
    m_isUserMediaPermissionSet = true;
    m_isCameraPermissionAllowed = enabled;
    decidePolicyForUserMediaPermissionRequestIfPossible();
}

void TestController::setMicrophonePermission(bool enabled)
{
    m_isUserMediaPermissionSet = true;
    m_isMicrophonePermissionAllowed = enabled;
    decidePolicyForUserMediaPermissionRequestIfPossible();
}

void TestController::resetUserMediaPermission()
{
    m_isUserMediaPermissionSet = false;
    m_isCameraPermissionAllowed = true;
    m_isMicrophonePermissionAllowed = true;
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
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    uint64_t frameIdentifier = WKFrameHandleGetFrameID(frameHandle.get());
ALLOW_DEPRECATED_DECLARATIONS_END
    String frameSalt = ephemeralSalts.get(frameIdentifier);

    if (settings.persistentPermission()) {
        if (frameSalt.length())
            return frameSalt;

        if (!settings.persistentSalt().length())
            settings.setPersistentSalt(createVersion4UUIDString());

        return settings.persistentSalt();
    }

    if (!frameSalt.length()) {
        frameSalt = createVersion4UUIDString();
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
    WKUserMediaPermissionCheckSetUserMediaAccessInfo(checkRequest, toWK(salt).get(), settingsForOrigin(originHash).persistentPermission());
}

bool TestController::handleDeviceOrientationAndMotionAccessRequest(WKSecurityOriginRef origin, WKFrameInfoRef frame)
{
    auto frameOrigin = adoptWK(WKFrameInfoCopySecurityOrigin(frame));
    m_currentInvocation->outputText(makeString("Received device orientation & motion access request for top level origin \""_s, originUserVisibleName(origin), "\", with frame origin \""_s, originUserVisibleName(frameOrigin.get()), "\".\n"_s));
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

        if (m_isUserMediaPermissionSet && WKUserMediaPermissionRequestRequiresCameraCapture(request) && !m_isCameraPermissionAllowed) {
            WKUserMediaPermissionRequestDeny(request, kWKPermissionDenied);
            continue;
        }

        if (m_isUserMediaPermissionSet && WKUserMediaPermissionRequestRequiresMicrophoneCapture(request) && !m_isMicrophonePermissionAllowed) {
            WKUserMediaPermissionRequestDeny(request, kWKPermissionDenied);
            continue;
        }

        auto audioDeviceUIDs = adoptWK(WKUserMediaPermissionRequestAudioDeviceUIDs(request));
        auto videoDeviceUIDs = adoptWK(WKUserMediaPermissionRequestVideoDeviceUIDs(request));

        if (!WKUserMediaPermissionRequestRequiresDisplayCapture(request) && !WKArrayGetSize(videoDeviceUIDs.get()) && !WKArrayGetSize(audioDeviceUIDs.get())) {
            WKUserMediaPermissionRequestDeny(request, kWKNoConstraints);
            continue;
        }

        WKRetainPtr<WKStringRef> videoDeviceUID;
        if (WKArrayGetSize(videoDeviceUIDs.get()))
            videoDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(videoDeviceUIDs.get(), 0));
        else
            videoDeviceUID = toWK("");

        WKRetainPtr<WKStringRef> audioDeviceUID;
        if (WKArrayGetSize(audioDeviceUIDs.get()))
            audioDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(audioDeviceUIDs.get(), 0));
        else
            audioDeviceUID = toWK("");

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

void TestController::decidePolicyForNotificationPermissionRequest(WKPageRef, WKSecurityOriginRef origin, WKNotificationPermissionRequestRef request)
{
    auto originName = originUserVisibleName(origin);
    auto securityOriginString = adoptWK(WKSecurityOriginCopyToString(origin));
    auto permissionState = m_webNotificationProvider.permissionState(origin);

    if (permissionState && !permissionState.value()) {
        WKNotificationPermissionRequestDeny(request);
        return;
    }

    if (m_notificationOriginsToDenyOnPrompt.contains(originName)) {
        m_webNotificationProvider.setPermission(toWTFString(securityOriginString.get()), false);
        WKNotificationPermissionRequestDeny(request);
        return;
    }

    m_webNotificationProvider.setPermission(toWTFString(securityOriginString.get()), true);
    WKNotificationPermissionRequestAllow(request);
}

void TestController::unavailablePluginButtonClicked(WKPageRef, WKPluginUnavailabilityReason, WKDictionaryRef, const void*)
{
    printf("MISSING PLUGIN BUTTON PRESSED\n");
}

void TestController::decidePolicyForNavigationAction(WKPageRef page, WKNavigationActionRef navigationAction, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo)
{
    static_cast<TestController*>(const_cast<void*>(clientInfo))->decidePolicyForNavigationAction(page, navigationAction, listener);
}

static inline bool isLocalFileScheme(WKStringRef scheme)
{
    return WKStringIsEqualToUTF8CStringIgnoringCase(scheme, "file");
}

WTF::String pathSuitableForTestResult(WKURLRef fileURL, WKPageRef page)
{
    if (!fileURL)
        return "(null)"_s;

    auto schemeString = adoptWK(WKURLCopyScheme(fileURL));
    if (!isLocalFileScheme(schemeString.get()))
        return toWTFString(adoptWK(WKURLCopyString(fileURL)));

    WKFrameRef mainFrame = WKPageGetMainFrame(page);
    auto mainFrameURL = adoptWK(WKFrameCopyURL(mainFrame));
    if (!mainFrameURL)
        mainFrameURL = adoptWK(WKFrameCopyProvisionalURL(mainFrame));

    String pathString = toWTFString(adoptWK(WKURLCopyPath(fileURL)));
    String mainFrameURLPathString = mainFrameURL ? toWTFString(adoptWK(WKURLCopyPath(mainFrameURL.get()))) : ""_s;
    auto basePath = StringView(mainFrameURLPathString).left(mainFrameURLPathString.reverseFind('/') + 1);

    if (!basePath.isEmpty() && pathString.startsWith(basePath))
        return pathString.substring(basePath.length());
    return toWTFString(adoptWK(WKURLCopyLastPathComponent(fileURL))); // We lose some information here, but it's better than exposing a full path, which is always machine specific.
}

static String string(WKURLRequestRef request, WKPageRef page)
{
    auto url = adoptWK(WKURLRequestCopyURL(request));
    auto firstParty = adoptWK(WKURLRequestCopyFirstPartyForCookies(request));
    auto httpMethod = adoptWK(WKURLRequestCopyHTTPMethod(request));
    return makeString("<NSURLRequest URL "_s, pathSuitableForTestResult(url.get(), page),
        ", main document URL "_s, pathSuitableForTestResult(firstParty.get(), page),
        ", http method "_s, WKStringIsEmpty(httpMethod.get()) ? "(none)"_s : ""_s, toWTFString(httpMethod.get()), '>');
}

static ASCIILiteral navigationTypeToString(WKFrameNavigationType type)
{
    switch (type) {
    case kWKFrameNavigationTypeLinkClicked:
        return "link clicked"_s;
    case kWKFrameNavigationTypeFormSubmitted:
        return "form submitted"_s;
    case kWKFrameNavigationTypeBackForward:
        return "back/forward"_s;
    case kWKFrameNavigationTypeReload:
        return "reload"_s;
    case kWKFrameNavigationTypeFormResubmitted:
        return "form resubmitted"_s;
    case kWKFrameNavigationTypeOther:
        return "other"_s;
    }
    return "illegal value"_s;
}

void TestController::decidePolicyForNavigationAction(WKPageRef page, WKNavigationActionRef navigationAction, WKFramePolicyListenerRef listener)
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

    auto request = adoptWK(WKNavigationActionCopyRequest(navigationAction));
    if (auto targetFrame = adoptWK(WKNavigationActionCopyTargetFrameInfo(navigationAction)); targetFrame && m_dumpPolicyDelegateCallbacks) {
        m_currentInvocation->outputText(makeString(" - decidePolicyForNavigationAction\n"_s, string(request.get(), page),
            " is main frame - "_s, targetFrame && WKFrameInfoGetIsMainFrame(targetFrame.get()) ? "yes"_s : "no"_s,
            " should open URLs externally - "_s, WKNavigationActionGetShouldOpenExternalSchemes(navigationAction) ? "yes"_s : "no"_s, '\n'));
    }

    if (m_policyDelegateEnabled) {
        auto url = adoptWK(WKURLRequestCopyURL(request.get()));
        auto urlScheme = adoptWK(WKURLCopyScheme(url.get()));

        StringBuilder stringBuilder;
        stringBuilder.append("Policy delegate: attempt to load "_s);
        if (isLocalFileScheme(urlScheme.get()))
            stringBuilder.append(toWTFString(adoptWK(WKURLCopyLastPathComponent(url.get())).get()));
        else
            stringBuilder.append(toWTFString(adoptWK(WKURLCopyString(url.get())).get()));
        stringBuilder.append(" with navigation type \'"_s, navigationTypeToString(WKNavigationActionGetNavigationType(navigationAction)), '\'');
        stringBuilder.append('\n');
        m_currentInvocation->outputText(stringBuilder.toString());
        if (!m_skipPolicyDelegateNotifyDone)
            WKPagePostMessageToInjectedBundle(mainWebView()->page(), toWK("NotifyDone").get(), nullptr);
    }

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
    auto response = adoptWK(WKNavigationResponseCopyResponse(navigationResponse));

    bool shouldDownloadUndisplayableMIMETypes = m_shouldDownloadUndisplayableMIMETypes;
    bool responseIsAttachment = WKURLResponseIsAttachment(response.get());
    auto decisionFunction = [shouldDownloadUndisplayableMIMETypes, retainedNavigationResponse, retainedListener, responseIsAttachment, shouldDownloadContentDispositionAttachments = m_shouldDownloadContentDispositionAttachments]() {
        if (responseIsAttachment && shouldDownloadContentDispositionAttachments) {
            WKFramePolicyListenerDownload(retainedListener.get());
            return;
        }

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

    if (m_policyDelegateEnabled) {
        if (responseIsAttachment)
            m_currentInvocation->outputText(makeString("Policy delegate: resource is an attachment, suggested file name \'"_s, toWTFString(adoptWK(WKURLResponseCopySuggestedFilename(response.get())).get()), "'\n"_s));
    }

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

    auto url = adoptWK(WKNavigationDataCopyURL(navigationData));
    auto urlString = toWTFString(adoptWK(WKURLCopyString(url.get())));
    auto title = toWTFString(adoptWK(WKNavigationDataCopyTitle(navigationData)));
    auto request = adoptWK(WKNavigationDataCopyOriginalRequest(navigationData));
    auto method = toWTFString(adoptWK(WKURLRequestCopyHTTPMethod(request.get())));

    // FIXME: Determine whether the navigation was successful / a client redirect rather than hard-coding the message here.
    m_currentInvocation->outputText(makeString("WebView navigated to url \""_s, urlString, "\" with title \""_s, title, "\" with HTTP equivalent method \""_s, method,
        "\".  The navigation was successful and was not a client redirect.\n"_s));
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

    m_currentInvocation->outputText(makeString("WebView performed a client redirect from \""_s, source, "\" to \""_s, destination, "\".\n"_s));
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

    m_currentInvocation->outputText(makeString("WebView performed a server redirect from \""_s, source, "\" to \""_s, destination, "\".\n"_s));
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
    m_currentInvocation->outputText(makeString("WebView updated the title for history URL \""_s, urlString, "\" to \""_s, toWTFString(title), "\".\n"_s));
}

void TestController::setNavigationGesturesEnabled(bool value)
{
    m_mainWebView->setNavigationGesturesEnabled(value);
}

void TestController::setIgnoresViewportScaleLimits(bool ignoresViewportScaleLimits)
{
    WKPageSetIgnoresViewportScaleLimits(m_mainWebView->page(), ignoresViewportScaleLimits);
}

void TestController::setUseDarkAppearanceForTesting(bool useDarkAppearance)
{
    WKPageSetUseDarkAppearanceForTesting(m_mainWebView->page(), useDarkAppearance);
}

void TestController::terminateGPUProcess()
{
    WKContextTerminateGPUProcess(platformContext());
}

void TestController::terminateNetworkProcess()
{
    WKWebsiteDataStoreTerminateNetworkProcess(websiteDataStore());
}

void TestController::terminateServiceWorkers()
{
    WKContextTerminateServiceWorkers(platformContext());
}

#if !PLATFORM(COCOA)
void TestController::platformWillRunTest(const TestInvocation&)
{
}

void TestController::platformInitializeDataStore(WKPageConfigurationRef configuration, const TestOptions& options)
{
    if (!options.useEphemeralSession())
        WKPageConfigurationSetWebsiteDataStore(configuration, defaultWebsiteDataStore());

    m_websiteDataStore = WKPageConfigurationGetWebsiteDataStore(configuration);
}

void TestController::platformCreateWebView(WKPageConfigurationRef configuration, const TestOptions& options)
{
    m_mainWebView = makeUnique<PlatformWebView>(configuration, options);
}

UniqueRef<PlatformWebView> TestController::platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef configuration, const TestOptions& options)
{
    return makeUniqueRef<PlatformWebView>(configuration, options);
}

WKContextRef TestController::platformAdjustContext(WKContextRef context, WKContextConfigurationRef)
{
    m_preferences = adoptWK(WKPreferencesCreate());
    return context;
}

unsigned TestController::imageCountInGeneralPasteboard() const
{
    return 0;
}

void TestController::removeAllSessionCredentials(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    completionHandler(nullptr);
}

bool TestController::didLoadAppInitiatedRequest()
{
    return false;
}

bool TestController::didLoadNonAppInitiatedRequest()
{
    return false;
}

void TestController::clearAppPrivacyReportTestingData()
{
}

#endif // !PLATFORM(COCOA)

void TestController::setPageScaleFactor(float scaleFactor, int x, int y, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKPageSetPageScaleFactorForTesting(mainWebView()->page(), scaleFactor, WKPointMake(x, y), completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::getAllStorageAccessEntries(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    auto context = completionHandler.leak();
    WKWebsiteDataStoreGetAllStorageAccessEntries(websiteDataStore(), m_mainWebView->page(), context, [] (void* context, WKArrayRef domainList) {
        auto completionHandler = WTF::adopt(static_cast<CompletionHandler<void(WKTypeRef)>::Impl*>(context));
        completionHandler(domainList);
    });
}

void TestController::loadedSubresourceDomains(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKPageLoadedSubresourceDomains(m_mainWebView->page(), [] (WKArrayRef domains, void* context) {
        auto completionHandler = WTF::adopt(static_cast<CompletionHandler<void(WKTypeRef)>::Impl*>(context));
        completionHandler(domains);
    }, completionHandler.leak());
}

void TestController::clearLoadedSubresourceDomains()
{
    WKPageClearLoadedSubresourceDomains(m_mainWebView->page());
}

void TestController::reloadFromOrigin()
{
    WKPageReloadFromOrigin(m_mainWebView->page());
}

struct GenericVoidContext {
    explicit GenericVoidContext(TestController& controller)
        : testController(controller)
    {
    }

    TestController& testController;
    bool done { false };
};

static void genericVoidCallback(void* userData)
{
    auto* context = static_cast<GenericVoidContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

void TestController::clearServiceWorkerRegistrations()
{
    GenericVoidContext context(*this);

    WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(websiteDataStore(), &context, genericVoidCallback);
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

void TestController::clearMemoryCache()
{
    ClearDOMCacheCallbackContext context(*this);

    WKWebsiteDataStoreRemoveMemoryCaches(websiteDataStore(), &context, clearDOMCacheCallback);
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
}

void TestController::syncLocalStorage()
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreSyncLocalStorage(TestController::websiteDataStore(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);
}

void TestController::resetQuota()
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreResetQuota(TestController::websiteDataStore(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);
}

void TestController::resetStoragePersistedState()
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreResetStoragePersistedState(TestController::websiteDataStore(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);
}

void TestController::clearStorage()
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreClearStorage(TestController::websiteDataStore(), &context, StorageVoidCallback);
    runUntil(context.done, noTimeout);
}

void TestController::setOriginQuotaRatioEnabled(bool enabled)
{
    StorageVoidCallbackContext context(*this);
    WKWebsiteDataStoreSetOriginQuotaRatioEnabled(websiteDataStore(), enabled, &context, StorageVoidCallback);
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

void TestController::setQuota(uint64_t)
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

void TestController::setStatisticsDebugMode(bool value, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(websiteDataStore(), value, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsPrevalentResourceForDebugMode(WKStringRef hostName, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(websiteDataStore(), hostName, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsLastSeen(WKStringRef host, double seconds, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetStatisticsLastSeen(websiteDataStore(), host, seconds, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsMergeStatistic(WKStringRef host, WKStringRef topFrameDomain1, WKStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, int dataRecordsRemoved, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetStatisticsMergeStatistic(websiteDataStore(), host, topFrameDomain1, topFrameDomain2, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsExpiredStatistic(WKStringRef host, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetStatisticsExpiredStatistic(websiteDataStore(), host, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsPrevalentResource(WKStringRef host, bool value, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetStatisticsPrevalentResource(websiteDataStore(), host, value, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsVeryPrevalentResource(WKStringRef host, bool value, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(websiteDataStore(), host, value, completionHandler.leak(), adoptAndCallCompletionHandler);
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

void TestController::setStatisticsHasHadUserInteraction(WKStringRef host, bool value, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(websiteDataStore(), host, value, completionHandler.leak(), adoptAndCallCompletionHandler);
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

void TestController::setStatisticsCrossSiteLoadWithLinkDecoration(WKStringRef fromHost, WKStringRef toHost, bool wasFiltered)
{
    ResourceStatisticsCallbackContext context(*this);
#if PLATFORM(COCOA)
    platformSetStatisticsCrossSiteLoadWithLinkDecoration(fromHost, toHost, wasFiltered, &context, &resourceStatisticsVoidResultCallback);
#else
    WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(websiteDataStore(), fromHost, toHost, wasFiltered, &context, resourceStatisticsVoidResultCallback);
#endif
    runUntil(context.done, noTimeout);
}

void TestController::setStatisticsTimeToLiveUserInteraction(double seconds)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(websiteDataStore(), seconds, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::statisticsProcessStatisticsAndDataRecords(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(websiteDataStore(), completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::statisticsUpdateCookieBlocking(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreStatisticsUpdateCookieBlocking(websiteDataStore(), completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsTimeAdvanceForTesting(double value)
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreSetResourceLoadStatisticsTimeAdvanceForTesting(websiteDataStore(), value, &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
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

void TestController::statisticsClearInMemoryAndPersistentStore(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(websiteDataStore(), completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(websiteDataStore(), hours, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::statisticsClearThroughWebsiteDataRemoval(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(websiteDataStore(), completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::statisticsDeleteCookiesForHost(WKStringRef host, bool includeHttpOnlyCookies, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(websiteDataStore(), host, includeHttpOnlyCookies, completionHandler.leak(), adoptAndCallCompletionHandler);
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

void TestController::setStatisticsShouldDowngradeReferrer(bool value, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(websiteDataStore(), value, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsShouldBlockThirdPartyCookies(bool value, bool onlyOnSitesWithoutUserInteraction, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(websiteDataStore(), value, onlyOnSitesWithoutUserInteraction, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsFirstPartyWebsiteDataRemovalMode(bool value, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(websiteDataStore(), value, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsToSameSiteStrictCookies(WKStringRef hostName, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(websiteDataStore(), hostName, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsFirstPartyHostCNAMEDomain(WKStringRef firstPartyURLString, WKStringRef cnameURLString, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(websiteDataStore(), firstPartyURLString, cnameURLString, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setStatisticsThirdPartyCNAMEDomain(WKStringRef cnameURLString, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(websiteDataStore(), cnameURLString, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setAppBoundDomains(WKArrayRef originURLs, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetAppBoundDomainsForTesting(originURLs, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::setManagedDomains(WKArrayRef originURLs, CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKWebsiteDataStoreSetManagedDomainsForTesting(originURLs, completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::statisticsResetToConsistentState()
{
    ResourceStatisticsCallbackContext context(*this);
    WKWebsiteDataStoreStatisticsResetToConsistentState(websiteDataStore(), &context, resourceStatisticsVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::removeAllCookies(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    WKHTTPCookieStoreDeleteAllCookies(WKWebsiteDataStoreGetHTTPCookieStore(websiteDataStore()), completionHandler.leak(), adoptAndCallCompletionHandler);
}

void TestController::addMockMediaDevice(WKStringRef persistentID, WKStringRef label, WKStringRef type, WKDictionaryRef properties)
{
    WKAddMockMediaDevice(platformContext(), persistentID, label, type, properties);
}

void TestController::clearMockMediaDevices()
{
    WKClearMockMediaDevices(platformContext());
}

void TestController::removeMockMediaDevice(WKStringRef persistentID)
{
    WKRemoveMockMediaDevice(platformContext(), persistentID);
}

void TestController::setMockMediaDeviceIsEphemeral(WKStringRef persistentID, bool isEphemeral)
{
    WKSetMockMediaDeviceIsEphemeral(platformContext(), persistentID, isEphemeral);
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

void TestController::setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted)
{
    WKPageSetMockCaptureDevicesInterrupted(m_mainWebView->page(), isCameraInterrupted, isMicrophoneInterrupted);
}

void TestController::triggerMockCaptureConfigurationChange(bool forMicrophone, bool forDisplay)
{
    WKPageTriggerMockCaptureConfigurationChange(m_mainWebView->page(), forMicrophone, forDisplay);
}

void TestController::setCaptureState(bool cameraState, bool microphoneState, bool displayState)
{
    WKPageSetMuted(m_mainWebView->page(), (cameraState ? kWKMediaCameraCaptureUnmuted : kWKMediaCameraCaptureMuted) | (microphoneState ? kWKMediaMicrophoneCaptureUnmuted : kWKMediaMicrophoneCaptureMuted) | (displayState ? kWKMediaScreenCaptureUnmuted : kWKMediaScreenCaptureMuted));
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

void TestController::updateBundleIdentifierInNetworkProcess(const std::string& bundleIdentifier)
{
    InAppBrowserPrivacyCallbackContext context(*this);
    WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(TestController::websiteDataStore(), toWK(bundleIdentifier).get(), &context, inAppBrowserPrivacyVoidResultCallback);
    runUntil(context.done, noTimeout);
}

void TestController::clearBundleIdentifierInNetworkProcess()
{
    InAppBrowserPrivacyCallbackContext context(*this);
    WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(TestController::websiteDataStore(), &context, inAppBrowserPrivacyVoidResultCallback);
    runUntil(context.done, noTimeout);
}

#if !PLATFORM(COCOA)
TestFeatures TestController::platformSpecificFeatureOverridesDefaultsForTest(const TestCommand&) const
{
    return { };
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

void TestController::setAllowedMenuActions(const Vector<String>&)
{
}
#endif

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WPE)
WKRetainPtr<WKStringRef> TestController::takeViewPortSnapshot()
{
    return adoptWK(WKStringCreateWithUTF8CString("not implemented"));
}
#endif

#if !PLATFORM(COCOA)
WKRetainPtr<WKArrayRef> TestController::getAndClearReportedWindowProxyAccessDomains()
{
    return nullptr;
}
#endif

void TestController::setServiceWorkerFetchTimeoutForTesting(double seconds)
{
    WKWebsiteDataStoreSetServiceWorkerFetchTimeoutForTesting(websiteDataStore(), seconds);
}

struct PrivateClickMeasurementStringResultCallbackContext {
    explicit PrivateClickMeasurementStringResultCallbackContext(TestController& controller)
        : testController(controller)
    {
    }
    
    TestController& testController;
    bool done { false };
    WKRetainPtr<WKStringRef> privateClickMeasurementRepresentation;
};

static void privateClickMeasurementStringResultCallback(WKStringRef privateClickMeasurementRepresentation, void* userData)
{
    auto* context = static_cast<PrivateClickMeasurementStringResultCallbackContext*>(userData);
    context->privateClickMeasurementRepresentation = privateClickMeasurementRepresentation;
    context->done = true;
    context->testController.notifyDone();
}

String TestController::dumpPrivateClickMeasurement()
{
    PrivateClickMeasurementStringResultCallbackContext callbackContext(*this);
    WKPageDumpPrivateClickMeasurement(m_mainWebView->page(), privateClickMeasurementStringResultCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
    return toWTFString(callbackContext.privateClickMeasurementRepresentation.get());
}

struct PrivateClickMeasurementVoidCallbackContext {
    explicit PrivateClickMeasurementVoidCallbackContext(TestController& controller)
        : testController(controller)
    {
    }
    
    TestController& testController;
    bool done { false };
};

static void privateClickMeasurementVoidCallback(void* userData)
{
    auto* context = static_cast<PrivateClickMeasurementVoidCallbackContext*>(userData);
    context->done = true;
    context->testController.notifyDone();
}

void TestController::clearPrivateClickMeasurement()
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageClearPrivateClickMeasurement(m_mainWebView->page(), privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::clearPrivateClickMeasurementsThroughWebsiteDataRemoval()
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKWebsiteDataStoreClearPrivateClickMeasurementsThroughWebsiteDataRemoval(websiteDataStore(), &callbackContext, privateClickMeasurementVoidCallback);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setPrivateClickMeasurementOverrideTimerForTesting(bool value)
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSetPrivateClickMeasurementOverrideTimerForTesting(m_mainWebView->page(), value, privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::markAttributedPrivateClickMeasurementsAsExpiredForTesting()
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageMarkAttributedPrivateClickMeasurementsAsExpiredForTesting(m_mainWebView->page(), privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setPrivateClickMeasurementEphemeralMeasurementForTesting(bool value)
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSetPrivateClickMeasurementEphemeralMeasurementForTesting(m_mainWebView->page(), value, privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::simulatePrivateClickMeasurementSessionRestart()
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSimulatePrivateClickMeasurementSessionRestart(m_mainWebView->page(), privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setPrivateClickMeasurementTokenPublicKeyURLForTesting(WKURLRef url)
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSetPrivateClickMeasurementTokenPublicKeyURLForTesting(m_mainWebView->page(), url, privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setPrivateClickMeasurementTokenSignatureURLForTesting(WKURLRef url)
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSetPrivateClickMeasurementTokenSignatureURLForTesting(m_mainWebView->page(), url, privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setPrivateClickMeasurementAttributionReportURLsForTesting(WKURLRef sourceURL, WKURLRef destinationURL)
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSetPrivateClickMeasurementAttributionReportURLsForTesting(m_mainWebView->page(), sourceURL, destinationURL, privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::markPrivateClickMeasurementsAsExpiredForTesting()
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageMarkPrivateClickMeasurementsAsExpiredForTesting(m_mainWebView->page(), privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setPCMFraudPreventionValuesForTesting(WKStringRef unlinkableToken, WKStringRef secretToken, WKStringRef signature, WKStringRef keyID)
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSetPCMFraudPreventionValuesForTesting(m_mainWebView->page(), unlinkableToken, secretToken, signature, keyID, privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

void TestController::setPrivateClickMeasurementAppBundleIDForTesting(WKStringRef appBundleID)
{
    PrivateClickMeasurementVoidCallbackContext callbackContext(*this);
    WKPageSetPrivateClickMeasurementAppBundleIDForTesting(m_mainWebView->page(), appBundleID, privateClickMeasurementVoidCallback, &callbackContext);
    runUntil(callbackContext.done, noTimeout);
}

WKURLRef TestController::currentTestURL() const
{
    return m_currentInvocation ? m_currentInvocation->url() : nullptr;
}

void TestController::setShouldAllowDeviceOrientationAndMotionAccess(bool value)
{
    m_shouldAllowDeviceOrientationAndMotionAccess = value;
    WKWebsiteDataStoreClearAllDeviceOrientationPermissions(websiteDataStore());
}

void TestController::setRequestStorageAccessThrowsExceptionUntilReload(bool enabled)
{
    auto configuration = adoptWK(WKPageCopyPageConfiguration(m_mainWebView->page()));
    auto preferences = WKPageConfigurationGetPreferences(configuration.get());
    WKPreferencesSetBoolValueForKeyForTesting(preferences, enabled, toWK("RequestStorageAccessThrowsExceptionUntilReload").get());
}

} // namespace WTR
