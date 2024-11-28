/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

namespace WTR {

class TestRunner : public JSWrappable {
public:
    static Ref<TestRunner> create();
    virtual ~TestRunner();

    // JSWrappable
    virtual JSClassRef wrapperClass();

    void makeWindowObject(JSContextRef);

    bool isIOSFamily() const
    {
#if PLATFORM(IOS_FAMILY)
        return true;
#else
        return false;
#endif
    }

    bool isMac() const
    {
#if PLATFORM(MAC)
        return true;
#else
        return false;
#endif
    }

    bool keyboardAppearsOverContent() const
    {
#if PLATFORM(VISION)
        return true;
#else
        return false;
#endif
    }

    bool isKeyboardImmediatelyAvailable()
    {
#if PLATFORM(VISION)
        return true;
#else
        return false;
#endif
    }

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

    void setRenderTreeDumpOptions(unsigned short);
    unsigned renderTreeDumpOptions() const { return m_renderTreeDumpOptions; }

    // Other dumping.
    void dumpBackForwardList();
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
    void dumpPolicyDelegateCallbacks();
    void dumpResourceLoadStatistics();

    void setShouldDumpFrameLoadCallbacks(bool value);
    void setShouldDumpProgressFinishedCallback(bool value) { m_dumpProgressFinishedCallback = value; }

    // Special options.
    void keepWebHistory();
    void setAcceptsEditing(bool value) { m_shouldAllowEditing = value; }
    void preventPopupWindows();

    void setCustomPolicyDelegate(bool enabled, bool permissive = false);
    void skipPolicyDelegateNotifyDone();
    void addOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);
    void setUserStyleSheetEnabled(bool);
    void setUserStyleSheetLocation(JSStringRef);
    void setTabKeyCyclesThroughElements(bool);
    void setSerializeHTTPLoads();
    void dispatchPendingLoadRequests();
    void setCacheModel(int);
    void setAsynchronousSpellCheckingEnabled(bool);
    void setAllowsAnySSLCertificate(bool);

    void setShouldSwapToEphemeralSessionOnNextNavigation(bool);
    void setShouldSwapToDefaultSessionOnNextNavigation(bool);
    void setCustomUserAgent(JSStringRef);

    // Special DOM functions.
    void clearBackForwardList(JSContextRef, JSValueRef callback);
    void execCommand(JSStringRef name, JSStringRef showUI, JSStringRef value);
    bool isCommandEnabled(JSStringRef name);
    unsigned windowCount();

    // Repaint testing.
    void testRepaint() { m_testRepaint = true; }
    void repaintSweepHorizontally() { m_testRepaintSweepHorizontally = true; }
    void display();
    void displayAndTrackRepaints(JSContextRef, JSValueRef callback);
    void displayOnLoadFinish() { m_displayOnLoadFinish = true; }
    bool shouldDisplayOnLoadFinish() { return m_displayOnLoadFinish; }
    void dontForceRepaint() { m_forceRepaint = false; }
    bool shouldForceRepaint() { return m_forceRepaint; }

    // UserContent testing.
    void addUserScript(JSStringRef source, bool runAtStart, bool allFrames);
    void addUserStyleSheet(JSStringRef source, bool allFrames);

    // Text search testing.
    bool findString(JSContextRef, JSStringRef, JSValueRef optionsArray);
    void findStringMatchesInPage(JSContextRef, JSStringRef, JSValueRef optionsArray);
    void replaceFindMatchesAtIndices(JSContextRef, JSValueRef matchIndices, JSStringRef replacementText, bool selectionOnly);

    // Local storage
    void clearAllDatabases();
    void setDatabaseQuota(uint64_t);
    JSRetainPtr<JSStringRef> pathToLocalResource(JSStringRef);
    void syncLocalStorage();

    void clearDOMCache(JSStringRef origin);
    void clearDOMCaches();
    bool hasDOMCache(JSStringRef origin);
    uint64_t domCacheSize(JSStringRef origin);
    void setAllowStorageQuotaIncrease(bool);
    void setQuota(uint64_t);
    void setOriginQuotaRatioEnabled(bool);

    // Failed load condition testing
    void forceImmediateCompletion();

