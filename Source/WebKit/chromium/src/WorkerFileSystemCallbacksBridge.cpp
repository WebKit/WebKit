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

#include "config.h"
#include "WorkerFileSystemCallbacksBridge.h"

#if ENABLE(FILE_SYSTEM)

#include "CrossThreadTask.h"
#include "WebCommonWorkerClient.h"
#include "WebFileInfo.h"
#include "WebFileSystemCallbacks.h"
#include "WebFileSystemEntry.h"
#include "WebString.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
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
            newEntries[i].name = name.crossThreadString();
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
    static MainThreadFileSystemCallbacks* createLeakedPtr(WorkerFileSystemCallbacksBridge* bridge, const String& mode)
    {
        OwnPtr<MainThreadFileSystemCallbacks> callbacks = adoptPtr(new MainThreadFileSystemCallbacks(bridge, mode));
        return callbacks.leakPtr();
    }

    virtual ~MainThreadFileSystemCallbacks()
    {
    }

    virtual void didOpenFileSystem(const WebString& name, const WebString& path)
    {
        m_bridge->didOpenFileSystemOnMainThread(name, path, m_mode);
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
    MainThreadFileSystemCallbacks(WorkerFileSystemCallbacksBridge* bridge, const String& mode)
        : m_bridge(bridge)
        , m_mode(mode)
    {
        ASSERT(m_bridge);
    }

    friend class WorkerFileSystemCallbacksBridge;
    // The bridge pointer is kept by the bridge itself on the WorkerThread.
    WorkerFileSystemCallbacksBridge* m_bridge;
    const String m_mode;
};

void WorkerFileSystemCallbacksBridge::stop()
{
    ASSERT(m_workerContext->isContextThread());
    MutexLocker locker(m_mutex);
    m_worker = 0;

    if (m_callbacksOnWorkerThread) {
        m_callbacksOnWorkerThread->didFail(WebFileErrorAbort);
        m_callbacksOnWorkerThread = 0;
    }
}

void WorkerFileSystemCallbacksBridge::postOpenFileSystemToMainThread(WebCommonWorkerClient* commonClient, WebFileSystem::Type type, long long size, bool create, const String& mode)
{
    dispatchTaskToMainThread(createCallbackTask(&openFileSystemOnMainThread, commonClient, type, size, create, this, mode));
}

void WorkerFileSystemCallbacksBridge::postMoveToMainThread(WebFileSystem* fileSystem, const String& sourcePath, const String& destinationPath, const String& mode)
{
    dispatchTaskToMainThread(createCallbackTask(&moveOnMainThread, fileSystem, sourcePath, destinationPath, this, mode));
}

void WorkerFileSystemCallbacksBridge::postCopyToMainThread(WebFileSystem* fileSystem, const String& sourcePath, const String& destinationPath, const String& mode)
{
    dispatchTaskToMainThread(createCallbackTask(&copyOnMainThread, fileSystem, sourcePath, destinationPath, this, mode));
}

void WorkerFileSystemCallbacksBridge::postRemoveToMainThread(WebFileSystem* fileSystem, const String& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(createCallbackTask(&removeOnMainThread, fileSystem, path, this, mode));
}

void WorkerFileSystemCallbacksBridge::postRemoveRecursivelyToMainThread(WebFileSystem* fileSystem, const String& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(createCallbackTask(&removeRecursivelyOnMainThread, fileSystem, path, this, mode));
}

void WorkerFileSystemCallbacksBridge::postReadMetadataToMainThread(WebFileSystem* fileSystem, const String& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(createCallbackTask(&readMetadataOnMainThread, fileSystem, path, this, mode));
}

void WorkerFileSystemCallbacksBridge::postCreateFileToMainThread(WebFileSystem* fileSystem, const String& path, bool exclusive, const String& mode)
{
    dispatchTaskToMainThread(createCallbackTask(&createFileOnMainThread, fileSystem, path, exclusive, this, mode));
}

void WorkerFileSystemCallbacksBridge::postCreateDirectoryToMainThread(WebFileSystem* fileSystem, const String& path, bool exclusive, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(createCallbackTask(&createDirectoryOnMainThread, fileSystem, path, exclusive, this, mode));
}

void WorkerFileSystemCallbacksBridge::postFileExistsToMainThread(WebFileSystem* fileSystem, const String& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(createCallbackTask(&fileExistsOnMainThread, fileSystem, path, this, mode));
}

void WorkerFileSystemCallbacksBridge::postDirectoryExistsToMainThread(WebFileSystem* fileSystem, const String& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(createCallbackTask(&directoryExistsOnMainThread, fileSystem, path, this, mode));
}

void WorkerFileSystemCallbacksBridge::postReadDirectoryToMainThread(WebFileSystem* fileSystem, const String& path, const String& mode)
{
    ASSERT(fileSystem);
    dispatchTaskToMainThread(createCallbackTask(&readDirectoryOnMainThread, fileSystem, path, this, mode));
}

