/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Pawel Hajdan (phajdan.jr@chromium.org)
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
#include "LayoutTestController.h"

#include "DRTDevToolsAgent.h"
#include "MockWebSpeechInputController.h"
#include "TestShell.h"
#include "WebAnimationController.h"
#include "WebBindings.h"
#include "WebWorkerInfo.h"
#include "WebConsoleMessage.h"
#include "platform/WebData.h"
#include "WebDeviceOrientation.h"
#include "WebDeviceOrientationClientMock.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFindOptions.h"
#include "WebFrame.h"
#include "WebGeolocationClientMock.h"
#include "WebIDBFactory.h"
#include "WebInputElement.h"
#include "WebIntentRequest.h"
#include "WebKit.h"
#include "WebNotificationPresenter.h"
#include "WebPermissions.h"
#include "WebScriptSource.h"
#include "WebSecurityPolicy.h"
#include "platform/WebSerializedScriptValue.h"
#include "WebSettings.h"
#include "platform/WebSize.h"
#include "platform/WebURL.h"
#include "WebView.h"
#include "WebViewHost.h"
#include "v8/include/v8.h"
#include "webkit/support/webkit_support.h"
#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <wtf/OwnArrayPtr.h>
#endif

using namespace WebCore;
using namespace WebKit;
using namespace std;

