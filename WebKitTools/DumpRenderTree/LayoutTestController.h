/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
 
#ifndef LayoutTestController_h
#define LayoutTestController_h

#include <JavaScriptCore/JSObjectRef.h>
#include <wtf/RefCounted.h>
#include <string>
#include <vector>

class LayoutTestController : public RefCounted<LayoutTestController> {
public:
    LayoutTestController(const std::string& testPathOrURL, const std::string& expectedPixelHash);
    ~LayoutTestController();

    void makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception);

    void addDisallowedURL(JSStringRef url);
    void clearAllDatabases();
    void clearBackForwardList();
    void clearPersistentUserStyleSheet();
    JSStringRef copyDecodedHostName(JSStringRef name);
    JSStringRef copyEncodedHostName(JSStringRef name);
    void disableImageLoading();
    void dispatchPendingLoadRequests();
    void display();
    void execCommand(JSStringRef name, JSStringRef value);
    bool isCommandEnabled(JSStringRef name);
    void keepWebHistory();
    void notifyDone();
    void overridePreference(JSStringRef key, JSStringRef value);
    JSStringRef pathToLocalResource(JSContextRef, JSStringRef url);
    void queueBackNavigation(int howFarBackward);
    void queueForwardNavigation(int howFarForward);
    void queueLoad(JSStringRef url, JSStringRef target);
    void queueLoadingScript(JSStringRef script);
    void queueNonLoadingScript(JSStringRef script);
    void queueReload();
    void removeAllVisitedLinks();
    void setAcceptsEditing(bool acceptsEditing);
    void setAppCacheMaximumSize(unsigned long long quota);
    void setAuthorAndUserStylesEnabled(bool);
    void setCacheModel(int);
    void setCustomPolicyDelegate(bool setDelegate, bool permissive);
    void setDatabaseQuota(unsigned long long quota);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy);
    void setMockGeolocationError(int code, JSStringRef message);
    void setIconDatabaseEnabled(bool iconDatabaseEnabled);
    void setJavaScriptProfilingEnabled(bool profilingEnabled);
    void setMainFrameIsFirstResponder(bool flag);
    void setPersistentUserStyleSheetLocation(JSStringRef path);
    void setPopupBlockingEnabled(bool flag);
    void setPrivateBrowsingEnabled(bool flag);
    void setXSSAuditorEnabled(bool flag);
    void setSelectTrailingWhitespaceEnabled(bool flag);
    void setSmartInsertDeleteEnabled(bool flag);
    void setTabKeyCyclesThroughElements(bool cycles);
    void setUseDashboardCompatibilityMode(bool flag);
    void setUserStyleSheetEnabled(bool flag);
    void setUserStyleSheetLocation(JSStringRef path);
    void waitForPolicyDelegate();
    size_t webHistoryItemCount();
    unsigned workerThreadCount() const;
    int windowCount();
    
    void grantDesktopNotificationPermission(JSStringRef origin);
    bool checkDesktopNotificationPermission(JSStringRef origin);

    bool elementDoesAutoCompleteForElementWithId(JSStringRef id);

    bool dumpAsPDF() const { return m_dumpAsPDF; }
    void setDumpAsPDF(bool dumpAsPDF) { m_dumpAsPDF = dumpAsPDF; }

    bool dumpAsText() const { return m_dumpAsText; }
    void setDumpAsText(bool dumpAsText) { m_dumpAsText = dumpAsText; }
    
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
    
    bool stopProvisionalFrameLoads() const { return m_stopProvisionalFrameLoads; }
    void setStopProvisionalFrameLoads(bool stopProvisionalFrameLoads) { m_stopProvisionalFrameLoads = stopProvisionalFrameLoads; }

    bool testOnscreen() const { return m_testOnscreen; }
    void setTestOnscreen(bool testOnscreen) { m_testOnscreen = testOnscreen; }

    bool testRepaint() const { return m_testRepaint; }
    void setTestRepaint(bool testRepaint) { m_testRepaint = testRepaint; }

    bool testRepaintSweepHorizontally() const { return m_testRepaintSweepHorizontally; }
    void setTestRepaintSweepHorizontally(bool testRepaintSweepHorizontally) { m_testRepaintSweepHorizontally = testRepaintSweepHorizontally; }

    bool waitToDump() const { return m_waitToDump; }
    void setWaitToDump(bool waitToDump);
    void waitToDumpWatchdogTimerFired();

    bool willSendRequestReturnsNullOnRedirect() const { return m_willSendRequestReturnsNullOnRedirect; }
    void setWillSendRequestReturnsNullOnRedirect(bool returnsNull) { m_willSendRequestReturnsNullOnRedirect = returnsNull; }

    bool windowIsKey() const { return m_windowIsKey; }
    void setWindowIsKey(bool windowIsKey);

    bool alwaysAcceptCookies() const { return m_alwaysAcceptCookies; }
    void setAlwaysAcceptCookies(bool alwaysAcceptCookies);
    
    bool handlesAuthenticationChallenges() const { return m_handlesAuthenticationChallenges; }
    void setHandlesAuthenticationChallenges(bool handlesAuthenticationChallenges) { m_handlesAuthenticationChallenges = handlesAuthenticationChallenges; }
    
    const std::string& authenticationUsername() const { return m_authenticationUsername; }
    void setAuthenticationUsername(std::string username) { m_authenticationUsername = username; }
    
    const std::string& authenticationPassword() const { return m_authenticationPassword; }
    void setAuthenticationPassword(std::string password) { m_authenticationPassword = password; }

    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool globalFlag) { m_globalFlag = globalFlag; }
    
    const std::string& testPathOrURL() const { return m_testPathOrURL; }
    const std::string& expectedPixelHash() const { return m_expectedPixelHash; }
    
    bool pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId);
    bool pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId);
    unsigned numberOfActiveAnimations() const;

    void whiteListAccessFromOrigin(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains);

    void addUserScript(JSStringRef source, bool runAtStart);
    void addUserStyleSheet(JSStringRef source);

    void setGeolocationPermission(bool allow);
    bool isGeolocationPermissionSet() const { return m_isGeolocationPermissionSet; }
    bool geolocationPermission() const { return m_geolocationPermission; }

    void showWebInspector();
    void closeWebInspector();
    void evaluateInWebInspector(long callId, JSStringRef script);
    void evaluateScriptInIsolatedWorld(unsigned worldId, JSObjectRef globalObject, JSStringRef script);

    void setPOSIXLocale(JSStringRef locale);

