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
#include "DRTTestRunner.h"

#include "DRTDevToolsAgent.h"
#include "MockWebSpeechInputController.h"
#include "MockWebSpeechRecognizer.h"
#include "Task.h"
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
using namespace WebTestRunner;
using namespace std;

DRTTestRunner::DRTTestRunner(TestShell* shell)
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
    // by CppBoundClass, the parent to DRTTestRunner).
#if ENABLE(INPUT_SPEECH)
    bindMethod("addMockSpeechInputResult", &DRTTestRunner::addMockSpeechInputResult);
    bindMethod("setMockSpeechInputDumpRect", &DRTTestRunner::setMockSpeechInputDumpRect);
#endif
#if ENABLE(SCRIPTED_SPEECH)
    bindMethod("addMockSpeechRecognitionResult", &DRTTestRunner::addMockSpeechRecognitionResult);
    bindMethod("setMockSpeechRecognitionError", &DRTTestRunner::setMockSpeechRecognitionError);
    bindMethod("wasMockSpeechRecognitionAborted", &DRTTestRunner::wasMockSpeechRecognitionAborted);
#endif
    bindMethod("clearAllDatabases", &DRTTestRunner::clearAllDatabases);
    bindMethod("closeWebInspector", &DRTTestRunner::closeWebInspector);
#if ENABLE(POINTER_LOCK)
    bindMethod("didAcquirePointerLock", &DRTTestRunner::didAcquirePointerLock);
    bindMethod("didLosePointerLock", &DRTTestRunner::didLosePointerLock);
    bindMethod("didNotAcquirePointerLock", &DRTTestRunner::didNotAcquirePointerLock);
#endif
    bindMethod("disableAutoResizeMode", &DRTTestRunner::disableAutoResizeMode);
    bindMethod("display", &DRTTestRunner::display);
    bindMethod("displayInvalidatedRegion", &DRTTestRunner::displayInvalidatedRegion);
    bindMethod("dumpBackForwardList", &DRTTestRunner::dumpBackForwardList);
    bindMethod("dumpFrameLoadCallbacks", &DRTTestRunner::dumpFrameLoadCallbacks);
    bindMethod("dumpProgressFinishedCallback", &DRTTestRunner::dumpProgressFinishedCallback);
    bindMethod("dumpUserGestureInFrameLoadCallbacks", &DRTTestRunner::dumpUserGestureInFrameLoadCallbacks);
    bindMethod("dumpResourceLoadCallbacks", &DRTTestRunner::dumpResourceLoadCallbacks);
    bindMethod("dumpResourceRequestCallbacks", &DRTTestRunner::dumpResourceRequestCallbacks);
    bindMethod("dumpResourceResponseMIMETypes", &DRTTestRunner::dumpResourceResponseMIMETypes);
    bindMethod("dumpSelectionRect", &DRTTestRunner::dumpSelectionRect);
    bindMethod("dumpStatusCallbacks", &DRTTestRunner::dumpWindowStatusChanges);
    bindMethod("dumpTitleChanges", &DRTTestRunner::dumpTitleChanges);
    bindMethod("dumpPermissionClientCallbacks", &DRTTestRunner::dumpPermissionClientCallbacks);
    bindMethod("dumpCreateView", &DRTTestRunner::dumpCreateView);
    bindMethod("enableAutoResizeMode", &DRTTestRunner::enableAutoResizeMode);
    bindMethod("evaluateInWebInspector", &DRTTestRunner::evaluateInWebInspector);
#if ENABLE(NOTIFICATIONS)
    bindMethod("grantWebNotificationPermission", &DRTTestRunner::grantWebNotificationPermission);
