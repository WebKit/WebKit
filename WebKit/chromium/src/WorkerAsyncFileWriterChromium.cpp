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
#include "WorkerAsyncFileWriterChromium.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystem.h"
#include "Blob.h"
#include "ScriptExecutionContext.h"
#include "WebFileSystem.h"
#include "WebFileWriter.h"
#include "WebURL.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
#include "WorkerFileWriterCallbacksBridge.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <wtf/Assertions.h>

using namespace WebKit;

namespace WebCore {

WorkerAsyncFileWriterChromium::WorkerAsyncFileWriterChromium(WebFileSystem* webFileSystem, const String& path, WorkerContext* workerContext, AsyncFileWriterClient* client, WriterType type)
    : m_type(type)
{
    ASSERT(m_type == Asynchronous); // Synchronous is not implemented yet.

    WorkerLoaderProxy* proxy = &workerContext->thread()->workerLoaderProxy();
    m_bridge = WorkerFileWriterCallbacksBridge::create(path, proxy, workerContext, client);
}

WorkerAsyncFileWriterChromium::~WorkerAsyncFileWriterChromium()
{
    m_bridge->postShutdownToMainThread(m_bridge);
}

bool WorkerAsyncFileWriterChromium::waitForOperationToComplete()
{
    return m_bridge->waitForOperationToComplete();
}

void WorkerAsyncFileWriterChromium::write(long long position, Blob* data)
{
    m_bridge->postWriteToMainThread(position, data->url());
}

void WorkerAsyncFileWriterChromium::truncate(long long length)
{
    m_bridge->postTruncateToMainThread(length);
}

void WorkerAsyncFileWriterChromium::abort()
{
    m_bridge->postAbortToMainThread();
}

}

#endif // ENABLE(FILE_SYSTEM)
