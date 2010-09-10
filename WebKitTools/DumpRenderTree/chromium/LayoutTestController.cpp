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
#include "TestShell.h"
#include "WebViewHost.h"
#include "public/WebAnimationController.h"
#include "public/WebBindings.h"
#include "public/WebConsoleMessage.h"
#include "public/WebDeviceOrientation.h"
#include "public/WebDeviceOrientationClientMock.h"
#include "public/WebDocument.h"
#include "public/WebElement.h"
#include "public/WebFrame.h"
#include "public/WebGeolocationServiceMock.h"
#include "public/WebInputElement.h"
#include "public/WebKit.h"
#include "public/WebNotificationPresenter.h"
#include "public/WebScriptSource.h"
#include "public/WebSecurityPolicy.h"
#include "public/WebSettings.h"
#include "public/WebSize.h"
#include "public/WebSpeechInputControllerMock.h"
#include "public/WebURL.h"
#include "public/WebView.h"
#include "webkit/support/webkit_support.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <wtf/OwnArrayPtr.h>
#endif

using namespace WebCore;
using namespace WebKit;
using namespace std;

LayoutTestController::LayoutTestController(TestShell* shell)
    : m_shell(shell)
    , m_workQueue(this)
{

    // Initialize the map that associates methods of this class with the names
    // they will use when called by JavaScript.  The actual binding of those
    // names to their methods will be done by calling bindToJavaScript() (defined
    // by CppBoundClass, the parent to LayoutTestController).
    bindMethod("dumpAsText", &LayoutTestController::dumpAsText);
    bindMethod("dumpChildFrameScrollPositions", &LayoutTestController::dumpChildFrameScrollPositions);
    bindMethod("dumpChildFramesAsText", &LayoutTestController::dumpChildFramesAsText);
    bindMethod("dumpDatabaseCallbacks", &LayoutTestController::dumpDatabaseCallbacks);
    bindMethod("dumpEditingCallbacks", &LayoutTestController::dumpEditingCallbacks);
    bindMethod("dumpBackForwardList", &LayoutTestController::dumpBackForwardList);
    bindMethod("dumpFrameLoadCallbacks", &LayoutTestController::dumpFrameLoadCallbacks);
    bindMethod("dumpResourceLoadCallbacks", &LayoutTestController::dumpResourceLoadCallbacks);
    bindMethod("dumpStatusCallbacks", &LayoutTestController::dumpWindowStatusChanges);
    bindMethod("dumpTitleChanges", &LayoutTestController::dumpTitleChanges);
    bindMethod("setAcceptsEditing", &LayoutTestController::setAcceptsEditing);
    bindMethod("waitUntilDone", &LayoutTestController::waitUntilDone);
    bindMethod("notifyDone", &LayoutTestController::notifyDone);
    bindMethod("queueReload", &LayoutTestController::queueReload);
    bindMethod("queueLoadingScript", &LayoutTestController::queueLoadingScript);
    bindMethod("queueNonLoadingScript", &LayoutTestController::queueNonLoadingScript);
    bindMethod("queueLoad", &LayoutTestController::queueLoad);
    bindMethod("queueBackNavigation", &LayoutTestController::queueBackNavigation);
    bindMethod("queueForwardNavigation", &LayoutTestController::queueForwardNavigation);
    bindMethod("windowCount", &LayoutTestController::windowCount);
    bindMethod("setCanOpenWindows", &LayoutTestController::setCanOpenWindows);
    bindMethod("setCloseRemainingWindowsWhenComplete", &LayoutTestController::setCloseRemainingWindowsWhenComplete);
    bindMethod("objCIdentityIsEqual", &LayoutTestController::objCIdentityIsEqual);
    bindMethod("setAlwaysAcceptCookies", &LayoutTestController::setAlwaysAcceptCookies);
    bindMethod("showWebInspector", &LayoutTestController::showWebInspector);
    bindMethod("closeWebInspector", &LayoutTestController::closeWebInspector);
    bindMethod("setWindowIsKey", &LayoutTestController::setWindowIsKey);
    bindMethod("setTabKeyCyclesThroughElements", &LayoutTestController::setTabKeyCyclesThroughElements);
    bindMethod("setUserStyleSheetLocation", &LayoutTestController::setUserStyleSheetLocation);
    bindMethod("setUserStyleSheetEnabled", &LayoutTestController::setUserStyleSheetEnabled);
    bindMethod("setAuthorAndUserStylesEnabled", &LayoutTestController::setAuthorAndUserStylesEnabled);
    bindMethod("pathToLocalResource", &LayoutTestController::pathToLocalResource);
    bindMethod("addFileToPasteboardOnDrag", &LayoutTestController::addFileToPasteboardOnDrag);
    bindMethod("execCommand", &LayoutTestController::execCommand);
    bindMethod("isCommandEnabled", &LayoutTestController::isCommandEnabled);
    bindMethod("setPopupBlockingEnabled", &LayoutTestController::setPopupBlockingEnabled);
    bindMethod("setStopProvisionalFrameLoads", &LayoutTestController::setStopProvisionalFrameLoads);
    bindMethod("setSmartInsertDeleteEnabled", &LayoutTestController::setSmartInsertDeleteEnabled);
    bindMethod("setSelectTrailingWhitespaceEnabled", &LayoutTestController::setSelectTrailingWhitespaceEnabled);
    bindMethod("pauseAnimationAtTimeOnElementWithId", &LayoutTestController::pauseAnimationAtTimeOnElementWithId);
    bindMethod("pauseTransitionAtTimeOnElementWithId", &LayoutTestController::pauseTransitionAtTimeOnElementWithId);
    bindMethod("elementDoesAutoCompleteForElementWithId", &LayoutTestController::elementDoesAutoCompleteForElementWithId);
    bindMethod("numberOfActiveAnimations", &LayoutTestController::numberOfActiveAnimations);
    bindMethod("suspendAnimations", &LayoutTestController::suspendAnimations);
    bindMethod("resumeAnimations", &LayoutTestController::resumeAnimations);
    bindMethod("disableImageLoading", &LayoutTestController::disableImageLoading);
    bindMethod("setIconDatabaseEnabled", &LayoutTestController::setIconDatabaseEnabled);
    bindMethod("setCustomPolicyDelegate", &LayoutTestController::setCustomPolicyDelegate);
    bindMethod("setScrollbarPolicy", &LayoutTestController::setScrollbarPolicy);
    bindMethod("waitForPolicyDelegate", &LayoutTestController::waitForPolicyDelegate);
    bindMethod("setWillSendRequestClearHeader", &LayoutTestController::setWillSendRequestClearHeader);
    bindMethod("setWillSendRequestReturnsNullOnRedirect", &LayoutTestController::setWillSendRequestReturnsNullOnRedirect);
    bindMethod("setWillSendRequestReturnsNull", &LayoutTestController::setWillSendRequestReturnsNull);
    bindMethod("addOriginAccessWhitelistEntry", &LayoutTestController::addOriginAccessWhitelistEntry);
    bindMethod("removeOriginAccessWhitelistEntry", &LayoutTestController::removeOriginAccessWhitelistEntry);
    bindMethod("clearAllDatabases", &LayoutTestController::clearAllDatabases);
    bindMethod("setDatabaseQuota", &LayoutTestController::setDatabaseQuota);
    bindMethod("setPOSIXLocale", &LayoutTestController::setPOSIXLocale);
    bindMethod("counterValueForElementById", &LayoutTestController::counterValueForElementById);
    bindMethod("addUserScript", &LayoutTestController::addUserScript);
    bindMethod("addUserStyleSheet", &LayoutTestController::addUserStyleSheet);
    bindMethod("pageNumberForElementById", &LayoutTestController::pageNumberForElementById);
    bindMethod("numberOfPages", &LayoutTestController::numberOfPages);
    bindMethod("dumpSelectionRect", &LayoutTestController::dumpSelectionRect);
    bindMethod("grantDesktopNotificationPermission", &LayoutTestController::grantDesktopNotificationPermission);
    bindMethod("simulateDesktopNotificationClick", &LayoutTestController::simulateDesktopNotificationClick);

    // The following are stubs.
    bindMethod("dumpAsWebArchive", &LayoutTestController::dumpAsWebArchive);
    bindMethod("setMainFrameIsFirstResponder", &LayoutTestController::setMainFrameIsFirstResponder);
    bindMethod("dumpSelectionRect", &LayoutTestController::dumpSelectionRect);
    bindMethod("display", &LayoutTestController::display);
    bindMethod("testRepaint", &LayoutTestController::testRepaint);
    bindMethod("repaintSweepHorizontally", &LayoutTestController::repaintSweepHorizontally);
    bindMethod("clearBackForwardList", &LayoutTestController::clearBackForwardList);
    bindMethod("keepWebHistory", &LayoutTestController::keepWebHistory);
    bindMethod("storeWebScriptObject", &LayoutTestController::storeWebScriptObject);
    bindMethod("accessStoredWebScriptObject", &LayoutTestController::accessStoredWebScriptObject);
    bindMethod("objCClassNameOf", &LayoutTestController::objCClassNameOf);
    bindMethod("addDisallowedURL", &LayoutTestController::addDisallowedURL);
    bindMethod("callShouldCloseOnWebView", &LayoutTestController::callShouldCloseOnWebView);
    bindMethod("setCallCloseOnWebViews", &LayoutTestController::setCallCloseOnWebViews);
    bindMethod("setPrivateBrowsingEnabled", &LayoutTestController::setPrivateBrowsingEnabled);
    bindMethod("setUseDashboardCompatibilityMode", &LayoutTestController::setUseDashboardCompatibilityMode);
    bindMethod("clearAllApplicationCaches", &LayoutTestController::clearAllApplicationCaches);
    bindMethod("setApplicationCacheOriginQuota", &LayoutTestController::setApplicationCacheOriginQuota);

    bindMethod("setJavaScriptCanAccessClipboard", &LayoutTestController::setJavaScriptCanAccessClipboard);
    bindMethod("setXSSAuditorEnabled", &LayoutTestController::setXSSAuditorEnabled);
    bindMethod("evaluateScriptInIsolatedWorld", &LayoutTestController::evaluateScriptInIsolatedWorld);
    bindMethod("overridePreference", &LayoutTestController::overridePreference);
    bindMethod("setAllowUniversalAccessFromFileURLs", &LayoutTestController::setAllowUniversalAccessFromFileURLs);
    bindMethod("setAllowFileAccessFromFileURLs", &LayoutTestController::setAllowFileAccessFromFileURLs);
    bindMethod("setTimelineProfilingEnabled", &LayoutTestController::setTimelineProfilingEnabled);
    bindMethod("evaluateInWebInspector", &LayoutTestController::evaluateInWebInspector);
    bindMethod("forceRedSelectionColors", &LayoutTestController::forceRedSelectionColors);
    bindMethod("setEditingBehavior", &LayoutTestController::setEditingBehavior);

    bindMethod("setMockDeviceOrientation", &LayoutTestController::setMockDeviceOrientation);

    bindMethod("setGeolocationPermission", &LayoutTestController::setGeolocationPermission);
    bindMethod("setMockGeolocationPosition", &LayoutTestController::setMockGeolocationPosition);
    bindMethod("setMockGeolocationError", &LayoutTestController::setMockGeolocationError);
    bindMethod("abortModal", &LayoutTestController::abortModal);
    bindMethod("setMockSpeechInputResult", &LayoutTestController::setMockSpeechInputResult);

    bindMethod("markerTextForListItem", &LayoutTestController::markerTextForListItem);

    // The fallback method is called when an unknown method is invoked.
    bindFallbackMethod(&LayoutTestController::fallbackMethod);

    // Shared properties.
    // globalFlag is used by a number of layout tests in
    // LayoutTests\http\tests\security\dataURL.
    bindProperty("globalFlag", &m_globalFlag);
    // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
    bindProperty("webHistoryItemCount", &m_webHistoryItemCount);
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
    while (!m_queue.isEmpty()) {
        delete m_queue.takeFirst();
    }
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

void LayoutTestController::dumpResourceLoadCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceLoadCallbacks = true;
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
    WorkItemBackForward(int distance) : m_distance(distance) {}
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
    WorkItemLoadingScript(const string& script) : m_script(script) {}
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
    WorkItemNonLoadingScript(const string& script) : m_script(script) {}
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
        , m_target(target) {}
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
        GURL currentURL = m_shell->webView()->mainFrame()->url();
        GURL fullURL = currentURL.Resolve(arguments[0].toString());

        string target = "";
        if (arguments.size() > 1 && arguments[1].isString())
            target = arguments[1].toString();

        m_workQueue.addWork(new WorkItemLoad(fullURL, WebString::fromUTF8(target)));
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
    }
    m_dumpAsText = false;
    m_dumpEditingCallbacks = false;
    m_dumpFrameLoadCallbacks = false;
    m_dumpResourceLoadCallbacks = false;
    m_dumpBackForwardList = false;
    m_dumpChildFrameScrollPositions = false;
    m_dumpChildFramesAsText = false;
    m_dumpWindowStatusChanges = false;
    m_dumpSelectionRect = false;
    m_dumpTitleChanges = false;
    m_generatePixelResults = true;
    m_acceptsEditing = true;
    m_waitUntilDone = false;
    m_canOpenWindows = false;
    m_testRepaint = false;
    m_sweepHorizontally = false;
    m_shouldAddFileToPasteboard = false;
    m_stopProvisionalFrameLoads = false;
    m_globalFlag.set(false);
    m_webHistoryItemCount.set(0);
    m_userStyleSheetLocation = WebURL();

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
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setUserStyleSheetLocation(arguments[0].value.boolValue ? m_userStyleSheetLocation : WebURL());
    result->setNull();
}

