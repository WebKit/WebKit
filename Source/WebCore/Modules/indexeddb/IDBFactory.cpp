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
#include "IDBFactory.h"

#if ENABLE(INDEXED_DATABASE)

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "GroupSettings.h"
#include "IDBBindingUtilities.h"
#include "IDBDatabase.h"
#include "IDBDatabaseCallbacksImpl.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBOpenDBRequest.h"
#include "Logging.h"
#include "Page.h"
#include "PageGroup.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"

namespace WebCore {

IDBFactory::IDBFactory(IDBFactoryBackendInterface* factory)
    : m_backend(factory)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

IDBFactory::~IDBFactory()
{
}

namespace {
static bool isContextValid(ScriptExecutionContext* context)
{
    ASSERT(context->isDocument() || context->isWorkerGlobalScope());
    if (context->isDocument()) {
        Document* document = toDocument(context);
        return document->frame() && document->page() && (!document->page()->usesEphemeralSession() || SchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(document->securityOrigin()->protocol()));
    }
    return true;
}

static String getIndexedDBDatabasePath(ScriptExecutionContext* context)
{
    ASSERT(isContextValid(context));
    if (context->isDocument()) {
        Document* document = toDocument(context);
        return document->page()->group().groupSettings().indexedDBDatabasePath();
    }
    const GroupSettings* groupSettings = toWorkerGlobalScope(context)->groupSettings();
    if (groupSettings)
        return groupSettings->indexedDBDatabasePath();
    return String();
}
}

PassRefPtr<IDBRequest> IDBFactory::getDatabaseNames(ScriptExecutionContext* context, ExceptionCode& ec)
{
    LOG(StorageAPI, "IDBFactory::getDatabaseNames");
    if (!isContextValid(context))
        return 0;
    if (!context->securityOrigin()->canAccessDatabase(context->topOrigin())) {
        ec = SECURITY_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), 0);
    m_backend->getDatabaseNames(request, *(context->securityOrigin()), *(context->topOrigin()), context, getIndexedDBDatabasePath(context));
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext* context, const String& name, ExceptionCode& ec)
{
    LOG(StorageAPI, "IDBFactory::open");
    return openInternal(context, name, 0, IndexedDB::VersionNullness::Null, ec);
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext* context, const String& name, unsigned long long version, ExceptionCode& ec)
{
    LOG(StorageAPI, "IDBFactory::open");
    if (!version) {
        ec = TypeError;
        return 0;
    }
    return openInternal(context, name, version, IndexedDB::VersionNullness::NonNull, ec);
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::openInternal(ScriptExecutionContext* context, const String& name, uint64_t version, IndexedDB::VersionNullness versionNullness, ExceptionCode& ec)
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
    int64_t transactionId = IDBDatabase::nextTransactionId();
    RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(context, databaseCallbacks, transactionId, version, versionNullness);
    m_backend->open(name, version, transactionId, request, databaseCallbacks, *(context->securityOrigin()), *(context->topOrigin()));
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::deleteDatabase(ScriptExecutionContext* context, const String& name, ExceptionCode& ec)
{
    LOG(StorageAPI, "IDBFactory::deleteDatabase");
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

    RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(context, 0, 0, 0, IndexedDB::VersionNullness::Null);
    m_backend->deleteDatabase(name, *context->securityOrigin(), *context->topOrigin(), request, context, getIndexedDBDatabasePath(context));
    return request;
}

short IDBFactory::cmp(ScriptExecutionContext* context, const Deprecated::ScriptValue& firstValue, const Deprecated::ScriptValue& secondValue, ExceptionCode& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> first = scriptValueToIDBKey(&requestState, firstValue);
    RefPtr<IDBKey> second = scriptValueToIDBKey(&requestState, secondValue);

    ASSERT(first);
    ASSERT(second);

    if (!first->isValid() || !second->isValid()) {
        ec = IDBDatabaseException::DataError;
        return 0;
    }

    return static_cast<short>(first->compare(second.get()));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