LayoutTestController::LayoutTestController(TestShell* shell)
    : m_shell(shell)
    , m_closeRemainingWindows(false)
    , m_deferMainResourceDataLoad(false)
    , m_showDebugLayerTree(false)
    , m_workQueue(this)
    , m_shouldStayOnPageAfterHandlingBeforeUnload(false)
{

    // Initialize the map that associates methods of this class with the names
    // they will use when called by JavaScript. The actual binding of those
    // names to their methods will be done by calling bindToJavaScript() (defined
    // by CppBoundClass, the parent to LayoutTestController).
    bindMethod("addFileToPasteboardOnDrag", &LayoutTestController::addFileToPasteboardOnDrag);
#if ENABLE(INPUT_SPEECH)
    bindMethod("addMockSpeechInputResult", &LayoutTestController::addMockSpeechInputResult);
    bindMethod("setMockSpeechInputDumpRect", &LayoutTestController::setMockSpeechInputDumpRect);
#endif
    bindMethod("addOriginAccessWhitelistEntry", &LayoutTestController::addOriginAccessWhitelistEntry);
    bindMethod("addUserScript", &LayoutTestController::addUserScript);
    bindMethod("addUserStyleSheet", &LayoutTestController::addUserStyleSheet);
    bindMethod("clearAllDatabases", &LayoutTestController::clearAllDatabases);
    bindMethod("closeWebInspector", &LayoutTestController::closeWebInspector);
    bindMethod("counterValueForElementById", &LayoutTestController::counterValueForElementById);
#if ENABLE(POINTER_LOCK)
    bindMethod("didLosePointerLock", &LayoutTestController::didLosePointerLock);
#endif
    bindMethod("disableAutoResizeMode", &LayoutTestController::disableAutoResizeMode);
    bindMethod("disableImageLoading", &LayoutTestController::disableImageLoading);
    bindMethod("display", &LayoutTestController::display);
    bindMethod("displayInvalidatedRegion", &LayoutTestController::displayInvalidatedRegion);
    bindMethod("dumpAsText", &LayoutTestController::dumpAsText);
    bindMethod("dumpBackForwardList", &LayoutTestController::dumpBackForwardList);
    bindMethod("dumpChildFramesAsText", &LayoutTestController::dumpChildFramesAsText);
    bindMethod("dumpChildFrameScrollPositions", &LayoutTestController::dumpChildFrameScrollPositions);
    bindMethod("dumpDatabaseCallbacks", &LayoutTestController::dumpDatabaseCallbacks);
    bindMethod("dumpEditingCallbacks", &LayoutTestController::dumpEditingCallbacks);
    bindMethod("dumpFrameLoadCallbacks", &LayoutTestController::dumpFrameLoadCallbacks);
    bindMethod("dumpProgressFinishedCallback", &LayoutTestController::dumpProgressFinishedCallback);
    bindMethod("dumpUserGestureInFrameLoadCallbacks", &LayoutTestController::dumpUserGestureInFrameLoadCallbacks);
    bindMethod("dumpResourceLoadCallbacks", &LayoutTestController::dumpResourceLoadCallbacks);
    bindMethod("dumpResourceResponseMIMETypes", &LayoutTestController::dumpResourceResponseMIMETypes);
    bindMethod("dumpSelectionRect", &LayoutTestController::dumpSelectionRect);
    bindMethod("dumpStatusCallbacks", &LayoutTestController::dumpWindowStatusChanges);
    bindMethod("dumpTitleChanges", &LayoutTestController::dumpTitleChanges);
    bindMethod("dumpPermissionClientCallbacks", &LayoutTestController::dumpPermissionClientCallbacks);
    bindMethod("dumpCreateView", &LayoutTestController::dumpCreateView);
    bindMethod("elementDoesAutoCompleteForElementWithId", &LayoutTestController::elementDoesAutoCompleteForElementWithId);
    bindMethod("enableAutoResizeMode", &LayoutTestController::enableAutoResizeMode);
    bindMethod("evaluateInWebInspector", &LayoutTestController::evaluateInWebInspector);
    bindMethod("evaluateScriptInIsolatedWorld", &LayoutTestController::evaluateScriptInIsolatedWorld);
    bindMethod("evaluateScriptInIsolatedWorldAndReturnValue", &LayoutTestController::evaluateScriptInIsolatedWorldAndReturnValue);
    bindMethod("setIsolatedWorldSecurityOrigin", &LayoutTestController::setIsolatedWorldSecurityOrigin);
    bindMethod("execCommand", &LayoutTestController::execCommand);
    bindMethod("forceRedSelectionColors", &LayoutTestController::forceRedSelectionColors);
#if ENABLE(NOTIFICATIONS)
    bindMethod("grantDesktopNotificationPermission", &LayoutTestController::grantDesktopNotificationPermission);
#endif
    bindMethod("findString", &LayoutTestController::findString);
    bindMethod("isCommandEnabled", &LayoutTestController::isCommandEnabled);
    bindMethod("hasCustomPageSizeStyle", &LayoutTestController::hasCustomPageSizeStyle);
    bindMethod("layerTreeAsText", &LayoutTestController::layerTreeAsText);
    bindMethod("loseCompositorContext", &LayoutTestController::loseCompositorContext);
    bindMethod("markerTextForListItem", &LayoutTestController::markerTextForListItem);
    bindMethod("notifyDone", &LayoutTestController::notifyDone);
    bindMethod("numberOfActiveAnimations", &LayoutTestController::numberOfActiveAnimations);
    bindMethod("numberOfPages", &LayoutTestController::numberOfPages);
    bindMethod("numberOfPendingGeolocationPermissionRequests", &LayoutTestController:: numberOfPendingGeolocationPermissionRequests);
    bindMethod("objCIdentityIsEqual", &LayoutTestController::objCIdentityIsEqual);
    bindMethod("overridePreference", &LayoutTestController::overridePreference);
    bindMethod("pageNumberForElementById", &LayoutTestController::pageNumberForElementById);
    bindMethod("pageProperty", &LayoutTestController::pageProperty);
    bindMethod("pageSizeAndMarginsInPixels", &LayoutTestController::pageSizeAndMarginsInPixels);
    bindMethod("pathToLocalResource", &LayoutTestController::pathToLocalResource);
    bindMethod("pauseAnimationAtTimeOnElementWithId", &LayoutTestController::pauseAnimationAtTimeOnElementWithId);
    bindMethod("pauseTransitionAtTimeOnElementWithId", &LayoutTestController::pauseTransitionAtTimeOnElementWithId);
    bindMethod("queueBackNavigation", &LayoutTestController::queueBackNavigation);
    bindMethod("queueForwardNavigation", &LayoutTestController::queueForwardNavigation);
    bindMethod("queueLoadingScript", &LayoutTestController::queueLoadingScript);
    bindMethod("queueLoad", &LayoutTestController::queueLoad);
    bindMethod("queueLoadHTMLString", &LayoutTestController::queueLoadHTMLString);
    bindMethod("queueNonLoadingScript", &LayoutTestController::queueNonLoadingScript);
    bindMethod("queueReload", &LayoutTestController::queueReload);
    bindMethod("removeOriginAccessWhitelistEntry", &LayoutTestController::removeOriginAccessWhitelistEntry);
    bindMethod("repaintSweepHorizontally", &LayoutTestController::repaintSweepHorizontally);
    bindMethod("resetPageVisibility", &LayoutTestController::resetPageVisibility);
    bindMethod("resumeAnimations", &LayoutTestController::resumeAnimations);
    bindMethod("setAcceptsEditing", &LayoutTestController::setAcceptsEditing);
    bindMethod("setAllowDisplayOfInsecureContent", &LayoutTestController::setAllowDisplayOfInsecureContent);
    bindMethod("setAllowFileAccessFromFileURLs", &LayoutTestController::setAllowFileAccessFromFileURLs);
    bindMethod("setAllowRunningOfInsecureContent", &LayoutTestController::setAllowRunningOfInsecureContent);
    bindMethod("setAllowUniversalAccessFromFileURLs", &LayoutTestController::setAllowUniversalAccessFromFileURLs);
    bindMethod("setAlwaysAcceptCookies", &LayoutTestController::setAlwaysAcceptCookies);
    bindMethod("setAuthorAndUserStylesEnabled", &LayoutTestController::setAuthorAndUserStylesEnabled);
    bindMethod("setAutofilled", &LayoutTestController::setAutofilled);
    bindMethod("setCanOpenWindows", &LayoutTestController::setCanOpenWindows);
    bindMethod("setCloseRemainingWindowsWhenComplete", &LayoutTestController::setCloseRemainingWindowsWhenComplete);
    bindMethod("setCustomPolicyDelegate", &LayoutTestController::setCustomPolicyDelegate);
    bindMethod("setDatabaseQuota", &LayoutTestController::setDatabaseQuota);
    bindMethod("setDeferMainResourceDataLoad", &LayoutTestController::setDeferMainResourceDataLoad);
    bindMethod("setDomainRelaxationForbiddenForURLScheme", &LayoutTestController::setDomainRelaxationForbiddenForURLScheme);
    bindMethod("setEditingBehavior", &LayoutTestController::setEditingBehavior);
    bindMethod("setAudioData", &LayoutTestController::setAudioData);
    bindMethod("setGeolocationPermission", &LayoutTestController::setGeolocationPermission);
    bindMethod("setIconDatabaseEnabled", &LayoutTestController::setIconDatabaseEnabled);
    bindMethod("setJavaScriptCanAccessClipboard", &LayoutTestController::setJavaScriptCanAccessClipboard);
    bindMethod("setJavaScriptProfilingEnabled", &LayoutTestController::setJavaScriptProfilingEnabled);
    bindMethod("setMinimumTimerInterval", &LayoutTestController::setMinimumTimerInterval);
    bindMethod("setMockDeviceOrientation", &LayoutTestController::setMockDeviceOrientation);
    bindMethod("setMockGeolocationError", &LayoutTestController::setMockGeolocationError);
    bindMethod("setMockGeolocationPosition", &LayoutTestController::setMockGeolocationPosition);
    bindMethod("setPageVisibility", &LayoutTestController::setPageVisibility);
    bindMethod("setPluginsEnabled", &LayoutTestController::setPluginsEnabled);
#if ENABLE(POINTER_LOCK)
    bindMethod("setPointerLockWillFailAsynchronously", &LayoutTestController::setPointerLockWillFailAsynchronously);
    bindMethod("setPointerLockWillFailSynchronously", &LayoutTestController::setPointerLockWillFailSynchronously);
#endif
    bindMethod("setPopupBlockingEnabled", &LayoutTestController::setPopupBlockingEnabled);
    bindMethod("setPOSIXLocale", &LayoutTestController::setPOSIXLocale);
    bindMethod("setPrinting", &LayoutTestController::setPrinting);
    bindMethod("setScrollbarPolicy", &LayoutTestController::setScrollbarPolicy);
    bindMethod("setSelectTrailingWhitespaceEnabled", &LayoutTestController::setSelectTrailingWhitespaceEnabled);
    bindMethod("setSmartInsertDeleteEnabled", &LayoutTestController::setSmartInsertDeleteEnabled);
    bindMethod("setStopProvisionalFrameLoads", &LayoutTestController::setStopProvisionalFrameLoads);
    bindMethod("setTabKeyCyclesThroughElements", &LayoutTestController::setTabKeyCyclesThroughElements);
    bindMethod("setUserStyleSheetEnabled", &LayoutTestController::setUserStyleSheetEnabled);
    bindMethod("setUserStyleSheetLocation", &LayoutTestController::setUserStyleSheetLocation);
    bindMethod("setValueForUser", &LayoutTestController::setValueForUser);
    bindMethod("setWillSendRequestClearHeader", &LayoutTestController::setWillSendRequestClearHeader);
    bindMethod("setWillSendRequestReturnsNull", &LayoutTestController::setWillSendRequestReturnsNull);
    bindMethod("setWillSendRequestReturnsNullOnRedirect", &LayoutTestController::setWillSendRequestReturnsNullOnRedirect);
    bindMethod("setWindowIsKey", &LayoutTestController::setWindowIsKey);
    bindMethod("setXSSAuditorEnabled", &LayoutTestController::setXSSAuditorEnabled);
    bindMethod("setAsynchronousSpellCheckingEnabled", &LayoutTestController::setAsynchronousSpellCheckingEnabled);
    bindMethod("showWebInspector", &LayoutTestController::showWebInspector);
#if ENABLE(NOTIFICATIONS)
    bindMethod("simulateDesktopNotificationClick", &LayoutTestController::simulateDesktopNotificationClick);
#endif
    bindMethod("startSpeechInput", &LayoutTestController::startSpeechInput);
    bindMethod("testRepaint", &LayoutTestController::testRepaint);
    bindMethod("waitForPolicyDelegate", &LayoutTestController::waitForPolicyDelegate);
    bindMethod("waitUntilDone", &LayoutTestController::waitUntilDone);
    bindMethod("windowCount", &LayoutTestController::windowCount);
    bindMethod("setTextDirection", &LayoutTestController::setTextDirection);
    bindMethod("setImagesAllowed", &LayoutTestController::setImagesAllowed);
    bindMethod("setScriptsAllowed", &LayoutTestController::setScriptsAllowed);
    bindMethod("setStorageAllowed", &LayoutTestController::setStorageAllowed);
    bindMethod("setPluginsAllowed", &LayoutTestController::setPluginsAllowed);

    // The following are stubs.
    bindMethod("abortModal", &LayoutTestController::abortModal);
    bindMethod("accessStoredWebScriptObject", &LayoutTestController::accessStoredWebScriptObject);
    bindMethod("addDisallowedURL", &LayoutTestController::addDisallowedURL);
    bindMethod("applicationCacheDiskUsageForOrigin", &LayoutTestController::applicationCacheDiskUsageForOrigin);
    bindMethod("callShouldCloseOnWebView", &LayoutTestController::callShouldCloseOnWebView);
    bindMethod("clearAllApplicationCaches", &LayoutTestController::clearAllApplicationCaches);
    bindMethod("clearApplicationCacheForOrigin", &LayoutTestController::clearApplicationCacheForOrigin);
    bindMethod("clearBackForwardList", &LayoutTestController::clearBackForwardList);
    bindMethod("dumpAsWebArchive", &LayoutTestController::dumpAsWebArchive);
    bindMethod("keepWebHistory", &LayoutTestController::keepWebHistory);
    bindMethod("objCClassNameOf", &LayoutTestController::objCClassNameOf);
    bindMethod("setApplicationCacheOriginQuota", &LayoutTestController::setApplicationCacheOriginQuota);
    bindMethod("setCallCloseOnWebViews", &LayoutTestController::setCallCloseOnWebViews);
    bindMethod("setMainFrameIsFirstResponder", &LayoutTestController::setMainFrameIsFirstResponder);
    bindMethod("setPrivateBrowsingEnabled", &LayoutTestController::setPrivateBrowsingEnabled);
    bindMethod("setUseDashboardCompatibilityMode", &LayoutTestController::setUseDashboardCompatibilityMode);
    bindMethod("storeWebScriptObject", &LayoutTestController::storeWebScriptObject);
    bindMethod("deleteAllLocalStorage", &LayoutTestController::deleteAllLocalStorage);
    bindMethod("localStorageDiskUsageForOrigin", &LayoutTestController::localStorageDiskUsageForOrigin);
    bindMethod("originsWithLocalStorage", &LayoutTestController::originsWithLocalStorage);
    bindMethod("deleteLocalStorageForOrigin", &LayoutTestController::deleteLocalStorageForOrigin);
    bindMethod("observeStorageTrackerNotifications", &LayoutTestController::observeStorageTrackerNotifications);
    bindMethod("syncLocalStorage", &LayoutTestController::syncLocalStorage);
    bindMethod("setShouldStayOnPageAfterHandlingBeforeUnload", &LayoutTestController::setShouldStayOnPageAfterHandlingBeforeUnload);
    bindMethod("enableFixedLayoutMode", &LayoutTestController::enableFixedLayoutMode);
    bindMethod("setFixedLayoutSize", &LayoutTestController::setFixedLayoutSize);
    bindMethod("selectionAsMarkup", &LayoutTestController::selectionAsMarkup);
    bindMethod("setHasCustomFullScreenBehavior", &LayoutTestController::setHasCustomFullScreenBehavior);
    
    // The fallback method is called when an unknown method is invoked.
    bindFallbackMethod(&LayoutTestController::fallbackMethod);

    // Shared properties.
    // globalFlag is used by a number of layout tests in
    // LayoutTests\http\tests\security\dataURL.
    bindProperty("globalFlag", &m_globalFlag);
    // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
    bindProperty("webHistoryItemCount", &m_webHistoryItemCount);
    bindProperty("titleTextDirection", &m_titleTextDirection);
    bindProperty("platformName", &m_platformName);
    bindProperty("interceptPostMessage", &m_interceptPostMessage);
    bindProperty("workerThreadCount", &LayoutTestController::workerThreadCount);
    bindMethod("sendWebIntentResponse", &LayoutTestController::sendWebIntentResponse);
}

