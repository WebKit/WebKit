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
#include "FileSystemStorageHandleProxy.h"

#include "FileSystemStorageError.h"
#include "NetworkStorageManagerMessages.h"
#include <WebCore/ExceptionOr.h>
#include <WebCore/FileSystemDirectoryHandle.h>
#include <WebCore/FileSystemFileHandle.h>

namespace WebKit {

Ref<FileSystemStorageHandleProxy> FileSystemStorageHandleProxy::create(FileSystemStorageHandleIdentifier identifier, IPC::Connection& connection)
{
    return adoptRef(*new FileSystemStorageHandleProxy(identifier, connection));
}

FileSystemStorageHandleProxy::FileSystemStorageHandleProxy(FileSystemStorageHandleIdentifier identifier, IPC::Connection& connection)
    : m_identifier(identifier)
    , m_connection(&connection)
{
}

void FileSystemStorageHandleProxy::connectionClosed()
{
    m_connection = nullptr;
}

void FileSystemStorageHandleProxy::isSameEntry(FileSystemHandleImpl& handle, CompletionHandler<void(WebCore::ExceptionOr<bool>&&)>&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    auto identifier = handle.storageHandleIdentifier();
    if (!identifier)
        return completionHandler(false);

    if (m_identifier.toUInt64() == *identifier)
        return completionHandler(true);

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::IsSameEntry(m_identifier, makeObjectIdentifier<FileSystemStorageHandleIdentifierType>(*identifier)), [completionHandler = WTFMove(completionHandler)](bool result) mutable {
        completionHandler(result);
    });
}

void FileSystemStorageHandleProxy::getFileHandle(const String& name, bool createIfNecessary, CompletionHandler<void(WebCore::ExceptionOr<Ref<WebCore::FileSystemHandleImpl>>&&)>&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetFileHandle(m_identifier, name, createIfNecessary), [connection = m_connection, name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(WebCore::Exception { convertToExceptionCode(result.error()) });
    
        auto handleIdentifier = result.value();
        if (!handleIdentifier.isValid())
            return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

        Ref<WebCore::FileSystemHandleImpl> impl = FileSystemStorageHandleProxy::create(handleIdentifier, *connection);
        completionHandler(WTFMove(impl));
    });
}

void FileSystemStorageHandleProxy::getDirectoryHandle(const String& name, bool createIfNecessary, CompletionHandler<void(WebCore::ExceptionOr<Ref<WebCore::FileSystemHandleImpl>>&&)>&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetDirectoryHandle(m_identifier, name, createIfNecessary), [connection = m_connection, name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(WebCore::Exception { convertToExceptionCode(result.error()) });
    
        auto handleIdentifier = result.value();
        if (!handleIdentifier.isValid())
            return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

        Ref<WebCore::FileSystemHandleImpl> impl = FileSystemStorageHandleProxy::create(handleIdentifier, *connection);
        completionHandler(WTFMove(impl));
    });
}

void FileSystemStorageHandleProxy::removeEntry(const String& name, bool deleteRecursively, CompletionHandler<void(WebCore::ExceptionOr<void>&&)>&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::RemoveEntry(m_identifier, name, deleteRecursively), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        if (error)
            return completionHandler(WebCore::Exception { convertToExceptionCode(error.value()) });

        completionHandler({ });
    });
}

void FileSystemStorageHandleProxy::resolve(FileSystemHandleImpl& handle, CompletionHandler<void(WebCore::ExceptionOr<Vector<String>>&&)>&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    auto identifier = handle.storageHandleIdentifier();
    if (!identifier)
        return completionHandler(Vector<String> { });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::Resolve(m_identifier, makeObjectIdentifier<FileSystemStorageHandleIdentifierType>(*identifier)), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(WebCore::Exception { convertToExceptionCode(result.error()) });

        completionHandler(WTFMove(result.value()));
    });
}

} // namespace WebKit

