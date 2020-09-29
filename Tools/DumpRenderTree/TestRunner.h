/*
 * Copyright (C) 2007-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "UIScriptContext.h"
#include "UIScriptController.h"
#include <JavaScriptCore/JSObjectRef.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <wtf/RefCounted.h>

extern FILE* testResult;

class TestRunner : public WTR::UIScriptContextDelegate, public RefCounted<TestRunner> {
    WTF_MAKE_NONCOPYABLE(TestRunner);
public:
    static Ref<TestRunner> create(const std::string& testURL, const std::string& expectedPixelHash);

    static const unsigned viewWidth;
    static const unsigned viewHeight;

    static const unsigned w3cSVGViewWidth;
    static const unsigned w3cSVGViewHeight;

    virtual ~TestRunner();
    
    void cleanup();

    void makeWindowObject(JSContextRef);

    void addDisallowedURL(JSStringRef url);
    const std::set<std::string>& allowedHosts() const { return m_allowedHosts; }
    void setAllowedHosts(std::set<std::string> hosts) { m_allowedHosts = WTFMove(hosts); }
    void addURLToRedirect(std::string origin, std::string destination);
    const char* redirectionDestinationForURL(const char*);
    void clearAllApplicationCaches();
    void clearAllDatabases();
    void clearApplicationCacheForOrigin(JSStringRef name);
    void clearBackForwardList();
    void clearPersistentUserStyleSheet();
    bool callShouldCloseOnWebView();
    JSRetainPtr<JSStringRef> copyDecodedHostName(JSStringRef name);
    JSRetainPtr<JSStringRef> copyEncodedHostName(JSStringRef name);
    void dispatchPendingLoadRequests();
    void display();
    void displayAndTrackRepaints();
    void execCommand(JSStringRef name, JSStringRef value);
    bool findString(JSContextRef, JSStringRef, JSObjectRef optionsArray);
    void forceImmediateCompletion();
    void goBack();
    JSValueRef originsWithApplicationCache(JSContextRef);
    long long applicationCacheDiskUsageForOrigin(JSStringRef name);
    bool isCommandEnabled(JSStringRef name);
    void keepWebHistory();
    void notifyDone();
    int numberOfPendingGeolocationPermissionRequests();
    bool isGeolocationProviderActive();
    void overridePreference(JSStringRef key, JSStringRef value);
    JSRetainPtr<JSStringRef> pathToLocalResource(JSContextRef, JSStringRef url);
    void queueBackNavigation(int howFarBackward);
    void queueForwardNavigation(int howFarForward);
    void queueLoad(JSStringRef url, JSStringRef target);
    void queueLoadHTMLString(JSStringRef content, JSStringRef baseURL);
    void queueLoadAlternateHTMLString(JSStringRef content, JSStringRef baseURL, JSStringRef unreachableURL);
    void queueLoadingScript(JSStringRef script);
    void queueNonLoadingScript(JSStringRef script);
    void queueReload();
    void removeAllVisitedLinks();
    void setAcceptsEditing(bool);
    void setAllowUniversalAccessFromFileURLs(bool);
    void setAllowFileAccessFromFileURLs(bool);
    void setNeedsStorageAccessFromFileURLsQuirk(bool);
    void setAppCacheMaximumSize(unsigned long long quota);
    void setAuthorAndUserStylesEnabled(bool);
    void setCacheModel(int);
    void setCustomPolicyDelegate(bool setDelegate, bool permissive);
    void setDatabaseQuota(unsigned long long quota);
    void setDomainRelaxationForbiddenForURLScheme(bool forbidden, JSStringRef scheme);
    void setDefersLoading(bool);
    void setIconDatabaseEnabled(bool);
    void setJavaScriptCanAccessClipboard(bool flag);
    void setAutomaticLinkDetectionEnabled(bool flag);
    void setMainFrameIsFirstResponder(bool flag);
    void setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, bool providesFloorLevel, double floorLevel);
    void setMockGeolocationPositionUnavailableError(JSStringRef message);
    void setPersistentUserStyleSheetLocation(JSStringRef path);
    void setPluginsEnabled(bool);
    void setPopupBlockingEnabled(bool);
    void setPrivateBrowsingEnabled(bool);

    void willNavigate();
    void setShouldSwapToEphemeralSessionOnNextNavigation(bool shouldSwap) { m_shouldSwapToEphemeralSessionOnNextNavigation = shouldSwap; }
    void setShouldSwapToDefaultSessionOnNextNavigation(bool shouldSwap) { m_shouldSwapToDefaultSessionOnNextNavigation = shouldSwap; }

    void setTabKeyCyclesThroughElements(bool);
    void setUserStyleSheetEnabled(bool flag);
    void setUserStyleSheetLocation(JSStringRef path);
    void setValueForUser(JSContextRef, JSValueRef nodeObject, JSStringRef value);
    void setXSSAuditorEnabled(bool flag);
    void setSpatialNavigationEnabled(bool);
    void setScrollbarPolicy(JSStringRef orientation, JSStringRef policy);
#if PLATFORM(IOS_FAMILY)
    void setTelephoneNumberParsingEnabled(bool enable);
    void setPagePaused(bool paused);
#endif

    void setPageVisibility(const char*);
    void resetPageVisibility();

    static void setAllowsAnySSLCertificate(bool);

    void waitForPolicyDelegate();
    size_t webHistoryItemCount();
    int windowCount();

#if ENABLE(TEXT_AUTOSIZING)
    void setTextAutosizingEnabled(bool);
#endif

    void setAccummulateLogsForChannel(JSStringRef);

    void runUIScript(JSContextRef, JSStringRef, JSValueRef callback);

    // Legacy here refers to the old TestRunner API for handling web notifications, not the legacy web notification API.
    void ignoreLegacyWebNotificationPermissionRequests();
    // Legacy here refers to the old TestRunner API for handling web notifications, not the legacy web notification API.
    void simulateLegacyWebNotificationClick(JSStringRef title);
    void grantWebNotificationPermission(JSStringRef origin);
    void denyWebNotificationPermission(JSStringRef origin);
    void removeAllWebNotificationPermissions();
    void simulateWebNotificationClick(JSValueRef notification);

    void setRenderTreeDumpOptions(unsigned);
    unsigned renderTreeDumpOptions() const { return m_renderTreeDumpOptions; }

    bool dumpAsAudio() const { return m_dumpAsAudio; }
    void setDumpAsAudio(bool dumpAsAudio) { m_dumpAsAudio = dumpAsAudio; }
    
    bool dumpAsPDF() const { return m_dumpAsPDF; }
    void setDumpAsPDF(bool dumpAsPDF) { m_dumpAsPDF = dumpAsPDF; }

    bool dumpAsText() const { return m_dumpAsText; }
    void setDumpAsText(bool dumpAsText) { m_dumpAsText = dumpAsText; }

    bool generatePixelResults() const { return m_generatePixelResults; }
    void setGeneratePixelResults(bool generatePixelResults) { m_generatePixelResults = generatePixelResults; }

    bool disallowIncreaseForApplicationCacheQuota() const { return m_disallowIncreaseForApplicationCacheQuota; }
    void setDisallowIncreaseForApplicationCacheQuota(bool disallowIncrease) { m_disallowIncreaseForApplicationCacheQuota = disallowIncrease; }

    bool dumpApplicationCacheDelegateCallbacks() const { return m_dumpApplicationCacheDelegateCallbacks; }
    void setDumpApplicationCacheDelegateCallbacks(bool dumpCallbacks) { m_dumpApplicationCacheDelegateCallbacks = dumpCallbacks; }

    bool dumpBackForwardList() const { return m_dumpBackForwardList; }
    void setDumpBackForwardList(bool dumpBackForwardList) { m_dumpBackForwardList = dumpBackForwardList; }

    bool dumpChildFrameScrollPositions() const { return m_dumpChildFrameScrollPositions; }
    void setDumpChildFrameScrollPositions(bool dumpChildFrameScrollPositions) { m_dumpChildFrameScrollPositions = dumpChildFrameScrollPositions; }

    bool dumpChildFramesAsText() const { return m_dumpChildFramesAsText; }
    void setDumpChildFramesAsText(bool dumpChildFramesAsText) { m_dumpChildFramesAsText = dumpChildFramesAsText; }

    bool dumpDatabaseCallbacks() const { return m_dumpDatabaseCallbacks; }
    void setDumpDatabaseCallbacks(bool dumpDatabaseCallbacks) { m_dumpDatabaseCallbacks = dumpDatabaseCallbacks; }

    bool dumpDOMAsWebArchive() const { return m_dumpDOMAsWebArchive; }
    void setDumpDOMAsWebArchive(bool dumpDOMAsWebArchive) { m_dumpDOMAsWebArchive = dumpDOMAsWebArchive; }

    bool dumpEditingCallbacks() const { return m_dumpEditingCallbacks; }
    void setDumpEditingCallbacks(bool dumpEditingCallbacks) { m_dumpEditingCallbacks = dumpEditingCallbacks; }

    bool dumpFrameLoadCallbacks() const { return m_dumpFrameLoadCallbacks; }
    void setDumpFrameLoadCallbacks(bool dumpFrameLoadCallbacks) { m_dumpFrameLoadCallbacks = dumpFrameLoadCallbacks; }

    bool dumpProgressFinishedCallback() const { return m_dumpProgressFinishedCallback; }
    void setDumpProgressFinishedCallback(bool dumpProgressFinishedCallback) { m_dumpProgressFinishedCallback = dumpProgressFinishedCallback; }
    
    bool dumpUserGestureInFrameLoadCallbacks() const { return m_dumpUserGestureInFrameLoadCallbacks; }
    void setDumpUserGestureInFrameLoadCallbacks(bool dumpUserGestureInFrameLoadCallbacks) { m_dumpUserGestureInFrameLoadCallbacks = dumpUserGestureInFrameLoadCallbacks; }    

    bool dumpHistoryDelegateCallbacks() const { return m_dumpHistoryDelegateCallbacks; }
    void setDumpHistoryDelegateCallbacks(bool dumpHistoryDelegateCallbacks) { m_dumpHistoryDelegateCallbacks = dumpHistoryDelegateCallbacks; }
    
    bool dumpResourceLoadCallbacks() const { return m_dumpResourceLoadCallbacks; }
    void setDumpResourceLoadCallbacks(bool dumpResourceLoadCallbacks) { m_dumpResourceLoadCallbacks = dumpResourceLoadCallbacks; }
    
    bool dumpResourceResponseMIMETypes() const { return m_dumpResourceResponseMIMETypes; }
    void setDumpResourceResponseMIMETypes(bool dumpResourceResponseMIMETypes) { m_dumpResourceResponseMIMETypes = dumpResourceResponseMIMETypes; }

    bool dumpSelectionRect() const { return m_dumpSelectionRect; }
    void setDumpSelectionRect(bool dumpSelectionRect) { m_dumpSelectionRect = dumpSelectionRect; }

    bool dumpSourceAsWebArchive() const { return m_dumpSourceAsWebArchive; }
    void setDumpSourceAsWebArchive(bool dumpSourceAsWebArchive) { m_dumpSourceAsWebArchive = dumpSourceAsWebArchive; }

    bool dumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    void setDumpStatusCallbacks(bool dumpStatusCallbacks) { m_dumpStatusCallbacks = dumpStatusCallbacks; }

    bool dumpTitleChanges() const { return m_dumpTitleChanges; }
    void setDumpTitleChanges(bool dumpTitleChanges) { m_dumpTitleChanges = dumpTitleChanges; }

    bool dumpVisitedLinksCallback() const { return m_dumpVisitedLinksCallback; }
    void setDumpVisitedLinksCallback(bool dumpVisitedLinksCallback) { m_dumpVisitedLinksCallback = dumpVisitedLinksCallback; }
    
    bool dumpWillCacheResponse() const { return m_dumpWillCacheResponse; }
    void setDumpWillCacheResponse(bool dumpWillCacheResponse) { m_dumpWillCacheResponse = dumpWillCacheResponse; }
    
    bool callCloseOnWebViews() const { return m_callCloseOnWebViews; }
    void setCallCloseOnWebViews(bool callCloseOnWebViews) { m_callCloseOnWebViews = callCloseOnWebViews; }

    bool canOpenWindows() const { return m_canOpenWindows; }
    void setCanOpenWindows(bool canOpenWindows) { m_canOpenWindows = canOpenWindows; }

    bool closeRemainingWindowsWhenComplete() const { return m_closeRemainingWindowsWhenComplete; }
    void setCloseRemainingWindowsWhenComplete(bool closeRemainingWindowsWhenComplete) { m_closeRemainingWindowsWhenComplete = closeRemainingWindowsWhenComplete; }
    
    bool newWindowsCopyBackForwardList() const { return m_newWindowsCopyBackForwardList; }
    void setNewWindowsCopyBackForwardList(bool newWindowsCopyBackForwardList) { m_newWindowsCopyBackForwardList = newWindowsCopyBackForwardList; }
    
    bool stopProvisionalFrameLoads() const { return m_stopProvisionalFrameLoads; }
    void setStopProvisionalFrameLoads(bool stopProvisionalFrameLoads) { m_stopProvisionalFrameLoads = stopProvisionalFrameLoads; }

    bool testOnscreen() const { return m_testOnscreen; }
    void setTestOnscreen(bool testOnscreen) { m_testOnscreen = testOnscreen; }

    bool testRepaint() const { return m_testRepaint; }
    void setTestRepaint(bool testRepaint) { m_testRepaint = testRepaint; }

    bool testRepaintSweepHorizontally() const { return m_testRepaintSweepHorizontally; }
    void setTestRepaintSweepHorizontally(bool testRepaintSweepHorizontally) { m_testRepaintSweepHorizontally = testRepaintSweepHorizontally; }

    bool waitToDump() const { return m_waitToDump; }
    void setWaitToDump(bool);
    void waitToDumpWatchdogTimerFired();

    const std::set<std::string>& willSendRequestClearHeaders() const { return m_willSendRequestClearHeaders; }
    void setWillSendRequestClearHeader(std::string header) { m_willSendRequestClearHeaders.insert(header); }

    bool willSendRequestReturnsNull() const { return m_willSendRequestReturnsNull; }
    void setWillSendRequestReturnsNull(bool returnsNull) { m_willSendRequestReturnsNull = returnsNull; }

    bool willSendRequestReturnsNullOnRedirect() const { return m_willSendRequestReturnsNullOnRedirect; }
    void setWillSendRequestReturnsNullOnRedirect(bool returnsNull) { m_willSendRequestReturnsNullOnRedirect = returnsNull; }

    bool windowIsKey() const { return m_windowIsKey; }
    void setWindowIsKey(bool);

    void setViewSize(double width, double height);

    bool alwaysAcceptCookies() const { return m_alwaysAcceptCookies; }
    void setAlwaysAcceptCookies(bool);
    void setOnlyAcceptFirstPartyCookies(bool);

    bool rejectsProtectionSpaceAndContinueForAuthenticationChallenges() const { return m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges; }
    void setRejectsProtectionSpaceAndContinueForAuthenticationChallenges(bool value) { m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges = value; }
    
    bool handlesAuthenticationChallenges() const { return m_handlesAuthenticationChallenges; }
    void setHandlesAuthenticationChallenges(bool handlesAuthenticationChallenges) { m_handlesAuthenticationChallenges = handlesAuthenticationChallenges; }
    
    bool isPrinting() const { return m_isPrinting; }
    void setIsPrinting(bool isPrinting) { m_isPrinting = isPrinting; }

    const std::string& authenticationUsername() const { return m_authenticationUsername; }
    void setAuthenticationUsername(std::string username) { m_authenticationUsername = username; }
    
    const std::string& authenticationPassword() const { return m_authenticationPassword; }
    void setAuthenticationPassword(std::string password) { m_authenticationPassword = password; }

    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool globalFlag) { m_globalFlag = globalFlag; }
    
    double databaseDefaultQuota() const { return m_databaseDefaultQuota; }
    void setDatabaseDefaultQuota(double quota) { m_databaseDefaultQuota = quota; }

    double databaseMaxQuota() const { return m_databaseMaxQuota; }
    void setDatabaseMaxQuota(double quota) { m_databaseMaxQuota = quota; }

    bool useDeferredFrameLoading() const { return m_useDeferredFrameLoading; }
    void setUseDeferredFrameLoading(bool flag) { m_useDeferredFrameLoading = flag; }

    const std::string& testURL() const { return m_testURL; }
    const std::string& expectedPixelHash() const { return m_expectedPixelHash; }

    const std::vector<char>& audioResult() const { return m_audioResult; }
    void setAudioResult(const std::vector<char>& audioData) { m_audioResult = audioData; }

    void addOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);

    void addUserScript(JSStringRef source, bool runAtStart, bool allFrames);
    void addUserStyleSheet(JSStringRef source, bool allFrames);

    void setGeolocationPermission(bool allow);
    bool isGeolocationPermissionSet() const { return m_isGeolocationPermissionSet; }
    bool geolocationPermission() const { return m_geolocationPermission; }

    void setDeveloperExtrasEnabled(bool);
    void showWebInspector();
    void closeWebInspector();
    void evaluateInWebInspector(JSStringRef script);
    JSRetainPtr<JSStringRef> inspectorTestStubURL();

    void evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script);
    void evaluateScriptInIsolatedWorldAndReturnValue(unsigned worldID, JSObjectRef globalObject, JSStringRef script);

    bool shouldStayOnPageAfterHandlingBeforeUnload() const { return m_shouldStayOnPageAfterHandlingBeforeUnload; }
    void setShouldStayOnPageAfterHandlingBeforeUnload(bool shouldStayOnPageAfterHandlingBeforeUnload) { m_shouldStayOnPageAfterHandlingBeforeUnload = shouldStayOnPageAfterHandlingBeforeUnload; }

    void addChromeInputField();
    void removeChromeInputField();
    void focusWebView();

    void setBackingScaleFactor(double);

    void setPOSIXLocale(JSStringRef);

    void setWebViewEditable(bool);

    void abortModal();

    static void setSerializeHTTPLoads(bool);

    // The following API test functions should probably be moved to platform-specific 
    // unit tests outside of DRT once they exist.
    void apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL);
    void apiTestGoToCurrentBackForwardItem();

    // Simulate a request an embedding application could make, populating per-session credential storage.
    void authenticateSession(JSStringRef url, JSStringRef username, JSStringRef password);

    void setShouldPaintBrokenImage(bool);
    bool shouldPaintBrokenImage() const { return m_shouldPaintBrokenImage; }

    void setTextDirection(JSStringRef);
    const std::string& titleTextDirection() const { return m_titleTextDirection; }
    void setTitleTextDirection(const std::string& direction) { m_titleTextDirection = direction; }

    // Custom full screen behavior.
    void setHasCustomFullScreenBehavior(bool value) { m_customFullScreenBehavior = value; }
    bool hasCustomFullScreenBehavior() const { return m_customFullScreenBehavior; }

    void setStorageDatabaseIdleInterval(double);
    void closeIdleLocalStorageDatabases();

    bool hasPendingWebNotificationClick() const { return m_hasPendingWebNotificationClick; }

    void setCustomTimeout(int duration) { m_timeout = duration; }
    double timeout() { return m_timeout; }

    unsigned imageCountInGeneralPasteboard() const;

    void callUIScriptCallback(unsigned callbackID, JSStringRef result);

    void setDumpJSConsoleLogInStdErr(bool inStdErr) { m_dumpJSConsoleLogInStdErr = inStdErr; }
    bool dumpJSConsoleLogInStdErr() const { return m_dumpJSConsoleLogInStdErr; }

    void setSpellCheckerLoggingEnabled(bool);

    const std::vector<std::string>& openPanelFiles() const { return m_openPanelFiles; }
    void setOpenPanelFiles(JSContextRef, JSValueRef);

#if PLATFORM(IOS_FAMILY)
    const std::vector<char>& openPanelFilesMediaIcon() const { return m_openPanelFilesMediaIcon; }
    void setOpenPanelFilesMediaIcon(JSContextRef, JSValueRef);
#endif

    bool didCancelClientRedirect() const { return m_didCancelClientRedirect; }
    void setDidCancelClientRedirect(bool value) { m_didCancelClientRedirect = value; }

private:
    TestRunner(const std::string& testURL, const std::string& expectedPixelHash);

    JSContextRef mainFrameJSContext();

    // UIScriptContextDelegate
    void uiScriptDidComplete(const String&, unsigned callbackID) override;
    
    void cacheTestRunnerCallback(unsigned index, JSValueRef);
    void callTestRunnerCallback(unsigned index, size_t argumentCount = 0, const JSValueRef arguments[] = nullptr);
    void clearTestRunnerCallbacks();

    void setGeolocationPermissionCommon(bool allow);

    bool m_disallowIncreaseForApplicationCacheQuota { false };
    bool m_dumpApplicationCacheDelegateCallbacks { false };
    bool m_dumpAsAudio { false };
    bool m_dumpAsPDF { false };
    bool m_dumpAsText { false };
    bool m_dumpBackForwardList { false };
    bool m_dumpChildFrameScrollPositions { false };
    bool m_dumpChildFramesAsText { false };
    bool m_dumpDOMAsWebArchive { false };
    bool m_dumpDatabaseCallbacks { false };
    bool m_dumpEditingCallbacks { false };
    bool m_dumpFrameLoadCallbacks { false };
    bool m_dumpProgressFinishedCallback { false };
    bool m_dumpUserGestureInFrameLoadCallbacks { false };
    bool m_dumpHistoryDelegateCallbacks { false };
    bool m_dumpResourceLoadCallbacks { false };
    bool m_dumpResourceResponseMIMETypes { false };
    bool m_dumpSelectionRect { false };
    bool m_dumpSourceAsWebArchive { false };
    bool m_dumpStatusCallbacks { false };
    bool m_dumpTitleChanges { false };
    bool m_dumpVisitedLinksCallback { false };
    bool m_dumpWillCacheResponse { false };
    bool m_generatePixelResults { true };
    bool m_callCloseOnWebViews { true };
    bool m_canOpenWindows { false };
    bool m_closeRemainingWindowsWhenComplete { true };
    bool m_newWindowsCopyBackForwardList { false };
    bool m_stopProvisionalFrameLoads { false };
    bool m_testOnscreen { false };
    bool m_testRepaint { false };
    bool m_testRepaintSweepHorizontally { false };
    bool m_waitToDump  { false }; // True if waitUntilDone() has been called, but notifyDone() has not yet been called.
    bool m_willSendRequestReturnsNull { false };
    bool m_willSendRequestReturnsNullOnRedirect { false };
    bool m_windowIsKey { true };
    bool m_alwaysAcceptCookies { false };
    bool m_globalFlag { false };
    bool m_isGeolocationPermissionSet { false };
    bool m_geolocationPermission { false };
    bool m_rejectsProtectionSpaceAndContinueForAuthenticationChallenges { false };
    bool m_handlesAuthenticationChallenges { false };
    bool m_isPrinting { false };
    bool m_useDeferredFrameLoading { false };
    bool m_shouldPaintBrokenImage { true };
    bool m_shouldStayOnPageAfterHandlingBeforeUnload { false };
    // FIXME 81697: This variable most likely will be removed once we have migrated the tests from fast/notifications to http/tests/notifications.
    bool m_areLegacyWebNotificationPermissionRequestsIgnored { false };
    bool m_customFullScreenBehavior { false };
    bool m_hasPendingWebNotificationClick { false };
    bool m_dumpJSConsoleLogInStdErr { false };
    bool m_didCancelClientRedirect { false };
    bool m_shouldSwapToEphemeralSessionOnNextNavigation { false };
    bool m_shouldSwapToDefaultSessionOnNextNavigation { false };

    double m_databaseDefaultQuota { -1 };
    double m_databaseMaxQuota { -1 };

    int m_timeout { 0 };
    unsigned m_renderTreeDumpOptions { 0 };

    std::string m_authenticationUsername;
    std::string m_authenticationPassword; 
    std::string m_testURL;
    std::string m_expectedPixelHash; // empty string if no hash
    std::string m_titleTextDirection;

    std::set<std::string> m_willSendRequestClearHeaders;
    std::set<std::string> m_allowedHosts;

    std::vector<char> m_audioResult;

    std::map<std::string, std::string> m_URLsToRedirect;

    struct UIScriptInvocationData {
        unsigned callbackID;
        String scriptString;
    };

    std::unique_ptr<WTR::UIScriptContext> m_UIScriptContext;
    UIScriptInvocationData* m_pendingUIScriptInvocationData { nullptr };

    std::vector<std::string> m_openPanelFiles;
#if PLATFORM(IOS_FAMILY)
    std::vector<char> m_openPanelFilesMediaIcon;
#endif

    static JSRetainPtr<JSClassRef> createJSClass();
    static const JSStaticValue* staticValues();
    static const JSStaticFunction* staticFunctions();
};
