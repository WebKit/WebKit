/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "JSWrappable.h"
#include "StringFunctions.h"
#include "WhatToDump.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKBundleScriptWorld.h>
#include <WebKit/WKRetainPtr.h>
#include <string>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#include <CoreFoundation/CFRunLoop.h>
typedef RetainPtr<CFRunLoopTimerRef> PlatformTimerRef;
#else
#include <wtf/RunLoop.h>
namespace WTR {
class TestRunner;
typedef RunLoop::Timer<TestRunner> PlatformTimerRef;
}
#endif

namespace WTR {

class TestRunner : public JSWrappable {
public:
    static Ref<TestRunner> create();
    virtual ~TestRunner();

    // JSWrappable
    virtual JSClassRef wrapperClass();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);

    bool isWebKit2() const { return true; }

    // The basics.
    WKURLRef testURL() const { return m_testURL.get(); }
    void setTestURL(WKURLRef url) { m_testURL = url; }
    void dumpAsText(bool dumpPixels);
    void waitForPolicyDelegate();
    void dumpChildFramesAsText() { setWhatToDump(WhatToDump::AllFramesText); }
    void waitUntilDownloadFinished();
    void waitUntilDone();
    void notifyDone();
    double preciseTime();
    double timeout() { return m_timeout.milliseconds(); }

    // Other dumping.
    void dumpBackForwardList() { m_shouldDumpBackForwardListsForAllWindows = true; }
    void dumpChildFrameScrollPositions() { m_shouldDumpAllFrameScrollPositions = true; }
    void dumpEditingCallbacks() { m_dumpEditingCallbacks = true; }
    void dumpSelectionRect() { m_dumpSelectionRect = true; }
    void dumpStatusCallbacks() { m_dumpStatusCallbacks = true; }
    void dumpTitleChanges() { m_dumpTitleChanges = true; }
    void dumpFullScreenCallbacks() { m_dumpFullScreenCallbacks = true; }
    void dumpFrameLoadCallbacks() { setShouldDumpFrameLoadCallbacks(true); }
    void dumpProgressFinishedCallback() { setShouldDumpProgressFinishedCallback(true); }
    void dumpResourceLoadCallbacks() { m_dumpResourceLoadCallbacks = true; }
    void dumpResourceResponseMIMETypes() { m_dumpResourceResponseMIMETypes = true; }
    void dumpWillCacheResponse() { m_dumpWillCacheResponse = true; }
    void dumpApplicationCacheDelegateCallbacks() { m_dumpApplicationCacheDelegateCallbacks = true; }
    void dumpDatabaseCallbacks() { m_dumpDatabaseCallbacks = true; }
    void dumpDOMAsWebArchive() { setWhatToDump(WhatToDump::DOMAsWebArchive); }
    void dumpPolicyDelegateCallbacks() { m_dumpPolicyCallbacks = true; }
    void dumpResourceLoadStatistics();

    void setShouldDumpFrameLoadCallbacks(bool value);
    void setShouldDumpProgressFinishedCallback(bool value) { m_dumpProgressFinishedCallback = value; }

    // Special options.
    void keepWebHistory();
    void setAcceptsEditing(bool value) { m_shouldAllowEditing = value; }
    void setCanOpenWindows();
    void setCloseRemainingWindowsWhenComplete(bool value) { m_shouldCloseExtraWindows = value; }
    void setXSSAuditorEnabled(bool);
    void setModernMediaControlsEnabled(bool);
    void setWebGL2Enabled(bool);
    void setWebMetalEnabled(bool);
    void setWritableStreamAPIEnabled(bool);
    void setReadableByteStreamAPIEnabled(bool);