void LayoutTestController::setUserStyleSheetLocation(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isString()) {
        m_userStyleSheetLocation = webkit_support::RewriteLayoutTestsURL(arguments[0].toString());
        m_shell->webView()->settings()->setUserStyleSheetLocation(m_userStyleSheetLocation);
    }
    result->setNull();
}

void LayoutTestController::setAuthorAndUserStylesEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setAuthorAndUserStylesEnabled(arguments[0].value.boolValue);
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
        m_shell->webView()->settings()->setJavaScriptCanOpenWindowsAutomatically(!blockPopups);
    }
    result->setNull();
}

void LayoutTestController::setUseDashboardCompatibilityMode(const CppArgumentList&, CppVariant* result)
{
    // We have no need to support Dashboard Compatibility Mode (mac-only)
    result->setNull();
}

void LayoutTestController::clearAllApplicationCaches(const CppArgumentList&, CppVariant* result)
{
    // FIXME: implement to support Application Cache Quotas.
    result->setNull();
}

void LayoutTestController::setApplicationCacheOriginQuota(const CppArgumentList&, CppVariant* result)
{
    // FIXME: implement to support Application Cache Quotas.
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
        OwnArrayPtr<WCHAR> tempPath(new WCHAR[bufferSize]);
        DWORD tempLength = ::GetTempPathW(bufferSize, tempPath.get());
        if (tempLength + url.length() - tempPrefixLength + 1 > bufferSize) {
            bufferSize = tempLength + url.length() - tempPrefixLength + 1;
            tempPath.set(new WCHAR[bufferSize]);
            tempLength = GetTempPathW(bufferSize, tempPath.get());
            ASSERT(tempLength < bufferSize);
        }
        string resultPath(WebString(tempPath.get(), tempLength).utf8());
        resultPath.append(url.substr(tempPrefixLength));
        result->set(resultPath);
        return;
    }