LayoutTestController::~LayoutTestController()
{
}

LayoutTestController::WorkQueue::~WorkQueue()
{
    reset();
}

void LayoutTestController::WorkQueue::processWorkSoon()
{
    if (m_controller->m_shell->webViewHost()->topLoadingFrame())
        return;

    if (!m_queue.isEmpty()) {
        // We delay processing queued work to avoid recursion problems.
        postTask(new WorkQueueTask(this));
    } else if (!m_controller->m_waitUntilDone)
        m_controller->m_shell->testFinished();
}

void LayoutTestController::WorkQueue::processWork()
{
    TestShell* shell = m_controller->m_shell;
    // Quit doing work once a load is in progress.
    while (!m_queue.isEmpty()) {
        bool startedLoad = m_queue.first()->run(shell);
        delete m_queue.takeFirst();
        if (startedLoad)
            return;
    }

    if (!m_controller->m_waitUntilDone && !shell->webViewHost()->topLoadingFrame())
        shell->testFinished();
}

void LayoutTestController::WorkQueue::reset()
{
    m_frozen = false;
    while (!m_queue.isEmpty())
        delete m_queue.takeFirst();
}

void LayoutTestController::WorkQueue::addWork(WorkItem* work)
{
    if (m_frozen) {
        delete work;
        return;
    }
    m_queue.append(work);
}

void LayoutTestController::dumpAsText(const CppArgumentList& arguments, CppVariant* result)
{
    m_dumpAsText = true;
    m_generatePixelResults = false;

    // Optional paramater, describing whether it's allowed to dump pixel results in dumpAsText mode.
    if (arguments.size() > 0 && arguments[0].isBool())
        m_generatePixelResults = arguments[0].value.boolValue;

    result->setNull();
}

void LayoutTestController::dumpDatabaseCallbacks(const CppArgumentList&, CppVariant* result)
{
    // Do nothing; we don't use this flag anywhere for now
    result->setNull();
}

void LayoutTestController::dumpEditingCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpEditingCallbacks = true;
    result->setNull();
}

void LayoutTestController::dumpBackForwardList(const CppArgumentList&, CppVariant* result)
{
    m_dumpBackForwardList = true;
    result->setNull();
}

void LayoutTestController::dumpFrameLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpFrameLoadCallbacks = true;
    result->setNull();
}

void LayoutTestController::dumpProgressFinishedCallback(const CppArgumentList&, CppVariant* result)
{
    m_dumpProgressFinishedCallback = true;
    result->setNull();
}

void LayoutTestController::dumpUserGestureInFrameLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpUserGestureInFrameLoadCallbacks = true;
    result->setNull();
}

void LayoutTestController::dumpResourceLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceLoadCallbacks = true;
    result->setNull();
}

void LayoutTestController::dumpResourceResponseMIMETypes(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceResponseMIMETypes = true;
    result->setNull();
}

void LayoutTestController::dumpChildFrameScrollPositions(const CppArgumentList&, CppVariant* result)
{
    m_dumpChildFrameScrollPositions = true;
    result->setNull();
}

void LayoutTestController::dumpChildFramesAsText(const CppArgumentList&, CppVariant* result)
{
    m_dumpChildFramesAsText = true;
    result->setNull();
}

void LayoutTestController::dumpWindowStatusChanges(const CppArgumentList&, CppVariant* result)
{
    m_dumpWindowStatusChanges = true;
    result->setNull();
}

void LayoutTestController::dumpTitleChanges(const CppArgumentList&, CppVariant* result)
{
    m_dumpTitleChanges = true;
    result->setNull();
}

void LayoutTestController::dumpPermissionClientCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpPermissionClientCallbacks = true;
    result->setNull();
}

void LayoutTestController::dumpCreateView(const CppArgumentList&, CppVariant* result)
{
    m_dumpCreateView = true;
    result->setNull();
}

void LayoutTestController::setAcceptsEditing(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_acceptsEditing = arguments[0].value.boolValue;
    result->setNull();
}

void LayoutTestController::waitUntilDone(const CppArgumentList&, CppVariant* result)
{
    if (!webkit_support::BeingDebugged())
        postDelayedTask(new NotifyDoneTimedOutTask(this), m_shell->layoutTestTimeout());
    m_waitUntilDone = true;
    result->setNull();
}

void LayoutTestController::notifyDone(const CppArgumentList&, CppVariant* result)
{
    // Test didn't timeout. Kill the timeout timer.
    m_taskList.revokeAll();

    completeNotifyDone(false);
    result->setNull();
}

void LayoutTestController::completeNotifyDone(bool isTimeout)
{
    if (m_waitUntilDone && !m_shell->webViewHost()->topLoadingFrame() && m_workQueue.isEmpty()) {
        if (isTimeout)
            m_shell->testTimedOut();
        else
            m_shell->testFinished();
    }
    m_waitUntilDone = false;
}

class WorkItemBackForward : public LayoutTestController::WorkItem {
public:
    WorkItemBackForward(int distance) : m_distance(distance) { }
    bool run(TestShell* shell)
    {
        shell->goToOffset(m_distance);
        return true; // FIXME: Did it really start a navigation?
    }

private:
    int m_distance;
};

void LayoutTestController::queueBackNavigation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isNumber())
        m_workQueue.addWork(new WorkItemBackForward(-arguments[0].toInt32()));
    result->setNull();
}

void LayoutTestController::queueForwardNavigation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isNumber())
        m_workQueue.addWork(new WorkItemBackForward(arguments[0].toInt32()));
    result->setNull();
}

class WorkItemReload : public LayoutTestController::WorkItem {
public:
    bool run(TestShell* shell)
    {
        shell->reload();
        return true;
    }
};

void LayoutTestController::queueReload(const CppArgumentList&, CppVariant* result)
{
    m_workQueue.addWork(new WorkItemReload);
    result->setNull();
}

class WorkItemLoadingScript : public LayoutTestController::WorkItem {
public:
    WorkItemLoadingScript(const string& script) : m_script(script) { }
    bool run(TestShell* shell)
    {
        shell->webView()->mainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(m_script)));
        return true; // FIXME: Did it really start a navigation?
    }

private:
    string m_script;
};

class WorkItemNonLoadingScript : public LayoutTestController::WorkItem {
public:
    WorkItemNonLoadingScript(const string& script) : m_script(script) { }
    bool run(TestShell* shell)
    {
        shell->webView()->mainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(m_script)));
        return false;
    }

private:
    string m_script;
};

void LayoutTestController::queueLoadingScript(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString())
        m_workQueue.addWork(new WorkItemLoadingScript(arguments[0].toString()));
    result->setNull();
}

void LayoutTestController::queueNonLoadingScript(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString())
        m_workQueue.addWork(new WorkItemNonLoadingScript(arguments[0].toString()));
    result->setNull();
}

class WorkItemLoad : public LayoutTestController::WorkItem {
public:
    WorkItemLoad(const WebURL& url, const WebString& target)
        : m_url(url)
        , m_target(target) { }
    bool run(TestShell* shell)
    {
        shell->webViewHost()->loadURLForFrame(m_url, m_target);
        return true; // FIXME: Did it really start a navigation?
    }

private:
    WebURL m_url;
    WebString m_target;
};

void LayoutTestController::queueLoad(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        // FIXME: Implement WebURL::resolve() and avoid GURL.
        GURL currentURL = m_shell->webView()->mainFrame()->document().url();
        GURL fullURL = currentURL.Resolve(arguments[0].toString());

        string target = "";
        if (arguments.size() > 1 && arguments[1].isString())
            target = arguments[1].toString();

        m_workQueue.addWork(new WorkItemLoad(fullURL, WebString::fromUTF8(target)));
    }
    result->setNull();
}

class WorkItemLoadHTMLString : public LayoutTestController::WorkItem  {
public:
    WorkItemLoadHTMLString(const std::string& html, const WebURL& baseURL)
        : m_html(html)
        , m_baseURL(baseURL) { }
    WorkItemLoadHTMLString(const std::string& html, const WebURL& baseURL, const WebURL& unreachableURL)
        : m_html(html)
        , m_baseURL(baseURL)
        , m_unreachableURL(unreachableURL) { }
    bool run(TestShell* shell)
    {
        shell->webView()->mainFrame()->loadHTMLString(
            WebKit::WebData(m_html.data(), m_html.length()), m_baseURL, m_unreachableURL);
        return true;
    }

private:
    std::string m_html;
    WebURL m_baseURL;
    WebURL m_unreachableURL;
};

void LayoutTestController::queueLoadHTMLString(const CppArgumentList& arguments, CppVariant* result)
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

