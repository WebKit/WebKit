/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebWorkerBase.h"

#include "GenericWorkerTask.h"
#include "MessagePortChannel.h"
#include "PlatformMessagePortChannel.h"

#include "WebDataSourceImpl.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebMessagePortChannel.h"
#include "WebView.h"
#include "WebWorkerClient.h"

#include "WorkerThread.h"
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

#if ENABLE(WORKERS)

// Dummy WebViewDelegate - we only need it in Worker process to load a
// 'shadow page' which will initialize WebCore loader.
class WorkerWebFrameClient : public WebFrameClient {
public:
    // Tell the loader to load the data into the 'shadow page' synchronously,
    // so we can grab the resulting Document right after load.
    virtual void didCreateDataSource(WebFrame* frame, WebDataSource* ds)
    {
        static_cast<WebDataSourceImpl*>(ds)->setDeferMainResourceDataLoad(false);
    }

    // Lazy allocate and leak this instance.
    static WorkerWebFrameClient* sharedInstance()
    {
        static WorkerWebFrameClient client;
        return &client;
    }

private:
    WorkerWebFrameClient()
    {
    }
};

// This function is called on the main thread to force to initialize some static
// values used in WebKit before any worker thread is started. This is because in
// our worker processs, we do not run any WebKit code in main thread and thus
// when multiple workers try to start at the same time, we might hit crash due
// to contention for initializing static values.
static void initializeWebKitStaticValues()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        // Note that we have to pass a URL with valid protocol in order to follow
        // the path to do static value initializations.
        RefPtr<SecurityOrigin> origin =
            SecurityOrigin::create(KURL(ParsedURLString, "http://localhost"));
        origin.release();
    }
}

WebWorkerBase::WebWorkerBase()
    : m_webView(0)
    , m_askedToTerminate(false)
{
    initializeWebKitStaticValues();
}

WebWorkerBase::~WebWorkerBase()
{
    ASSERT(m_webView);
    m_webView->close();
}

void WebWorkerBase::stopWorkerThread()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;
    if (m_workerThread)
        m_workerThread->stop();
}

void WebWorkerBase::initializeLoader(const WebURL& url)
{
    // Create 'shadow page'. This page is never displayed, it is used to proxy the
    // loading requests from the worker context to the rest of WebKit and Chromium
    // infrastructure.
    ASSERT(!m_webView);
    m_webView = WebView::create(0);
    m_webView->initializeMainFrame(WorkerWebFrameClient::sharedInstance());

    WebFrameImpl* webFrame = static_cast<WebFrameImpl*>(m_webView->mainFrame());

    // Construct substitute data source for the 'shadow page'. We only need it
    // to have same origin as the worker so the loading checks work correctly.
    CString content("");
    int len = static_cast<int>(content.length());
    RefPtr<SharedBuffer> buf(SharedBuffer::create(content.data(), len));
    SubstituteData substData(buf, String("text/html"), String("UTF-8"), KURL());
    ResourceRequest request(url, CString());
    webFrame->frame()->loader()->load(request, substData, false);

    // This document will be used as 'loading context' for the worker.
    m_loadingDocument = webFrame->frame()->document();
}

void WebWorkerBase::dispatchTaskToMainThread(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    return callOnMainThread(invokeTaskMethod, task.release());
}

void WebWorkerBase::invokeTaskMethod(void* param)
{
    ScriptExecutionContext::Task* task =
        static_cast<ScriptExecutionContext::Task*>(param);
    task->performTask(0);
    delete task;
}

// WorkerObjectProxy -----------------------------------------------------------

void WebWorkerBase::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message,
                                              PassOwnPtr<MessagePortChannelArray> channels)
{
    dispatchTaskToMainThread(createCallbackTask(&postMessageTask, this,
                                                message->toString(), channels));
}

void WebWorkerBase::postMessageTask(ScriptExecutionContext* context,
                                    WebWorkerBase* thisPtr,
                                    String message,
                                    PassOwnPtr<MessagePortChannelArray> channels)
{
    if (!thisPtr->client())
        return;

    WebMessagePortChannelArray webChannels(channels.get() ? channels->size() : 0);
    for (size_t i = 0; i < webChannels.size(); ++i) {
        webChannels[i] = (*channels)[i]->channel()->webChannelRelease();
        webChannels[i]->setClient(0);
    }

    thisPtr->client()->postMessageToWorkerObject(message, webChannels);
}