private:
    bool m_dumpAsPDF;
    bool m_dumpAsText;
    bool m_dumpBackForwardList;
    bool m_dumpChildFrameScrollPositions;
    bool m_dumpChildFramesAsText;
    bool m_dumpDOMAsWebArchive;
    bool m_dumpDatabaseCallbacks;
    bool m_dumpEditingCallbacks;
    bool m_dumpFrameLoadCallbacks;
    bool m_dumpHistoryDelegateCallbacks;
    bool m_dumpResourceLoadCallbacks;
    bool m_dumpResourceResponseMIMETypes;
    bool m_dumpSelectionRect;
    bool m_dumpSourceAsWebArchive;
    bool m_dumpStatusCallbacks;
    bool m_dumpTitleChanges;
    bool m_dumpVisitedLinksCallback;
    bool m_dumpWillCacheResponse;
    bool m_callCloseOnWebViews;
    bool m_canOpenWindows;
    bool m_closeRemainingWindowsWhenComplete;
    bool m_stopProvisionalFrameLoads;
    bool m_testOnscreen;
    bool m_testRepaint;
    bool m_testRepaintSweepHorizontally;
    bool m_waitToDump; // True if waitUntilDone() has been called, but notifyDone() has not yet been called.
    bool m_willSendRequestReturnsNullOnRedirect;
    bool m_windowIsKey;
    bool m_alwaysAcceptCookies;
    bool m_globalFlag;
    bool m_isGeolocationPermissionSet;
    bool m_geolocationPermission;
    bool m_handlesAuthenticationChallenges;

    std::string m_authenticationUsername;
    std::string m_authenticationPassword; 
    std::string m_testPathOrURL;
    std::string m_expectedPixelHash;    // empty string if no hash
    
    // origins which have been granted desktop notification access
    std::vector<JSStringRef> m_desktopNotificationAllowedOrigins;
    
    static JSClassRef getJSClass();
    static JSStaticValue* staticValues();
    static JSStaticFunction* staticFunctions();
};

#endif // LayoutTestController_h
