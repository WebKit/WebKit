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
#include "WebFileSystemCallbacks.h"
#include "WebString.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
#include "WorkerScriptController.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

using namespace WebCore;

namespace WebKit {

// FileSystemCallbacks that are to be dispatched on the main thread.
class MainThreadFileSystemCallbacks : public WebFileSystemCallbacks {
public:
    static PassOwnPtr<MainThreadFileSystemCallbacks> create(PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
    {
        return adoptPtr(new MainThreadFileSystemCallbacks(bridge, mode));
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
        WEBKIT_ASSERT_NOT_REACHED();
    }

    virtual void didReadMetadata(const WebFileInfo& info)
    {
        WEBKIT_ASSERT_NOT_REACHED();
    }

    virtual void didReadDirectory(const WebVector<WebFileSystemEntry>& entries, bool hasMore)
    {
        WEBKIT_ASSERT_NOT_REACHED();
    }

private:
    MainThreadFileSystemCallbacks(PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& mode)
        : m_bridge(bridge)
        , m_mode(mode)
    {
        ASSERT(m_bridge.get());
    }

    friend class WorkerFileSystemCallbacksBridge;
    RefPtr<WorkerFileSystemCallbacksBridge> m_bridge;
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

void WorkerFileSystemCallbacksBridge::postOpenFileSystemToMainThread(WebCommonWorkerClient* commonClient, WebFileSystem::Type type, long long size, const String& mode)
{
    m_selfRef = this;
    ASSERT(m_workerContext->isContextThread());
    ASSERT(m_worker);
    m_worker->dispatchTaskToMainThread(createCallbackTask(&openFileSystemOnMainThread, commonClient, type, size, this, mode));
}

void WorkerFileSystemCallbacksBridge::openFileSystemOnMainThread(ScriptExecutionContext*, WebCommonWorkerClient* commonClient, WebFileSystem::Type type, long long size, WorkerFileSystemCallbacksBridge* bridge, const String& mode)
{
    ASSERT(isMainThread());
    if (!commonClient)
        bridge->didFailOnMainThread(WebFileErrorAbort, mode);
    else {
        // MainThreadFileSystemCallbacks is self-destructed, so we leak ptr here.
        commonClient->openFileSystem(type, size, MainThreadFileSystemCallbacks::create(bridge, mode).leakPtr());
    }
}

void WorkerFileSystemCallbacksBridge::didFailOnMainThread(WebFileError error, const String& mode)
{
    ASSERT(isMainThread());
    mayPostTaskToWorker(createCallbackTask(&didFailOnWorkerThread, m_selfRef, error), mode);
}

void WorkerFileSystemCallbacksBridge::didOpenFileSystemOnMainThread(const String& name, const String& rootPath, const String& mode)
{
    ASSERT(isMainThread());
    mayPostTaskToWorker(createCallbackTask(&didOpenFileSystemOnWorkerThread, m_selfRef, name, rootPath), mode);
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

void WorkerFileSystemCallbacksBridge::didFailOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, WebFileError error)
{
    if (bridge->m_callbacksOnWorkerThread) {
        ASSERT(bridge->m_workerContext->isContextThread());
        bridge->m_callbacksOnWorkerThread->didFail(error);
        bridge->m_callbacksOnWorkerThread = 0;
    }
}

void WorkerFileSystemCallbacksBridge::didOpenFileSystemOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileSystemCallbacksBridge> bridge, const String& name, const String& rootPath)
{
    if (bridge->m_callbacksOnWorkerThread) {
        ASSERT(bridge->m_workerContext->isContextThread());
        bridge->m_callbacksOnWorkerThread->didOpenFileSystem(name, rootPath);
        bridge->m_callbacksOnWorkerThread = 0;
    }
}

void WorkerFileSystemCallbacksBridge::mayPostTaskToWorker(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    { // Let go of the mutex before possibly deleting this due to m_selfRef.clear().
        MutexLocker locker(m_mutex);
        if (m_worker)
            m_worker->postTaskForModeToWorkerContext(task, mode);
    }
    m_selfRef.clear();
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
