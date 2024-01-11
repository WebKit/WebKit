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
#include "FileSystemDirectoryHandle.h"

#include "ContextDestructionObserverInlines.h"
#include "FileSystemHandleCloseScope.h"
#include "FileSystemStorageConnection.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFileSystemDirectoryHandle.h"
#include "JSFileSystemFileHandle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FileSystemDirectoryHandle);

Ref<FileSystemDirectoryHandle> FileSystemDirectoryHandle::create(ScriptExecutionContext& context, String&& name, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
{
    auto result = adoptRef(*new FileSystemDirectoryHandle(context, WTFMove(name), identifier, WTFMove(connection)));
    result->suspendIfNeeded();
    return result;
}

FileSystemDirectoryHandle::FileSystemDirectoryHandle(ScriptExecutionContext& context, String&& name, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
    : FileSystemHandle(context, FileSystemHandle::Kind::Directory, WTFMove(name), identifier, WTFMove(connection))
{
}

void FileSystemDirectoryHandle::getFileHandle(const String& name, const FileSystemDirectoryHandle::GetFileOptions& options, DOMPromiseDeferred<IDLInterface<FileSystemFileHandle>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().getFileHandle(identifier(), name, options.create, [weakThis = ThreadSafeWeakPtr { *this }, connection = Ref { connection() }, name, promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        auto protectedThis = weakThis.get();
        RefPtr context = protectedThis ? protectedThis->scriptExecutionContext() : nullptr;
        if (!context)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });

        auto [identifier, isDirectory] = result.returnValue()->release();
        ASSERT(!isDirectory);
        promise.resolve(FileSystemFileHandle::create(*context, String { name }, identifier, WTFMove(connection)));
    });
}

void FileSystemDirectoryHandle::getDirectoryHandle(const String& name, const FileSystemDirectoryHandle::GetDirectoryOptions& options, DOMPromiseDeferred<IDLInterface<FileSystemDirectoryHandle>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().getDirectoryHandle(identifier(), name, options.create, [weakThis = ThreadSafeWeakPtr { *this }, connection = Ref { connection() }, name, promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        auto protectedThis = weakThis.get();
        RefPtr context = protectedThis ? protectedThis->scriptExecutionContext() : nullptr;
        if (!context)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });

        auto [identifier, isDirectory] = result.returnValue()->release();
        ASSERT(isDirectory);
        promise.resolve(FileSystemDirectoryHandle::create(*context, String { name }, identifier, WTFMove(connection)));
    });
}

void FileSystemDirectoryHandle::removeEntry(const String& name, const FileSystemDirectoryHandle::RemoveOptions& options, DOMPromiseDeferred<void>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().removeEntry(identifier(), name, options.recursive, [promise = WTFMove(promise)](auto result) mutable {
        promise.settle(WTFMove(result));
    });
}

void FileSystemDirectoryHandle::resolve(const FileSystemHandle& handle, DOMPromiseDeferred<IDLSequence<IDLUSVString>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().resolve(identifier(), handle.identifier(), [promise = WTFMove(promise)](auto result) mutable {
        promise.settle(WTFMove(result));
    });
}

void FileSystemDirectoryHandle::getHandleNames(CompletionHandler<void(ExceptionOr<Vector<String>>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().getHandleNames(identifier(), WTFMove(completionHandler));
}

void FileSystemDirectoryHandle::getHandle(const String& name, CompletionHandler<void(ExceptionOr<Ref<FileSystemHandle>>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    connection().getHandle(identifier(), name, [weakThis = ThreadSafeWeakPtr { *this }, name, connection = Ref { connection() }, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (result.hasException())
            return completionHandler(result.releaseException());

        auto [identifier, isDirectory] = result.returnValue()->release();
        auto protectedThis = weakThis.get();
        RefPtr context = protectedThis ? protectedThis->scriptExecutionContext() : nullptr;
        if (!context)
            return completionHandler(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });

        if (isDirectory) {
            Ref<FileSystemHandle> handle = FileSystemDirectoryHandle::create(*context, String { name }, identifier, WTFMove(connection));
            return completionHandler(WTFMove(handle));
        }

        Ref<FileSystemHandle> handle = FileSystemFileHandle::create(*context, String { name }, identifier, WTFMove(connection));
        completionHandler(WTFMove(handle));
    });
}

using FileSystemDirectoryHandleIterator = FileSystemDirectoryHandle::Iterator;

Ref<FileSystemDirectoryHandleIterator> FileSystemDirectoryHandle::createIterator(ScriptExecutionContext*)
{
    return Iterator::create(*this);
}

Ref<FileSystemDirectoryHandleIterator> FileSystemDirectoryHandleIterator::create(FileSystemDirectoryHandle& source)
{
    return adoptRef(*new FileSystemDirectoryHandle::Iterator(source));
}

void FileSystemDirectoryHandleIterator::next(CompletionHandler<void(ExceptionOr<Result>&&)>&& completionHandler)
{
    ASSERT(!m_isWaitingForResult);
    m_isWaitingForResult = true;

    auto wrappedCompletionHandler = [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        m_isWaitingForResult = false;
        completionHandler(WTFMove(result));
    };

    if (!m_isInitialized) {
        m_source->getHandleNames([this, protectedThis = Ref { *this }, completionHandler = WTFMove(wrappedCompletionHandler)](auto result) mutable {
            m_isInitialized = true;
            if (result.hasException())
                return completionHandler(result.releaseException());

            m_keys = result.releaseReturnValue();
            advance(WTFMove(completionHandler));
        });
        return;
    }

    advance(WTFMove(wrappedCompletionHandler));
}

void FileSystemDirectoryHandleIterator::advance(CompletionHandler<void(ExceptionOr<Result>&&)>&& completionHandler)
{
    ASSERT(m_isInitialized);

    if (m_index >= m_keys.size()) {
        Result result = std::nullopt;
        return completionHandler(Result { });
    }

    auto key = m_keys[m_index++];
    m_source->getHandle(key, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), key](auto result) mutable {
        if (result.hasException()) {
            if (result.exception().code() == ExceptionCode::NotFoundError)
                return advance(WTFMove(completionHandler));

            return completionHandler(result.releaseException());
        }

        Result resultValue = KeyValuePair<String, Ref<FileSystemHandle>> { WTFMove(key), result.releaseReturnValue() };
        completionHandler(WTFMove(resultValue));
    });
}

} // namespace WebCore


