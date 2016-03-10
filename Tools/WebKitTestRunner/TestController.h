/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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

#ifndef TestController_h
#define TestController_h

#include "GeolocationProviderMock.h"
#include "WebNotificationProvider.h"
#include "WorkQueueManager.h"
#include <WebKit/WKRetainPtr.h>
#include <string>
#include <vector>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

OBJC_CLASS WKWebViewConfiguration;

namespace WTR {

class TestInvocation;
class OriginSettings;
class PlatformWebView;
class EventSenderProxy;
struct TestOptions;

// FIXME: Rename this TestRunner?
class TestController {
public:
    static TestController& singleton();

    static const unsigned viewWidth;
    static const unsigned viewHeight;

    static const unsigned w3cSVGViewWidth;
    static const unsigned w3cSVGViewHeight;

    static const double shortTimeout;
    static const double noTimeout;

    TestController(int argc, const char* argv[]);
    ~TestController();

    bool verbose() const { return m_verbose; }

    WKStringRef injectedBundlePath() const { return m_injectedBundlePath.get(); }
    WKStringRef testPluginDirectory() const { return m_testPluginDirectory.get(); }

    PlatformWebView* mainWebView() { return m_mainWebView.get(); }
    WKContextRef context() { return m_context.get(); }

    EventSenderProxy* eventSenderProxy() { return m_eventSenderProxy.get(); }

    bool shouldUseRemoteLayerTree() const { return m_shouldUseRemoteLayerTree; }
    
    // Runs the run loop until `done` is true or the timeout elapses.
    bool useWaitToDumpWatchdogTimer() { return m_useWaitToDumpWatchdogTimer; }
    void runUntil(bool& done, double timeoutSeconds);
    void notifyDone();

    bool shouldShowWebView() const { return m_shouldShowWebView; }

    void configureViewForTest(const TestInvocation&);
    
    bool beforeUnloadReturnValue() const { return m_beforeUnloadReturnValue; }
    void setBeforeUnloadReturnValue(bool value) { m_beforeUnloadReturnValue = value; }

    void simulateWebNotificationClick(uint64_t notificationID);

    // Geolocation.
    void setGeolocationPermission(bool);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed);
    void setMockGeolocationPositionUnavailableError(WKStringRef errorMessage);
    void handleGeolocationPermissionRequest(WKGeolocationPermissionRequestRef);
    bool isGeolocationProviderActive() const;

    // MediaStream.
    String saltForOrigin(WKFrameRef, String);
    void getUserMediaInfoForOrigin(WKFrameRef, WKStringRef originKey, bool&, WKRetainPtr<WKStringRef>&);
    WKStringRef getUserMediaSaltForOrigin(WKFrameRef, WKStringRef originKey);
    void setUserMediaPermission(bool);
    void setUserMediaPermissionForOrigin(bool, WKStringRef userMediaDocumentOriginString, WKStringRef topLevelDocumentOriginString);
    void handleUserMediaPermissionRequest(WKFrameRef, WKSecurityOriginRef, WKSecurityOriginRef, WKUserMediaPermissionRequestRef);
    void handleCheckOfUserMediaPermissionForOrigin(WKFrameRef, WKSecurityOriginRef, WKSecurityOriginRef, const WKUserMediaPermissionCheckRef&);

    // Policy delegate.
    void setCustomPolicyDelegate(bool enabled, bool permissive);

    // Page Visibility.
    void setHidden(bool);

    bool resetStateToConsistentValues();
    void resetPreferencesToConsistentValues();

    void terminateWebContentProcess();
    void reattachPageToWebProcess();

    static const char* webProcessName();
    static const char* networkProcessName();
    static const char* databaseProcessName();

    WorkQueueManager& workQueueManager() { return m_workQueueManager; }

    void setHandlesAuthenticationChallenges(bool value) { m_handlesAuthenticationChallenges = value; }
    void setAuthenticationUsername(String username) { m_authenticationUsername = username; }
    void setAuthenticationPassword(String password) { m_authenticationPassword = password; }

    void setBlockAllPlugins(bool shouldBlock) { m_shouldBlockAllPlugins = shouldBlock; }

    void setShouldLogHistoryClientCallbacks(bool shouldLog) { m_shouldLogHistoryClientCallbacks = shouldLog; }

    bool isCurrentInvocation(TestInvocation* invocation) const { return invocation == m_currentInvocation.get(); }

    void setShouldDecideNavigationPolicyAfterDelay(bool value) { m_shouldDecideNavigationPolicyAfterDelay = value; }

    void setNavigationGesturesEnabled(bool value);

private:
    WKRetainPtr<WKPageConfigurationRef> generatePageConfiguration(WKContextConfigurationRef);
    WKRetainPtr<WKContextConfigurationRef> generateContextConfiguration() const;
    void initialize(int argc, const char* argv[]);
    void createWebViewWithOptions(const TestOptions&);
    void run();

    void runTestingServerLoop();
    bool runTest(const char* pathOrURL);

    void platformInitialize();
    void platformDestroy();
    WKContextRef platformAdjustContext(WKContextRef, WKContextConfigurationRef);
    void platformInitializeContext();
    void platformCreateWebView(WKPageConfigurationRef, const TestOptions&);
    static PlatformWebView* platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef, const TestOptions&);
    void platformResetPreferencesToConsistentValues();
    void platformResetStateToConsistentValues();
#if PLATFORM(COCOA)
    void cocoaResetStateToConsistentValues();