void WorkerFileSystemCallbacksBridge::openFileSystemOnMainThread(ScriptExecutionContext*, WebCommonWorkerClient* commonClient, WebFileSystem::Type type, long long size, bool create, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    if (!commonClient)
        bridge->didFailOnMainThread(WebFileErrorAbort, mode);
    else {
        commonClient->openFileSystem(type, size, create, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
    }
}

void WorkerFileSystemCallbacksBridge::moveOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& sourcePath, const String& destinationPath, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->move(sourcePath, destinationPath, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::copyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& sourcePath, const String& destinationPath, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->copy(sourcePath, destinationPath, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::removeOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->remove(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::removeRecursivelyOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->removeRecursively(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::readMetadataOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->readMetadata(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::createFileOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, bool exclusive, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->createFile(path, exclusive, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::createDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, bool exclusive, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->createDirectory(path, exclusive, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::fileExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->fileExists(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::directoryExistsOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->directoryExists(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::readDirectoryOnMainThread(WebCore::ScriptExecutionContext*, WebFileSystem* fileSystem, const String& path, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    fileSystem->readDirectory(path, MainThreadFileSystemCallbacks::createLeakedPtr(bridge, mode));
}

void WorkerFileSystemCallbacksBridge::didFailOnMainThread(WebFileError error, const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didFailOnWorkerThread, this, error), mode);
}

void WorkerFileSystemCallbacksBridge::didOpenFileSystemOnMainThread(const String& name, const String& rootPath, const String& mode)
{
    mayPostTaskToWorker(createCallbackTask(&didOpenFileSystemOnWorkerThread, this, name, rootPath), mode);
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
    mayPostTaskToWorker(createCallbackTask(&didReadDirectoryOnWorkerThread, this, entries, hasMore), mode);
}

WorkerFileSystemCallbacksBridge::WorkerFileSystemCallbacksBridge(WebWorkerBase* worker, ScriptExecutionContext* scriptExecutionContext, WebFileSystemCallbacks* callbacks)
    : WorkerContext::Observer(static_cast<WorkerContext*>(scriptExecutionContext))
    , m_worker(worker)
    , m_workerContext(scriptExecutionContext)
    , m_callbacksOnWorkerThread(callbacks)
{
    ASSERT(m_workerContext->isContextThread());
}

WorkerFileSystemCallbacksBridge::~WorkerFileSystemCallbacksBridge()
{
    ASSERT(!m_callbacksOnWorkerThread);
}

void WorkerFileSystemCallbacksBridge::didFailOnWorkerThread(ScriptExecutionContext*, WorkerFileSystemCallbacksBridge* bridge, WebFileError error)
{
    bridge->m_callbacksOnWorkerThread->didFail(error);
}

void WorkerFileSystemCallbacksBridge::didOpenFileSystemOnWorkerThread(ScriptExecutionContext*, WorkerFileSystemCallbacksBridge* bridge, const String& name, const String& rootPath)
{
    bridge->m_callbacksOnWorkerThread->didOpenFileSystem(name, rootPath);
}

void WorkerFileSystemCallbacksBridge::didSucceedOnWorkerThread(ScriptExecutionContext*, WorkerFileSystemCallbacksBridge* bridge)
{
    bridge->m_callbacksOnWorkerThread->didSucceed();
}

void WorkerFileSystemCallbacksBridge::didReadMetadataOnWorkerThread(ScriptExecutionContext*, WorkerFileSystemCallbacksBridge* bridge, const WebFileInfo& info)
{
    bridge->m_callbacksOnWorkerThread->didReadMetadata(info);
}

void WorkerFileSystemCallbacksBridge::didReadDirectoryOnWorkerThread(ScriptExecutionContext*, WorkerFileSystemCallbacksBridge* bridge, const WebVector<WebFileSystemEntry>& entries, bool hasMore)
{
    bridge->m_callbacksOnWorkerThread->didReadDirectory(entries, hasMore);
}


void WorkerFileSystemCallbacksBridge::runTaskOnMainThread(WebCore::ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, PassOwnPtr<WebCore::ScriptExecutionContext::Task> taskToRun)
{
    ASSERT(isMainThread());

    // Every task run will result in one call to mayPostTaskToWorker, which is where this ref is released.
    WorkerFileSystemCallbacksBridge* leaked = bridge.leakRef();
    UNUSED_PARAM(leaked);
    taskToRun->performTask(scriptExecutionContext);
}

void WorkerFileSystemCallbacksBridge::runTaskOnWorkerThread(WebCore::ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, PassOwnPtr<WebCore::ScriptExecutionContext::Task> taskToRun)
{
    if (!bridge->m_callbacksOnWorkerThread)
        return;
    ASSERT(bridge->m_workerContext->isContextThread());
    taskToRun->performTask(scriptExecutionContext);
    bridge->m_callbacksOnWorkerThread = 0;
    bridge->stopObserving();
}

void WorkerFileSystemCallbacksBridge::dispatchTaskToMainThread(PassOwnPtr<WebCore::ScriptExecutionContext::Task> task)
{
    ASSERT(m_worker);
    ASSERT(m_workerContext->isContextThread());
    m_worker->dispatchTaskToMainThread(createCallbackTask(&runTaskOnMainThread, RefPtr<WorkerFileSystemCallbacksBridge>(this).release(), task));
}

void WorkerFileSystemCallbacksBridge::mayPostTaskToWorker(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    ASSERT(isMainThread());

    // Balancing out the ref() done in runTaskOnMainThread. (Since m_mutex is a member and the deref may result
    // in the destruction of WorkerFileSystemCallbacksBridge, the ordering of the RefPtr and the MutexLocker
    // is very important, to ensure that the m_mutex is still valid when it gets unlocked.)
    RefPtr<WorkerFileSystemCallbacksBridge> bridge = adoptRef(this);
    MutexLocker locker(m_mutex);
    if (m_worker)
        m_worker->postTaskForModeToWorkerContext(createCallbackTask(&runTaskOnWorkerThread, bridge, task), mode);
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
