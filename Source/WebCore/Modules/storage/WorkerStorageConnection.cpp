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
#include "WorkerStorageConnection.h"

#include "ClientOrigin.h"
#include "Document.h"
#include "StorageEstimate.h"
#include "WorkerFileSystemStorageConnection.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <wtf/Scope.h>

namespace WebCore {

Ref<WorkerStorageConnection> WorkerStorageConnection::create(WorkerGlobalScope& scope)
{
    return adoptRef(*new WorkerStorageConnection(scope));
}

WorkerStorageConnection::WorkerStorageConnection(WorkerGlobalScope& scope)
    : m_scope(scope)
{
}

void WorkerStorageConnection::scopeClosed()
{
    auto getPersistedCallbacks = std::exchange(m_getPersistedCallbacks, { });
    for (auto& callback : getPersistedCallbacks.values())
        callback(false);

    auto getDirectoryCallbacks = std::exchange(m_getDirectoryCallbacks, { });
    for (auto& callback : getDirectoryCallbacks.values())
        callback(Exception { ExceptionCode::InvalidStateError });

    m_scope = nullptr;
}

void WorkerStorageConnection::getPersisted(ClientOrigin&& origin, StorageConnection::PersistCallback&& completionHandler)
{
    ASSERT(m_scope);

    auto* workerLoaderProxy = m_scope->thread().workerLoaderProxy();
    if (!workerLoaderProxy)
        return completionHandler(false);

    auto callbackIdentifier = ++m_lastCallbackIdentifier;
    m_getPersistedCallbacks.add(callbackIdentifier, WTFMove(completionHandler));

    workerLoaderProxy->postTaskToLoader([callbackIdentifier, contextIdentifier = m_scope->identifier(), origin = WTFMove(origin).isolatedCopy()](auto& context) mutable {
        ASSERT(isMainThread());

        auto& document = downcast<Document>(context);
        auto mainThreadConnection = document.storageConnection();
        auto mainThreadCallback = [callbackIdentifier, contextIdentifier](bool result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [callbackIdentifier, result] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).storageConnection().didGetPersisted(callbackIdentifier, result);
            });
        };
        if (!mainThreadConnection)
            return mainThreadCallback(false);

        mainThreadConnection->getPersisted(WTFMove(origin), WTFMove(mainThreadCallback));
    });
}

void WorkerStorageConnection::didGetPersisted(uint64_t callbackIdentifier, bool persisted)
{
    if (auto callback = m_getPersistedCallbacks.take(callbackIdentifier))
        callback(persisted);
}

void WorkerStorageConnection::getEstimate(ClientOrigin&& origin, StorageConnection::GetEstimateCallback&& completionHandler)
{
    ASSERT(m_scope);

    auto* workerLoaderProxy = m_scope->thread().workerLoaderProxy();
    if (!workerLoaderProxy)
        return completionHandler(Exception { ExceptionCode::InvalidStateError });

    auto callbackIdentifier = ++m_lastCallbackIdentifier;
    m_getEstimateCallbacks.add(callbackIdentifier, WTFMove(completionHandler));

    workerLoaderProxy->postTaskToLoader([callbackIdentifier, contextIdentifier = m_scope->identifier(), origin = WTFMove(origin).isolatedCopy()](auto& context) mutable {
        ASSERT(isMainThread());

        auto& document = downcast<Document>(context);
        auto mainThreadConnection = document.storageConnection();
        auto mainThreadCallback = [callbackIdentifier, contextIdentifier](ExceptionOr<StorageEstimate>&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [callbackIdentifier, result = crossThreadCopy(WTFMove(result))] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).storageConnection().didGetEstimate(callbackIdentifier, WTFMove(result));
            });
        };
        if (!mainThreadConnection)
            return mainThreadCallback(Exception { ExceptionCode::InvalidStateError });

        mainThreadConnection->getEstimate(WTFMove(origin), WTFMove(mainThreadCallback));
    });
}

void WorkerStorageConnection::didGetEstimate(uint64_t callbackIdentifier, ExceptionOr<StorageEstimate>&& result)
{
    if (auto callback = m_getEstimateCallbacks.take(callbackIdentifier))
        callback(WTFMove(result));
}

void WorkerStorageConnection::fileSystemGetDirectory(ClientOrigin&& origin, StorageConnection::GetDirectoryCallback&& completionHandler)
{
    ASSERT(m_scope);

    auto* workerLoaderProxy = m_scope->thread().workerLoaderProxy();
    if (!workerLoaderProxy)
        return completionHandler(Exception { ExceptionCode::InvalidStateError });
    
    auto callbackIdentifier = ++m_lastCallbackIdentifier;
    m_getDirectoryCallbacks.add(callbackIdentifier, WTFMove(completionHandler));

    workerLoaderProxy->postTaskToLoader([callbackIdentifier, contextIdentifier = m_scope->identifier(), origin = WTFMove(origin).isolatedCopy()](auto& context) mutable {
        ASSERT(isMainThread());

        auto& document = downcast<Document>(context);
        auto mainThreadConnection = document.storageConnection();
        auto mainThreadCallback = [callbackIdentifier, contextIdentifier](auto&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [callbackIdentifier, result = crossThreadCopy(WTFMove(result))] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).storageConnection().didGetDirectory(callbackIdentifier, WTFMove(result));
            });
        };
        if (!mainThreadConnection)
            return mainThreadCallback(Exception { ExceptionCode::InvalidStateError });

        mainThreadConnection->fileSystemGetDirectory(WTFMove(origin), WTFMove(mainThreadCallback));
    });
}

void WorkerStorageConnection::didGetDirectory(uint64_t callbackIdentifier, ExceptionOr<StorageConnection::DirectoryInfo>&& result)
{
    RefPtr<FileSystemStorageConnection> mainThreadFileSystemStorageConnection = result.hasException() ? nullptr : result.returnValue().second;
    auto releaseConnectionScope = makeScopeExit([connection = mainThreadFileSystemStorageConnection]() mutable {
        if (connection)
            callOnMainThread([connection = WTFMove(connection)]() { });
    });

    auto callback = m_getDirectoryCallbacks.take(callbackIdentifier);
    if (!callback)
        return;

    if (result.hasException())
        return callback(WTFMove(result));

    if (!m_scope)
        return callback(Exception { ExceptionCode::InvalidStateError });
    releaseConnectionScope.release();

    auto& workerFileSystemStorageConnection = m_scope->getFileSystemStorageConnection(Ref { *mainThreadFileSystemStorageConnection });
    callback(StorageConnection::DirectoryInfo { result.returnValue().first, Ref { workerFileSystemStorageConnection } });
}

} // namespace WebCore
