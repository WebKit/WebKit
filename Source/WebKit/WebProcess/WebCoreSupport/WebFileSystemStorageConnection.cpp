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
#include <WebCore/FileSystemHandleCloseScope.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/WorkerFileSystemStorageConnection.h>
#include <WebCore/WorkerGlobalScope.h>

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

    for (auto identifier : m_syncAccessHandles.keys())
        invalidateAccessHandle(identifier);
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
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    if (identifier == otherIdentifier)
        return completionHandler(true);

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::IsSameEntry(identifier, otherIdentifier), WTFMove(completionHandler));
}

void WebFileSystemStorageConnection::getFileHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool createIfNecessary, WebCore::FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetFileHandle(identifier, name, createIfNecessary), [this, protectedThis = Ref { *this }, name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        auto identifier = result.value();
        ASSERT(identifier.isValid());
        completionHandler(WebCore::FileSystemHandleCloseScope::create(identifier, false, *this));
    });
}

void WebFileSystemStorageConnection::getDirectoryHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool createIfNecessary, WebCore::FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetDirectoryHandle(identifier, name, createIfNecessary), [this, protectedThis = Ref { *this }, name, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        auto identifier = result.value();
        ASSERT(identifier.isValid());
        completionHandler(WebCore::FileSystemHandleCloseScope::create(identifier, true, *this));
    });
}

void WebFileSystemStorageConnection::removeEntry(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, WebCore::FileSystemStorageConnection::VoidCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::RemoveEntry(identifier, name, deleteRecursively), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
        return completionHandler(convertToExceptionOr(error));
    });
}

void WebFileSystemStorageConnection::resolve(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier otherIdentifier, WebCore::FileSystemStorageConnection::ResolveCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::Resolve(identifier, otherIdentifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::getFile(WebCore::FileSystemHandleIdentifier identifier, StringCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetFile(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::createSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemStorageConnection::GetAccessHandleCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::CreateSyncAccessHandle(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(std::pair { result.value().first, result.value().second.release() });
    });
}

void WebFileSystemStorageConnection::closeSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier)
{
    if (!m_connection)
        return;

    m_connection->send(Messages::NetworkStorageManager::CloseSyncAccessHandle(identifier, accessHandleIdentifier), 0);
}

void WebFileSystemStorageConnection::getHandleNames(WebCore::FileSystemHandleIdentifier identifier, FileSystemStorageConnection::GetHandleNamesCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetHandleNames(identifier), [completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));

        completionHandler(WTFMove(result.value()));
    });
}

void WebFileSystemStorageConnection::getHandle(WebCore::FileSystemHandleIdentifier identifier, const String& name, FileSystemStorageConnection::GetHandleCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::GetHandle(identifier, name), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](auto result) mutable {
        if (!result)
            return completionHandler(convertToException(result.error()));
        
        auto [identifier, isDirectory] = result.value();
        ASSERT(identifier.isValid());
        completionHandler(WebCore::FileSystemHandleCloseScope::create(identifier, isDirectory, *this));
    });
}

void WebFileSystemStorageConnection::move(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, VoidCallback&& completionHandler)
{
    if (!m_connection)
        return completionHandler(WebCore::Exception { WebCore::UnknownError, "Connection is lost"_s });

    m_connection->sendWithAsyncReply(Messages::NetworkStorageManager::Move(identifier, destinationIdentifier, newName), [completionHandler = WTFMove(completionHandler)](auto error) mutable {
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
            if (FileSystemStorageConnection* connection = downcast<WebCore::WorkerGlobalScope>(context).fileSystemStorageConnection())
                connection->invalidateAccessHandle(identifier);
        });
    }
}

} // namespace WebKit
