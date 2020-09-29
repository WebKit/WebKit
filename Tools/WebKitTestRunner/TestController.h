/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "GeolocationProviderMock.h"
#include "TestOptions.h"
#include "WebNotificationProvider.h"
#include "WorkQueueManager.h"
#include <WebKit/WKRetainPtr.h>
#include <set>
#include <string>
#include <vector>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "ClassMethodSwizzler.h"
#include "InstanceMethodSwizzler.h"
#endif

OBJC_CLASS NSString;
OBJC_CLASS UIKeyboardInputMode;
OBJC_CLASS WKWebViewConfiguration;

namespace WTR {

class TestInvocation;
class OriginSettings;
class PlatformWebView;
class EventSenderProxy;
struct TestCommand;
struct TestOptions;

class AsyncTask {
public:
    AsyncTask(WTF::Function<void ()>&& task, WTF::Seconds timeout)
        : m_task(WTFMove(task))
        , m_timeout(timeout)
    {
        ASSERT(!currentTask());
    }

    // Returns false on timeout.
    bool run();

    void taskComplete()
    {
        m_taskDone = true;
    }

    static AsyncTask* currentTask();

private:
    static AsyncTask* m_currentTask;

    WTF::Function<void ()> m_task;
    WTF::Seconds m_timeout;
    bool m_taskDone { false };
};

// FIXME: Rename this TestRunner?
class TestController {
public:
    static TestController& singleton();
    static void configureWebsiteDataStoreTemporaryDirectories(WKWebsiteDataStoreConfigurationRef);
    static WKWebsiteDataStoreRef defaultWebsiteDataStore();

    static const unsigned viewWidth;
    static const unsigned viewHeight;

    static const unsigned w3cSVGViewWidth;
    static const unsigned w3cSVGViewHeight;

    static const WTF::Seconds defaultShortTimeout;
    static const WTF::Seconds noTimeout;

    TestController(int argc, const char* argv[]);
    ~TestController();

    bool verbose() const { return m_verbose; }

    WKStringRef injectedBundlePath() const { return m_injectedBundlePath.get(); }
    WKStringRef testPluginDirectory() const { return m_testPluginDirectory.get(); }

    PlatformWebView* mainWebView() { return m_mainWebView.get(); }
    WKContextRef context() { return m_context.get(); }
    WKUserContentControllerRef userContentController() { return m_userContentController.get(); }

    WKWebsiteDataStoreRef websiteDataStore();

    EventSenderProxy* eventSenderProxy() { return m_eventSenderProxy.get(); }

    bool shouldUseRemoteLayerTree() const { return m_shouldUseRemoteLayerTree; }
    
    // Runs the run loop until `done` is true or the timeout elapses.
    bool useWaitToDumpWatchdogTimer() { return m_useWaitToDumpWatchdogTimer; }
    void runUntil(bool& done, WTF::Seconds timeout);
    void notifyDone();

    bool shouldShowWebView() const { return m_shouldShowWebView; }
    bool usingServerMode() const { return m_usingServerMode; }
    void configureViewForTest(const TestInvocation&);
    
    bool shouldShowTouches() const { return m_shouldShowTouches; }
    
    bool beforeUnloadReturnValue() const { return m_beforeUnloadReturnValue; }
    void setBeforeUnloadReturnValue(bool value) { m_beforeUnloadReturnValue = value; }

    void simulateWebNotificationClick(uint64_t notificationID);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool accessibilityIsolatedTreeMode() const { return m_accessibilityIsolatedTreeMode; }
#endif
    
    // Geolocation.
    void setGeolocationPermission(bool);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, bool providesFloorLevel, double floorLevel);
    void setMockGeolocationPositionUnavailableError(WKStringRef errorMessage);
    void handleGeolocationPermissionRequest(WKGeolocationPermissionRequestRef);
    bool isGeolocationProviderActive() const;

