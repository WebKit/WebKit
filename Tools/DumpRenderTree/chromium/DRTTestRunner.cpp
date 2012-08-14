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
#include "DRTTestRunner.h"

#include "DRTDevToolsAgent.h"
#include "MockWebSpeechInputController.h"
#include "MockWebSpeechRecognizer.h"
#include "TestShell.h"
#include "WebAnimationController.h"
#include "WebBindings.h"
#include "WebConsoleMessage.h"
#include "WebDeviceOrientation.h"
#include "WebDeviceOrientationClientMock.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFindOptions.h"
#include "WebFrame.h"
#include "WebGeolocationClientMock.h"
#include "WebIDBFactory.h"
#include "WebInputElement.h"
#include "WebIntent.h"
#include "WebIntentRequest.h"
#include "WebKit.h"
#include "WebNotificationPresenter.h"
#include "WebPermissions.h"
#include "WebPrintParams.h"
#include "WebScriptSource.h"
#include "WebSecurityPolicy.h"
#include "WebSettings.h"
#include "WebSurroundingText.h"
#include "WebView.h"
#include "WebViewHost.h"
#include "WebWorkerInfo.h"
#include "platform/WebData.h"
#include "platform/WebSerializedScriptValue.h"
#include "platform/WebSize.h"
#include "platform/WebURL.h"
#include "v8/include/v8.h"
#include "webkit/support/webkit_support.h"
#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <wtf/OwnArrayPtr.h>
#include <wtf/text/WTFString.h>

#if OS(LINUX) || OS(ANDROID)
#include "linux/WebFontRendering.h"
#endif

using namespace WebCore;
using namespace WebKit;
using namespace std;

class EmptyWebDeliveredIntentClient : public WebKit::WebDeliveredIntentClient {
public:
    EmptyWebDeliveredIntentClient() { }
    ~EmptyWebDeliveredIntentClient() { }

    virtual void postResult(const WebSerializedScriptValue& data) const { }
    virtual void postFailure(const WebSerializedScriptValue& data) const { }
    virtual void destroy() { }
};