void LayoutTestController::objCIdentityIsEqual(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 2) {
        // This is the best we can do to return an error.
        result->setNull();
        return;
    }
    result->set(arguments[0].isEqual(arguments[1]));
}

void LayoutTestController::reset()
{
    if (m_shell) {
        m_shell->webView()->setZoomLevel(false, 0);
        m_shell->webView()->setTabKeyCyclesThroughElements(true);
#if !OS(DARWIN) && !OS(WINDOWS) // Actually, TOOLKIT_GTK
        // (Constants copied because we can't depend on the header that defined
        // them from this file.)
        m_shell->webView()->setSelectionColors(0xff1e90ff, 0xff000000, 0xffc8c8c8, 0xff323232);
#endif
        m_shell->webView()->removeAllUserContent();
        WebKit::WebSize empty;
        m_shell->webView()->disableAutoResizeMode();
    }
    m_dumpAsText = false;
    m_dumpAsAudio = false;
    m_dumpCreateView = false;
    m_dumpEditingCallbacks = false;
    m_dumpFrameLoadCallbacks = false;
    m_dumpProgressFinishedCallback = false;
    m_dumpUserGestureInFrameLoadCallbacks = false;
    m_dumpResourceLoadCallbacks = false;
    m_dumpResourceResponseMIMETypes = false;
    m_dumpBackForwardList = false;
    m_dumpChildFrameScrollPositions = false;
    m_dumpChildFramesAsText = false;
    m_dumpWindowStatusChanges = false;
    m_dumpSelectionRect = false;
    m_dumpTitleChanges = false;
    m_dumpPermissionClientCallbacks = false;
    m_generatePixelResults = true;
    m_acceptsEditing = true;
    m_waitUntilDone = false;
    m_canOpenWindows = false;
    m_testRepaint = false;
    m_sweepHorizontally = false;
    m_shouldAddFileToPasteboard = false;
    m_stopProvisionalFrameLoads = false;
    m_deferMainResourceDataLoad = true;
    m_globalFlag.set(false);
    m_webHistoryItemCount.set(0);
    m_titleTextDirection.set("ltr");
    m_platformName.set("chromium");
    m_interceptPostMessage.set(false);
    m_userStyleSheetLocation = WebURL();
    m_isPrinting = false;

    webkit_support::SetAcceptAllCookies(false);
    WebSecurityPolicy::resetOriginAccessWhitelists();

    // Reset the default quota for each origin to 5MB
    webkit_support::SetDatabaseQuota(5 * 1024 * 1024);

    setlocale(LC_ALL, "");

    if (m_closeRemainingWindows)
        m_shell->closeRemainingWindows();
    else
        m_closeRemainingWindows = true;
    m_workQueue.reset();
    m_taskList.revokeAll();
    m_shouldStayOnPageAfterHandlingBeforeUnload = false;
    m_hasCustomFullScreenBehavior = false;
}

void LayoutTestController::locationChangeDone()
{
    m_webHistoryItemCount.set(m_shell->navigationEntryCount());

    // No more new work after the first complete load.
    m_workQueue.setFrozen(true);

    if (!m_waitUntilDone)
        m_workQueue.processWorkSoon();
}

void LayoutTestController::policyDelegateDone()
{
    ASSERT(m_waitUntilDone);
    m_shell->testFinished();
    m_waitUntilDone = false;
}

void LayoutTestController::setCanOpenWindows(const CppArgumentList&, CppVariant* result)
{
    m_canOpenWindows = true;
    result->setNull();
}

void LayoutTestController::setTabKeyCyclesThroughElements(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->setTabKeyCyclesThroughElements(arguments[0].toBoolean());
    result->setNull();
}

void LayoutTestController::windowCount(const CppArgumentList&, CppVariant* result)
{
    result->set(static_cast<int>(m_shell->windowCount()));
}

void LayoutTestController::setCloseRemainingWindowsWhenComplete(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_closeRemainingWindows = arguments[0].value.boolValue;
    result->setNull();
}

void LayoutTestController::setAlwaysAcceptCookies(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0)
        webkit_support::SetAcceptAllCookies(cppVariantToBool(arguments[0]));
    result->setNull();
}

void LayoutTestController::setAsynchronousSpellCheckingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setAsynchronousSpellCheckingEnabled(cppVariantToBool(arguments[0]));
    result->setNull();
}

void LayoutTestController::showWebInspector(const CppArgumentList&, CppVariant* result)
{
    m_shell->showDevTools();
    result->setNull();
}

void LayoutTestController::closeWebInspector(const CppArgumentList& args, CppVariant* result)
{
    m_shell->closeDevTools();
    result->setNull();
}

void LayoutTestController::setWindowIsKey(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->setFocus(m_shell->webView(), arguments[0].value.boolValue);
    result->setNull();
}

void LayoutTestController::setUserStyleSheetEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->userStyleSheetLocation = arguments[0].value.boolValue ? m_userStyleSheetLocation : WebURL();
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::setUserStyleSheetLocation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        m_userStyleSheetLocation = webkit_support::LocalFileToDataURL(
            webkit_support::RewriteLayoutTestsURL(arguments[0].toString()));
        m_shell->preferences()->userStyleSheetLocation = m_userStyleSheetLocation;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::setAuthorAndUserStylesEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->authorAndUserStylesEnabled = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::execCommand(const CppArgumentList& arguments, CppVariant* result)
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
    m_shell->webView()->focusedFrame()->executeCommand(WebString::fromUTF8(command), WebString::fromUTF8(value));
}

void LayoutTestController::isCommandEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() <= 0 || !arguments[0].isString()) {
        result->setNull();
        return;
    }

    std::string command = arguments[0].toString();
    bool rv = m_shell->webView()->focusedFrame()->isCommandEnabled(WebString::fromUTF8(command));
    result->set(rv);
}

void LayoutTestController::setPopupBlockingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        bool blockPopups = arguments[0].toBoolean();
        m_shell->preferences()->javaScriptCanOpenWindowsAutomatically = !blockPopups;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::setImagesAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setImagesAllowed(arguments[0].toBoolean());
    result->setNull();
}

void LayoutTestController::setScriptsAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setScriptsAllowed(arguments[0].toBoolean());
    result->setNull();
}

void LayoutTestController::setStorageAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setStorageAllowed(arguments[0].toBoolean());
    result->setNull();
}

void LayoutTestController::setPluginsAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setPluginsAllowed(arguments[0].toBoolean());
    result->setNull();
}

void LayoutTestController::setUseDashboardCompatibilityMode(const CppArgumentList&, CppVariant* result)
{
    // We have no need to support Dashboard Compatibility Mode (mac-only)
    result->setNull();
}

void LayoutTestController::clearAllApplicationCaches(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support application cache quotas.
    result->setNull();
}

void LayoutTestController::clearApplicationCacheForOrigin(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support deleting all application cache for an origin.
    result->setNull();
}

void LayoutTestController::setApplicationCacheOriginQuota(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support application cache quotas.
    result->setNull();
}

void LayoutTestController::originsWithApplicationCache(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support getting origins that have application caches.
    result->setNull();
}

void LayoutTestController::applicationCacheDiskUsageForOrigin(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support getting disk usage by all application cache for an origin.
    result->setNull();
}

void LayoutTestController::setScrollbarPolicy(const CppArgumentList&, CppVariant* result)
{
    // FIXME: implement.
    // Currently only has a non-null implementation on QT.
    result->setNull();
}

void LayoutTestController::setCustomPolicyDelegate(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        bool enable = arguments[0].value.boolValue;
        bool permissive = false;
        if (arguments.size() > 1 && arguments[1].isBool())
            permissive = arguments[1].value.boolValue;
        m_shell->webViewHost()->setCustomPolicyDelegate(enable, permissive);
    }
    result->setNull();
}

void LayoutTestController::waitForPolicyDelegate(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->waitForPolicyDelegate();
    m_waitUntilDone = true;
    result->setNull();
}

void LayoutTestController::setWillSendRequestClearHeader(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        string header = arguments[0].toString();
        if (!header.empty())
            m_shell->webViewHost()->addClearHeader(String::fromUTF8(header.c_str()));
    }
    result->setNull();
}

void LayoutTestController::setWillSendRequestReturnsNullOnRedirect(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setBlockRedirects(arguments[0].value.boolValue);
    result->setNull();
}

void LayoutTestController::setWillSendRequestReturnsNull(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setRequestReturnNull(arguments[0].value.boolValue);
    result->setNull();
}

void LayoutTestController::pathToLocalResource(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <= 0 || !arguments[0].isString())
        return;

    string url = arguments[0].toString();
#if OS(WINDOWS)
    if (!url.find("/tmp/")) {
        // We want a temp file.
        const unsigned tempPrefixLength = 5;
        size_t bufferSize = MAX_PATH;
        OwnArrayPtr<WCHAR> tempPath = adoptArrayPtr(new WCHAR[bufferSize]);
        DWORD tempLength = ::GetTempPathW(bufferSize, tempPath.get());
        if (tempLength + url.length() - tempPrefixLength + 1 > bufferSize) {
            bufferSize = tempLength + url.length() - tempPrefixLength + 1;
            tempPath = adoptArrayPtr(new WCHAR[bufferSize]);
            tempLength = GetTempPathW(bufferSize, tempPath.get());
            ASSERT(tempLength < bufferSize);
        }
        string resultPath(WebString(tempPath.get(), tempLength).utf8());
        resultPath.append(url.substr(tempPrefixLength));
        result->set(resultPath);
        return;
    }