    // MediaStream.
    String saltForOrigin(WKFrameRef, String);
    void getUserMediaInfoForOrigin(WKFrameRef, WKStringRef originKey, bool&, WKRetainPtr<WKStringRef>&);
    WKStringRef getUserMediaSaltForOrigin(WKFrameRef, WKStringRef originKey);
    void setUserMediaPermission(bool);
    void resetUserMediaPermission();
    void setUserMediaPersistentPermissionForOrigin(bool, WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString);
    void handleUserMediaPermissionRequest(WKFrameRef, WKSecurityOriginRef, WKSecurityOriginRef, WKUserMediaPermissionRequestRef);
    void handleCheckOfUserMediaPermissionForOrigin(WKFrameRef, WKSecurityOriginRef, WKSecurityOriginRef, const WKUserMediaPermissionCheckRef&);
    OriginSettings& settingsForOrigin(const String&);
    unsigned userMediaPermissionRequestCountForOrigin(WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString);
    void resetUserMediaPermissionRequestCountForOrigin(WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString);

    // Device Orientation / Motion.
    bool handleDeviceOrientationAndMotionAccessRequest(WKSecurityOriginRef);

    // Content Extensions.
    void configureContentExtensionForTest(const TestInvocation&);
    void resetContentExtensions();

    // Policy delegate.
    void setCustomPolicyDelegate(bool enabled, bool permissive);

    // Page Visibility.
    void setHidden(bool);

    unsigned imageCountInGeneralPasteboard() const;

    enum class ResetStage { BeforeTest, AfterTest };
    bool resetStateToConsistentValues(const TestOptions&, ResetStage);
    void resetPreferencesToConsistentValues(const TestOptions&);

    void willDestroyWebView();

    void terminateWebContentProcess();
    void reattachPageToWebProcess();

    static const char* webProcessName();
    static const char* networkProcessName();
    static const char* databaseProcessName();

    WorkQueueManager& workQueueManager() { return m_workQueueManager; }

    void setRejectsProtectionSpaceAndContinueForAuthenticationChallenges(bool value) { m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges = value; }
    void setHandlesAuthenticationChallenges(bool value) { m_handlesAuthenticationChallenges = value; }
    void setAuthenticationUsername(String username) { m_authenticationUsername = username; }
    void setAuthenticationPassword(String password) { m_authenticationPassword = password; }
    void setAllowsAnySSLCertificate(bool);
    void setShouldSwapToEphemeralSessionOnNextNavigation(bool value) { m_shouldSwapToEphemeralSessionOnNextNavigation = value; }
    void setShouldSwapToDefaultSessionOnNextNavigation(bool value) { m_shouldSwapToDefaultSessionOnNextNavigation = value; }

    void setBlockAllPlugins(bool shouldBlock);
    void setPluginSupportedMode(const String&);

    void setShouldLogHistoryClientCallbacks(bool shouldLog) { m_shouldLogHistoryClientCallbacks = shouldLog; }
    void setShouldLogCanAuthenticateAgainstProtectionSpace(bool shouldLog) { m_shouldLogCanAuthenticateAgainstProtectionSpace = shouldLog; }
    void setShouldLogDownloadCallbacks(bool shouldLog) { m_shouldLogDownloadCallbacks = shouldLog; }

    bool isCurrentInvocation(TestInvocation* invocation) const { return invocation == m_currentInvocation.get(); }

    void setShouldDecideNavigationPolicyAfterDelay(bool value) { m_shouldDecideNavigationPolicyAfterDelay = value; }
    void setShouldDecideResponsePolicyAfterDelay(bool value) { m_shouldDecideResponsePolicyAfterDelay = value; }

    void setNavigationGesturesEnabled(bool value);
    void setIgnoresViewportScaleLimits(bool);

    void setShouldDownloadUndisplayableMIMETypes(bool value) { m_shouldDownloadUndisplayableMIMETypes = value; }
    void setShouldAllowDeviceOrientationAndMotionAccess(bool value) { m_shouldAllowDeviceOrientationAndMotionAccess = value; }

