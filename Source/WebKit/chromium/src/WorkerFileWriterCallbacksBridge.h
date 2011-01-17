/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WorkerFileWriterCallbacksBridge_h
#define WorkerFileWriterCallbacksBridge_h

#if ENABLE(FILE_SYSTEM)

#include "WebFileError.h"
#include "WebFileWriterClient.h"
#include "WorkerContext.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/ThreadSafeShared.h>

namespace WebCore {
    class AsyncFileWriterClient;
    class KURL;
    class WorkerLoaderProxy;
}

namespace WTF {
    class String;
}
using WTF::String;

namespace WebKit {

class WebFileSystem;
class WebFileWriter;
class WebFileWriterClient;
class WebWorkerBase;

// This class is used as a mechanism to bridge calls between threads.
// Calls to a WebFileWriter must happen on the main thread, but they come from
// the context thread. The responses through the WebFileWriterClient interface
// start on the main thread, but must be sent via the worker context thread.
//
// A typical flow for write would look like this:
// Bridge::postWriteToMainThread() on WorkerThread
//  --> Bridge::writeOnMainThread() is called on MainThread
//  --> WebFileWriter::write()
//      This makes an IPC; the actual operation is down in the browser.
//  --> Bridge::didWrite is called on MainThread
//  --> Bridge::didWriteOnWorkerThread is called on WorkerThread
//      This calls the original client (m_clientOnWorkerThread).
//
// The bridge object is refcounted, so that it doesn't get deleted while there
// are cross-thread calls in flight. Each CrossThreadTask carries a reference
// to the bridge, which guarantees that the bridge will still be valid when the
// task is executed. In order to shut down the bridge, the WebFileWriterClient
// should call postShutdownToMainThread before dropping its reference to the
// bridge. This ensures that the WebFileWriter will be cleared on the main
// thread and that no further calls to the WebFileWriterClient will be made.
class WorkerFileWriterCallbacksBridge : public ThreadSafeShared<WorkerFileWriterCallbacksBridge>, public WebCore::WorkerContext::Observer, public WebFileWriterClient {
public:
    ~WorkerFileWriterCallbacksBridge();

    // WorkerContext::Observer method.
    virtual void notifyStop();

    static PassRefPtr<WorkerFileWriterCallbacksBridge> create(const String& path, WebCore::WorkerLoaderProxy* proxy, WebCore::ScriptExecutionContext* workerContext, WebCore::AsyncFileWriterClient* client)
    {
        return adoptRef(new WorkerFileWriterCallbacksBridge(path, proxy, workerContext, client));
    }

    // Methods that create an instance and post an initial request task to the main thread. They must be called on the worker thread.
    void postWriteToMainThread(long long position, const WebCore::KURL& data);
    void postTruncateToMainThread(long long length);
    void postAbortToMainThread();

    // The owning WorkerAsyncFileWriterChromium should call this method before dropping its last reference to the bridge, on the context thread.
    // The actual deletion of the WorkerFileWriterCallbacksBridge may happen on either the main or context thread, depending on where the last reference goes away; that's safe as long as this is called first.
    void postShutdownToMainThread(PassRefPtr<WorkerFileWriterCallbacksBridge>);

    // Callback methods that are called on the main thread.
    // These are the implementation of WebKit::WebFileWriterClient.
    void didWrite(long long bytes, bool complete);
    void didFail(WebFileError);
    void didTruncate();

    // Call this on the context thread to wait for the current operation to complete.
    bool waitForOperationToComplete();

private:
    WorkerFileWriterCallbacksBridge(const String& path, WebCore::WorkerLoaderProxy*, WebCore::ScriptExecutionContext*, WebCore::AsyncFileWriterClient*);

    void postInitToMainThread(const String& path);

    // Methods that are to be called on the main thread.
    static void writeOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>, long long position, const WebCore::KURL& data);
    static void truncateOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>, long long length);
    static void abortOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>);
    static void initOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>, const String& path);
    static void shutdownOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>);

    // Methods that dispatch to AsyncFileWriterClient on the worker threads.
    static void didWriteOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>, long long length, bool complete);
    static void didFailOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>, WebFileError);
    static void didTruncateOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>);

    // Called on the main thread to run the supplied task.
    static void runTaskOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    // Called on the worker thread to run the supplied task.
    static void runTaskOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);

    // Called on the worker thread to dispatch to the main thread.
    void dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    // Called on the main thread to dispatch to the worker thread.
    void dispatchTaskToWorkerThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);

    // Used from the main thread to post tasks to the context thread.
    WebCore::WorkerLoaderProxy* m_proxy;

    // Used on the context thread, only to check that we're running on the context thread.
    WebCore::ScriptExecutionContext* m_workerContext;

    // Created and destroyed from the main thread.
    OwnPtr<WebKit::WebFileWriter> m_writer;

    // Used on the context thread to call back into the client.
    WebCore::AsyncFileWriterClient* m_clientOnWorkerThread;

    // Used to indicate that shutdown has started on the main thread, and hence the writer has been deleted.
    bool m_writerDeleted;

    // Used by waitForOperationToComplete.
    bool m_operationInProgress;

    // Used by postTaskForModeToWorkerContext and runInMode.
    String m_mode;
};

} // namespace WebCore

#endif

#endif // WorkerFileWriterCallbacksBridge_h
