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
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertSequences.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFileSystemDirectoryHandle.h"
#include "JSFileSystemFileHandle.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileSystemDirectoryHandle);

Ref<FileSystemDirectoryHandle> FileSystemDirectoryHandle::create(ScriptExecutionContext& context, String&& name, FileSystemHandleGlobalIdentifier globalIdentifier, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
{
    Ref result = adoptRef(*new FileSystemDirectoryHandle(context, WTF::move(name), globalIdentifier, identifier, WTF::move(connection)));
    result->suspendIfNeeded();
    return result;
}

Ref<FileSystemDirectoryHandle> FileSystemDirectoryHandle::create(ScriptExecutionContext& context, String&& name, FileSystemHandleGlobalIdentifier globalIdentifier)
{
    Ref result = adoptRef(*new FileSystemDirectoryHandle(context, WTF::move(name), globalIdentifier, { }, nullptr));
    result->suspendIfNeeded();
    return result;
}

FileSystemDirectoryHandle::FileSystemDirectoryHandle(ScriptExecutionContext& context, String&& name, FileSystemHandleGlobalIdentifier globalIdentifier, Markable<FileSystemHandleIdentifier> identifier, RefPtr<FileSystemStorageConnection>&& connection)
    : FileSystemHandle(context, FileSystemHandle::Kind::Directory, WTF::move(name), globalIdentifier, identifier, WTF::move(connection))
{
}

void FileSystemDirectoryHandle::getFileHandle(const String& name, const FileSystemDirectoryHandle::GetFileOptions& options, DOMPromiseDeferred<IDLInterface<FileSystemFileHandle>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    ensureIdentifier([protectedThis = Ref { *this }, name, options, promise = WTF::move(promise)](bool success) mutable {
        if (!success)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        Ref connection = protectedThis->connection();
        connection->getFileHandle(protectedThis->identifier(), name, options.create, [weakContext = WeakPtr { *protectedThis->scriptExecutionContext() }, connection = WTF::move(connection), name, promise = WTF::move(promise)](auto result) mutable {
            if (result.hasException())
                return promise.reject(result.releaseException());

            RefPtr context = weakContext.get();
            if (!context)
                return promise.reject(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });

            auto info = result.returnValue()->release();
            ASSERT(info.kind == FileSystemHandleKind::File);
            Ref handle = FileSystemFileHandle::create(*context, String { name }, info.globalIdentifier, info.identifier, WTF::move(connection));
            promise.resolve(handle);
        });
    });
}

void FileSystemDirectoryHandle::getDirectoryHandle(const String& name, const FileSystemDirectoryHandle::GetDirectoryOptions& options, DOMPromiseDeferred<IDLInterface<FileSystemDirectoryHandle>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    ensureIdentifier([protectedThis = Ref { *this }, name, options, promise = WTF::move(promise)](bool success) mutable {
        if (!success)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        Ref connection = protectedThis->connection();
        connection->getDirectoryHandle(protectedThis->identifier(), name, options.create, [weakContext = WeakPtr { *protectedThis->scriptExecutionContext() }, connection = WTF::move(connection), name, promise = WTF::move(promise)](auto result) mutable {
            if (result.hasException())
                return promise.reject(result.releaseException());

            RefPtr context = weakContext.get();
            if (!context)
                return promise.reject(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });

            auto info = result.returnValue()->release();
            ASSERT(info.kind == FileSystemHandleKind::Directory);
            Ref handle = FileSystemDirectoryHandle::create(*context, String { name }, info.globalIdentifier, info.identifier, WTF::move(connection));
            promise.resolve(handle);
        });
    });
}

void FileSystemDirectoryHandle::removeEntry(const String& name, const FileSystemDirectoryHandle::RemoveOptions& options, DOMPromiseDeferred<void>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    ensureIdentifier([protectedThis = Ref { *this }, name, options, promise = WTF::move(promise)](bool success) mutable {
        if (!success)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        protect(protectedThis->connection())->removeEntry(protectedThis->identifier(), name, options.recursive, [promise = WTF::move(promise)](auto result) mutable {
            promise.settle(WTF::move(result));
        });
    });
}

