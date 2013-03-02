/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Pawel Hajdan (phajdan.jr@chromium.org)
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestRunner.h"

#include "MockWebSpeechInputController.h"
#include "MockWebSpeechRecognizer.h"
#include "NotificationPresenter.h"
#include "TestInterfaces.h"
#include "WebBindings.h"
#include "WebDataSource.h"
#include "WebDeviceOrientation.h"
#include "WebDeviceOrientationClientMock.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFindOptions.h"
#include "WebFrame.h"
#include "WebGeolocationClientMock.h"
#include "WebInputElement.h"
#include "WebPermissions.h"
#include "WebPreferences.h"
#include "WebScriptSource.h"
#include "WebSecurityPolicy.h"
#include "WebSerializedScriptValue.h"
#include "WebSettings.h"
#include "WebSurroundingText.h"
#include "WebTask.h"
#include "WebTestDelegate.h"
#include "WebTestProxy.h"
#include "WebView.h"
#include "v8/include/v8.h"
#include <limits>
#include <memory>
#include <public/WebData.h>
#include <public/WebPoint.h>
#include <public/WebURLResponse.h>

#if defined(__linux__) || defined(ANDROID)
#include "linux/WebFontRendering.h"
#endif

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

namespace {

class InvokeCallbackTask : public WebMethodTask<TestRunner> {
public:
    InvokeCallbackTask(TestRunner* object, auto_ptr<CppVariant> callbackArguments)
        : WebMethodTask<TestRunner>(object)
        , m_callbackArguments(callbackArguments)
    {
    }

    virtual void runIfValid()
    {
        CppVariant invokeResult;
        m_callbackArguments->invokeDefault(m_callbackArguments.get(), 1, invokeResult);
    }

private:
    auto_ptr<CppVariant> m_callbackArguments;
};

}

TestRunner::WorkQueue::~WorkQueue()
{
    reset();
}

void TestRunner::WorkQueue::processWorkSoon()
{
    if (m_controller->topLoadingFrame())
        return;

    if (!m_queue.empty()) {
        // We delay processing queued work to avoid recursion problems.
        m_controller->m_delegate->postTask(new WorkQueueTask(this));
    } else if (!m_controller->m_waitUntilDone)
        m_controller->m_delegate->testFinished();
}

void TestRunner::WorkQueue::processWork()
{
    // Quit doing work once a load is in progress.
    while (!m_queue.empty()) {
        bool startedLoad = m_queue.front()->run(m_controller->m_delegate, m_controller->m_webView);
        delete m_queue.front();
        m_queue.pop_front();
        if (startedLoad)
            return;
    }

    if (!m_controller->m_waitUntilDone && !m_controller->topLoadingFrame())
        m_controller->m_delegate->testFinished();
}

void TestRunner::WorkQueue::reset()
{
    m_frozen = false;
    while (!m_queue.empty()) {
        delete m_queue.front();
        m_queue.pop_front();
    }
}

void TestRunner::WorkQueue::addWork(WorkItem* work)
{
    if (m_frozen) {
        delete work;
        return;
    }
    m_queue.push_back(work);
}


TestRunner::TestRunner(TestInterfaces* interfaces)
    : m_testIsRunning(false)
    , m_closeRemainingWindows(false)
    , m_workQueue(this)
    , m_testInterfaces(interfaces)
    , m_delegate(0)
    , m_webView(0)
    , m_webPermissions(new WebPermissions)
#if ENABLE_NOTIFICATIONS
    , m_notificationPresenter(new NotificationPresenter)