#endif
    bindMethod("notifyDone", &DRTTestRunner::notifyDone);
    bindMethod("numberOfPendingGeolocationPermissionRequests", &DRTTestRunner:: numberOfPendingGeolocationPermissionRequests);
    bindMethod("pathToLocalResource", &DRTTestRunner::pathToLocalResource);
    bindMethod("queueBackNavigation", &DRTTestRunner::queueBackNavigation);
    bindMethod("queueForwardNavigation", &DRTTestRunner::queueForwardNavigation);
    bindMethod("queueLoadingScript", &DRTTestRunner::queueLoadingScript);
    bindMethod("queueLoad", &DRTTestRunner::queueLoad);
    bindMethod("queueLoadHTMLString", &DRTTestRunner::queueLoadHTMLString);
    bindMethod("queueNonLoadingScript", &DRTTestRunner::queueNonLoadingScript);
    bindMethod("queueReload", &DRTTestRunner::queueReload);
    bindMethod("repaintSweepHorizontally", &DRTTestRunner::repaintSweepHorizontally);
    bindMethod("setAllowDisplayOfInsecureContent", &DRTTestRunner::setAllowDisplayOfInsecureContent);
    bindMethod("setAllowRunningOfInsecureContent", &DRTTestRunner::setAllowRunningOfInsecureContent);
    bindMethod("setAlwaysAcceptCookies", &DRTTestRunner::setAlwaysAcceptCookies);
    bindMethod("setCanOpenWindows", &DRTTestRunner::setCanOpenWindows);
    bindMethod("setCloseRemainingWindowsWhenComplete", &DRTTestRunner::setCloseRemainingWindowsWhenComplete);
    bindMethod("setCustomPolicyDelegate", &DRTTestRunner::setCustomPolicyDelegate);
    bindMethod("setDatabaseQuota", &DRTTestRunner::setDatabaseQuota);
    bindMethod("setDeferMainResourceDataLoad", &DRTTestRunner::setDeferMainResourceDataLoad);
    bindMethod("setAudioData", &DRTTestRunner::setAudioData);
    bindMethod("setGeolocationPermission", &DRTTestRunner::setGeolocationPermission);
    bindMethod("setMockDeviceOrientation", &DRTTestRunner::setMockDeviceOrientation);
    bindMethod("setMockGeolocationPositionUnavailableError", &DRTTestRunner::setMockGeolocationPositionUnavailableError);
    bindMethod("setMockGeolocationPosition", &DRTTestRunner::setMockGeolocationPosition);
#if ENABLE(POINTER_LOCK)
    bindMethod("setPointerLockWillRespondAsynchronously", &DRTTestRunner::setPointerLockWillRespondAsynchronously);
    bindMethod("setPointerLockWillFailSynchronously", &DRTTestRunner::setPointerLockWillFailSynchronously);
#endif
    bindMethod("setPOSIXLocale", &DRTTestRunner::setPOSIXLocale);
    bindMethod("setPrinting", &DRTTestRunner::setPrinting);
    bindMethod("setSelectTrailingWhitespaceEnabled", &DRTTestRunner::setSelectTrailingWhitespaceEnabled);
    bindMethod("setBackingScaleFactor", &DRTTestRunner::setBackingScaleFactor);
    bindMethod("setSmartInsertDeleteEnabled", &DRTTestRunner::setSmartInsertDeleteEnabled);
    bindMethod("setStopProvisionalFrameLoads", &DRTTestRunner::setStopProvisionalFrameLoads);
    bindMethod("setWillSendRequestClearHeader", &DRTTestRunner::setWillSendRequestClearHeader);
    bindMethod("setWillSendRequestReturnsNull", &DRTTestRunner::setWillSendRequestReturnsNull);
    bindMethod("setWillSendRequestReturnsNullOnRedirect", &DRTTestRunner::setWillSendRequestReturnsNullOnRedirect);
    bindMethod("setWindowIsKey", &DRTTestRunner::setWindowIsKey);
    bindMethod("showWebInspector", &DRTTestRunner::showWebInspector);
#if ENABLE(NOTIFICATIONS)
    bindMethod("simulateLegacyWebNotificationClick", &DRTTestRunner::simulateLegacyWebNotificationClick);
#endif
    bindMethod("testRepaint", &DRTTestRunner::testRepaint);
    bindMethod("waitForPolicyDelegate", &DRTTestRunner::waitForPolicyDelegate);
    bindMethod("waitUntilDone", &DRTTestRunner::waitUntilDone);
    bindMethod("windowCount", &DRTTestRunner::windowCount);
    bindMethod("setImagesAllowed", &DRTTestRunner::setImagesAllowed);
    bindMethod("setScriptsAllowed", &DRTTestRunner::setScriptsAllowed);
    bindMethod("setStorageAllowed", &DRTTestRunner::setStorageAllowed);
    bindMethod("setPluginsAllowed", &DRTTestRunner::setPluginsAllowed);

    bindMethod("setShouldStayOnPageAfterHandlingBeforeUnload", &DRTTestRunner::setShouldStayOnPageAfterHandlingBeforeUnload);

    // Shared properties.
    // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
    bindProperty("webHistoryItemCount", &m_webHistoryItemCount);
    bindProperty("titleTextDirection", &m_titleTextDirection);
    bindProperty("interceptPostMessage", &m_interceptPostMessage);
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

void DRTTestRunner::dumpResourceRequestCallbacks(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceRequestCallbacks = true;
    result->setNull();
}

