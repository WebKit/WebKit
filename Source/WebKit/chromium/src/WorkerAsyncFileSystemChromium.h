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

#ifndef WorkerAsyncFileSystemChromium_h
#define WorkerAsyncFileSystemChromium_h

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystem.h"
#include "PlatformString.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {
class WebFileSystem;
class WebWorkerBase;
class WorkerFileSystemCallbacksBridge;
}

namespace WebCore {

class AsyncFileSystemCallbacks;
class ScriptExecutionContext;
class WorkerContext;

class WorkerAsyncFileSystemChromium : public AsyncFileSystem {
public:
    static PassOwnPtr<AsyncFileSystem> create(ScriptExecutionContext* context, AsyncFileSystem::Type type, const String& rootPath, bool synchronous)
    {
        return adoptPtr(new WorkerAsyncFileSystemChromium(context, type, rootPath, synchronous));
    }

    virtual ~WorkerAsyncFileSystemChromium();

    // Runs one pending operation (to wait for completion in the sync-mode).
    virtual bool waitForOperationToComplete();

    virtual void move(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void copy(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void remove(const String& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void removeRecursively(const String& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void readMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void createFile(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void createDirectory(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void fileExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void directoryExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void readDirectory(const String& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void createWriter(AsyncFileWriterClient* client, const String& path, PassOwnPtr<AsyncFileSystemCallbacks>);

private:
    WorkerAsyncFileSystemChromium(ScriptExecutionContext*, AsyncFileSystem::Type, const String& rootPath, bool synchronous);

    PassRefPtr<WebKit::WorkerFileSystemCallbacksBridge> createWorkerFileSystemCallbacksBridge(PassOwnPtr<AsyncFileSystemCallbacks>);

    ScriptExecutionContext* m_scriptExecutionContext;
    WebKit::WebFileSystem* m_webFileSystem;
    WebKit::WebWorkerBase* m_worker;
    WorkerContext* m_workerContext;
    RefPtr<WebKit::WorkerFileSystemCallbacksBridge> m_bridgeForCurrentOperation;
    String m_modeForCurrentOperation;
    bool m_synchronous;
};

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)

#endif // WorkerAsyncFileSystemChromium_h
