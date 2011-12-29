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
#include "WebSharedWorkerImpl.h"

#include "CrossThreadTask.h"
#include "DatabaseTask.h"
#include "Document.h"
#include "KURL.h"
#include "MessageEvent.h"
#include "MessagePortChannel.h"
#include "PlatformMessagePortChannel.h"
#include "SecurityOrigin.h"
#include "ScriptExecutionContext.h"
#include "SharedWorkerContext.h"
#include "SharedWorkerThread.h"
#include "WebDataSourceImpl.h"
#include "WebFileError.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebMessagePortChannel.h"
#include "WebRuntimeFeatures.h"
#include "WebSettings.h"
#include "WebSharedWorkerClient.h"
#include "WebView.h"
#include "WorkerContext.h"
#include "WorkerDebuggerAgent.h"
#include "WorkerInspectorController.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

#if ENABLE(SHARED_WORKERS)
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

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : m_webView(0)
    , m_askedToTerminate(false)
    , m_client(client)
    , m_pauseWorkerContextOnStart(false)
{
    initializeWebKitStaticValues();
}

WebSharedWorkerImpl::~WebSharedWorkerImpl()
{
    ASSERT(m_webView);
    WebFrameImpl* webFrame = static_cast<WebFrameImpl*>(m_webView->mainFrame());
    if (webFrame)
        webFrame->setClient(0);
    m_webView->close();
}

void WebSharedWorkerImpl::stopWorkerThread()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;
    if (m_workerThread)
        m_workerThread->stop();
}

void WebSharedWorkerImpl::initializeLoader(const WebURL& url)
{
    // Create 'shadow page'. This page is never displayed, it is used to proxy the
    // loading requests from the worker context to the rest of WebKit and Chromium
    // infrastructure.
    ASSERT(!m_webView);
    m_webView = WebView::create(0);
    m_webView->settings()->setOfflineWebApplicationCacheEnabled(WebRuntimeFeatures::isApplicationCacheEnabled());
    // FIXME: Settings information should be passed to the Worker process from Browser process when the worker
    // is created (similar to RenderThread::OnCreateNewView).
    m_webView->settings()->setHixie76WebSocketProtocolEnabled(false);
    m_webView->initializeMainFrame(this);

    WebFrameImpl* webFrame = static_cast<WebFrameImpl*>(m_webView->mainFrame());

    // Construct substitute data source for the 'shadow page'. We only need it
    // to have same origin as the worker so the loading checks work correctly.
    CString content("");
    int len = static_cast<int>(content.length());
    RefPtr<SharedBuffer> buf(SharedBuffer::create(content.data(), len));
    SubstituteData substData(buf, String("text/html"), String("UTF-8"), KURL());
    webFrame->frame()->loader()->load(ResourceRequest(url), substData, false);

    // This document will be used as 'loading context' for the worker.
    m_loadingDocument = webFrame->frame()->document();
}

void WebSharedWorkerImpl::didCreateDataSource(WebFrame*, WebDataSource* ds)
{
    // Tell the loader to load the data into the 'shadow page' synchronously,
    // so we can grab the resulting Document right after load.
    static_cast<WebDataSourceImpl*>(ds)->setDeferMainResourceDataLoad(false);
}

WebApplicationCacheHost* WebSharedWorkerImpl::createApplicationCacheHost(WebFrame*, WebApplicationCacheHostClient* appcacheHostClient)
{
    if (client())
        return client()->createApplicationCacheHost(appcacheHostClient);
    return 0;
}

// WorkerObjectProxy -----------------------------------------------------------

void WebSharedWorkerImpl::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message,
                                                    PassOwnPtr<MessagePortChannelArray> channels)
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&postMessageTask, AllowCrossThreadAccess(this),
                                                message->toWireString(), channels));
}

void WebSharedWorkerImpl::postMessageTask(ScriptExecutionContext* context,
                                          WebSharedWorkerImpl* thisPtr,
                                          String message,
                                          PassOwnPtr<MessagePortChannelArray> channels)
{
    if (!thisPtr->client())
        return;

    WebMessagePortChannelArray webChannels(channels ? channels->size() : 0);
    for (size_t i = 0; i < webChannels.size(); ++i) {
        webChannels[i] = (*channels)[i]->channel()->webChannelRelease();
        webChannels[i]->setClient(0);
    }

    thisPtr->client()->postMessageToWorkerObject(message, webChannels);
}

void WebSharedWorkerImpl::postExceptionToWorkerObject(const String& errorMessage,
                                                      int lineNumber,
                                                      const String& sourceURL)
{
    WebWorkerBase::dispatchTaskToMainThread(
        createCallbackTask(&postExceptionTask, AllowCrossThreadAccess(this),
                           errorMessage, lineNumber,
                           sourceURL));
}