#endif

    // Some layout tests use file://// which we resolve as a UNC path.  Normalize
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

void LayoutTestController::suspendAnimations()
{
    WebFrame* webFrame = m_shell->webView()->mainFrame();
    if (!webFrame)
        return;

    WebAnimationController* controller = webFrame->animationController();
    if (!controller)
        return;

    controller->suspendAnimations();
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

void LayoutTestController::numberOfActiveAnimations(const CppArgumentList&, CppVariant* result)
{
    result->set(numberOfActiveAnimations());
}

void LayoutTestController::suspendAnimations(const CppArgumentList&, CppVariant* result)
{
    suspendAnimations();
    result->setNull();
}

void LayoutTestController::resumeAnimations(const CppArgumentList&, CppVariant* result)
{
    resumeAnimations();
    result->setNull();
}

void LayoutTestController::disableImageLoading(const CppArgumentList&, CppVariant* result)
{
    m_shell->webView()->settings()->setLoadsImagesAutomatically(false);
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

void LayoutTestController::grantDesktopNotificationPermission(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
    m_shell->notificationPresenter()->grantPermission(cppVariantToWebString(arguments[0]));
    result->set(true);
}

void LayoutTestController::simulateDesktopNotificationClick(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 1 || !arguments[0].isString()) {
        result->set(false);
        return;
    }
    if (m_shell->notificationPresenter()->simulateClick(cppVariantToWebString(arguments[0])))
        result->set(true);
    else
        result->set(false);
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
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setJavaScriptCanAccessClipboard(arguments[0].value.boolValue);
    result->setNull();
}

void LayoutTestController::setXSSAuditorEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setXSSAuditorEnabled(arguments[0].value.boolValue);
    result->setNull();
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

void LayoutTestController::setAllowUniversalAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setAllowUniversalAccessFromFileURLs(arguments[0].value.boolValue);
    result->setNull();
}

