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

#include "config.h"
#include "FileSystemSyncAccessHandle.h"

#include "BufferSource.h"
#include "FileSystemFileHandle.h"
#include "JSDOMPromiseDeferred.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

Ref<FileSystemSyncAccessHandle> FileSystemSyncAccessHandle::create(ScriptExecutionContext& context, FileSystemFileHandle& source, FileSystemSyncAccessHandleIdentifier identifier, FileHandle&& file)
{
    auto handle = adoptRef(*new FileSystemSyncAccessHandle(context, source, identifier, WTFMove(file)));
    handle->suspendIfNeeded();
    return handle;
}

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(ScriptExecutionContext& context, FileSystemFileHandle& source, FileSystemSyncAccessHandleIdentifier identifier, FileHandle&& file)
    : ActiveDOMObject(&context)
    , m_source(source)
    , m_identifier(identifier)
    , m_file(WTFMove(file))
{
    ASSERT(m_file);

    m_source->registerSyncAccessHandle(m_identifier, *this);
}

FileSystemSyncAccessHandle::~FileSystemSyncAccessHandle()
{
    ASSERT(isClosingOrClosed());

    m_source->unregisterSyncAccessHandle(m_identifier);

    if (m_closeResult)
        return;

    // Task replies may be scheduled to WorkerRunLoop but do not run.
    auto pendingPromises = std::exchange(m_pendingPromises, { });
    for (auto& promise : pendingPromises) {
        std::visit([](auto& promise) {
            promise.reject(Exception { UnknownError, "AccessHandle is about to be destroyed"_s });
        }, promise);
    }

    closeBackend(CloseMode::Sync);
}

bool FileSystemSyncAccessHandle::isClosingOrClosed() const
{
    return m_closeResult || !m_closeCallbacks.isEmpty();
}

void FileSystemSyncAccessHandle::truncate(unsigned long long size, DOMPromiseDeferred<void>&& promise)
{
    if (isClosingOrClosed())
        return promise.reject(Exception { InvalidStateError, "AccessHandle is closing or closed"_s });

    auto* scope = downcast<WorkerGlobalScope>(scriptExecutionContext());
    if (!scope)
        return promise.reject(Exception { InvalidStateError, "Context is invalid"_s });

    m_pendingPromises.append(WTFMove(promise));
    WorkerGlobalScope::postFileSystemStorageTask([weakThis = WeakPtr { *this }, file = m_file.handle(), size, workerThread = Ref { scope->thread() }]() mutable {
        workerThread->runLoop().postTask([weakThis = WTFMove(weakThis), success = FileSystem::truncateFile(file, size)](auto&) mutable {
            if (weakThis)
                weakThis->completePromise(success ? ExceptionOr<void> { } : Exception { UnknownError });
        });
    });
}

void FileSystemSyncAccessHandle::getSize(DOMPromiseDeferred<IDLUnsignedLongLong>&& promise)
{
    if (isClosingOrClosed())
        return promise.reject(Exception { InvalidStateError, "AccessHandle is closing or closed"_s });

    auto* scope = downcast<WorkerGlobalScope>(scriptExecutionContext());
    if (!scope)
        return promise.reject(Exception { InvalidStateError, "Context is invalid"_s });

    m_pendingPromises.append(WTFMove(promise));
    WorkerGlobalScope::postFileSystemStorageTask([weakThis = WeakPtr { *this }, file = m_file.handle(), workerThread = Ref { scope->thread() }]() mutable {
        workerThread->runLoop().postTask([weakThis = WTFMove(weakThis), success = FileSystem::fileSize(file)](auto&) mutable {
            if (weakThis)
                weakThis->completePromise(success ? ExceptionOr<uint64_t> { success.value() } : Exception { UnknownError });
        });
    });
}

void FileSystemSyncAccessHandle::flush(DOMPromiseDeferred<void>&& promise)
{
    if (isClosingOrClosed())
        return promise.reject(Exception { InvalidStateError, "AccessHandle is closing or closed"_s });

    auto* scope = downcast<WorkerGlobalScope>(scriptExecutionContext());
    if (!scope)
        return promise.reject(Exception { InvalidStateError, "Context is invalid"_s });

    m_pendingPromises.append(WTFMove(promise));
    WorkerGlobalScope::postFileSystemStorageTask([weakThis = WeakPtr { *this }, file = m_file.handle(), workerThread = Ref { scope->thread() }]() mutable {
        workerThread->runLoop().postTask([weakThis = WTFMove(weakThis), success = FileSystem::flushFile(file)](auto&) mutable {
            if (weakThis)
                weakThis->completePromise(success ? ExceptionOr<void> { } : Exception { UnknownError });
        });
    });
}

void FileSystemSyncAccessHandle::close(DOMPromiseDeferred<void>&& promise)
{
    closeInternal([weakThis = WeakPtr { *this }, promise = WTFMove(promise)](auto result) mutable {
        promise.settle(WTFMove(result));
    });
}

