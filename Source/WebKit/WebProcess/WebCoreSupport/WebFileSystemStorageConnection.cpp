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
#include "WebFileSystemStorageConnection.h"

#include "NetworkStorageManagerMessages.h"
#include <WebCore/ExceptionOr.h>
#include <WebCore/FileSystemDirectoryHandle.h>
#include <WebCore/FileSystemFileHandle.h>

namespace WebKit {

Ref<WebFileSystemStorageConnection> WebFileSystemStorageConnection::create(IPC::Connection& connection)
{
    return adoptRef(*new WebFileSystemStorageConnection(connection));
}

WebFileSystemStorageConnection::WebFileSystemStorageConnection(IPC::Connection& connection)
    : m_connection(&connection)
{
}

void WebFileSystemStorageConnection::connectionClosed()
{
    m_connection = nullptr;
}

void WebFileSystemStorageConnection::closeHandle(WebCore::FileSystemHandleIdentifier identifier)
{
    if (!m_connection)
        return;

    m_connection->send(Messages::NetworkStorageManager::CloseHandle(identifier), 0);
}

void WebFileSystemStorageConnection::isSameEntry(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier otherIdentifier, WebCore::FileSystemStorageConnection::SameEntryCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    if (identifier == otherIdentifier)
        return completionHandler(true);

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::IsSameEntry(identifier, otherIdentifier), WTFMove(completionHandler));
}

void WebFileSystemStorageConnection::getFileHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool createIfNecessary, WebCore::FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetFileHandle(identifier, name, createIfNecessary), [name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        auto identifier = result.value();
        ASSERT(identifier.isValid());
        completionHandler(WTFMove(identifier));
    });
}

void WebFileSystemStorageConnection::getDirectoryHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool createIfNecessary, WebCore::FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetDirectoryHandle(identifier, name, createIfNecessary), [name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        auto identifier = result.value();
        ASSERT(identifier.isValid());
        completionHandler(WTFMove(identifier));
    });
}

void WebFileSystemStorageConnection::removeEntry(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, WebCore::FileSystemStorageConnection::VoidCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::RemoveEntry(identifier, name, deleteRecursively), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        return completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::resolve(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier otherIdentifier, WebCore::FileSystemStorageConnection::ResolveCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::Resolve(identifier, otherIdentifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::createSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemStorageConnection::GetAccessHandleCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::CreateSyncAccessHandle(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        auto resultValue = result.value();
        completionHandler(std::pair { resultValue.first, resultValue.second.handle() });
    });
}

void WebFileSystemStorageConnection::getSize(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, IntegerCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetSizeForAccessHandle(identifier, accessHandleIdentifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(result.value());
    });
}

void WebFileSystemStorageConnection::truncate(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t size, VoidCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::TruncateForAccessHandle(identifier, accessHandleIdentifier, size), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::flush(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, VoidCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::FlushForAccessHandle(identifier, accessHandleIdentifier), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::close(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, VoidCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::CloseAccessHandle(identifier, accessHandleIdentifier), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::getHandleNames(WebCore::FileSystemHandleIdentifier identifier, FileSystemStorageConnection::GetHandleNamesCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetHandleNames(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::getHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, FileSystemStorageConnection::GetHandleWithTypeCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetHandle(identifier, name), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::move(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, VoidCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost" });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::Move(identifier, destinationIdentifier, newName), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(convertToExceptionOr(error));
    });
}

} // namespace WebKit
