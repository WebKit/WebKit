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
#include "StorageManager.h"

#include "ClientOrigin.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "FileSystemDirectoryHandle.h"
#include "FileSystemStorageConnection.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFileSystemDirectoryHandle.h"
#include "JSStorageManager.h"
#include "NavigatorBase.h"
#include "SecurityOrigin.h"
#include "WorkerGlobalScope.h"
#include "WorkerStorageConnection.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(StorageManager);

Ref<StorageManager> StorageManager::create(NavigatorBase& navigator)
{
    return adoptRef(*new StorageManager(navigator));
}

StorageManager::StorageManager(NavigatorBase& navigator)
    : m_navigator(navigator)
{
}

struct ConnectionInfo {
    StorageConnection& connection;
    ClientOrigin origin;
};

static ExceptionOr<ConnectionInfo> connectionInfo(NavigatorBase* navigator)
{
    if (!navigator)
        return Exception { ExceptionCode::InvalidStateError, "Navigator does not exist"_s };

    RefPtr context = navigator->scriptExecutionContext();
    if (!context)
        return Exception { ExceptionCode::InvalidStateError, "Context is invalid"_s };

    if (context->canAccessResource(ScriptExecutionContext::ResourceType::StorageManager) == ScriptExecutionContext::HasResourceAccess::No)
        return Exception { ExceptionCode::TypeError, "Context not access storage"_s };

    RefPtr origin = context->securityOrigin();
    ASSERT(origin);

    if (RefPtr document = dynamicDowncast<Document>(context)) {
        if (RefPtr connection = document->storageConnection())
            return ConnectionInfo { *connection, { document->topOrigin().data(), origin->data() } };

        return Exception { ExceptionCode::InvalidStateError, "Connection is invalid"_s };
    }

    if (RefPtr globalScope = dynamicDowncast<WorkerGlobalScope>(context))
        return ConnectionInfo { globalScope->storageConnection(), { globalScope->topOrigin().data(), origin->data() } };

    return Exception { ExceptionCode::NotSupportedError };
}

void StorageManager::persisted(DOMPromiseDeferred<IDLBoolean>&& promise)
{
    auto connectionInfoOrException = connectionInfo(m_navigator.get());
    if (connectionInfoOrException.hasException())
        return promise.reject(connectionInfoOrException.releaseException());

    auto connectionInfo = connectionInfoOrException.releaseReturnValue();
    connectionInfo.connection.getPersisted(WTFMove(connectionInfo.origin), [promise = WTFMove(promise)](bool persisted) mutable {
        promise.resolve(persisted);
    });
}

void StorageManager::persist(DOMPromiseDeferred<IDLBoolean>&& promise)
{
    auto connectionInfoOrException = connectionInfo(m_navigator.get());
    if (connectionInfoOrException.hasException())
        return promise.reject(connectionInfoOrException.releaseException());

    auto connectionInfo = connectionInfoOrException.releaseReturnValue();
    connectionInfo.connection.persist(connectionInfo.origin, [promise = WTFMove(promise)](bool persisted) mutable {
        promise.resolve(persisted);
    });
}

void StorageManager::estimate(DOMPromiseDeferred<IDLDictionary<StorageEstimate>>&& promise)
{
    auto connectionInfoOrException = connectionInfo(m_navigator.get());
    if (connectionInfoOrException.hasException())
        return promise.reject(connectionInfoOrException.releaseException());

    auto connectionInfo = connectionInfoOrException.releaseReturnValue();
    connectionInfo.connection.getEstimate(WTFMove(connectionInfo.origin), [promise = WTFMove(promise)](ExceptionOr<StorageEstimate>&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void StorageManager::fileSystemAccessGetDirectory(DOMPromiseDeferred<IDLInterface<FileSystemDirectoryHandle>>&& promise)
{
    auto connectionInfoOrException = connectionInfo(m_navigator.get());
    if (connectionInfoOrException.hasException())
        return promise.reject(connectionInfoOrException.releaseException());

    auto connectionInfo = connectionInfoOrException.releaseReturnValue();
    connectionInfo.connection.fileSystemGetDirectory(WTFMove(connectionInfo.origin), [promise = WTFMove(promise), weakNavigator = m_navigator](auto&& result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        auto [identifier, connection] = result.releaseReturnValue();
        RefPtr context = weakNavigator ? weakNavigator->scriptExecutionContext() : nullptr;
        if (!context) {
            connection->closeHandle(identifier);
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Context has stopped"_s });
        }

        promise.resolve(FileSystemDirectoryHandle::create(*context, { }, identifier, Ref { *connection }));
    });
}

} // namespace WebCore
