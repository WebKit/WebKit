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

#ifndef WebWorkerBase_h
#define WebWorkerBase_h

#if ENABLE(WORKERS)

#include "ScriptExecutionContext.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class WorkerThread;
}

namespace WebKit {
class WebCommonWorkerClient;
class WebURL;
class WebView;
class WebWorkerClient;

// Base class for WebSharedWorkerImpl and WebWorkerImpl. It contains common
// code used by both implementation classes, including implementations of the
// WorkerObjectProxy and WorkerLoaderProxy interfaces.
class WebWorkerBase : public WebCore::WorkerObjectProxy
                    , public WebCore::WorkerLoaderProxy {
public:
    WebWorkerBase();
    virtual ~WebWorkerBase();

    // WebCore::WorkerObjectProxy methods:
    virtual void postMessageToWorkerObject(
        PassRefPtr<WebCore::SerializedScriptValue>,
        PassOwnPtr<WebCore::MessagePortChannelArray>);
    virtual void postExceptionToWorkerObject(
        const WebCore::String&, int, const WebCore::String&);
    virtual void postConsoleMessageToWorkerObject(
        WebCore::MessageDestination, WebCore::MessageSource, WebCore::MessageType,
        WebCore::MessageLevel, const WebCore::String&, int, const WebCore::String&);
    virtual void confirmMessageFromWorkerObject(bool);
    virtual void reportPendingActivity(bool);
    virtual void workerContextClosed();
    virtual void workerContextDestroyed();

    // WebCore::WorkerLoaderProxy methods:
    virtual void postTaskToLoader(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    virtual void postTaskForModeToWorkerContext(
        PassOwnPtr<WebCore::ScriptExecutionContext::Task>, const WebCore::String& mode);

    // Executes the given task on the main thread.
    static void dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);

protected:
    virtual WebWorkerClient* client() = 0;
    virtual WebCommonWorkerClient* commonClient() = 0;

    void setWorkerThread(PassRefPtr<WebCore::WorkerThread> thread) { m_workerThread = thread; }
    WebCore::WorkerThread* workerThread() { return m_workerThread.get(); }

    // Shuts down the worker thread.
    void stopWorkerThread();

    // Creates the shadow loader used for worker network requests.
    void initializeLoader(const WebURL&);

private:
    // Function used to invoke tasks on the main thread.
    static void invokeTaskMethod(void*);

    // Tasks that are run on the main thread.
    static void postMessageTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerBase* thisPtr,
        WebCore::String message,
        PassOwnPtr<WebCore::MessagePortChannelArray> channels);
    static void postExceptionTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerBase* thisPtr,
        const WebCore::String& message,
        int lineNumber,
        const WebCore::String& sourceURL);
    static void postConsoleMessageTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerBase* thisPtr,
        int destination,
        int source,
        int type,
        int level,
        const WebCore::String& message,
        int lineNumber,
        const WebCore::String& sourceURL);
    static void confirmMessageTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerBase* thisPtr,
        bool hasPendingActivity);
    static void reportPendingActivityTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerBase* thisPtr,
        bool hasPendingActivity);
    static void workerContextClosedTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerBase* thisPtr);
    static void workerContextDestroyedTask(
        WebCore::ScriptExecutionContext* context,
        WebWorkerBase* thisPtr);

    // 'shadow page' - created to proxy loading requests from the worker.
    RefPtr<WebCore::ScriptExecutionContext> m_loadingDocument;
    WebView* m_webView;
    bool m_askedToTerminate;

    RefPtr<WebCore::WorkerThread> m_workerThread;
};

} // namespace WebKit

#endif // ENABLE(WORKERS)

#endif