void DRTTestRunner::dumpResourceResponseMIMETypes(const CppArgumentList&, CppVariant* result)
{
    m_dumpResourceResponseMIMETypes = true;
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

void DRTTestRunner::reset()
{
    TestRunner::reset();
    if (m_shell)
        m_shell->webViewHost()->setDeviceScaleFactor(1);
    m_dumpAsAudio = false;
    m_dumpCreateView = false;
    m_dumpFrameLoadCallbacks = false;
    m_dumpProgressFinishedCallback = false;
    m_dumpUserGestureInFrameLoadCallbacks = false;
    m_dumpResourceLoadCallbacks = false;
    m_dumpResourceRequestCallbacks = false;
    m_dumpResourceResponseMIMETypes = false;
    m_dumpBackForwardList = false;
    m_dumpWindowStatusChanges = false;
    m_dumpSelectionRect = false;
    m_dumpTitleChanges = false;
    m_dumpPermissionClientCallbacks = false;
    m_waitUntilDone = false;
    m_canOpenWindows = false;
    m_testRepaint = false;
    m_sweepHorizontally = false;
    m_stopProvisionalFrameLoads = false;
    m_deferMainResourceDataLoad = true;
    m_webHistoryItemCount.set(0);
    m_titleTextDirection.set("ltr");
    m_interceptPostMessage.set(false);
    m_isPrinting = false;

    webkit_support::SetAcceptAllCookies(false);

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

#if ENABLE(NOTIFICATIONS)
void DRTTestRunner::grantWebNotificationPermission(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::simulateLegacyWebNotificationClick(const CppArgumentList& arguments, CppVariant* result)
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

void DRTTestRunner::setDeferMainResourceDataLoad(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1)
        m_deferMainResourceDataLoad = cppVariantToBool(arguments[0]);
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
    host->proxy()->setPaintRect(rect);
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

void DRTTestRunner::setAllowDisplayOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setDisplayingInsecureContentAllowed(arguments[0].toBoolean());

    result->setNull();
}

void DRTTestRunner::setAllowRunningOfInsecureContent(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() > 0 && arguments[0].isBool())
        m_shell->webPermissions()->setRunningInsecureContentAllowed(arguments[0].value.boolValue);

    result->setNull();
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

void DRTTestRunner::setPrinting(const CppArgumentList& arguments, CppVariant* result)
{
    setIsPrinting(true);
    result->setNull();
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

void DRTTestRunner::evaluateInWebInspector(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isString())
        return;
    m_shell->drtDevToolsAgent()->evaluateInWebInspector(arguments[0].toInt32(), arguments[1].toString());
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

void DRTTestRunner::setMockGeolocationPositionUnavailableError(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 1 || !arguments[0].isString())
        return;
    Vector<WebViewHost*> windowList = m_shell->windowList();
    // FIXME: Benjamin
    for (size_t i = 0; i < windowList.size(); i++)
        windowList[i]->geolocationClientMock()->setPositionUnavailableError(cppVariantToWebString(arguments[0]));
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
    if (arguments.size() != 2 || !arguments[0].isString() || !arguments[1].isString())
        return;

    if (MockWebSpeechRecognizer* recognizer = m_shell->webViewHost()->mockSpeechRecognizer())
        recognizer->setError(cppVariantToWebString(arguments[0]), cppVariantToWebString(arguments[1]));
}

void DRTTestRunner::wasMockSpeechRecognitionAborted(const CppArgumentList&, CppVariant* result)
{
    result->set(false);
    if (MockWebSpeechRecognizer* recognizer = m_shell->webViewHost()->mockSpeechRecognizer())
        result->set(recognizer->wasAborted());
}
#endif

void DRTTestRunner::setShouldStayOnPageAfterHandlingBeforeUnload(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() == 1 && arguments[0].isBool())
        m_shouldStayOnPageAfterHandlingBeforeUnload = arguments[0].toBoolean();

    result->setNull();
}

class InvokeCallbackTask : public WebMethodTask<DRTTestRunner> {
public:
    InvokeCallbackTask(DRTTestRunner* object, PassOwnArrayPtr<CppVariant> callbackArguments, uint32_t numberOfArguments)
        : WebMethodTask<DRTTestRunner>(object)
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
    m_shell->webViewHost()->setDeviceScaleFactor(value);

    OwnArrayPtr<CppVariant> callbackArguments = adoptArrayPtr(new CppVariant[1]);
    callbackArguments[0].set(arguments[1]);
    result->setNull();
    postTask(new InvokeCallbackTask(this, callbackArguments.release(), 1));
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

#if ENABLE(POINTER_LOCK)
void DRTTestRunner::didAcquirePointerLock(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->didAcquirePointerLock();
    result->setNull();
}

void DRTTestRunner::didNotAcquirePointerLock(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->didNotAcquirePointerLock();
    result->setNull();
}

void DRTTestRunner::didLosePointerLock(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->didLosePointerLock();
    result->setNull();
}

void DRTTestRunner::setPointerLockWillRespondAsynchronously(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->setPointerLockWillRespondAsynchronously();
    result->setNull();
}

void DRTTestRunner::setPointerLockWillFailSynchronously(const CppArgumentList&, CppVariant* result)
{
    m_shell->webViewHost()->setPointerLockWillFailSynchronously();
    result->setNull();
}
#endif