void WebSharedWorkerImpl::postExceptionTask(ScriptExecutionContext* context,
                                            WebSharedWorkerImpl* thisPtr,
                                            const String& errorMessage,
                                            int lineNumber, const String& sourceURL)
{
    if (!thisPtr->client())
        return;

    thisPtr->client()->postExceptionToWorkerObject(errorMessage,
                                                   lineNumber,
                                                   sourceURL);
}

void WebSharedWorkerImpl::postConsoleMessageToWorkerObject(MessageSource source,
                                                           MessageType type,
                                                           MessageLevel level,
                                                           const String& message,
                                                           int lineNumber,
                                                           const String& sourceURL)
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&postConsoleMessageTask, AllowCrossThreadAccess(this),
                                            source, type, level,
                                            message, lineNumber, sourceURL));
}

void WebSharedWorkerImpl::postConsoleMessageTask(ScriptExecutionContext* context,
                                                 WebSharedWorkerImpl* thisPtr,
                                                 int source,
                                                 int type, int level,
                                                 const String& message,
                                                 int lineNumber,
                                                 const String& sourceURL)
{
    if (!thisPtr->client())
        return;
    thisPtr->client()->postConsoleMessageToWorkerObject(source,
                                                              type, level, message,
                                                              lineNumber, sourceURL);
}

void WebSharedWorkerImpl::postMessageToPageInspector(const String& message)
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&postMessageToPageInspectorTask, AllowCrossThreadAccess(this), message));
}

void WebSharedWorkerImpl::postMessageToPageInspectorTask(ScriptExecutionContext*, WebSharedWorkerImpl* thisPtr, const String& message)
{
    if (!thisPtr->client())
        return;
    thisPtr->client()->dispatchDevToolsMessage(message);
}

void WebSharedWorkerImpl::updateInspectorStateCookie(const WTF::String& cookie)
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&updateInspectorStateCookieTask, AllowCrossThreadAccess(this), cookie));
}

void WebSharedWorkerImpl::updateInspectorStateCookieTask(ScriptExecutionContext*, WebSharedWorkerImpl* thisPtr, const String& cookie)
{
    if (!thisPtr->client())
        return;
    thisPtr->client()->saveDevToolsAgentState(cookie);
}

void WebSharedWorkerImpl::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&confirmMessageTask, AllowCrossThreadAccess(this),
                                            hasPendingActivity));
}

void WebSharedWorkerImpl::confirmMessageTask(ScriptExecutionContext* context,
                                             WebSharedWorkerImpl* thisPtr,
                                             bool hasPendingActivity)
{
    if (!thisPtr->client())
        return;
    thisPtr->client()->confirmMessageFromWorkerObject(hasPendingActivity);
}

void WebSharedWorkerImpl::reportPendingActivity(bool hasPendingActivity)
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&reportPendingActivityTask,
                                            AllowCrossThreadAccess(this),
                                            hasPendingActivity));
}

void WebSharedWorkerImpl::reportPendingActivityTask(ScriptExecutionContext* context,
                                                    WebSharedWorkerImpl* thisPtr,
                                                    bool hasPendingActivity)
{
    if (!thisPtr->client())
        return;
    thisPtr->client()->reportPendingActivity(hasPendingActivity);
}

void WebSharedWorkerImpl::workerContextClosed()
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&workerContextClosedTask,
                                            AllowCrossThreadAccess(this)));
}

void WebSharedWorkerImpl::workerContextClosedTask(ScriptExecutionContext* context,
                                                  WebSharedWorkerImpl* thisPtr)
{
    if (thisPtr->client())
        thisPtr->client()->workerContextClosed();

    thisPtr->stopWorkerThread();
}

void WebSharedWorkerImpl::workerContextDestroyed()
{
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&workerContextDestroyedTask,
                                                               AllowCrossThreadAccess(this)));
}

void WebSharedWorkerImpl::workerContextDestroyedTask(ScriptExecutionContext* context,
                                                     WebSharedWorkerImpl* thisPtr)
{
    if (thisPtr->client())
        thisPtr->client()->workerContextDestroyed();
    // The lifetime of this proxy is controlled by the worker context.
    delete thisPtr;
}

// WorkerLoaderProxy -----------------------------------------------------------

void WebSharedWorkerImpl::postTaskToLoader(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    ASSERT(m_loadingDocument->isDocument());
    m_loadingDocument->postTask(task);
}

void WebSharedWorkerImpl::postTaskForModeToWorkerContext(
    PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    m_workerThread->runLoop().postTaskForMode(task, mode);
}