void FileSystemSyncAccessHandle::closeInternal(CloseCallback&& callback)
{
    if (m_closeResult)
        return callback(ExceptionOr<void> { *m_closeResult });

    auto isClosing = !m_closeCallbacks.isEmpty();
    m_closeCallbacks.append(WTFMove(callback));
    if (isClosing)
        return;

    ASSERT(m_file);
    closeFile();
}

void FileSystemSyncAccessHandle::closeFile()
{
    if (!m_file)
        return;

    auto* scope = downcast<WorkerGlobalScope>(scriptExecutionContext());
    ASSERT(scope);

    WorkerGlobalScope::postFileSystemStorageTask([weakThis = WeakPtr { *this }, file = std::exchange(m_file, { }), workerThread = Ref { scope->thread() }]() mutable {
        workerThread->runLoop().postTask([weakThis = WTFMove(weakThis)](auto&) mutable {
            if (weakThis)
                weakThis->didCloseFile();
        });
    });
}

void FileSystemSyncAccessHandle::didCloseFile()
{
    closeBackend(CloseMode::Async);
}

void FileSystemSyncAccessHandle::closeBackend(CloseMode mode)
{
    if (m_closeResult)
        return;

    if (mode == CloseMode::Async) {
        m_source->closeSyncAccessHandle(m_identifier, [this, protectedThis = Ref { *this }](auto result) mutable {
            didCloseBackend(WTFMove(result));
        });
        return;
    }

    m_source->closeSyncAccessHandle(m_identifier, [](auto) { });
    didCloseBackend({ });
}

void FileSystemSyncAccessHandle::didCloseBackend(ExceptionOr<void>&& result)
{
    if (m_closeResult)
        return;

    m_closeResult = WTFMove(result);
    auto callbacks = std::exchange(m_closeCallbacks, { });
    for (auto& callback : callbacks)
        callback(ExceptionOr<void> { *m_closeResult });
}

ExceptionOr<unsigned long long> FileSystemSyncAccessHandle::read(BufferSource&& buffer, FileSystemSyncAccessHandle::FilesystemReadWriteOptions options)
{
    ASSERT(!isMainThread());

    if (isClosingOrClosed())
        return Exception { InvalidStateError, "AccessHandle is closing or closed"_s };

    if (!m_pendingPromises.isEmpty())
        return Exception { InvalidStateError, "Access handle has unfinished operation"_s };

    if (options.at) {
        int result = FileSystem::seekFile(m_file.handle(), options.at.value(), FileSystem::FileSeekOrigin::Beginning);
        if (result == -1)
            return Exception { InvalidStateError, "Failed to read at offset"_s };
    }

    int result = FileSystem::readFromFile(m_file.handle(), buffer.mutableData(), buffer.length());
    if (result == -1)
        return Exception { InvalidStateError, "Failed to read from file"_s };

    return result;
}

ExceptionOr<unsigned long long> FileSystemSyncAccessHandle::write(BufferSource&& buffer, FileSystemSyncAccessHandle::FilesystemReadWriteOptions options)
{
    ASSERT(!isMainThread());

    if (isClosingOrClosed())
        return Exception { InvalidStateError, "AccessHandle is closing or closed"_s };

    if (!m_pendingPromises.isEmpty())
        return Exception { InvalidStateError, "Access handle has unfinished operation"_s };

    if (options.at) {
        int result = FileSystem::seekFile(m_file.handle(), options.at.value(), FileSystem::FileSeekOrigin::Beginning);
        if (result == -1)
            return Exception { InvalidStateError, "Failed to write at offset"_s };
    }

    int result = FileSystem::writeToFile(m_file.handle(), buffer.data(), buffer.length());
    if (result == -1)
        return Exception { InvalidStateError, "Failed to write to file"_s };

    return result;
}

void FileSystemSyncAccessHandle::completePromise(Result&& result)
{
    if (m_pendingPromises.isEmpty())
        return;

    auto pendingPromise = m_pendingPromises.takeFirst();
    WTF::switchOn(WTFMove(result), [&pendingPromise](ExceptionOr<void>&& result) {
        auto* promise = std::get_if<DOMPromiseDeferred<void>>(&pendingPromise);
        ASSERT(promise);
        promise->settle(WTFMove(result));
    }, [&pendingPromise](ExceptionOr<uint64_t>&& result) {
        auto* promise = std::get_if<DOMPromiseDeferred<IDLUnsignedLongLong>>(&pendingPromise);
        ASSERT(promise);
        promise->settle(WTFMove(result));
    });
}

const char* FileSystemSyncAccessHandle::activeDOMObjectName() const
{
    return "FileSystemSyncAccessHandle";
}

void FileSystemSyncAccessHandle::stop()
{
    closeInternal([](auto) { });
}

void FileSystemSyncAccessHandle::invalidate()
{
    closeFile();

    // Invalidation is initiated by backend.
    didCloseBackend({ });
}

} // namespace WebCore