#endif

    // Some layout tests use file://// which we resolve as a UNC path. Normalize
    // them to just file:///.
    string lowerUrl = url;
    transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
    while (!lowerUrl.find("file:////")) {
        url = url.substr(0, 8) + url.substr(9);
        lowerUrl = lowerUrl.substr(0, 8) + lowerUrl.substr(9);
    }
    result->set(webkit_support::RewriteLayoutTestsURL(url).spec());
}

void LayoutTestController::addFileToPasteboardOnDrag(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
    m_shouldAddFileToPasteboard = true;
}

void LayoutTestController::setStopProvisionalFrameLoads(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
    m_stopProvisionalFrameLoads = true;
}

void LayoutTestController::setSmartInsertDeleteEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setSmartInsertDeleteEnabled(arguments[0].value.boolValue);
    result->setNull();
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setSelectTrailingWhitespaceEnabled(arguments[0].value.boolValue);
    result->setNull();
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(const WebString& animationName, double time, const WebString& elementId)
{
    WebFrame* webFrame = m_shell->webView()->mainFrame();
    if (!webFrame)
        return false;

    WebAnimationController* controller = webFrame->animationController();
    if (!controller)
        return false;

    WebElement element = webFrame->document().getElementById(elementId);
    if (element.isNull())
        return false;
    return controller->pauseAnimationAtTime(element, animationName, time);
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(const WebString& propertyName, double time, const WebString& elementId)
{
    WebFrame* webFrame = m_shell->webView()->mainFrame();
    if (!webFrame)
        return false;

    WebAnimationController* controller = webFrame->animationController();
    if (!controller)
        return false;

    WebElement element = webFrame->document().getElementById(elementId);
    if (element.isNull())
        return false;
    return controller->pauseTransitionAtTime(element, propertyName, time);
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(const WebString& elementId)
{
    WebFrame* webFrame = m_shell->webView()->mainFrame();
    if (!webFrame)
        return false;

    WebElement element = webFrame->document().getElementById(elementId);
    if (element.isNull() || !element.hasTagName("input"))
        return false;

    WebInputElement inputElement = element.to<WebInputElement>();
    return inputElement.autoComplete();
}

int LayoutTestController::numberOfActiveAnimations()
{
    WebFrame* webFrame = m_shell->webView()->mainFrame();
    if (!webFrame)
        return -1;

    WebAnimationController* controller = webFrame->animationController();
    if (!controller)
        return -1;

    return controller->numberOfActiveAnimations();
}

void LayoutTestController::resumeAnimations()
{
    WebFrame* webFrame = m_shell->webView()->mainFrame();
    if (!webFrame)
        return;

    WebAnimationController* controller = webFrame->animationController();
    if (!controller)
        return;

    controller->resumeAnimations();
}

void LayoutTestController::pauseAnimationAtTimeOnElementWithId(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(false);
    if (arguments.size() > 2 && arguments[0].isString() && arguments[1].isNumber() && arguments[2].isString()) {
        WebString animationName = cppVariantToWebString(arguments[0]);
        double time = arguments[1].toDouble();
        WebString elementId = cppVariantToWebString(arguments[2]);
        result->set(pauseAnimationAtTimeOnElementWithId(animationName, time, elementId));
    }
}

void LayoutTestController::pauseTransitionAtTimeOnElementWithId(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(false);
    if (arguments.size() > 2 && arguments[0].isString() && arguments[1].isNumber() && arguments[2].isString()) {
        WebString propertyName = cppVariantToWebString(arguments[0]);
        double time = arguments[1].toDouble();
        WebString elementId = cppVariantToWebString(arguments[2]);
        result->set(pauseTransitionAtTimeOnElementWithId(propertyName, time, elementId));
    }
}

void LayoutTestController::elementDoesAutoCompleteForElementWithId(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
    WebString elementId = cppVariantToWebString(arguments[0]);
    result->set(elementDoesAutoCompleteForElementWithId(elementId));
}

void LayoutTestController::enableAutoResizeMode(const CppArgumentList& arguments, CppVariant* result)
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

    m_shell->webView()->enableAutoResizeMode(minSize, maxSize);
    result->set(true);
}

void LayoutTestController::disableAutoResizeMode(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() !=2) {
        result->set(false);
        return;
    }
    int newWidth = cppVariantToInt32(arguments[0]);
    int newHeight = cppVariantToInt32(arguments[1]);
    WebKit::WebSize newSize(newWidth, newHeight);

    m_shell->webViewHost()->setWindowRect(WebRect(0, 0, newSize.width, newSize.height));
    m_shell->webView()->disableAutoResizeMode();
    m_shell->webView()->resize(newSize);
    result->set(true);
}

void LayoutTestController::numberOfActiveAnimations(const CppArgumentList&, CppVariant* result)
{
    result->set(numberOfActiveAnimations());
}

void LayoutTestController::resumeAnimations(const CppArgumentList&, CppVariant* result)
{
    resumeAnimations();
    result->setNull();
}

void LayoutTestController::disableImageLoading(const CppArgumentList&, CppVariant* result)
{
    m_shell->preferences()->loadsImagesAutomatically = false;
    m_shell->applyPreferences();
    result->setNull();
}

void LayoutTestController::setIconDatabaseEnabled(const CppArgumentList&, CppVariant* result)
{
    // We don't use the WebKit icon database.
    result->setNull();
}

void LayoutTestController::callShouldCloseOnWebView(const CppArgumentList&, CppVariant* result)
{
    result->set(m_shell->webView()->dispatchBeforeUnloadEvent());
}

#if ENABLE(NOTIFICATIONS)
void LayoutTestController::grantDesktopNotificationPermission(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    m_shell->notificationPresenter()->grantPermission(cppVariantToWebString(arguments[0]));
#endif
    result->set(true);
}

void LayoutTestController::simulateDesktopNotificationClick(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    if (m_shell->notificationPresenter()->simulateClick(cppVariantToWebString(arguments[0])))
        result->set(true);
    else
#endif
        result->set(false);
}
#endif

void LayoutTestController::setDomainRelaxationForbiddenForURLScheme(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 2 || !arguments[0].isBool() || !arguments[1].isString())
        return;
    m_shell->webView()->setDomainRelaxationForbidden(cppVariantToBool(arguments[0]), cppVariantToWebString(arguments[1]));
}

void LayoutTestController::setDeferMainResourceDataLoad(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1)
        m_deferMainResourceDataLoad = cppVariantToBool(arguments[0]);
}

//
// Unimplemented stubs
//