#endif
{
    // Initialize the map that associates methods of this class with the names
    // they will use when called by JavaScript. The actual binding of those
    // names to their methods will be done by calling bindToJavaScript() (defined
    // by CppBoundClass, the parent to TestRunner).

    // Methods controlling test execution.
    bindMethod("notifyDone", &TestRunner::notifyDone);
    bindMethod("queueBackNavigation", &TestRunner::queueBackNavigation);
    bindMethod("queueForwardNavigation", &TestRunner::queueForwardNavigation);
    bindMethod("queueLoadingScript", &TestRunner::queueLoadingScript);
    bindMethod("queueLoad", &TestRunner::queueLoad);
    bindMethod("queueLoadHTMLString", &TestRunner::queueLoadHTMLString);
    bindMethod("queueNonLoadingScript", &TestRunner::queueNonLoadingScript);
    bindMethod("queueReload", &TestRunner::queueReload);
    bindMethod("setCloseRemainingWindowsWhenComplete", &TestRunner::setCloseRemainingWindowsWhenComplete);
    bindMethod("setCustomPolicyDelegate", &TestRunner::setCustomPolicyDelegate);
    bindMethod("waitForPolicyDelegate", &TestRunner::waitForPolicyDelegate);
    bindMethod("waitUntilDone", &TestRunner::waitUntilDone);
    bindMethod("windowCount", &TestRunner::windowCount);
    // Methods implemented in terms of chromium's public WebKit API.
    bindMethod("setTabKeyCyclesThroughElements", &TestRunner::setTabKeyCyclesThroughElements);
    bindMethod("execCommand", &TestRunner::execCommand);
    bindMethod("isCommandEnabled", &TestRunner::isCommandEnabled);
    bindMethod("elementDoesAutoCompleteForElementWithId", &TestRunner::elementDoesAutoCompleteForElementWithId);
    bindMethod("callShouldCloseOnWebView", &TestRunner::callShouldCloseOnWebView);
    bindMethod("setDomainRelaxationForbiddenForURLScheme", &TestRunner::setDomainRelaxationForbiddenForURLScheme);
    bindMethod("evaluateScriptInIsolatedWorldAndReturnValue", &TestRunner::evaluateScriptInIsolatedWorldAndReturnValue);
    bindMethod("evaluateScriptInIsolatedWorld", &TestRunner::evaluateScriptInIsolatedWorld);
    bindMethod("setIsolatedWorldSecurityOrigin", &TestRunner::setIsolatedWorldSecurityOrigin);
    bindMethod("setIsolatedWorldContentSecurityPolicy", &TestRunner::setIsolatedWorldContentSecurityPolicy);
    bindMethod("addOriginAccessWhitelistEntry", &TestRunner::addOriginAccessWhitelistEntry);
    bindMethod("removeOriginAccessWhitelistEntry", &TestRunner::removeOriginAccessWhitelistEntry);
    bindMethod("hasCustomPageSizeStyle", &TestRunner::hasCustomPageSizeStyle);
    bindMethod("forceRedSelectionColors", &TestRunner::forceRedSelectionColors);
    bindMethod("addUserScript", &TestRunner::addUserScript);
    bindMethod("addUserStyleSheet", &TestRunner::addUserStyleSheet);
    bindMethod("startSpeechInput", &TestRunner::startSpeechInput);
    bindMethod("findString", &TestRunner::findString);
    bindMethod("setValueForUser", &TestRunner::setValueForUser);
    bindMethod("enableFixedLayoutMode", &TestRunner::enableFixedLayoutMode);
    bindMethod("setFixedLayoutSize", &TestRunner::setFixedLayoutSize);
    bindMethod("selectionAsMarkup", &TestRunner::selectionAsMarkup);
    bindMethod("setTextSubpixelPositioning", &TestRunner::setTextSubpixelPositioning);
    bindMethod("resetPageVisibility", &TestRunner::resetPageVisibility);
    bindMethod("setPageVisibility", &TestRunner::setPageVisibility);
    bindMethod("setTextDirection", &TestRunner::setTextDirection);
    bindMethod("textSurroundingNode", &TestRunner::textSurroundingNode);
    bindMethod("disableAutoResizeMode", &TestRunner::disableAutoResizeMode);
    bindMethod("enableAutoResizeMode", &TestRunner::enableAutoResizeMode);
    bindMethod("setSmartInsertDeleteEnabled", &TestRunner::setSmartInsertDeleteEnabled);
    bindMethod("setSelectTrailingWhitespaceEnabled", &TestRunner::setSelectTrailingWhitespaceEnabled);
    bindMethod("setMockDeviceOrientation", &TestRunner::setMockDeviceOrientation);
    bindMethod("didAcquirePointerLock", &TestRunner::didAcquirePointerLock);
    bindMethod("didLosePointerLock", &TestRunner::didLosePointerLock);
    bindMethod("didNotAcquirePointerLock", &TestRunner::didNotAcquirePointerLock);
    bindMethod("setPointerLockWillRespondAsynchronously", &TestRunner::setPointerLockWillRespondAsynchronously);
    bindMethod("setPointerLockWillFailSynchronously", &TestRunner::setPointerLockWillFailSynchronously);

    // The following modify WebPreferences.
    bindMethod("setUserStyleSheetEnabled", &TestRunner::setUserStyleSheetEnabled);
    bindMethod("setUserStyleSheetLocation", &TestRunner::setUserStyleSheetLocation);
    bindMethod("setAuthorAndUserStylesEnabled", &TestRunner::setAuthorAndUserStylesEnabled);
    bindMethod("setPopupBlockingEnabled", &TestRunner::setPopupBlockingEnabled);
    bindMethod("setJavaScriptCanAccessClipboard", &TestRunner::setJavaScriptCanAccessClipboard);
    bindMethod("setXSSAuditorEnabled", &TestRunner::setXSSAuditorEnabled);
    bindMethod("setAllowUniversalAccessFromFileURLs", &TestRunner::setAllowUniversalAccessFromFileURLs);
    bindMethod("setAllowFileAccessFromFileURLs", &TestRunner::setAllowFileAccessFromFileURLs);
    bindMethod("overridePreference", &TestRunner::overridePreference);
    bindMethod("setPluginsEnabled", &TestRunner::setPluginsEnabled);
    bindMethod("setAsynchronousSpellCheckingEnabled", &TestRunner::setAsynchronousSpellCheckingEnabled);
    bindMethod("setTouchDragDropEnabled", &TestRunner::setTouchDragDropEnabled);

    // The following modify the state of the TestRunner.
    bindMethod("dumpEditingCallbacks", &TestRunner::dumpEditingCallbacks);
    bindMethod("dumpAsText", &TestRunner::dumpAsText);
    bindMethod("dumpChildFramesAsText", &TestRunner::dumpChildFramesAsText);
    bindMethod("dumpChildFrameScrollPositions", &TestRunner::dumpChildFrameScrollPositions);
    bindMethod("setAudioData", &TestRunner::setAudioData);
    bindMethod("dumpFrameLoadCallbacks", &TestRunner::dumpFrameLoadCallbacks);
    bindMethod("dumpUserGestureInFrameLoadCallbacks", &TestRunner::dumpUserGestureInFrameLoadCallbacks);
    bindMethod("setStopProvisionalFrameLoads", &TestRunner::setStopProvisionalFrameLoads);
    bindMethod("dumpTitleChanges", &TestRunner::dumpTitleChanges);
    bindMethod("dumpCreateView", &TestRunner::dumpCreateView);
    bindMethod("setCanOpenWindows", &TestRunner::setCanOpenWindows);
    bindMethod("dumpResourceLoadCallbacks", &TestRunner::dumpResourceLoadCallbacks);
    bindMethod("dumpResourceRequestCallbacks", &TestRunner::dumpResourceRequestCallbacks);
    bindMethod("dumpResourceResponseMIMETypes", &TestRunner::dumpResourceResponseMIMETypes);
    bindMethod("dumpPermissionClientCallbacks", &TestRunner::dumpPermissionClientCallbacks);
    bindMethod("setImagesAllowed", &TestRunner::setImagesAllowed);
    bindMethod("setScriptsAllowed", &TestRunner::setScriptsAllowed);
    bindMethod("setStorageAllowed", &TestRunner::setStorageAllowed);
    bindMethod("setPluginsAllowed", &TestRunner::setPluginsAllowed);
    bindMethod("setAllowDisplayOfInsecureContent", &TestRunner::setAllowDisplayOfInsecureContent);
    bindMethod("setAllowRunningOfInsecureContent", &TestRunner::setAllowRunningOfInsecureContent);
    bindMethod("dumpStatusCallbacks", &TestRunner::dumpWindowStatusChanges);
    bindMethod("dumpProgressFinishedCallback", &TestRunner::dumpProgressFinishedCallback);
    bindMethod("dumpBackForwardList", &TestRunner::dumpBackForwardList);
    bindMethod("setDeferMainResourceDataLoad", &TestRunner::setDeferMainResourceDataLoad);
    bindMethod("dumpSelectionRect", &TestRunner::dumpSelectionRect);
    bindMethod("testRepaint", &TestRunner::testRepaint);
    bindMethod("repaintSweepHorizontally", &TestRunner::repaintSweepHorizontally);
    bindMethod("setPrinting", &TestRunner::setPrinting);
    bindMethod("setShouldStayOnPageAfterHandlingBeforeUnload", &TestRunner::setShouldStayOnPageAfterHandlingBeforeUnload);
    bindMethod("setWillSendRequestClearHeader", &TestRunner::setWillSendRequestClearHeader);
    bindMethod("setWillSendRequestReturnsNull", &TestRunner::setWillSendRequestReturnsNull);
    bindMethod("setWillSendRequestReturnsNullOnRedirect", &TestRunner::setWillSendRequestReturnsNullOnRedirect);
    bindMethod("dumpResourceRequestPriorities", &TestRunner::dumpResourceRequestPriorities);

    // The following methods interact with the WebTestProxy.
    // The following methods interact with the WebTestDelegate.
    bindMethod("showWebInspector", &TestRunner::showWebInspector);
    bindMethod("closeWebInspector", &TestRunner::closeWebInspector);
    bindMethod("evaluateInWebInspector", &TestRunner::evaluateInWebInspector);
    bindMethod("clearAllDatabases", &TestRunner::clearAllDatabases);
    bindMethod("setDatabaseQuota", &TestRunner::setDatabaseQuota);
    bindMethod("setAlwaysAcceptCookies", &TestRunner::setAlwaysAcceptCookies);
    bindMethod("setWindowIsKey", &TestRunner::setWindowIsKey);
    bindMethod("pathToLocalResource", &TestRunner::pathToLocalResource);
    bindMethod("setBackingScaleFactor", &TestRunner::setBackingScaleFactor);
    bindMethod("setPOSIXLocale", &TestRunner::setPOSIXLocale);
    bindMethod("numberOfPendingGeolocationPermissionRequests", &TestRunner:: numberOfPendingGeolocationPermissionRequests);
    bindMethod("setGeolocationPermission", &TestRunner::setGeolocationPermission);
    bindMethod("setMockGeolocationPositionUnavailableError", &TestRunner::setMockGeolocationPositionUnavailableError);
    bindMethod("setMockGeolocationPosition", &TestRunner::setMockGeolocationPosition);
#if ENABLE_NOTIFICATIONS
    bindMethod("grantWebNotificationPermission", &TestRunner::grantWebNotificationPermission);
    bindMethod("simulateLegacyWebNotificationClick", &TestRunner::simulateLegacyWebNotificationClick);
#endif
    bindMethod("addMockSpeechInputResult", &TestRunner::addMockSpeechInputResult);
    bindMethod("setMockSpeechInputDumpRect", &TestRunner::setMockSpeechInputDumpRect);
    bindMethod("addMockSpeechRecognitionResult", &TestRunner::addMockSpeechRecognitionResult);
    bindMethod("setMockSpeechRecognitionError", &TestRunner::setMockSpeechRecognitionError);
    bindMethod("wasMockSpeechRecognitionAborted", &TestRunner::wasMockSpeechRecognitionAborted);
    bindMethod("display", &TestRunner::display);
    bindMethod("displayInvalidatedRegion", &TestRunner::displayInvalidatedRegion);

    // Properties.
    bindProperty("globalFlag", &m_globalFlag);
    bindProperty("titleTextDirection", &m_titleTextDirection);
    bindProperty("platformName", &m_platformName);
    // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
    bindProperty("webHistoryItemCount", &m_webHistoryItemCount);
    bindProperty("interceptPostMessage", &m_interceptPostMessage);

    // The following are stubs.
    bindMethod("dumpDatabaseCallbacks", &TestRunner::notImplemented);
    bindMethod("denyWebNotificationPermission", &TestRunner::notImplemented);
    bindMethod("removeAllWebNotificationPermissions", &TestRunner::notImplemented);
    bindMethod("simulateWebNotificationClick", &TestRunner::notImplemented);
    bindMethod("setIconDatabaseEnabled", &TestRunner::notImplemented);
    bindMethod("setScrollbarPolicy", &TestRunner::notImplemented);
    bindMethod("clearAllApplicationCaches", &TestRunner::notImplemented);
    bindMethod("clearApplicationCacheForOrigin", &TestRunner::notImplemented);
    bindMethod("clearBackForwardList", &TestRunner::notImplemented);
    bindMethod("keepWebHistory", &TestRunner::notImplemented);
    bindMethod("setApplicationCacheOriginQuota", &TestRunner::notImplemented);
    bindMethod("setCallCloseOnWebViews", &TestRunner::notImplemented);
    bindMethod("setMainFrameIsFirstResponder", &TestRunner::notImplemented);
    bindMethod("setPrivateBrowsingEnabled", &TestRunner::notImplemented);
    bindMethod("setUseDashboardCompatibilityMode", &TestRunner::notImplemented);
    bindMethod("deleteAllLocalStorage", &TestRunner::notImplemented);
    bindMethod("localStorageDiskUsageForOrigin", &TestRunner::notImplemented);
    bindMethod("originsWithLocalStorage", &TestRunner::notImplemented);
    bindMethod("deleteLocalStorageForOrigin", &TestRunner::notImplemented);
    bindMethod("observeStorageTrackerNotifications", &TestRunner::notImplemented);
    bindMethod("syncLocalStorage", &TestRunner::notImplemented);
    bindMethod("addDisallowedURL", &TestRunner::notImplemented);
    bindMethod("applicationCacheDiskUsageForOrigin", &TestRunner::notImplemented);
    bindMethod("abortModal", &TestRunner::notImplemented);

    // The fallback method is called when an unknown method is invoked.
    bindFallbackMethod(&TestRunner::fallbackMethod);
}

