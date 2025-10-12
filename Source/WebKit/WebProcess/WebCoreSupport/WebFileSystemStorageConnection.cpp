/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include <WebCore/FileSystemHandleCloseScope.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/WorkerFileSystemStorageConnection.h>
#include <WebCore/WorkerGlobalScope.h>

namespace WebKit {

Ref<WebFileSystemStorageConnection> WebFileSystemStorageConnection::create(Ref<IPC::Connection>&& connection)
{
    return adoptRef(*new WebFileSystemStorageConnection(WTFMove(connection)));
}

WebFileSystemStorageConnection::WebFileSystemStorageConnection(Ref<IPC::Connection>&& connection)
    : m_connection(WTFMove(connection))
{
}

void WebFileSystemStorageConnection::errorWritable(WebCore::ScriptExecutionContextIdentifier contextIdentifier, WebCore::FileSystemWritableFileStreamIdentifier writableIdentifier)
{
    if (errorFileSystemWritable(writableIdentifier))
        return;

    WebCore::ScriptExecutionContext::postTaskTo(contextIdentifier, [writableIdentifier, protectedThis = Ref { *this }](auto& context) mutable {
        RefPtr globalScope = dynamicDowncast<WebCore::WorkerGlobalScope>(context);
        RefPtr connection = globalScope ? globalScope->fileSystemStorageConnection() : nullptr;
        if (connection)
            connection->errorFileSystemWritable(writableIdentifier);
    });
}

void WebFileSystemStorageConnection::connectionClosed()
{
    m_connection = nullptr;

    for (auto identifier : m_syncAccessHandles.keys())
        invalidateAccessHandle(identifier);

    auto writableIdentifiers = std::exchange(m_writableIdentifiers, { });
    for (auto keyValue : writableIdentifiers)
        errorWritable(keyValue.value, keyValue.key);
}

void WebFileSystemStorageConnection::closeHandle(WebCore::FileSystemHandleIdentifier identifier)
{
    RefPtr connection = m_connection;
    if (!connection)
        return;

    connection->send(Messages::NetworkStorageManager::CloseHandle(identifier), 0);
}

void WebFileSystemStorageConnection::isSameEntry(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier otherIdentifier, WebCore::FileSystemStorageConnection::SameEntryCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    if (identifier == otherIdentifier)
        return completionHandler(true);

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::IsSameEntry(identifier, otherIdentifier), WTFMove(completionHandler));
}

void WebFileSystemStorageConnection::getFileHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool createIfNecessary, WebCore::FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetFileHandle(identifier, name, createIfNecessary), [this, protectedThis = Ref { *this }, name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WebCore::FileSystemHandleCloseScope::create(result.value(), false, *this));
    });
}

void WebFileSystemStorageConnection::getDirectoryHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool createIfNecessary, WebCore::FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetDirectoryHandle(identifier, name, createIfNecessary), [this, protectedThis = Ref { *this }, name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WebCore::FileSystemHandleCloseScope::create(result.value(), true, *this));
    });
}

void WebFileSystemStorageConnection::removeEntry(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, WebCore::FileSystemStorageConnection::VoidCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::RemoveEntry(identifier, name, deleteRecursively), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        return completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::resolve(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier otherIdentifier, WebCore::FileSystemStorageConnection::ResolveCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::Resolve(identifier, otherIdentifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::getFile(WebCore::FileSystemHandleIdentifier identifier, StringCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetFile(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::createSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemStorageConnection::GetAccessHandleCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::CreateSyncAccessHandle(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WebCore::FileSystemStorageConnection::SyncAccessHandleInfo { *result->identifier, result->handle.release(), result->capacity });
    });
}

void WebFileSystemStorageConnection::closeSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, FileSystemStorageConnection::EmptyCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler();

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::CloseSyncAccessHandle(identifier, accessHandleIdentifier), WTFMove(completionHandler));
}

void WebFileSystemStorageConnection::getHandleNames(WebCore::FileSystemHandleIdentifier identifier, FileSystemStorageConnection::GetHandleNamesCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetHandleNames(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::getHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetHandle(identifier, name), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));
        
        auto [identifier, isDirectory] = *result.value();
        completionHandler(WebCore::FileSystemHandleCloseScope::create(identifier, isDirectory, *this));
    });
}

void WebFileSystemStorageConnection::move(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, VoidCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::Move(identifier, destinationIdentifier, newName), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::registerSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier identifier, WebCore::ScriptExecutionContextIdentifier contextIdentifier)
{
    m_syncAccessHandles.add(identifier, contextIdentifier);
}

void WebFileSystemStorageConnection::unregisterSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier identifier)
{
    m_syncAccessHandles.remove(identifier);
}

void WebFileSystemStorageConnection::invalidateAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier identifier)
{
    if (auto contextIdentifier = m_syncAccessHandles.get(identifier)) {
        WebCore::ScriptExecutionContext::postTaskTo(contextIdentifier, [identifier](auto& context) mutable {
            // FIXME: We should not have to list FileSystemStorageConnection here.
            if (RefPtr<FileSystemStorageConnection> connection = downcast<WebCore::WorkerGlobalScope>(context).fileSystemStorageConnection())
                connection->invalidateAccessHandle(identifier);
        });
    }
}

void WebFileSystemStorageConnection::createWritable(WebCore::ScriptExecutionContextIdentifier contextIdentifier, WebCore::FileSystemHandleIdentifier identifier, bool keepExistingData, StreamCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::CreateWritable(identifier, keepExistingData), [protectedThis = Ref { *this }, contextIdentifier, completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        ASSERT(!protectedThis->m_writableIdentifiers.contains(result.value()));
        protectedThis->m_writableIdentifiers.add(result.value(), contextIdentifier);
        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::invalidateWritable(WebCore::FileSystemWritableFileStreamIdentifier identifier)
{
    if (auto contextIdentifier = m_writableIdentifiers.take(identifier))
        errorWritable(contextIdentifier, identifier);
}

void WebFileSystemStorageConnection::closeWritable(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCloseReason reason, VoidCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    m_writableIdentifiers.remove(streamIdentifier);
    connection->sendWithAsyncReply(Messages::NetworkStorageManager::CloseWritable(identifier, streamIdentifier, reason), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::executeCommandForWritable(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCommandType type, std::optional<uint64_t> position, std::optional<uint64_t> size, std::span<const uint8_t> dataBytes, bool hasDataError, VoidCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(WebCore::Exception { WebCore::ExceptionCode::UnknownError, "Connection is lost"_s });

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::ExecuteCommandForWritable(identifier, streamIdentifier, type, position, size, dataBytes, hasDataError), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::requestNewCapacityForSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t newCapacity, RequestCapacityCallback&& completionHandler)
{
    RefPtr connection = m_connection;
    if (!connection)
        return completionHandler(std::nullopt);

    connection->sendWithAsyncReply(Messages::NetworkStorageManager::RequestNewCapacityForSyncAccessHandle(identifier, accessHandleIdentifier, newCapacity), WTFMove(completionHandler));
}

} // namespace WebKit
