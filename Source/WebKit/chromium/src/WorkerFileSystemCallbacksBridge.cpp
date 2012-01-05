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

#include "config.h"
#include "WorkerFileSystemCallbacksBridge.h"

#if ENABLE(FILE_SYSTEM) && ENABLE(WORKERS)

#include "CrossThreadTask.h"
#include "KURL.h"
#include "WebCommonWorkerClient.h"
#include "WebFileInfo.h"
#include "WebFileSystemCallbacks.h"
#include "WebFileSystemEntry.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
#include "WorkerLoaderProxy.h"
#include "WorkerScriptController.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

template<> struct CrossThreadCopierBase<false, false, WebKit::WebFileInfo> {
    typedef WebKit::WebFileInfo Type;
    static Type copy(const WebKit::WebFileInfo& info)
    {
        // Perform per-field copy to make sure we don't do any (unexpected) non-thread safe copy here.
        struct WebKit::WebFileInfo newInfo;
        newInfo.modificationTime = info.modificationTime;
        newInfo.length = info.length;
        newInfo.type = info.type;
        newInfo.platformPath.assign(info.platformPath.data(), info.platformPath.length());
        return newInfo;
    }
};

template<> struct CrossThreadCopierBase<false, false, WebKit::WebVector<WebKit::WebFileSystemEntry> > {
    typedef WebKit::WebVector<WebKit::WebFileSystemEntry> Type;
    static Type copy(const WebKit::WebVector<WebKit::WebFileSystemEntry>& entries)
    {
        WebKit::WebVector<WebKit::WebFileSystemEntry> newEntries(entries.size());
        for (size_t i = 0; i < entries.size(); ++i) {
            String name = entries[i].name;
            newEntries[i].isDirectory = entries[i].isDirectory;
            newEntries[i].name = name.isolatedCopy();
        }
        return newEntries;
    }
};

}

using namespace WebCore;