TestRunner::~TestRunner()
{
}

void TestRunner::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
    m_webPermissions->setDelegate(delegate);
#if ENABLE_NOTIFICATIONS
    m_notificationPresenter->setDelegate(delegate);
#endif
}

void TestRunner::setWebView(WebView* webView, WebTestProxyBase* proxy)
{
    m_webView = webView;
    m_proxy = proxy;
}

void TestRunner::reset()
{
    if (m_webView) {
        m_webView->setZoomLevel(false, 0);
        m_webView->setTabKeyCyclesThroughElements(true);
#if !defined(__APPLE__) && !defined(WIN32) // Actually, TOOLKIT_GTK
        // (Constants copied because we can't depend on the header that defined
        // them from this file.)
        m_webView->setSelectionColors(0xff1e90ff, 0xff000000, 0xffc8c8c8, 0xff323232);
#endif
        m_webView->removeAllUserContent();
        m_webView->disableAutoResizeMode();
    }
    m_topLoadingFrame = 0;
    m_waitUntilDone = false;
    m_policyDelegateEnabled = false;
    m_policyDelegateIsPermissive = false;
    m_policyDelegateShouldNotifyDone = false;

    WebSecurityPolicy::resetOriginAccessWhitelists();
#if defined(__linux__) || defined(ANDROID)
    WebFontRendering::setSubpixelPositioning(false);
#endif

    if (m_delegate) {
        // Reset the default quota for each origin to 5MB
        m_delegate->setDatabaseQuota(5 * 1024 * 1024);
        m_delegate->setDeviceScaleFactor(1);
        m_delegate->setAcceptAllCookies(false);
        m_delegate->setLocale("");
    }

    m_dumpEditingCallbacks = false;
    m_dumpAsText = false;
    m_generatePixelResults = true;
    m_dumpChildFrameScrollPositions = false;
    m_dumpChildFramesAsText = false;
    m_dumpAsAudio = false;
    m_dumpFrameLoadCallbacks = false;
    m_dumpUserGestureInFrameLoadCallbacks = false;
    m_stopProvisionalFrameLoads = false;
    m_dumpTitleChanges = false;
    m_dumpCreateView = false;
    m_canOpenWindows = false;
    m_dumpResourceLoadCallbacks = false;
    m_dumpResourceRequestCallbacks = false;
    m_dumpResourceResponseMIMETypes = false;
    m_dumpWindowStatusChanges = false;
    m_dumpProgressFinishedCallback = false;
    m_dumpBackForwardList = false;
    m_deferMainResourceDataLoad = true;
    m_dumpSelectionRect = false;
    m_testRepaint = false;
    m_sweepHorizontally = false;
    m_isPrinting = false;
    m_shouldStayOnPageAfterHandlingBeforeUnload = false;
    m_shouldBlockRedirects = false;
    m_willSendRequestShouldReturnNull = false;
    m_smartInsertDeleteEnabled = true;
#ifdef WIN32
    m_selectTrailingWhitespaceEnabled = true;
#else
    m_selectTrailingWhitespaceEnabled = false;
#endif
    m_shouldDumpResourcePriorities = false;

    m_httpHeadersToClear.clear();

    m_globalFlag.set(false);
    m_titleTextDirection.set("ltr");
    m_webHistoryItemCount.set(0);
    m_interceptPostMessage.set(false);
    m_platformName.set("chromium");

    m_userStyleSheetLocation = WebURL();

    m_webPermissions->reset();

#if ENABLE_NOTIFICATIONS
    m_notificationPresenter->reset();
#endif
    m_pointerLocked = false;
    m_pointerLockPlannedResult = PointerLockWillSucceed;

    m_taskList.revokeAll();
    m_workQueue.reset();

    if (m_closeRemainingWindows && m_delegate)
        m_delegate->closeRemainingWindows();
    else
        m_closeRemainingWindows = true;
}


void TestRunner::setTestIsRunning(bool running)
{
    m_testIsRunning = running;
}

bool TestRunner::shouldDumpEditingCallbacks() const
{
    return m_dumpEditingCallbacks;
}

void TestRunner::checkResponseMimeType()
{
    // Text output: the test page can request different types of output
    // which we handle here.
    if (!m_dumpAsText) {
        string mimeType = m_webView->mainFrame()->dataSource()->response().mimeType().utf8();
        if (mimeType == "text/plain") {
            m_dumpAsText = true;
            m_generatePixelResults = false;
        }
    }
}

bool TestRunner::shouldDumpAsText()
{
    checkResponseMimeType();
    return m_dumpAsText;
}

void TestRunner::setShouldDumpAsText(bool value)
{
    m_dumpAsText = value;
}

bool TestRunner::shouldGeneratePixelResults()
{
    checkResponseMimeType();
    return m_generatePixelResults;
}

void TestRunner::setShouldGeneratePixelResults(bool value)
{
    m_generatePixelResults = value;
}

bool TestRunner::shouldDumpChildFrameScrollPositions() const
{
    return m_dumpChildFrameScrollPositions;
}

bool TestRunner::shouldDumpChildFramesAsText() const
{
    return m_dumpChildFramesAsText;
}

bool TestRunner::shouldDumpAsAudio() const
{
    return m_dumpAsAudio;
}

const WebArrayBufferView* TestRunner::audioData() const
{
    return &m_audioData;
}

bool TestRunner::shouldDumpFrameLoadCallbacks() const
{
    return m_testIsRunning && m_dumpFrameLoadCallbacks;
}

void TestRunner::setShouldDumpFrameLoadCallbacks(bool value)
{
    m_dumpFrameLoadCallbacks = value;
}

bool TestRunner::shouldDumpUserGestureInFrameLoadCallbacks() const
{
    return m_testIsRunning && m_dumpUserGestureInFrameLoadCallbacks;
}

bool TestRunner::stopProvisionalFrameLoads() const
{
    return m_stopProvisionalFrameLoads;
}

bool TestRunner::shouldDumpTitleChanges() const
{
    return m_dumpTitleChanges;
}

bool TestRunner::shouldDumpCreateView() const
{
    return m_dumpCreateView;
}

bool TestRunner::canOpenWindows() const
{
    return m_canOpenWindows;
}

bool TestRunner::shouldDumpResourceLoadCallbacks() const
{
    return m_testIsRunning && m_dumpResourceLoadCallbacks;
}

bool TestRunner::shouldDumpResourceRequestCallbacks() const
{
    return m_testIsRunning && m_dumpResourceRequestCallbacks;
}

bool TestRunner::shouldDumpResourceResponseMIMETypes() const
{
    return m_testIsRunning && m_dumpResourceResponseMIMETypes;
}

WebPermissionClient* TestRunner::webPermissions() const
{
    return m_webPermissions.get();
}

bool TestRunner::shouldDumpStatusCallbacks() const
{
    return m_dumpWindowStatusChanges;
}

bool TestRunner::shouldDumpProgressFinishedCallback() const
{
    return m_dumpProgressFinishedCallback;
}

bool TestRunner::shouldDumpBackForwardList() const
{
    return m_dumpBackForwardList;
}

bool TestRunner::deferMainResourceDataLoad() const
{
    return m_deferMainResourceDataLoad;
}

bool TestRunner::shouldDumpSelectionRect() const
{
    return m_dumpSelectionRect;
}

bool TestRunner::testRepaint() const
{
    return m_testRepaint;
}