void LayoutTestController::dumpAsWebArchive(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::setMainFrameIsFirstResponder(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::dumpSelectionRect(const CppArgumentList& arguments, CppVariant* result)
{
    m_dumpSelectionRect = true;
    result->setNull();
}

void LayoutTestController::display(const CppArgumentList& arguments, CppVariant* result)
{
    WebViewHost* host = m_shell->webViewHost();
    const WebKit::WebSize& size = m_shell->webView()->size();
    WebRect rect(0, 0, size.width, size.height);
    host->updatePaintRect(rect);
    host->paintInvalidatedRegion();
    host->displayRepaintMask();
    result->setNull();
}

void LayoutTestController::displayInvalidatedRegion(const CppArgumentList& arguments, CppVariant* result)
{
    WebViewHost* host = m_shell->webViewHost();
    host->paintInvalidatedRegion();
    host->displayRepaintMask();
    result->setNull();
}

void LayoutTestController::testRepaint(const CppArgumentList&, CppVariant* result)
{
    m_testRepaint = true;
    result->setNull();
}

void LayoutTestController::repaintSweepHorizontally(const CppArgumentList&, CppVariant* result)
{
    m_sweepHorizontally = true;
    result->setNull();
}

void LayoutTestController::clearBackForwardList(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::keepWebHistory(const CppArgumentList& arguments,  CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::storeWebScriptObject(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::accessStoredWebScriptObject(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::objCClassNameOf(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::addDisallowedURL(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::setCallCloseOnWebViews(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::setPrivateBrowsingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::setJavaScriptCanAccessClipboard(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->javaScriptCanAccessClipboard = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::setXSSAuditorEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->XSSAuditorEnabled = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::evaluateScriptInIsolatedWorldAndReturnValue(const CppArgumentList& arguments, CppVariant* result)
{
    v8::HandleScope scope;
    WebVector<v8::Local<v8::Value> > values;
    if (arguments.size() >= 2 && arguments[0].isNumber() && arguments[1].isString()) {
        WebScriptSource source(cppVariantToWebString(arguments[1]));
        // This relies on the iframe focusing itself when it loads. This is a bit
        // sketchy, but it seems to be what other tests do.
        m_shell->webView()->focusedFrame()->executeScriptInIsolatedWorld(arguments[0].toInt32(), &source, 1, 1, &values);
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

void LayoutTestController::evaluateScriptInIsolatedWorld(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() >= 2 && arguments[0].isNumber() && arguments[1].isString()) {
        WebScriptSource source(cppVariantToWebString(arguments[1]));
        // This relies on the iframe focusing itself when it loads. This is a bit
        // sketchy, but it seems to be what other tests do.
        m_shell->webView()->focusedFrame()->executeScriptInIsolatedWorld(arguments[0].toInt32(), &source, 1, 1);
    }
    result->setNull();
}

void LayoutTestController::setIsolatedWorldSecurityOrigin(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;

    m_shell->webView()->focusedFrame()->setIsolatedWorldSecurityOrigin(
        arguments[0].toInt32(),
        WebSecurityOrigin::createFromString(cppVariantToWebString(arguments[1])));
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->allowUniversalAccessFromFileURLs = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::setAllowDisplayOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setDisplayingInsecureContentAllowed(arguments[0].toBoolean());

    result->setNull();
}

void LayoutTestController::setAllowFileAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->allowFileAccessFromFileURLs = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::setAllowRunningOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setRunningInsecureContentAllowed(arguments[0].value.boolValue);

    result->setNull();
}

// Need these conversions because the format of the value for booleans
// may vary - for example, on mac "1" and "0" are used for boolean.
bool LayoutTestController::cppVariantToBool(const CppVariant& value)
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
    logErrorToConsole("Invalid value. Expected boolean value.");
    return false;
}

int32_t LayoutTestController::cppVariantToInt32(const CppVariant& value)
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
    logErrorToConsole("Invalid value for preference. Expected integer value.");
    return 0;
}

WebString LayoutTestController::cppVariantToWebString(const CppVariant& value)
{
    if (!value.isString()) {
        logErrorToConsole("Invalid value for preference. Expected string value.");
        return WebString();
    }
    return WebString::fromUTF8(value.toString());
}

Vector<WebString> LayoutTestController::cppVariantToWebStringArray(const CppVariant& value)
{
    if (!value.isObject()) {
        logErrorToConsole("Invalid value for preference. Expected object value.");
        return Vector<WebString>();
    }
    Vector<WebString> resultVector;
    Vector<string> stringVector = value.toStringVector();
    for (size_t i = 0; i < stringVector.size(); ++i)
        resultVector.append(WebString::fromUTF8(stringVector[i].c_str()));
    return resultVector;
}

// Sets map based on scriptFontPairs, a collapsed vector of pairs of ISO 15924
// four-letter script code and font such as:
// { "Arab", "My Arabic Font", "Grek", "My Greek Font" }
static void setFontMap(WebPreferences::ScriptFontFamilyMap& map, const Vector<WebString>& scriptFontPairs)
{
    map.clear();
    size_t i = 0;
    while (i + 1 < scriptFontPairs.size()) {
        const WebString& script = scriptFontPairs[i++];
        const WebString& font = scriptFontPairs[i++];

        int32_t code = u_getPropertyValueEnum(UCHAR_SCRIPT, script.utf8().data());
        if (code >= 0 && code < USCRIPT_CODE_LIMIT)
            map.set(static_cast<int>(code), font);
    }
}

void LayoutTestController::overridePreference(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[0].isString())
        return;

    string key = arguments[0].toString();
    CppVariant value = arguments[1];
    WebPreferences* prefs = m_shell->preferences();
    if (key == "WebKitStandardFont")
        prefs->standardFontFamily = cppVariantToWebString(value);
    else if (key == "WebKitFixedFont")
        prefs->fixedFontFamily = cppVariantToWebString(value);
    else if (key == "WebKitSerifFont")
        prefs->serifFontFamily = cppVariantToWebString(value);
    else if (key == "WebKitSansSerifFont")
        prefs->sansSerifFontFamily = cppVariantToWebString(value);
    else if (key == "WebKitCursiveFont")
        prefs->cursiveFontFamily = cppVariantToWebString(value);
    else if (key == "WebKitFantasyFont")
        prefs->fantasyFontFamily = cppVariantToWebString(value);
    else if (key == "WebKitStandardFontMap")
        setFontMap(prefs->standardFontMap, cppVariantToWebStringArray(value));
    else if (key == "WebKitFixedFontMap")
        setFontMap(prefs->fixedFontMap, cppVariantToWebStringArray(value));
    else if (key == "WebKitSerifFontMap")
        setFontMap(prefs->serifFontMap, cppVariantToWebStringArray(value));
    else if (key == "WebKitSansSerifFontMap")
        setFontMap(prefs->sansSerifFontMap, cppVariantToWebStringArray(value));
    else if (key == "WebKitCursiveFontMap")
        setFontMap(prefs->cursiveFontMap, cppVariantToWebStringArray(value));
    else if (key == "WebKitFantasyFontMap")
        setFontMap(prefs->fantasyFontMap, cppVariantToWebStringArray(value));
    else if (key == "WebKitDefaultFontSize")
        prefs->defaultFontSize = cppVariantToInt32(value);
    else if (key == "WebKitDefaultFixedFontSize")
        prefs->defaultFixedFontSize = cppVariantToInt32(value);
    else if (key == "WebKitMinimumFontSize")
        prefs->minimumFontSize = cppVariantToInt32(value);
    else if (key == "WebKitMinimumLogicalFontSize")
        prefs->minimumLogicalFontSize = cppVariantToInt32(value);
    else if (key == "WebKitDefaultTextEncodingName")
        prefs->defaultTextEncodingName = cppVariantToWebString(value);
    else if (key == "WebKitJavaScriptEnabled")
        prefs->javaScriptEnabled = cppVariantToBool(value);
    else if (key == "WebKitWebSecurityEnabled")
        prefs->webSecurityEnabled = cppVariantToBool(value);
    else if (key == "WebKitJavaScriptCanOpenWindowsAutomatically")
        prefs->javaScriptCanOpenWindowsAutomatically = cppVariantToBool(value);
    else if (key == "WebKitDisplayImagesKey")
        prefs->loadsImagesAutomatically = cppVariantToBool(value);
    else if (key == "WebKitPluginsEnabled")
        prefs->pluginsEnabled = cppVariantToBool(value);
    else if (key == "WebKitDOMPasteAllowedPreferenceKey")
        prefs->DOMPasteAllowed = cppVariantToBool(value);
    else if (key == "WebKitDeveloperExtrasEnabledPreferenceKey")
        prefs->developerExtrasEnabled = cppVariantToBool(value);
    else if (key == "WebKitShrinksStandaloneImagesToFit")
        prefs->shrinksStandaloneImagesToFit = cppVariantToBool(value);
    else if (key == "WebKitTextAreasAreResizable")
        prefs->textAreasAreResizable = cppVariantToBool(value);
    else if (key == "WebKitJavaEnabled")
        prefs->javaEnabled = cppVariantToBool(value);
    else if (key == "WebKitUsesPageCachePreferenceKey")
        prefs->usesPageCache = cppVariantToBool(value);
    else if (key == "WebKitPageCacheSupportsPluginsPreferenceKey")
        prefs->pageCacheSupportsPlugins = cppVariantToBool(value);
    else if (key == "WebKitJavaScriptCanAccessClipboard")
        prefs->javaScriptCanAccessClipboard = cppVariantToBool(value);
    else if (key == "WebKitXSSAuditorEnabled")
        prefs->XSSAuditorEnabled = cppVariantToBool(value);
    else if (key == "WebKitLocalStorageEnabledPreferenceKey")
        prefs->localStorageEnabled = cppVariantToBool(value);
    else if (key == "WebKitOfflineWebApplicationCacheEnabled")
        prefs->offlineWebApplicationCacheEnabled = cppVariantToBool(value);
    else if (key == "WebKitTabToLinksPreferenceKey")
        prefs->tabsToLinks = cppVariantToBool(value);
    else if (key == "WebKitWebGLEnabled")
        prefs->experimentalWebGLEnabled = cppVariantToBool(value);
    else if (key == "WebKitCSSRegionsEnabled")
        prefs->experimentalCSSRegionsEnabled = cppVariantToBool(value);
    else if (key == "WebKitHyperlinkAuditingEnabled")
        prefs->hyperlinkAuditingEnabled = cppVariantToBool(value);
    else if (key == "WebKitEnableCaretBrowsing")
        prefs->caretBrowsingEnabled = cppVariantToBool(value);
    else if (key == "WebKitAllowDisplayingInsecureContent")
        prefs->allowDisplayOfInsecureContent = cppVariantToBool(value);
    else if (key == "WebKitAllowRunningInsecureContent")
        prefs->allowRunningOfInsecureContent = cppVariantToBool(value);
    else if (key == "WebKitHixie76WebSocketProtocolEnabled")
        prefs->hixie76WebSocketProtocolEnabled = cppVariantToBool(value);
    else if (key == "WebKitCSSCustomFilterEnabled")
        prefs->cssCustomFilterEnabled = cppVariantToBool(value);
    else if (key == "WebKitWebAudioEnabled") {
        ASSERT(cppVariantToBool(value));
    } else {
        string message("Invalid name for preference: ");
        message.append(key);
        logErrorToConsole(message);
    }
    m_shell->applyPreferences();
}

void LayoutTestController::fallbackMethod(const CppArgumentList&, CppVariant* result)
{
    printf("CONSOLE MESSAGE: JavaScript ERROR: unknown method called on LayoutTestController\n");
    result->setNull();
}

void LayoutTestController::addOriginAccessWhitelistEntry(const CppArgumentList& arguments, CppVariant* result)
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

void LayoutTestController::removeOriginAccessWhitelistEntry(const CppArgumentList& arguments, CppVariant* result)
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

void LayoutTestController::clearAllDatabases(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    webkit_support::ClearAllDatabases();
}

void LayoutTestController::setDatabaseQuota(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if ((arguments.size() >= 1) && arguments[0].isNumber())
        webkit_support::SetDatabaseQuota(arguments[0].toInt32());
}

void LayoutTestController::setPOSIXLocale(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() == 1 && arguments[0].isString())
        setlocale(LC_ALL, arguments[0].toString().c_str());
}

void LayoutTestController::counterValueForElementById(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isString())
        return;
    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    WebString counterValue = frame->counterValueForElementById(cppVariantToWebString(arguments[0]));
    if (counterValue.isNull())
        return;
    result->set(counterValue.utf8());
}

// Parse a single argument. The method returns true if there is an argument that
// is a number or if there is no argument at all. It returns false only if there
// is some argument that is not a number. The value parameter is filled with the
// parsed number, or given the default if there is no argument.
static bool parseCppArgumentInt32(const CppArgumentList& arguments, int argIndex, int* value, int defaultValue)
{
    if (static_cast<int>(arguments.size()) > argIndex) {
        if (!arguments[argIndex].isNumber())
            return false;
        *value = arguments[argIndex].toInt32();
        return true;
    }
    *value = defaultValue;
    return true;
}

static bool parsePageSizeParameters(const CppArgumentList& arguments,
                                    int argOffset,
                                    int* pageWidthInPixels,
                                    int* pageHeightInPixels)
{
    // WebKit is using the window width/height of DumpRenderTree as the
    // default value of the page size.
    // FIXME: share the default values with other ports.
    int argCount = static_cast<int>(arguments.size()) - argOffset;
    if (argCount && argCount != 2)
        return false;
    if (!parseCppArgumentInt32(arguments, argOffset, pageWidthInPixels, 800)
        || !parseCppArgumentInt32(arguments, argOffset + 1, pageHeightInPixels, 600))
        return false;
    return true;
}

static bool parsePageNumber(const CppArgumentList& arguments, int argOffset, int* pageNumber)
{
    if (static_cast<int>(arguments.size()) > argOffset + 1)
        return false;
    if (!parseCppArgumentInt32(arguments, argOffset, pageNumber, 0))
        return false;
    return true;
}

static bool parsePageNumberSizeMargins(const CppArgumentList& arguments, int argOffset,
                                       int* pageNumber, int* width, int* height,
                                       int* marginTop, int* marginRight, int* marginBottom, int* marginLeft)
{
    int argCount = static_cast<int>(arguments.size()) - argOffset;
    if (argCount && argCount != 7)
        return false;
    if (!parseCppArgumentInt32(arguments, argOffset, pageNumber, 0)
        || !parseCppArgumentInt32(arguments, argOffset + 1, width, 0)
        || !parseCppArgumentInt32(arguments, argOffset + 2, height, 0)
        || !parseCppArgumentInt32(arguments, argOffset + 3, marginTop, 0)
        || !parseCppArgumentInt32(arguments, argOffset + 4, marginRight, 0)
        || !parseCppArgumentInt32(arguments, argOffset + 5, marginBottom, 0)
        || !parseCppArgumentInt32(arguments, argOffset + 6, marginLeft, 0))
        return false;
    return true;
}

void LayoutTestController::setPrinting(const CppArgumentList& arguments, CppVariant* result)
{
    setIsPrinting(true);
    result->setNull();
}

void LayoutTestController::pageNumberForElementById(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    int pageWidthInPixels = 0;
    int pageHeightInPixels = 0;
    if (!parsePageSizeParameters(arguments, 1,
                                 &pageWidthInPixels, &pageHeightInPixels))
        return;
    if (!arguments[0].isString())
        return;
    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    result->set(frame->pageNumberForElementById(cppVariantToWebString(arguments[0]),
                                                static_cast<float>(pageWidthInPixels),
                                                static_cast<float>(pageHeightInPixels)));
}

void LayoutTestController::pageSizeAndMarginsInPixels(const CppArgumentList& arguments, CppVariant* result)
{
    result->set("");
    int pageNumber = 0;
    int width = 0;
    int height = 0;
    int marginTop = 0;
    int marginRight = 0;
    int marginBottom = 0;
    int marginLeft = 0;
    if (!parsePageNumberSizeMargins(arguments, 0, &pageNumber, &width, &height, &marginTop, &marginRight, &marginBottom,
                                    &marginLeft))
        return;

    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    WebSize pageSize(width, height);
    frame->pageSizeAndMarginsInPixels(pageNumber, pageSize, marginTop, marginRight, marginBottom, marginLeft);
    stringstream resultString;
    resultString << "(" << pageSize.width << ", " << pageSize.height << ") " << marginTop << " " << marginRight << " "
                 << marginBottom << " " << marginLeft;
    result->set(resultString.str());
}

void LayoutTestController::hasCustomPageSizeStyle(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(false);
    int pageIndex = 0;
    if (!parsePageNumber(arguments, 0, &pageIndex))
        return;
    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    result->set(frame->hasCustomPageSizeStyle(pageIndex));
}

void LayoutTestController::pageProperty(const CppArgumentList& arguments, CppVariant* result)
{
    result->set("");
    int pageNumber = 0;
    if (!parsePageNumber(arguments, 1, &pageNumber))
        return;
    if (!arguments[0].isString())
        return;
    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    WebSize pageSize(800, 800);
    frame->printBegin(pageSize);
    result->set(frame->pageProperty(cppVariantToWebString(arguments[0]), pageNumber).utf8());
    frame->printEnd();
}

void LayoutTestController::numberOfPages(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    int pageWidthInPixels = 0;
    int pageHeightInPixels = 0;
    if (!parsePageSizeParameters(arguments, 0, &pageWidthInPixels, &pageHeightInPixels))
        return;

    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    WebSize size(pageWidthInPixels, pageHeightInPixels);
    int numberOfPages = frame->printBegin(size);
    frame->printEnd();
    result->set(numberOfPages);
}

void LayoutTestController::numberOfPendingGeolocationPermissionRequests(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    Vector<WebViewHost*> windowList = m_shell->windowList();
    int numberOfRequests = 0;
    for (size_t i = 0; i < windowList.size(); i++)
        numberOfRequests += windowList[i]->geolocationClientMock()->numberOfPendingPermissionRequests();
    result->set(numberOfRequests);
}

void LayoutTestController::logErrorToConsole(const std::string& text)
{
    m_shell->webViewHost()->didAddMessageToConsole(
        WebConsoleMessage(WebConsoleMessage::LevelError, WebString::fromUTF8(text)),
        WebString(), 0);
}

void LayoutTestController::setJavaScriptProfilingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;
    m_shell->drtDevToolsAgent()->setJavaScriptProfilingEnabled(arguments[0].toBoolean());
}

void LayoutTestController::evaluateInWebInspector(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;
    m_shell->drtDevToolsAgent()->evaluateInWebInspector(arguments[0].toInt32(), arguments[1].toString());
}

void LayoutTestController::forceRedSelectionColors(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    m_shell->webView()->setSelectionColors(0xffee0000, 0xff00ee00, 0xff000000, 0xffc0c0c0);
}

void LayoutTestController::addUserScript(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isString() || !arguments[1].isBool() || !arguments[2].isBool())
        return;
    WebView::addUserScript(
        cppVariantToWebString(arguments[0]), WebVector<WebString>(),
        arguments[1].toBoolean() ? WebView::UserScriptInjectAtDocumentStart : WebView::UserScriptInjectAtDocumentEnd,
        arguments[2].toBoolean() ? WebView::UserContentInjectInAllFrames : WebView::UserContentInjectInTopFrameOnly);
}

void LayoutTestController::addUserStyleSheet(const CppArgumentList& arguments, CppVariant* result)
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

void LayoutTestController::setEditingBehavior(const CppArgumentList& arguments, CppVariant* results)
{
    string key = arguments[0].toString();
    if (key == "mac") {
        m_shell->preferences()->editingBehavior = WebSettings::EditingBehaviorMac;
        m_shell->applyPreferences();
    } else if (key == "win") {
        m_shell->preferences()->editingBehavior = WebSettings::EditingBehaviorWin;
        m_shell->applyPreferences();
    } else if (key == "unix") {
        m_shell->preferences()->editingBehavior = WebSettings::EditingBehaviorUnix;
        m_shell->applyPreferences();
    } else
        logErrorToConsole("Passed invalid editing behavior. Should be 'mac', 'win', or 'unix'.");
}

void LayoutTestController::setMockDeviceOrientation(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 6 || !arguments[0].isBool() || !arguments[1].isNumber() || !arguments[2].isBool() || !arguments[3].isNumber() || !arguments[4].isBool() || !arguments[5].isNumber())
        return;

    WebDeviceOrientation orientation(arguments[0].toBoolean(), arguments[1].toDouble(), arguments[2].toBoolean(), arguments[3].toDouble(), arguments[4].toBoolean(), arguments[5].toDouble());
    // Note that we only call setOrientation on the main page's mock since this is all that the
    // tests require. If necessary, we could get a list of WebViewHosts from the TestShell and
    // call setOrientation on each DeviceOrientationClientMock.
    m_shell->webViewHost()->deviceOrientationClientMock()->setOrientation(orientation);
}

// FIXME: For greater test flexibility, we should be able to set each page's geolocation mock individually.
// https://bugs.webkit.org/show_bug.cgi?id=52368
void LayoutTestController::setGeolocationPermission(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;
    Vector<WebViewHost*> windowList = m_shell->windowList();
    for (size_t i = 0; i < windowList.size(); i++)
        windowList[i]->geolocationClientMock()->setPermission(arguments[0].toBoolean());
}

void LayoutTestController::setMockGeolocationPosition(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isNumber() || !arguments[1].isNumber() || !arguments[2].isNumber())
        return;
    Vector<WebViewHost*> windowList = m_shell->windowList();
    for (size_t i = 0; i < windowList.size(); i++)
        windowList[i]->geolocationClientMock()->setPosition(arguments[0].toDouble(), arguments[1].toDouble(), arguments[2].toDouble());
}

void LayoutTestController::setMockGeolocationError(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;
    Vector<WebViewHost*> windowList = m_shell->windowList();
    for (size_t i = 0; i < windowList.size(); i++)
        windowList[i]->geolocationClientMock()->setError(arguments[0].toInt32(), cppVariantToWebString(arguments[1]));
}

void LayoutTestController::abortModal(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

#if ENABLE(INPUT_SPEECH)
void LayoutTestController::addMockSpeechInputResult(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isString() || !arguments[1].isNumber() || !arguments[2].isString())
        return;

    if (MockWebSpeechInputController* controller = m_shell->webViewHost()->speechInputControllerMock())
        controller->addMockRecognitionResult(cppVariantToWebString(arguments[0]), arguments[1].toDouble(), cppVariantToWebString(arguments[2]));
}

void LayoutTestController::setMockSpeechInputDumpRect(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;

    if (MockWebSpeechInputController* controller = m_shell->webViewHost()->speechInputControllerMock())
        controller->setDumpRect(arguments[0].value.boolValue);
}
#endif

void LayoutTestController::startSpeechInput(const CppArgumentList& arguments, CppVariant* result)
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

void LayoutTestController::layerTreeAsText(const CppArgumentList& args, CppVariant* result)
{
    result->set(m_shell->webView()->mainFrame()->layerTreeAsText(m_showDebugLayerTree).utf8());
}

void LayoutTestController::loseCompositorContext(const CppArgumentList& args, CppVariant*)
{
    int numTimes;
    if (args.size() == 1 || !args[0].isNumber())
        numTimes = 1;
    else
        numTimes = args[0].toInt32();
    m_shell->webView()->loseCompositorContext(numTimes);
}

void LayoutTestController::markerTextForListItem(const CppArgumentList& args, CppVariant* result)
{
    WebElement element;
    if (!WebBindings::getElement(args[0].value.objectValue, &element))
        result->setNull();
    else
        result->set(element.document().frame()->markerTextForListItem(element).utf8());
}

void LayoutTestController::findString(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isString())
        return;

    WebFindOptions findOptions;
    bool wrapAround = false;
    if (arguments.size() >= 2) {
        Vector<std::string> optionsArray = arguments[1].toStringVector();
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

    WebFrame* frame = m_shell->webView()->mainFrame();
    const bool findResult = frame->find(0, cppVariantToWebString(arguments[0]), findOptions, wrapAround, 0);
    result->set(findResult);
}

void LayoutTestController::setMinimumTimerInterval(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isNumber())
        return;
    m_shell->webView()->settings()->setMinimumTimerInterval(arguments[0].toDouble());
}

