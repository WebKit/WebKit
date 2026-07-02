/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "FileSystemStorageConnection.h"

#include "ClientOrigin.h"
#include "Document.h"
#include "DocumentPage.h"
#include "Exception.h"
#include "FileSystemWritableFileStream.h"
#include "StorageConnection.h"
#include "WorkerFileSystemStorageConnection.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

bool FileSystemStorageConnection::errorFileSystemWritable(FileSystemWritableFileStreamIdentifier identifier)
{
    RefPtr writable = m_writables.take(identifier).get();
    if (writable)
        writable->errorIfPossible(Exception { ExceptionCode::AbortError });
    return writable;
}

void FileSystemStorageConnection::registerFileSystemWritable(FileSystemWritableFileStreamIdentifier identifier, FileSystemWritableFileStream& writer)
{
    ASSERT(!m_writables.contains(identifier));
    m_writables.add(identifier, WeakPtr { writer });
}

void FileSystemStorageConnection::unregisterFileSystemWritable(FileSystemWritableFileStreamIdentifier identifier)
{
    m_writables.remove(identifier);
}

FileSystemHandleKeepAlive::FileSystemHandleKeepAlive(ClientOrigin&& origin, FileSystemHandleGlobalIdentifier globalIdentifier, Ref<FileSystemStorageConnection>&& connection)
    : m_globalIdentifier(globalIdentifier)
    , m_origin(WTF::move(origin))
    , m_connection(WTF::move(connection))
{
    protect(m_connection)->addGlobalIdentifierReference(ClientOrigin { m_origin }, *m_globalIdentifier);
}

FileSystemHandleKeepAlive::~FileSystemHandleKeepAlive()
{
    if (RefPtr connection = m_connection; connection && m_globalIdentifier)
        connection->removeGlobalIdentifierReferences(ClientOrigin { m_origin }, { *m_globalIdentifier });
}

FileSystemHandleKeepAlive& FileSystemHandleKeepAlive::operator=(FileSystemHandleKeepAlive&& other)
{
    if (this != &other) {
        if (RefPtr connection = m_connection; connection && m_globalIdentifier)
            connection->removeGlobalIdentifierReferences(ClientOrigin { m_origin }, { *m_globalIdentifier });
        m_globalIdentifier = std::exchange(other.m_globalIdentifier, Markable<FileSystemHandleGlobalIdentifier> { });
        m_origin = WTF::move(other.m_origin);
        m_connection = WTF::move(other.m_connection);
    }
    return *this;
}

FileSystemHandleKeepAlive FileSystemHandleKeepAlive::copy() const
{
    if (!m_globalIdentifier || !m_connection)
        return { };
    return FileSystemHandleKeepAlive(ClientOrigin { m_origin }, *m_globalIdentifier, Ref { *m_connection });
}

RefPtr<FileSystemStorageConnection> fileSystemStorageConnectionForContext(ScriptExecutionContext& context)
{
    if (auto* workerScope = dynamicDowncast<WorkerGlobalScope>(context))
        return workerScope->fileSystemStorageConnection();

    if (auto* document = dynamicDowncast<Document>(context)) {
        if (RefPtr storageConnection = document->storageConnection())
            return storageConnection->fileSystemStorageConnection();
    }

    return nullptr;
}

ClientOrigin clientOriginForContext(ScriptExecutionContext& context)
{
    if (auto* workerScope = dynamicDowncast<WorkerGlobalScope>(context))
        return workerScope->clientOrigin();
    if (auto* document = dynamicDowncast<Document>(context))
        return { document->topOrigin().data(), document->securityOrigin().data() };
    return { };
}

} // namespace WebCore