void LayoutTestController::setAllowFileAccessFromFileURLs(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webView()->settings()->setAllowFileAccessFromFileURLs(arguments[0].value.boolValue);
    result->setNull();
}

// Need these conversions because the format of the value for booleans
// may vary - for example, on mac "1" and "0" are used for boolean.
bool LayoutTestController::cppVariantToBool(const CppVariant& value)
{
    if (value.isBool())
        return value.toBoolean();
    if (value.isInt32())
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
    if (value.isInt32())
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

void LayoutTestController::overridePreference(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[0].isString())
        return;

    string key = arguments[0].toString();
    CppVariant value = arguments[1];
    WebSettings* settings = m_shell->webView()->settings();
    if (key == "WebKitStandardFont")
        settings->setStandardFontFamily(cppVariantToWebString(value));
    else if (key == "WebKitFixedFont")
        settings->setFixedFontFamily(cppVariantToWebString(value));
    else if (key == "WebKitSerifFont")
        settings->setSerifFontFamily(cppVariantToWebString(value));
    else if (key == "WebKitSansSerifFont")
        settings->setSansSerifFontFamily(cppVariantToWebString(value));
    else if (key == "WebKitCursiveFont")
        settings->setCursiveFontFamily(cppVariantToWebString(value));
    else if (key == "WebKitFantasyFont")
        settings->setFantasyFontFamily(cppVariantToWebString(value));
    else if (key == "WebKitDefaultFontSize")
        settings->setDefaultFontSize(cppVariantToInt32(value));
    else if (key == "WebKitDefaultFixedFontSize")
        settings->setDefaultFixedFontSize(cppVariantToInt32(value));
    else if (key == "WebKitMinimumFontSize")
        settings->setMinimumFontSize(cppVariantToInt32(value));
    else if (key == "WebKitMinimumLogicalFontSize")
        settings->setMinimumLogicalFontSize(cppVariantToInt32(value));
    else if (key == "WebKitDefaultTextEncodingName")
        settings->setDefaultTextEncodingName(cppVariantToWebString(value));
    else if (key == "WebKitJavaScriptEnabled")
        settings->setJavaScriptEnabled(cppVariantToBool(value));
    else if (key == "WebKitWebSecurityEnabled")
        settings->setWebSecurityEnabled(cppVariantToBool(value));
    else if (key == "WebKitJavaScriptCanOpenWindowsAutomatically")
        settings->setJavaScriptCanOpenWindowsAutomatically(cppVariantToBool(value));
    else if (key == "WebKitDisplayImagesKey")
        settings->setLoadsImagesAutomatically(cppVariantToBool(value));
    else if (key == "WebKitPluginsEnabled")
        settings->setPluginsEnabled(cppVariantToBool(value));
    else if (key == "WebKitDOMPasteAllowedPreferenceKey")
        settings->setDOMPasteAllowed(cppVariantToBool(value));
    else if (key == "WebKitDeveloperExtrasEnabledPreferenceKey")
        settings->setDeveloperExtrasEnabled(cppVariantToBool(value));
    else if (key == "WebKitShrinksStandaloneImagesToFit")
        settings->setShrinksStandaloneImagesToFit(cppVariantToBool(value));
    else if (key == "WebKitTextAreasAreResizable")
        settings->setTextAreasAreResizable(cppVariantToBool(value));
    else if (key == "WebKitJavaEnabled")
        settings->setJavaEnabled(cppVariantToBool(value));
    else if (key == "WebKitUsesPageCachePreferenceKey")
        settings->setUsesPageCache(cppVariantToBool(value));
    else if (key == "WebKitJavaScriptCanAccessClipboard")
        settings->setJavaScriptCanAccessClipboard(cppVariantToBool(value));
    else if (key == "WebKitXSSAuditorEnabled")
        settings->setXSSAuditorEnabled(cppVariantToBool(value));
    else if (key == "WebKitLocalStorageEnabledPreferenceKey")
        settings->setLocalStorageEnabled(cppVariantToBool(value));
    else if (key == "WebKitOfflineWebApplicationCacheEnabled")
        settings->setOfflineWebApplicationCacheEnabled(cppVariantToBool(value));
    else if (key == "WebKitTabToLinksPreferenceKey")
        m_shell->webView()->setTabsToLinks(cppVariantToBool(value));
    else if (key == "WebKitWebGLEnabled")
        settings->setExperimentalWebGLEnabled(cppVariantToBool(value));
    else {
        string message("Invalid name for preference: ");
        message.append(key);
        logErrorToConsole(message);
    }
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
    if ((arguments.size() >= 1) && arguments[0].isInt32())
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

static bool parsePageSizeParameters(const CppArgumentList& arguments,
                                    int argOffset,
                                    float* pageWidthInPixels,
                                    float* pageHeightInPixels)
{
    // WebKit is using the window width/height of DumpRenderTree as the
    // default value of the page size.
    // FIXME: share these values with other ports.
    *pageWidthInPixels = 800;
    *pageHeightInPixels = 600;
    switch (arguments.size() - argOffset) {
    case 2:
        if (!arguments[argOffset].isNumber() || !arguments[1 + argOffset].isNumber())
            return false;
        *pageWidthInPixels = static_cast<float>(arguments[argOffset].toInt32());
        *pageHeightInPixels = static_cast<float>(arguments[1 + argOffset].toInt32());
        // fall through.
    case 0:
        break;
    default:
        return false;
    }
    return true;
}

void LayoutTestController::pageNumberForElementById(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    float pageWidthInPixels = 0;
    float pageHeightInPixels = 0;
    if (!parsePageSizeParameters(arguments, 1,
                                 &pageWidthInPixels, &pageHeightInPixels))
        return;
    if (!arguments[0].isString())
        return;
    WebFrame* frame = m_shell->webView()->mainFrame();
    if (!frame)
        return;
    result->set(frame->pageNumberForElementById(cppVariantToWebString(arguments[0]),
                                                pageWidthInPixels, pageHeightInPixels));
}

void LayoutTestController::numberOfPages(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    float pageWidthInPixels = 0;
    float pageHeightInPixels = 0;
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

void LayoutTestController::logErrorToConsole(const std::string& text)
{
    m_shell->webViewHost()->didAddMessageToConsole(
        WebConsoleMessage(WebConsoleMessage::LevelError, WebString::fromUTF8(text)),
        WebString(), 0);
}

void LayoutTestController::setTimelineProfilingEnabled(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;
    m_shell->drtDevToolsAgent()->setTimelineProfilingEnabled(arguments[0].toBoolean());
}

void LayoutTestController::evaluateInWebInspector(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isInt32() || !arguments[1].isString())
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
        arguments[2].toBoolean() ? WebView::UserContentInjectInAllFrames : WebView::UserContentInjectInTopFrameOnly);
}

