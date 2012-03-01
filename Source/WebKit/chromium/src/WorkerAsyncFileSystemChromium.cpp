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

#if ENABLE(FILE_SYSTEM) && ENABLE(WORKERS)

#include "AsyncFileSystemCallbacks.h"
#include "BlobURL.h"
#include "FileMetadata.h"
#include "FileSystem.h"
#include "NotImplemented.h"
#include "platform/WebFileSystem.h"
#include "WebFileSystemCallbacksImpl.h"
#include "WebFileWriter.h"
#include "WebKit.h"
#include "platform/WebKitPlatformSupport.h"
#include "WebWorkerBase.h"
#include "WorkerAsyncFileWriterChromium.h"
#include "WorkerContext.h"
#include "WorkerFileSystemCallbacksBridge.h"
#include "WorkerScriptController.h"
#include "WorkerThread.h"
#include <wtf/text/CString.h>

using namespace WebKit;

namespace WebCore {

static const char fileSystemOperationsMode[] = "fileSystemOperationsMode";

WorkerAsyncFileSystemChromium::WorkerAsyncFileSystemChromium(ScriptExecutionContext* context, AsyncFileSystem::Type type, const WebKit::WebURL& rootURL, bool synchronous)
    : AsyncFileSystemChromium(type, rootURL)
    , m_scriptExecutionContext(context)
    , m_workerContext(static_cast<WorkerContext*>(context))
    , m_synchronous(synchronous)
{
    ASSERT(m_scriptExecutionContext->isWorkerContext());

    WorkerLoaderProxy* workerLoaderProxy = &m_workerContext->thread()->workerLoaderProxy();
    m_worker = static_cast<WebWorkerBase*>(workerLoaderProxy);
}

WorkerAsyncFileSystemChromium::~WorkerAsyncFileSystemChromium()
{
}

bool WorkerAsyncFileSystemChromium::waitForOperationToComplete()
{
    if (!m_bridgeForCurrentOperation)
        return false;

    RefPtr<WorkerFileSystemCallbacksBridge> bridge = m_bridgeForCurrentOperation.release();
    if (m_workerContext->thread()->runLoop().runInMode(m_workerContext, m_modeForCurrentOperation) == MessageQueueTerminated) {
        bridge->stop();
        return false;
    }
    return true;
}

void WorkerAsyncFileSystemChromium::move(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postMoveToMainThread(m_webFileSystem, virtualPathToFileSystemURL(sourcePath), virtualPathToFileSystemURL(destinationPath), m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::copy(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postCopyToMainThread(m_webFileSystem, virtualPathToFileSystemURL(sourcePath), virtualPathToFileSystemURL(destinationPath), m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::remove(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postRemoveToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::removeRecursively(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postRemoveRecursivelyToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::readMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postReadMetadataToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::createFile(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postCreateFileToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), exclusive, m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::createDirectory(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postCreateDirectoryToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), exclusive, m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::fileExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postFileExistsToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::directoryExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postDirectoryExistsToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::readDirectory(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    createWorkerFileSystemCallbacksBridge(callbacks)->postReadDirectoryToMainThread(m_webFileSystem, virtualPathToFileSystemURL(path), m_modeForCurrentOperation);
}

class WorkerFileWriterHelperCallbacks : public AsyncFileSystemCallbacks {
public:
    static PassOwnPtr<WorkerFileWriterHelperCallbacks> create(AsyncFileWriterClient* client, const WebURL& path, WebKit::WebFileSystem* webFileSystem, PassOwnPtr<WebCore::AsyncFileSystemCallbacks> callbacks, WorkerContext* workerContext)
    {
        return adoptPtr(new WorkerFileWriterHelperCallbacks(client, path, webFileSystem, callbacks, workerContext));
    }

    virtual void didSucceed()
    {
        ASSERT_NOT_REACHED();
    }

    virtual void didReadMetadata(const FileMetadata& metadata)
    {
        ASSERT(m_callbacks);
        if (metadata.type != FileMetadata::TypeFile || metadata.length < 0)
            m_callbacks->didFail(WebKit::WebFileErrorInvalidState);
        else {
            OwnPtr<WorkerAsyncFileWriterChromium> asyncFileWriterChromium = WorkerAsyncFileWriterChromium::create(m_webFileSystem, m_path, m_workerContext, m_client, WorkerAsyncFileWriterChromium::Asynchronous);
            m_callbacks->didCreateFileWriter(asyncFileWriterChromium.release(), metadata.length);
        }
    }

    virtual void didReadDirectoryEntry(const String& name, bool isDirectory)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void didReadDirectoryEntries(bool hasMore)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void didOpenFileSystem(const String&, PassOwnPtr<AsyncFileSystem>)
    {
        ASSERT_NOT_REACHED();
    }

    // Called when an AsyncFileWrter has been created successfully.
    virtual void didCreateFileWriter(PassOwnPtr<AsyncFileWriter>, long long)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void didFail(int code)
    {
        ASSERT(m_callbacks);
        m_callbacks->didFail(code);
    }

private:
    WorkerFileWriterHelperCallbacks(AsyncFileWriterClient* client, const WebURL& path, WebKit::WebFileSystem* webFileSystem, PassOwnPtr<WebCore::AsyncFileSystemCallbacks> callbacks, WorkerContext* workerContext)
        : m_client(client)
        , m_path(path)
        , m_webFileSystem(webFileSystem)
        , m_callbacks(callbacks)
        , m_workerContext(workerContext)
    {
    }

    AsyncFileWriterClient* m_client;
    WebURL m_path;
    WebKit::WebFileSystem* m_webFileSystem;
    OwnPtr<WebCore::AsyncFileSystemCallbacks> m_callbacks;
    WorkerContext* m_workerContext;
};

void WorkerAsyncFileSystemChromium::createWriter(AsyncFileWriterClient* client, const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    KURL pathAsURL = virtualPathToFileSystemURL(path);
    createWorkerFileSystemCallbacksBridge(WorkerFileWriterHelperCallbacks::create(client, pathAsURL, m_webFileSystem, callbacks, m_workerContext))->postReadMetadataToMainThread(m_webFileSystem, pathAsURL, m_modeForCurrentOperation);
}

void WorkerAsyncFileSystemChromium::createSnapshotFileAndReadMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    KURL pathAsURL = virtualPathToFileSystemURL(path);
    KURL internalBlobURL = BlobURL::createInternalURL();

    createWorkerFileSystemCallbacksBridge(createSnapshotFileCallback(internalBlobURL, callbacks))->postCreateSnapshotFileToMainThread(m_webFileSystem, internalBlobURL, pathAsURL, m_modeForCurrentOperation);
}

PassRefPtr<WorkerFileSystemCallbacksBridge> WorkerAsyncFileSystemChromium::createWorkerFileSystemCallbacksBridge(PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    ASSERT(!m_synchronous || !m_bridgeForCurrentOperation);

    m_modeForCurrentOperation = fileSystemOperationsMode;
    m_modeForCurrentOperation.append(String::number(m_workerContext->thread()->runLoop().createUniqueId()));

    m_bridgeForCurrentOperation = WorkerFileSystemCallbacksBridge::create(m_worker, m_scriptExecutionContext, new WebKit::WebFileSystemCallbacksImpl(callbacks));
    return m_bridgeForCurrentOperation;
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
