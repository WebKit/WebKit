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
    m_source->unregisterSyncAccessHandle(m_identifier);
    closeInternal(ShouldNotifyBackend::Yes);
}

ExceptionOr<void> FileSystemSyncAccessHandle::truncate(unsigned long long size)
{
    if (m_isClosed)
        return Exception { InvalidStateError, "AccessHandle is closed"_s };

    bool succeeded = FileSystem::truncateFile(m_file.handle(), size);
    return succeeded ? ExceptionOr<void> { } : Exception { InvalidStateError, "Failed to truncate file"_s };
}

ExceptionOr<unsigned long long> FileSystemSyncAccessHandle::getSize()
{
    if (m_isClosed)
        return Exception { InvalidStateError, "AccessHandle is closed"_s };

    auto result = FileSystem::fileSize(m_file.handle());
    return result ? ExceptionOr<unsigned long long> { result.value() } : Exception { InvalidStateError, "Failed to get file size"_s };
}

ExceptionOr<void> FileSystemSyncAccessHandle::flush()
{
    if (m_isClosed)
        return Exception { InvalidStateError, "AccessHandle is closed"_s };

    bool succeeded = FileSystem::flushFile(m_file.handle());
    return succeeded ? ExceptionOr<void> { } : Exception { InvalidStateError, "Failed to flush file"_s };
}

ExceptionOr<void> FileSystemSyncAccessHandle::close()
{
    if (m_isClosed)
        return Exception { InvalidStateError, "AccessHandle is closed"_s };

    closeInternal(ShouldNotifyBackend::Yes);
    return { };
}

void FileSystemSyncAccessHandle::closeInternal(ShouldNotifyBackend shouldNotifyBackend)
{
    if (m_isClosed)
        return;

    m_isClosed = true;
    ASSERT(m_file);
    m_file = { };

    if (shouldNotifyBackend == ShouldNotifyBackend::Yes)
        m_source->closeSyncAccessHandle(m_identifier);
}

ExceptionOr<unsigned long long> FileSystemSyncAccessHandle::read(BufferSource&& buffer, FileSystemSyncAccessHandle::FilesystemReadWriteOptions options)
{
    if (m_isClosed)
        return Exception { InvalidStateError, "AccessHandle is closed"_s };

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
    if (m_isClosed)
        return Exception { InvalidStateError, "AccessHandle is closed"_s };

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

const char* FileSystemSyncAccessHandle::activeDOMObjectName() const
{
    return "FileSystemSyncAccessHandle";
}

void FileSystemSyncAccessHandle::stop()
{
    closeInternal(ShouldNotifyBackend::Yes);
}

void FileSystemSyncAccessHandle::invalidate()
{
    // Invalidation is initiated by backend.
    closeInternal(ShouldNotifyBackend::No);
}

} // namespace WebCore
