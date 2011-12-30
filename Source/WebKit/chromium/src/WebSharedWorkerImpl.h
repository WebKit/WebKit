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

#ifndef WebSharedWorkerImpl_h
#define WebSharedWorkerImpl_h

#include "WebSharedWorker.h"

#if ENABLE(SHARED_WORKERS)
#include "ScriptExecutionContext.h"
#include "WebCommonWorkerClient.h"
#include "WebFrameClient.h"
#include "WebWorkerBase.h"
#include "WebWorkerClient.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include "WorkerThread.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>


namespace WebKit {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebWorkerClient;
class WebSecurityOrigin;
class WebString;
class WebURL;
class WebView;
class WebWorker;
class WebSharedWorkerClient;
// This class is used by the worker process code to talk to the WebCore::SharedWorker implementation.
// It can't use it directly since it uses WebKit types, so this class converts the data types.
// When the WebCore::SharedWorker object wants to call WebCore::WorkerReportingProxy, this class will
// convert to Chrome data types first and then call the supplied WebCommonWorkerClient.
class WebSharedWorkerImpl : public WebCore::WorkerObjectProxy
                          , public WebWorkerBase
                          , public WebFrameClient
                          , public WebSharedWorker {
public:
    explicit WebSharedWorkerImpl(WebSharedWorkerClient*);

    virtual void postMessageToWorkerObject(
        PassRefPtr<WebCore::SerializedScriptValue>,
        PassOwnPtr<WebCore::MessagePortChannelArray>);
    virtual void postExceptionToWorkerObject(
        const WTF::String&, int, const WTF::String&);
    virtual void postConsoleMessageToWorkerObject(
        WebCore::MessageSource, WebCore::MessageType,
        WebCore::MessageLevel, const WTF::String&, int, const WTF::String&);
    virtual void postMessageToPageInspector(const WTF::String&);
    virtual void updateInspectorStateCookie(const WTF::String&);
    virtual void confirmMessageFromWorkerObject(bool);
    virtual void reportPendingActivity(bool);
    virtual void workerContextClosed();
    virtual void workerContextDestroyed();
    virtual WebView* view() const { return m_webView; }

    // WebCore::WorkerLoaderProxy methods:
    virtual void postTaskToLoader(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    virtual void postTaskForModeToWorkerContext(
        PassOwnPtr<WebCore::ScriptExecutionContext::Task>, const WTF::String& mode);

    // WebFrameClient methods to support resource loading thru the 'shadow page'.
    virtual void didCreateDataSource(WebFrame*, WebDataSource*);
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebFrame*, WebApplicationCacheHostClient*);


    // WebSharedWorker methods:
    virtual bool isStarted();
    virtual void startWorkerContext(const WebURL&, const WebString& name, const WebString& userAgent, const WebString& sourceCode, long long);
    virtual void connect(WebMessagePortChannel*, ConnectListener*);
    virtual void terminateWorkerContext();
    virtual void clientDestroyed();

    virtual void pauseWorkerContextOnStart();
    virtual void resumeWorkerContext();
    virtual void attachDevTools();
    virtual void reattachDevTools(const WebString& savedState);
    virtual void detachDevTools();
    virtual void dispatchDevToolsMessage(const WebString&);


    // NewWebWorkerBase methods:
    WebCommonWorkerClient* commonClient() { return m_client; }

private:
    virtual ~WebSharedWorkerImpl();

    WebSharedWorkerClient* client() { return m_client; }

    void setWorkerThread(PassRefPtr<WebCore::WorkerThread> thread) { m_workerThread = thread; }
    WebCore::WorkerThread* workerThread() { return m_workerThread.get(); }

    // Shuts down the worker thread.
    void stopWorkerThread();

    // Creates the shadow loader used for worker network requests.
    void initializeLoader(const WebURL&);


    static void connectTask(WebCore::ScriptExecutionContext*, PassOwnPtr<WebCore::MessagePortChannel>);
    // Tasks that are run on the main thread.
    static void postMessageTask(
        WebCore::ScriptExecutionContext*,
        WebSharedWorkerImpl* thisPtr,
        WTF::String message,
        PassOwnPtr<WebCore::MessagePortChannelArray> channels);
    static void postExceptionTask(
        WebCore::ScriptExecutionContext*,
        WebSharedWorkerImpl* thisPtr,
        const WTF::String& message,
        int lineNumber,
        const WTF::String& sourceURL);
    static void postConsoleMessageTask(
        WebCore::ScriptExecutionContext*,
        WebSharedWorkerImpl* thisPtr,
        int source,
        int type,
        int level,
        const WTF::String& message,
        int lineNumber,
        const WTF::String& sourceURL);
    static void postMessageToPageInspectorTask(WebCore::ScriptExecutionContext*, WebSharedWorkerImpl*, const WTF::String&);
    static void updateInspectorStateCookieTask(WebCore::ScriptExecutionContext*, WebSharedWorkerImpl* thisPtr, const WTF::String& cookie);
    static void confirmMessageTask(
        WebCore::ScriptExecutionContext*,
        WebSharedWorkerImpl* thisPtr,
        bool hasPendingActivity);
    static void reportPendingActivityTask(
        WebCore::ScriptExecutionContext*,
        WebSharedWorkerImpl* thisPtr,
        bool hasPendingActivity);
    static void workerContextClosedTask(
        WebCore::ScriptExecutionContext*,
        WebSharedWorkerImpl* thisPtr);
    static void workerContextDestroyedTask(
        WebCore::ScriptExecutionContext*,
        WebSharedWorkerImpl* thisPtr);

    // 'shadow page' - created to proxy loading requests from the worker.
    RefPtr<WebCore::ScriptExecutionContext> m_loadingDocument;
    WebView* m_webView;
    bool m_askedToTerminate;

    RefPtr<WebCore::WorkerThread> m_workerThread;

    WebSharedWorkerClient* m_client;
    bool m_pauseWorkerContextOnStart;
};

} // namespace WebKit

#endif // ENABLE(SHARED_WORKERS)

#endif