DRTTestRunner::DRTTestRunner(TestShell* shell)
    : m_shell(shell)
    , m_closeRemainingWindows(false)
    , m_deferMainResourceDataLoad(false)
    , m_showDebugLayerTree(false)
    , m_workQueue(this)
    , m_intentClient(adoptPtr(new EmptyWebDeliveredIntentClient))
    , m_shouldStayOnPageAfterHandlingBeforeUnload(false)
{

    // Initialize the map that associates methods of this class with the names
    // they will use when called by JavaScript. The actual binding of those
    // names to their methods will be done by calling bindToJavaScript() (defined
    // by CppBoundClass, the parent to DRTTestRunner).
    bindMethod("addFileToPasteboardOnDrag", &DRTTestRunner::addFileToPasteboardOnDrag);
#if ENABLE(INPUT_SPEECH)
    bindMethod("addMockSpeechInputResult", &DRTTestRunner::addMockSpeechInputResult);
    bindMethod("setMockSpeechInputDumpRect", &DRTTestRunner::setMockSpeechInputDumpRect);
#endif
#if ENABLE(SCRIPTED_SPEECH)
    bindMethod("addMockSpeechRecognitionResult", &DRTTestRunner::addMockSpeechRecognitionResult);
    bindMethod("setMockSpeechRecognitionError", &DRTTestRunner::setMockSpeechRecognitionError);
    bindMethod("wasMockSpeechRecognitionAborted", &DRTTestRunner::wasMockSpeechRecognitionAborted);
#endif
    bindMethod("addOriginAccessWhitelistEntry", &DRTTestRunner::addOriginAccessWhitelistEntry);
    bindMethod("addUserScript", &DRTTestRunner::addUserScript);
    bindMethod("addUserStyleSheet", &DRTTestRunner::addUserStyleSheet);
    bindMethod("clearAllDatabases", &DRTTestRunner::clearAllDatabases);
    bindMethod("closeWebInspector", &DRTTestRunner::closeWebInspector);
#if ENABLE(POINTER_LOCK)
    bindMethod("didLosePointerLock", &DRTTestRunner::didLosePointerLock);
#endif
    bindMethod("disableAutoResizeMode", &DRTTestRunner::disableAutoResizeMode);
    bindMethod("disableImageLoading", &DRTTestRunner::disableImageLoading);
    bindMethod("display", &DRTTestRunner::display);
    bindMethod("displayInvalidatedRegion", &DRTTestRunner::displayInvalidatedRegion);
    bindMethod("dumpAsText", &DRTTestRunner::dumpAsText);
    bindMethod("dumpBackForwardList", &DRTTestRunner::dumpBackForwardList);
    bindMethod("dumpChildFramesAsText", &DRTTestRunner::dumpChildFramesAsText);
    bindMethod("dumpChildFrameScrollPositions", &DRTTestRunner::dumpChildFrameScrollPositions);
    bindMethod("dumpDatabaseCallbacks", &DRTTestRunner::dumpDatabaseCallbacks);
    bindMethod("dumpEditingCallbacks", &DRTTestRunner::dumpEditingCallbacks);
    bindMethod("dumpFrameLoadCallbacks", &DRTTestRunner::dumpFrameLoadCallbacks);
    bindMethod("dumpProgressFinishedCallback", &DRTTestRunner::dumpProgressFinishedCallback);
    bindMethod("dumpUserGestureInFrameLoadCallbacks", &DRTTestRunner::dumpUserGestureInFrameLoadCallbacks);
    bindMethod("dumpResourceLoadCallbacks", &DRTTestRunner::dumpResourceLoadCallbacks);
    bindMethod("dumpResourceResponseMIMETypes", &DRTTestRunner::dumpResourceResponseMIMETypes);
    bindMethod("dumpSelectionRect", &DRTTestRunner::dumpSelectionRect);
    bindMethod("dumpStatusCallbacks", &DRTTestRunner::dumpWindowStatusChanges);
    bindMethod("dumpTitleChanges", &DRTTestRunner::dumpTitleChanges);
    bindMethod("dumpPermissionClientCallbacks", &DRTTestRunner::dumpPermissionClientCallbacks);
    bindMethod("dumpCreateView", &DRTTestRunner::dumpCreateView);
    bindMethod("elementDoesAutoCompleteForElementWithId", &DRTTestRunner::elementDoesAutoCompleteForElementWithId);
    bindMethod("enableAutoResizeMode", &DRTTestRunner::enableAutoResizeMode);
    bindMethod("evaluateInWebInspector", &DRTTestRunner::evaluateInWebInspector);
    bindMethod("evaluateScriptInIsolatedWorld", &DRTTestRunner::evaluateScriptInIsolatedWorld);
    bindMethod("evaluateScriptInIsolatedWorldAndReturnValue", &DRTTestRunner::evaluateScriptInIsolatedWorldAndReturnValue);
    bindMethod("setIsolatedWorldSecurityOrigin", &DRTTestRunner::setIsolatedWorldSecurityOrigin);
    bindMethod("execCommand", &DRTTestRunner::execCommand);
    bindMethod("forceRedSelectionColors", &DRTTestRunner::forceRedSelectionColors);
#if ENABLE(NOTIFICATIONS)
    bindMethod("grantDesktopNotificationPermission", &DRTTestRunner::grantDesktopNotificationPermission);
#endif
    bindMethod("findString", &DRTTestRunner::findString);
    bindMethod("isCommandEnabled", &DRTTestRunner::isCommandEnabled);
    bindMethod("hasCustomPageSizeStyle", &DRTTestRunner::hasCustomPageSizeStyle);
    bindMethod("layerTreeAsText", &DRTTestRunner::layerTreeAsText);
    bindMethod("loseCompositorContext", &DRTTestRunner::loseCompositorContext);
    bindMethod("markerTextForListItem", &DRTTestRunner::markerTextForListItem);
    bindMethod("notifyDone", &DRTTestRunner::notifyDone);
    bindMethod("numberOfActiveAnimations", &DRTTestRunner::numberOfActiveAnimations);
    bindMethod("numberOfPages", &DRTTestRunner::numberOfPages);
    bindMethod("numberOfPendingGeolocationPermissionRequests", &DRTTestRunner:: numberOfPendingGeolocationPermissionRequests);
    bindMethod("objCIdentityIsEqual", &DRTTestRunner::objCIdentityIsEqual);
    bindMethod("overridePreference", &DRTTestRunner::overridePreference);
    bindMethod("pageProperty", &DRTTestRunner::pageProperty);
    bindMethod("pageSizeAndMarginsInPixels", &DRTTestRunner::pageSizeAndMarginsInPixels);
    bindMethod("pathToLocalResource", &DRTTestRunner::pathToLocalResource);
    bindMethod("pauseAnimationAtTimeOnElementWithId", &DRTTestRunner::pauseAnimationAtTimeOnElementWithId);
    bindMethod("pauseTransitionAtTimeOnElementWithId", &DRTTestRunner::pauseTransitionAtTimeOnElementWithId);
    bindMethod("queueBackNavigation", &DRTTestRunner::queueBackNavigation);
    bindMethod("queueForwardNavigation", &DRTTestRunner::queueForwardNavigation);
    bindMethod("queueLoadingScript", &DRTTestRunner::queueLoadingScript);
    bindMethod("queueLoad", &DRTTestRunner::queueLoad);
    bindMethod("queueLoadHTMLString", &DRTTestRunner::queueLoadHTMLString);
    bindMethod("queueNonLoadingScript", &DRTTestRunner::queueNonLoadingScript);
    bindMethod("queueReload", &DRTTestRunner::queueReload);
    bindMethod("removeOriginAccessWhitelistEntry", &DRTTestRunner::removeOriginAccessWhitelistEntry);
    bindMethod("repaintSweepHorizontally", &DRTTestRunner::repaintSweepHorizontally);
    bindMethod("resetPageVisibility", &DRTTestRunner::resetPageVisibility);
    bindMethod("setAcceptsEditing", &DRTTestRunner::setAcceptsEditing);
    bindMethod("setAllowDisplayOfInsecureContent", &DRTTestRunner::setAllowDisplayOfInsecureContent);
    bindMethod("setAllowFileAccessFromFileURLs", &DRTTestRunner::setAllowFileAccessFromFileURLs);
    bindMethod("setAllowRunningOfInsecureContent", &DRTTestRunner::setAllowRunningOfInsecureContent);
    bindMethod("setAllowUniversalAccessFromFileURLs", &DRTTestRunner::setAllowUniversalAccessFromFileURLs);
    bindMethod("setAlwaysAcceptCookies", &DRTTestRunner::setAlwaysAcceptCookies);
    bindMethod("setAuthorAndUserStylesEnabled", &DRTTestRunner::setAuthorAndUserStylesEnabled);
    bindMethod("setAutofilled", &DRTTestRunner::setAutofilled);
    bindMethod("setCanOpenWindows", &DRTTestRunner::setCanOpenWindows);
    bindMethod("setCloseRemainingWindowsWhenComplete", &DRTTestRunner::setCloseRemainingWindowsWhenComplete);
    bindMethod("setCustomPolicyDelegate", &DRTTestRunner::setCustomPolicyDelegate);
    bindMethod("setDatabaseQuota", &DRTTestRunner::setDatabaseQuota);
    bindMethod("setDeferMainResourceDataLoad", &DRTTestRunner::setDeferMainResourceDataLoad);
    bindMethod("setDomainRelaxationForbiddenForURLScheme", &DRTTestRunner::setDomainRelaxationForbiddenForURLScheme);
    bindMethod("setAudioData", &DRTTestRunner::setAudioData);
    bindMethod("setGeolocationPermission", &DRTTestRunner::setGeolocationPermission);
    bindMethod("setIconDatabaseEnabled", &DRTTestRunner::setIconDatabaseEnabled);
    bindMethod("setJavaScriptCanAccessClipboard", &DRTTestRunner::setJavaScriptCanAccessClipboard);
    bindMethod("setMinimumTimerInterval", &DRTTestRunner::setMinimumTimerInterval);
    bindMethod("setMockDeviceOrientation", &DRTTestRunner::setMockDeviceOrientation);
    bindMethod("setMockGeolocationError", &DRTTestRunner::setMockGeolocationError);
    bindMethod("setMockGeolocationPosition", &DRTTestRunner::setMockGeolocationPosition);
    bindMethod("setPageVisibility", &DRTTestRunner::setPageVisibility);
    bindMethod("setPluginsEnabled", &DRTTestRunner::setPluginsEnabled);
#if ENABLE(POINTER_LOCK)
    bindMethod("setPointerLockWillFailAsynchronously", &DRTTestRunner::setPointerLockWillFailAsynchronously);
    bindMethod("setPointerLockWillFailSynchronously", &DRTTestRunner::setPointerLockWillFailSynchronously);
#endif
    bindMethod("setPopupBlockingEnabled", &DRTTestRunner::setPopupBlockingEnabled);
    bindMethod("setPOSIXLocale", &DRTTestRunner::setPOSIXLocale);
    bindMethod("setPrinting", &DRTTestRunner::setPrinting);
    bindMethod("setScrollbarPolicy", &DRTTestRunner::setScrollbarPolicy);
    bindMethod("setSelectTrailingWhitespaceEnabled", &DRTTestRunner::setSelectTrailingWhitespaceEnabled);
    bindMethod("setTextSubpixelPositioning", &DRTTestRunner::setTextSubpixelPositioning);
    bindMethod("setBackingScaleFactor", &DRTTestRunner::setBackingScaleFactor);
    bindMethod("setSmartInsertDeleteEnabled", &DRTTestRunner::setSmartInsertDeleteEnabled);
    bindMethod("setStopProvisionalFrameLoads", &DRTTestRunner::setStopProvisionalFrameLoads);
    bindMethod("setTabKeyCyclesThroughElements", &DRTTestRunner::setTabKeyCyclesThroughElements);
    bindMethod("setUserStyleSheetEnabled", &DRTTestRunner::setUserStyleSheetEnabled);
    bindMethod("setUserStyleSheetLocation", &DRTTestRunner::setUserStyleSheetLocation);
    bindMethod("setValueForUser", &DRTTestRunner::setValueForUser);
    bindMethod("setWillSendRequestClearHeader", &DRTTestRunner::setWillSendRequestClearHeader);
    bindMethod("setWillSendRequestReturnsNull", &DRTTestRunner::setWillSendRequestReturnsNull);
    bindMethod("setWillSendRequestReturnsNullOnRedirect", &DRTTestRunner::setWillSendRequestReturnsNullOnRedirect);
    bindMethod("setWindowIsKey", &DRTTestRunner::setWindowIsKey);
    bindMethod("setXSSAuditorEnabled", &DRTTestRunner::setXSSAuditorEnabled);
    bindMethod("setAsynchronousSpellCheckingEnabled", &DRTTestRunner::setAsynchronousSpellCheckingEnabled);
    bindMethod("showWebInspector", &DRTTestRunner::showWebInspector);
#if ENABLE(NOTIFICATIONS)
    bindMethod("simulateDesktopNotificationClick", &DRTTestRunner::simulateDesktopNotificationClick);
#endif
    bindMethod("startSpeechInput", &DRTTestRunner::startSpeechInput);
    bindMethod("testRepaint", &DRTTestRunner::testRepaint);
    bindMethod("waitForPolicyDelegate", &DRTTestRunner::waitForPolicyDelegate);
    bindMethod("waitUntilDone", &DRTTestRunner::waitUntilDone);
    bindMethod("windowCount", &DRTTestRunner::windowCount);
    bindMethod("setTextDirection", &DRTTestRunner::setTextDirection);
    bindMethod("setImagesAllowed", &DRTTestRunner::setImagesAllowed);
    bindMethod("setScriptsAllowed", &DRTTestRunner::setScriptsAllowed);
    bindMethod("setStorageAllowed", &DRTTestRunner::setStorageAllowed);
    bindMethod("setPluginsAllowed", &DRTTestRunner::setPluginsAllowed);

    // The following are stubs.
    bindMethod("abortModal", &DRTTestRunner::abortModal);
    bindMethod("accessStoredWebScriptObject", &DRTTestRunner::accessStoredWebScriptObject);
    bindMethod("addDisallowedURL", &DRTTestRunner::addDisallowedURL);
    bindMethod("applicationCacheDiskUsageForOrigin", &DRTTestRunner::applicationCacheDiskUsageForOrigin);
    bindMethod("callShouldCloseOnWebView", &DRTTestRunner::callShouldCloseOnWebView);
    bindMethod("clearAllApplicationCaches", &DRTTestRunner::clearAllApplicationCaches);
    bindMethod("clearApplicationCacheForOrigin", &DRTTestRunner::clearApplicationCacheForOrigin);
    bindMethod("clearBackForwardList", &DRTTestRunner::clearBackForwardList);
    bindMethod("dumpAsWebArchive", &DRTTestRunner::dumpAsWebArchive);
    bindMethod("keepWebHistory", &DRTTestRunner::keepWebHistory);
    bindMethod("objCClassNameOf", &DRTTestRunner::objCClassNameOf);
    bindMethod("setApplicationCacheOriginQuota", &DRTTestRunner::setApplicationCacheOriginQuota);
    bindMethod("setCallCloseOnWebViews", &DRTTestRunner::setCallCloseOnWebViews);
    bindMethod("setMainFrameIsFirstResponder", &DRTTestRunner::setMainFrameIsFirstResponder);
    bindMethod("setPrivateBrowsingEnabled", &DRTTestRunner::setPrivateBrowsingEnabled);
    bindMethod("setUseDashboardCompatibilityMode", &DRTTestRunner::setUseDashboardCompatibilityMode);
    bindMethod("storeWebScriptObject", &DRTTestRunner::storeWebScriptObject);
    bindMethod("deleteAllLocalStorage", &DRTTestRunner::deleteAllLocalStorage);
    bindMethod("localStorageDiskUsageForOrigin", &DRTTestRunner::localStorageDiskUsageForOrigin);
    bindMethod("originsWithLocalStorage", &DRTTestRunner::originsWithLocalStorage);
    bindMethod("deleteLocalStorageForOrigin", &DRTTestRunner::deleteLocalStorageForOrigin);
    bindMethod("observeStorageTrackerNotifications", &DRTTestRunner::observeStorageTrackerNotifications);
    bindMethod("syncLocalStorage", &DRTTestRunner::syncLocalStorage);
    bindMethod("setShouldStayOnPageAfterHandlingBeforeUnload", &DRTTestRunner::setShouldStayOnPageAfterHandlingBeforeUnload);
    bindMethod("enableFixedLayoutMode", &DRTTestRunner::enableFixedLayoutMode);
    bindMethod("setFixedLayoutSize", &DRTTestRunner::setFixedLayoutSize);
    bindMethod("selectionAsMarkup", &DRTTestRunner::selectionAsMarkup);
    bindMethod("setHasCustomFullScreenBehavior", &DRTTestRunner::setHasCustomFullScreenBehavior);
    bindMethod("textSurroundingNode", &DRTTestRunner::textSurroundingNode);

    // The fallback method is called when an unknown method is invoked.
    bindFallbackMethod(&DRTTestRunner::fallbackMethod);

    // Shared properties.
    // globalFlag is used by a number of layout tests in
    // LayoutTests\http\tests\security\dataURL.
    bindProperty("globalFlag", &m_globalFlag);
    // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
    bindProperty("webHistoryItemCount", &m_webHistoryItemCount);
    bindProperty("titleTextDirection", &m_titleTextDirection);
    bindProperty("platformName", &m_platformName);
    bindProperty("interceptPostMessage", &m_interceptPostMessage);
    bindProperty("workerThreadCount", &DRTTestRunner::workerThreadCount);
    bindMethod("sendWebIntentResponse", &DRTTestRunner::sendWebIntentResponse);
    bindMethod("deliverWebIntent", &DRTTestRunner::deliverWebIntent);
}

