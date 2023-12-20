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

#include "ContextDestructionObserverInlines.h"
#include "File.h"
#include "FileSystemHandleCloseScope.h"
#include "FileSystemStorageConnection.h"
#include "FileSystemSyncAccessHandle.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFile.h"
#include "JSFileSystemSyncAccessHandle.h"
#include "WorkerFileSystemStorageConnection.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FileSystemFileHandle);

Ref<FileSystemFileHandle> FileSystemFileHandle::create(ScriptExecutionContext& context, String&& name, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
{
    auto result = adoptRef(*new FileSystemFileHandle(context, WTFMove(name), identifier, WTFMove(connection)));
    result->suspendIfNeeded();
    return result;
}

FileSystemFileHandle::FileSystemFileHandle(ScriptExecutionContext& context, String&& name, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
    : FileSystemHandle(context, FileSystemHandle::Kind::File, WTFMove(name), identifier, WTFMove(connection))
{
}

void FileSystemFileHandle::getFile(DOMPromiseDeferred<IDLInterface<File>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().getFile(identifier(), [protectedThis = Ref { *this }, promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        RefPtr context = protectedThis->scriptExecutionContext();
        if (!context)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });

        promise.resolve(File::create(context.get(), result.returnValue(), { }, protectedThis->name()));
    });
}

void FileSystemFileHandle::createSyncAccessHandle(DOMPromiseDeferred<IDLInterface<FileSystemSyncAccessHandle>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().createSyncAccessHandle(identifier(), [protectedThis = Ref { *this }, promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        auto info = result.releaseReturnValue();
        if (!info.file)
            return promise.reject(Exception { ExceptionCode::UnknownError, "Invalid platform file handle"_s });

        RefPtr context = protectedThis->scriptExecutionContext();
        if (!context) {
            protectedThis->closeSyncAccessHandle(info.identifier);
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });
        }

        promise.resolve(FileSystemSyncAccessHandle::create(*context, protectedThis.get(), info.identifier, WTFMove(info.file), info.capacity));
    });
}

void FileSystemFileHandle::closeSyncAccessHandle(FileSystemSyncAccessHandleIdentifier accessHandleIdentifier)
{
    if (isClosed())
        return;

    downcast<WorkerFileSystemStorageConnection>(connection()).closeSyncAccessHandle(identifier(), accessHandleIdentifier);
}

std::optional<uint64_t> FileSystemFileHandle::requestNewCapacityForSyncAccessHandle(FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t newCapacity)
{
    if (isClosed())
        return std::nullopt;

    return downcast<WorkerFileSystemStorageConnection>(connection()).requestNewCapacityForSyncAccessHandle(identifier(), accessHandleIdentifier, newCapacity);
}

void FileSystemFileHandle::registerSyncAccessHandle(FileSystemSyncAccessHandleIdentifier identifier, FileSystemSyncAccessHandle& handle)
{
    if (isClosed())
        return;

    downcast<WorkerFileSystemStorageConnection>(connection()).registerSyncAccessHandle(identifier, handle);
}

void FileSystemFileHandle::unregisterSyncAccessHandle(FileSystemSyncAccessHandleIdentifier identifier)
{
    if (isClosed())
        return;

    connection().unregisterSyncAccessHandle(identifier);
}

} // namespace WebCore

