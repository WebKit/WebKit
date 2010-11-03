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
#include "LocalFileSystem.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystem.h"
#include "ErrorCallback.h"
#include "FileSystemCallback.h"
#include "FileSystemCallbacks.h"
#include "PlatformString.h"
#include "WebFileSystem.h"
#include "WebFileSystemCallbacksImpl.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebWorkerImpl.h"
#include "WorkerContext.h"
#include "WorkerThread.h"
#include <wtf/Threading.h>

using namespace WebKit;

namespace WebCore {

LocalFileSystem& LocalFileSystem::localFileSystem()
{
    AtomicallyInitializedStatic(LocalFileSystem*, localFileSystem = new LocalFileSystem(""));
    return *localFileSystem;
}

void LocalFileSystem::readFileSystem(ScriptExecutionContext* context, AsyncFileSystem::Type type, long long size, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    ASSERT(context && context->isDocument());
    Document* document = static_cast<Document*>(context);
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
    webFrame->client()->openFileSystem(webFrame, static_cast<WebFileSystem::Type>(type), size, false, new WebFileSystemCallbacksImpl(callbacks));
}

void LocalFileSystem::requestFileSystem(ScriptExecutionContext* context, AsyncFileSystem::Type type, long long size, PassOwnPtr<AsyncFileSystemCallbacks> callbacks, bool synchronous)
{
    ASSERT(context);
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        webFrame->client()->openFileSystem(webFrame, static_cast<WebFileSystem::Type>(type), size, true, new WebFileSystemCallbacksImpl(callbacks));
    } else {
        WorkerContext* workerContext = static_cast<WorkerContext*>(context);
        WorkerLoaderProxy* workerLoaderProxy = &workerContext->thread()->workerLoaderProxy();
        WebWorkerBase* webWorker = static_cast<WebWorkerBase*>(workerLoaderProxy);
        webWorker->openFileSystem(static_cast<WebFileSystem::Type>(type), size, new WebFileSystemCallbacksImpl(callbacks, context, synchronous), synchronous);
    }
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