bool TestRunner::sweepHorizontally() const
{
    return m_sweepHorizontally;
}

bool TestRunner::isPrinting() const
{
    return m_isPrinting;
}

bool TestRunner::shouldStayOnPageAfterHandlingBeforeUnload() const
{
    return m_shouldStayOnPageAfterHandlingBeforeUnload;
}

void TestRunner::setTitleTextDirection(WebKit::WebTextDirection dir)
{
    m_titleTextDirection.set(dir == WebKit::WebTextDirectionLeftToRight ? "ltr" : "rtl");
}

const std::set<std::string>* TestRunner::httpHeadersToClear() const
{
    return &m_httpHeadersToClear;
}

bool TestRunner::shouldBlockRedirects() const
{
    return m_shouldBlockRedirects;
}

bool TestRunner::willSendRequestShouldReturnNull() const
{
    return m_willSendRequestShouldReturnNull;
}

void TestRunner::setTopLoadingFrame(WebFrame* frame, bool clear)
{
    if (frame->top()->view() != m_webView)
        return;
    if (clear) {
        m_topLoadingFrame = 0;
        locationChangeDone();
    } else if (!m_topLoadingFrame)
        m_topLoadingFrame = frame;
}

WebFrame* TestRunner::topLoadingFrame() const
{
    return m_topLoadingFrame;
}

void TestRunner::policyDelegateDone()
{
    WEBKIT_ASSERT(m_waitUntilDone);
    m_delegate->testFinished();
    m_waitUntilDone = false;
}

bool TestRunner::policyDelegateEnabled() const
{
    return m_policyDelegateEnabled;
}

bool TestRunner::policyDelegateIsPermissive() const
{
    return m_policyDelegateIsPermissive;
}

bool TestRunner::policyDelegateShouldNotifyDone() const
{
    return m_policyDelegateShouldNotifyDone;
}

bool TestRunner::shouldInterceptPostMessage() const
{
    return m_interceptPostMessage.isBool() && m_interceptPostMessage.toBoolean();
}

bool TestRunner::isSmartInsertDeleteEnabled() const
{
    return m_smartInsertDeleteEnabled;
}

bool TestRunner::isSelectTrailingWhitespaceEnabled() const
{
    return m_selectTrailingWhitespaceEnabled;
}

bool TestRunner::shouldDumpResourcePriorities() const
{
    return m_shouldDumpResourcePriorities;
}

#if ENABLE_NOTIFICATIONS
WebNotificationPresenter* TestRunner::notificationPresenter() const
{
    return m_notificationPresenter.get();
}
#endif

bool TestRunner::requestPointerLock()
{
    switch (m_pointerLockPlannedResult) {
    case PointerLockWillSucceed:
        m_delegate->postDelayedTask(new HostMethodTask(this, &TestRunner::didAcquirePointerLockInternal), 0);
        return true;
    case PointerLockWillRespondAsync:
        WEBKIT_ASSERT(!m_pointerLocked);
        return true;
    case PointerLockWillFailSync:
        WEBKIT_ASSERT(!m_pointerLocked);
        return false;
    default:
        WEBKIT_ASSERT_NOT_REACHED();
        return false;
    }
}

void TestRunner::requestPointerUnlock()
{
    m_delegate->postDelayedTask(new HostMethodTask(this, &TestRunner::didLosePointerLockInternal), 0);
}

bool TestRunner::isPointerLocked()
{
    return m_pointerLocked;
}

void TestRunner::didAcquirePointerLockInternal()
{
    m_pointerLocked = true;
    m_webView->didAcquirePointerLock();

    // Reset planned result to default.
    m_pointerLockPlannedResult = PointerLockWillSucceed;
}

void TestRunner::didNotAcquirePointerLockInternal()
{
    WEBKIT_ASSERT(!m_pointerLocked);
    m_pointerLocked = false;
    m_webView->didNotAcquirePointerLock();

    // Reset planned result to default.
    m_pointerLockPlannedResult = PointerLockWillSucceed;
}

void TestRunner::didLosePointerLockInternal()
{
    bool wasLocked = m_pointerLocked;
    m_pointerLocked = false;
    if (wasLocked)
        m_webView->didLosePointerLock();
}

void TestRunner::showDevTools()
{
    m_delegate->showDevTools();
}

void TestRunner::waitUntilDone(const CppArgumentList&, CppVariant* result)
{
    if (!m_delegate->isBeingDebugged())
        m_delegate->postDelayedTask(new NotifyDoneTimedOutTask(this), m_delegate->layoutTestTimeout());
    m_waitUntilDone = true;
    result->setNull();
}

void TestRunner::notifyDone(const CppArgumentList&, CppVariant* result)
{
    // Test didn't timeout. Kill the timeout timer.
    taskList()->revokeAll();

    completeNotifyDone(false);
    result->setNull();
}

void TestRunner::completeNotifyDone(bool isTimeout)
{
    if (m_waitUntilDone && !topLoadingFrame() && m_workQueue.isEmpty()) {
        if (isTimeout)
            m_delegate->testTimedOut();
        else
            m_delegate->testFinished();
    }
    m_waitUntilDone = false;
}

class WorkItemBackForward : public TestRunner::WorkItem {
public:
    WorkItemBackForward(int distance) : m_distance(distance) { }
    bool run(WebTestDelegate* delegate, WebView*)
    {
        delegate->goToOffset(m_distance);
        return true; // FIXME: Did it really start a navigation?
    }

private:
    int m_distance;
};

void TestRunner::queueBackNavigation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isNumber())
        m_workQueue.addWork(new WorkItemBackForward(-arguments[0].toInt32()));
    result->setNull();
}

void TestRunner::queueForwardNavigation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isNumber())
        m_workQueue.addWork(new WorkItemBackForward(arguments[0].toInt32()));
    result->setNull();
}

class WorkItemReload : public TestRunner::WorkItem {
public:
    bool run(WebTestDelegate* delegate, WebView*)
    {
        delegate->reload();
        return true;
    }
};

void TestRunner::queueReload(const CppArgumentList&, CppVariant* result)
{
    m_workQueue.addWork(new WorkItemReload);
    result->setNull();
}

class WorkItemLoadingScript : public TestRunner::WorkItem {
public:
    WorkItemLoadingScript(const string& script) : m_script(script) { }
    bool run(WebTestDelegate*, WebView* webView)
    {
        webView->mainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(m_script)));
        return true; // FIXME: Did it really start a navigation?
    }

private:
    string m_script;
};

class WorkItemNonLoadingScript : public TestRunner::WorkItem {
public:
    WorkItemNonLoadingScript(const string& script) : m_script(script) { }
    bool run(WebTestDelegate*, WebView* webView)
    {
        webView->mainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(m_script)));
        return false;
    }

private:
    string m_script;
};

void TestRunner::queueLoadingScript(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString())
        m_workQueue.addWork(new WorkItemLoadingScript(arguments[0].toString()));
    result->setNull();
}

void TestRunner::queueNonLoadingScript(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString())
        m_workQueue.addWork(new WorkItemNonLoadingScript(arguments[0].toString()));
    result->setNull();
}

class WorkItemLoad : public TestRunner::WorkItem {
public:
    WorkItemLoad(const WebURL& url, const string& target)
        : m_url(url)
        , m_target(target) { }
    bool run(WebTestDelegate* delegate, WebView*)
    {
        delegate->loadURLForFrame(m_url, m_target);
        return true; // FIXME: Did it really start a navigation?
    }

private:
    WebURL m_url;
    string m_target;
};

void TestRunner::queueLoad(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        // FIXME: Implement WebURL::resolve() and avoid GURL.
        GURL currentURL = m_webView->mainFrame()->document().url();
        GURL fullURL = currentURL.Resolve(arguments[0].toString());

        string target = "";
        if (arguments.size() > 1 && arguments[1].isString())
            target = arguments[1].toString();

        m_workQueue.addWork(new WorkItemLoad(fullURL, target));
    }
    result->setNull();
}

class WorkItemLoadHTMLString : public TestRunner::WorkItem  {
public:
    WorkItemLoadHTMLString(const std::string& html, const WebURL& baseURL)
        : m_html(html)
        , m_baseURL(baseURL) { }
    WorkItemLoadHTMLString(const std::string& html, const WebURL& baseURL, const WebURL& unreachableURL)
        : m_html(html)
        , m_baseURL(baseURL)
        , m_unreachableURL(unreachableURL) { }
    bool run(WebTestDelegate*, WebView* webView)
    {
        webView->mainFrame()->loadHTMLString(
            WebKit::WebData(m_html.data(), m_html.length()), m_baseURL, m_unreachableURL);
        return true;
    }

private:
    std::string m_html;
    WebURL m_baseURL;
    WebURL m_unreachableURL;
};