void FileSystemDirectoryHandle::resolve(const FileSystemHandle& handle, DOMPromiseDeferred<IDLNullable<IDLSequence<IDLUSVString>>>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    ensureIdentifier([protectedThis = Ref { *this }, handle = Ref { handle }, promise = WTF::move(promise)](bool success) mutable {
        if (!success)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        protect(protectedThis->connection())->resolve(protectedThis->identifier(), handle->identifier(), [promise = WTF::move(promise)](auto result) mutable {
            promise.settle(WTF::move(result));
        });
    });
}

void FileSystemDirectoryHandle::getHandleNames(CompletionHandler<void(ExceptionOr<Vector<String>>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    ensureIdentifier([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](bool success) mutable {
        if (!success)
            return completionHandler(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        protect(protectedThis->connection())->getHandleNames(protectedThis->identifier(), WTF::move(completionHandler));
    });
}

void FileSystemDirectoryHandle::getHandle(const String& name, CompletionHandler<void(ExceptionOr<Ref<FileSystemHandle>>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    ensureIdentifier([protectedThis = Ref { *this }, name, completionHandler = WTF::move(completionHandler)](bool success) mutable {
        if (!success)
            return completionHandler(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        Ref connection = protectedThis->connection();
        connection->getHandle(protectedThis->identifier(), name, [weakContext = WeakPtr { *protectedThis->scriptExecutionContext() }, name, connection = WTF::move(connection), completionHandler = WTF::move(completionHandler)](auto result) mutable {
            if (result.hasException())
                return completionHandler(result.releaseException());

            auto info = result.returnValue()->release();
            RefPtr context = weakContext.get();
            if (!context)
                return completionHandler(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });

            if (info.kind == FileSystemHandleKind::Directory) {
                Ref<FileSystemHandle> handle = FileSystemDirectoryHandle::create(*context, String { name }, info.globalIdentifier, info.identifier, WTF::move(connection));
                return completionHandler(WTF::move(handle));
            }

            Ref<FileSystemHandle> handle = FileSystemFileHandle::create(*context, String { name }, info.globalIdentifier, info.identifier, WTF::move(connection));
            completionHandler(WTF::move(handle));
        });
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

    auto wrappedCompletionHandler = [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](auto result) mutable {
        protectedThis->m_isWaitingForResult = false;
        completionHandler(WTF::move(result));
    };

    if (!m_isInitialized) {
        m_source->getHandleNames([protectedThis = Ref { *this }, completionHandler = WTF::move(wrappedCompletionHandler)](auto result) mutable {
            protectedThis->m_isInitialized = true;
            if (result.hasException())
                return completionHandler(result.releaseException());

            protectedThis->m_keys = result.releaseReturnValue();
            protectedThis->advance(WTF::move(completionHandler));
        });
        return;
    }

    advance(WTF::move(wrappedCompletionHandler));
}

void FileSystemDirectoryHandleIterator::advance(CompletionHandler<void(ExceptionOr<Result>&&)>&& completionHandler)
{
    ASSERT(m_isInitialized);

    if (m_index >= m_keys.size()) {
        Result result = std::nullopt;
        return completionHandler(Result { });
    }

    auto key = m_keys[m_index++];
    m_source->getHandle(key, [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), key](auto result) mutable {
        if (result.hasException()) {
            if (result.exception().code() == ExceptionCode::NotFoundError)
                return protectedThis->advance(WTF::move(completionHandler));

            return completionHandler(result.releaseException());
        }

        Result resultValue = KeyValuePair<String, Ref<FileSystemHandle>> { WTF::move(key), result.releaseReturnValue() };
        completionHandler(WTF::move(resultValue));
    });
}

} // namespace WebCore