    void setAllowUniversalAccessFromFileURLs(bool);
    void setAllowFileAccessFromFileURLs(bool);
    void setNeedsStorageAccessFromFileURLsQuirk(bool);
    void setPluginsEnabled(bool);
    void setJavaScriptCanAccessClipboard(bool);
    void setPrivateBrowsingEnabled(bool);
    void setUseDashboardCompatibilityMode(bool);
    void setPopupBlockingEnabled(bool);
    void setAuthorAndUserStylesEnabled(bool);
    void setCustomPolicyDelegate(bool enabled, bool permissive = false);
    void addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);
    void setUserStyleSheetEnabled(bool);
    void setUserStyleSheetLocation(JSStringRef);
    void setSpatialNavigationEnabled(bool);
    void setTabKeyCyclesThroughElements(bool);
    void setSerializeHTTPLoads();
    void dispatchPendingLoadRequests();
    void setCacheModel(int);
    void setAsynchronousSpellCheckingEnabled(bool);
    void setAllowsAnySSLCertificate(bool);
    void setEncryptedMediaAPIEnabled(bool);
    void setMediaDevicesEnabled(bool);
    void setWebRTCMDNSICECandidatesEnabled(bool);
    void setWebRTCUnifiedPlanEnabled(bool);
    void setCustomUserAgent(JSStringRef);
    void setWebAPIStatisticsEnabled(bool);

    // Special DOM functions.
    void clearBackForwardList();
    void execCommand(JSStringRef name, JSStringRef showUI, JSStringRef value);
    bool isCommandEnabled(JSStringRef name);
    unsigned windowCount();

    // Repaint testing.
    void testRepaint() { m_testRepaint = true; }
    void repaintSweepHorizontally() { m_testRepaintSweepHorizontally = true; }
    void display();
    void displayAndTrackRepaints();

    // UserContent testing.
    void addUserScript(JSStringRef source, bool runAtStart, bool allFrames);
    void addUserStyleSheet(JSStringRef source, bool allFrames);

    // Text search testing.
    bool findString(JSStringRef, JSValueRef optionsArray);
    void findStringMatchesInPage(JSStringRef, JSValueRef optionsArray);
    void replaceFindMatchesAtIndices(JSValueRef matchIndices, JSStringRef replacementText, bool selectionOnly);

    // Local storage
    void clearAllDatabases();
    void setDatabaseQuota(uint64_t);
    JSRetainPtr<JSStringRef> pathToLocalResource(JSStringRef);

    // Application Cache
    void clearAllApplicationCaches();
    void clearApplicationCacheForOrigin(JSStringRef origin);
    void setAppCacheMaximumSize(uint64_t);
    long long applicationCacheDiskUsageForOrigin(JSStringRef origin);
    void disallowIncreaseForApplicationCacheQuota();
    bool shouldDisallowIncreaseForApplicationCacheQuota() { return m_disallowIncreaseForApplicationCacheQuota; }
    JSValueRef originsWithApplicationCache();

    void clearDOMCache(JSStringRef origin);
    void clearDOMCaches();
    bool hasDOMCache(JSStringRef origin);
    uint64_t domCacheSize(JSStringRef origin);
    void allowCacheStorageQuotaIncrease();

    // IndexedDB
    void setIDBPerOriginQuota(uint64_t);

    // Failed load condition testing
    void forceImmediateCompletion();

    // Printing
    bool isPageBoxVisible(int pageIndex);
    bool isPrinting() { return m_isPrinting; }
    void setPrinting() { m_isPrinting = true; }

    // Authentication
    void setRejectsProtectionSpaceAndContinueForAuthenticationChallenges(bool);
    void setHandlesAuthenticationChallenges(bool);
    void setShouldLogCanAuthenticateAgainstProtectionSpace(bool);
    void setAuthenticationUsername(JSStringRef);
    void setAuthenticationPassword(JSStringRef);

    void setValueForUser(JSContextRef, JSValueRef element, JSStringRef value);

    // Audio testing.
    void setAudioResult(JSContextRef, JSValueRef data);

    void setBlockAllPlugins(bool);
    void setPluginSupportedMode(JSStringRef);

    WhatToDump whatToDump() const;
    void setWhatToDump(WhatToDump);

    bool shouldDumpAllFrameScrollPositions() const { return m_shouldDumpAllFrameScrollPositions; }
    bool shouldDumpBackForwardListsForAllWindows() const { return m_shouldDumpBackForwardListsForAllWindows; }
    bool shouldDumpEditingCallbacks() const { return m_dumpEditingCallbacks; }
    bool shouldDumpMainFrameScrollPosition() const { return whatToDump() == WhatToDump::RenderTree; }
    bool shouldDumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    bool shouldDumpTitleChanges() const { return m_dumpTitleChanges; }
    bool shouldDumpPixels() const;
    bool shouldDumpFullScreenCallbacks() const { return m_dumpFullScreenCallbacks; }
    bool shouldDumpFrameLoadCallbacks();
    bool shouldDumpProgressFinishedCallback() const { return m_dumpProgressFinishedCallback; }
    bool shouldDumpResourceLoadCallbacks() const { return m_dumpResourceLoadCallbacks; }
    bool shouldDumpResourceResponseMIMETypes() const { return m_dumpResourceResponseMIMETypes; }
    bool shouldDumpWillCacheResponse() const { return m_dumpWillCacheResponse; }
    bool shouldDumpApplicationCacheDelegateCallbacks() const { return m_dumpApplicationCacheDelegateCallbacks; }
    bool shouldDumpDatabaseCallbacks() const { return m_dumpDatabaseCallbacks; }
    bool shouldDumpSelectionRect() const { return m_dumpSelectionRect; }
    bool shouldDumpPolicyCallbacks() const { return m_dumpPolicyCallbacks; }

    bool isPolicyDelegateEnabled() const { return m_policyDelegateEnabled; }
    bool isPolicyDelegatePermissive() const { return m_policyDelegatePermissive; }

    bool didReceiveServerRedirectForProvisionalNavigation() const;
    void clearDidReceiveServerRedirectForProvisionalNavigation();

    bool shouldWaitUntilDone() const;
    void waitToDumpWatchdogTimerFired();
    void invalidateWaitToDumpWatchdogTimer();

    // Downloads
    bool shouldFinishAfterDownload() const { return m_shouldFinishAfterDownload; }
    void setShouldLogDownloadCallbacks(bool);

    bool shouldAllowEditing() const { return m_shouldAllowEditing; }

    bool shouldCloseExtraWindowsAfterRunningTest() const { return m_shouldCloseExtraWindows; }

    void evaluateScriptInIsolatedWorld(JSContextRef, unsigned worldID, JSStringRef script);
    static unsigned worldIDForWorld(WKBundleScriptWorldRef);

    void showWebInspector();
    void closeWebInspector();
    void evaluateInWebInspector(JSStringRef script);
    JSRetainPtr<JSStringRef> inspectorTestStubURL();

    void setPOSIXLocale(JSStringRef);

    bool willSendRequestReturnsNull() const { return m_willSendRequestReturnsNull; }
    void setWillSendRequestReturnsNull(bool f) { m_willSendRequestReturnsNull = f; }
    bool willSendRequestReturnsNullOnRedirect() const { return m_willSendRequestReturnsNullOnRedirect; }
    void setWillSendRequestReturnsNullOnRedirect(bool f) { m_willSendRequestReturnsNullOnRedirect = f; }
    void setWillSendRequestAddsHTTPBody(JSStringRef body) { m_willSendRequestHTTPBody = toWTFString(toWK(body)); }
    String willSendRequestHTTPBody() const { return m_willSendRequestHTTPBody; }

    void setTextDirection(JSStringRef);

    void setShouldStayOnPageAfterHandlingBeforeUnload(bool);

    void setDefersLoading(bool);

    void setStopProvisionalFrameLoads() { m_shouldStopProvisionalFrameLoads = true; }
    bool shouldStopProvisionalFrameLoads() const { return m_shouldStopProvisionalFrameLoads; }
    
    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool value) { m_globalFlag = value; }

    double databaseDefaultQuota() const { return m_databaseDefaultQuota; }
    void setDatabaseDefaultQuota(double quota) { m_databaseDefaultQuota = quota; }

    double databaseMaxQuota() const { return m_databaseMaxQuota; }
    void setDatabaseMaxQuota(double quota) { m_databaseMaxQuota = quota; }

    void addChromeInputField(JSValueRef);
    void removeChromeInputField(JSValueRef);
    void focusWebView(JSValueRef);
    void setBackingScaleFactor(double, JSValueRef);

    void setWindowIsKey(bool);

    void setViewSize(double width, double height);

    void callAddChromeInputFieldCallback();
    void callRemoveChromeInputFieldCallback();
    void callFocusWebViewCallback();
    void callSetBackingScaleFactorCallback();

    void overridePreference(JSStringRef preference, JSStringRef value);

    // Cookies testing
    void setAlwaysAcceptCookies(bool);
    void setOnlyAcceptFirstPartyCookies(bool);

    // Custom full screen behavior.
    void setHasCustomFullScreenBehavior(bool value) { m_customFullScreenBehavior = value; }
    bool hasCustomFullScreenBehavior() const { return m_customFullScreenBehavior; }

    // Web notifications.
    static void grantWebNotificationPermission(JSStringRef origin);
    static void denyWebNotificationPermission(JSStringRef origin);
    static void removeAllWebNotificationPermissions();
    static void simulateWebNotificationClick(JSValueRef notification);

    // Geolocation.
    void setGeolocationPermission(bool);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy, JSValueRef altitude, JSValueRef altitudeAccuracy, JSValueRef heading, JSValueRef speed, JSValueRef floorLevel);
    void setMockGeolocationPositionUnavailableError(JSStringRef message);
    bool isGeolocationProviderActive();

    // MediaStream
    void setUserMediaPermission(bool);
    void resetUserMediaPermission();
    void setUserMediaPersistentPermissionForOrigin(bool permission, JSStringRef origin, JSStringRef parentOrigin);
    unsigned userMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin) const;
    void resetUserMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin);

    void setPageVisibility(JSStringRef state);
    void resetPageVisibility();

    bool callShouldCloseOnWebView();

    void setCustomTimeout(WTF::Seconds duration) { m_timeout = duration; }

    // Work queue.
    void queueBackNavigation(unsigned howFarBackward);
    void queueForwardNavigation(unsigned howFarForward);
    void queueLoad(JSStringRef url, JSStringRef target, bool shouldOpenExternalURLs);
    void queueLoadHTMLString(JSStringRef content, JSStringRef baseURL, JSStringRef unreachableURL);
    void queueReload();
    void queueLoadingScript(JSStringRef script);
    void queueNonLoadingScript(JSStringRef script);

    bool secureEventInputIsEnabled() const;

    JSValueRef failNextNewCodeBlock();
    JSValueRef numberOfDFGCompiles(JSValueRef theFunction);
    JSValueRef neverInlineFunction(JSValueRef theFunction);

    bool shouldDecideNavigationPolicyAfterDelay() const { return m_shouldDecideNavigationPolicyAfterDelay; }
    void setShouldDecideNavigationPolicyAfterDelay(bool);
    bool shouldDecideResponsePolicyAfterDelay() const { return m_shouldDecideResponsePolicyAfterDelay; }
    void setShouldDecideResponsePolicyAfterDelay(bool);
    void setNavigationGesturesEnabled(bool);
    void setIgnoresViewportScaleLimits(bool);
    void setShouldDownloadUndisplayableMIMETypes(bool);

    bool didCancelClientRedirect() const { return m_didCancelClientRedirect; }
    void setDidCancelClientRedirect(bool value) { m_didCancelClientRedirect = value; }

    void runUIScript(JSStringRef script, JSValueRef callback);
    void runUIScriptCallback(unsigned callbackID, JSStringRef result);

    void installDidBeginSwipeCallback(JSValueRef);
    void installWillEndSwipeCallback(JSValueRef);
    void installDidEndSwipeCallback(JSValueRef);
    void installDidRemoveSwipeSnapshotCallback(JSValueRef);
    void callDidBeginSwipeCallback();
    void callWillEndSwipeCallback();
    void callDidEndSwipeCallback();
    void callDidRemoveSwipeSnapshotCallback();

    void clearTestRunnerCallbacks();

    void accummulateLogsForChannel(JSStringRef channel);

    unsigned imageCountInGeneralPasteboard() const;

    // Gamepads
    void connectMockGamepad(unsigned index);
    void disconnectMockGamepad(unsigned index);
    void setMockGamepadDetails(unsigned index, JSStringRef gamepadID, unsigned axisCount, unsigned buttonCount);
    void setMockGamepadAxisValue(unsigned index, unsigned axisIndex, double value);
    void setMockGamepadButtonValue(unsigned index, unsigned buttonIndex, double value);
    
    // Resource Load Statistics
    void installStatisticsDidModifyDataRecordsCallback(JSValueRef callback);
    void installStatisticsDidScanDataRecordsCallback(JSValueRef callback);
    void installStatisticsDidRunTelemetryCallback(JSValueRef callback);
    void statisticsDidModifyDataRecordsCallback();
    void statisticsDidScanDataRecordsCallback();
    void statisticsDidRunTelemetryCallback(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins);
    void statisticsNotifyObserver();
    void statisticsProcessStatisticsAndDataRecords();
    void statisticsUpdateCookieBlocking(JSValueRef completionHandler);
    void statisticsCallDidSetBlockCookiesForHostCallback();
    void statisticsSubmitTelemetry();
    void setStatisticsDebugMode(bool value, JSValueRef completionHandler);
    void statisticsCallDidSetDebugModeCallback();
    void setStatisticsPrevalentResourceForDebugMode(JSStringRef hostName, JSValueRef completionHandler);
    void statisticsCallDidSetPrevalentResourceForDebugModeCallback();
    void setStatisticsLastSeen(JSStringRef hostName, double seconds, JSValueRef completionHandler);
    void statisticsCallDidSetLastSeenCallback();
    void setStatisticsPrevalentResource(JSStringRef hostName, bool value, JSValueRef completionHandler);
    void statisticsCallDidSetPrevalentResourceCallback();
    void setStatisticsVeryPrevalentResource(JSStringRef hostName, bool value, JSValueRef completionHandler);
    void statisticsCallDidSetVeryPrevalentResourceCallback();
    bool isStatisticsPrevalentResource(JSStringRef hostName);
    bool isStatisticsVeryPrevalentResource(JSStringRef hostName);
    bool isStatisticsRegisteredAsSubresourceUnder(JSStringRef subresourceHost, JSStringRef topFrameHost);
    bool isStatisticsRegisteredAsSubFrameUnder(JSStringRef subFrameHost, JSStringRef topFrameHost);
    bool isStatisticsRegisteredAsRedirectingTo(JSStringRef hostRedirectedFrom, JSStringRef hostRedirectedTo);
    void setStatisticsHasHadUserInteraction(JSStringRef hostName, bool value, JSValueRef completionHandler);
    void statisticsCallDidSetHasHadUserInteractionCallback();
    bool isStatisticsHasHadUserInteraction(JSStringRef hostName);
    void setStatisticsGrandfathered(JSStringRef hostName, bool value);
    bool isStatisticsGrandfathered(JSStringRef hostName);
    void setStatisticsSubframeUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName);
    void setStatisticsSubresourceUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName);
    void setStatisticsSubresourceUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo);
    void setStatisticsSubresourceUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom);
    void setStatisticsTopFrameUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo);
    void setStatisticsTopFrameUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom);
    void setStatisticsTimeToLiveUserInteraction(double seconds);
    void setStatisticsNotifyPagesWhenDataRecordsWereScanned(bool);
    void setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setStatisticsNotifyPagesWhenTelemetryWasCaptured(bool value);
    void setStatisticsMinimumTimeBetweenDataRecordsRemoval(double);
    void setStatisticsGrandfatheringTime(double seconds);
    void setStatisticsMaxStatisticsEntries(unsigned);
    void setStatisticsPruneEntriesDownTo(unsigned);
    void statisticsClearInMemoryAndPersistentStore(JSValueRef callback);
    void statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours, JSValueRef callback);
    void statisticsClearThroughWebsiteDataRemoval(JSValueRef callback);
    void statisticsCallClearThroughWebsiteDataRemovalCallback();
    void setStatisticsCacheMaxAgeCap(double seconds);
    void statisticsResetToConsistentState(JSValueRef completionHandler);
    void statisticsCallDidResetToConsistentStateCallback();

    // Injected bundle form client.
    void installTextDidChangeInTextFieldCallback(JSValueRef callback);
    void textDidChangeInTextFieldCallback();
    void installTextFieldDidBeginEditingCallback(JSValueRef callback);
    void textFieldDidBeginEditingCallback();
    void installTextFieldDidEndEditingCallback(JSValueRef callback);
    void textFieldDidEndEditingCallback();

    // Storage Access API
    void setStorageAccessAPIEnabled(bool);
    void getAllStorageAccessEntries(JSValueRef callback);
    void callDidReceiveAllStorageAccessEntriesCallback(Vector<String>& domains);

    // Open panel
    void setOpenPanelFiles(JSValueRef);

    // Modal alerts
    void setShouldDismissJavaScriptAlertsAsynchronously(bool);

    void terminateNetworkProcess();
    void terminateServiceWorkerProcess();

    void removeAllSessionCredentials(JSValueRef);
    void callDidRemoveAllSessionCredentialsCallback();
    
    void getApplicationManifestThen(JSValueRef);
    void didGetApplicationManifest();

    void installFakeHelvetica(JSStringRef configuration);

    void dumpAllHTTPRedirectedResponseHeaders() { m_dumpAllHTTPRedirectedResponseHeaders = true; }
    bool shouldDumpAllHTTPRedirectedResponseHeaders() const { return m_dumpAllHTTPRedirectedResponseHeaders; }

    void addMockCameraDevice(JSStringRef persistentId, JSStringRef label);
    void addMockMicrophoneDevice(JSStringRef persistentId, JSStringRef label);
    void addMockScreenDevice(JSStringRef persistentId, JSStringRef label);
    void clearMockMediaDevices();
    void removeMockMediaDevice(JSStringRef persistentId);
    void resetMockMediaDevices();

    size_t userScriptInjectedCount() const;
    void injectUserScript(JSStringRef);

    void sendDisplayConfigurationChangedMessageForTesting();

    // WebAuthN
    void setWebAuthenticationMockConfiguration(JSValueRef);
    // FIXME(189876)
    void addTestKeyToKeychain(JSStringRef privateKeyBase64, JSStringRef attrLabel, JSStringRef applicationTagBase64);
    void cleanUpKeychain(JSStringRef attrLabel);
    bool keyExistsInKeychain(JSStringRef attrLabel, JSStringRef applicationTagBase64);

    void setCanHandleHTTPSServerTrustEvaluation(bool canHandle);
    bool canDoServerTrustEvaluationInNetworkProcess();
    unsigned long serverTrustEvaluationCallbackCallsCount();

