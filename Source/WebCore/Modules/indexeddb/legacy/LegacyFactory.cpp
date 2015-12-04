/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LegacyFactory.h"

#if ENABLE(INDEXED_DATABASE)

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "IDBBindingUtilities.h"
#include "IDBDatabaseCallbacksImpl.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "LegacyAny.h"
#include "LegacyDatabase.h"
#include "LegacyOpenDBRequest.h"
#include "Logging.h"
#include "Page.h"
#include "PageGroup.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"

namespace WebCore {

LegacyFactory::LegacyFactory(IDBFactoryBackendInterface* factory)
    : m_backend(factory)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

LegacyFactory::~LegacyFactory()
{
}

static bool isContextValid(ScriptExecutionContext* context)
{
    ASSERT(is<Document>(*context) || context->isWorkerGlobalScope());
    if (is<Document>(*context)) {
        Document& document = downcast<Document>(*context);
        return document.frame() && document.page() && (!document.page()->usesEphemeralSession() || SchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(document.securityOrigin()->protocol()));
    }
    return true;
}

RefPtr<IDBRequest> LegacyFactory::getDatabaseNames(ScriptExecutionContext* context, ExceptionCode& ec)
{
    LOG(StorageAPI, "LegacyFactory::getDatabaseNames");
    if (!isContextValid(context))
        return 0;
    if (!context->securityOrigin()->canAccessDatabase(context->topOrigin())) {
        ec = SECURITY_ERR;
        return 0;
    }

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), 0);
    m_backend->getDatabaseNames(request, *(context->securityOrigin()), *(context->topOrigin()), context);
    return request;
}

RefPtr<IDBOpenDBRequest> LegacyFactory::open(ScriptExecutionContext* context, const String& name, ExceptionCode& ec)
{
    LOG(StorageAPI, "LegacyFactory::open");
    return openInternal(context, name, 0, IndexedDB::VersionNullness::Null, ec);
}

RefPtr<IDBOpenDBRequest> LegacyFactory::open(ScriptExecutionContext* context, const String& name, unsigned long long version, ExceptionCode& ec)
{
    LOG(StorageAPI, "LegacyFactory::open");
    if (!version) {
        ec = TypeError;
        return 0;
    }
    return openInternal(context, name, version, IndexedDB::VersionNullness::NonNull, ec);
}

PassRefPtr<IDBOpenDBRequest> LegacyFactory::openInternal(ScriptExecutionContext* context, const String& name, uint64_t version, IndexedDB::VersionNullness versionNullness, ExceptionCode& ec)
{
    ASSERT(version >= 1 || versionNullness == IndexedDB::VersionNullness::Null);
    if (name.isNull()) {
        ec = TypeError;
        return 0;
    }
    if (!isContextValid(context))
        return 0;
    if (!context->securityOrigin()->canAccessDatabase(context->topOrigin())) {
        ec = SECURITY_ERR;
        return 0;
    }

    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = IDBDatabaseCallbacksImpl::create();
    int64_t transactionId = LegacyDatabase::nextTransactionId();
    RefPtr<LegacyOpenDBRequest> request = LegacyOpenDBRequest::create(context, databaseCallbacks, transactionId, version, versionNullness);
    m_backend->open(name, version, transactionId, request, databaseCallbacks, *(context->securityOrigin()), *(context->topOrigin()));
    return request;
}

RefPtr<IDBOpenDBRequest> LegacyFactory::deleteDatabase(ScriptExecutionContext* context, const String& name, ExceptionCode& ec)
{
    LOG(StorageAPI, "LegacyFactory::deleteDatabase");
    if (name.isNull()) {
        ec = TypeError;
        return 0;
    }
    if (!isContextValid(context))
        return 0;
    if (!context->securityOrigin()->canAccessDatabase(context->topOrigin())) {
        ec = SECURITY_ERR;
        return 0;
    }

    RefPtr<LegacyOpenDBRequest> request = LegacyOpenDBRequest::create(context, 0, 0, 0, IndexedDB::VersionNullness::Null);
    m_backend->deleteDatabase(name, *context->securityOrigin(), *context->topOrigin(), request, context);
    return request;
}

short LegacyFactory::cmp(ScriptExecutionContext* context, const Deprecated::ScriptValue& firstValue, const Deprecated::ScriptValue& secondValue, ExceptionCodeWithMessage& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> first = scriptValueToIDBKey(&requestState, firstValue);
    RefPtr<IDBKey> second = scriptValueToIDBKey(&requestState, secondValue);

    ASSERT(first);
    ASSERT(second);

    if (!first->isValid() || !second->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        return 0;
    }

    return static_cast<short>(first->compare(second.get()));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