void TestRunner::queueLoadHTMLString(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        string html = arguments[0].toString();
        WebURL baseURL(GURL(""));
        if (arguments.size() > 1 && arguments[1].isString())
            baseURL = WebURL(GURL(arguments[1].toString()));
        if (arguments.size() > 2 && arguments[2].isString())
            m_workQueue.addWork(new WorkItemLoadHTMLString(html, baseURL, WebURL(GURL(arguments[2].toString()))));
        else
            m_workQueue.addWork(new WorkItemLoadHTMLString(html, baseURL));
    }
    result->setNull();
}

void TestRunner::locationChangeDone()
{
    m_webHistoryItemCount.set(m_delegate->navigationEntryCount());

    // No more new work after the first complete load.
    m_workQueue.setFrozen(true);

    if (!m_waitUntilDone)
        m_workQueue.processWorkSoon();
}

void TestRunner::windowCount(const CppArgumentList&, CppVariant* result)
{
    result->set(static_cast<int>(m_testInterfaces->windowList().size()));
}

void TestRunner::setCloseRemainingWindowsWhenComplete(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_closeRemainingWindows = arguments[0].value.boolValue;
    result->setNull();
}

void TestRunner::setCustomPolicyDelegate(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_policyDelegateEnabled = arguments[0].value.boolValue;
        m_policyDelegateIsPermissive = false;
        if (arguments.size() > 1 && arguments[1].isBool())
            m_policyDelegateIsPermissive = arguments[1].value.boolValue;
    }
    result->setNull();
}

void TestRunner::waitForPolicyDelegate(const CppArgumentList&, CppVariant* result)
{
    m_policyDelegateEnabled = true;
    m_policyDelegateShouldNotifyDone = true;
    m_waitUntilDone = true;
    result->setNull();
}

void TestRunner::dumpPermissionClientCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_webPermissions->setDumpCallbacks(true);
    result->setNull();
}

void TestRunner::setImagesAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_webPermissions->setImagesAllowed(arguments[0].toBoolean());
    result->setNull();
}

void TestRunner::setScriptsAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_webPermissions->setScriptsAllowed(arguments[0].toBoolean());
    result->setNull();
}

void TestRunner::setStorageAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_webPermissions->setStorageAllowed(arguments[0].toBoolean());
    result->setNull();
}

void TestRunner::setPluginsAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_webPermissions->setPluginsAllowed(arguments[0].toBoolean());
    result->setNull();
}

void TestRunner::setAllowDisplayOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_webPermissions->setDisplayingInsecureContentAllowed(arguments[0].toBoolean());

    result->setNull();
}

void TestRunner::setAllowRunningOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_webPermissions->setRunningInsecureContentAllowed(arguments[0].value.boolValue);

    result->setNull();
}

void TestRunner::dumpWindowStatusChanges(const CppArgumentList&, CppVariant* result)
{
    m_dumpWindowStatusChanges = true;
    result->setNull();
}

void TestRunner::dumpProgressFinishedCallback(const CppArgumentList&, CppVariant* result)
{
    m_dumpProgressFinishedCallback = true;
    result->setNull();
}

void TestRunner::dumpBackForwardList(const CppArgumentList&, CppVariant* result)
{
    m_dumpBackForwardList = true;
    result->setNull();
}

void TestRunner::setDeferMainResourceDataLoad(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1)
        m_deferMainResourceDataLoad = cppVariantToBool(arguments[0]);
}

void TestRunner::dumpSelectionRect(const CppArgumentList& arguments, CppVariant* result)
{
    m_dumpSelectionRect = true;
    result->setNull();
}

void TestRunner::testRepaint(const CppArgumentList&, CppVariant* result)
{
    m_testRepaint = true;
    result->setNull();
}

void TestRunner::repaintSweepHorizontally(const CppArgumentList&, CppVariant* result)
{
    m_sweepHorizontally = true;
    result->setNull();
}

void TestRunner::setPrinting(const CppArgumentList& arguments, CppVariant* result)
{
    m_isPrinting = true;
    result->setNull();
}

void TestRunner::setShouldStayOnPageAfterHandlingBeforeUnload(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1 && arguments[0].isBool())
        m_shouldStayOnPageAfterHandlingBeforeUnload = arguments[0].toBoolean();

    result->setNull();
}

void TestRunner::setWillSendRequestClearHeader(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        string header = arguments[0].toString();
        if (!header.empty())
            m_httpHeadersToClear.insert(header);
    }
    result->setNull();
}

void TestRunner::setWillSendRequestReturnsNullOnRedirect(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shouldBlockRedirects = arguments[0].toBoolean();
    result->setNull();
}

void TestRunner::setWillSendRequestReturnsNull(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_willSendRequestShouldReturnNull = arguments[0].toBoolean();
    result->setNull();
}

void TestRunner::setTabKeyCyclesThroughElements(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_webView->setTabKeyCyclesThroughElements(arguments[0].toBoolean());
    result->setNull();
}

void TestRunner::execCommand(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <= 0 || !arguments[0].isString())
        return;

    std::string command = arguments[0].toString();
    std::string value("");
    // Ignore the second parameter (which is userInterface)
    // since this command emulates a manual action.
    if (arguments.size() >= 3 && arguments[2].isString())
        value = arguments[2].toString();

    // Note: webkit's version does not return the boolean, so neither do we.
    m_webView->focusedFrame()->executeCommand(WebString::fromUTF8(command), WebString::fromUTF8(value));
}

void TestRunner::isCommandEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() <= 0 || !arguments[0].isString()) {
        result->setNull();
        return;
    }

    std::string command = arguments[0].toString();
    bool rv = m_webView->focusedFrame()->isCommandEnabled(WebString::fromUTF8(command));
    result->set(rv);
}

bool TestRunner::elementDoesAutoCompleteForElementWithId(const WebString& elementId)
{
    WebFrame* webFrame = m_webView->mainFrame();
    if (!webFrame)
        return false;

    WebElement element = webFrame->document().getElementById(elementId);
    if (element.isNull() || !element.hasTagName("input"))
        return false;

    WebInputElement inputElement = element.to<WebInputElement>();
    return inputElement.autoComplete();
}

void TestRunner::elementDoesAutoCompleteForElementWithId(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
    WebString elementId = cppVariantToWebString(arguments[0]);
    result->set(elementDoesAutoCompleteForElementWithId(elementId));
}

void TestRunner::callShouldCloseOnWebView(const CppArgumentList&, CppVariant* result)
{
    result->set(m_webView->dispatchBeforeUnloadEvent());
}

void TestRunner::setDomainRelaxationForbiddenForURLScheme(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 2 || !arguments[0].isBool() || !arguments[1].isString())
        return;
    m_webView->setDomainRelaxationForbidden(cppVariantToBool(arguments[0]), cppVariantToWebString(arguments[1]));
}

void TestRunner::evaluateScriptInIsolatedWorldAndReturnValue(const CppArgumentList& arguments, CppVariant* result)
{
    v8::HandleScope scope;
    WebVector<v8::Local<v8::Value> > values;
    if (arguments.size() >= 2 && arguments[0].isNumber() && arguments[1].isString()) {
        WebScriptSource source(cppVariantToWebString(arguments[1]));
        // This relies on the iframe focusing itself when it loads. This is a bit
        // sketchy, but it seems to be what other tests do.
        m_webView->focusedFrame()->executeScriptInIsolatedWorld(arguments[0].toInt32(), &source, 1, 1, &values);
    }
    result->setNull();
    // Since only one script was added, only one result is expected
    if (values.size() == 1 && !values[0].IsEmpty()) {
        v8::Local<v8::Value> scriptValue = values[0];
        // FIXME: There are many more types that can be handled.
        if (scriptValue->IsString()) {
            v8::String::AsciiValue asciiV8(scriptValue);
            result->set(std::string(*asciiV8));
        } else if (scriptValue->IsBoolean())
            result->set(scriptValue->ToBoolean()->Value());
        else if (scriptValue->IsNumber()) {
            if (scriptValue->IsInt32())
                result->set(scriptValue->ToInt32()->Value());
            else
                result->set(scriptValue->ToNumber()->Value());
        } else if (scriptValue->IsNull())
            result->setNull();
    }
}