    void clearStatisticsDataForDomain(WKStringRef domain);
    bool doesStatisticsDomainIDExistInDatabase(unsigned domainID);
    void setStatisticsEnabled(bool value);
    bool isStatisticsEphemeral();
    void setStatisticsDebugMode(bool value);
    void setStatisticsPrevalentResourceForDebugMode(WKStringRef hostName);
    void setStatisticsLastSeen(WKStringRef hostName, double seconds);
    void setStatisticsMergeStatistic(WKStringRef host, WKStringRef topFrameDomain1, WKStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, int dataRecordsRemoved);
    void setStatisticsExpiredStatistic(WKStringRef host, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent);
    void setStatisticsPrevalentResource(WKStringRef hostName, bool value);
    void setStatisticsVeryPrevalentResource(WKStringRef hostName, bool value);
    String dumpResourceLoadStatistics();
    bool isStatisticsPrevalentResource(WKStringRef hostName);
    bool isStatisticsVeryPrevalentResource(WKStringRef hostName);
    bool isStatisticsRegisteredAsSubresourceUnder(WKStringRef subresourceHost, WKStringRef topFrameHost);
    bool isStatisticsRegisteredAsSubFrameUnder(WKStringRef subFrameHost, WKStringRef topFrameHost);
    bool isStatisticsRegisteredAsRedirectingTo(WKStringRef hostRedirectedFrom, WKStringRef hostRedirectedTo);
    void setStatisticsHasHadUserInteraction(WKStringRef hostName, bool value);
    bool isStatisticsHasHadUserInteraction(WKStringRef hostName);
    bool isStatisticsOnlyInDatabaseOnce(WKStringRef subHost, WKStringRef topHost);
    void setStatisticsGrandfathered(WKStringRef hostName, bool value);
    bool isStatisticsGrandfathered(WKStringRef hostName);
    void setStatisticsSubframeUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName);
    void setStatisticsSubresourceUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName);
    void setStatisticsSubresourceUniqueRedirectTo(WKStringRef hostName, WKStringRef hostNameRedirectedTo);
    void setStatisticsSubresourceUniqueRedirectFrom(WKStringRef host, WKStringRef hostRedirectedFrom);
    void setStatisticsTopFrameUniqueRedirectTo(WKStringRef host, WKStringRef hostRedirectedTo);
    void setStatisticsTopFrameUniqueRedirectFrom(WKStringRef host, WKStringRef hostRedirectedFrom);
    void setStatisticsCrossSiteLoadWithLinkDecoration(WKStringRef fromHost, WKStringRef toHost);
    void setStatisticsTimeToLiveUserInteraction(double seconds);
    void statisticsProcessStatisticsAndDataRecords();
    void statisticsUpdateCookieBlocking();
    void statisticsSubmitTelemetry();
    void setStatisticsNotifyPagesWhenDataRecordsWereScanned(bool);
    void setStatisticsIsRunningTest(bool);
    void setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setStatisticsMinimumTimeBetweenDataRecordsRemoval(double);
    void setStatisticsGrandfatheringTime(double seconds);
    void setStatisticsMaxStatisticsEntries(unsigned);
    void setStatisticsPruneEntriesDownTo(unsigned);
    void statisticsClearInMemoryAndPersistentStore();
    void statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned);
    void statisticsClearThroughWebsiteDataRemoval();
    void statisticsDeleteCookiesForHost(WKStringRef host, bool includeHttpOnlyCookies);
    bool isStatisticsHasLocalStorage(WKStringRef hostName);
    void setStatisticsCacheMaxAgeCap(double seconds);
    bool hasStatisticsIsolatedSession(WKStringRef hostName);
    void setStatisticsShouldDowngradeReferrer(bool value);
    void setStatisticsShouldBlockThirdPartyCookies(bool value, bool onlyOnSitesWithoutUserInteraction);
    void setStatisticsFirstPartyWebsiteDataRemovalMode(bool value);
    void setStatisticsToSameSiteStrictCookies(WKStringRef hostName);
    void setStatisticsFirstPartyHostCNAMEDomain(WKStringRef firstPartyURLString, WKStringRef cnameURLString);
    void setStatisticsThirdPartyCNAMEDomain(WKStringRef cnameURLString);
    void setAppBoundDomains(WKArrayRef originURLs);
    void statisticsResetToConsistentState();

    void getAllStorageAccessEntries();
    void loadedSubresourceDomains();
    void clearLoadedSubresourceDomains();
    void clearAppBoundSession();
    void reinitializeAppBoundDomains();
    void updateBundleIdentifierInNetworkProcess(const String& bundleIdentifier);
    void clearBundleIdentifierInNetworkProcess();

    WKArrayRef openPanelFileURLs() const { return m_openPanelFileURLs.get(); }
    void setOpenPanelFileURLs(WKArrayRef fileURLs) { m_openPanelFileURLs = fileURLs; }

