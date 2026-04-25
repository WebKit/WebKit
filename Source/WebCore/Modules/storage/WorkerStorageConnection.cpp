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
#include "WorkerStorageConnection.h"

#include "ClientOrigin.h"
#include "Document.h"
#include "ExceptionOr.h"
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
    RefPtr scope = m_scope.get();
    ASSERT(scope);

    CheckedPtr workerLoaderProxy = scope->thread()->workerLoaderProxy();
    if (!workerLoaderProxy)
        return completionHandler(false);

    auto callbackIdentifier = ++m_lastCallbackIdentifier;
    m_getPersistedCallbacks.add(callbackIdentifier, WTF::move(completionHandler));

    workerLoaderProxy->postTaskToLoader([callbackIdentifier, contextIdentifier = scope->identifier(), origin = WTF::move(origin).isolatedCopy()](auto& context) mutable {
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

        mainThreadConnection->getPersisted(WTF::move(origin), WTF::move(mainThreadCallback));
    });
}

void WorkerStorageConnection::didGetPersisted(uint64_t callbackIdentifier, bool persisted)
{
    if (auto callback = m_getPersistedCallbacks.take(callbackIdentifier))
        callback(persisted);
}

void WorkerStorageConnection::getEstimate(ClientOrigin&& origin, StorageConnection::GetEstimateCallback&& completionHandler)
{
    RefPtr scope = m_scope.get();
    ASSERT(scope);

    CheckedPtr workerLoaderProxy = scope->thread()->workerLoaderProxy();
    if (!workerLoaderProxy)
        return completionHandler(Exception { ExceptionCode::InvalidStateError });

    auto callbackIdentifier = ++m_lastCallbackIdentifier;
    m_getEstimateCallbacks.add(callbackIdentifier, WTF::move(completionHandler));

    workerLoaderProxy->postTaskToLoader([callbackIdentifier, contextIdentifier = scope->identifier(), origin = WTF::move(origin).isolatedCopy()](auto& context) mutable {
        ASSERT(isMainThread());

        auto& document = downcast<Document>(context);
        auto mainThreadConnection = document.storageConnection();
        auto mainThreadCallback = [callbackIdentifier, contextIdentifier](ExceptionOr<StorageEstimate>&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [callbackIdentifier, result = crossThreadCopy(WTF::move(result))] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).storageConnection().didGetEstimate(callbackIdentifier, WTF::move(result));
            });
        };
        if (!mainThreadConnection)
            return mainThreadCallback(Exception { ExceptionCode::InvalidStateError });

        mainThreadConnection->getEstimate(WTF::move(origin), WTF::move(mainThreadCallback));
    });
}

void WorkerStorageConnection::didGetEstimate(uint64_t callbackIdentifier, ExceptionOr<StorageEstimate>&& result)
{
    if (auto callback = m_getEstimateCallbacks.take(callbackIdentifier))
        callback(WTF::move(result));
}

void WorkerStorageConnection::fileSystemGetDirectory(ClientOrigin&& origin, StorageConnection::GetDirectoryCallback&& completionHandler)
{
    RefPtr scope = m_scope.get();
    ASSERT(scope);

    CheckedPtr workerLoaderProxy = scope->thread()->workerLoaderProxy();
    if (!workerLoaderProxy)
        return completionHandler(Exception { ExceptionCode::InvalidStateError });
    
    auto callbackIdentifier = ++m_lastCallbackIdentifier;
    m_getDirectoryCallbacks.add(callbackIdentifier, WTF::move(completionHandler));

    workerLoaderProxy->postTaskToLoader([callbackIdentifier, contextIdentifier = m_scope->identifier(), origin = WTF::move(origin).isolatedCopy()](auto& context) mutable {
        ASSERT(isMainThread());

        auto& document = downcast<Document>(context);
        auto mainThreadConnection = document.storageConnection();
        auto mainThreadCallback = [callbackIdentifier, contextIdentifier](auto&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [callbackIdentifier, result = crossThreadCopy(WTF::move(result))] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).storageConnection().didGetDirectory(callbackIdentifier, WTF::move(result));
            });
        };
        if (!mainThreadConnection)
            return mainThreadCallback(Exception { ExceptionCode::InvalidStateError });

        mainThreadConnection->fileSystemGetDirectory(WTF::move(origin), WTF::move(mainThreadCallback));
    });
}

void WorkerStorageConnection::didGetDirectory(uint64_t callbackIdentifier, ExceptionOr<StorageConnection::DirectoryInfo>&& result)
{
    RefPtr<FileSystemStorageConnection> mainThreadFileSystemStorageConnection = result.hasException() ? nullptr : result.returnValue().second;
    auto releaseConnectionScope = makeScopeExit([connection = mainThreadFileSystemStorageConnection]() mutable {
        if (connection)
            callOnMainThread([connection = WTF::move(connection)]() { });
    });

    auto callback = m_getDirectoryCallbacks.take(callbackIdentifier);
    if (!callback)
        return;

    if (result.hasException())
        return callback(WTF::move(result));

    RefPtr scope = m_scope.get();
    if (!scope)
        return callback(Exception { ExceptionCode::InvalidStateError });
    releaseConnectionScope.release();

    Ref workerFileSystemStorageConnection = scope->getFileSystemStorageConnection(Ref { *mainThreadFileSystemStorageConnection });
    callback(StorageConnection::DirectoryInfo { result.returnValue().first, workerFileSystemStorageConnection });
}

} // namespace WebCore