void TestRunner::evaluateScriptInIsolatedWorld(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() >= 2 && arguments[0].isNumber() && arguments[1].isString()) {
        WebScriptSource source(cppVariantToWebString(arguments[1]));
        // This relies on the iframe focusing itself when it loads. This is a bit
        // sketchy, but it seems to be what other tests do.
        m_webView->focusedFrame()->executeScriptInIsolatedWorld(arguments[0].toInt32(), &source, 1, 1);
    }
    result->setNull();
}

void TestRunner::setIsolatedWorldSecurityOrigin(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !(arguments[1].isString() || arguments[1].isNull()))
        return;

    WebSecurityOrigin origin;
    if (arguments[1].isString())
        origin = WebSecurityOrigin::createFromString(cppVariantToWebString(arguments[1]));
    m_webView->focusedFrame()->setIsolatedWorldSecurityOrigin(arguments[0].toInt32(), origin);
}

void TestRunner::setIsolatedWorldContentSecurityPolicy(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;

    m_webView->focusedFrame()->setIsolatedWorldContentSecurityPolicy(arguments[0].toInt32(), cppVariantToWebString(arguments[1]));
}

void TestRunner::addOriginAccessWhitelistEntry(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 4 || !arguments[0].isString() || !arguments[1].isString()
        || !arguments[2].isString() || !arguments[3].isBool())
        return;

    WebKit::WebURL url(GURL(arguments[0].toString()));
    if (!url.isValid())
        return;

    WebSecurityPolicy::addOriginAccessWhitelistEntry(
        url,
        cppVariantToWebString(arguments[1]),
        cppVariantToWebString(arguments[2]),
        arguments[3].toBoolean());
}

void TestRunner::removeOriginAccessWhitelistEntry(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 4 || !arguments[0].isString() || !arguments[1].isString()
        || !arguments[2].isString() || !arguments[3].isBool())
        return;

    WebKit::WebURL url(GURL(arguments[0].toString()));
    if (!url.isValid())
        return;

    WebSecurityPolicy::removeOriginAccessWhitelistEntry(
        url,
        cppVariantToWebString(arguments[1]),
        cppVariantToWebString(arguments[2]),
        arguments[3].toBoolean());
}

void TestRunner::hasCustomPageSizeStyle(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(false);
    int pageIndex = 0;
    if (arguments.size() > 1)
        return;
    if (arguments.size() == 1)
        pageIndex = cppVariantToInt32(arguments[0]);
    WebFrame* frame = m_webView->mainFrame();
    if (!frame)
        return;
    result->set(frame->hasCustomPageSizeStyle(pageIndex));
}

void TestRunner::forceRedSelectionColors(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    m_webView->setSelectionColors(0xffee0000, 0xff00ee00, 0xff000000, 0xffc0c0c0);
}

void TestRunner::addUserScript(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isString() || !arguments[1].isBool() || !arguments[2].isBool())
        return;
    WebView::addUserScript(
        cppVariantToWebString(arguments[0]), WebVector<WebString>(),
        arguments[1].toBoolean() ? WebView::UserScriptInjectAtDocumentStart : WebView::UserScriptInjectAtDocumentEnd,
        arguments[2].toBoolean() ? WebView::UserContentInjectInAllFrames : WebView::UserContentInjectInTopFrameOnly);
}

void TestRunner::addUserStyleSheet(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isString() || !arguments[1].isBool())
        return;
    WebView::addUserStyleSheet(
        cppVariantToWebString(arguments[0]), WebVector<WebString>(),
        arguments[1].toBoolean() ? WebView::UserContentInjectInAllFrames : WebView::UserContentInjectInTopFrameOnly,
        // Chromium defaults to InjectInSubsequentDocuments, but for compatibility
        // with the other ports' DRTs, we use UserStyleInjectInExistingDocuments.
        WebView::UserStyleInjectInExistingDocuments);
}

void TestRunner::startSpeechInput(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 1)
        return;

    WebElement element;
    if (!WebBindings::getElement(arguments[0].value.objectValue, &element))
        return;

    WebInputElement* input = toWebInputElement(&element);
    if (!input)
        return;

    if (!input->isSpeechInputEnabled())
        return;

    input->startSpeechInput();
}

void TestRunner::findString(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isString())
        return;

    WebFindOptions findOptions;
    bool wrapAround = false;
    if (arguments.size() >= 2) {
        vector<string> optionsArray = arguments[1].toStringVector();
        findOptions.matchCase = true;

        for (size_t i = 0; i < optionsArray.size(); ++i) {
            const std::string& option = optionsArray[i];
            // FIXME: Support all the options, so we can run findString.html too.
            if (option == "CaseInsensitive")
                findOptions.matchCase = false;
            else if (option == "Backwards")
                findOptions.forward = false;
            else if (option == "WrapAround")
                wrapAround = true;
        }
    }

    WebFrame* frame = m_webView->mainFrame();
    const bool findResult = frame->find(0, cppVariantToWebString(arguments[0]), findOptions, wrapAround, 0);
    result->set(findResult);
}

void TestRunner::setValueForUser(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2)
        return;

    WebElement element;
    if (!WebBindings::getElement(arguments[0].value.objectValue, &element))
        return;

    WebInputElement* input = toWebInputElement(&element);
    if (!input)
        return;

    input->setValue(cppVariantToWebString(arguments[1]), true);
}

void TestRunner::enableFixedLayoutMode(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  1 || !arguments[0].isBool())
        return;
    bool enableFixedLayout = arguments[0].toBoolean();
    m_webView->enableFixedLayoutMode(enableFixedLayout);
}

void TestRunner::setFixedLayoutSize(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;
    int width = arguments[0].toInt32();
    int height = arguments[1].toInt32();
    m_webView->setFixedLayoutSize(WebSize(width, height));
}

void TestRunner::selectionAsMarkup(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(m_webView->mainFrame()->selectionAsMarkup().utf8());
}

void TestRunner::setTextSubpixelPositioning(const CppArgumentList& arguments, CppVariant* result)
{
#if defined(__linux__) || defined(ANDROID)
    // Since FontConfig doesn't provide a variable to control subpixel positioning, we'll fall back
    // to setting it globally for all fonts.
    if (arguments.size() > 0 && arguments[0].isBool())
        WebFontRendering::setSubpixelPositioning(arguments[0].value.boolValue);
#endif
    result->setNull();
}

void TestRunner::resetPageVisibility(const CppArgumentList& arguments, CppVariant* result)
{
    m_webView->setVisibilityState(WebPageVisibilityStateVisible, true);
}

void TestRunner::setPageVisibility(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        string newVisibility = arguments[0].toString();
        if (newVisibility == "visible")
            m_webView->setVisibilityState(WebPageVisibilityStateVisible, false);
        else if (newVisibility == "hidden")
            m_webView->setVisibilityState(WebPageVisibilityStateHidden, false);
        else if (newVisibility == "prerender")
            m_webView->setVisibilityState(WebPageVisibilityStatePrerender, false);
        else if (newVisibility == "preview")
            m_webView->setVisibilityState(WebPageVisibilityStatePreview, false);
    }
}

void TestRunner::setTextDirection(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 1 || !arguments[0].isString())
        return;

    // Map a direction name to a WebTextDirection value.
    std::string directionName = arguments[0].toString();
    WebKit::WebTextDirection direction;
    if (directionName == "auto")
        direction = WebKit::WebTextDirectionDefault;
    else if (directionName == "rtl")
        direction = WebKit::WebTextDirectionRightToLeft;
    else if (directionName == "ltr")
        direction = WebKit::WebTextDirectionLeftToRight;
    else
        return;

    m_webView->setTextDirection(direction);
}

void TestRunner::textSurroundingNode(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 4 || !arguments[0].isObject() || !arguments[1].isNumber() || !arguments[2].isNumber() || !arguments[3].isNumber())
        return;

    WebNode node;
    if (!WebBindings::getNode(arguments[0].value.objectValue, &node))
        return;

    if (node.isNull() || !node.isTextNode())
        return;

    WebPoint point(arguments[1].toInt32(), arguments[2].toInt32());
    unsigned maxLength = arguments[3].toInt32();

    WebSurroundingText surroundingText;
    surroundingText.initialize(node, point, maxLength);
    if (surroundingText.isNull())
        return;

    result->set(surroundingText.textContent().utf8());
}

void TestRunner::setSmartInsertDeleteEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_smartInsertDeleteEnabled = arguments[0].value.boolValue;
    result->setNull();
}

void TestRunner::setSelectTrailingWhitespaceEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_selectTrailingWhitespaceEnabled = arguments[0].value.boolValue;
    result->setNull();
}

void TestRunner::dumpResourceRequestPriorities(const CppArgumentList& arguments, CppVariant* result)
{
    m_shouldDumpResourcePriorities = true;
    result->setNull();
}