#if PLATFORM(IOS_FAMILY)
    WKDataRef openPanelFileURLsMediaIcon() const { return m_openPanelFileURLsMediaIcon.get(); }
    void setOpenPanelFileURLsMediaIcon(WKDataRef mediaIcon) { m_openPanelFileURLsMediaIcon = mediaIcon; }
#endif

    void terminateNetworkProcess();
    void terminateServiceWorkers();

    void resetQuota();

    void removeAllSessionCredentials();

    void clearIndexedDatabases();
    void clearLocalStorage();
    void syncLocalStorage();

    void clearServiceWorkerRegistrations();

    void clearDOMCache(WKStringRef origin);
    void clearDOMCaches();
    bool hasDOMCache(WKStringRef origin);
    uint64_t domCacheSize(WKStringRef origin);

    void setAllowStorageQuotaIncrease(bool);

    bool didReceiveServerRedirectForProvisionalNavigation() const { return m_didReceiveServerRedirectForProvisionalNavigation; }
    void clearDidReceiveServerRedirectForProvisionalNavigation() { m_didReceiveServerRedirectForProvisionalNavigation = false; }

    void addMockMediaDevice(WKStringRef persistentID, WKStringRef label, WKStringRef type);
    void clearMockMediaDevices();
    void removeMockMediaDevice(WKStringRef persistentID);
    void resetMockMediaDevices();
    void setMockCameraOrientation(uint64_t);
    bool isMockRealtimeMediaSourceCenterEnabled() const;
    bool hasAppBoundSession();

    void injectUserScript(WKStringRef);
    
    void sendDisplayConfigurationChangedMessageForTesting();

    void setServiceWorkerFetchTimeoutForTesting(double seconds);

    void addTestKeyToKeychain(const String& privateKeyBase64, const String& attrLabel, const String& applicationTagBase64);
    void cleanUpKeychain(const String& attrLabel, const String& applicationLabelBase64);
    bool keyExistsInKeychain(const String& attrLabel, const String& applicationLabelBase64);

#if PLATFORM(COCOA)
    NSString *overriddenCalendarIdentifier() const;
    NSString *overriddenCalendarLocaleIdentifier() const;
    void setDefaultCalendarType(NSString *identifier, NSString *localeIdentifier);
#endif // PLATFORM(COCOA)

#if PLATFORM(IOS_FAMILY)
    void setKeyboardInputModeIdentifier(const String&);
    UIKeyboardInputMode *overriddenKeyboardInputMode() const { return m_overriddenKeyboardInputMode.get(); }
#endif

    void setAllowedMenuActions(const Vector<String>&);
    void installCustomMenuAction(const String& name, bool dismissesAutomatically);

    uint64_t serverTrustEvaluationCallbackCallsCount() const { return m_serverTrustEvaluationCallbackCallsCount; }

    void setShouldDismissJavaScriptAlertsAsynchronously(bool);
    void handleJavaScriptAlert(WKPageRunJavaScriptAlertResultListenerRef);
    void abortModal();

    bool isDoingMediaCapture() const;

    String dumpAdClickAttribution();
    void clearAdClickAttribution();
    void clearAdClickAttributionsThroughWebsiteDataRemoval();
    void setAdClickAttributionOverrideTimerForTesting(bool value);
    void setAdClickAttributionConversionURLForTesting(WKURLRef);
    void markAdClickAttributionsAsExpiredForTesting();

    void didSetAppBoundDomains() const;

    WKURLRef currentTestURL() const;