namespace WebKit {

// FileSystemCallbacks that are to be dispatched on the main thread.
class MainThreadFileSystemCallbacks : public WebFileSystemCallbacks {
public:
    // Callbacks are self-destructed and we always return leaked pointer here.
    static MainThreadFileSystemCallbacks* createLeakedPtr(PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
    {
        OwnPtr<MainThreadFileSystemCallbacks> callbacks = adoptPtr(new MainThreadFileSystemCallbacks(bridge, mode));
        return callbacks.leakPtr();
    }

    virtual ~MainThreadFileSystemCallbacks()
    {
    }

    virtual void didOpenFileSystem(const WebString& name, const WebURL& rootURL)
    {
        m_bridge->didOpenFileSystemOnMainThread(name, rootURL, m_mode);
        delete this;
    }

    virtual void didFail(WebFileError error)
    {
        m_bridge->didFailOnMainThread(error, m_mode);
        delete this;
    }

    virtual void didSucceed()
    {
        m_bridge->didSucceedOnMainThread(m_mode);
        delete this;
    }

    virtual void didReadMetadata(const WebFileInfo& info)
    {
        m_bridge->didReadMetadataOnMainThread(info, m_mode);
        delete this;
    }

    virtual void didReadDirectory(const WebVector<WebFileSystemEntry>& entries, bool hasMore)
    {
        m_bridge->didReadDirectoryOnMainThread(entries, hasMore, m_mode);
        delete this;
    }

private:
    MainThreadFileSystemCallbacks(PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
        : m_bridge(bridge)
        , m_mode(mode)
    {
        ASSERT(m_bridge);
    }

    RefPtr<WorkerFileSystemCallbacksBridge> m_bridge;
    const String m_mode;
};

// Observes the worker context. By keeping this separate, it is easier to verify
// that it only gets deleted on the worker context thread which is verified by ~Observer.
class WorkerFileSystemContextObserver : public WebCore::WorkerContext::Observer {
public:
    static PassOwnPtr<WorkerFileSystemContextObserver> create(WorkerContext* context, WorkerFileSystemCallbacksBridge* bridge)
    {
        return adoptPtr(new WorkerFileSystemContextObserver(context, bridge));
    }

    // WorkerContext::Observer method.
    virtual void notifyStop()
    {
        m_bridge->stop();
    }

private:
    WorkerFileSystemContextObserver(WorkerContext* context, WorkerFileSystemCallbacksBridge* bridge)
        : WebCore::WorkerContext::Observer(context)
        , m_bridge(bridge)
    {
    }

    // Since WorkerFileSystemCallbacksBridge manages the lifetime of this class,
    // m_bridge will be valid throughout its lifetime.
    WorkerFileSystemCallbacksBridge* m_bridge;
};

void WorkerFileSystemCallbacksBridge::stop()
{
    ASSERT(m_workerContext->isContextThread());
    {
        MutexLocker locker(m_loaderProxyMutex);
        m_workerLoaderProxy = 0;
    }

    if (m_callbacksOnWorkerThread)
        m_callbacksOnWorkerThread->didFail(WebFileErrorAbort);

    cleanUpAfterCallback();
}

void WorkerFileSystemCallbacksBridge::cleanUpAfterCallback()
{
    ASSERT(m_workerContext->isContextThread());

    m_callbacksOnWorkerThread = 0;
    if (m_workerContextObserver) {
        delete m_workerContextObserver;
        m_workerContextObserver = 0;
    }
}

void WorkerFileSystemCallbacksBridge::postOpenFileSystemToMainThread(WebCommonWorkerClient* commonClient, WebFileSystem::Type type, long long size, bool create, const String& mode)
{
    dispatchTaskToMainThread(
        createCallbackTask(&openFileSystemOnMainThread,
                           AllowCrossThreadAccess(commonClient), type, size, create,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postMoveToMainThread(WebFileSystem* fileSystem, const KURL& sourcePath, const KURL& destinationPath, const String& mode)
{
    dispatchTaskToMainThread(
        createCallbackTask(&moveOnMainThread,
                           AllowCrossThreadAccess(fileSystem), sourcePath, destinationPath,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postCopyToMainThread(WebFileSystem* fileSystem, const KURL& sourcePath, const KURL& destinationPath, const String& mode)
{
    dispatchTaskToMainThread(
        createCallbackTask(&copyOnMainThread,
                           AllowCrossThreadAccess(fileSystem), sourcePath, destinationPath,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postRemoveToMainThread(WebFileSystem* fileSystem, const KURL& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(
        createCallbackTask(&removeOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postRemoveRecursivelyToMainThread(WebFileSystem* fileSystem, const KURL& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(
        createCallbackTask(&removeRecursivelyOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postReadMetadataToMainThread(WebFileSystem* fileSystem, const KURL& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(
        createCallbackTask(&readMetadataOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postCreateFileToMainThread(WebFileSystem* fileSystem, const KURL& path, bool exclusive, const String& mode)
{
    dispatchTaskToMainThread(
        createCallbackTask(&createFileOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path, exclusive,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postCreateDirectoryToMainThread(WebFileSystem* fileSystem, const KURL& path, bool exclusive, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(
        createCallbackTask(&createDirectoryOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path, exclusive,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postFileExistsToMainThread(WebFileSystem* fileSystem, const KURL& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(
        createCallbackTask(&fileExistsOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postDirectoryExistsToMainThread(WebFileSystem* fileSystem, const KURL& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(
        createCallbackTask(&directoryExistsOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::postReadDirectoryToMainThread(WebFileSystem* fileSystem, const KURL& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(
        createCallbackTask(&readDirectoryOnMainThread,
                           AllowCrossThreadAccess(fileSystem), path,
                           this, mode));
}

void WorkerFileSystemCallbacksBridge::openFileSystemOnMainThread(ScriptExecutionContext*, WebCommonWorkerClient* commonClient, WebFileSystem::Type type, long long size, bool create, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    if (!commonClient)
        bridge->didFailOnMainThread(WebFileErrorAbort, mode);
    else {
        commonClient->openFileSystem(type, size, create, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
    }
}

void WorkerFileSystemCallbacksBridge::moveOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& sourcePath, const KURL& destinationPath, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->move(sourcePath, destinationPath, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::copyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& sourcePath, const KURL& destinationPath, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->copy(sourcePath, destinationPath, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::removeOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->remove(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::removeRecursivelyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->removeRecursively(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::readMetadataOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->readMetadata(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::createFileOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, bool exclusive, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->createFile(path, exclusive, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::createDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, bool exclusive, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->createDirectory(path, exclusive, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::fileExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->fileExists(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::directoryExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->directoryExists(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::readDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const KURL& path, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
{
    fileSystem->readDirectory(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::didFailOnMainThread(WebFileError error, const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didFailOnWorkerThread, this, error), mode);
}

void WorkerFileSystemCallbacksBridge::didOpenFileSystemOnMainThread(const String& name, const KURL& rootURL, const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didOpenFileSystemOnWorkerThread,
                                           this, name, rootURL), mode);
}

void WorkerFileSystemCallbacksBridge::didSucceedOnMainThread(const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didSucceedOnWorkerThread, this), mode);
}

void WorkerFileSystemCallbacksBridge::didReadMetadataOnMainThread(const WebFileInfo& info, const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didReadMetadataOnWorkerThread, this, info), mode);
}

void WorkerFileSystemCallbacksBridge::didReadDirectoryOnMainThread(const WebVector<WebFileSystemEntry>& entries, bool hasMore, const String& mode)
{
    mayPostTaskToWorker(
        createCallbackTask(&didReadDirectoryOnWorkerThread,
                           this, entries, hasMore), mode);
}

WorkerFileSystemCallbacksBridge::WorkerFileSystemCallbacksBridge(WebCore::WorkerLoaderProxy* workerLoaderProxy, ScriptExecutionContext* scriptExecutionContext, WebFileSystemCallbacks* callbacks)
    : m_workerLoaderProxy(workerLoaderProxy)
    , m_workerContext(scriptExecutionContext)
    , m_workerContextObserver(WorkerFileSystemContextObserver::create(static_cast<WorkerContext*>(m_workerContext), this).leakPtr())
    , m_callbacksOnWorkerThread(callbacks)
{
    ASSERT(m_workerContext->isContextThread());
}

WorkerFileSystemCallbacksBridge::~WorkerFileSystemCallbacksBridge()
{
    ASSERT(!m_callbacksOnWorkerThread);
}

void WorkerFileSystemCallbacksBridge::didFailOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, WebFileError error)
{
    bridge->m_callbacksOnWorkerThread->didFail(error);
}

void WorkerFileSystemCallbacksBridge::didOpenFileSystemOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& name, const KURL& rootURL)
{
    bridge->m_callbacksOnWorkerThread->didOpenFileSystem(name, rootURL);
}

void WorkerFileSystemCallbacksBridge::didSucceedOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge)
{
    bridge->m_callbacksOnWorkerThread->didSucceed();
}

void WorkerFileSystemCallbacksBridge::didReadMetadataOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const WebFileInfo& info)
{
    bridge->m_callbacksOnWorkerThread->didReadMetadata(info);
}

void WorkerFileSystemCallbacksBridge::didReadDirectoryOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const WebVector<WebFileSystemEntry>& entries, bool hasMore)
{
    bridge->m_callbacksOnWorkerThread->didReadDirectory(entries, hasMore);
}


void WorkerFileSystemCallbacksBridge::runTaskOnMainThread(WebCore::ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, PassOwnPtr<WebCore::ScriptExecutionContext::Task> taskToRun)
{
    ASSERT(isMainThread());
    taskToRun->performTask(scriptExecutionContext);
}

void WorkerFileSystemCallbacksBridge::runTaskOnWorkerThread(WebCore::ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, PassOwnPtr<WebCore::ScriptExecutionContext::Task> taskToRun)
{
    if (!bridge->m_callbacksOnWorkerThread)
        return;
    ASSERT(bridge->m_workerContext->isContextThread());
    taskToRun->performTask(scriptExecutionContext);

    // taskToRun does the callback.
    bridge->cleanUpAfterCallback();

    // WorkerFileSystemCallbacksBridge may be deleted here when bridge goes out of scope.
}

void WorkerFileSystemCallbacksBridge::dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task> task)
{
    ASSERT(m_workerLoaderProxy);
    ASSERT(m_workerContext->isContextThread());
    WebWorkerBase::dispatchTaskToMainThread(createCallbackTask(&runTaskOnMainThread, RefPtr<WorkerFileSystemCallbacksBridge>(this).release(), task));
}

void WorkerFileSystemCallbacksBridge::mayPostTaskToWorker(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    // Relies on its caller (MainThreadFileSystemCallbacks:did*) to keep WorkerFileSystemCallbacksBridge alive.
    ASSERT(isMainThread());

    MutexLocker locker(m_loaderProxyMutex);
    if (m_workerLoaderProxy)
        m_workerLoaderProxy->postTaskForModeToWorkerContext(createCallbackTask(&runTaskOnWorkerThread, this, task), mode);
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
