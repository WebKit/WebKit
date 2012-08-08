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
#include "WorkerFileWriterCallbacksBridge.h"

#if ENABLE(FILE_SYSTEM) && ENABLE(WORKERS)

#include "AsyncFileWriterClient.h"
#include "CrossThreadTask.h"
#include "WebFileWriter.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <public/Platform.h>
#include <public/WebCString.h>
#include <public/WebFileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

using namespace WebCore;

namespace WebKit {

void WorkerFileWriterCallbacksBridge::notifyStop()
{
    ASSERT(m_workerContext->isContextThread());
    m_clientOnWorkerThread = 0;
}

void WorkerFileWriterCallbacksBridge::postWriteToMainThread(long long position, const KURL& data)
{
    ASSERT(!m_operationInProgress);
    m_operationInProgress = true;
    dispatchTaskToMainThread(createCallbackTask(&writeOnMainThread, 
                                                this, position, data));
}

void WorkerFileWriterCallbacksBridge::postTruncateToMainThread(long long length)
{
    ASSERT(!m_operationInProgress);
    m_operationInProgress = true;
    dispatchTaskToMainThread(createCallbackTask(&truncateOnMainThread, 
                                                this, length));
}

void WorkerFileWriterCallbacksBridge::postAbortToMainThread()
{
    ASSERT(m_operationInProgress);
    dispatchTaskToMainThread(createCallbackTask(&abortOnMainThread, this));
}

void WorkerFileWriterCallbacksBridge::postShutdownToMainThread(PassRefPtr<WorkerFileWriterCallbacksBridge> bridge)
{
    ASSERT(m_workerContext->isContextThread());
    m_clientOnWorkerThread = 0;
    dispatchTaskToMainThread(createCallbackTask(&shutdownOnMainThread, bridge));
}

void WorkerFileWriterCallbacksBridge::writeOnMainThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge, long long position, const KURL& data)
{
    bridge->m_writer->write(position, WebURL(data));
}

void WorkerFileWriterCallbacksBridge::truncateOnMainThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge, long long length)
{
    bridge->m_writer->truncate(length);
}

void WorkerFileWriterCallbacksBridge::abortOnMainThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge)
{
    bridge->m_writer->cancel();
}

void WorkerFileWriterCallbacksBridge::initOnMainThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge, const KURL& path)
{
    ASSERT(!bridge->m_writer);
    bridge->m_writer = adoptPtr(WebKit::Platform::current()->fileSystem()->createFileWriter(path, bridge.get()));
}

void WorkerFileWriterCallbacksBridge::shutdownOnMainThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge)
{
    bridge->m_writerDeleted = true;
    bridge->m_writer.clear();
}

void WorkerFileWriterCallbacksBridge::didWrite(long long bytes, bool complete)
{
    dispatchTaskToWorkerThread(createCallbackTask(&didWriteOnWorkerThread, this, bytes, complete));
}

void WorkerFileWriterCallbacksBridge::didFail(WebFileError error)
{
    dispatchTaskToWorkerThread(createCallbackTask(&didFailOnWorkerThread, this, error));
}

void WorkerFileWriterCallbacksBridge::didTruncate()
{
    dispatchTaskToWorkerThread(createCallbackTask(&didTruncateOnWorkerThread, this));
}

static const char fileWriterOperationsMode[] = "fileWriterOperationsMode";

WorkerFileWriterCallbacksBridge::WorkerFileWriterCallbacksBridge(const KURL& path, WorkerLoaderProxy* proxy, ScriptExecutionContext* scriptExecutionContext, AsyncFileWriterClient* client)
    : WorkerContext::Observer(static_cast<WorkerContext*>(scriptExecutionContext))
    , m_proxy(proxy)
    , m_workerContext(scriptExecutionContext)
    , m_clientOnWorkerThread(client)
    , m_writerDeleted(false)
    , m_operationInProgress(false)
{
    ASSERT(m_workerContext->isContextThread());
    m_mode = fileWriterOperationsMode;
    m_mode.append(String::number(static_cast<WorkerContext*>(scriptExecutionContext)->thread()->runLoop().createUniqueId()));
    postInitToMainThread(path);
}

void WorkerFileWriterCallbacksBridge::postInitToMainThread(const KURL& path)
{
    dispatchTaskToMainThread(
        createCallbackTask(&initOnMainThread, this, path));
}

WorkerFileWriterCallbacksBridge::~WorkerFileWriterCallbacksBridge()
{
    ASSERT(!m_clientOnWorkerThread);
    ASSERT(!m_writer);
}

// We know m_clientOnWorkerThread is still valid because it is only cleared on the context thread, and because we check in runTaskOnWorkerThread before calling any of these methods.
void WorkerFileWriterCallbacksBridge::didWriteOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge, long long length, bool complete)
{
    ASSERT(bridge->m_workerContext->isContextThread());
    ASSERT(bridge->m_operationInProgress);
    if (complete)
        bridge->m_operationInProgress = false;
    bridge->m_clientOnWorkerThread->didWrite(length, complete);
}

void WorkerFileWriterCallbacksBridge::didFailOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge, WebFileError error)
{
    ASSERT(bridge->m_workerContext->isContextThread());
    ASSERT(bridge->m_operationInProgress);
    bridge->m_operationInProgress = false;
    bridge->m_clientOnWorkerThread->didFail(static_cast<FileError::ErrorCode>(error));
}

void WorkerFileWriterCallbacksBridge::didTruncateOnWorkerThread(ScriptExecutionContext*, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge)
{
    ASSERT(bridge->m_workerContext->isContextThread());
    ASSERT(bridge->m_operationInProgress);
    bridge->m_operationInProgress = false;
    bridge->m_clientOnWorkerThread->didTruncate();
}

void WorkerFileWriterCallbacksBridge::runTaskOnMainThread(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge, PassOwnPtr<ScriptExecutionContext::Task> taskToRun)
{
    ASSERT(isMainThread());
    if (!bridge->m_writerDeleted)
        taskToRun->performTask(scriptExecutionContext);
}

void WorkerFileWriterCallbacksBridge::runTaskOnWorkerThread(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<WorkerFileWriterCallbacksBridge> bridge, PassOwnPtr<ScriptExecutionContext::Task> taskToRun)
{
    ASSERT(bridge->m_workerContext->isContextThread());
    if (bridge->m_clientOnWorkerThread)
        taskToRun->performTask(scriptExecutionContext);
}

void WorkerFileWriterCallbacksBridge::dispatchTaskToMainThread(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    ASSERT(m_workerContext->isContextThread());
    WebWorkerBase::dispatchTaskToMainThread(
        createCallbackTask(&runTaskOnMainThread, this, task));
}

void WorkerFileWriterCallbacksBridge::dispatchTaskToWorkerThread(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    ASSERT(isMainThread());
    m_proxy->postTaskForModeToWorkerContext(
        createCallbackTask(&runTaskOnWorkerThread, this, task), m_mode);
}

bool WorkerFileWriterCallbacksBridge::waitForOperationToComplete()
{
    while (m_operationInProgress) {
        WorkerContext* context = static_cast<WorkerContext*>(m_workerContext);
        if (context->thread()->runLoop().runInMode(context, m_mode) == MessageQueueTerminated)
            return false;
    }
    return true;
}

} // namespace WebKit

#endif // ENABLE(FILE_SYSTEM)
