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
#include "FileSystemFileHandle.h"

#include "FileSystemStorageConnection.h"
#include "FileSystemSyncAccessHandle.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFileSystemSyncAccessHandle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FileSystemFileHandle);

Ref<FileSystemFileHandle> FileSystemFileHandle::create(ScriptExecutionContext& context, String&& name, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
{
    return adoptRef(*new FileSystemFileHandle(context, WTFMove(name), identifier, WTFMove(connection)));
}

FileSystemFileHandle::FileSystemFileHandle(ScriptExecutionContext& context, String&& name, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
    : FileSystemHandle(context, FileSystemHandle::Kind::File, WTFMove(name), identifier, WTFMove(connection))
{
}

void FileSystemFileHandle::getFile(DOMPromiseDeferred<IDLInterface<File>>&& promise)
{
    promise.reject(Exception { NotSupportedError, "getFile is not implemented"_s });
}

void FileSystemFileHandle::createSyncAccessHandle(DOMPromiseDeferred<IDLInterface<FileSystemSyncAccessHandle>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { InvalidStateError, "Handle is closed" });

    connection().createSyncAccessHandle(identifier(), [protectedThis = Ref { *this }, promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        auto [identifier, file] = result.releaseReturnValue();
        if (file == FileSystem::invalidPlatformFileHandle)
            return promise.reject(Exception { UnknownError, "Invalid platform file handle"_s });

        auto* context = protectedThis->scriptExecutionContext();
        if (!context) {
            FileSystem::closeFile(file);
            protectedThis->close(identifier, { });
            return promise.reject(Exception { InvalidStateError, "Context has stopped"_s });
        }

        promise.settle(FileSystemSyncAccessHandle::create(*context, protectedThis.get(), identifier, file));
    });
}

void FileSystemFileHandle::getSize(FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void(ExceptionOr<uint64_t>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { InvalidStateError, "Handle is closed"_s });

    connection().getSize(identifier(), accessHandleIdentifier, WTFMove(completionHandler));
}

void FileSystemFileHandle::truncate(FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, unsigned long long size, CompletionHandler<void(ExceptionOr<void>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { InvalidStateError, "Handle is closed"_s });

    connection().truncate(identifier(), accessHandleIdentifier, size, WTFMove(completionHandler));
}

void FileSystemFileHandle::flush(FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void(ExceptionOr<void>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { InvalidStateError, "Handle is closed"_s });

    connection().flush(identifier(), accessHandleIdentifier, WTFMove(completionHandler));
}

void FileSystemFileHandle::close(FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void(ExceptionOr<void>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { InvalidStateError, "Handle is closed"_s });

    connection().close(identifier(), accessHandleIdentifier, WTFMove(completionHandler));
}

} // namespace WebCore