private:
    WKRetainPtr<WKPageConfigurationRef> generatePageConfiguration(const TestOptions&);
    WKRetainPtr<WKContextConfigurationRef> generateContextConfiguration(const TestOptions::ContextOptions&) const;
    void initialize(int argc, const char* argv[]);
    void createWebViewWithOptions(const TestOptions&);
    void run();

    void runTestingServerLoop();
    bool runTest(const char* pathOrURL);
    
    // Returns false if timed out.
    bool waitForCompletion(const WTF::Function<void ()>&, WTF::Seconds timeout);

    bool handleControlCommand(const char* command);

    void platformInitialize();
    void platformInitializeDataStore(WKPageConfigurationRef, const TestOptions&);
    void platformDestroy();
    WKContextRef platformAdjustContext(WKContextRef, WKContextConfigurationRef);
    void platformInitializeContext();
    void platformAddTestOptions(TestOptions&) const;
    void platformCreateWebView(WKPageConfigurationRef, const TestOptions&);
    static PlatformWebView* platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef, const TestOptions&);
    void platformResetPreferencesToConsistentValues();
    // Returns false if the reset timed out.
    bool platformResetStateToConsistentValues(const TestOptions&);
#if PLATFORM(COCOA)
    void cocoaPlatformInitialize();
    void cocoaResetStateToConsistentValues(const TestOptions&);
    void setApplicationBundleIdentifier(const String&);
    void clearApplicationBundleIdentifierTestingOverride();
