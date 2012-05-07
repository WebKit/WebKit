/*
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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

#if ENABLE(FILE_SYSTEM) && ENABLE(WORKERS)

#include "PlatformString.h"
#include "ScriptExecutionContext.h"
#include "WebFileError.h"
#include "platform/WebVector.h"
#include <public/WebFileSystem.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {
class WorkerLoaderProxy;
}

namespace WebKit {

class AsyncFileSystem;
class MainThreadFileSystemCallbacks;
class WebCommonWorkerClient;
class ThreadableCallbacksBridgeWrapper;
class WebFileSystemCallbacks;
class WorkerFileSystemContextObserver;
struct WebFileInfo;
struct WebFileSystemEntry;

// Used to post a openFileSystem request to the main thread and get called back for the request.
//
// Lifetime for this class is maintained by posted "tasks" which ref count it and by MainThreadFileSystemCallbacks.
// Either a task finishing or the MainThreadFileSystemCallbacks being deleted may release the last ref on WorkerFileSystemCallbacksBridge.
//
// A typical flow for openFileSystem would look like this:
// Bridge::postOpenFileSystemToMainThread() on WorkerThread
//  --> Bridge::openFileSystemOnMainThread() is called on MainThread
//      This makes an IPC with a MainThreadFileSystemCallbacks instance
//     [actual operation is down in the browser]
//  --> MainThreadFileSystemCallbacks::didXxx is called on MainThread
//  --> Bridge::didXxxOnMainThread is called on MainThread
//  --> Bridge::didXxxOnWorkerThread is called on WorkerThread
//      This calls the original callbacks (m_callbacksOnWorkerThread).
class WorkerFileSystemCallbacksBridge : public ThreadSafeRefCounted<WorkerFileSystemCallbacksBridge> {
public:
    ~WorkerFileSystemCallbacksBridge();

    void stop();

    static PassRefPtr<WorkerFileSystemCallbacksBridge> create(WebCore::WorkerLoaderProxy* workerLoaderProxy, WebCore::ScriptExecutionContext* workerContext, WebFileSystemCallbacks* callbacks)
    {
        return adoptRef(new WorkerFileSystemCallbacksBridge(workerLoaderProxy, workerContext, callbacks));
    }

    // Methods that create an instance and post an initial request task to the main thread. They must be called on the worker thread.
    void postOpenFileSystemToMainThread(WebCommonWorkerClient*, WebFileSystem::Type, long long size, bool create, const String& mode);
    void postMoveToMainThread(WebFileSystem*, const WebCore::KURL& srcPath, const WebCore::KURL& destPath, const String& mode);
    void postCopyToMainThread(WebFileSystem*, const WebCore::KURL& srcPath, const WebCore::KURL& destPath, const String& mode);
    void postRemoveToMainThread(WebFileSystem*, const WebCore::KURL& path, const String& mode);
    void postRemoveRecursivelyToMainThread(WebFileSystem*, const WebCore::KURL& path, const String& mode);
    void postReadMetadataToMainThread(WebFileSystem*, const WebCore::KURL& path, const String& mode);
    void postCreateFileToMainThread(WebFileSystem*, const WebCore::KURL& path, bool exclusive, const String& mode);
    void postCreateDirectoryToMainThread(WebFileSystem*, const WebCore::KURL& path, bool exclusive, const String& mode);
    void postFileExistsToMainThread(WebFileSystem*, const WebCore::KURL& path, const String& mode);
    void postDirectoryExistsToMainThread(WebFileSystem*, const WebCore::KURL& path, const String& mode);
    void postReadDirectoryToMainThread(WebFileSystem*, const WebCore::KURL& path, const String& mode);
    void postCreateSnapshotFileToMainThread(WebFileSystem*, const WebCore::KURL& internalBlobURL, const WebCore::KURL& path, const String& mode);

    // Callback methods that are called on the main thread.
    void didFailOnMainThread(WebFileError, const String& mode);
    void didOpenFileSystemOnMainThread(const String& name, const WebCore::KURL& rootURL, const String& mode);
    void didSucceedOnMainThread(const String& mode);
    void didReadMetadataOnMainThread(const WebFileInfo&, const String& mode);
    void didReadDirectoryOnMainThread(const WebVector<WebFileSystemEntry>&, bool hasMore, const String& mode);

private:
    WorkerFileSystemCallbacksBridge(WebCore::WorkerLoaderProxy*, WebCore::ScriptExecutionContext*, WebFileSystemCallbacks*);

    // Methods that are to be called on the main thread.
    static void openFileSystemOnMainThread(WebCore::ScriptExecutionContext*, WebCommonWorkerClient*, WebFileSystem::Type, long long size, bool create, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void moveOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& srcPath, const WebCore::KURL& destPath, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void copyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& srcPath, const WebCore::KURL& destPath, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void removeOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void removeRecursivelyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void readMetadataOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void createFileOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, bool exclusive, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void createDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, bool exclusive, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void fileExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void directoryExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void readDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);
    static void createSnapshotFileOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem*, const WebCore::KURL& internalBlobURL, const WebCore::KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& mode);

    friend class MainThreadFileSystemCallbacks;

    // Methods that dispatch WebFileSystemCallbacks on the worker threads.
    static void didFailOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, WebFileError);
    static void didOpenFileSystemOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, const String& name, const WebCore::KURL& rootPath);
    static void didSucceedOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>);
    static void didReadMetadataOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, const WebFileInfo&);
    static void didReadDirectoryOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, const WebVector<WebFileSystemEntry>&, bool hasMore);

    static void runTaskOnMainThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    static void runTaskOnWorkerThread(WebCore::ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);

    void dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    void mayPostTaskToWorker(PassOwnPtr<WebCore::ScriptExecutionContext::Task>, const String& mode);

    void cleanUpAfterCallback();

    Mutex m_loaderProxyMutex;
    WebCore::WorkerLoaderProxy* m_workerLoaderProxy;

    WebCore::ScriptExecutionContext* m_workerContext;

    // Must be deleted on the WorkerContext thread.
    WorkerFileSystemContextObserver* m_workerContextObserver;

    // This is self-destructed and must be fired on the worker thread.
    WebFileSystemCallbacks* m_callbacksOnWorkerThread;
};

} // namespace WebCore

#endif

#endif // WorkerFileSystemCallbacksBridge_h