void TestRunner::enableAutoResizeMode(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 4) {
        result->set(false);
        return;
    }
    int minWidth = cppVariantToInt32(arguments[0]);
    int minHeight = cppVariantToInt32(arguments[1]);
    WebKit::WebSize minSize(minWidth, minHeight);

    int maxWidth = cppVariantToInt32(arguments[2]);
    int maxHeight = cppVariantToInt32(arguments[3]);
    WebKit::WebSize maxSize(maxWidth, maxHeight);

    m_webView->enableAutoResizeMode(minSize, maxSize);
    result->set(true);
}

void TestRunner::disableAutoResizeMode(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() !=2) {
        result->set(false);
        return;
    }
    int newWidth = cppVariantToInt32(arguments[0]);
    int newHeight = cppVariantToInt32(arguments[1]);
    WebKit::WebSize newSize(newWidth, newHeight);

    m_delegate->setClientWindowRect(WebRect(0, 0, newSize.width, newSize.height));
    m_webView->disableAutoResizeMode();
    m_webView->resize(newSize);
    result->set(true);
}

void TestRunner::setMockDeviceOrientation(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 6 || !arguments[0].isBool() || !arguments[1].isNumber() || !arguments[2].isBool() || !arguments[3].isNumber() || !arguments[4].isBool() || !arguments[5].isNumber())
        return;

    WebDeviceOrientation orientation;
    orientation.setNull(false);
    if (arguments[0].toBoolean())
        orientation.setAlpha(arguments[1].toDouble());
    if (arguments[2].toBoolean())
        orientation.setBeta(arguments[3].toDouble());
    if (arguments[4].toBoolean())
        orientation.setGamma(arguments[5].toDouble());

    // Note that we only call setOrientation on the main page's mock since this
    // tests require. If necessary, we could get a list of WebViewHosts from th
    // call setOrientation on each DeviceOrientationClientMock.
    m_proxy->deviceOrientationClientMock()->setOrientation(orientation);
}

void TestRunner::setUserStyleSheetEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->userStyleSheetLocation = arguments[0].value.boolValue ? m_userStyleSheetLocation : WebURL();
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setUserStyleSheetLocation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        m_userStyleSheetLocation = m_delegate->localFileToDataURL(m_delegate->rewriteLayoutTestsURL(arguments[0].toString()));
        m_delegate->preferences()->userStyleSheetLocation = m_userStyleSheetLocation;
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setAuthorAndUserStylesEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->authorAndUserStylesEnabled = arguments[0].value.boolValue;
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setPopupBlockingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        bool blockPopups = arguments[0].toBoolean();
        m_delegate->preferences()->javaScriptCanOpenWindowsAutomatically = !blockPopups;
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setJavaScriptCanAccessClipboard(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->javaScriptCanAccessClipboard = arguments[0].value.boolValue;
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setXSSAuditorEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->XSSAuditorEnabled = arguments[0].value.boolValue;
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setAllowUniversalAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->allowUniversalAccessFromFileURLs = arguments[0].value.boolValue;
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setAllowFileAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->allowFileAccessFromFileURLs = arguments[0].value.boolValue;
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::overridePreference(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[0].isString())
        return;

    string key = arguments[0].toString();
    CppVariant value = arguments[1];
    WebPreferences* prefs = m_delegate->preferences();
    if (key == "WebKitDefaultFontSize")
        prefs->defaultFontSize = cppVariantToInt32(value);
    else if (key == "WebKitMinimumFontSize")
        prefs->minimumFontSize = cppVariantToInt32(value);
    else if (key == "WebKitDefaultTextEncodingName")
        prefs->defaultTextEncodingName = cppVariantToWebString(value);
    else if (key == "WebKitJavaScriptEnabled")
        prefs->javaScriptEnabled = cppVariantToBool(value);
    else if (key == "WebKitSupportsMultipleWindows")
        prefs->supportsMultipleWindows = cppVariantToBool(value);
    else if (key == "WebKitDisplayImagesKey")
        prefs->loadsImagesAutomatically = cppVariantToBool(value);
    else if (key == "WebKitPluginsEnabled")
        prefs->pluginsEnabled = cppVariantToBool(value);
    else if (key == "WebKitJavaEnabled")
        prefs->javaEnabled = cppVariantToBool(value);
    else if (key == "WebKitUsesPageCachePreferenceKey")
        prefs->usesPageCache = cppVariantToBool(value);
    else if (key == "WebKitPageCacheSupportsPluginsPreferenceKey")
        prefs->pageCacheSupportsPlugins = cppVariantToBool(value);
    else if (key == "WebKitOfflineWebApplicationCacheEnabled")
        prefs->offlineWebApplicationCacheEnabled = cppVariantToBool(value);
    else if (key == "WebKitTabToLinksPreferenceKey")
        prefs->tabsToLinks = cppVariantToBool(value);
    else if (key == "WebKitWebGLEnabled")
        prefs->experimentalWebGLEnabled = cppVariantToBool(value);
    else if (key == "WebKitCSSRegionsEnabled")
        prefs->experimentalCSSRegionsEnabled = cppVariantToBool(value);
    else if (key == "WebKitCSSGridLayoutEnabled")
        prefs->experimentalCSSGridLayoutEnabled = cppVariantToBool(value);
    else if (key == "WebKitHyperlinkAuditingEnabled")
        prefs->hyperlinkAuditingEnabled = cppVariantToBool(value);
    else if (key == "WebKitEnableCaretBrowsing")
        prefs->caretBrowsingEnabled = cppVariantToBool(value);
    else if (key == "WebKitAllowDisplayingInsecureContent")
        prefs->allowDisplayOfInsecureContent = cppVariantToBool(value);
    else if (key == "WebKitAllowRunningInsecureContent")
        prefs->allowRunningOfInsecureContent = cppVariantToBool(value);
    else if (key == "WebKitCSSCustomFilterEnabled")
        prefs->cssCustomFilterEnabled = cppVariantToBool(value);
    else if (key == "WebKitShouldRespectImageOrientation")
        prefs->shouldRespectImageOrientation = cppVariantToBool(value);
    else if (key == "WebKitWebAudioEnabled")
        WEBKIT_ASSERT(cppVariantToBool(value));
    else {
        string message("Invalid name for preference: ");
        message.append(key);
        printErrorMessage(message);
    }
    m_delegate->applyPreferences();
}

void TestRunner::setPluginsEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->pluginsEnabled = arguments[0].toBoolean();
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setAsynchronousSpellCheckingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->asynchronousSpellCheckingEnabled = cppVariantToBool(arguments[0]);
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::setTouchDragDropEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_delegate->preferences()->touchDragDropEnabled = arguments[0].toBoolean();
        m_delegate->applyPreferences();
    }
    result->setNull();
}

void TestRunner::showWebInspector(const CppArgumentList&, CppVariant* result)
{
    showDevTools();
    result->setNull();
}

void TestRunner::closeWebInspector(const CppArgumentList& args, CppVariant* result)
{
    m_delegate->closeDevTools();
    result->setNull();
}

void TestRunner::evaluateInWebInspector(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;
    m_delegate->evaluateInWebInspector(arguments[0].toInt32(), arguments[1].toString());
}

void TestRunner::clearAllDatabases(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    m_delegate->clearAllDatabases();
}

void TestRunner::setDatabaseQuota(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if ((arguments.size() >= 1) && arguments[0].isNumber())
        m_delegate->setDatabaseQuota(arguments[0].toInt32());
}

void TestRunner::setAlwaysAcceptCookies(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0)
        m_delegate->setAcceptAllCookies(cppVariantToBool(arguments[0]));
    result->setNull();
}

void TestRunner::setWindowIsKey(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_delegate->setFocus(arguments[0].value.boolValue);
    result->setNull();
}

void TestRunner::pathToLocalResource(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <= 0 || !arguments[0].isString())
        return;

    result->set(m_delegate->pathToLocalResource(arguments[0].toString()));
}

void TestRunner::setBackingScaleFactor(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isObject())
        return;

    float value = arguments[0].value.doubleValue;
    m_delegate->setDeviceScaleFactor(value);
    m_proxy->discardBackingStore();

    auto_ptr<CppVariant> callbackArguments(new CppVariant());
    callbackArguments->set(arguments[1]);
    result->setNull();
    m_delegate->postTask(new InvokeCallbackTask(this, callbackArguments));
}

void TestRunner::setPOSIXLocale(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() == 1 && arguments[0].isString())
        m_delegate->setLocale(arguments[0].toString());
}