void LayoutTestController::setAutofilled(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[1].isBool())
        return;

    WebElement element;
    if (!WebBindings::getElement(arguments[0].value.objectValue, &element))
        return;

    WebInputElement* input = toWebInputElement(&element);
    if (!input)
        return;

    input->setAutofilled(arguments[1].value.boolValue);
}

void LayoutTestController::setValueForUser(const CppArgumentList& arguments, CppVariant* result)
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

void LayoutTestController::deleteAllLocalStorage(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void LayoutTestController::localStorageDiskUsageForOrigin(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void LayoutTestController::originsWithLocalStorage(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void LayoutTestController::deleteLocalStorageForOrigin(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void LayoutTestController::observeStorageTrackerNotifications(const CppArgumentList&, CppVariant*)
{
    // Not Implemented
}

void LayoutTestController::syncLocalStorage(const CppArgumentList&, CppVariant*)
{
    // Not Implemented
}

void LayoutTestController::setShouldStayOnPageAfterHandlingBeforeUnload(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1 && arguments[0].isBool())
        m_shouldStayOnPageAfterHandlingBeforeUnload = arguments[0].toBoolean();

    result->setNull();
}

void LayoutTestController::enableFixedLayoutMode(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  1 || !arguments[0].isBool())
        return;
    bool enableFixedLayout = arguments[0].toBoolean();
    m_shell->webView()->enableFixedLayoutMode(enableFixedLayout);
}

void LayoutTestController::setFixedLayoutSize(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;
    int width = arguments[0].toInt32();
    int height = arguments[1].toInt32();
    m_shell->webView()->setFixedLayoutSize(WebSize(width, height));
}

void LayoutTestController::selectionAsMarkup(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(m_shell->webView()->mainFrame()->selectionAsMarkup().utf8());
}

void LayoutTestController::workerThreadCount(CppVariant* result)
{
    result->set(static_cast<int>(WebWorkerInfo::dedicatedWorkerCount()));
}

void LayoutTestController::sendWebIntentResponse(const CppArgumentList& arguments, CppVariant* result)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> ctx = m_shell->webView()->mainFrame()->mainWorldScriptContext();
    result->set(m_shell->webView()->mainFrame()->selectionAsMarkup().utf8());
    v8::Context::Scope cscope(ctx);

    WebKit::WebIntentRequest* request = m_shell->webViewHost()->currentIntentRequest();
    if (request->isNull())
        return;

    if (arguments.size() == 1) {
        WebKit::WebCString reply = cppVariantToWebString(arguments[0]).utf8();
        v8::Handle<v8::Value> v8value = v8::String::New(reply.data(), reply.length());
        request->postResult(WebKit::WebSerializedScriptValue::serialize(v8value));
    } else {
        v8::Handle<v8::Value> v8value = v8::String::New("ERROR");
        request->postFailure(WebKit::WebSerializedScriptValue::serialize(v8value));
    }
    result->setNull();
}

void LayoutTestController::setPluginsEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->pluginsEnabled = arguments[0].toBoolean();
        m_shell->applyPreferences();
    }
    result->setNull();
}