#endif
    void platformConfigureViewForTest(const TestInvocation&);
    void platformWillRunTest(const TestInvocation&);
    void platformRunUntil(bool& done, WTF::Seconds timeout);
    void platformDidCommitLoadForFrame(WKPageRef, WKFrameRef);
    WKContextRef platformContext();
    WKPreferencesRef platformPreferences();
    void initializeInjectedBundlePath();
    void initializeTestPluginDirectory();

    void ensureViewSupportsOptionsForTest(const TestInvocation&);
    TestOptions testOptionsForTest(const TestCommand&) const;
    void updatePlatformSpecificTestOptionsForTest(TestOptions&, const std::string& pathOrURL) const;

    void updateWebViewSizeForTest(const TestInvocation&);
    void updateWindowScaleForTest(PlatformWebView*, const TestInvocation&);

    void updateLiveDocumentsAfterTest();
    void checkForWorldLeaks();

    void didReceiveLiveDocumentsList(WKArrayRef);
    void dumpResponse(const String&);
    void findAndDumpWebKitProcessIdentifiers();
    void findAndDumpWorldLeaks();

    void decidePolicyForGeolocationPermissionRequestIfPossible();
    void decidePolicyForUserMediaPermissionRequestIfPossible();

    // WKContextInjectedBundleClient
    static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void*);
    static void didReceiveSynchronousMessageFromInjectedBundleWithListener(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef, const void*);
    static WKTypeRef getInjectedBundleInitializationUserData(WKContextRef, const void *clientInfo);

    // WKPageInjectedBundleClient
    static void didReceivePageMessageFromInjectedBundle(WKPageRef, WKStringRef messageName, WKTypeRef messageBody, const void*);
    static void didReceiveSynchronousPageMessageFromInjectedBundleWithListener(WKPageRef, WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef, const void*);
    void didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);
    void didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody, WKMessageListenerRef);
    WKRetainPtr<WKTypeRef> getInjectedBundleInitializationUserData();

    void didReceiveKeyDownMessageFromInjectedBundle(WKDictionaryRef messageBodyDictionary, bool synchronous);

    // WKContextClient
    static void networkProcessDidCrash(WKContextRef, const void*);
    void networkProcessDidCrash();
    static void serviceWorkerProcessDidCrash(WKContextRef, WKProcessID, const void*);
    void serviceWorkerProcessDidCrash(WKProcessID);
    static void gpuProcessDidCrash(WKContextRef, WKProcessID, const void*);
    void gpuProcessDidCrash(WKProcessID);

    // WKPageNavigationClient
    static void didCommitNavigation(WKPageRef, WKNavigationRef, WKTypeRef userData, const void*);
    void didCommitNavigation(WKPageRef, WKNavigationRef);

    static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef userData, const void*);
    void didFinishNavigation(WKPageRef, WKNavigationRef);

    // WKContextDownloadClient
    static void downloadDidStart(WKContextRef, WKDownloadRef, const void*);
    void downloadDidStart(WKContextRef, WKDownloadRef);
    static WKStringRef decideDestinationWithSuggestedFilename(WKContextRef, WKDownloadRef, WKStringRef filename, bool* allowOverwrite, const void *clientInfo);
    WKStringRef decideDestinationWithSuggestedFilename(WKContextRef, WKDownloadRef, WKStringRef filename, bool*& allowOverwrite);
    static void downloadDidFinish(WKContextRef, WKDownloadRef, const void*);
    void downloadDidFinish(WKContextRef, WKDownloadRef);
    static void downloadDidFail(WKContextRef, WKDownloadRef, WKErrorRef, const void*);
    void downloadDidFail(WKContextRef, WKDownloadRef, WKErrorRef);
    static void downloadDidCancel(WKContextRef, WKDownloadRef, const void*);
    void downloadDidCancel(WKContextRef, WKDownloadRef);
    static void downloadDidReceiveServerRedirectToURL(WKContextRef, WKDownloadRef, WKURLRef, const void*);
    void downloadDidReceiveServerRedirectToURL(WKContextRef, WKDownloadRef, WKURLRef);
    
    static void processDidCrash(WKPageRef, const void* clientInfo);
    void processDidCrash();

    static void didBeginNavigationGesture(WKPageRef, const void*);
    static void willEndNavigationGesture(WKPageRef, WKBackForwardListItemRef, const void*);
    static void didEndNavigationGesture(WKPageRef, WKBackForwardListItemRef, const void*);
    static void didRemoveNavigationGestureSnapshot(WKPageRef, const void*);
    void didBeginNavigationGesture(WKPageRef);
    void willEndNavigationGesture(WKPageRef, WKBackForwardListItemRef);
    void didEndNavigationGesture(WKPageRef, WKBackForwardListItemRef);
    void didRemoveNavigationGestureSnapshot(WKPageRef);

    static WKPluginLoadPolicy decidePolicyForPluginLoad(WKPageRef, WKPluginLoadPolicy currentPluginLoadPolicy, WKDictionaryRef pluginInformation, WKStringRef* unavailabilityDescription, const void* clientInfo);
    WKPluginLoadPolicy decidePolicyForPluginLoad(WKPageRef, WKPluginLoadPolicy currentPluginLoadPolicy, WKDictionaryRef pluginInformation, WKStringRef* unavailabilityDescription);
    

    static void decidePolicyForNotificationPermissionRequest(WKPageRef, WKSecurityOriginRef, WKNotificationPermissionRequestRef, const void*);
    void decidePolicyForNotificationPermissionRequest(WKPageRef, WKSecurityOriginRef, WKNotificationPermissionRequestRef);

    static void unavailablePluginButtonClicked(WKPageRef, WKPluginUnavailabilityReason, WKDictionaryRef, const void*);

    static void didReceiveServerRedirectForProvisionalNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*);
    void didReceiveServerRedirectForProvisionalNavigation(WKPageRef, WKNavigationRef, WKTypeRef);

    static bool canAuthenticateAgainstProtectionSpace(WKPageRef, WKProtectionSpaceRef, const void*);
    bool canAuthenticateAgainstProtectionSpace(WKPageRef, WKProtectionSpaceRef);

    static void didReceiveAuthenticationChallenge(WKPageRef, WKAuthenticationChallengeRef, const void*);
    void didReceiveAuthenticationChallenge(WKPageRef, WKAuthenticationChallengeRef);

    static void decidePolicyForNavigationAction(WKPageRef, WKNavigationActionRef, WKFramePolicyListenerRef, WKTypeRef, const void*);
    void decidePolicyForNavigationAction(WKNavigationActionRef, WKFramePolicyListenerRef);

    static void decidePolicyForNavigationResponse(WKPageRef, WKNavigationResponseRef, WKFramePolicyListenerRef, WKTypeRef, const void*);
    void decidePolicyForNavigationResponse(WKNavigationResponseRef, WKFramePolicyListenerRef);

    // WKContextHistoryClient
    static void didNavigateWithNavigationData(WKContextRef, WKPageRef, WKNavigationDataRef, WKFrameRef, const void*);
    void didNavigateWithNavigationData(WKNavigationDataRef, WKFrameRef);

    static void didPerformClientRedirect(WKContextRef, WKPageRef, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef, const void*);
    void didPerformClientRedirect(WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef);

    static void didPerformServerRedirect(WKContextRef, WKPageRef, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef, const void*);
    void didPerformServerRedirect(WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef);

    static void didUpdateHistoryTitle(WKContextRef, WKPageRef, WKStringRef title, WKURLRef, WKFrameRef, const void*);
    void didUpdateHistoryTitle(WKStringRef title, WKURLRef, WKFrameRef);

    static WKPageRef createOtherPage(WKPageRef, WKPageConfigurationRef, WKNavigationActionRef, WKWindowFeaturesRef, const void*);
    WKPageRef createOtherPage(PlatformWebView* parentView, WKPageConfigurationRef, WKNavigationActionRef, WKWindowFeaturesRef);

    static void runModal(WKPageRef, const void* clientInfo);
    static void runModal(PlatformWebView*);