DRTTestRunner::~DRTTestRunner()
{
}

DRTTestRunner::WorkQueue::~WorkQueue()
{
    reset();
}

void DRTTestRunner::WorkQueue::processWorkSoon()
{
    if (m_controller->m_shell->webViewHost()->topLoadingFrame())
        return;

    if (!m_queue.isEmpty()) {
        // We delay processing queued work to avoid recursion problems.
        postTask(new WorkQueueTask(this));
    } else if (!m_controller->m_waitUntilDone)
        m_controller->m_shell->testFinished();
}

void DRTTestRunner::WorkQueue::processWork()
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

void DRTTestRunner::WorkQueue::reset()
{
    m_frozen = false;
    while (!m_queue.isEmpty())
        delete m_queue.takeFirst();
}

void DRTTestRunner::WorkQueue::addWork(WorkItem* work)
{
    if (m_frozen) {
        delete work;
        return;
    }
    m_queue.append(work);
}

void DRTTestRunner::dumpAsText(const CppArgumentList& arguments, CppVariant* result)
{
    m_dumpAsText = true;
    m_generatePixelResults = false;

    // Optional paramater, describing whether it's allowed to dump pixel results in dumpAsText mode.
    if (arguments.size() > 0 && arguments[0].isBool())
        m_generatePixelResults = arguments[0].value.boolValue;

    result->setNull();
}

