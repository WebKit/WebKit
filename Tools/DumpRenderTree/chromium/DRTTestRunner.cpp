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
#include "Task.h"
#include "TestShell.h"
#include "WebAnimationController.h"
#include "WebBindings.h"
#include "WebConsoleMessage.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFindOptions.h"
#include "WebFrame.h"
#include "WebIDBFactory.h"
#include "WebInputElement.h"
#include "WebKit.h"
#include "WebPrintParams.h"
#include "WebScriptSource.h"
#include "WebSecurityPolicy.h"
#include "WebSerializedScriptValue.h"
#include "WebSettings.h"
#include "WebSurroundingText.h"
#include "WebView.h"
#include "WebViewHost.h"
#include "WebWorkerInfo.h"
#include "v8/include/v8.h"
#include "webkit/support/webkit_support.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <public/WebData.h>
#include <public/WebSize.h>
#include <public/WebURL.h>
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
    , m_showDebugLayerTree(false)
    , m_workQueue(this)
{

    // Initialize the map that associates methods of this class with the names
    // they will use when called by JavaScript. The actual binding of those
    // names to their methods will be done by calling bindToJavaScript() (defined
    // by CppBoundClass, the parent to DRTTestRunner).
    bindMethod("notifyDone", &DRTTestRunner::notifyDone);
    bindMethod("queueBackNavigation", &DRTTestRunner::queueBackNavigation);
    bindMethod("queueForwardNavigation", &DRTTestRunner::queueForwardNavigation);
    bindMethod("queueLoadingScript", &DRTTestRunner::queueLoadingScript);
    bindMethod("queueLoad", &DRTTestRunner::queueLoad);
    bindMethod("queueLoadHTMLString", &DRTTestRunner::queueLoadHTMLString);
    bindMethod("queueNonLoadingScript", &DRTTestRunner::queueNonLoadingScript);
    bindMethod("queueReload", &DRTTestRunner::queueReload);
    bindMethod("setCloseRemainingWindowsWhenComplete", &DRTTestRunner::setCloseRemainingWindowsWhenComplete);
    bindMethod("setCustomPolicyDelegate", &DRTTestRunner::setCustomPolicyDelegate);
    bindMethod("waitForPolicyDelegate", &DRTTestRunner::waitForPolicyDelegate);
    bindMethod("waitUntilDone", &DRTTestRunner::waitUntilDone);
    bindMethod("windowCount", &DRTTestRunner::windowCount);


    // Shared properties.
    // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
    bindProperty("webHistoryItemCount", &m_webHistoryItemCount);
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
    if (m_controller->topLoadingFrame())
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

    if (!m_controller->m_waitUntilDone && !m_controller->topLoadingFrame())
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
    taskList()->revokeAll();

    completeNotifyDone(false);
    result->setNull();
}

void DRTTestRunner::completeNotifyDone(bool isTimeout)
{
    if (m_waitUntilDone && !topLoadingFrame() && m_workQueue.isEmpty()) {
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
    m_waitUntilDone = false;
    m_webHistoryItemCount.set(0);
    m_interceptPostMessage.set(false);

    if (m_closeRemainingWindows)
        m_shell->closeRemainingWindows();
    else
        m_closeRemainingWindows = true;
    m_workQueue.reset();
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
