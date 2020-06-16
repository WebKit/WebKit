/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "IDBFactory.h"

#if ENABLE(INDEXED_DATABASE)

#include "Document.h"
#include "IDBBindingUtilities.h"
#include "IDBConnectionProxy.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBKey.h"
#include "IDBOpenDBRequest.h"
#include "JSIDBFactory.h"
#include "Logging.h"
#include "Page.h"
#include "ScriptExecutionContext.h"


namespace WebCore {
using namespace JSC;

static bool shouldThrowSecurityException(ScriptExecutionContext& context)
{
    ASSERT(is<Document>(context) || context.isWorkerGlobalScope());
    if (is<Document>(context)) {
        Document& document = downcast<Document>(context);
        if (!document.frame())
            return true;
        if (!document.page())
            return true;
    }

    if (!context.securityOrigin()->canAccessDatabase(context.topOrigin()))
        return true;

    return false;
}

Ref<IDBFactory> IDBFactory::create(IDBClient::IDBConnectionProxy& connectionProxy)
{
    return adoptRef(*new IDBFactory(connectionProxy));
}

IDBFactory::IDBFactory(IDBClient::IDBConnectionProxy& connectionProxy)
    : m_connectionProxy(connectionProxy)
{
}

IDBFactory::~IDBFactory() = default;

ExceptionOr<Ref<IDBOpenDBRequest>> IDBFactory::open(ScriptExecutionContext& context, const String& name, Optional<uint64_t> version)
{
    LOG(IndexedDB, "IDBFactory::open");
    
    if (version && !version.value())
        return Exception { TypeError, "IDBFactory.open() called with a version of 0"_s };

    return openInternal(context, name, version.valueOr(0));
}

ExceptionOr<Ref<IDBOpenDBRequest>> IDBFactory::openInternal(ScriptExecutionContext& context, const String& name, uint64_t version)
{
    if (name.isNull())
        return Exception { TypeError, "IDBFactory.open() called without a database name"_s };

    if (shouldThrowSecurityException(context))
        return Exception { SecurityError, "IDBFactory.open() called in an invalid security context"_s };

    ASSERT(context.securityOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, SecurityOriginData { context.securityOrigin()->data() }, SecurityOriginData { context.topOrigin().data() });
    if (!databaseIdentifier.isValid())
        return Exception { TypeError, "IDBFactory.open() called with an invalid security origin"_s };

    LOG(IndexedDBOperations, "IDB opening database: %s %" PRIu64, name.utf8().data(), version);

    return m_connectionProxy->openDatabase(context, databaseIdentifier, version);
}

ExceptionOr<Ref<IDBOpenDBRequest>> IDBFactory::deleteDatabase(ScriptExecutionContext& context, const String& name)
{
    LOG(IndexedDB, "IDBFactory::deleteDatabase - %s", name.utf8().data());

    if (name.isNull())
        return Exception { TypeError, "IDBFactory.deleteDatabase() called without a database name"_s };

    if (shouldThrowSecurityException(context))
        return Exception { SecurityError, "IDBFactory.deleteDatabase() called in an invalid security context"_s };

    ASSERT(context.securityOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, SecurityOriginData { context.securityOrigin()->data() }, SecurityOriginData { context.topOrigin().data() });
    if (!databaseIdentifier.isValid())
        return Exception { TypeError, "IDBFactory.deleteDatabase() called with an invalid security origin"_s };

    LOG(IndexedDBOperations, "IDB deleting database: %s", name.utf8().data());

    return m_connectionProxy->deleteDatabase(context, databaseIdentifier);
}

ExceptionOr<short> IDBFactory::cmp(JSGlobalObject& execState, JSValue firstValue, JSValue secondValue)
{
    auto first = scriptValueToIDBKey(execState, firstValue);
    if (!first->isValid())
        return Exception { DataError, "Failed to execute 'cmp' on 'IDBFactory': The parameter is not a valid key."_s };

    auto second = scriptValueToIDBKey(execState, secondValue);
    if (!second->isValid())
        return Exception { DataError, "Failed to execute 'cmp' on 'IDBFactory': The parameter is not a valid key."_s };

    return first->compare(second.get());
}

void IDBFactory::databases(ScriptExecutionContext& context, IDBDatabasesResponsePromise&& promise)
{
    LOG(IndexedDB, "IDBFactory::databases");

    if (shouldThrowSecurityException(context)) {
        promise.reject(SecurityError);
        return;
    }

    ASSERT(context.securityOrigin());

    m_connectionProxy->getAllDatabaseNamesAndVersions(context, [promise = WTFMove(promise)](auto&& result) mutable {
        if (!result) {
            promise.reject(Exception { UnknownError });
            return;
        }

        promise.resolve(WTF::map(*result, [](auto&& info) {
            return IDBFactory::DatabaseInfo { WTFMove(info.name), info.version };
        }));
    });
}

void IDBFactory::getAllDatabaseNames(ScriptExecutionContext& context, Function<void(const Vector<String>&)>&& callback)
{
    m_connectionProxy->getAllDatabaseNamesAndVersions(context, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result) {
            callback({ });
            return;
        }

        callback(WTF::map(*result, [](auto&& info) {
            return WTFMove(info.name);
        }));
    });
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