void DRTTestRunner::dumpDatabaseCallbacks(const CppArgumentList&, CppVariant* result)
{
    // Do nothing; we don't use this flag anywhere for now
    result->setNull();
}

void DRTTestRunner::dumpEditingCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpEditingCallbacks = true;
    result->setNull();
}

void DRTTestRunner::dumpBackForwardList(const CppArgumentList&, CppVariant* result)
{
    m_dumpBackForwardList = true;
    result->setNull();
}

void DRTTestRunner::dumpFrameLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpFrameLoadCallbacks = true;
    result->setNull();
}

void DRTTestRunner::dumpProgressFinishedCallback(const CppArgumentList&, CppVariant* result)
{
    m_dumpProgressFinishedCallback = true;
    result->setNull();
}

void DRTTestRunner::dumpUserGestureInFrameLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpUserGestureInFrameLoadCallbacks = true;
    result->setNull();
}

void DRTTestRunner::dumpResourceLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceLoadCallbacks = true;
    result->setNull();
}

void DRTTestRunner::dumpResourceResponseMIMETypes(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceResponseMIMETypes = true;
    result->setNull();
}

void DRTTestRunner::dumpChildFrameScrollPositions(const CppArgumentList&, CppVariant* result)
{
    m_dumpChildFrameScrollPositions = true;
    result->setNull();
}

void DRTTestRunner::dumpChildFramesAsText(const CppArgumentList&, CppVariant* result)
{
    m_dumpChildFramesAsText = true;
    result->setNull();
}

void DRTTestRunner::dumpWindowStatusChanges(const CppArgumentList&, CppVariant* result)
{
    m_dumpWindowStatusChanges = true;
    result->setNull();
}

void DRTTestRunner::dumpTitleChanges(const CppArgumentList&, CppVariant* result)
{
    m_dumpTitleChanges = true;
    result->setNull();
}

void DRTTestRunner::dumpPermissionClientCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpPermissionClientCallbacks = true;
    result->setNull();
}

void DRTTestRunner::dumpCreateView(const CppArgumentList&, CppVariant* result)
{
    m_dumpCreateView = true;
    result->setNull();
}

void DRTTestRunner::setAcceptsEditing(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_acceptsEditing = arguments[0].value.boolValue;
    result->setNull();
}

void DRTTestRunner::waitUntilDone(const CppArgumentList&, CppVariant* result)
{
    if (!webkit_support::BeingDebugged())
        postDelayedTask(new NotifyDoneTimedOutTask(this), m_shell->layoutTestTimeout());
    m_waitUntilDone = true;
    result->setNull();
}

void DRTTestRunner::notifyDone(const CppArgumentList&, CppVariant* result)
{
    // Test didn't timeout. Kill the timeout timer.
    m_taskList.revokeAll();

    completeNotifyDone(false);
    result->setNull();
}

void DRTTestRunner::completeNotifyDone(bool isTimeout)
{
    if (m_waitUntilDone && !m_shell->webViewHost()->topLoadingFrame() && m_workQueue.isEmpty()) {
        if (isTimeout)
            m_shell->testTimedOut();
        else
            m_shell->testFinished();
    }
    m_waitUntilDone = false;
}

class WorkItemBackForward : public DRTTestRunner::WorkItem {
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

void DRTTestRunner::queueBackNavigation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isNumber())
        m_workQueue.addWork(new WorkItemBackForward(-arguments[0].toInt32()));
    result->setNull();
}

void DRTTestRunner::queueForwardNavigation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isNumber())
        m_workQueue.addWork(new WorkItemBackForward(arguments[0].toInt32()));
    result->setNull();
}

class WorkItemReload : public DRTTestRunner::WorkItem {
public:
    bool run(TestShell* shell)
    {
        shell->reload();
        return true;
    }
};

void DRTTestRunner::queueReload(const CppArgumentList&, CppVariant* result)
{
    m_workQueue.addWork(new WorkItemReload);
    result->setNull();
}

class WorkItemLoadingScript : public DRTTestRunner::WorkItem {
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

class WorkItemNonLoadingScript : public DRTTestRunner::WorkItem {
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

void DRTTestRunner::queueLoadingScript(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString())
        m_workQueue.addWork(new WorkItemLoadingScript(arguments[0].toString()));
    result->setNull();
}

void DRTTestRunner::queueNonLoadingScript(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString())
        m_workQueue.addWork(new WorkItemNonLoadingScript(arguments[0].toString()));
    result->setNull();
}