void WebWorkerBase::postExceptionToWorkerObject(const String& errorMessage,
                                                int lineNumber,
                                                const String& sourceURL)
{
    dispatchTaskToMainThread(createCallbackTask(&postExceptionTask, this,
                                                errorMessage, lineNumber,
                                                sourceURL));
}

void WebWorkerBase::postExceptionTask(ScriptExecutionContext* context,
                                      WebWorkerBase* thisPtr,
                                      const String& errorMessage,
                                      int lineNumber, const String& sourceURL)
{
    if (!thisPtr->commonClient())
        return;

    thisPtr->commonClient()->postExceptionToWorkerObject(errorMessage,
                                                         lineNumber,
                                                         sourceURL);
}

void WebWorkerBase::postConsoleMessageToWorkerObject(MessageDestination destination,
                                                     MessageSource source,
                                                     MessageType type,
                                                     MessageLevel level,
                                                     const String& message,
                                                     int lineNumber,
                                                     const String& sourceURL)
{
    dispatchTaskToMainThread(createCallbackTask(&postConsoleMessageTask, this,
                                                static_cast<int>(destination),
                                                static_cast<int>(source),
                                                static_cast<int>(type),
                                                static_cast<int>(level),
                                                message, lineNumber, sourceURL));
}

void WebWorkerBase::postConsoleMessageTask(ScriptExecutionContext* context,
                                           WebWorkerBase* thisPtr,
                                           int destination, int source,
                                           int type, int level,
                                           const String& message,
                                           int lineNumber,
                                           const String& sourceURL)
{
    if (!thisPtr->commonClient())
        return;
    thisPtr->commonClient()->postConsoleMessageToWorkerObject(destination, source,
                                                              type, level, message,
                                                              lineNumber, sourceURL);
}

void WebWorkerBase::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    dispatchTaskToMainThread(createCallbackTask(&confirmMessageTask, this,
                                                hasPendingActivity));
}

void WebWorkerBase::confirmMessageTask(ScriptExecutionContext* context,
                                       WebWorkerBase* thisPtr,
                                       bool hasPendingActivity)
{
    if (!thisPtr->client())
        return;
    thisPtr->client()->confirmMessageFromWorkerObject(hasPendingActivity);
}

void WebWorkerBase::reportPendingActivity(bool hasPendingActivity)
{
    dispatchTaskToMainThread(createCallbackTask(&reportPendingActivityTask,
                                                this, hasPendingActivity));
}

void WebWorkerBase::reportPendingActivityTask(ScriptExecutionContext* context,
                                              WebWorkerBase* thisPtr,
                                              bool hasPendingActivity)
{
    if (!thisPtr->client())
        return;
    thisPtr->client()->reportPendingActivity(hasPendingActivity);
}

void WebWorkerBase::workerContextClosed()
{
    dispatchTaskToMainThread(createCallbackTask(&workerContextClosedTask,
                                                this));
}

void WebWorkerBase::workerContextClosedTask(ScriptExecutionContext* context,
                                            WebWorkerBase* thisPtr)
{
    if (thisPtr->commonClient())
        thisPtr->commonClient()->workerContextClosed();
}

void WebWorkerBase::workerContextDestroyed()
{
    dispatchTaskToMainThread(createCallbackTask(&workerContextDestroyedTask,
                                                this));
}

void WebWorkerBase::workerContextDestroyedTask(ScriptExecutionContext* context,
                                               WebWorkerBase* thisPtr)
{
    if (thisPtr->commonClient())
        thisPtr->commonClient()->workerContextDestroyed();
    // The lifetime of this proxy is controlled by the worker context.
    delete thisPtr;
}

// WorkerLoaderProxy -----------------------------------------------------------

void WebWorkerBase::postTaskToLoader(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    ASSERT(m_loadingDocument->isDocument());
    m_loadingDocument->postTask(task);
}

void WebWorkerBase::postTaskForModeToWorkerContext(
    PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    m_workerThread->runLoop().postTaskForMode(task, mode);
}

#endif // ENABLE(WORKERS)

} // namespace WebKit
