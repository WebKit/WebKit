/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WorkerStorageQuotaCallbacksBridge_h
#define WorkerStorageQuotaCallbacksBridge_h

#if ENABLE(QUOTA) && ENABLE(WORKERS)

#include "ScriptExecutionContext.h"
#include "StorageArea.h"
#include <WebStorageQuotaError.h>
#include <WebStorageQuotaType.h>
#include <public/WebVector.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class WorkerLoaderProxy;
}

namespace WebKit {

class MainThreadStorageQuotaCallbacks;
class WebCommonWorkerClient;
class WebStorageQuotaCallbacksImpl;
class WorkerStorageQuotaContextObserver;

// Used to post a queryUsageAndQuota request to the main thread and get called back for the request.
//
// Lifetime for this class is maintained by posted "tasks" which ref count it and by MainThreadStorageQuotaCallbacks.
// Either a task finishing or the MainThreadStorageQuotaCallbacks being deleted may release the last ref on WorkerStorageQuotaCallbacksBridge.
//
// A typical flow for queryUsageAndQuota would look like this:
// Bridge::postQueryUsageAndQuotaToMainThread() on WorkerThread
//  --> Bridge::queryUsageAndQuotaOnMainThread() is called on MainThread
//      This makes an IPC with a MainThreadStorageQuotaCallbacks instance
//     [actual operation is down in the browser]
//  --> MainThreadStorageQuotaCallbacks::didXxx is called on MainThread
//  --> Bridge::didXxxOnMainThread is called on MainThread
//  --> Bridge::didXxxOnWorkerThread is called on WorkerThread
//      This calls the original callbacks (m_callbacksOnWorkerThread).
class WorkerStorageQuotaCallbacksBridge : public ThreadSafeRefCounted<WorkerStorageQuotaCallbacksBridge> {
public:
    ~WorkerStorageQuotaCallbacksBridge();

    void stop();

    static PassRefPtr<WorkerStorageQuotaCallbacksBridge> create(WebCore::WorkerLoaderProxy* workerLoaderProxy, WebCore::ScriptExecutionContext* workerContext, WebStorageQuotaCallbacksImpl* callbacks)
    {
        return adoptRef(new WorkerStorageQuotaCallbacksBridge(workerLoaderProxy, workerContext, callbacks));
    }

    // Entry method to post QueryUsageAndQuota task to main thread.
    void postQueryUsageAndQuotaToMainThread(WebCommonWorkerClient*, WebStorageQuotaType, const String& mode);

    // Callback methods that are called on the main thread.
    void didFailOnMainThread(WebStorageQuotaError, const String& mode);
    void didQueryStorageUsageAndQuotaOnMainThread(unsigned long long usageInBytes, unsigned long long quotaInBytes, const String& mode);

private:
    WorkerStorageQuotaCallbacksBridge(WebCore::WorkerLoaderProxy*, WebCore::ScriptExecutionContext*, WebStorageQuotaCallbacksImpl*);

    // Method that is called on the main thread.
    static void runTaskOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerStorageQuotaCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    static void runTaskOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerStorageQuotaCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    static void queryUsageAndQuotaOnMainThread(WebCore::ScriptExecutionContext*, WebCommonWorkerClient*, WebStorageQuotaType, PassRefPtr<WorkerStorageQuotaCallbacksBridge>, const String& mode);

    friend class MainThreadStorageQuotaCallbacks;

    // Methods that dispatch WebStorageQuotaCallbacks on the worker threads.
    static void didFailOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerStorageQuotaCallbacksBridge>, WebStorageQuotaError);
    static void didQueryStorageUsageAndQuotaOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerStorageQuotaCallbacksBridge>, unsigned long long usageInBytes, unsigned long long quotaInBytes);

    void dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    void mayPostTaskToWorker(PassOwnPtr<WebCore::ScriptExecutionContext::Task>, const String& mode);

    void cleanUpAfterCallback();

    Mutex m_loaderProxyMutex;
    WebCore::WorkerLoaderProxy* m_workerLoaderProxy;

    WebCore::ScriptExecutionContext* m_workerContext;

    // Must be deleted on the WorkerContext thread.
    WorkerStorageQuotaContextObserver* m_workerContextObserver;

    // This is self-destructed and must be fired on the worker thread.
    WebStorageQuotaCallbacksImpl* m_callbacksOnWorkerThread;
};

} // namespace WebCore

#endif

#endif // WorkerStorageQuotaCallbacksBridge_h