    // Printing
    bool isPageBoxVisible(JSContextRef, int pageIndex);
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
    bool shouldDumpBackForwardListsForAllWindows() const;
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

    bool didReceiveServerRedirectForProvisionalNavigation() const;
    void clearDidReceiveServerRedirectForProvisionalNavigation();

    bool shouldWaitUntilDone() const;

    // Downloads
    bool shouldFinishAfterDownload() const { return m_shouldFinishAfterDownload; }
    void setShouldLogDownloadCallbacks(bool);
    void setShouldLogDownloadSize(bool);
    void setShouldLogDownloadExpectedSize(bool);
    void setShouldDownloadContentDispositionAttachments(bool);

    bool shouldAllowEditing() const { return m_shouldAllowEditing; }

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
    void setWillSendRequestAddsHTTPBody(JSStringRef body) { m_willSendRequestHTTPBody = toWTFString(body); }
    String willSendRequestHTTPBody() const { return m_willSendRequestHTTPBody; }

    void setTextDirection(JSContextRef, JSStringRef);

    void setShouldStayOnPageAfterHandlingBeforeUnload(bool);

    void setStopProvisionalFrameLoads() { m_shouldStopProvisionalFrameLoads = true; }
    bool shouldStopProvisionalFrameLoads() const { return m_shouldStopProvisionalFrameLoads; }
    
    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool value) { m_globalFlag = value; }

    double databaseDefaultQuota() const { return m_databaseDefaultQuota; }
    void setDatabaseDefaultQuota(double quota) { m_databaseDefaultQuota = quota; }

    double databaseMaxQuota() const { return m_databaseMaxQuota; }
    void setDatabaseMaxQuota(double quota) { m_databaseMaxQuota = quota; }

    void addChromeInputField(JSContextRef, JSValueRef);
    void removeChromeInputField(JSContextRef, JSValueRef);
    void focusWebView(JSContextRef, JSValueRef);

    void setTextInChromeInputField(JSContextRef, JSStringRef text, JSValueRef callback);
    void selectChromeInputField(JSContextRef, JSValueRef callback);
    void getSelectedTextInChromeInputField(JSContextRef, JSValueRef callback);

    void setBackingScaleFactor(JSContextRef, double, JSValueRef);

    void setWindowIsKey(bool);

    void setViewSize(double width, double height);

    static void overridePreference(JSStringRef preference, JSStringRef value);

    // Cookies testing
    void setAlwaysAcceptCookies(bool);
    void setOnlyAcceptFirstPartyCookies(bool);
    void removeAllCookies(JSContextRef, JSValueRef callback);

    // Custom full screen behavior.
    void setHasCustomFullScreenBehavior(bool value) { m_customFullScreenBehavior = value; }
    bool hasCustomFullScreenBehavior() const { return m_customFullScreenBehavior; }
    void setEnterFullscreenForElementCallback(JSContextRef, JSValueRef);
    void callEnterFullscreenForElementCallback();
    void setExitFullscreenForElementCallback(JSContextRef, JSValueRef);
    void callExitFullscreenForElementCallback();

    // Web notifications.
    void grantWebNotificationPermission(JSStringRef origin);
    void denyWebNotificationPermission(JSStringRef origin);
    void denyWebNotificationPermissionOnPrompt(JSStringRef origin);
    void removeAllWebNotificationPermissions();
    void simulateWebNotificationClick(JSContextRef, JSValueRef notification);
    void simulateWebNotificationClickForServiceWorkerNotifications();

    JSRetainPtr<JSStringRef> getBackgroundFetchIdentifier();
    void abortBackgroundFetch(JSStringRef);
    void pauseBackgroundFetch(JSStringRef);
    void resumeBackgroundFetch(JSStringRef);
    void simulateClickBackgroundFetch(JSStringRef);
    void setBackgroundFetchPermission(bool);
    JSRetainPtr<JSStringRef> lastAddedBackgroundFetchIdentifier() const;
    JSRetainPtr<JSStringRef> lastRemovedBackgroundFetchIdentifier() const;
    JSRetainPtr<JSStringRef> lastUpdatedBackgroundFetchIdentifier() const;
    JSRetainPtr<JSStringRef> backgroundFetchState(JSStringRef);

    // Geolocation.
    void setGeolocationPermission(bool);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy, std::optional<double> altitude, std::optional<double> altitudeAccuracy, std::optional<double> heading, std::optional<double> speed, std::optional<double> floorLevel);
    void setMockGeolocationPositionUnavailableError(JSStringRef message);
    bool isGeolocationProviderActive();

    // Screen Wake Lock.
    void setScreenWakeLockPermission(bool);

    // MediaStream
    void setCameraPermission(bool);
    void setMicrophonePermission(bool);
    void setUserMediaPermission(bool);
    void resetUserMediaPermission();
    void setUserMediaPersistentPermissionForOrigin(bool permission, JSStringRef origin, JSStringRef parentOrigin);
    unsigned userMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin) const;
    void resetUserMediaPermissionRequestCountForOrigin(JSStringRef origin, JSStringRef parentOrigin);
    bool isDoingMediaCapture() const;

    void setPageVisibility(JSStringRef state);
    void resetPageVisibility();

    bool callShouldCloseOnWebView(JSContextRef);

    void setCustomTimeout(WTF::Seconds duration) { m_timeout = duration; }

    // Work queue.
    void queueBackNavigation(unsigned howFarBackward);
    void queueForwardNavigation(unsigned howFarForward);
    void queueLoad(JSStringRef url, JSStringRef target, bool shouldOpenExternalURLs);
    void queueLoadHTMLString(JSStringRef content, JSStringRef baseURL, JSStringRef unreachableURL);
    void queueReload();
    void reloadFromOrigin();
    void queueLoadingScript(JSStringRef script);
    void queueNonLoadingScript(JSStringRef script);

    bool secureEventInputIsEnabled() const;

    JSValueRef failNextNewCodeBlock(JSContextRef);
    JSValueRef numberOfDFGCompiles(JSContextRef, JSValueRef function);
    JSValueRef neverInlineFunction(JSContextRef, JSValueRef function);

    bool shouldDecideNavigationPolicyAfterDelay() const { return m_shouldDecideNavigationPolicyAfterDelay; }
    void setShouldDecideNavigationPolicyAfterDelay(bool);
    bool shouldDecideResponsePolicyAfterDelay() const { return m_shouldDecideResponsePolicyAfterDelay; }
    void setShouldDecideResponsePolicyAfterDelay(bool);
    void setNavigationGesturesEnabled(bool);
    void setIgnoresViewportScaleLimits(bool);
    void setUseDarkAppearanceForTesting(bool);
    void setShouldDownloadUndisplayableMIMETypes(bool);
    void setShouldAllowDeviceOrientationAndMotionAccess(bool);
    void stopLoading();

    bool didCancelClientRedirect() const { return m_didCancelClientRedirect; }
    void setDidCancelClientRedirect(bool value) { m_didCancelClientRedirect = value; }

    void runUIScript(JSContextRef, JSStringRef script, JSValueRef callback);
    void runUIScriptImmediately(JSContextRef, JSStringRef script, JSValueRef callback);
    void runUIScriptCallback(unsigned callbackID, JSStringRef result);

    // Contextual menu actions
    void setAllowedMenuActions(JSContextRef, JSValueRef);

    void installDidBeginSwipeCallback(JSContextRef, JSValueRef);
    void installWillEndSwipeCallback(JSContextRef, JSValueRef);
    void installDidEndSwipeCallback(JSContextRef, JSValueRef);
    void installDidRemoveSwipeSnapshotCallback(JSContextRef, JSValueRef);
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
    void setMockGamepadDetails(unsigned index, JSStringRef gamepadID, JSStringRef mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble);
    void setMockGamepadAxisValue(unsigned index, unsigned axisIndex, double value);
    void setMockGamepadButtonValue(unsigned index, unsigned buttonIndex, double value);
    
    // Resource Load Statistics
    void clearStatisticsDataForDomain(JSStringRef domain);
    bool doesStatisticsDomainIDExistInDatabase(unsigned domainID);
    void setStatisticsEnabled(bool value);
    bool isStatisticsEphemeral();
    void statisticsNotifyObserver(JSContextRef, JSValueRef completionHandler);
    void statisticsProcessStatisticsAndDataRecords(JSContextRef, JSValueRef completionHandler);
    void statisticsUpdateCookieBlocking(JSContextRef, JSValueRef completionHandler);
    void setStatisticsDebugMode(JSContextRef, bool value, JSValueRef completionHandler);
    void setStatisticsPrevalentResourceForDebugMode(JSContextRef, JSStringRef hostName, JSValueRef completionHandler);
    void setStatisticsLastSeen(JSContextRef, JSStringRef hostName, double seconds, JSValueRef completionHandler);
    void setStatisticsMergeStatistic(JSContextRef, JSStringRef hostName, JSStringRef topFrameDomain1, JSStringRef topFrameDomain2, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, JSValueRef completionHandler);
    void setStatisticsExpiredStatistic(JSContextRef, JSStringRef hostName, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, JSValueRef completionHandler);
    void setStatisticsPrevalentResource(JSContextRef, JSStringRef hostName, bool value, JSValueRef completionHandler);
    void setStatisticsVeryPrevalentResource(JSContextRef, JSStringRef hostName, bool value, JSValueRef completionHandler);
    bool isStatisticsPrevalentResource(JSStringRef hostName);
    bool isStatisticsVeryPrevalentResource(JSStringRef hostName);
    bool isStatisticsRegisteredAsSubresourceUnder(JSStringRef subresourceHost, JSStringRef topFrameHost);
    bool isStatisticsRegisteredAsSubFrameUnder(JSStringRef subFrameHost, JSStringRef topFrameHost);
    bool isStatisticsRegisteredAsRedirectingTo(JSStringRef hostRedirectedFrom, JSStringRef hostRedirectedTo);
    void setStatisticsHasHadUserInteraction(JSContextRef, JSStringRef hostName, bool value, JSValueRef completionHandler);
    bool isStatisticsHasHadUserInteraction(JSStringRef hostName);
    bool isStatisticsOnlyInDatabaseOnce(JSStringRef subHost, JSStringRef topHost);
    void setStatisticsGrandfathered(JSStringRef hostName, bool value);
    bool isStatisticsGrandfathered(JSStringRef hostName);
    void setStatisticsSubframeUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName);
    void setStatisticsSubresourceUnderTopFrameOrigin(JSStringRef hostName, JSStringRef topFrameHostName);
    void setStatisticsSubresourceUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo);
    void setStatisticsSubresourceUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom);
    void setStatisticsTopFrameUniqueRedirectTo(JSStringRef hostName, JSStringRef hostNameRedirectedTo);
    void setStatisticsTopFrameUniqueRedirectFrom(JSStringRef hostName, JSStringRef hostNameRedirectedFrom);
    void setStatisticsCrossSiteLoadWithLinkDecoration(JSStringRef fromHost, JSStringRef toHost, bool wasFiltered);
    void setStatisticsTimeToLiveUserInteraction(double seconds);
    void setStatisticsTimeAdvanceForTesting(double);
    void setStatisticsIsRunningTest(bool);
    void setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setStatisticsMinimumTimeBetweenDataRecordsRemoval(double);
    void setStatisticsGrandfatheringTime(double seconds);
    void setStatisticsMaxStatisticsEntries(unsigned);
    void setStatisticsPruneEntriesDownTo(unsigned);
    void statisticsClearInMemoryAndPersistentStore(JSContextRef, JSValueRef callback);
    void statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(JSContextRef, unsigned hours, JSValueRef callback);
    void statisticsClearThroughWebsiteDataRemoval(JSContextRef, JSValueRef callback);
    void statisticsDeleteCookiesForHost(JSContextRef, JSStringRef hostName, bool includeHttpOnlyCookies, JSValueRef callback);
    bool isStatisticsHasLocalStorage(JSStringRef hostName);
    void setStatisticsCacheMaxAgeCap(double seconds);
    bool hasStatisticsIsolatedSession(JSStringRef hostName);
    void setStatisticsShouldDowngradeReferrer(JSContextRef, bool, JSValueRef callback);
    void setStatisticsShouldBlockThirdPartyCookies(JSContextRef, bool value, JSValueRef callback, bool onlyOnSitesWithoutUserInteraction);
    void setStatisticsFirstPartyWebsiteDataRemovalMode(JSContextRef, bool value, JSValueRef callback);
    void statisticsSetToSameSiteStrictCookies(JSContextRef, JSStringRef hostName, JSValueRef callback);
    void statisticsSetFirstPartyHostCNAMEDomain(JSContextRef, JSStringRef firstPartyURLString, JSStringRef cnameURLString, JSValueRef completionHandler);
    void statisticsSetThirdPartyCNAMEDomain(JSContextRef, JSStringRef cnameURLString, JSValueRef completionHandler);
    void statisticsResetToConsistentState(JSContextRef, JSValueRef completionHandler);
    void loadedSubresourceDomains(JSContextRef, JSValueRef callback);

    // Injected bundle form client.
    void installTextDidChangeInTextFieldCallback(JSContextRef, JSValueRef callback);
    void textDidChangeInTextFieldCallback();
    void installTextFieldDidBeginEditingCallback(JSContextRef, JSValueRef callback);
    void textFieldDidBeginEditingCallback();
    void installTextFieldDidEndEditingCallback(JSContextRef, JSValueRef callback);
    void textFieldDidEndEditingCallback();

    // Storage Access API
    void getAllStorageAccessEntries(JSContextRef, JSValueRef callback);
    void setRequestStorageAccessThrowsExceptionUntilReload(bool enabled);

    // Open panel
    void setOpenPanelFiles(JSContextRef, JSValueRef);
    void setOpenPanelFilesMediaIcon(JSContextRef, JSValueRef);

    // Modal alerts
    void setShouldDismissJavaScriptAlertsAsynchronously(bool);
    void abortModal();

    void terminateGPUProcess();
    void terminateNetworkProcess();
    void terminateServiceWorkers();
    void setUseSeparateServiceWorkerProcess(bool);

    void removeAllSessionCredentials(JSContextRef, JSValueRef);
    
    void getApplicationManifestThen(JSContextRef, JSValueRef);

    void installFakeHelvetica(JSStringRef configuration);

    void dumpAllHTTPRedirectedResponseHeaders() { m_dumpAllHTTPRedirectedResponseHeaders = true; }
    bool shouldDumpAllHTTPRedirectedResponseHeaders() const { return m_dumpAllHTTPRedirectedResponseHeaders; }

    void addMockCameraDevice(JSContextRef, JSStringRef persistentId, JSStringRef label, JSValueRef properties);
    void addMockMicrophoneDevice(JSContextRef, JSStringRef persistentId, JSStringRef label, JSValueRef propertie);
    void addMockScreenDevice(JSStringRef persistentId, JSStringRef label);
    void clearMockMediaDevices();
    void removeMockMediaDevice(JSStringRef persistentId);
    void setMockMediaDeviceIsEphemeral(JSStringRef persistentId, bool isEphemeral);
    void resetMockMediaDevices();
    void setMockCameraOrientation(unsigned);
    bool isMockRealtimeMediaSourceCenterEnabled();
    void setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted);
    void triggerMockCaptureConfigurationChange(bool forMicrophone, bool forDisplay);
    void setCaptureState(bool cameraState, bool microphoneState, bool displayState);

    bool hasAppBoundSession();
    void clearAppBoundSession();
    void setAppBoundDomains(JSContextRef, JSValueRef originArray, JSValueRef callback);
    void setManagedDomains(JSContextRef, JSValueRef originArray, JSValueRef callback);

    bool didLoadAppInitiatedRequest();
    bool didLoadNonAppInitiatedRequest();

    size_t userScriptInjectedCount() const;
    void injectUserScript(JSStringRef);

    void setServiceWorkerFetchTimeout(double seconds);

    // FIXME(189876)
    void addTestKeyToKeychain(JSStringRef privateKeyBase64, JSStringRef attrLabel, JSStringRef applicationTagBase64);
    void cleanUpKeychain(JSStringRef attrLabel, JSStringRef applicationLabelBase64);
    bool keyExistsInKeychain(JSStringRef attrLabel, JSStringRef applicationLabelBase64);

    unsigned long serverTrustEvaluationCallbackCallsCount();

    void clearMemoryCache();

    // Private Click Measurement.
    void dumpPrivateClickMeasurement();
    void clearPrivateClickMeasurement();
    void clearPrivateClickMeasurementsThroughWebsiteDataRemoval();
    void setPrivateClickMeasurementOverrideTimerForTesting(bool value);
    void setPrivateClickMeasurementTokenPublicKeyURLForTesting(JSStringRef);
    void setPrivateClickMeasurementTokenSignatureURLForTesting(JSStringRef);
    void setPrivateClickMeasurementAttributionReportURLsForTesting(JSStringRef sourceURL, JSStringRef destinationURL);
    void markPrivateClickMeasurementsAsExpiredForTesting();
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting();
    void setPrivateClickMeasurementEphemeralMeasurementForTesting(bool value);
    void setPrivateClickMeasurementFraudPreventionValuesForTesting(JSStringRef unlinkableToken, JSStringRef secretToken, JSStringRef signature, JSStringRef keyID);
    void setPrivateClickMeasurementAppBundleIDForTesting(JSStringRef);
    void simulatePrivateClickMeasurementSessionRestart();

    void setIsSpeechRecognitionPermissionGranted(bool);

    void setIsMediaKeySystemPermissionGranted(bool);

    void takeViewPortSnapshot(JSContextRef, JSValueRef callback);

    void flushConsoleLogs(JSContextRef, JSValueRef callback);

    // Reporting API
    void generateTestReport(JSContextRef, JSStringRef message, JSStringRef group);

    void getAndClearReportedWindowProxyAccessDomains(JSContextRef, JSValueRef);

    void setTopContentInset(JSContextRef, double, JSValueRef);

    void setPageScaleFactor(JSContextRef, double scaleFactor, long x, long y, JSValueRef callback);

