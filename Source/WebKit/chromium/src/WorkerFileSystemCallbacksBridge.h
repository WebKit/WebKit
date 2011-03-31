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

#ifndef WorkerFileSystemCallbacksBridge_h
#define WorkerFileSystemCallbacksBridge_h

#if ENABLE(FILE_SYSTEM)

#include "PlatformString.h"
#include "ScriptExecutionContext.h"
#include "WebFileError.h"
#include "WebFileSystem.h"
#include "WebVector.h"
#include "WorkerContext.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

namespace WebKit {

class AsyncFileSystem;
class MainThreadFileSystemCallbacks;
class ThreadableCallbacksBridgeWrapper;
class WebCommonWorkerClient;
class WebFileSystemCallbacks;
class WebWorkerBase;
struct WebFileInfo;
struct WebFileSystemEntry;

// This class is used to post a openFileSystem request to the main thread and get called back for the request. This must be destructed on the worker thread.
//
// A typical flow for openFileSystem would look like this:
// Bridge::postOpenFileSystemToMainThread() on WorkerThread
//  --> Bridge::openFileSystemOnMainThread() is called on MainThread
//      This makes an IPC with a MainThreadFileSystemCallbacks instance
//     [actual operation is down in the browser]
//  --> MainThreadFileSystemCallbacks::didXxx is called on MainThread
//  --> Bridge::didXxxOnMainThread is called on MainThread
//  --> Bridge::didXxxOnWorkerThread is called on WorkerThread
//      This calls the original callbacks (m_callbacksOnWorkerThread) and
//      releases a self-reference to the bridge.
class WorkerFileSystemCallbacksBridge : public ThreadSafeRefCounted<WorkerFileSystemCallbacksBridge>, public WebCore::WorkerContext::Observer {
public:
    ~WorkerFileSystemCallbacksBridge();

    // WorkerContext::Observer method.
    virtual void notifyStop()
    {
        stop();
    }

    void stop();

    static PassRefPtr<WorkerFileSystemCallbacksBridge> create(WebWorkerBase* worker, WebCore::ScriptExecutionContext* workerContext, WebFileSystemCallbacks* callbacks)
    {
        return adoptRef(new WorkerFileSystemCallbacksBridge(worker, workerContext, callbacks));
    }

    // Methods that create an instance and post an initial request task to the main thread. They must be called on the worker thread.
    void postOpenFileSystemToMainThread(WebCommonWorkerClient*, WebFileSystem::Type, long long size, bool create, const String& mode);
    void postMoveToMainThread(WebFileSystem*, const String& srcPath, const String& destPath, const String& mode);
    void postCopyToMainThread(WebFileSystem*, const String& srcPath, const String& destPath, const String& mode);
    void postRemoveToMainThread(WebFileSystem*, const String& path, const String& mode);
    void postRemoveRecursivelyToMainThread(WebFileSystem*, const String& path, const String& mode);
    void postReadMetadataToMainThread(WebFileSystem*, const String& path, const String& mode);
    void postCreateFileToMainThread(WebFileSystem*, const String& path, bool exclusive, const String& mode);
    void postCreateDirectoryToMainThread(WebFileSystem*, const String& path, bool exclusive, const String& mode);
    void postFileExistsToMainThread(WebFileSystem*, const String& path, const String& mode);
    void postDirectoryExistsToMainThread(WebFileSystem*, const String& path, const String& mode);
    void postReadDirectoryToMainThread(WebFileSystem*, const String& path, const String& mode);

    // Callback methods that are called on the main thread.
    void didFailOnMainThread(WebFileError, const String& mode);
    void didOpenFileSystemOnMainThread(const String& name, const String& rootPath, const String& mode);
    void didSucceedOnMainThread(const String& mode);
    void didReadMetadataOnMainThread(const WebFileInfo&, const String& mode);
    void didReadDirectoryOnMainThread(const WebVector<WebFileSystemEntry>&, bool hasMore, const String& mode);

private:
    WorkerFileSystemCallbacksBridge(WebWorkerBase*, WebCore::ScriptExecutionContext*, WebFileSystemCallbacks*);

    // Methods that are to be called on the main thread.
    static void openFileSystemOnMainThread(WebCore::ScriptExecutionContext*, WebCommonWorkerClient*, WebFileSystem::Type, long long size, bool create, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void moveOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& srcPath, const String& destPath, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void copyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& srcPath, const String& destPath, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void removeOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void removeRecursivelyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void readMetadataOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void createFileOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, bool exclusive, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void createDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, bool exclusive, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void fileExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void directoryExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, WorkerFileSystemCallbacksBridge*, const String& mode);
    static void readDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const String& path, WorkerFileSystemCallbacksBridge*, const String& mode);

    friend class MainThreadFileSystemCallbacks;

    // Methods that dispatch WebFileSystemCallbacks on the worker threads.
    static void didFailOnWorkerThread(WebCore::ScriptExecutionContext*, WorkerFileSystemCallbacksBridge*, WebFileError);
    static void didOpenFileSystemOnWorkerThread(WebCore::ScriptExecutionContext*, WorkerFileSystemCallbacksBridge*, const String& name, const String& rootPath);
    static void didSucceedOnWorkerThread(WebCore::ScriptExecutionContext*, WorkerFileSystemCallbacksBridge*);
    static void didReadMetadataOnWorkerThread(WebCore::ScriptExecutionContext*, WorkerFileSystemCallbacksBridge*, const WebFileInfo&);
    static void didReadDirectoryOnWorkerThread(WebCore::ScriptExecutionContext*, WorkerFileSystemCallbacksBridge*, const WebVector<WebFileSystemEntry>&, bool hasMore);

    static void runTaskOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    static void runTaskOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);

    void dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    void mayPostTaskToWorker(PassOwnPtr<WebCore::ScriptExecutionContext::Task>, const String& mode);

    Mutex m_mutex;
    WebWorkerBase* m_worker;
    WebCore::ScriptExecutionContext* m_workerContext;

    // This is self-destructed and must be fired on the worker thread.
    WebFileSystemCallbacks* m_callbacksOnWorkerThread;
};

} // namespace WebCore

#endif

#endif // WorkerFileSystemCallbacksBridge_h