void LayoutTestController::setEditingBehavior(const CppArgumentList& arguments, CppVariant* results)
{
    WebSettings* settings = m_shell->webView()->settings();
    string key = arguments[0].toString();
    if (key == "mac")
        settings->setEditingBehavior(WebSettings::EditingBehaviorMac);
    else if (key == "win")
        settings->setEditingBehavior(WebSettings::EditingBehaviorWin);
    else
        logErrorToConsole("Passed invalid editing behavior. Should be 'mac' or 'win'.");
}

void LayoutTestController::setMockDeviceOrientation(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 6 || !arguments[0].isBool() || !arguments[1].isNumber() || !arguments[2].isBool() || !arguments[3].isNumber() || !arguments[4].isBool() || !arguments[5].isNumber())
        return;

    WebKit::WebDeviceOrientation orientation(arguments[0].toBoolean(), arguments[1].toDouble(), arguments[2].toBoolean(), arguments[3].toDouble(), arguments[4].toBoolean(), arguments[5].toDouble());

    ASSERT(m_deviceOrientationClientMock);
    m_deviceOrientationClientMock->setOrientation(orientation);
}

void LayoutTestController::setGeolocationPermission(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isBool())
        return;
    WebGeolocationServiceMock::setMockGeolocationPermission(arguments[0].toBoolean());
}