class WorkItemLoad : public DRTTestRunner::WorkItem {
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

void DRTTestRunner::queueLoad(const CppArgumentList& arguments, CppVariant* result)
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

class WorkItemLoadHTMLString : public DRTTestRunner::WorkItem  {
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

void DRTTestRunner::queueLoadHTMLString(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::objCIdentityIsEqual(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 2) {
        // This is the best we can do to return an error.
        result->setNull();
        return;
    }
    result->set(arguments[0].isEqual(arguments[1]));
}

void DRTTestRunner::reset()
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
        m_shell->webView()->setDeviceScaleFactor(1);
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
#if OS(LINUX) || OS(ANDROID)
    WebFontRendering::setSubpixelPositioning(false);
#endif
}

void DRTTestRunner::locationChangeDone()
{
    m_webHistoryItemCount.set(m_shell->navigationEntryCount());

    // No more new work after the first complete load.
    m_workQueue.setFrozen(true);

    if (!m_waitUntilDone)
        m_workQueue.processWorkSoon();
}

void DRTTestRunner::policyDelegateDone()
{
    ASSERT(m_waitUntilDone);
    m_shell->testFinished();
    m_waitUntilDone = false;
}

void DRTTestRunner::setCanOpenWindows(const CppArgumentList&, CppVariant* result)
{
    m_canOpenWindows = true;
    result->setNull();
}

void DRTTestRunner::setTabKeyCyclesThroughElements(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->setTabKeyCyclesThroughElements(arguments[0].toBoolean());
    result->setNull();
}

void DRTTestRunner::windowCount(const CppArgumentList&, CppVariant* result)
{
    result->set(static_cast<int>(m_shell->windowCount()));
}

void DRTTestRunner::setCloseRemainingWindowsWhenComplete(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_closeRemainingWindows = arguments[0].value.boolValue;
    result->setNull();
}

void DRTTestRunner::setAlwaysAcceptCookies(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0)
        webkit_support::SetAcceptAllCookies(cppVariantToBool(arguments[0]));
    result->setNull();
}

void DRTTestRunner::setAsynchronousSpellCheckingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setAsynchronousSpellCheckingEnabled(cppVariantToBool(arguments[0]));
    result->setNull();
}

void DRTTestRunner::showWebInspector(const CppArgumentList&, CppVariant* result)
{
    m_shell->showDevTools();
    result->setNull();
}

void DRTTestRunner::closeWebInspector(const CppArgumentList& args, CppVariant* result)
{
    m_shell->closeDevTools();
    result->setNull();
}

void DRTTestRunner::setWindowIsKey(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->setFocus(m_shell->webView(), arguments[0].value.boolValue);
    result->setNull();
}

void DRTTestRunner::setUserStyleSheetEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->userStyleSheetLocation = arguments[0].value.boolValue ? m_userStyleSheetLocation : WebURL();
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::setUserStyleSheetLocation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        m_userStyleSheetLocation = webkit_support::LocalFileToDataURL(
            webkit_support::RewriteLayoutTestsURL(arguments[0].toString()));
        m_shell->preferences()->userStyleSheetLocation = m_userStyleSheetLocation;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::setAuthorAndUserStylesEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->authorAndUserStylesEnabled = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::execCommand(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::isCommandEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() <= 0 || !arguments[0].isString()) {
        result->setNull();
        return;
    }

    std::string command = arguments[0].toString();
    bool rv = m_shell->webView()->focusedFrame()->isCommandEnabled(WebString::fromUTF8(command));
    result->set(rv);
}

void DRTTestRunner::setPopupBlockingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        bool blockPopups = arguments[0].toBoolean();
        m_shell->preferences()->javaScriptCanOpenWindowsAutomatically = !blockPopups;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::setImagesAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setImagesAllowed(arguments[0].toBoolean());
    result->setNull();
}

void DRTTestRunner::setScriptsAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setScriptsAllowed(arguments[0].toBoolean());
    result->setNull();
}

void DRTTestRunner::setStorageAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setStorageAllowed(arguments[0].toBoolean());
    result->setNull();
}

void DRTTestRunner::setPluginsAllowed(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setPluginsAllowed(arguments[0].toBoolean());
    result->setNull();
}

void DRTTestRunner::setUseDashboardCompatibilityMode(const CppArgumentList&, CppVariant* result)
{
    // We have no need to support Dashboard Compatibility Mode (mac-only)
    result->setNull();
}

void DRTTestRunner::clearAllApplicationCaches(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support application cache quotas.
    result->setNull();
}

void DRTTestRunner::clearApplicationCacheForOrigin(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support deleting all application cache for an origin.
    result->setNull();
}

void DRTTestRunner::setApplicationCacheOriginQuota(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support application cache quotas.
    result->setNull();
}

void DRTTestRunner::originsWithApplicationCache(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support getting origins that have application caches.
    result->setNull();
}

void DRTTestRunner::applicationCacheDiskUsageForOrigin(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement to support getting disk usage by all application cache for an origin.
    result->setNull();
}

void DRTTestRunner::setScrollbarPolicy(const CppArgumentList&, CppVariant* result)
{
    // FIXME: implement.
    // Currently only has a non-null implementation on QT.
    result->setNull();
}

void DRTTestRunner::setCustomPolicyDelegate(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::waitForPolicyDelegate(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->waitForPolicyDelegate();
    m_waitUntilDone = true;
    result->setNull();
}

void DRTTestRunner::setWillSendRequestClearHeader(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        string header = arguments[0].toString();
        if (!header.empty())
            m_shell->webViewHost()->addClearHeader(String::fromUTF8(header.c_str()));
    }
    result->setNull();
}

void DRTTestRunner::setWillSendRequestReturnsNullOnRedirect(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setBlockRedirects(arguments[0].value.boolValue);
    result->setNull();
}

void DRTTestRunner::setWillSendRequestReturnsNull(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setRequestReturnNull(arguments[0].value.boolValue);
    result->setNull();
}

void DRTTestRunner::pathToLocalResource(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::addFileToPasteboardOnDrag(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
    m_shouldAddFileToPasteboard = true;
}

void DRTTestRunner::setStopProvisionalFrameLoads(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
    m_stopProvisionalFrameLoads = true;
}

void DRTTestRunner::setSmartInsertDeleteEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setSmartInsertDeleteEnabled(arguments[0].value.boolValue);
    result->setNull();
}

void DRTTestRunner::setSelectTrailingWhitespaceEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webViewHost()->setSelectTrailingWhitespaceEnabled(arguments[0].value.boolValue);
    result->setNull();
}

bool DRTTestRunner::pauseAnimationAtTimeOnElementWithId(const WebString& animationName, double time, const WebString& elementId)
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

bool DRTTestRunner::pauseTransitionAtTimeOnElementWithId(const WebString& propertyName, double time, const WebString& elementId)
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

bool DRTTestRunner::elementDoesAutoCompleteForElementWithId(const WebString& elementId)
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

int DRTTestRunner::numberOfActiveAnimations()
{
    WebFrame* webFrame = m_shell->webView()->mainFrame();
    if (!webFrame)
        return -1;

    WebAnimationController* controller = webFrame->animationController();
    if (!controller)
        return -1;

    return controller->numberOfActiveAnimations();
}