private:
    TestRunner();

    void platformInitialize();
    void initializeWaitToDumpWatchdogTimerIfNeeded();

    void setDumpPixels(bool);
    void setWaitUntilDone(bool);

    void addMockMediaDevice(JSStringRef persistentId, JSStringRef label, const char* type);

    WKRetainPtr<WKURLRef> m_testURL; // Set by InjectedBundlePage once provisional load starts.

    bool m_shouldDumpAllFrameScrollPositions;
    bool m_shouldDumpBackForwardListsForAllWindows;

    bool m_shouldAllowEditing;
    bool m_shouldCloseExtraWindows;

    bool m_dumpEditingCallbacks;
    bool m_dumpStatusCallbacks;
    bool m_dumpTitleChanges;
    bool m_dumpPixels;
    bool m_dumpSelectionRect;
    bool m_dumpFullScreenCallbacks;
    bool m_dumpProgressFinishedCallback;
    bool m_dumpResourceLoadCallbacks;
    bool m_dumpResourceResponseMIMETypes;
    bool m_dumpWillCacheResponse;
    bool m_dumpApplicationCacheDelegateCallbacks;
    bool m_dumpDatabaseCallbacks;
    bool m_dumpPolicyCallbacks { false };
    bool m_disallowIncreaseForApplicationCacheQuota;
    bool m_testRepaint;
    bool m_testRepaintSweepHorizontally;
    bool m_isPrinting;

    bool m_willSendRequestReturnsNull;
    bool m_willSendRequestReturnsNullOnRedirect;
    bool m_shouldStopProvisionalFrameLoads;
    String m_willSendRequestHTTPBody;

    bool m_policyDelegateEnabled;
    bool m_policyDelegatePermissive;
    
    bool m_globalFlag;
    bool m_customFullScreenBehavior;

    WTF::Seconds m_timeout;

    double m_databaseDefaultQuota;
    double m_databaseMaxQuota;

    bool m_shouldDecideNavigationPolicyAfterDelay { false };
    bool m_shouldDecideResponsePolicyAfterDelay { false };
    bool m_shouldFinishAfterDownload { false };
    bool m_didCancelClientRedirect { false };

    bool m_userStyleSheetEnabled;
    WKRetainPtr<WKStringRef> m_userStyleSheetLocation;

    WKRetainPtr<WKArrayRef> m_allowedHosts;

    size_t m_userMediaPermissionRequestCount { 0 };

    PlatformTimerRef m_waitToDumpWatchdogTimer;

    bool m_dumpAllHTTPRedirectedResponseHeaders { false };
};

} // namespace WTR