private:
    TestRunner();

    void platformInitialize();

    void setDumpPixels(bool);
    void setWaitUntilDone(bool);

    void addMockMediaDevice(JSStringRef persistentId, JSStringRef label, const char* type, WKDictionaryRef);

    WKRetainPtr<WKURLRef> m_testURL; // Set by InjectedBundlePage once provisional load starts.

    String m_willSendRequestHTTPBody;
    WTF::Seconds m_timeout { 30_s };

    WKRetainPtr<WKStringRef> m_userStyleSheetLocation;
    WKRetainPtr<WKArrayRef> m_allowedHosts;

    double m_databaseDefaultQuota { -1 };
    double m_databaseMaxQuota { -1 };

    size_t m_userMediaPermissionRequestCount { 0 };

    unsigned m_renderTreeDumpOptions { 0 };
    bool m_shouldDumpAllFrameScrollPositions { false };
    bool m_shouldAllowEditing { true };

    bool m_dumpEditingCallbacks { false };
    bool m_dumpStatusCallbacks { false };
    bool m_dumpTitleChanges { false };
    bool m_dumpPixels { false };
    bool m_dumpSelectionRect { false };
    bool m_dumpFullScreenCallbacks { false };
    bool m_dumpProgressFinishedCallback { false };
    bool m_dumpResourceLoadCallbacks { false };
    bool m_dumpResourceResponseMIMETypes { false };
    bool m_dumpWillCacheResponse { false };
    bool m_dumpApplicationCacheDelegateCallbacks { false };
    bool m_dumpDatabaseCallbacks { false };

    bool m_testRepaint { false };
    bool m_testRepaintSweepHorizontally { false };
    bool m_displayOnLoadFinish { false };
    bool m_forceRepaint { true };
    bool m_isPrinting { false };
    bool m_willSendRequestReturnsNull { false };
    bool m_willSendRequestReturnsNullOnRedirect { false };
    bool m_shouldStopProvisionalFrameLoads { false };

    bool m_globalFlag { false };
    bool m_customFullScreenBehavior { false };

    bool m_shouldDecideNavigationPolicyAfterDelay { false };
    bool m_shouldDecideResponsePolicyAfterDelay { false };
    bool m_shouldFinishAfterDownload { false };
    bool m_didCancelClientRedirect { false };

    bool m_userStyleSheetEnabled { false };
    bool m_dumpAllHTTPRedirectedResponseHeaders { false };
};

} // namespace WTR