void LayoutTestController::resetPageVisibility(const CppArgumentList& arguments, CppVariant* result)
{
    m_shell->webView()->setVisibilityState(WebPageVisibilityStateVisible, true);
}

void LayoutTestController::setPageVisibility(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        string newVisibility = arguments[0].toString();
        if (newVisibility == "visible")
            m_shell->webView()->setVisibilityState(WebPageVisibilityStateVisible, false);
        else if (newVisibility == "hidden")
            m_shell->webView()->setVisibilityState(WebPageVisibilityStateHidden, false);
        else if (newVisibility == "prerender")
            m_shell->webView()->setVisibilityState(WebPageVisibilityStatePrerender, false);
        else if (newVisibility == "preview")
            m_shell->webView()->setVisibilityState(WebPageVisibilityStatePreview, false);
    }
}

void LayoutTestController::setTextDirection(const CppArgumentList& arguments, CppVariant* result)
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

    m_shell->webView()->setTextDirection(direction);
}

void LayoutTestController::setAudioData(const CppArgumentList& arguments, CppVariant* result)
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

    setShouldDumpAsAudio(true);
}

void LayoutTestController::setHasCustomFullScreenBehavior(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  1 || !arguments[0].isBool())
        return;
    m_hasCustomFullScreenBehavior = arguments[0].toBoolean();
}

#if ENABLE(POINTER_LOCK)
void LayoutTestController::didLosePointerLock(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->didLosePointerLock();
    result->setNull();
}

void LayoutTestController::setPointerLockWillFailAsynchronously(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->setPointerLockWillFailAsynchronously();
    result->setNull();
}

void LayoutTestController::setPointerLockWillFailSynchronously(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->setPointerLockWillFailSynchronously();
    result->setNull();
}
#endif