#endif
    void platformConfigureViewForTest(const TestInvocation&);
    void platformWillRunTest(const TestInvocation&);
    void platformRunUntil(bool& done, double timeout);
    void platformDidCommitLoadForFrame(WKPageRef, WKFrameRef);
    WKPreferencesRef platformPreferences();
    void initializeInjectedBundlePath();
    void initializeTestPluginDirectory();

    void ensureViewSupportsOptionsForTest(const TestInvocation&);
    TestOptions testOptionsForTest(const std::string& pathOrURL) const;
    void updatePlatformSpecificTestOptionsForTest(TestOptions&, const std::string& pathOrURL) const;

    void updateWebViewSizeForTest(const TestInvocation&);
    void updateWindowScaleForTest(PlatformWebView*, const TestInvocation&);

    void decidePolicyForGeolocationPermissionRequestIfPossible();
    void decidePolicyForUserMediaPermissionRequestIfPossible();

    // WKContextInjectedBundleClient
    static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void*);
    static void didReceiveSynchronousMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, WKTypeRef* returnData, const void*);
    static WKTypeRef getInjectedBundleInitializationUserData(WKContextRef, const void *clientInfo);

    // WKPageInjectedBundleClient
    static void didReceivePageMessageFromInjectedBundle(WKPageRef, WKStringRef messageName, WKTypeRef messageBody, const void*);
    static void didReceiveSynchronousPageMessageFromInjectedBundle(WKPageRef, WKStringRef messageName, WKTypeRef messageBody, WKTypeRef* returnData, const void*);
    void didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);
    WKRetainPtr<WKTypeRef> didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);
    WKRetainPtr<WKTypeRef> getInjectedBundleInitializationUserData();

    void didReceiveKeyDownMessageFromInjectedBundle(WKDictionaryRef messageBodyDictionary, bool synchronous);

    // WKContextClient
    static void networkProcessDidCrash(WKContextRef, const void*);
    void networkProcessDidCrash();
    static void databaseProcessDidCrash(WKContextRef, const void*);
    void databaseProcessDidCrash();

    // WKPageNavigationClient
    static void didCommitNavigation(WKPageRef, WKNavigationRef, WKTypeRef userData, const void*);
    void didCommitNavigation(WKPageRef, WKNavigationRef);

    static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef userData, const void*);
    void didFinishNavigation(WKPageRef, WKNavigationRef);

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

    static bool canAuthenticateAgainstProtectionSpace(WKPageRef, WKProtectionSpaceRef, const void *clientInfo);

    static void didReceiveAuthenticationChallenge(WKPageRef, WKAuthenticationChallengeRef, const void *clientInfo);
    void didReceiveAuthenticationChallenge(WKPageRef, WKAuthenticationChallengeRef);

    static void decidePolicyForNavigationAction(WKPageRef, WKNavigationActionRef, WKFramePolicyListenerRef, WKTypeRef, const void*);
    void decidePolicyForNavigationAction(WKFramePolicyListenerRef);

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

    static void runModal(WKPageRef, const void* clientInfo);
    static void runModal(PlatformWebView*);

    static const char* libraryPathForTesting();
    static const char* platformLibraryPathForTesting();

    std::unique_ptr<TestInvocation> m_currentInvocation;

    bool m_verbose;
    bool m_printSeparators;
    bool m_usingServerMode;
    bool m_gcBetweenTests;
    bool m_shouldDumpPixelsForAllTests;
    std::vector<std::string> m_paths;
    std::vector<std::string> m_allowedHosts;
    WKRetainPtr<WKStringRef> m_injectedBundlePath;
    WKRetainPtr<WKStringRef> m_testPluginDirectory;

    WebNotificationProvider m_webNotificationProvider;

    std::unique_ptr<PlatformWebView> m_mainWebView;
    WKRetainPtr<WKContextRef> m_context;
    WKRetainPtr<WKPageGroupRef> m_pageGroup;

    enum State {
        Initial,
        Resetting,
        RunningTest
    };
    State m_state;
    bool m_doneResetting;

    bool m_useWaitToDumpWatchdogTimer;
    bool m_forceNoTimeout;

    bool m_didPrintWebProcessCrashedMessage;
    bool m_shouldExitWhenWebProcessCrashes;
    
    bool m_beforeUnloadReturnValue;

    std::unique_ptr<GeolocationProviderMock> m_geolocationProvider;
    Vector<WKRetainPtr<WKGeolocationPermissionRequestRef> > m_geolocationPermissionRequests;
    bool m_isGeolocationPermissionSet;
    bool m_isGeolocationPermissionAllowed;

    HashMap<String, RefPtr<OriginSettings>> m_cahcedUserMediaPermissions;

    typedef Vector<std::pair<String, WKRetainPtr<WKUserMediaPermissionRequestRef>>> PermissionRequestList;
    PermissionRequestList m_userMediaPermissionRequests;

    bool m_isUserMediaPermissionSet;
    bool m_isUserMediaPermissionAllowed;

    bool m_policyDelegateEnabled;
    bool m_policyDelegatePermissive;

    bool m_handlesAuthenticationChallenges;
    String m_authenticationUsername;
    String m_authenticationPassword;

    bool m_shouldBlockAllPlugins;

    bool m_forceComplexText;
    bool m_shouldUseAcceleratedDrawing;
    bool m_shouldUseRemoteLayerTree;

    bool m_shouldLogHistoryClientCallbacks;
    bool m_shouldShowWebView;

    bool m_shouldDecideNavigationPolicyAfterDelay { false };

    std::unique_ptr<EventSenderProxy> m_eventSenderProxy;

    WorkQueueManager m_workQueueManager;
};

} // namespace WTR

#endif // TestController_h