void TestRunner::numberOfPendingGeolocationPermissionRequests(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(m_proxy->geolocationClientMock()->numberOfPendingPermissionRequests());
}

// FIXME: For greater test flexibility, we should be able to set each page's geolocation mock individually.
// https://bugs.webkit.org/show_bug.cgi?id=52368
void TestRunner::setGeolocationPermission(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;
    const vector<WebTestProxyBase*>& windowList = m_testInterfaces->windowList();
    for (unsigned i = 0; i < windowList.size(); ++i)
        windowList.at(i)->geolocationClientMock()->setPermission(arguments[0].toBoolean());
}

void TestRunner::setMockGeolocationPosition(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isNumber() || !arguments[1].isNumber() || !arguments[2].isNumber())
        return;
    const vector<WebTestProxyBase*>& windowList = m_testInterfaces->windowList();
    for (unsigned i = 0; i < windowList.size(); ++i)
        windowList.at(i)->geolocationClientMock()->setPosition(arguments[0].toDouble(), arguments[1].toDouble(), arguments[2].toDouble());
}

void TestRunner::setMockGeolocationPositionUnavailableError(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 1 || !arguments[0].isString())
        return;
    const vector<WebTestProxyBase*>& windowList = m_testInterfaces->windowList();
    for (unsigned i = 0; i < windowList.size(); ++i)
        windowList.at(i)->geolocationClientMock()->setPositionUnavailableError(WebString::fromUTF8(arguments[0].toString()));
}

#if ENABLE_NOTIFICATIONS
void TestRunner::grantWebNotificationPermission(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
    m_notificationPresenter->grantPermission(WebString::fromUTF8(arguments[0].toString()));
    result->set(true);
}

void TestRunner::simulateLegacyWebNotificationClick(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
    result->set(m_notificationPresenter->simulateClick(WebString::fromUTF8(arguments[0].toString())));
}
#endif

void TestRunner::addMockSpeechInputResult(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isString() || !arguments[1].isNumber() || !arguments[2].isString())
        return;

#if ENABLE_INPUT_SPEECH
    m_proxy->speechInputControllerMock()->addMockRecognitionResult(WebString::fromUTF8(arguments[0].toString()), arguments[1].toDouble(), WebString::fromUTF8(arguments[2].toString()));
#endif
}

void TestRunner::setMockSpeechInputDumpRect(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;

#if ENABLE_INPUT_SPEECH
    m_proxy->speechInputControllerMock()->setDumpRect(arguments[0].toBoolean());
#endif
}

void TestRunner::addMockSpeechRecognitionResult(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isString() || !arguments[1].isNumber())
        return;

    m_proxy->speechRecognizerMock()->addMockResult(WebString::fromUTF8(arguments[0].toString()), arguments[1].toDouble());
}

void TestRunner::setMockSpeechRecognitionError(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[0].isString() || !arguments[1].isString())
        return;

    m_proxy->speechRecognizerMock()->setError(WebString::fromUTF8(arguments[0].toString()), WebString::fromUTF8(arguments[1].toString()));
}

void TestRunner::wasMockSpeechRecognitionAborted(const CppArgumentList&, CppVariant* result)
{
    result->set(m_proxy->speechRecognizerMock()->wasAborted());
}

void TestRunner::display(const CppArgumentList& arguments, CppVariant* result)
{
    m_proxy->display();
    result->setNull();
}

void TestRunner::displayInvalidatedRegion(const CppArgumentList& arguments, CppVariant* result)
{
    m_proxy->displayInvalidatedRegion();
    result->setNull();
}

void TestRunner::dumpEditingCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpEditingCallbacks = true;
    result->setNull();
}

void TestRunner::dumpAsText(const CppArgumentList& arguments, CppVariant* result)
{
    m_dumpAsText = true;
    m_generatePixelResults = false;

    // Optional paramater, describing whether it's allowed to dump pixel results in dumpAsText mode.
    if (arguments.size() > 0 && arguments[0].isBool())
        m_generatePixelResults = arguments[0].value.boolValue;

    result->setNull();
}

void TestRunner::dumpChildFrameScrollPositions(const CppArgumentList&, CppVariant* result)
{
    m_dumpChildFrameScrollPositions = true;
    result->setNull();
}

void TestRunner::dumpChildFramesAsText(const CppArgumentList&, CppVariant* result)
{
    m_dumpChildFramesAsText = true;
    result->setNull();
}

void TestRunner::setAudioData(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() < 1 || !arguments[0].isObject())
        return;

    // Check that passed-in object is, in fact, an ArrayBufferView.
    NPObject* npobject = NPVARIANT_TO_OBJECT(arguments[0]);
    if (!npobject)
        return;
    if (!WebBindings::getArrayBufferView(npobject, &m_audioData))
        return;

    m_dumpAsAudio = true;
}

void TestRunner::dumpFrameLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpFrameLoadCallbacks = true;
    result->setNull();
}

void TestRunner::dumpUserGestureInFrameLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpUserGestureInFrameLoadCallbacks = true;
    result->setNull();
}

void TestRunner::setStopProvisionalFrameLoads(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
    m_stopProvisionalFrameLoads = true;
}

void TestRunner::dumpTitleChanges(const CppArgumentList&, CppVariant* result)
{
    m_dumpTitleChanges = true;
    result->setNull();
}

void TestRunner::dumpCreateView(const CppArgumentList&, CppVariant* result)
{
    m_dumpCreateView = true;
    result->setNull();
}

void TestRunner::setCanOpenWindows(const CppArgumentList&, CppVariant* result)
{
    m_canOpenWindows = true;
    result->setNull();
}

void TestRunner::dumpResourceLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceLoadCallbacks = true;
    result->setNull();
}

void TestRunner::dumpResourceRequestCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceRequestCallbacks = true;
    result->setNull();
}

void TestRunner::dumpResourceResponseMIMETypes(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceResponseMIMETypes = true;
    result->setNull();
}

// Need these conversions because the format of the value for booleans
// may vary - for example, on mac "1" and "0" are used for boolean.
bool TestRunner::cppVariantToBool(const CppVariant& value)
{
    if (value.isBool())
        return value.toBoolean();
    if (value.isNumber())
        return value.toInt32();
    if (value.isString()) {
        string valueString = value.toString();
        if (valueString == "true" || valueString == "1")
            return true;
        if (valueString == "false" || valueString == "0")
            return false;
    }
    printErrorMessage("Invalid value. Expected boolean value.");
    return false;
}

int32_t TestRunner::cppVariantToInt32(const CppVariant& value)
{
    if (value.isNumber())
        return value.toInt32();
    if (value.isString()) {
        string stringSource = value.toString();
        const char* source = stringSource.data();
        char* end;
        long number = strtol(source, &end, 10);
        if (end == source + stringSource.length() && number >= numeric_limits<int32_t>::min() && number <= numeric_limits<int32_t>::max())
            return static_cast<int32_t>(number);
    }
    printErrorMessage("Invalid value for preference. Expected integer value.");
    return 0;
}

WebString TestRunner::cppVariantToWebString(const CppVariant& value)
{
    if (!value.isString()) {
        printErrorMessage("Invalid value for preference. Expected string value.");
        return WebString();
    }
    return WebString::fromUTF8(value.toString());
}

void TestRunner::printErrorMessage(const string& text)
{
    m_delegate->printMessage(string("CONSOLE MESSAGE: ") + text + "\n");
}

void TestRunner::fallbackMethod(const CppArgumentList&, CppVariant* result)
{
    printErrorMessage("JavaScript ERROR: unknown method called on TestRunner");
    result->setNull();
}

void TestRunner::notImplemented(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void TestRunner::didAcquirePointerLock(const CppArgumentList&, CppVariant* result)
{
    didAcquirePointerLockInternal();
    result->setNull();
}

void TestRunner::didNotAcquirePointerLock(const CppArgumentList&, CppVariant* result)
{
    didNotAcquirePointerLockInternal();
    result->setNull();
}

void TestRunner::didLosePointerLock(const CppArgumentList&, CppVariant* result)
{
    didLosePointerLockInternal();
    result->setNull();
}

void TestRunner::setPointerLockWillRespondAsynchronously(const CppArgumentList&, CppVariant* result)
{
    m_pointerLockPlannedResult = PointerLockWillRespondAsync;
    result->setNull();
}

void TestRunner::setPointerLockWillFailSynchronously(const CppArgumentList&, CppVariant* result)
{
    m_pointerLockPlannedResult = PointerLockWillFailSync;
    result->setNull();
}

}
