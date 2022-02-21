/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ActiveDOMObject.h"
#include "BufferSource.h"
#include "ExceptionOr.h"
#include "FileHandle.h"
#include "FileSystemSyncAccessHandleIdentifier.h"
#include "IDLTypes.h"
#include <wtf/Deque.h>
#include <wtf/FileSystem.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class FileSystemFileHandle;
template<typename> class DOMPromiseDeferred;

class FileSystemSyncAccessHandle : public ActiveDOMObject, public RefCounted<FileSystemSyncAccessHandle>, public CanMakeWeakPtr<FileSystemSyncAccessHandle> {
public:
    struct FilesystemReadWriteOptions {
        unsigned long long at;
    };

    static Ref<FileSystemSyncAccessHandle> create(ScriptExecutionContext&, FileSystemFileHandle&, FileSystemSyncAccessHandleIdentifier, FileHandle&&);
    ~FileSystemSyncAccessHandle();

    void truncate(unsigned long long size, DOMPromiseDeferred<void>&&);
    void getSize(DOMPromiseDeferred<IDLUnsignedLongLong>&&);
    void flush(DOMPromiseDeferred<void>&&);
    void close(DOMPromiseDeferred<void>&&);
    ExceptionOr<unsigned long long> read(BufferSource&&, FilesystemReadWriteOptions);
    ExceptionOr<unsigned long long> write(BufferSource&&, FilesystemReadWriteOptions);
    using Result = std::variant<ExceptionOr<void>, ExceptionOr<uint64_t>>;
    void completePromise(Result&&);
    void invalidate();

private:
    FileSystemSyncAccessHandle(ScriptExecutionContext&, FileSystemFileHandle&, FileSystemSyncAccessHandleIdentifier, FileHandle&&);
    bool isClosingOrClosed() const;
    using CloseCallback = CompletionHandler<void(ExceptionOr<void>&&)>;
    void closeInternal(CloseCallback&&);
    void closeFile();
    void didCloseFile();
    enum class CloseMode : bool { Async, Sync };
    void closeBackend(CloseMode);
    void didCloseBackend(ExceptionOr<void>&&);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    void stop() final;

    Ref<FileSystemFileHandle> m_source;
    FileSystemSyncAccessHandleIdentifier m_identifier;
    FileHandle m_file;
    std::optional<ExceptionOr<void>> m_closeResult;
    Vector<CloseCallback> m_closeCallbacks;
    using Promise = std::variant<DOMPromiseDeferred<void>, DOMPromiseDeferred<IDLUnsignedLongLong>>;
    Deque<Promise> m_pendingPromises;
};

} // namespace WebCore