void DRTTestRunner::pauseAnimationAtTimeOnElementWithId(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(false);
    if (arguments.size() > 2 && arguments[0].isString() && arguments[1].isNumber() && arguments[2].isString()) {
        WebString animationName = cppVariantToWebString(arguments[0]);
        double time = arguments[1].toDouble();
        WebString elementId = cppVariantToWebString(arguments[2]);
        result->set(pauseAnimationAtTimeOnElementWithId(animationName, time, elementId));
    }
}

void DRTTestRunner::pauseTransitionAtTimeOnElementWithId(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(false);
    if (arguments.size() > 2 && arguments[0].isString() && arguments[1].isNumber() && arguments[2].isString()) {
        WebString propertyName = cppVariantToWebString(arguments[0]);
        double time = arguments[1].toDouble();
        WebString elementId = cppVariantToWebString(arguments[2]);
        result->set(pauseTransitionAtTimeOnElementWithId(propertyName, time, elementId));
    }
}

void DRTTestRunner::elementDoesAutoCompleteForElementWithId(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
    WebString elementId = cppVariantToWebString(arguments[0]);
    result->set(elementDoesAutoCompleteForElementWithId(elementId));
}

void DRTTestRunner::enableAutoResizeMode(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::disableAutoResizeMode(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::numberOfActiveAnimations(const CppArgumentList&, CppVariant* result)
{
    result->set(numberOfActiveAnimations());
}

void DRTTestRunner::disableImageLoading(const CppArgumentList&, CppVariant* result)
{
    m_shell->preferences()->loadsImagesAutomatically = false;
    m_shell->applyPreferences();
    result->setNull();
}

void DRTTestRunner::setIconDatabaseEnabled(const CppArgumentList&, CppVariant* result)
{
    // We don't use the WebKit icon database.
    result->setNull();
}

void DRTTestRunner::callShouldCloseOnWebView(const CppArgumentList&, CppVariant* result)
{
    result->set(m_shell->webView()->dispatchBeforeUnloadEvent());
}

#if ENABLE(NOTIFICATIONS)
void DRTTestRunner::grantDesktopNotificationPermission(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::simulateDesktopNotificationClick(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setDomainRelaxationForbiddenForURLScheme(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 2 || !arguments[0].isBool() || !arguments[1].isString())
        return;
    m_shell->webView()->setDomainRelaxationForbidden(cppVariantToBool(arguments[0]), cppVariantToWebString(arguments[1]));
}

void DRTTestRunner::setDeferMainResourceDataLoad(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1)
        m_deferMainResourceDataLoad = cppVariantToBool(arguments[0]);
}

//
// Unimplemented stubs
//

void DRTTestRunner::dumpAsWebArchive(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::setMainFrameIsFirstResponder(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::dumpSelectionRect(const CppArgumentList& arguments, CppVariant* result)
{
    m_dumpSelectionRect = true;
    result->setNull();
}

void DRTTestRunner::display(const CppArgumentList& arguments, CppVariant* result)
{
    WebViewHost* host = m_shell->webViewHost();
    const WebKit::WebSize& size = m_shell->webView()->size();
    WebRect rect(0, 0, size.width, size.height);
    host->updatePaintRect(rect);
    host->paintInvalidatedRegion();
    host->displayRepaintMask();
    result->setNull();
}

void DRTTestRunner::displayInvalidatedRegion(const CppArgumentList& arguments, CppVariant* result)
{
    WebViewHost* host = m_shell->webViewHost();
    host->paintInvalidatedRegion();
    host->displayRepaintMask();
    result->setNull();
}

void DRTTestRunner::testRepaint(const CppArgumentList&, CppVariant* result)
{
    m_testRepaint = true;
    result->setNull();
}

void DRTTestRunner::repaintSweepHorizontally(const CppArgumentList&, CppVariant* result)
{
    m_sweepHorizontally = true;
    result->setNull();
}

void DRTTestRunner::clearBackForwardList(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::keepWebHistory(const CppArgumentList& arguments,  CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::storeWebScriptObject(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::accessStoredWebScriptObject(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::objCClassNameOf(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::addDisallowedURL(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::setCallCloseOnWebViews(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::setPrivateBrowsingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void DRTTestRunner::setJavaScriptCanAccessClipboard(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->javaScriptCanAccessClipboard = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::setXSSAuditorEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->XSSAuditorEnabled = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::evaluateScriptInIsolatedWorldAndReturnValue(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::evaluateScriptInIsolatedWorld(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() >= 2 && arguments[0].isNumber() && arguments[1].isString()) {
        WebScriptSource source(cppVariantToWebString(arguments[1]));
        // This relies on the iframe focusing itself when it loads. This is a bit
        // sketchy, but it seems to be what other tests do.
        m_shell->webView()->focusedFrame()->executeScriptInIsolatedWorld(arguments[0].toInt32(), &source, 1, 1);
    }
    result->setNull();
}

void DRTTestRunner::setIsolatedWorldSecurityOrigin(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;

    m_shell->webView()->focusedFrame()->setIsolatedWorldSecurityOrigin(
        arguments[0].toInt32(),
        WebSecurityOrigin::createFromString(cppVariantToWebString(arguments[1])));
}

void DRTTestRunner::setAllowUniversalAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->allowUniversalAccessFromFileURLs = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::setAllowDisplayOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setDisplayingInsecureContentAllowed(arguments[0].toBoolean());

    result->setNull();
}

void DRTTestRunner::setAllowFileAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->allowFileAccessFromFileURLs = arguments[0].value.boolValue;
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::setAllowRunningOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setRunningInsecureContentAllowed(arguments[0].value.boolValue);

    result->setNull();
}

// Need these conversions because the format of the value for booleans
// may vary - for example, on mac "1" and "0" are used for boolean.
bool DRTTestRunner::cppVariantToBool(const CppVariant& value)
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

int32_t DRTTestRunner::cppVariantToInt32(const CppVariant& value)
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

WebString DRTTestRunner::cppVariantToWebString(const CppVariant& value)
{
    if (!value.isString()) {
        logErrorToConsole("Invalid value for preference. Expected string value.");
        return WebString();
    }
    return WebString::fromUTF8(value.toString());
}

Vector<WebString> DRTTestRunner::cppVariantToWebStringArray(const CppVariant& value)
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

void DRTTestRunner::overridePreference(const CppArgumentList& arguments, CppVariant* result)
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
    else if (key == "WebKitWebAudioEnabled") {
        ASSERT(cppVariantToBool(value));
    } else {
        string message("Invalid name for preference: ");
        message.append(key);
        logErrorToConsole(message);
    }
    m_shell->applyPreferences();
}

void DRTTestRunner::fallbackMethod(const CppArgumentList&, CppVariant* result)
{
    printf("CONSOLE MESSAGE: JavaScript ERROR: unknown method called on DRTTestRunner\n");
    result->setNull();
}

void DRTTestRunner::addOriginAccessWhitelistEntry(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::removeOriginAccessWhitelistEntry(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::clearAllDatabases(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    webkit_support::ClearAllDatabases();
}

void DRTTestRunner::setDatabaseQuota(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if ((arguments.size() >= 1) && arguments[0].isNumber())
        webkit_support::SetDatabaseQuota(arguments[0].toInt32());
}

void DRTTestRunner::setPOSIXLocale(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() == 1 && arguments[0].isString())
        setlocale(LC_ALL, arguments[0].toString().c_str());
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

void DRTTestRunner::setPrinting(const CppArgumentList& arguments, CppVariant* result)
{
    setIsPrinting(true);
    result->setNull();
}

void DRTTestRunner::pageSizeAndMarginsInPixels(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::hasCustomPageSizeStyle(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::pageProperty(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::numberOfPages(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    int pageWidthInPixels = 0;
    int pageHeightInPixels = 0;
    if (!parsePageSizeParameters(arguments, 0, &pageWidthInPixels, &pageHeightInPixels))
        return;

    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    WebPrintParams printParams(WebSize(pageWidthInPixels, pageHeightInPixels));
    int numberOfPages = frame->printBegin(printParams);
    frame->printEnd();
    result->set(numberOfPages);
}

void DRTTestRunner::numberOfPendingGeolocationPermissionRequests(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    Vector<WebViewHost*> windowList = m_shell->windowList();
    int numberOfRequests = 0;
    for (size_t i = 0; i < windowList.size(); i++)
        numberOfRequests += windowList[i]->geolocationClientMock()->numberOfPendingPermissionRequests();
    result->set(numberOfRequests);
}

void DRTTestRunner::logErrorToConsole(const std::string& text)
{
    m_shell->webViewHost()->didAddMessageToConsole(
        WebConsoleMessage(WebConsoleMessage::LevelError, WebString::fromUTF8(text)),
        WebString(), 0);
}

void DRTTestRunner::evaluateInWebInspector(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;
    m_shell->drtDevToolsAgent()->evaluateInWebInspector(arguments[0].toInt32(), arguments[1].toString());
}

void DRTTestRunner::forceRedSelectionColors(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    m_shell->webView()->setSelectionColors(0xffee0000, 0xff00ee00, 0xff000000, 0xffc0c0c0);
}

void DRTTestRunner::addUserScript(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isString() || !arguments[1].isBool() || !arguments[2].isBool())
        return;
    WebView::addUserScript(
        cppVariantToWebString(arguments[0]), WebVector<WebString>(),
        arguments[1].toBoolean() ? WebView::UserScriptInjectAtDocumentStart : WebView::UserScriptInjectAtDocumentEnd,
        arguments[2].toBoolean() ? WebView::UserContentInjectInAllFrames : WebView::UserContentInjectInTopFrameOnly);
}

void DRTTestRunner::addUserStyleSheet(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setMockDeviceOrientation(const CppArgumentList& arguments, CppVariant* result)
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

    // Note that we only call setOrientation on the main page's mock since this is all that the
    // tests require. If necessary, we could get a list of WebViewHosts from the TestShell and
    // call setOrientation on each DeviceOrientationClientMock.
    m_shell->webViewHost()->deviceOrientationClientMock()->setOrientation(orientation);
}

// FIXME: For greater test flexibility, we should be able to set each page's geolocation mock individually.
// https://bugs.webkit.org/show_bug.cgi?id=52368
void DRTTestRunner::setGeolocationPermission(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;
    Vector<WebViewHost*> windowList = m_shell->windowList();
    for (size_t i = 0; i < windowList.size(); i++)
        windowList[i]->geolocationClientMock()->setPermission(arguments[0].toBoolean());
}

void DRTTestRunner::setMockGeolocationPosition(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isNumber() || !arguments[1].isNumber() || !arguments[2].isNumber())
        return;
    Vector<WebViewHost*> windowList = m_shell->windowList();
    for (size_t i = 0; i < windowList.size(); i++)
        windowList[i]->geolocationClientMock()->setPosition(arguments[0].toDouble(), arguments[1].toDouble(), arguments[2].toDouble());
}

void DRTTestRunner::setMockGeolocationError(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;
    Vector<WebViewHost*> windowList = m_shell->windowList();
    for (size_t i = 0; i < windowList.size(); i++)
        windowList[i]->geolocationClientMock()->setError(arguments[0].toInt32(), cppVariantToWebString(arguments[1]));
}

void DRTTestRunner::abortModal(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

#if ENABLE(INPUT_SPEECH)
void DRTTestRunner::addMockSpeechInputResult(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isString() || !arguments[1].isNumber() || !arguments[2].isString())
        return;

    if (MockWebSpeechInputController* controller = m_shell->webViewHost()->speechInputControllerMock())
        controller->addMockRecognitionResult(cppVariantToWebString(arguments[0]), arguments[1].toDouble(), cppVariantToWebString(arguments[2]));
}

void DRTTestRunner::setMockSpeechInputDumpRect(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;

    if (MockWebSpeechInputController* controller = m_shell->webViewHost()->speechInputControllerMock())
        controller->setDumpRect(arguments[0].value.boolValue);
}
#endif

#if ENABLE(SCRIPTED_SPEECH)
void DRTTestRunner::addMockSpeechRecognitionResult(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isString() || !arguments[1].isNumber())
        return;

    if (MockWebSpeechRecognizer* recognizer = m_shell->webViewHost()->mockSpeechRecognizer())
        recognizer->addMockResult(cppVariantToWebString(arguments[0]), arguments[1].toDouble());
}

void DRTTestRunner::setMockSpeechRecognitionError(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;

    if (MockWebSpeechRecognizer* recognizer = m_shell->webViewHost()->mockSpeechRecognizer())
        recognizer->setError(arguments[0].toInt32(), cppVariantToWebString(arguments[1]));
}

void DRTTestRunner::wasMockSpeechRecognitionAborted(const CppArgumentList&, CppVariant* result)
{
    result->set(false);
    if (MockWebSpeechRecognizer* recognizer = m_shell->webViewHost()->mockSpeechRecognizer())
        result->set(recognizer->wasAborted());
}
#endif

void DRTTestRunner::startSpeechInput(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::layerTreeAsText(const CppArgumentList& args, CppVariant* result)
{
    result->set(m_shell->webView()->mainFrame()->layerTreeAsText(m_showDebugLayerTree).utf8());
}

void DRTTestRunner::loseCompositorContext(const CppArgumentList& args, CppVariant*)
{
    int numTimes;
    if (args.size() == 1 || !args[0].isNumber())
        numTimes = 1;
    else
        numTimes = args[0].toInt32();
    m_shell->webView()->loseCompositorContext(numTimes);
}

void DRTTestRunner::markerTextForListItem(const CppArgumentList& args, CppVariant* result)
{
    WebElement element;
    if (!WebBindings::getElement(args[0].value.objectValue, &element))
        result->setNull();
    else
        result->set(element.document().frame()->markerTextForListItem(element).utf8());
}

void DRTTestRunner::findString(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setMinimumTimerInterval(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isNumber())
        return;
    m_shell->webView()->settings()->setMinimumTimerInterval(arguments[0].toDouble());
}

void DRTTestRunner::setAutofilled(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setValueForUser(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::deleteAllLocalStorage(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void DRTTestRunner::localStorageDiskUsageForOrigin(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void DRTTestRunner::originsWithLocalStorage(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void DRTTestRunner::deleteLocalStorageForOrigin(const CppArgumentList& arguments, CppVariant*)
{
    // Not Implemented
}

void DRTTestRunner::observeStorageTrackerNotifications(const CppArgumentList&, CppVariant*)
{
    // Not Implemented
}

void DRTTestRunner::syncLocalStorage(const CppArgumentList&, CppVariant*)
{
    // Not Implemented
}

void DRTTestRunner::setShouldStayOnPageAfterHandlingBeforeUnload(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1 && arguments[0].isBool())
        m_shouldStayOnPageAfterHandlingBeforeUnload = arguments[0].toBoolean();

    result->setNull();
}

void DRTTestRunner::enableFixedLayoutMode(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  1 || !arguments[0].isBool())
        return;
    bool enableFixedLayout = arguments[0].toBoolean();
    m_shell->webView()->enableFixedLayoutMode(enableFixedLayout);
}

void DRTTestRunner::setFixedLayoutSize(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;
    int width = arguments[0].toInt32();
    int height = arguments[1].toInt32();
    m_shell->webView()->setFixedLayoutSize(WebSize(width, height));
}

void DRTTestRunner::selectionAsMarkup(const CppArgumentList& arguments, CppVariant* result)
{
    result->set(m_shell->webView()->mainFrame()->selectionAsMarkup().utf8());
}

void DRTTestRunner::workerThreadCount(CppVariant* result)
{
    result->set(static_cast<int>(WebWorkerInfo::dedicatedWorkerCount()));
}

void DRTTestRunner::sendWebIntentResponse(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::deliverWebIntent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() <  3)
        return;

    v8::HandleScope scope;
    v8::Local<v8::Context> ctx = m_shell->webView()->mainFrame()->mainWorldScriptContext();
    result->set(m_shell->webView()->mainFrame()->selectionAsMarkup().utf8());
    v8::Context::Scope cscope(ctx);

    WebString action = cppVariantToWebString(arguments[0]);
    WebString type = cppVariantToWebString(arguments[1]);
    WebKit::WebCString data = cppVariantToWebString(arguments[2]).utf8();
    WebSerializedScriptValue serializedData = WebSerializedScriptValue::serialize(
        v8::String::New(data.data(), data.length()));

    WebIntent intent = WebIntent::create(action, type, serializedData.toString(), WebVector<WebString>(), WebVector<WebString>());

    m_shell->webView()->mainFrame()->deliverIntent(intent, 0, m_intentClient.get());
}

void DRTTestRunner::setTextSubpixelPositioning(const CppArgumentList& arguments, CppVariant* result)
{
#if OS(LINUX) || OS(ANDROID)
    // Since FontConfig doesn't provide a variable to control subpixel positioning, we'll fall back
    // to setting it globally for all fonts.
    if (arguments.size() > 0 && arguments[0].isBool())
        WebFontRendering::setSubpixelPositioning(arguments[0].value.boolValue);
#endif
    result->setNull();
}

class InvokeCallbackTask : public MethodTask<DRTTestRunner> {
public:
    InvokeCallbackTask(DRTTestRunner* object, PassOwnArrayPtr<CppVariant> callbackArguments, uint32_t numberOfArguments)
        : MethodTask<DRTTestRunner>(object)
        , m_callbackArguments(callbackArguments)
        , m_numberOfArguments(numberOfArguments)
    {
    }

    virtual void runIfValid()
    {
        CppVariant invokeResult;
        m_callbackArguments[0].invokeDefault(m_callbackArguments.get(), m_numberOfArguments, invokeResult);
    }

private:
    OwnArrayPtr<CppVariant> m_callbackArguments;
    uint32_t m_numberOfArguments;
};

void DRTTestRunner::setBackingScaleFactor(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isObject())
        return;
    
    float value = arguments[0].value.doubleValue;
    m_shell->webView()->setDeviceScaleFactor(value);

    OwnArrayPtr<CppVariant> callbackArguments = adoptArrayPtr(new CppVariant[1]);
    callbackArguments[0].set(arguments[1]);
    result->setNull();
    postTask(new InvokeCallbackTask(this, callbackArguments.release(), 1));
}

void DRTTestRunner::setPluginsEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool()) {
        m_shell->preferences()->pluginsEnabled = arguments[0].toBoolean();
        m_shell->applyPreferences();
    }
    result->setNull();
}

void DRTTestRunner::resetPageVisibility(const CppArgumentList& arguments, CppVariant* result)
{
    m_shell->webView()->setVisibilityState(WebPageVisibilityStateVisible, true);
}

void DRTTestRunner::setPageVisibility(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setAutomaticLinkDetectionEnabled(bool)
{
    // Not Implemented
}

void DRTTestRunner::setTextDirection(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setAudioData(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setHasCustomFullScreenBehavior(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() <  1 || !arguments[0].isBool())
        return;
    m_hasCustomFullScreenBehavior = arguments[0].toBoolean();
}

#if ENABLE(POINTER_LOCK)
void DRTTestRunner::didLosePointerLock(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->didLosePointerLock();
    result->setNull();
}

void DRTTestRunner::setPointerLockWillFailAsynchronously(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->setPointerLockWillFailAsynchronously();
    result->setNull();
}

void DRTTestRunner::setPointerLockWillFailSynchronously(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->setPointerLockWillFailSynchronously();
    result->setNull();
}
#endif

void DRTTestRunner::textSurroundingNode(const CppArgumentList& arguments, CppVariant* result)
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
