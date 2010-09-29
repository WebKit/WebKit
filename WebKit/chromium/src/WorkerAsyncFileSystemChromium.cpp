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
#include "WorkerAsyncFileSystemChromium.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystemCallbacks.h"
#include "FileSystem.h"
#include "NotImplemented.h"
#include "WebFileSystem.h"
#include "WebFileSystemCallbacksImpl.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
#include "WorkerFileSystemCallbacksBridge.h"
#include "WorkerScriptController.h"
#include "WorkerThread.h"
#include <wtf/text/CString.h>

using namespace WebKit;

namespace WebCore {

static const char fileSystemOperationsMode[] = "fileSystemOperationsMode";

WorkerAsyncFileSystemChromium::WorkerAsyncFileSystemChromium(ScriptExecutionContext* context, const String& rootPath)
    : AsyncFileSystem(rootPath)
    , m_scriptExecutionContext(context)
    , m_webFileSystem(WebKit::webKitClient()->fileSystem())
    , m_mode(fileSystemOperationsMode)
{
    ASSERT(m_webFileSystem);
    ASSERT(m_scriptExecutionContext->isWorkerContext());

    WorkerContext* workerContext = static_cast<WorkerContext*>(context);
    WorkerLoaderProxy* workerLoaderProxy = &workerContext->thread()->workerLoaderProxy();
    m_worker = static_cast<WebWorkerBase*>(workerLoaderProxy);
}

WorkerAsyncFileSystemChromium::~WorkerAsyncFileSystemChromium()
{
}

void WorkerAsyncFileSystemChromium::move(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postMoveToMainThread(m_webFileSystem, sourcePath, destinationPath, m_mode);
}

void WorkerAsyncFileSystemChromium::copy(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postCopyToMainThread(m_webFileSystem, sourcePath, destinationPath, m_mode);
}

void WorkerAsyncFileSystemChromium::remove(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postRemoveToMainThread(m_webFileSystem, path, m_mode);
}

void WorkerAsyncFileSystemChromium::readMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postReadMetadataToMainThread(m_webFileSystem, path, m_mode);
}

void WorkerAsyncFileSystemChromium::createFile(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postCreateFileToMainThread(m_webFileSystem, path, exclusive, m_mode);
}

void WorkerAsyncFileSystemChromium::createDirectory(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postCreateDirectoryToMainThread(m_webFileSystem, path, exclusive, m_mode);
}

void WorkerAsyncFileSystemChromium::fileExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postFileExistsToMainThread(m_webFileSystem, path, m_mode);
}

void WorkerAsyncFileSystemChromium::directoryExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postDirectoryExistsToMainThread(m_webFileSystem, path, m_mode);
}

void WorkerAsyncFileSystemChromium::readDirectory(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postReadDirectoryToMainThread(m_webFileSystem, path, m_mode);
}

void WorkerAsyncFileSystemChromium::createWriter(AsyncFileWriterClient*, const String&, PassOwnPtr<AsyncFileSystemCallbacks>)
{
    notImplemented();
}

PassRefPtr<WorkerFileSystemCallbacksBridge> WorkerAsyncFileSystemChromium::createWorkerFileSystemCallbacksBridge(PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    return WorkerFileSystemCallbacksBridge::create(m_worker, m_scriptExecutionContext, new WebFileSystemCallbacksImpl(callbacks));
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