#if PLATFORM(COCOA)
    static void finishCreatingPlatformWebView(PlatformWebView*, const TestOptions&);
    void configureContentMode(WKWebViewConfiguration *, const TestOptions&);
#endif

    static const char* libraryPathForTesting();
    static const char* platformLibraryPathForTesting();

    std::unique_ptr<TestInvocation> m_currentInvocation;
#if PLATFORM(COCOA)
    std::unique_ptr<ClassMethodSwizzler> m_calendarSwizzler;
    std::pair<RetainPtr<NSString>, RetainPtr<NSString>> m_overriddenCalendarAndLocaleIdentifiers;
#endif // PLATFORM(COCOA)
    bool m_verbose { false };
    bool m_printSeparators { false };
    bool m_usingServerMode { false };
    bool m_gcBetweenTests { false };
    bool m_shouldDumpPixelsForAllTests { false };
    bool m_createdOtherPage { false };
    std::vector<std::string> m_paths;
    std::set<std::string> m_allowedHosts;
    HashMap<String, bool> m_internalFeatures;
    HashMap<String, bool> m_experimentalFeatures;

    WKRetainPtr<WKStringRef> m_injectedBundlePath;
    WKRetainPtr<WKStringRef> m_testPluginDirectory;

    WebNotificationProvider m_webNotificationProvider;

    std::unique_ptr<PlatformWebView> m_mainWebView;
    WKRetainPtr<WKContextRef> m_context;
    Optional<TestOptions::ContextOptions> m_contextOptions;
    WKRetainPtr<WKPageGroupRef> m_pageGroup;
    WKRetainPtr<WKUserContentControllerRef> m_userContentController;

#if PLATFORM(IOS_FAMILY)
    Vector<std::unique_ptr<InstanceMethodSwizzler>> m_inputModeSwizzlers;
    RetainPtr<UIKeyboardInputMode> m_overriddenKeyboardInputMode;
    Vector<std::unique_ptr<InstanceMethodSwizzler>> m_presentPopoverSwizzlers;
#if !HAVE(NONDESTRUCTIVE_IMAGE_PASTE_SUPPORT_QUERY)
    std::unique_ptr<InstanceMethodSwizzler> m_keyboardDelegateSupportsImagePasteSwizzler;