bool WebSharedWorkerImpl::isStarted()
{
    // Should not ever be called from the worker thread (this API is only called on WebSharedWorkerProxy on the renderer thread).
    ASSERT_NOT_REACHED();
    return workerThread();
}

void WebSharedWorkerImpl::connect(WebMessagePortChannel* webChannel, ConnectListener* listener)
{
    // Convert the WebMessagePortChanel to a WebCore::MessagePortChannel.
    RefPtr<PlatformMessagePortChannel> platform_channel =
        PlatformMessagePortChannel::create(webChannel);
    webChannel->setClient(platform_channel.get());
    OwnPtr<MessagePortChannel> channel =
        MessagePortChannel::create(platform_channel);

    workerThread()->runLoop().postTask(
        createCallbackTask(&connectTask, channel.release()));
    if (listener)
        listener->connected();
}

void WebSharedWorkerImpl::connectTask(ScriptExecutionContext* context, PassOwnPtr<MessagePortChannel> channel)
{
    // Wrap the passed-in channel in a MessagePort, and send it off via a connect event.
    RefPtr<MessagePort> port = MessagePort::create(*context);
    port->entangle(channel);
    ASSERT(context->isWorkerContext());
    WorkerContext* workerContext = static_cast<WorkerContext*>(context);
    ASSERT(workerContext->isSharedWorkerContext());
    workerContext->dispatchEvent(createConnectEvent(port));
}

void WebSharedWorkerImpl::startWorkerContext(const WebURL& url, const WebString& name, const WebString& userAgent, const WebString& sourceCode, long long)
{
    initializeLoader(url);
    WorkerThreadStartMode startMode = m_pauseWorkerContextOnStart ? PauseWorkerContextOnStart : DontPauseWorkerContextOnStart;
    setWorkerThread(SharedWorkerThread::create(name, url, userAgent, sourceCode, *this, *this, startMode));
    workerThread()->start();
}

void WebSharedWorkerImpl::terminateWorkerContext()
{
    stopWorkerThread();
}

void WebSharedWorkerImpl::clientDestroyed()
{
    m_client = 0;
}

void WebSharedWorkerImpl::pauseWorkerContextOnStart()
{
    m_pauseWorkerContextOnStart = true;
}

static void resumeWorkerContextTask(ScriptExecutionContext* context, bool)
{
    ASSERT(context->isWorkerContext());
    static_cast<WorkerContext*>(context)->workerInspectorController()->resume();
}

void WebSharedWorkerImpl::resumeWorkerContext()
{
    m_pauseWorkerContextOnStart = false;
    if (workerThread())
        workerThread()->runLoop().postTaskForMode(createCallbackTask(resumeWorkerContextTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void connectToWorkerContextInspectorTask(ScriptExecutionContext* context, bool)
{
    ASSERT(context->isWorkerContext());
    static_cast<WorkerContext*>(context)->workerInspectorController()->connectFrontend();
}

void WebSharedWorkerImpl::attachDevTools()
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(connectToWorkerContextInspectorTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void reconnectToWorkerContextInspectorTask(ScriptExecutionContext* context, const String& savedState)
{
    ASSERT(context->isWorkerContext());
    WorkerInspectorController* ic = static_cast<WorkerContext*>(context)->workerInspectorController();
    ic->restoreInspectorStateFromCookie(savedState);
    ic->resume();
}

void WebSharedWorkerImpl::reattachDevTools(const WebString& savedState)
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(reconnectToWorkerContextInspectorTask, String(savedState)), WorkerDebuggerAgent::debuggerTaskMode);
}

static void disconnectFromWorkerContextInspectorTask(ScriptExecutionContext* context, bool)
{
    ASSERT(context->isWorkerContext());
    static_cast<WorkerContext*>(context)->workerInspectorController()->disconnectFrontend();
}

void WebSharedWorkerImpl::detachDevTools()
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(disconnectFromWorkerContextInspectorTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void dispatchOnInspectorBackendTask(ScriptExecutionContext* context, const String& message)
{
    ASSERT(context->isWorkerContext());
    static_cast<WorkerContext*>(context)->workerInspectorController()->dispatchMessageFromFrontend(message);
}

void WebSharedWorkerImpl::dispatchDevToolsMessage(const WebString& message)
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(dispatchOnInspectorBackendTask, String(message)), WorkerDebuggerAgent::debuggerTaskMode);
}

WebSharedWorker* WebSharedWorker::create(WebSharedWorkerClient* client)
{
    return new WebSharedWorkerImpl(client);
}
#endif // ENABLE(SHARED_WORKERS)

} // namespace WebKit