void LayoutTestController::setMockGeolocationPosition(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 3 || !arguments[0].isNumber() || !arguments[1].isNumber() || !arguments[2].isNumber())
        return;
    WebGeolocationServiceMock::setMockGeolocationPosition(arguments[0].toDouble(), arguments[1].toDouble(), arguments[2].toDouble());
}

void LayoutTestController::setMockGeolocationError(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isInt32() || !arguments[1].isString())
        return;
    WebGeolocationServiceMock::setMockGeolocationError(arguments[0].toInt32(), cppVariantToWebString(arguments[1]));
}

void LayoutTestController::abortModal(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
}

void LayoutTestController::setMockSpeechInputResult(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 1 || !arguments[0].isString())
        return;

    m_speechInputControllerMock->setMockRecognitionResult(cppVariantToWebString(arguments[0]));
}

WebKit::WebSpeechInputController* LayoutTestController::speechInputController(WebKit::WebSpeechInputListener* listener)
{
    if (!m_speechInputControllerMock.get())
        m_speechInputControllerMock.set(WebSpeechInputControllerMock::create(listener));
    return m_speechInputControllerMock.get();
}

void LayoutTestController::markerTextForListItem(const CppArgumentList& args, CppVariant* result)
{
    WebElement element;
    if (!WebBindings::getElement(args[0].value.objectValue, &element))
        result->setNull();
    else
        result->set(element.document().frame()->markerTextForListItem(element).utf8());
}

WebKit::WebDeviceOrientationClient* LayoutTestController::deviceOrientationClient()
{
    if (!m_deviceOrientationClientMock.get())
        m_deviceOrientationClientMock.set(new WebKit::WebDeviceOrientationClientMock());
    return m_deviceOrientationClientMock.get();
}