#endif
#endif

    enum State {
        Initial,
        Resetting,
        RunningTest
    };
    State m_state { Initial };
    bool m_doneResetting { false };

    bool m_useWaitToDumpWatchdogTimer { true };
    bool m_forceNoTimeout { false };

    bool m_didPrintWebProcessCrashedMessage { false };
    bool m_shouldExitWhenWebProcessCrashes { true };
    
    bool m_beforeUnloadReturnValue { true };

    std::unique_ptr<GeolocationProviderMock> m_geolocationProvider;
    Vector<WKRetainPtr<WKGeolocationPermissionRequestRef> > m_geolocationPermissionRequests;
    bool m_isGeolocationPermissionSet { false };
    bool m_isGeolocationPermissionAllowed { false };

    HashMap<String, RefPtr<OriginSettings>> m_cachedUserMediaPermissions;

    typedef Vector<std::pair<String, WKRetainPtr<WKUserMediaPermissionRequestRef>>> PermissionRequestList;
    PermissionRequestList m_userMediaPermissionRequests;

    bool m_isUserMediaPermissionSet { false };
    bool m_isUserMediaPermissionAllowed { false };

    bool m_policyDelegateEnabled { false };
    bool m_policyDelegatePermissive { false };
    bool m_shouldDownloadUndisplayableMIMETypes { false };
    bool m_shouldAllowDeviceOrientationAndMotionAccess { false };

    bool m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges { false };
    bool m_handlesAuthenticationChallenges { false };
    String m_authenticationUsername;
    String m_authenticationPassword;

    bool m_shouldBlockAllPlugins { false };
    String m_unsupportedPluginMode;

    bool m_forceComplexText { false };
    bool m_shouldUseAcceleratedDrawing { false };
    bool m_shouldUseRemoteLayerTree { false };

    bool m_shouldLogCanAuthenticateAgainstProtectionSpace { false };
    bool m_shouldLogDownloadCallbacks { false };
    bool m_shouldLogHistoryClientCallbacks { false };
    bool m_shouldShowWebView { false };
    
    bool m_shouldShowTouches { false };
    bool m_checkForWorldLeaks { false };

    bool m_allowAnyHTTPSCertificateForAllowedHosts { false };

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool m_accessibilityIsolatedTreeMode { false };
#endif
    
    bool m_shouldDecideNavigationPolicyAfterDelay { false };
    bool m_shouldDecideResponsePolicyAfterDelay { false };

    bool m_didReceiveServerRedirectForProvisionalNavigation { false };

    WKRetainPtr<WKArrayRef> m_openPanelFileURLs;
#if PLATFORM(IOS_FAMILY)
    WKRetainPtr<WKDataRef> m_openPanelFileURLsMediaIcon;
#endif

    std::unique_ptr<EventSenderProxy> m_eventSenderProxy;
    WKRetainPtr<WKWebsiteDataStoreRef> m_websiteDataStore;

    WorkQueueManager m_workQueueManager;

    struct AbandonedDocumentInfo {
        String testURL;
        String abandonedDocumentURL;

        AbandonedDocumentInfo() = default;
        AbandonedDocumentInfo(String inTestURL, String inAbandonedDocumentURL)
            : testURL(inTestURL)
            , abandonedDocumentURL(inAbandonedDocumentURL)
        { }
    };
    HashMap<uint64_t, AbandonedDocumentInfo> m_abandonedDocumentInfo;

    uint64_t m_serverTrustEvaluationCallbackCallsCount { 0 };
    bool m_shouldDismissJavaScriptAlertsAsynchronously { false };
    bool m_allowsAnySSLCertificate { true };
    bool m_shouldSwapToEphemeralSessionOnNextNavigation { false };
    bool m_shouldSwapToDefaultSessionOnNextNavigation { false };
    
#if PLATFORM(COCOA)
    bool m_hasSetApplicationBundleIdentifier { false };
#endif
};

struct TestCommand {
    std::string pathOrURL;
    std::string absolutePath;
    std::string expectedPixelHash;
    WTF::Seconds timeout;
    bool shouldDumpPixels { false };
    bool dumpJSConsoleLogInStdErr { false };
};

} // namespace WTR
